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

#include "mca.h"
#include "drvMcaAsyn.h"
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
    asynInterface mca;
} mcaAIMPvt;

static int sendAIMSetup(mcaAIMPvt *drvPvt);
static asynStatus AIMCommand(void *drvPvt, asynUser *pasynUser, 
                             int signal, mcaCommand command, 
                             int ivalue, double dvalue);
static asynStatus AIMReadStatus(void *drvPvt, asynUser *pasynUser,
                             int signal, mcaAsynAcquireStatus *pstat);
static asynStatus AIMReadData(void *drvPvt, asynUser *pasynUser, int signal, 
                           int maxChans, int *nactual, int *data);
static void AIMReport(void *drvPvt, FILE *fp, int details);
static asynStatus AIMConnect(void *drvPvt, asynUser *pasynUser);
static asynStatus AIMDisconnect(void *drvPvt, asynUser *pasynUser);
/*
 * asynCommon methods
 */
static const struct asynCommon mcaAIMCommon = {
    AIMReport,
    AIMConnect,
    AIMDisconnect
};

/*
 * asynMca methods
 */
static const asynMca mcaAIMMca = {
    AIMCommand,
    AIMReadStatus,
    AIMReadData
};


int AIMConfig(
    const char *portName, int address, int port, int maxChans,
    int maxSignals, int maxSequences, char *ethernetDevice)
{
    unsigned char enet_address[6];
    int status;
    mcaAIMPvt *pPvt;
    int priority=0;

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

    /* Find the particular module on the net */
    status = nmc_findmod_by_addr(&pPvt->module, enet_address);
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
    pPvt->mca.interfaceType = asynMcaType;
    pPvt->mca.pinterface  = (void *)&mcaAIMMca;
    pPvt->mca.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                   0, /*not multiDevice*/
                                   1,
                                   priority,
                                   0);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig ERROR: Can't register myself.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig ERROR: Can't register common.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->mca);
    if (status != asynSuccess) {
        errlogPrintf("AIMConfig ERROR: Can't register mca.\n");
        return -1;
    }
    return(0);
}


static asynStatus AIMCommand(void *drvPvt, asynUser *pasynUser, 
                             int signal, mcaCommand command, 
                             int ivalue, double dvalue)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
    int len;
    int address, seq;
    int status;

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "mcaAIMAsynDriver::AIMCommand entry, command=%d, signal=%d, "
             "ivalue=%d, dvalue=%f\n", command, signal, ivalue, dvalue);
    switch (command) {
        case MSG_ACQUIRE:
            /* Start acquisition. */
            status = nmc_acqu_setstate(pPvt->module, pPvt->adc, 1);
            break;
        case MSG_SET_CHAS_INT:
            /* set channel advance source to internal (timed) */
            /* This is a NOOP for current MCS hardware - done manually */
            break;
        case MSG_SET_CHAS_EXT:
            /* set channel advance source to external */
            /* This is a NOOP for current MCS hardware - done manually */
            break;
        case MSG_SET_NCHAN:
            /* set number of channels */
            pPvt->nchans = ivalue;
            if (pPvt->nchans > pPvt->maxChans) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                          "mcaAIMServer Illegal nuse field");
                pPvt->nchans = pPvt->maxChans;
            }
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_DWELL:
            /* set dwell time */
            /* This is a NOOP for current MCS hardware - done manually */
            break;
        case MSG_SET_REAL_TIME:
            /* set preset real time. Convert to centiseconds */
            pPvt->preal = (int) (100. * dvalue);
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_LIVE_TIME:
            /* set preset live time. Convert to centiseconds */
            pPvt->plive = (int) (100. * dvalue);
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_COUNTS:
            /* set preset counts */
            pPvt->ptotal = ivalue;
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_LO_CHAN:
            /* set lower side of region integrated for preset counts */
            /* The preset total start channel is in bytes from start of
               AIM memory */
            pPvt->ptschan = pPvt->seq_address + 
                                  pPvt->maxChans*signal*4 + ivalue*4;
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_HI_CHAN:
            /* set high side of region integrated for preset counts */
            /* The preset total end channel is in bytes from start of
               AIM memory */
            pPvt->ptechan = pPvt->seq_address + 
                                  pPvt->maxChans*signal*4 + ivalue*4;
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_NSWEEPS:
            /* set number of sweeps (for MCS mode) */
            /* This is a NOOP on current version of MCS */
            break;
        case MSG_SET_MODE_PHA:
            /* set mode to Pulse Height Analysis */
            pPvt->acqmod = 1;
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_MODE_MCS:
            /* set mode to MultiChannel Scaler */
            pPvt->acqmod = 1;
            status = sendAIMSetup(pPvt);
            break;
        case MSG_SET_MODE_LIST:
            /* set mode to LIST (record each incoming event) */
            pPvt->acqmod = 3;
            status = sendAIMSetup(pPvt);
            break;
        case MSG_STOP_ACQUISITION:
            /* stop data acquisition */
            status = nmc_acqu_setstate(pPvt->module, pPvt->adc, 0);
            break;
        case MSG_ERASE:
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
        case MSG_SET_SEQ:
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
        case MSG_SET_PSCL:
            /* No-op for AIM */
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "mcaAIMAsynDriver::command port %s got illegal command %d\n",
                      pPvt->portName, command);
            break;
    }
    return(asynSuccess);
}


static asynStatus AIMReadData(void *drvPvt, asynUser *pasynUser, int signal, 
                              int maxChans, int *nactual, int *data)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
    int status;
    int address;

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "mcaAIMAsynDriver::AIMReadData entry, signal=%d, maxChans=%d\n", 
             signal, maxChans);

    address = pPvt->seq_address + pPvt->maxChans*signal*4;
 
    status = nmc_acqu_getmemory_cmp(pPvt->module, pPvt->adc, address, 1, 1, 1, 
                                    maxChans, data);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "(mcaAIMAsynDriver [%s signal=%d]): read %d chans, status=%d\n", 
              pPvt->portName, signal, maxChans, status);
    *nactual = maxChans;
    return(asynSuccess);
}


static asynStatus AIMReadStatus(void *drvPvt, asynUser *pasynUser,
                                int signal, mcaAsynAcquireStatus *pstat)
{
    mcaAIMPvt *pPvt = (mcaAIMPvt *)drvPvt;
    int status;
    epicsTimeStamp now;

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "mcaAIMAsynDriver::AIMReadStatus entry, signal=%d\n", signal);

    epicsTimeGetCurrent(&now);

    /* The status will be the same for each signal on a port.
     * We optimize by not reading the status if this is not
     * signal 0 and if the cached status is relatively recent */
    /* Read the current status of the device if signal 0 or
      * if the existing status info is too old */
    if ((signal == 0) || (epicsTimeDiffInSeconds(&now, &pPvt->statusTime) 
                              > pPvt->maxStatusTime)) {
        status = nmc_acqu_statusupdate(pPvt->module, pPvt->adc, 0, 0, 0,
                                       &pPvt->elive, &pPvt->ereal, &pPvt->etotals, 
                                       &pPvt->acquiring);
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                  "(mcaAIMAsynDriver [%s signal=%d]): get_acq_status=%d\n", 
                  pPvt->portName, signal, status);
        epicsTimeGetCurrent(&pPvt->statusTime);
    }
    pstat->realTime = pPvt->ereal/100.0;
    pstat->liveTime = pPvt->elive/100.0;
    pstat->dwellTime = 0.;
    pstat->acquiring = pPvt->acquiring;
    pstat->totalCounts = pPvt->etotals;
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
 

