/* drvMcaRontec.c
    Author: Mark Rivers

*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>


#include <errlog.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsTime.h>
#include <epicsExport.h>
#include <asynDriver.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynInt32Array.h>
#include <asynDrvUser.h>
#include <asynOctetSyncIO.h>

#include "mca.h"
#include "drvMca.h"

#define RONTEC_MAXCHANS 4096
#define RONTEC_MESSAGE_SIZE 100
#define RONTEC_TIMEOUT 2.0

typedef struct {
    int module;
    char *portName;
    char *serialPort;
    int maxChans;
    int nchans;
    int binning;
    int plive;
    int preal;
    int ptotal;
    int ptschan;
    int ptechan;
    int elive;
    int ereal;
    int etotals;
    int exists;
    int acquiring;
    char inputEos[10];
    int inputEosLen;
    char mcaBuffer[RONTEC_MAXCHANS*4 + 4];
    asynUser *pasynUser;
    asynInterface common;
    asynInterface int32;
    asynInterface float64;
    asynInterface int32Array;
    asynInterface drvUser;
} mcaRontecPvt;

/* These functions are in public interfaces */
static asynStatus int32Write        (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 value);
static asynStatus int32Read         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *value);
static asynStatus getBounds         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *low, epicsInt32 *high);
static asynStatus float64Write      (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 value);
static asynStatus float64Read       (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 *value);
static asynStatus int32ArrayRead    (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans,
                                     size_t *nactual);
static asynStatus int32ArrayWrite   (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans);
static asynStatus drvUserCreate     (void *drvPvt, asynUser *pasynUser,
                                     const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType    (void *drvPvt, asynUser *pasynUser,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy    (void *drvPvt, asynUser *pasynUser);

static void RontecReport               (void *drvPvt, FILE *fp, int details);
static asynStatus RontecConnect        (void *drvPvt, asynUser *pasynUser);
static asynStatus RontecDisconnect     (void *drvPvt, asynUser *pasynUser);

/* Private methods */
static asynStatus RontecWrite(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus RontecRead(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue);
static int sendMessage(mcaRontecPvt *pPvt, char *output, char *input);

/*
 * asynCommon methods
 */
static struct asynCommon mcaRontecCommon = {
    RontecReport,
    RontecConnect,
    RontecDisconnect
};

/* asynInt32 methods */
static asynInt32 mcaRontecInt32 = {
    int32Write,
    int32Read,
    getBounds,
    NULL,
    NULL
};

/* asynFloat64 methods */
static asynFloat64 mcaRontecFloat64 = {
    float64Write,
    float64Read,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array mcaRontecInt32Array = {
    int32ArrayWrite,
    int32ArrayRead,
    NULL,
    NULL
};

static asynDrvUser mcaRontecDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


int RontecConfig(const char *portName, char *serialPort, int serialPortAddress)
{
    int status;
    mcaRontecPvt *pPvt;

    pPvt = callocMustSucceed(1, sizeof(mcaRontecPvt), "RontecConfig");

    status = pasynOctetSyncIO->connect(serialPort, serialPortAddress, &pPvt->pasynUser, NULL);
    
    if (status != 0) {
        errlogPrintf("RontecConfig: can't connect to serial port %s, status=%d\n",
                     serialPort, status);
        return(-1);
    }

    /* Copy parameters to object private */
    pPvt->exists = 1;
    pPvt->maxChans = RONTEC_MAXCHANS;
    pPvt->nchans = pPvt->maxChans;
    pPvt->binning  = 1;
    /* Set reasonable defaults for other parameters */
    pPvt->plive   = 0;
    pPvt->preal   = 0;
    pPvt->ptotal  = 0;
    pPvt->ptschan = 0;
    pPvt->ptechan = 0;
    pPvt->acquiring = 0;
    
    pPvt->serialPort = epicsStrDup(serialPort);
    pPvt->portName = epicsStrDup(portName);

    /* Get the input EOS to be able to reset it later */
    pasynOctetSyncIO->getInputEos(pPvt->pasynUser, pPvt->inputEos, sizeof(pPvt->inputEos), &pPvt->inputEosLen);
    /* Initial settings for Rontec */
    sendMessage(pPvt, "$SM 4,0", NULL);  /* Send 4 bytes/channel, don't clear after sending */

    /*
     *  Link with higher level routines
     */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&mcaRontecCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&mcaRontecInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&mcaRontecFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&mcaRontecInt32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&mcaRontecDrvUser;
    pPvt->drvUser.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1,  /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stacksize */
    if (status != asynSuccess) {
        errlogPrintf("RontecConfig ERROR: Can't register myself.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("RontecConfig: Can't register common.\n");
        return -1;
    }
    status = pasynInt32Base->initialize(pPvt->portName, &pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("RontecConfig: Can't register int32.\n");
        return -1;
    }

    status = pasynFloat64Base->initialize(pPvt->portName, &pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("RontecConfig: Can't register float64.\n");
        return -1;
    }

    status = pasynInt32ArrayBase->initialize(pPvt->portName, &pPvt->int32Array);
    if (status != asynSuccess) {
        errlogPrintf("RontecConfig: Can't register int32Array.\n");
        return -1;
    }

    status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("RontecConfig ERROR: Can't register drvUser\n");
        return -1;
    }
    return(0);
}


static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 value)
{
    return(RontecWrite(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    return(RontecWrite(drvPvt, pasynUser, 0, value));
}

static asynStatus RontecWrite(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue)
{
    mcaRontecPvt *pPvt = (mcaRontecPvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    char message[RONTEC_MESSAGE_SIZE], response[RONTEC_MESSAGE_SIZE];
    int signal;
    size_t nactual;
    epicsInt32 dummy[1];

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "drvMcaRontec::RontecWrite entry, command=%d, signal=%d, "
             "ivalue=%d, dvalue=%f\n", command, signal, ivalue, dvalue);
    switch (command) {
        case mcaStartAcquire:
            /* Start acquisition. */
            if (pPvt->plive > 0.) {
               sprintf(message, "$LT %d", pPvt->plive);
               sendMessage(pPvt, message, NULL);
            } else if (pPvt->preal > 0.) {
               sprintf(message, "$MT %d", pPvt->preal);
               sendMessage(pPvt, message, NULL);
            } else {
               sendMessage(pPvt, "$MT 0", NULL);
            }
            break;
        case mcaStopAcquire:
            /* stop data acquisition */
            sendMessage(pPvt, "$MP ON", NULL);
            break;
        case mcaErase:
            sendMessage(pPvt, "$CC", NULL);
            break;
        case mcaReadStatus:
            /* Read acquisition status */
            sendMessage(pPvt, "$FP", response);
            if (response[4] == '-') pPvt->acquiring = 1; else pPvt->acquiring = 0;
            /* Read 1 channel of the spectrum so we get the elapsed live and real time */
            int32ArrayRead(pPvt, pasynUser, dummy, 1, &nactual);
            sendMessage(pPvt, "$MS", response);
            pPvt->ereal = atoi(&response[4]);
            /* LS seems not to be supported - track this down */
            /* sendMessage(pPvt, "$LS", response); */
            pPvt->elive = atoi(&response[4]);
        case mcaChannelAdvanceInternal:
            /* set channel advance source to internal (timed) */
            /* This is a NOOP for Rontec */
            break;
        case mcaChannelAdvanceExternal:
            /* set channel advance source to external */
            /* This is a NOOP for Rontec */
            break;
        case mcaNumChannels:
            /* set number of channels */
            pPvt->nchans = ivalue;
            if (pPvt->nchans > pPvt->maxChans) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                          "mcaRontecServer Illegal nuse field");
                pPvt->nchans = pPvt->maxChans;
            }
            pPvt->binning = pPvt->maxChans / pPvt->nchans;
            break;
        case mcaModePHA:
            /* set mode to Pulse Height Analysis */
            /* This is a NOOP for Rontec */
            break;
        case mcaModeMCS:
            /* set mode to MultiChannel Scaler */
            /* This is a NOOP for Rontec */
            break;
        case mcaModeList:
            /* set mode to LIST (record each incoming event) */
            /* This is a NOOP for Rontec */
            break;
        case mcaSequence:
            /* set sequence number */
            /* This is a NOOP for Rontec */
            break;
        case mcaPrescale:
            /* No-op for Rontec */
            break;
        case mcaPresetSweeps:
            /* set number of sweeps (for MCS mode) */
            /* This is a NOOP for Rontec */
            break;
        case mcaPresetLowChannel:
            /* set lower side of region integrated for preset counts */
            /* This is a NOOP for Rontec */
            break;
        case mcaPresetHighChannel:
            /* set high side of region integrated for preset counts */
            /* This is a NOOP for Rontec */
            break;
        case mcaDwellTime:
            /* set dwell time */
            /* This is a NOOP for Rontec */
            break;
        case mcaPresetRealTime:
            /* set preset real time. Convert to centiseconds */
            pPvt->preal = (int) (1000. * dvalue);
            break;
        case mcaPresetLiveTime:
            /* set preset live time. Convert to centiseconds */
            pPvt->plive = (int) (1000. * dvalue);
            break;
        case mcaPresetCounts:
            /* set preset counts */
            pPvt->ptotal = ivalue;
            /* This is a NOOP for Rontec */
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "drvMcaRontec::RontecWrite port %s got illegal command %d\n",
                      pPvt->portName, command);
            break;
    }
    return(asynSuccess);
}


static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *value)
{
    return(RontecRead(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    return(RontecRead(drvPvt, pasynUser, NULL, value));
}

static asynStatus RontecRead(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    mcaRontecPvt *pPvt = (mcaRontecPvt *)drvPvt;
    mcaCommand command = pasynUser->reason;
    asynStatus status=asynSuccess;

    switch (command) {
        case mcaAcquiring:
            *pivalue = pPvt->acquiring;
            break;
        case mcaDwellTime:
            *pfvalue = 0.;
            break;
        case mcaElapsedLiveTime:
            *pfvalue = pPvt->elive/1000.;
            break;
        case mcaElapsedRealTime:
            *pfvalue = pPvt->ereal/1000.;
            break;
        case mcaElapsedCounts:
            *pfvalue = 0;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvMcaRontec::RontecRead got illegal command %d\n",
                      command);
            status = asynError;
            break;
    }
    return(status);
}

static asynStatus getBounds(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = 0;
    return(asynSuccess);
}


static asynStatus int32ArrayRead(void *drvPvt, asynUser *pasynUser,
                                 epicsInt32 *data, size_t maxChans, 
                                 size_t *nactual)
{
    mcaRontecPvt *pPvt = (mcaRontecPvt *)drvPvt;
    int signal;
    double timeout=10.0;
    int nbytesOut, nbytesIn, eomReason, nread;
    char message[RONTEC_MESSAGE_SIZE];
    unsigned char *pin;
    int temp[4];
    int i, j;

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "drvMcaRontec::int32ArrayRead entry, signal=%d, maxChans=%d\n", 
             signal, maxChans);

    sprintf(message, "$SS 0,%d,%d,%d", pPvt->binning, pPvt->binning, maxChans*pPvt->binning);
    /* The MCA data from the Rontec are in binary format, need to turn off the input EOS processing */
    pasynOctetSyncIO->setInputEos(pPvt->pasynUser, "", 0);
    nread = 4 + 4*maxChans;  /* Rontec sends !SS<CR> followed by 4 bytes/channel */
    pasynOctetSyncIO->writeRead(pPvt->pasynUser, message, strlen(message), pPvt->mcaBuffer, nread,
                               timeout, &nbytesOut, &nbytesIn, &eomReason);
    pasynOctetSyncIO->setInputEos(pPvt->pasynUser, pPvt->inputEos, pPvt->inputEosLen);
    if (nbytesIn != nread) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                  "drvMcaRontec::int32ArrayRead: error reading MCA data, nbytesIn=%d, should be %d\n",
                  nbytesIn, nread);
    }
    /* Copy data from buffer to array */
    pin = &pPvt->mcaBuffer[4];
    for (i=0; i<maxChans; i++) {
       for (j=0; j<4; j++) {
           temp[j] = *pin++;
       }
       data[i] = (temp[0] << 24) + (temp[1] << 16) + (temp[2] << 8) + temp[3];
    }
    *nactual = maxChans;
    return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
                                  epicsInt32 *data, size_t maxChans)
{
    mcaRontecPvt *pPvt = (mcaRontecPvt *)drvPvt;
    int signal;

    if (maxChans > pPvt->maxChans) maxChans = pPvt->maxChans;

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "drvMcaRontec::int32ArrayWrite entry, signal=%d, maxChans=%d\n",
             signal, maxChans);

    /* There seems to be a way to write the Rontec memory, with SETHOSTMEM,
     * but there is no callable function to do it yet */
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvMcaRontec::int32ArrayWrite, write to Rontec not implemented\n");
    return(asynSuccess);
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    int i;
    char *pstring;

    for (i=0; i<MAX_MCA_COMMANDS; i++) {
        pstring = mcaCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = mcaCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(mcaCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvSweep::drvUserCreate, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvMcaRontec::drvUserCreate, unknown command=%s\n", drvInfo);
    return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    mcaCommand command = pasynUser->reason;

    *pptypeName = NULL;
    *psize = 0;
    if (pptypeName)
        *pptypeName = epicsStrDup(mcaCommands[command].commandString);
    if (psize) *psize = sizeof(command);
    return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return(asynSuccess);
}


/* asynCommon routines */

/* Report  parameters */
static void RontecReport(void *drvPvt, FILE *fp, int details)
{
    mcaRontecPvt *pPvt = (mcaRontecPvt *)drvPvt;

    fprintf(fp, "Rontec %s: connected on serial port %s\n",
            pPvt->portName, pPvt->serialPort);
    if (details >= 1) {
        fprintf(fp, "              maxChans: %d\n", pPvt->maxChans);
        fprintf(fp, "              nchans:   %d\n", pPvt->nchans);
        fprintf(fp, "              binning:  %d\n", pPvt->binning);
    }
}

/* Connect */
static asynStatus RontecConnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Connect */
static asynStatus RontecDisconnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


/* Support routines */
static int sendMessage(mcaRontecPvt *pPvt, char *output, char *input)
{
   int status;
   int nbytesIn=0, nbytesOut=0, eomReason=0;
   char buffer[RONTEC_MESSAGE_SIZE];

   if (input == NULL) input=buffer;

   status = pasynOctetSyncIO->writeRead(pPvt->pasynUser, output, strlen(output), input, RONTEC_MESSAGE_SIZE,
                                        RONTEC_TIMEOUT, &nbytesOut, &nbytesIn, &eomReason);

   asynPrint(pPvt->pasynUser, ASYN_TRACEIO_DEVICE, 
                 "drvMcaRontec::sendMessage: output=%s, output_len=%d number sent=%d\n"
                 "                           input=%s, input_len=%d, number_received=%d\n",
                                             output, strlen(output), nbytesOut, 
                                             input, RONTEC_MESSAGE_SIZE, nbytesIn);
   if ((status != asynSuccess) || strncmp(input, "!ERROR", 6) == 0) {
       asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR, 
                 "drvMcaRontec::sendMessage: error %s, status=%d, nbytesOut=%d, nbytesIn=%d\n",
                 input, status, nbytesOut, nbytesIn);
   }
   return(status);
}


/* iocsh functions */

static const iocshArg RontecConfigArg0 = { "Asyn port name",iocshArgString};
static const iocshArg RontecConfigArg1 = { "Serial port name",iocshArgString};
static const iocshArg RontecConfigArg2 = { "Serial port address",iocshArgInt};
static const iocshArg * const RontecConfigArgs[3] = {&RontecConfigArg0,
                                                     &RontecConfigArg1,
                                                     &RontecConfigArg2};
static const iocshFuncDef RontecConfigFuncDef = {"RontecConfig",3,RontecConfigArgs};
static void RontecConfigCallFunc(const iocshArgBuf *args)
{
    RontecConfig(args[0].sval, args[1].sval, args[2].ival);
}

void mcaRontecRegister(void)
{
    iocshRegister(&RontecConfigFuncDef,RontecConfigCallFunc);
}

epicsExportRegistrar(mcaRontecRegister);
 
