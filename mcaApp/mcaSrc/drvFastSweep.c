/*  fastSweep.cc

    Author: Mark Rivers

    8-July-2004  MLR  Converted from MPF to aysn and from C++ to C

    These routines implement the asynMca interface, and use the asynFastSweep
    interface.
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsTime.h>
#include <epicsTypes.h>
#include <epicsThread.h>
#include <epicsExport.h>
#include <epicsString.h>
#include <errlog.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <asynDriver.h>
#include <asynInt32ArrayCallback.h>

#include "mca.h"
#include "asynMca.h"

typedef struct {
    char *portName;
    char *inputName;
    int maxSignals;
    int maxPoints;
    int numPoints;
    int acquiring;
    int numAcquired;
    double realTime;
    double liveTime;
    double elapsedTime;
    epicsTimeStamp startTime;
    int *pData;
    int numAverage;
    int accumulated;
    double *pAverageStore;
    asynInterface common;
    asynInterface mca;
    asynInt32ArrayCallback *pint32ArrayCallback;
    void *int32ArrayCallbackPvt;
    asynUser *pasynUser;
} fastSweepPvt;

static void callback(void *drvPvt, epicsInt32 *data);
static void nextPoint(fastSweepPvt *drvPvt, int *newData);
static asynStatus command(void *drvPvt, asynUser *pasynUser,
                          mcaCommand command,
                          int ivalue, double dvalue);
static asynStatus readStatus(void *drvPvt, asynUser *pasynUser,
                             mcaAsynAcquireStatus *pstat);
static asynStatus readData(void *drvPvt, asynUser *pasynUser,
                           int maxChans, int *nactual, int *data);
static void report(void *drvPvt, FILE *fp, int details);
static asynStatus connect(void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect(void *drvPvt, asynUser *pasynUser);

/* asynCommon methods */
static const struct asynCommon fastSweepCommon = {
    report,
    connect,
    disconnect
};

/* asynMca methods */
static const asynMca fastSweepMca = {
    command,
    readStatus,
    readData
};


int initFastSweep(const char *portName, const char *inputName, 
                  int maxSignals, int maxPoints)
{
    fastSweepPvt *pPvt;
    asynStatus status;
    asynInterface *pasynInterface;

    pPvt = callocMustSucceed(1, sizeof(*pPvt), "initFastSweep");
    pPvt->maxSignals = maxSignals;
    pPvt->maxPoints = maxPoints;
    pPvt->portName = epicsStrDup(portName);
    pPvt->inputName = epicsStrDup(inputName);
    epicsTimeGetCurrent(&pPvt->startTime);

    pPvt->pData = (int *)callocMustSucceed(pPvt->maxPoints*pPvt->maxSignals,
                                           sizeof(int), "initFastSweep");
    pPvt->pAverageStore = (double *)callocMustSucceed(pPvt->maxSignals,
                                           sizeof(double), "initFastSweep");
    /*  Link with higher level routines */
    status = pasynManager->registerPort(portName,
                                        1, /* is multiDevice */
                                        1, /* autoconnect */
                                        epicsThreadPriorityMedium,
                                        0); /* stack size */
    if (status != asynSuccess) {
        errlogPrintf("initQuadEM: Can't register myself.\n");
        return -1;
    }
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&fastSweepCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->mca.interfaceType = asynMcaType;
    pPvt->mca.pinterface  = (void *)&fastSweepMca;
    pPvt->mca.drvPvt = pPvt;

    status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register common.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName, &pPvt->mca);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register mca.\n");
        return -1;
    }

    pPvt->pasynUser = pasynManager->createAsynUser(0,0);
    status = pasynManager->connectDevice(pPvt->pasynUser, inputName, 0);
    if (status != asynSuccess) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                  "drvFastSweep::initFastSweep, connectDevice failed\n");
        return -1;
    }
    /* Get the asynInt32Callback interface */
    pasynInterface = pasynManager->findInterface(pPvt->pasynUser, 
                                                 asynInt32ArrayCallbackType, 1);
    if (!pasynInterface) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                  "initFastSweep, find asynInt32ArrayCallback"
                  " interface failed for input %s\n",
                  inputName);
        return -1;
    }
    pPvt->pint32ArrayCallback = 
                          (asynInt32ArrayCallback *)pasynInterface->pinterface;
    pPvt->int32ArrayCallbackPvt = pasynInterface->drvPvt;

    pPvt->pint32ArrayCallback->registerCallback(pPvt->int32ArrayCallbackPvt, 
                                                pPvt->pasynUser, callback, pPvt);
    return(0);
}

static void callback(void *drvPvt, epicsInt32 *newData)
{
    /* This is callback function that is called from the port-specific driver */
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    int data[pPvt->maxSignals];
    int i;

    if (!pPvt->acquiring) return;

    /* No need to average if collecting every point */
    if (pPvt->numAverage == 1) {
        nextPoint(pPvt, newData);
        return;
    }
    for (i=0; i<pPvt->maxSignals; i++) pPvt->pAverageStore[i] += newData[i];
    if (++(pPvt->accumulated) < pPvt->numAverage) return;
    /* We have now collected the desired number of points to average */
    for (i=0; i<pPvt->maxSignals; i++) data[i] = 0.5 +
                                       pPvt->pAverageStore[i]/pPvt->accumulated;
    nextPoint(pPvt, data);
    for (i=0; i<pPvt->maxSignals; i++) pPvt->pAverageStore[i] = 0;
    pPvt->accumulated = 0;
}


static void nextPoint(fastSweepPvt *pPvt, epicsInt32 *newData)
{
    int i;
    int offset;
    epicsTimeStamp now;

    if (pPvt->numAcquired >= pPvt->numPoints) {
       pPvt->acquiring = 0;
    }
    if (!pPvt->acquiring) return;
    
    offset = pPvt->numAcquired;
    for (i = 0; i < pPvt->maxSignals; i++) {
        pPvt->pData[offset] = newData[i];
        offset += pPvt->maxPoints;
    }
    pPvt->numAcquired++;
    epicsTimeGetCurrent(&now);
    pPvt->elapsedTime = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
    if ((pPvt->realTime > 0) && (pPvt->elapsedTime >= pPvt->realTime))
        pPvt->acquiring = 0;
    if ((pPvt->liveTime > 0) && (pPvt->elapsedTime >= pPvt->liveTime))
        pPvt->acquiring = 0;
}


static asynStatus command(void *drvPvt, asynUser *pasynUser,
                          mcaCommand command,
                          int ivalue, double dvalue)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    asynStatus status=asynSuccess;

    switch (command) {
        case MSG_ACQUIRE:
            if (!pPvt->acquiring) {
                pPvt->acquiring = 1;
                epicsTimeGetCurrent(&pPvt->startTime);
            }
            break;
        case MSG_SET_CHAS_INT:
            /* No-op for fastSweep */
            break;
        case MSG_SET_CHAS_EXT:
            /* No-op for fastSweep */
            break;
        case MSG_SET_NCHAN:
            if ((ivalue < 1) || (ivalue > pPvt->maxPoints)) return(asynError);
            pPvt->numPoints = ivalue;
            break;
        case MSG_SET_DWELL:
            pPvt->numAverage = (int) (dvalue /
                 pPvt->pint32ArrayCallback->getCallbackInterval(
                                                  pPvt->int32ArrayCallbackPvt,
                                                  pPvt->pasynUser) + 0.5);
            if (pPvt->numAverage < 1) pPvt->numAverage = 1;
            pPvt->accumulated = 0;
            break;
        case MSG_SET_REAL_TIME:
            pPvt->realTime = dvalue;
            break;
        case MSG_SET_LIVE_TIME:
            pPvt->liveTime = dvalue;
            break;
        case MSG_SET_COUNTS:
            /* No-op for fastSweep */
            break;
        case MSG_SET_LO_CHAN:
            /* No-op for fastSweep */
            break;
        case MSG_SET_HI_CHAN:
            /* No-op for fastSweep */
            break;
        case MSG_SET_NSWEEPS:
            /* No-op for fastSweep */
            break;
        case MSG_SET_MODE_PHA:
            /* No-op for fastSweep */
            break;
        case MSG_SET_MODE_MCS:
            /* No-op for fastSweep */
            break;
        case MSG_SET_MODE_LIST:
            /* No-op for fastSweep */
            break;
        case MSG_STOP_ACQUISITION:
            pPvt->acquiring = 0;
            break;
        case MSG_ERASE:
            memset(pPvt->pData, 0, pPvt->maxPoints * pPvt->maxSignals * 
                                    sizeof(int));
            pPvt->numAcquired = 0;
            /* Reset the elapsed time */
            pPvt->elapsedTime = 0;
            epicsTimeGetCurrent(&pPvt->startTime);
            break;
        case MSG_SET_SEQ:
            /* No-op for fastSweep */
            break;
        case MSG_SET_PSCL:
            /* No-op for fastSweep */
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "fastSweep::command got illegal command %d\n",
                      command);
            break;
    }
    return(status);
}

   
static asynStatus readStatus(void *drvPvt, asynUser *pasynUser,
                             mcaAsynAcquireStatus *pstat)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;

    pstat->realTime = pPvt->elapsedTime;
    pstat->liveTime = pPvt->elapsedTime;
    pstat->dwellTime = pPvt->pint32ArrayCallback->getCallbackInterval(
                                                   pPvt->int32ArrayCallbackPvt,
                                                   pPvt->pasynUser) * 
                                               pPvt->numAverage;
    pstat->acquiring = pPvt->acquiring;
    pstat->totalCounts = 0;
    return(asynSuccess);
}

static asynStatus readData(void *drvPvt, asynUser *pasynUser,
                           int maxChans, int *nactual, int *data)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    int signal;

    pasynManager->getAddr(pasynUser, &signal);
    memcpy(data, &pPvt->pData[pPvt->maxPoints*signal], 
           pPvt->numPoints*sizeof(int));
    *nactual = pPvt->numPoints;
    return(asynSuccess);
}


/* asynCommon routines */

/* Report  parameters */
static void report(void *drvPvt, FILE *fp, int details)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;

    fprintf(fp, "fastSweep %s: connected, input=%s\n",
            pPvt->portName, pPvt->inputName);
    if (details >= 1) {
        fprintf(fp, "    maxPoints=%d, maxSignals=%d\n", 
                pPvt->maxPoints, pPvt->maxSignals);
    }
}

/* Connect */
static asynStatus connect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Disconnect */
static asynStatus disconnect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


static const iocshArg initSweepArg0 = { "portName",iocshArgString};
static const iocshArg initSweepArg1 = { "inputName",iocshArgString};
static const iocshArg initSweepArg2 = { "maxSignals",iocshArgInt};
static const iocshArg initSweepArg3 = { "maxPoints",iocshArgInt};
static const iocshArg * const initSweepArgs[4] = {&initSweepArg0,
                                                  &initSweepArg1,
                                                  &initSweepArg2,
                                                  &initSweepArg3};
static const iocshFuncDef initSweepFuncDef = {"initSweep",4,initSweepArgs};
static void initSweepCallFunc(const iocshArgBuf *args)
{
    initFastSweep(args[0].sval, args[1].sval, args[2].ival, args[3].ival);
}

void fastSweepRegister(void)
{
    iocshRegister(&initSweepFuncDef,initSweepCallFunc);
}

epicsExportRegistrar(fastSweepRegister);

