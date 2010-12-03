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
#include <epicsMutex.h>
#include <epicsString.h>
#include <errlog.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <asynDriver.h>
#include <asynInt32.h>
#include <asynInt32Array.h>
#include <asynFloat64.h>
#include <asynDrvUser.h>

#include "mca.h"
#include "drvMca.h"
#include <epicsExport.h>

typedef struct {
    char *portName;
    char *inputName;
    char *dataString;
    char *intervalString;
    int maxSignals;
    int maxPoints;
    int numPoints;
    int acquiring;
    int numAcquired;
    double realTime;
    double liveTime;
    double elapsedTime;
    double dwellTime;
    epicsTimeStamp startTime;
    epicsMutexId mutexId;
    double callbackInterval;
    int *pData;
    int numAverage;
    int accumulated;
    double *pAverageStore;
    asynInterface common;
    asynInterface int32;
    asynInterface float64;
    asynInterface int32Array;
    asynInterface drvUser;
} fastSweepPvt;

typedef struct {
    mcaCommand command;
    char *commandString;
} mcaCommandStruct;

static mcaCommandStruct mcaCommands[MAX_MCA_COMMANDS] = {
    {mcaStartAcquire,           mcaStartAcquireString},           /* int32, write */
    {mcaStopAcquire,            mcaStopAcquireString},            /* int32, write */
    {mcaErase,                  mcaEraseString},                  /* int32, write */
    {mcaData,                   mcaDataString},                   /* int32Array, read/write */
    {mcaReadStatus,             mcaReadStatusString},             /* int32, write */
    {mcaChannelAdvanceInternal, mcaChannelAdvanceInternalString}, /* int32, write */
    {mcaChannelAdvanceExternal, mcaChannelAdvanceExternalString}, /* int32, write */
    {mcaNumChannels,            mcaNumChannelsString},            /* int32, write */
    {mcaDwellTime,              mcaDwellTimeString},              /* float64, write */
    {mcaPresetLiveTime,         mcaPresetLiveTimeString},         /* float64, write */
    {mcaPresetRealTime,         mcaPresetRealTimeString},         /* float64, write */
    {mcaPresetCounts,           mcaPresetCountsString},           /* float64, write */
    {mcaPresetLowChannel,       mcaPresetLowChannelString},       /* int32, write */
    {mcaPresetHighChannel,      mcaPresetHighChannelString},      /* int32, write */
    {mcaPresetSweeps,           mcaPresetSweepsString},           /* int32, write */
    {mcaModePHA,                mcaModePHAString},                /* int32, write */
    {mcaModeMCS,                mcaModeMCSString},                /* int32, write */
    {mcaModeList,               mcaModeListString},               /* int32, write */
    {mcaSequence,               mcaSequenceString},               /* int32, write */
    {mcaPrescale,               mcaPrescaleString},               /* int32, write */
    {mcaAcquiring,              mcaAcquiringString},              /* int32, read */
    {mcaElapsedLiveTime,        mcaElapsedLiveTimeString},        /* float64, read */
    {mcaElapsedRealTime,        mcaElapsedRealTimeString},        /* float64, read */
    {mcaElapsedCounts,          mcaElapsedCountsString}           /* float64, read */
};

/* These are callback functions, called from driver */
static void dataCallback(void *drvPvt, asynUser *pasynUser, epicsInt32 *data, 
                         size_t nelem);
static void intervalCallback(void *drvPvt, asynUser *pasynUser, double seconds);

/* These are private functions, not in any interface */
static void nextPoint(fastSweepPvt *pPvt, int *newData);
static void computeNumAverage(fastSweepPvt *pPvt);
static asynStatus fastSweepWrite(void *drvPvt, asynUser *pasynUser,
                                 epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus fastSweepRead(void *drvPvt, asynUser *pasynUser,
                                epicsInt32 *pivalue, epicsFloat64 *pfvalue);

/* These functions are in the asynInt32, asynFloat64 and asynInt32Array
 * interfaces */
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

/* These functions are in the asynCommon interface */
static void report                  (void *drvPvt, FILE *fp, int details);
static asynStatus connect           (void *drvPvt, asynUser *pasynUser);
static asynStatus disconnect         (void *drvPvt, asynUser *pasynUser);

/* asynCommon methods */
static struct asynCommon fastSweepCommon = {
    report,
    connect,
    disconnect
};

/* asynInt32 methods */
static asynInt32 fastSweepInt32 = {
    int32Write,
    int32Read,
    getBounds,
    NULL,
    NULL
};

/* asynFloat64 methods */
static asynFloat64 fastSweepFloat64 = {
    float64Write,
    float64Read,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array fastSweepInt32Array = {
    int32ArrayWrite,
    int32ArrayRead,
    NULL,
    NULL
};

static asynDrvUser fastSweepDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


int initFastSweep(const char *portName, const char *inputName, 
                  int maxSignals, int maxPoints, char *dataString, char *intervalString)
{
    fastSweepPvt *pPvt;
    asynStatus status;
    asynInterface *pasynInterface;
    asynUser *pasynUser;
    asynFloat64 *pfloat64;
    void *float64Pvt;
    asynInt32Array *pint32Array;
    void *int32ArrayPvt;
    asynDrvUser *pdrvUser;
    void *drvUserPvt;
    const char *ptypeName;
    size_t psize;
    void *registrarPvt;

    pPvt = callocMustSucceed(1, sizeof(*pPvt), "initFastSweep");
    pPvt->maxSignals = maxSignals;
    pPvt->maxPoints = maxPoints;
    pPvt->portName = epicsStrDup(portName);
    pPvt->inputName = epicsStrDup(inputName);
    if ((dataString != NULL) && (strlen(dataString) != 0)) {
        pPvt->dataString = epicsStrDup(dataString);
    } else {
        pPvt->dataString = epicsStrDup("DATA");
    }
    if ((intervalString != NULL) && (strlen(intervalString) != 0)) {
        pPvt->intervalString = epicsStrDup(intervalString);
    } else {
        pPvt->intervalString = epicsStrDup("SCAN_PERIOD");
    }
    epicsTimeGetCurrent(&pPvt->startTime);

    pPvt->mutexId = epicsMutexCreate();
    pPvt->pData = (int *)callocMustSucceed(pPvt->maxPoints*pPvt->maxSignals,
                                           sizeof(int), "initFastSweep");
    pPvt->pAverageStore = (double *)callocMustSucceed(pPvt->maxSignals,
                                           sizeof(double), "initFastSweep");
    /*  Link with higher level routines */
    status = pasynManager->registerPort(portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1, /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register myself.\n");
        return -1;
    }
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&fastSweepCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&fastSweepInt32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&fastSweepFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&fastSweepInt32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&fastSweepDrvUser;
    pPvt->drvUser.drvPvt = pPvt;


    status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register common.\n");
        return -1;
    }
    status = pasynInt32Base->initialize(pPvt->portName, &pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register int32.\n");
        return -1;
    }

    status = pasynInt32Base->initialize(pPvt->portName, &pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register float64.\n");
        return -1;
    }

    status = pasynInt32ArrayBase->initialize(pPvt->portName, &pPvt->int32Array);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep: Can't register int32Array.\n");
        return -1;
    }

    status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("initFastSweep ERROR: Can't register drvUser\n");
        return -1;
    }

    pasynUser = pasynManager->createAsynUser(0,0);
    status = pasynManager->connectDevice(pasynUser, inputName, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "drvFastSweep::initFastSweep, connectDevice failed\n");
        return -1;
    }
    /* Get the asynDrvUser interface */
    pasynInterface = pasynManager->findInterface(pasynUser, 
                                                 asynDrvUserType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "initFastSweep, find asynDrvUser"
                  " interface failed for input %s\n",
                  inputName);
        return -1;
    }
    pdrvUser = (asynDrvUser *)pasynInterface->pinterface;
    drvUserPvt = pasynInterface->drvPvt;

    /* Get the asynInt32Array interface */
    pasynInterface = pasynManager->findInterface(pasynUser, 
                                                 asynInt32ArrayType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "initFastSweep, find asynInt32Array interface failed"
                  " for input %s\n",
                  inputName);
        return -1;
    }
    pint32Array = (asynInt32Array *)pasynInterface->pinterface;
    int32ArrayPvt = pasynInterface->drvPvt;

    /* Configure the asynUser for data command */
    status = pdrvUser->create(drvUserPvt, pasynUser, pPvt->dataString, &ptypeName, &psize);
    if (status) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "initFastSweep, error in drvUser for %s\n", pPvt->dataString);
        return(-1);
    }
    pint32Array->registerInterruptUser(int32ArrayPvt, pasynUser, 
                                       dataCallback, pPvt, &registrarPvt);

    /* Get the asynFloat64 interface */
    pasynUser = pasynManager->duplicateAsynUser(pasynUser,0,0);
    pasynInterface = pasynManager->findInterface(pasynUser,
                                                 asynFloat64Type, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "initFastSweep, find asynFloat64"
                  " interface failed for input %s\n",
                  inputName);
        return -1;
    }
    pfloat64 = (asynFloat64 *)pasynInterface->pinterface;
    float64Pvt = pasynInterface->drvPvt;
    /* Configure the asynUser for callback interval command */
    status = pdrvUser->create(drvUserPvt, pasynUser, pPvt->intervalString, &ptypeName, &psize);
    if (status) {
        /* This driver does not support this command.  Just set numAverage=1 */
        pPvt->numAverage = 1;
    } else {
        pfloat64->registerInterruptUser(float64Pvt, pasynUser, 
                                        intervalCallback, pPvt, &registrarPvt);
        status = pfloat64->read(float64Pvt, pasynUser, &pPvt->callbackInterval);
    }
    return(0);
}

static void intervalCallback(void *drvPvt, asynUser *pasynUser, double seconds)
{
    /* This is callback function that is called from the port-specific driver */
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;

    epicsMutexLock(pPvt->mutexId);
    pPvt->callbackInterval = seconds;
    computeNumAverage(pPvt);
    epicsMutexUnlock(pPvt->mutexId);
}

static void dataCallback(void *drvPvt, asynUser *pasynUser, 
                         epicsInt32 *newData, size_t nelem)
{
    /* This is callback function that is called from the port-specific driver */
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    int i;

    if (!pPvt->acquiring) return;

    epicsMutexLock(pPvt->mutexId);
    /* No need to average if collecting every point */
    if (pPvt->numAverage == 1) {
        nextPoint(pPvt, newData);
        goto done;
    }
    for (i=0; i<pPvt->maxSignals; i++) pPvt->pAverageStore[i] += newData[i];
    if (++(pPvt->accumulated) < pPvt->numAverage) goto done;
    /* We have now collected the desired number of points to average */
    for (i=0; i<pPvt->maxSignals; i++) newData[i] = 0.5 +
                                       pPvt->pAverageStore[i]/pPvt->accumulated;
    nextPoint(pPvt, newData);
    for (i=0; i<pPvt->maxSignals; i++) pPvt->pAverageStore[i] = 0;
    pPvt->accumulated = 0;
done:
    epicsMutexUnlock(pPvt->mutexId);
}


static void nextPoint(fastSweepPvt *pPvt, epicsInt32 *newData)
{
    int i;
    int offset;
    epicsTimeStamp now;

    if (!pPvt->acquiring) return;
    
    offset = pPvt->numAcquired;
    for (i = 0; i < pPvt->maxSignals; i++) {
        pPvt->pData[offset] = newData[i];
        offset += pPvt->maxPoints;
    }
    pPvt->numAcquired++;
    if (pPvt->numAcquired >= pPvt->numPoints) {
       pPvt->acquiring = 0;
    }
    epicsTimeGetCurrent(&now);
    pPvt->elapsedTime = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
    if ((pPvt->realTime > 0) && (pPvt->elapsedTime >= pPvt->realTime))
        pPvt->acquiring = 0;
    if ((pPvt->liveTime > 0) && (pPvt->elapsedTime >= pPvt->liveTime))
        pPvt->acquiring = 0;
}

static void computeNumAverage(fastSweepPvt *pPvt)
{
    pPvt->numAverage = (int) (pPvt->dwellTime/pPvt->callbackInterval + 0.5);
    if (pPvt->numAverage < 1) pPvt->numAverage = 1;
    pPvt->accumulated = 0;
}

static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 value)
{
    return(fastSweepWrite(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    return(fastSweepWrite(drvPvt, pasynUser, 0, value));
}

static asynStatus fastSweepWrite(void *drvPvt, asynUser *pasynUser,
                                      epicsInt32 ivalue, epicsFloat64 dvalue)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    mcaCommand command = pasynUser->reason;
    asynStatus status=asynSuccess;

    epicsMutexLock(pPvt->mutexId);
    switch (command) {
        case mcaStartAcquire:
            if (!pPvt->acquiring) {
                pPvt->acquiring = 1;
                epicsTimeGetCurrent(&pPvt->startTime);
            }
            break;
        case mcaStopAcquire:
            pPvt->acquiring = 0;
            break;
        case mcaErase:
            memset(pPvt->pData, 0, pPvt->maxPoints * pPvt->maxSignals * 
                                    sizeof(int));
            pPvt->numAcquired = 0;
            /* Reset the elapsed time */
            pPvt->elapsedTime = 0;
            epicsTimeGetCurrent(&pPvt->startTime);
            break;
        case mcaReadStatus:
            /* No-op for fastSweep */
            break;
        case mcaChannelAdvanceInternal:
            /* No-op for fastSweep */
            break;
        case mcaChannelAdvanceExternal:
            /* No-op for fastSweep */
            break;
        case mcaNumChannels:
            if ((ivalue < 1) || (ivalue > pPvt->maxPoints))
                status = asynError;
            else
                pPvt->numPoints = ivalue;
            break;
        case mcaModePHA:
            /* No-op for fastSweep */
            break;
        case mcaModeMCS:
            /* No-op for fastSweep */
            break;
        case mcaModeList:
            /* No-op for fastSweep */
            break;
        case mcaSequence:
            /* No-op for fastSweep */
            break;
        case mcaPrescale:
            /* No-op for fastSweep */
            break;
        case mcaPresetSweeps:
            /* No-op for fastSweep */
            break;
        case mcaPresetLowChannel:
            /* No-op for fastSweep */
            break;
        case mcaPresetHighChannel:
            /* No-op for fastSweep */
            break;
        case mcaDwellTime:
            pPvt->dwellTime = dvalue;
            computeNumAverage(pPvt);
            break;
        case mcaPresetRealTime:
            pPvt->realTime = dvalue;
            break;
        case mcaPresetLiveTime:
            pPvt->liveTime = dvalue;
            break;
        case mcaPresetCounts:
            /* No-op for fastSweep */
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "fastSweep::fastSweepWrite got illegal command %d\n",
                      command);
            status = asynError;
            break;
    }
    epicsMutexUnlock(pPvt->mutexId);
    return(status);
}

static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *value)
{
    return(fastSweepRead(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    return(fastSweepRead(drvPvt, pasynUser, NULL, value));
}

static asynStatus fastSweepRead(void *drvPvt, asynUser *pasynUser,
                                epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    mcaCommand command = pasynUser->reason;
    asynStatus status=asynSuccess;

    epicsMutexLock(pPvt->mutexId);
    switch (command) {
        case mcaAcquiring:
            *pivalue = pPvt->acquiring;
            break;
        case mcaDwellTime:
            *pfvalue = pPvt->callbackInterval * pPvt->numAverage;
            break;
        case mcaElapsedLiveTime:
            *pfvalue = pPvt->elapsedTime;
            break;
        case mcaElapsedRealTime:
            *pfvalue = pPvt->elapsedTime;
            break;
        case mcaElapsedCounts:
            *pfvalue = 0.;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "fastSweep::fastSweepRead got illegal command %d\n",
                      command);
            status = asynError;
            break;
    }
    epicsMutexUnlock(pPvt->mutexId);
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
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    int signal;

    epicsMutexLock(pPvt->mutexId);
    pasynManager->getAddr(pasynUser, &signal);
    memcpy(data, &pPvt->pData[pPvt->maxPoints*signal], 
           pPvt->numPoints*sizeof(int));
    *nactual = pPvt->numPoints;
    epicsMutexUnlock(pPvt->mutexId);
    return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
                                  epicsInt32 *data, size_t maxChans)
{
    fastSweepPvt *pPvt = (fastSweepPvt *)drvPvt;
    int signal;

    if (maxChans > pPvt->maxPoints) maxChans = pPvt->maxPoints;

    epicsMutexLock(pPvt->mutexId);
    pasynManager->getAddr(pasynUser, &signal);
    memcpy(&pPvt->pData[pPvt->maxPoints*signal], data,
           maxChans*sizeof(int));
    epicsMutexUnlock(pPvt->mutexId);
    return(asynSuccess);
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    int i;
    const char *pstring;

    for (i=0; i<MAX_MCA_COMMANDS; i++) {
        pstring = mcaCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = mcaCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(mcaCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvFastSweep::drvUserCreate, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvFastSweep::drvUserCreate, unknown command=%s\n", drvInfo);
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
static const iocshArg initSweepArg4 = { "dataString",iocshArgString};
static const iocshArg initSweepArg5 = { "intervalString",iocshArgString};
static const iocshArg * const initSweepArgs[6] = {&initSweepArg0,
                                                  &initSweepArg1,
                                                  &initSweepArg2,
                                                  &initSweepArg3,
                                                  &initSweepArg4,
                                                  &initSweepArg5};
static const iocshFuncDef initSweepFuncDef = {"initFastSweep",6,initSweepArgs};
static void initSweepCallFunc(const iocshArgBuf *args)
{
    initFastSweep(args[0].sval, args[1].sval, args[2].ival, args[3].ival,
                  args[4].sval, args[5].sval);
}

void fastSweepRegister(void)
{
    iocshRegister(&initSweepFuncDef,initSweepCallFunc);
}

epicsExportRegistrar(fastSweepRegister);

