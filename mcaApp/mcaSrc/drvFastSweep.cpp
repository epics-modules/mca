/*  drvastSweep.cpp

    Author: Mark Rivers

    18-June-2012  Converted from drvFastSweep.c to asynPortDriver based driver

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

#include <asynFloat64SyncIO.h>

#include <drvMca.h>

#include <drvFastSweep.h>
#include <epicsExport.h>

static const char *driverName = "drvFastSweep";

static void dataCallbackC(void *drvPvt, asynUser *pasynUser, 
                         epicsInt32 *newData, size_t nelem)
{
    drvFastSweep *pPvt = (drvFastSweep *)drvPvt;
    
    pPvt->dataCallback(newData, nelem);
}

static void intervalCallbackC(void *drvPvt, asynUser *pasynUser, double seconds)
{
    drvFastSweep *pPvt = (drvFastSweep *)drvPvt;
    
    pPvt->intervalCallback(seconds);
}


drvFastSweep::drvFastSweep(const char *portName, const char *inputName, 
                           int maxSignals, int maxPoints, 
                           const char *dataString, const char *intervalString)
   : asynPortDriver(portName, 
                    maxSignals,
                    NUM_FAST_SWEEP_PARAMS,
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask,                   /* Interrupt mask */
                    ASYN_MULTIDEVICE, /* asynFlags.  This driver does not block and it is multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0), /* Default stack size*/
     int32ArrayRegistrarPvt_(NULL)
{
    const char *functionName = "drvFastSweep";
    
    asynStatus status;
    asynInterface *pasynInterface;
    asynFloat64 *pfloat64;
    void *float64Pvt;
    asynDrvUser *pdrvUser;
    void *drvUserPvt;
    const char *ptypeName;
    size_t psize;

    // Uncomment this line to enable asynTraceFlow during the constructor
    //pasynTrace->setTraceMask(pasynUserSelf, 0x11);

    createParam(mcaStartAcquireString,                asynParamInt32, &mcaStartAcquire_);
    createParam(mcaStopAcquireString,                 asynParamInt32, &mcaStopAcquire_);            /* int32, write */
    createParam(mcaEraseString,                       asynParamInt32, &mcaErase_);                  /* int32, write */
    createParam(mcaDataString,                        asynParamInt32, &mcaData_);                   /* int32Array, read/write */
    createParam(mcaReadStatusString,                  asynParamInt32, &mcaReadStatus_);             /* int32, write */
    createParam(mcaChannelAdvanceSourceString,        asynParamInt32, &mcaChannelAdvanceSource_);   /* int32, write */
    createParam(mcaNumChannelsString,                 asynParamInt32, &mcaNumChannels_);            /* int32, write */
    createParam(mcaDwellTimeString,                 asynParamFloat64, &mcaDwellTime_);              /* float64, write */
    createParam(mcaPresetLiveTimeString,            asynParamFloat64, &mcaPresetLiveTime_);         /* float64, write */
    createParam(mcaPresetRealTimeString,            asynParamFloat64, &mcaPresetRealTime_);         /* float64, write */
    createParam(mcaPresetCountsString,              asynParamFloat64, &mcaPresetCounts_);           /* float64, write */
    createParam(mcaPresetLowChannelString,            asynParamInt32, &mcaPresetLowChannel_);       /* int32, write */
    createParam(mcaPresetHighChannelString,           asynParamInt32, &mcaPresetHighChannel_);      /* int32, write */
    createParam(mcaPresetSweepsString,                asynParamInt32, &mcaPresetSweeps_);           /* int32, write */
    createParam(mcaAcquireModeString,                 asynParamInt32, &mcaAcquireMode_);            /* int32, write */
    createParam(mcaSequenceString,                    asynParamInt32, &mcaSequence_);               /* int32, write */
    createParam(mcaPrescaleString,                    asynParamInt32, &mcaPrescale_);               /* int32, write */
    createParam(mcaAcquiringString,                   asynParamInt32, &mcaAcquiring_);              /* int32, read */
    createParam(mcaElapsedLiveTimeString,           asynParamFloat64, &mcaElapsedLiveTime_);        /* float64, read */
    createParam(mcaElapsedRealTimeString,           asynParamFloat64, &mcaElapsedRealTime_);        /* float64, read */
    createParam(mcaElapsedCountsString,             asynParamFloat64, &mcaElapsedCounts_);          /* float64, read */
    createParam(fastSweepMaxChannelsString,           asynParamInt32, &fastSweepMaxChannels_);      /* int32, read */
    createParam(fastSweepCurrentChannelString,        asynParamInt32, &fastSweepCurrentChannel_);   /* int32, read */

    maxSignals_ = maxSignals;
    maxPoints_ = maxPoints;
    numPoints_ = 0;
    acquiring_ = 0;
    numAcquired_ = 0;
    realTime_ = 0.;
    elapsedTime_ = 0.;
    dwellTime_ = 0.;
    callbackInterval_ = 0.;
    numAverage_ = 1;
    accumulated_ = 0;
    erased_ = true;
    setIntegerParam(fastSweepMaxChannels_, maxPoints_);
    setIntegerParam(fastSweepCurrentChannel_, 0);
    inputName_ = epicsStrDup(inputName);
    if ((dataString != NULL) && (strlen(dataString) != 0)) {
        dataString_ = epicsStrDup(dataString);
    } else {
        dataString_ = epicsStrDup("DATA");
    }
    if ((intervalString != NULL) && (strlen(intervalString) != 0)) {
        intervalString_ = epicsStrDup(intervalString);
    } else {
        intervalString_ = epicsStrDup("SCAN_PERIOD");
    }
    epicsTimeGetCurrent(&startTime_);

    pData_ = (int *)callocMustSucceed(maxPoints_ * maxSignals_,
                                      sizeof(int), "initFastSweep");
    pAverageStore_ = (double *)callocMustSucceed(maxSignals_,
                                                 sizeof(double), "initFastSweep");
    // Connect to our input driver
    pasynUserInt32Array_ = pasynManager->createAsynUser(0,0);
    status = pasynManager->connectDevice(pasynUserInt32Array_, inputName, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUserInt32Array_, ASYN_TRACE_ERROR,
                  "%s:%s:, connectDevice failed\n",
                  driverName, functionName);
        goto error;
    }
    /* Get the asynDrvUser interface */
    pasynInterface = pasynManager->findInterface(pasynUserInt32Array_, 
                                                 asynDrvUserType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUserInt32Array_, ASYN_TRACE_ERROR,
                  "%s:%s:, find asynDrvUser"
                  " interface failed for input %s\n",
                  driverName, functionName, inputName);
        goto error;
    }
    pdrvUser = (asynDrvUser *)pasynInterface->pinterface;
    drvUserPvt = pasynInterface->drvPvt;

    /* Get the asynInt32Array interface */
    pasynInterface = pasynManager->findInterface(pasynUserInt32Array_, 
                                                 asynInt32ArrayType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUserInt32Array_, ASYN_TRACE_ERROR,
                  "%s:%s:, find asynInt32Array interface failed"
                  " for input %s\n",
                  driverName, functionName, inputName);
        goto error;
    }
    pint32Array_ = (asynInt32Array *)pasynInterface->pinterface;
    int32ArrayPvt_ = pasynInterface->drvPvt;

    /* Configure the asynUser for data command */
    status = pdrvUser->create(drvUserPvt, pasynUserInt32Array_, dataString_, &ptypeName, &psize);
    if (status) {
        asynPrint(pasynUserInt32Array_, ASYN_TRACE_ERROR,
                  "%s:%s:, error in drvUser for %s\n",
                  driverName, functionName, dataString_);
        goto error;
    }

    pint32Array_->registerInterruptUser(int32ArrayPvt_, pasynUserInt32Array_, 
                                        dataCallbackC, this, &int32ArrayRegistrarPvt_);

    /* Get the asynFloat64 interface */
    pasynUserFloat64_ = pasynManager->duplicateAsynUser(pasynUserInt32Array_,0,0);
    pasynInterface = pasynManager->findInterface(pasynUserFloat64_,
                                                 asynFloat64Type, 1);
    if (!pasynInterface) {
        asynPrint(pasynUserFloat64_, ASYN_TRACE_ERROR,
                  "%s:%s:, find asynFloat64"
                  " interface failed for input %s\n",
                  driverName, functionName, inputName);
        goto error;
    }
    pfloat64 = (asynFloat64 *)pasynInterface->pinterface;
    float64Pvt = pasynInterface->drvPvt;
    status = pasynFloat64SyncIO->connect(inputName, 0, &pasynUserFloat64SyncIO_, intervalString_);
    if (status) {
        asynPrint(pasynUserFloat64_, ASYN_TRACE_ERROR,
                  "%s:%s:, cannot connect to asynFloat64SyncIO for input %s\n",
                  driverName, functionName, inputName);
        goto error;
    }
    /* Configure the asynUser for callback interval command */
    status = pdrvUser->create(drvUserPvt, pasynUserFloat64_, intervalString_, &ptypeName, &psize);
    if (status) {
        /* This driver does not support this command.  Just set numAverage=1 */
        numAverage_ = 1;
    } else {
        pfloat64->registerInterruptUser(float64Pvt, pasynUserFloat64_, 
                                        intervalCallbackC, this, &float64RegistrarPvt_);
        status = pasynFloat64SyncIO->read(pasynUserFloat64SyncIO_, &callbackInterval_, 1.0);
    }
    error:
    return;
}

void drvFastSweep::intervalCallback(double seconds)
{
    lock();
    callbackInterval_ = seconds;
    computeNumAverage();
    unlock();
}

void drvFastSweep::dataCallback(epicsInt32 *newData, size_t nelem)
{
    int i;

    if (!acquiring_) return;
    
    lock();

    /* No need to average if collecting every point */
    if (numAverage_ == 1) {
        nextPoint(newData);
        goto done;
    }
    for (i=0; i<maxSignals_; i++) 
        pAverageStore_[i] += newData[i];
    if (++(accumulated_) < numAverage_) goto done;
    /* We have now collected the desired number of points to average */
    for (i=0; i<maxSignals_; i++) 
        newData[i] = (epicsInt32)(0.5 + pAverageStore_[i]/accumulated_);
    nextPoint(newData);
    for (i=0; i<maxSignals_; i++) 
        pAverageStore_[i] = 0;
    accumulated_ = 0;
done:
    unlock();
}


void drvFastSweep::nextPoint(epicsInt32 *newData)
{
    int i;
    int offset;
    epicsTimeStamp now;

    if (!acquiring_) return;

    offset = numAcquired_;
    for (i = 0; i < maxSignals_; i++) {
        pData_[offset] = newData[i];
        offset += maxPoints_;
    }
    numAcquired_++;
    if (numAcquired_ >= numPoints_) {
       stopAcquire();
    }
    epicsTimeGetCurrent(&now);
    elapsedTime_ = epicsTimeDiffInSeconds(&now, &startTime_);
    if ((realTime_ > 0) && (elapsedTime_ >= realTime_)) {
        stopAcquire();
    }
    setIntegerParam(fastSweepCurrentChannel_, numAcquired_);
    setDoubleParam(mcaElapsedRealTime_, elapsedTime_);
    callParamCallbacks();
}

void drvFastSweep::computeNumAverage()
{
    numAverage_ = (int) (dwellTime_/callbackInterval_ + 0.5);
    if (numAverage_ < 1) numAverage_ = 1;
    accumulated_ = 0;
    dwellTime_ = callbackInterval_ * numAverage_;
    setDoubleParam(mcaDwellTime_, dwellTime_);
    callParamCallbacks();
}

void drvFastSweep::stopAcquire()
{
    acquiring_ = 0;
    setIntegerParam(mcaAcquiring_, acquiring_);
}

asynStatus drvFastSweep::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;

    /* Set the parameter in the parameter library. */
    status = setIntegerParam(command, value);
    if (command == mcaStartAcquire_) {
        if (!acquiring_) {
            acquiring_ = 1;
            setIntegerParam(mcaAcquiring_, acquiring_);
            epicsTimeGetCurrent(&startTime_);
        }
    }  
    else if (command == mcaStopAcquire_) {
        stopAcquire();
    }
    else if (command == mcaErase_) {
        memset(pData_, 0, maxPoints_ * maxSignals_ * sizeof(int));
        numAcquired_ = 0;
        /* Reset the elapsed time */
        elapsedTime_ = 0;
        setDoubleParam(mcaElapsedRealTime_, elapsedTime_);
        epicsTimeGetCurrent(&startTime_);
    }
    else if (command == mcaNumChannels_) {
        if ((value < 1) || (value > maxPoints_))
            status = asynError;
        else
            numPoints_ = value;
    }
    callParamCallbacks();
    return(status);
}

asynStatus drvFastSweep::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;

    /* Set the parameter in the parameter library. */
    status = setDoubleParam(command, value);
    if (command == mcaDwellTime_) {
        dwellTime_ = value;
        computeNumAverage();
    }
    else if (command == mcaPresetRealTime_) {
        realTime_ = value;
    }
    callParamCallbacks();
    return(status);
}

asynStatus drvFastSweep::readInt32Array(asynUser *pasynUser,
                                        epicsInt32 *data, size_t maxChans, 
                                        size_t *nactual)
{
    int signal;

    getAddress(pasynUser, &signal);
    memcpy(data, &pData_[maxPoints_*signal], 
           numPoints_*sizeof(int));
    *nactual = numAcquired_;
    return(asynSuccess);
}


/* Report  parameters */
void drvFastSweep::report(FILE *fp, int details)
{
    fprintf(fp, "fastSweep %s: connected, input=%s\n",
            portName, inputName_);
    if (details >= 1) {
        fprintf(fp, "    maxPoints=%d, maxSignals=%d, numAverage=%d, numPoints=%d, "
                    "numAcquired=%d, elapsedTime=%f, acquring_=%d\n", 
                maxPoints_, maxSignals_, numAverage_, numPoints_, numAcquired_, elapsedTime_, acquiring_);
    }
    asynPortDriver::report(fp, details);
}

extern "C" {
int initFastSweep(const char *portName, const char *inputName, 
                  int maxSignals, int maxPoints, 
                  const char *dataString, const char *intervalString)
{
    new drvFastSweep(portName, inputName, maxSignals, maxPoints, dataString, intervalString);
    return asynSuccess;
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

}
