/* devMcaAsyn.c

    Author: Mark Rivers
    16-May-2004

    This is device support for the MCA record with asyn drivers.
  
    Modifications:
      16-May-2004  MLR  Created from previous devMcaMpf.cc
*/


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <recGbl.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <errlog.h>
#include <dbCommon.h>
#include <dbScan.h>
#include <cantProceed.h>
#include <recSup.h>
#include <devSup.h>
#include <alarm.h>

#include <asynDriver.h>
#include <asynInt32.h>
#include <asynInt32Array.h>
#include <asynFloat64.h>
#include <asynDrvUser.h>
#include <asynEpicsUtils.h>
#include <epicsExport.h>

#include "mcaRecord.h"
#include "mca.h"
#include "drvMca.h"

typedef enum {int32Type, float64Type, int32ArrayType} interfaceType;

typedef struct {
    mcaCommand command;
    interfaceType interface;
    int ivalue;
    double dvalue;
} mcaAsynMessage;

typedef struct {
    mcaRecord *pmca;
    asynUser *pasynUser;
    asynInt32 *pasynInt32;
    void *asynInt32Pvt;
    asynFloat64 *pasynFloat64;
    void *asynFloat64Pvt;
    asynInt32Array *pasynInt32Array;
    void *asynInt32ArrayPvt;
    asynDrvUser *pasynDrvUser;
    void *asynDrvUserPvt;
    size_t nread;
    int *data;
    double elapsedLive;
    double elapsedReal;
    double dwellTime;
    double totalCounts;
    int acquiring;
    /* These are the pasynUser->reason values returned by the driver for each drvInfo string */
    int driverReasons[MAX_MCA_COMMANDS];
} mcaAsynPvt;

static long init_record(mcaRecord *pmca);
static long send_msg(mcaRecord *pmca, mcaCommand command, void *parg);
static long read_array(mcaRecord *pmca);
static void asynCallback(asynUser *pasynUser);
static long findDrvInfo(mcaRecord *pmca, asynUser *pasynUser, char *drvInfoString, int command);

typedef struct {
    long            number;
    DEVSUPFUN       report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record;
    DEVSUPFUN       get_ioint_info;
    long            (*send_msg)(mcaRecord *pmca, mcaCommand command, void *parg);
    long            (*read_array)(mcaRecord *pmca);
} mcaAsynDset;

mcaAsynDset devMcaAsyn = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    send_msg,
    read_array
};
epicsExportAddress(dset, devMcaAsyn);


static long init_record(mcaRecord *pmca)
{
    asynUser *pasynUser;
    char *port, *userParam;
    int signal;
    asynStatus status;
    asynInterface *pasynInterface;
    mcaAsynPvt *pPvt;

    /* Allocate asynMcaPvt private structure */
    pPvt = callocMustSucceed(1, sizeof(mcaAsynPvt), "devMcaAsyn init_record()");
    pPvt->data = callocMustSucceed(pmca->nmax, sizeof(long), 
                                   "devMcaAsyn init_record()");
    /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(asynCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->pmca = pmca;
    pmca->dpvt = pPvt;

    status = pasynEpicsUtils->parseLink(pasynUser, &pmca->inp,
                                    &port, &signal, &userParam);
    if (status != asynSuccess) {
        errlogPrintf("devMcaAsyn::init_record %s bad link %s\n",
                     pmca->name, pasynUser->errorMessage);
        goto bad;
    }

    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, port, signal);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s connectDevice failed to %s\n",
                  pmca->name, port);
        goto bad;
    }

    /* Get the asynInt32 interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynInt32Type, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s find int32 interface failed\n",
                  pmca->name);
        goto bad;
    }
    pPvt->pasynInt32 = (asynInt32 *)pasynInterface->pinterface;
    pPvt->asynInt32Pvt = pasynInterface->drvPvt;

    /* Get the asynFloat64 interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynFloat64Type, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s find float64 interface failed\n",
                  pmca->name);
        goto bad;
    }
    pPvt->pasynFloat64 = (asynFloat64 *)pasynInterface->pinterface;
    pPvt->asynFloat64Pvt = pasynInterface->drvPvt;

    /* Get the asynInt32Array interface */
    pasynInterface = pasynManager->findInterface(pasynUser, 
                                                 asynInt32ArrayType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s find int32Array interface failed\n",
                  pmca->name);
        goto bad;
    }
    pPvt->pasynInt32Array = (asynInt32Array *)pasynInterface->pinterface;
    pPvt->asynInt32ArrayPvt = pasynInterface->drvPvt;

    /* Get the asynDrvUser interface */
    pasynInterface = pasynManager->findInterface(pasynUser, 
                                                 asynDrvUserType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s find drvUser interface failed\n",
                  pmca->name);
        goto bad;
    }
    pPvt->pasynDrvUser = (asynDrvUser *)pasynInterface->pinterface;
    pPvt->asynDrvUserPvt = pasynInterface->drvPvt;
    
    if (findDrvInfo(pmca, pasynUser, mcaDataString,                    mcaData)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaStartAcquireString,            mcaStartAcquire)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaStopAcquireString,             mcaStopAcquire)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaEraseString,                   mcaErase)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaReadStatusString,              mcaReadStatus)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaChannelAdvanceSourceString,    mcaChannelAdvanceSource)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaNumChannelsString,             mcaNumChannels)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaAcquireModeString,             mcaAcquireMode)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaSequenceString,                mcaSequence)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPrescaleString,                mcaPrescale)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPresetSweepsString,            mcaPresetSweeps)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPresetLowChannelString,        mcaPresetLowChannel)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPresetHighChannelString,       mcaPresetHighChannel)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaDwellTimeString,               mcaDwellTime)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPresetLiveTimeString,          mcaPresetLiveTime)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPresetRealTimeString,          mcaPresetRealTime)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaPresetCountsString,            mcaPresetCounts)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaAcquiringString,               mcaAcquiring)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaElapsedLiveTimeString,         mcaElapsedLiveTime)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaElapsedRealTimeString,         mcaElapsedRealTime)) goto bad;
    if (findDrvInfo(pmca, pasynUser, mcaElapsedCountsString,           mcaElapsedCounts)) goto bad;


    return(0);
bad:
    pmca->pact=1;
    return(0);
}

static long findDrvInfo(mcaRecord *pmca, asynUser *pasynUser, char *drvInfoString, int command)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pmca->dpvt;

    /* Look up the pasynUser->reason */
    if (pPvt->pasynDrvUser->create(pPvt->asynDrvUserPvt, pasynUser, drvInfoString, NULL, NULL) != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::findDrvInfo, %s drvUserCreate failed for %s\n",
                  pmca->name, drvInfoString);
        return(-1);
    }
    pPvt->driverReasons[command] = pasynUser->reason;
    return(0);
}


static long send_msg(mcaRecord *pmca, mcaCommand command, void *parg)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pmca->dpvt;
    asynUser *pasynUser = pPvt->pasynUser;
    mcaAsynMessage *pmsg;
    mcaStatus *pstatus = pmca->pstatus;
    int status;

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "devMcaAsyn::send_msg: %s command=%d, pact=%d, rdns=%d, rdng=%d\n", 
              pmca->name, command, pmca->pact, pmca->rdns, pmca->rdng);

    /* If we are already in COMM_ALARM then this server is not reachable, 
     * return */
    if ((pmca->nsta == COMM_ALARM) || (pmca->stat == COMM_ALARM)) return(-1);

    /* If rdns is true and command=mcaReadStatus then this is a second 
     * call from the record to complete */
    if (pmca->rdns && (command == mcaReadStatus)) {
        /* This is a second call from record after I/O is complete. 
         * Copy information from private to record */
        pstatus->elapsedReal = pPvt->elapsedReal;
        pstatus->elapsedLive = pPvt->elapsedLive;
        pstatus->dwellTime   = pPvt->dwellTime;
        pstatus->totalCounts = pPvt->totalCounts;
        pstatus->acquiring   = pPvt->acquiring;
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                  "devMcaAsyn::send_msg, record=%s, elapsed real=%f,"
                  " elapsed live=%f, dwell time=%f, acqg=%d\n", 
                  pmca->name, pstatus->elapsedReal, pstatus->elapsedLive, pstatus->dwellTime, pstatus->acquiring);
        return(0);
    }

    /* Make a copy of asynUser.  This is needed because we can have multiple
     * requests queued.  It will be freed in the callback */
    pasynUser = pasynManager->duplicateAsynUser(pasynUser, asynCallback, 0);
    pmsg = pasynManager->memMalloc(sizeof *pmsg);
    pmsg->command = command;
    if (parg) { 
        pmsg->ivalue= *(int *)parg;
        pmsg->dvalue= *(double*)parg;
    } else {
        pmsg->ivalue = 0;
        pmsg->dvalue = 0.;
    }
    pmsg->interface = int32Type;
    pasynUser->userData = pmsg;

    switch (command) {
    case mcaStartAcquire:
        break;
    case mcaStopAcquire:
        break;
    case mcaErase:
        break;
    case mcaData:
        /* start read operation */
        /* Set the flag which tells the record that the read is not complete */
        pmca->rdng = 1;
        pmca->pact = 1;
        break;
    case mcaReadStatus:
        /* Read the current status of the device */
        /* Set the flag which tells the record that the read status is not
           complete */
        pmca->rdns = 1;
        pmca->pact = 1;
        break;
    case mcaChannelAdvanceSource:
        break;
    case mcaNumChannels:
        break;
    case mcaDwellTime:
        pmsg->interface = float64Type;
        break;
    case mcaPresetLiveTime:
        pmsg->interface = float64Type;
        break;
    case mcaPresetRealTime:
        pmsg->interface = float64Type;
        break;
    case mcaPresetCounts:
        pmsg->interface = float64Type;
        break;
    case mcaPresetLowChannel:
        break;
    case mcaPresetHighChannel:
        break;
    case mcaPresetSweeps:
        break;
    case mcaAcquireMode:
        break;
    case mcaSequence:
        break;
    case mcaPrescale:
        break;
    default:
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::send_msg, %s invalid command=%d\n", 
                  pmca->name, command);
    }
    /* Queue asyn request, so we get a callback when driver is ready */
    status = pasynManager->queueRequest(pasynUser, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devMcaAsyn::send_msg: %s error calling queueRequest, %s\n", 
                  pmca->name, pasynUser->errorMessage);
        return(-1);
    }
    return(0);
}


static void asynCallback(asynUser *pasynUser)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pasynUser->userPvt;
    mcaRecord *pmca = pPvt->pmca;
    mcaAsynMessage *pmsg = pasynUser->userData;
    rset *prset = (rset *)pmca->rset;
    int status;

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "devMcaAsyn::asynCallback: %s command=%d, ivalue=%d, dvalue=%f\n",
              pmca->name, pmsg->command, pmsg->ivalue, pmsg->dvalue);
    pasynUser->reason = pPvt->driverReasons[pmsg->command];

    if (pmsg->command == mcaData) {
        /* Read data */
       pPvt->pasynInt32Array->read(pPvt->asynInt32ArrayPvt, pasynUser, 
                                   pPvt->data, pmca->nuse, &pPvt->nread);
       dbScanLock((dbCommon *)pmca);
       (*prset->process)(pmca);
       dbScanUnlock((dbCommon *)pmca);


    } else if (pmsg->command == mcaReadStatus) {
        /* Read the current status of the device */
       pPvt->pasynInt32->write(pPvt->asynInt32Pvt, pasynUser, 0);
       pasynUser->reason = pPvt->driverReasons[mcaAcquiring];
       pPvt->pasynInt32->read(pPvt->asynInt32Pvt, pasynUser, &pPvt->acquiring);
       pasynUser->reason = pPvt->driverReasons[mcaElapsedLiveTime];
       pPvt->pasynFloat64->read(pPvt->asynFloat64Pvt, pasynUser, 
                                &pPvt->elapsedLive);
       pasynUser->reason = pPvt->driverReasons[mcaElapsedRealTime];
       pPvt->pasynFloat64->read(pPvt->asynFloat64Pvt, pasynUser, 
                                &pPvt->elapsedReal);
       pasynUser->reason = pPvt->driverReasons[mcaElapsedCounts];
       pPvt->pasynFloat64->read(pPvt->asynFloat64Pvt, pasynUser, 
                                &pPvt->totalCounts);
       pasynUser->reason = pPvt->driverReasons[mcaDwellTime];
       pPvt->pasynFloat64->read(pPvt->asynFloat64Pvt, pasynUser, 
                                &pPvt->dwellTime);
       dbScanLock((dbCommon *)pmca);
       (*prset->process)(pmca);
       dbScanUnlock((dbCommon *)pmca);     
    } else {
        if (pmsg->interface == int32Type) {
            pPvt->pasynInt32->write(pPvt->asynInt32Pvt, pasynUser,
                                    pmsg->ivalue);
        } else {
            pPvt->pasynFloat64->write(pPvt->asynFloat64Pvt, pasynUser,
                                      pmsg->dvalue);
        }
    }
    pasynManager->memFree(pmsg, sizeof(*pmsg));
    status = pasynManager->freeAsynUser(pasynUser);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devMcaAsyn::asynCallback: %s error in freeAsynUser, %s\n",
                  pmca->name, pasynUser->errorMessage);
    }
}


static long read_array(mcaRecord *pmca)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pmca->dpvt;
    asynUser *pasynUser = pPvt->pasynUser;

    /* Copy data from private buffer to record */
    memcpy(pmca->bptr, pPvt->data, pPvt->nread*sizeof(long));
    pmca->udf=0;
    pmca->nord = pPvt->nread;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "devMcaAsyn::read_value, record=%s, nord=%d\n",
              pmca->name, pmca->nord);
    return(0);
}

