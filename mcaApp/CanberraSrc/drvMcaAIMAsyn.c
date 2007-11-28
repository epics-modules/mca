/* drvMcaAIMAsyn.c
    Author: Mark Rivers

   27-June-2004  Converted from mcaAIMServer.cc
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

#include "mca.h"
#include "drvMca.h"
#include "nmc_sys_defs.h"

typedef struct {
    int module;
    int adc;
    char *portName;
    char *ethernetDevice;
    int base_address;
    int seq_address;
    int maxChans;
    int maxSignals;
    int maxSequences;
    int nchans;
    int plive;
    int preal;
    int ptotal;
    int ptschan;
    int ptechan;
    int elive;
    int ereal;
    int etotals;
    int acqmod;
    int exists;
    epicsTimeStamp statusTime;
    double maxStatusTime;
    int acquiring;
    asynInterface common;
    asynInterface int32;
    asynInterface float64;
    asynInterface int32Array;
    asynInterface drvUser;
} mcaAIMPvt;

extern struct nmc_module_info_struct *nmc_module_info;

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

static void AIMReport               (void *drvPvt, FILE *fp, int details);
static asynStatus AIMConnect        (void *drvPvt, asynUser *pasynUser);
static asynStatus AIMDisconnect     (void *drvPvt, asynUser *pasynUser);

/* Private methods */
static int sendAIMSetup(mcaAIMPvt *drvPvt);
static asynStatus AIMWrite(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus AIMRead(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue);

/*
 * asynCommon methods
 */
static struct asynCommon mcaAIMCommon = {
    AIMReport,
    AIMConnect,
    AIMDisconnect
};

/* asynInt32 methods */
static asynInt32 mcaAIMInt32 = {
    int32Write,
    int32Read,
    getBounds,
    NULL,
    NULL
};

/* asynFloat64 methods */
static asynFloat64 mcaAIMFloat64 = {
    float64Write,
    float64Read,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array mcaAIMInt32Array = {
    int32ArrayWrite,
    int32ArrayRead,
    NULL,
    NULL
};

static asynDrvUser mcaAIMDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


int AIMConfig(
    const char *portName, int address, int port, int maxChans,
    int maxSignals, int maxSequences, char *ethernetDevice)
{
    unsigned char enet_address[6];
    int status;
    int retries = 0;
    mcaAIMPvt *pPvt;

    pPvt = callocMustSucceed(1, sizeof(mcaAIMPvt), "AIMConfig");
    status = nmc_initialize(ethernetDevice);
    if (status != 0) {
        errlogPrintf("AIMConfig: nmc_initialize failed for device %s status=%d\n",
                     ethernetDevice, status);
        return(ERROR);
    }

    /* Copy parameters to object private */
    pPvt->exists = 1;
    pPvt->maxChans = maxChans;
    pPvt->nchans = maxChans;
    pPvt->maxSignals = maxSignals;
    pPvt->maxSequences = maxSequences;
    /* Copy the module ADC port number. Subtract 1 to convert from */
    /* user units to device units  */
    pPvt->adc = port - 1;
    /* Set reasonable defaults for other parameters */
    pPvt->plive   = 0;
    pPvt->preal   = 0;
    pPvt->ptotal  = 0;
    pPvt->ptschan = 0;
    pPvt->ptechan = 0;
    pPvt->acqmod  = 1;
    pPvt->acquiring = 0;
    
    /* Maximum time to use old status info before a new query is sent. Only used 
     * if signal!=0, typically with multiplexors. 0.1 seconds  */
    pPvt->maxStatusTime = 0.1;
    /* Compute the module Ethernet address */
    nmc_build_enet_addr(address, enet_address);

    /* Find the particular module on the net - may have to retry in case
       the module database is still being built after initialisation.
       This is not possible to resolve other than by waiting, since there are
       an unknown number of modules on the local subnet, and they can reply
       to the initial inquiry broadcast in an arbitrary order. */
    do {
	status = nmc_findmod_by_addr(&pPvt->module, enet_address);
	epicsThreadSleep(0.1);
    } while ((status != OK) && (retries++ < 5));

    if (status != OK) {
        errlogPrintf("AIMConfig ERROR: cannot find module on the network!\n");
        return (ERROR);
    }

    /* Buy the module (make this IOC own it) */
    status = nmc_buymodule(pPvt->module, 0);
    if (status != OK) {
        errlogPrintf("AIMConfig ERROR: cannot buy module, someone else owns it!\n");
        return (ERROR);
    }

    /* Allocate memory in the AIM, get base address */
    status = nmc_allocate_memory(pPvt->module,
                    pPvt->maxChans * pPvt->maxSignals * pPvt->maxSequences * 4,
                    &pPvt->base_address);
    if (status != OK) {
        errlogPrintf("AIMConfig ERROR: cannot allocate required memory in AIM\n");
        return (ERROR);
    }
    pPvt->seq_address = pPvt->base_address;

    pPvt->ethernetDevice = epicsStrDup(ethernetDevice);
    pPvt->portName = epicsStrDup(portName);

    /*
     *  Link with higher level routines
     */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&mcaAIMCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&mcaAIMInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&mcaAIMFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&mcaAIMInt32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&mcaAIMDrvUser;
    pPvt->drvUser.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1,  /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stacksize */
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig ERROR: Can't register myself.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig: Can't register common.\n");
        return -1;
    }
    status = pasynInt32Base->initialize(pPvt->portName, &pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig: Can't register int32.\n");
        return -1;
    }

    status = pasynFloat64Base->initialize(pPvt->portName, &pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig: Can't register float64.\n");
        return -1;
    }

    status = pasynInt32ArrayBase->initialize(pPvt->portName, &pPvt->int32Array);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig: Can't register int32Array.\n");
        return -1;
    }

    status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig ERROR: Can't register drvUser\n");
        return -1;
    }
    return(0);
}


static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 value)
{
    return(AIMWrite(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    return(AIMWrite(drvPvt, pasynUser, 0, value));
}

static asynStatus AIMWrite(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    asynStatus status=asynSuccess;
    int len;
    int address, seq;
    int signal;
    epicsTimeStamp now;

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "mcaAIMAsynDriver::AIMWrite entry, command=%d, signal=%d, "
             "ivalue=%d, dvalue=%f\n", command, signal, ivalue, dvalue);
    switch (command) {
        case mcaStartAcquire:
            /* Start acquisition. */
            status = nmc_acqu_setstate(pPvt->module, pPvt->adc, 1);
            break;
        case mcaStopAcquire:
            /* stop data acquisition */
            status = nmc_acqu_setstate(pPvt->module, pPvt->adc, 0);
            break;
        case mcaErase:
            /* Erase. If this is signal 0 then erase memory for all signals.
             * Databases take advantage of this for performance with
             * multielement detectors with multiplexors */
            if (signal == 0)
                len = pPvt->maxChans*pPvt->maxSignals*4;
            else
                len = pPvt->nchans*4;
            /* If AIM is acquiring, turn if off */
            if (pPvt->acquiring)
            status = nmc_acqu_setstate(pPvt->module, pPvt->adc, 0);
            address = pPvt->seq_address + pPvt->maxChans*signal*4;
            status = nmc_acqu_erase(pPvt->module, address, len);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                    "(mcaAIMAsynDriver::command [%s signal=%d]):"
                    " erased %d chans, status=%d\n",
                    pPvt->portName, signal, len, status);
            /* Set the elapsed live and real time back to zero. */
            status = nmc_acqu_setelapsed(pPvt->module, pPvt->adc, 0, 0);
            /* If AIM was acquiring, turn it back on */
            if (pPvt->acquiring)
                status = nmc_acqu_setstate(pPvt->module, pPvt->adc, 1);
                break;
        case mcaReadStatus:
            /* The status will be the same for each signal on a port.
             * We optimize by not reading the status if this is not
             * signal 0 and if the cached status is relatively recent
             * Read the current status of the device if signal 0 or
             * if the existing status info is too old */
            epicsTimeGetCurrent(&now);
            if ((signal == 0) || 
                    (epicsTimeDiffInSeconds(&now, &pPvt->statusTime)
                    > pPvt->maxStatusTime)) {
                status = nmc_acqu_statusupdate(pPvt->module, pPvt->adc, 0, 0, 0,
                                              &pPvt->elive, &pPvt->ereal, 
                                              &pPvt->etotals, &pPvt->acquiring);
                asynPrint(pasynUser, ASYN_TRACE_FLOW,
                          "(mcaAIMAsynDriver [%s signal=%d]): get_acq_status=%d\n",
                          pPvt->portName, signal, status);
                epicsTimeGetCurrent(&pPvt->statusTime);
            }
        case mcaChannelAdvanceInternal:
            /* set channel advance source to internal (timed) */
            /* This is a NOOP for current MCS hardware - done manually */
            break;
        case mcaChannelAdvanceExternal:
            /* set channel advance source to external */
            /* This is a NOOP for current MCS hardware - done manually */
            break;
        case mcaNumChannels:
            /* set number of channels */
            pPvt->nchans = ivalue;
            if (pPvt->nchans > pPvt->maxChans) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                          "mcaAIMServer Illegal nuse field");
                pPvt->nchans = pPvt->maxChans;
            }
            status = sendAIMSetup(pPvt);
            break;
        case mcaModePHA:
            /* set mode to Pulse Height Analysis */
            pPvt->acqmod = 1;
            status = sendAIMSetup(pPvt);
            break;
        case mcaModeMCS:
            /* set mode to MultiChannel Scaler */
            pPvt->acqmod = 1;
            status = sendAIMSetup(pPvt);
            break;
        case mcaModeList:
            /* set mode to LIST (record each incoming event) */
            pPvt->acqmod = 3;
            status = sendAIMSetup(pPvt);
            break;
        case mcaSequence:
            /* set sequence number */
            seq = ivalue;
            if (seq > pPvt->maxSequences) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                          "mcaAIMAsynDriver::command: Illegal seq field\n");
                seq = pPvt->maxSequences;
            }
            pPvt->seq_address = pPvt->base_address + 
                                      pPvt->maxChans * pPvt->maxSignals * seq * 4;
            status = sendAIMSetup(pPvt);
            break;
        case mcaPrescale:
            /* No-op for AIM */
            break;
        case mcaPresetSweeps:
            /* set number of sweeps (for MCS mode) */
            /* This is a NOOP on current version of MCS */
            break;
        case mcaPresetLowChannel:
            /* set lower side of region integrated for preset counts */
            /* The preset total start channel is in bytes from start of
               AIM memory */
            pPvt->ptschan = pPvt->seq_address + 
                                  pPvt->maxChans*signal*4 + ivalue*4;
            status = sendAIMSetup(pPvt);
            break;
        case mcaPresetHighChannel:
            /* set high side of region integrated for preset counts */
            /* The preset total end channel is in bytes from start of
               AIM memory */
            pPvt->ptechan = pPvt->seq_address + 
                                  pPvt->maxChans*signal*4 + ivalue*4;
            status = sendAIMSetup(pPvt);
            break;
        case mcaDwellTime:
            /* set dwell time */
            /* This is a NOOP for current MCS hardware - done manually */
            break;
        case mcaPresetRealTime:
            /* set preset real time. Convert to centiseconds */
            pPvt->preal = (int) (100. * dvalue);
            status = sendAIMSetup(pPvt);
            break;
        case mcaPresetLiveTime:
            /* set preset live time. Convert to centiseconds */
            pPvt->plive = (int) (100. * dvalue);
            status = sendAIMSetup(pPvt);
            break;
        case mcaPresetCounts:
            /* set preset counts */
            pPvt->ptotal = ivalue;
            status = sendAIMSetup(pPvt);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "mcaAIMAsynDriver::command port %s got illegal command %d\n",
                      pPvt->portName, command);
            break;
    }
    return(asynSuccess);
}


static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *value)
{
    return(AIMRead(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    return(AIMRead(drvPvt, pasynUser, NULL, value));
}

static asynStatus AIMRead(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
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
            *pfvalue = pPvt->elive/100.;
            break;
        case mcaElapsedRealTime:
            *pfvalue = pPvt->ereal/100.;
            break;
        case mcaElapsedCounts:
            *pfvalue = pPvt->etotals;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvMcaAIMAsyn::AIMRead got illegal command %d\n",
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
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
    int status;
    int address;
    int signal;
    struct nmc_module_info_struct minfo = nmc_module_info[pPvt->module];

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "mcaAIMAsynDriver::AIMReadData entry, signal=%d, maxChans=%d\n", 
             signal, maxChans);

    address = pPvt->seq_address + pPvt->maxChans*signal*4;
 
    /* There is a real performance difference between reading compressed and
     * uncompressed data on different module types.  It is 40% faster to
     * read compressed data on the original 556 model, 300% faster to read 
     * uncompresed data on the 556A, and 230% faster to read uncompressed data
     * on the DSA2000.  The hw_revision of the 556 is 0, 556A is 1, and DS2000 is 2.
     * We read compressed for 556 and uncompressed for others.
     */
    if (minfo.hw_revision == 0) {
        status = nmc_acqu_getmemory_cmp(pPvt->module, pPvt->adc, address, 1, 1, 1, 
                                        maxChans, data);
    } else {
        status = nmc_acqu_getmemory(pPvt->module, pPvt->adc, address, 1, 1, 1, 
                                    maxChans, data);
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "(mcaAIMAsynDriver [%s signal=%d]): read %d chans, status=%d\n", 
              pPvt->portName, signal, maxChans, status);
    *nactual = maxChans;
    return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
                                  epicsInt32 *data, size_t maxChans)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
    int address;
    int signal;

    if (maxChans > pPvt->maxChans) maxChans = pPvt->maxChans;

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "mcaAIMAsynDriver::AIMWrtieData entry, signal=%d, maxChans=%d\n",
             signal, maxChans);

    address = pPvt->seq_address + pPvt->maxChans*signal*4;

    /* There seems to be a way to write the AIM memory, with SETHOSTMEM,
     * but there is no callable function to do it yet */
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvMcaAIM::AIMWriteData, write to AIM not implemented\n");
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
              "drvMcaAIMAsyn::drvUserCreate, unknown command=%s\n", drvInfo);
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
static void AIMReport(void *drvPvt, FILE *fp, int details)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;

    assert(pPvt);
    fprintf(fp, "AIM %s: connected on Ethernet device %s, ADC port %d\n",
            pPvt->portName, pPvt->ethernetDevice, pPvt->adc);
    if (details >= 1) {
        fprintf(fp, "              maxChans: %d\n", pPvt->maxChans);
    }
}

/* Connect */
static asynStatus AIMConnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Connect */
static asynStatus AIMDisconnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


/* Support routines */
int sendAIMSetup(mcaAIMPvt *pPvt)
{
   int status;
   status = nmc_acqu_setup(pPvt->module, pPvt->adc, pPvt->seq_address, 
                           pPvt->maxSignals*pPvt->maxChans,
                           pPvt->plive, pPvt->preal, pPvt->ptotal, 
                           pPvt->ptschan, pPvt->ptechan, pPvt->acqmod);
   return(status);
}


/* iocsh functions */

static const iocshArg AIMConfigArg0 = { "Asyn port name",iocshArgString};
static const iocshArg AIMConfigArg1 = { "Address",iocshArgInt};
static const iocshArg AIMConfigArg2 = { "Port",iocshArgInt};
static const iocshArg AIMConfigArg3 = { "MaxChan",iocshArgInt};
static const iocshArg AIMConfigArg4 = { "MaxSign",iocshArgInt};
static const iocshArg AIMConfigArg5 = { "MaxSeq",iocshArgInt};
static const iocshArg AIMConfigArg6 = { "Eth. dev",iocshArgString};
static const iocshArg * const AIMConfigArgs[7] = {&AIMConfigArg0,
                                                  &AIMConfigArg1,
                                                  &AIMConfigArg2,
                                                  &AIMConfigArg3,
                                                  &AIMConfigArg4,
                                                  &AIMConfigArg5,
                                                  &AIMConfigArg6};
static const iocshFuncDef AIMConfigFuncDef = {"AIMConfig",7,AIMConfigArgs};
static void AIMConfigCallFunc(const iocshArgBuf *args)
{
    AIMConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
              args[4].ival, args[5].ival, args[6].sval);
}

void mcaAIMRegister(void)
{
    iocshRegister(&AIMConfigFuncDef,AIMConfigCallFunc);
}

epicsExportRegistrar(mcaAIMRegister);
 

