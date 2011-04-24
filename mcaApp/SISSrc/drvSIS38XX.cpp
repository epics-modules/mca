/* File:    drvSIS38XX.cpp
 * Author:  Mark Rivers
 * Date:    22-Apr-2011
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for the SIS3820 and SIS3801 multichannel scalers.  This is for the base class.
 *
 * Acknowledgements:
 * This driver module is based on previous versions by Wayne Lewis and Ulrik Pedersen.
 *
 */

/*******************/
/* System includes */
/*******************/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

/******************/
/* EPICS includes */
/******************/

#include <cantProceed.h>
#include <devLib.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsExport.h>
#include <errlog.h>
#include <iocsh.h>
#include <initHooks.h>

/*******************/
/* Custom includes */
/*******************/

#include "drvMca.h"
#include "devScalerAsyn.h"
#include "drvSIS38XX.h"

static void dmaCallbackC(void *drvPvt);

static const char *driverName="drvSIS38XX";
/***************/
/* Definitions */
/***************/

/*Constructor */
drvSIS38XX::drvSIS38XX(const char *portName, int maxChans, int maxSignals)
  :  asynPortDriver(portName, maxSignals, NUM_SIS38XX_PARAMS, 
                    asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynOctetMask | asynDrvUserMask,
                    asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynOctetMask,
                    ASYN_MULTIDEVICE, 1, 0, 0),
     exists_(false), maxSignals_(maxSignals), maxChans_(maxChans),
     acquiring_(false), prevAcquiring_(false)
{
  int i;
  static const char* functionName="SIS38XX";
  
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
  createParam(SCALER_RESET_COMMAND_STRING,          asynParamInt32, &scalerReset_);               /* int32, write */
  createParam(SCALER_CHANNELS_COMMAND_STRING,       asynParamInt32, &scalerChannels_);            /* int32, read */
  createParam(SCALER_READ_COMMAND_STRING,      asynParamInt32Array, &scalerRead_);                /* int32Array, read */
  createParam(SCALER_PRESET_COMMAND_STRING,         asynParamInt32, &scalerPresets_);             /* int32, write */
  createParam(SCALER_ARM_COMMAND_STRING,            asynParamInt32, &scalerArm_);                 /* int32, write */
  createParam(SCALER_DONE_COMMAND_STRING,           asynParamInt32, &scalerDone_);                /* int32, read */
  createParam(SIS38XXLEDString,                     asynParamInt32, &SIS38XXLED_);                /* int32, write */
  createParam(SIS38XXMuxOutString,                  asynParamInt32, &SIS38XXMuxOut_);             /* int32, write */
  createParam(SIS38XXCh1RefEnableString,            asynParamInt32, &SIS38XXCh1RefEnable_);       /* int32, write */
  createParam(SIS38XXAcquireModeString,             asynParamInt32, &SIS38XXAcquireMode_);        /* int32, write */
  createParam(SIS38XXInputModeString,               asynParamInt32, &SIS38XXInputMode_);          /* int32, write */
  createParam(SIS38XXOutputModeString,              asynParamInt32, &SIS38XXOutputMode_);         /* int32, write */
  createParam(SIS38XXModelString,                   asynParamOctet, &SIS38XXModel_);              /* octet, read */
  createParam(SIS38XXFirmwareString,                asynParamInt32, &SIS38XXFirmware_);           /* int32, read */

  // Default values of some parameters
  setIntegerParam(mcaNumChannels_, maxChans);
  setIntegerParam(mcaAcquiring_, 0);
  setIntegerParam(scalerDone_, 1);
  setIntegerParam(scalerChannels_, maxSignals);
  setIntegerParam(SIS38XXInputMode_, 3);
  setIntegerParam(SIS38XXOutputMode_, 0);
  for (i=0; i<maxSignals; i++) {
    setDoubleParam(i, mcaElapsedCounts_, 0.0);
    setDoubleParam(i, mcaElapsedRealTime_, 0.0);
    setDoubleParam(i, mcaElapsedLiveTime_, 0.0);
    callParamCallbacks(i);
  }

  /* Enable channel 1 reference pulses by default */
  setIntegerParam(SIS38XXCh1RefEnable_, 1);

  /* Allocate sufficient memory space to hold all of the data collected from the
   * SIS38XX.
   */
  mcsData_ = (epicsUInt32 *)malloc(maxSignals*maxChans*sizeof(epicsUInt32));
  if (mcsData_ == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: malloc failure for mcsData_\n", 
              driverName, functionName);
    return;
  }
  
  scalerData_ = (epicsUInt32 *)malloc(maxSignals*sizeof(epicsUInt32));
  if (scalerData_ == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: malloc failure for scalerData_\n", 
              driverName, functionName);
    return;
  }

  /* Initialise the pointers to the start of the buffer area */
  nextChan_ = 0;
  nextSignal_ = 0;
  
  /* Create the EPICS event used to wake up the readFIFOThread */
  readFIFOEventId_ = epicsEventCreate(epicsEventEmpty);
  
  dmaId_ = sysDmaCreate(dmaCallbackC, (void*)this);
  dmaDoneEventId_ = epicsEventCreate(epicsEventEmpty);
  
  // Create the mutex used to lock access to the FIFO
  fifoLockId_ = epicsMutexCreate();

  return;
}



asynStatus drvSIS38XX::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  int command=pasynUser->reason;
  int signal;
  int i;
  int nChans;
  asynStatus status=asynError;
  static const char* functionName = "writeInt32";

  if (!exists_) return asynError;
  
  disableInterrupts();
  pasynManager->getAddr(pasynUser, &signal);
  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: entry, command=%d, signal=%d, value=%d\n", 
            driverName, functionName, command, signal, value);
  
  // Set the value in the parameter library
  setIntegerParam(signal, command, value);
  
  getIntegerParam(mcaNumChannels_, &nChans);


  if (command == mcaStartAcquire_) {
    /* Start acquisition. */
    /* Nothing to do if we are already acquiring. */
    if (acquiring_) goto done;
    setAcquireMode(MCS_MODE);

    /* If the MCS is set to use internal channel advance, just start
     * collection. If it is set to use an external channel advance, arm
     * the scaler so that the next LNE pulse starts collection
     */

    acquiring_ = true;
    setIntegerParam(mcaAcquiring_, 1);
    prevAcquiring_ = true;
    erased_ = 0;
    // Set the acquisition start time
    epicsTimeGetCurrent(&startTime_);
    // Start the hardware
    startMCSAcquire();
    // Wake up the FIFO reading thread
    epicsEventSignal(readFIFOEventId_);
  }
    
  else if (command == mcaStopAcquire_) {
    /* stop data acquisition */
    acquiring_ = false;
    // Stop the hardware
    stopMCSAcquire();
  }

  else if (command == mcaErase_) {
    /* Erase.
     * Do the complete erase, but we keep
     * a flag that says we have been erased since acquire was
     * last started, so we only do it once.
     */
    /* If SIS38XX is acquiring, turn if off */
    if (acquiring_)
      stopMCSAcquire();

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s:%s: [%s signal=%d]: erased\n",
              driverName, functionName, portName, signal);

    /* Erase the buffer in the private data structure */
    erase();

    /* If device was acquiring, turn it back on */
    if (acquiring_) {
      startMCSAcquire();
      erased_ = 0;
    }
  }

  else if (command == mcaReadStatus_) {
    /* No-op?
     */
  }

  else if (command == mcaChannelAdvanceSource_) {
    setChannelAdvanceSource();
  }


  else if (command == mcaNumChannels_) {
    /* Terminology warning:
     * This is the number of channels that are to be acquired. Channels
     * correspond to time bins or external channel advance triggers, as
     * opposed to the 32 input signals that the SIS38XX supports.
     */
    if (value > maxChans_) {
      setIntegerParam(mcaNumChannels_, maxChans_);
      asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                "%s:%s:  # channels=%d too large, max=%d\n",
                driverName, functionName, nChans, maxChans_);
    }
  }

  else if (command == scalerReset_) {
    /* Reset scaler */
   resetScaler();
   acquiring_ = false;
    /* Clear all of the presets */
    for (i=0; i<maxSignals_; i++) {
      setIntegerParam(i, scalerPresets_, 0);
    }
  }

  else if (command == scalerArm_) {
    /* Arm or disarm scaler */
    setAcquireMode(SCALER_MODE);
    setScalerPresets();
    if (value != 0) {
      startScaler();
      acquiring_ = true;
      prevAcquiring_ = true;
    } else {
      stopScaler();
    }
    setIntegerParam(scalerDone_, 0);
  }

  else if (command == SIS38XXLED_) {
    setLED();
  }
    
  else if (command == SIS38XXMuxOut_) {
    setMuxOut();
  }
  
  else if (command == SIS38XXInputMode_) {
    setInputMode();
  }
  
  else if (command == SIS38XXOutputMode_) {
    setOutputMode();
  }

  status = asynSuccess;
  done:
  callParamCallbacks(signal);
  enableInterrupts();
  return status;
}



asynStatus drvSIS38XX::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
  int command = pasynUser->reason;
  asynStatus status=asynSuccess;
  int signal;
  static const char* functionName="readInt32";

  if (!exists_) return asynError;

  pasynManager->getAddr(pasynUser, &signal);
  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: entry, command=%d, signal=%d, &value=%p\n", 
            driverName, functionName, command, signal, value);

  disableInterrupts();
  if (command == scalerRead_) {
    readScalers();
    /* Read a single scaler channel */
    *value = scalerData_[signal];
  }
  else if (command == SIS38XXLED_) {
    *value = getLED();
  }
  else if (command == SIS38XXMuxOut_) {
    *value = getMuxOut();
  }
  else {
    status = asynPortDriver::readInt32(pasynUser, value);
  }
  enableInterrupts();
  return status;
}



asynStatus drvSIS38XX::readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                                      size_t numRead, size_t *numActual)
{
  int signal;
  int command = pasynUser->reason;
  asynStatus status = asynSuccess;
  size_t i;
  static const char* functionName="readInt32Array";

  if (!exists_) return asynError;

  disableInterrupts();
  pasynManager->getAddr(pasynUser, &signal);
  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: entry, command=%d, signal=%d, numRead=%d, &data=%p\n", 
            driverName, functionName, command, signal, numRead, data);

  if (command == mcaData_) {
    /* Transfer the data from the private driver structure to the supplied data
     * buffer. The private data structure will have the information for all the
     * signals, so we need to just extract the signal being requested.
     */
    int nChans;
    int numCopy;
    getIntegerParam(mcaNumChannels_, &nChans);
    numCopy = numRead;
    if (numCopy > nChans) numCopy = nChans;
    // We copy all the channels but we only report nchans
    // This ensures the entire array is correct even if it was not set to zero at the start
    memcpy(data, mcsData_ + signal*maxChans_, *numActual*sizeof(epicsInt32));
    *numActual = numRead;
    if ((int)*numActual > nextChan_) *numActual = nextChan_;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: [signal=%d]: read %d chans (numRead=%d, numCopy=%d, nextChan=%d, nChans=%d)\n",  
              driverName, functionName, signal, *numActual, numRead, numCopy, nextChan_, nChans);
    }
  else if (command == scalerRead_) {
    readScalers();
    for (i=0; (i<numRead && i<(size_t)maxSignals_); i++) {
      data[i] = scalerData_[i];
    }
    for (i=maxSignals_; i<numRead; i++) {
      data[i] = 0;
    }
    *numActual = numRead;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: scalerReadCommand: read %d chans, channel[0]=%d\n", 
              driverName, functionName, numRead, data[0]);
  }
  else {
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "%s:%s: got illegal command %d\n",
              driverName, functionName, command);
    status = asynError;
  }
  enableInterrupts();
  return status;
}

/* Report  parameters */
void drvSIS38XX::report(FILE *fp, int details)
{
  if (details > 0) {
    fprintf(fp, "  moduleID         = %d\n",   moduleID_);
    fprintf(fp, "  acquire mode     = %d\n",   acquireMode_);
    fprintf(fp, "  max signals      = %d\n",   maxSignals_);
    fprintf(fp, "  max channels     = %d\n",   maxChans_);
    fprintf(fp, "  next channel     = %d\n",   nextChan_);
    fprintf(fp, "  next signal      = %d\n",   nextSignal_);
    fprintf(fp, "  elapsed previous = %f\n",   elapsedPrevious_);
    fprintf(fp, "  erased           = %d\n",   erased_);
    fprintf(fp, "  LNE source       = %d\n",   lneSource_);
    fprintf(fp, "  LNE prescale     = %d\n",   lnePrescale_);
    fprintf(fp, "  soft advance     = %d\n",   softAdvance_);
    fprintf(fp, "  acquiring        = %d\n",   acquiring_);
    fprintf(fp, "  prev acquiring   = %d\n",   prevAcquiring_);
  }
  // Call the base class method
  asynPortDriver::report(fp, details);
}

void drvSIS38XX::erase()
{
  int i;
  epicsTimeStamp begin, end;
  static const char *functionName="erase";
  int nChans;

  /* If we are already erased return */
  if (erased_) {
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s:%s: already erased, skipping\n",
              driverName, functionName);
    return;
  }
  erased_ = 1;

  getIntegerParam(mcaNumChannels_, &nChans);
  epicsTimeGetCurrent(&begin);
  /* Erase buffer in driver */
  memset(mcsData_, 0, maxSignals_ * nChans * sizeof(epicsUInt32));
  epicsTimeGetCurrent(&end);
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: cleared local buffer (%d) in %fs\n",
            driverName, functionName, maxSignals_*nChans, epicsTimeDiffInSeconds(&end, &begin));

  /* Reset pointers to start of buffer */
  nextChan_ = 0;
  nextSignal_ = 0;

  /* Reset elapsed times */
  elapsedTime_ = 0.;
  elapsedPrevious_ = 0.;

  /* Reset the elapsed counts */
  for (i=0; i<maxSignals_; i++) {
    setDoubleParam(i, mcaElapsedLiveTime_, 0.0);
    setDoubleParam(i, mcaElapsedRealTime_, 0.0);
    setDoubleParam(i, mcaElapsedCounts_, 0.0);
    callParamCallbacks(i);
  }

  /* Reset the start time.  This is necessary here because we may be
   * acquiring, and AcqOn will not be called. Normally this is set in AcqOn.
   */
  epicsTimeGetCurrent(&startTime_);

  return;
}


bool drvSIS38XX::checkDone(int fifoCount)
{
  int signal;
  int i;
  epicsTimeStamp now;
  int nChans;
  bool mcaDone;
  epicsUInt32 *pData;
  double presetLive, presetReal, presetCounts, elapsedCounts;
  int presetStartChan, presetEndChan;
  static const char* functionName="checkDone";

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: enter: acquiring=%d, prevAcquiring=%d\n",
            driverName, functionName, acquiring_, prevAcquiring_);

  getIntegerParam(mcaNumChannels_, &nChans);
  getDoubleParam(mcaPresetLiveTime_, &presetLive);
  getDoubleParam(mcaPresetRealTime_, &presetReal);
  getDoubleParam(mcaPresetCounts_, &presetCounts);

  epicsTimeGetCurrent(&now);
  if (acquiring_) {
    elapsedTime_ = epicsTimeDiffInSeconds(&now, &startTime_);
    if ((presetReal > 0) && (elapsedTime_ >= presetReal)) {
      acquiring_ = false;
      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s:, stopped acquisition by preset real time\n",
                driverName, functionName);
    }
  }

  if (acquiring_) {
    if ((presetLive > 0) && (elapsedTime_ >= presetLive)) {
      acquiring_ = false;
      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s:, stopped acquisition by preset live time\n",
                driverName, functionName);
    }
  }

  /* Check that acquisition is complete by nextChan and nextSignal.  This ensures
   * that it will be detected even if interrupts are disabled.
   */
  if (acquiring_) {
    if (nextChan_ >= maxChans_) {
      acquiring_ = false;
      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s:, stopped acquisition by nextChan = %d\n",
                driverName, functionName, nextChan_);
    }
  }

  /* Check that acquisition is complete by preset counts */
  if (acquiring_) {
    for (signal=0; signal<maxSignals_; signal++) {
      getDoubleParam(signal, mcaPresetCounts_, &presetCounts);
      getIntegerParam(signal, mcaPresetLowChannel_, &presetStartChan);
      getIntegerParam(signal, mcaPresetHighChannel_, &presetEndChan);
      if (presetCounts > 0) {
        elapsedCounts = 0;
        pData = mcsData_ + signal*maxChans_;
        for (i=presetStartChan; i<=presetEndChan; i++) {
          elapsedCounts += pData[i];
        }
        if (elapsedCounts >= presetCounts) {
          acquiring_ = false;
          asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                    "%s:%s:, stopped acquisition by preset counts\n",
                    driverName, functionName);
        }
      }
    }
  }

  /* If acquisition just stopped then ... */
  if ((prevAcquiring_ == true) && (acquiring_ == false)) {
    epicsTimeGetCurrent(&now);
    elapsedTime_ = epicsTimeDiffInSeconds(&now, &startTime_);
    /* Turn off hardware acquisition */
    stopMCSAcquire();
  }

  // Set elapsed times
  for (signal=0; signal<maxSignals_; signal++) {
    setDoubleParam(signal, mcaElapsedRealTime_, elapsedTime_);
    setDoubleParam(signal, mcaElapsedLiveTime_, elapsedTime_);
 }

  // We only set mcaAcquiring to 0 if acquiring_=false and count=0, which tells the MCA
  // records to process
  mcaDone = ((acquiring_ == false) && 
             (fifoCount == 0));
  if (mcaDone) {
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
               "%s:%s:, acquisition complete, doing callbacks on mcaAcquiring\n",
               driverName, functionName);
    for (signal=0; signal<maxSignals_; signal++) {
      setIntegerParam(signal, mcaAcquiring_, 0);
    }
  }

  // Do callbacks on all channels
  for (signal=0; signal<maxSignals_; signal++) {
    callParamCallbacks(signal);
  }

  /* Save the acquisition status */
  prevAcquiring_ = acquiring_;

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: exit: acquiring=%d, prevAcquiring=%d\n",
            driverName, functionName, acquiring_, prevAcquiring_);
  return acquiring_ || prevAcquiring_;
}

/**********************/
/* DMA handling       */
/**********************/

static void dmaCallbackC(void *drvPvt)
{
  drvSIS38XX *pSIS38XX = (drvSIS38XX*)drvPvt;
  pSIS38XX->dmaCallback();
}

void drvSIS38XX::dmaCallback()
{
  epicsEventSignal(dmaDoneEventId_);
}

