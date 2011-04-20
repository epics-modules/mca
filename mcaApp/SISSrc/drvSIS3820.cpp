/* File  SIS3820.cpp
 * Author:  Mark Rivers
 * Date:    09-Apr-2011
 * 
 * Based on file  drvMcaSIS3820Asyn.c
 * Author:  Wayne Lewis
 * Date:    04-sep-2006
 *
 * Purpose: 
 * This module provides the driver support for the MCA and scaler record asyn device 
 * support layer for the SIS3820 multichannel scaler.
 *
 *
 * Revisions:
 * 0.1  Wayne Lewis               Original version
 *
 * Copyright (c) 2006, Australian Synchrotron
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * Neither the name of the Australian Synchrotron nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLOSED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*********/
/* To do */
/*********/

/*
 * - Track down problem with FIFO threshold interrupts.  
 *     Works fine with 1024 word threshold down to 100 microsecond dwell time,
 *     below that it messes up, 100% CPU time.
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
#include "drvSIS3820.h"
#include "sis3820.h"

static const char *driverName="drvSIS3820";
static void intFuncC(void *drvPvt);
static void readFIFOThreadC(void *drvPvt);
static void dmaCallbackC(void *drvPvt);


/***************/
/* Definitions */
/***************/

/*Constructor */
drvSIS3820::drvSIS3820(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                       int maxChans, int maxSignals, int inputMode, int outputMode, 
                       bool useDma, int fifoBufferWords)
  :  asynPortDriver(portName, maxSignals, NUM_SIS3820_PARAMS, 
                    asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask | asynDrvUserMask,
                    asynInt32Mask | asynFloat64Mask | asynInt32ArrayMask,
//                    ASYN_MULTIDEVICE | ASYN_CANBLOCK, 1, 0, 0),
                    ASYN_MULTIDEVICE, 1, 0, 0),
     exists_(false), maxSignals_(maxSignals), maxChans_(maxChans),
     inputMode_(inputMode), outputMode_(outputMode),
     acquiring_(false), prevAcquiring_(false),
     useDma_(useDma)
{
  int status;
  epicsUInt32 controlStatusReg;
  int i;
  static const char* functionName="SIS3820";
  
  // Uncomment this line to enable asynTraceFlow during the constructor
  //pasynTrace->setTraceMask(pasynUserSelf, 0x11);
  
  createParam(mcaStartAcquireString,                asynParamInt32, &mcaStartAcquire_);
  createParam(mcaStopAcquireString,                 asynParamInt32, &mcaStopAcquire_);            /* int32, write */
  createParam(mcaEraseString,                       asynParamInt32, &mcaErase_);                  /* int32, write */
  createParam(mcaDataString,                        asynParamInt32, &mcaData_);                   /* int32Array, read/write */
  createParam(mcaReadStatusString,                  asynParamInt32, &mcaReadStatus_);             /* int32, write */
  createParam(mcaChannelAdvanceInternalString,      asynParamInt32, &mcaChannelAdvanceInternal_); /* int32, write */
  createParam(mcaChannelAdvanceExternalString,      asynParamInt32, &mcaChannelAdvanceExternal_); /* int32, write */
  createParam(mcaNumChannelsString,                 asynParamInt32, &mcaNumChannels_);            /* int32, write */
  createParam(mcaDwellTimeString,                 asynParamFloat64, &mcaDwellTime_);              /* float64, write */
  createParam(mcaPresetLiveTimeString,            asynParamFloat64, &mcaPresetLiveTime_);         /* float64, write */
  createParam(mcaPresetRealTimeString,            asynParamFloat64, &mcaPresetRealTime_);         /* float64, write */
  createParam(mcaPresetCountsString,              asynParamFloat64, &mcaPresetCounts_);           /* float64, write */
  createParam(mcaPresetLowChannelString,            asynParamInt32, &mcaPresetLowChannel_);       /* int32, write */
  createParam(mcaPresetHighChannelString,           asynParamInt32, &mcaPresetHighChannel_);      /* int32, write */
  createParam(mcaPresetSweepsString,                asynParamInt32, &mcaPresetSweeps_);           /* int32, write */
  createParam(mcaModePHAString,                     asynParamInt32, &mcaModePHA_);                /* int32, write */
  createParam(mcaModeMCSString,                     asynParamInt32, &mcaModeMCS_);                /* int32, write */
  createParam(mcaModeListString,                    asynParamInt32, &mcaModeList_);               /* int32, write */
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
  createParam(SIS3820LEDString,                     asynParamInt32, &SIS3820LED_);
  createParam(SIS3820MuxOutString,                  asynParamInt32, &SIS3820MuxOut_);
  createParam(SIS3820Ch1RefEnableString,            asynParamInt32, &SIS3820Ch1RefEnable_);
  createParam(SIS3820ModeString,                    asynParamInt32, &SIS3820Mode_);

  // Default values of some parameters
  setIntegerParam(mcaNumChannels_, maxChans);
  setIntegerParam(mcaAcquiring_, 0);
  setIntegerParam(scalerDone_, 1);
  setIntegerParam(scalerChannels_, maxSignals);
  for (i=0; i<maxSignals; i++) {
    setDoubleParam(i, mcaElapsedCounts_, 0.0);
    setDoubleParam(i, mcaElapsedRealTime_, 0.0);
    setDoubleParam(i, mcaElapsedLiveTime_, 0.0);
    callParamCallbacks(i);
  }

  /* Call devLib to get the system address that corresponds to the VME
   * base address of the board.
   */
  status = devRegisterAddress("drvSIS3820",
                               SIS3820_ADDRESS_TYPE,
                               (size_t)baseAddress,
                               SIS3820_BOARD_SIZE,
                               (volatile void **)&registers_);

  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: %s, Can't register VME address %p\n", 
              driverName, functionName, portName, baseAddress);
    return;
  }
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: Registered VME address: %p to local address: %p size: 0x%X\n", 
            driverName, functionName, baseAddress, registers_, SIS3820_BOARD_SIZE);

  /* Call devLib to get the system address that corresponds to the VME
   * FIFO address of the board.
   */
  status = devRegisterAddress("drvSIS3820",
                              SIS3820_ADDRESS_TYPE,
                              (size_t)(baseAddress + SIS3820_FIFO_BASE),
                              SIS3820_FIFO_BYTE_SIZE,
                              (volatile void **)&fifo_base_);

  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: %s, Can't register FIFO address %p\n", 
              driverName, functionName, portName, baseAddress + SIS3820_FIFO_BASE);
    return;
  }

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: Registered VME FIFO address: %p to local address: %p size: 0x%X\n", 
            driverName, functionName, baseAddress + SIS3820_FIFO_BASE, 
            fifo_base_, SIS3820_FIFO_BYTE_SIZE);

  /* Probe VME bus to see if card is there */
  status = devReadProbe(4, (char *) registers_->control_status_reg,
                       (char *) &controlStatusReg);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: devReadProbe failure = %d\n", 
              driverName, functionName, status);
    return;
  }

  /* Get the module info from the card */
  moduleID_ = (registers_->moduleID_reg & 0xFFFF0000) >> 16;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: module ID=%x\n", 
            driverName, functionName, moduleID_);
  firmwareVersion_ = (registers_->moduleID_reg & 0x0000FFFF);
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: firmwareVersion=%d\n",
            driverName, functionName, firmwareVersion_);

  /* Assume initially that the LNE is internal */
  lneSource_ = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
  softAdvance_ = 1;

  // Set a default value of lnePrescale for 1 second
  lnePrescale_ = 10000000;

  /* Enable channel 1 reference pulses by default */
  setIntegerParam(SIS3820Ch1RefEnable_, 1);

  /* Allocate sufficient memory space to hold all of the data collected from the
   * SIS3820.
   */
  mcsBuffer_ = (epicsUInt32 *)malloc(maxSignals*maxChans*sizeof(epicsUInt32));
  if (mcsBuffer_ == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: malloc failure for mcsBuffer_\n", 
              driverName, functionName);
    return;
  }

  /* Initialise the pointers to the start of the buffer area */
  nextChan_ = 0;
  nextSignal_ = 0;
  
  // Allocate FIFO readout buffer
  // fifoBufferWords input argument is in words, must be less than SIS3820_FIFO_WORD_SIZE
  if (fifoBufferWords == 0) fifoBufferWords = SIS3820_FIFO_WORD_SIZE;
  if (fifoBufferWords > SIS3820_FIFO_WORD_SIZE) fifoBufferWords = SIS3820_FIFO_WORD_SIZE;
  fifoBufferWords_ = fifoBufferWords;
  fifoBuffer_ = (epicsUInt32*) memalign(8, fifoBufferWords_*sizeof(epicsUInt32));
  if (fifoBuffer_ == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: posix_memalign failure for fifoBuffer_ = %d\n", 
              driverName, functionName, status);
    return;
  }
  
  /* Create the EPICS event used to wake up the readFIFOThread */
  readFIFOEventId_ = epicsEventCreate(epicsEventEmpty);
  
  dmaId_ = sysDmaCreate(dmaCallbackC, (void*)this);
  dmaDoneEventId_ = epicsEventCreate(epicsEventEmpty);
  
  // Create the mutex used to lock access to the FIFO
  fifoLockId_ = epicsMutexCreate();

  /* Reset card */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: resetting port %s\n", 
            driverName, functionName, portName);
  registers_->key_reset_reg = 1;

  /* Clear FIFO */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
           "%s:%s: clearing FIFO\n",
           driverName, functionName);
  resetFIFO();

  /* Initialize board in MCS mode */
  setAcquireMode(MCS_MODE);

  /* Set up the interrupt service routine */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: interruptServiceRoutine pointer %p\n",
            driverName, functionName, intFuncC);

  status = devConnectInterruptVME(interruptVector, intFuncC, this);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: Can't connect to vector % d\n", 
              driverName, functionName, interruptVector);
    return;
  }
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: Connected interrupt vector: %d\n\n",
            driverName, functionName, interruptVector);
  
  /* Write interrupt level to hardware */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq before setting IntLevel= 0x%x\n", 
            driverName, functionName, registers_->irq_config_reg);

  registers_->irq_config_reg &= ~SIS3820_IRQ_LEVEL_MASK;
  registers_->irq_config_reg |= (interruptLevel << 8);

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: IntLevel mask= 0x%x\n", 
            driverName, functionName, (interruptLevel << 8));
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq after setting IntLevel= 0x%x\n", 
             driverName, functionName, registers_->irq_config_reg);

  /* Write interrupt vector to hardware */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq before setting IntLevel= 0x%x\n", 
            driverName, functionName, registers_->irq_config_reg);

  registers_->irq_config_reg &= ~SIS3820_IRQ_VECTOR_MASK;
  registers_->irq_config_reg |= interruptVector;

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq = 0x%08x\n", 
            driverName, functionName, registers_->irq_config_reg);

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "m%s:%s: irq config register after enabling interrupts= 0x%08x\n", 
            driverName, functionName, registers_->irq_config_reg);

  /* Create the thread that reads the FIFO */
  if (epicsThreadCreate("SIS3820FIFOThread",
                         epicsThreadPriorityLow,
                         epicsThreadGetStackSize(epicsThreadStackMedium),
                         (EPICSTHREADFUNC)readFIFOThreadC,
                         this) == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: epicsThreadCreate failure\n", 
              driverName, functionName);
    return;
  }
  else
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: epicsThreadCreate success\n", 
            driverName, functionName, registers_->irq_config_reg);

  erase();

  /* Enable interrupts in hardware */  
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq before enabling interrupts= 0x%08x\n", 
            driverName, functionName, registers_->irq_config_reg);

  registers_->irq_config_reg |= SIS3820_IRQ_ENABLE;

  /* Enable interrupt level in EPICS */
  status = devEnableInterruptLevel(intVME, interruptLevel);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: Can't enable enterrupt level %d\n", 
              driverName, functionName, interruptLevel);
    return;
  }
  
  exists_ = true;
  return;
}



asynStatus drvSIS3820::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
  int command=pasynUser->reason;
  int signal;
  int i;
  int nChans;
  asynStatus status=asynError;
  static const char* functionName = "writeInt32";

  disableInterrupts();
  pasynManager->getAddr(pasynUser, &signal);
  
  // Set the value in the parameter library
  setIntegerParam(signal, command, value);
  
  getIntegerParam(mcaNumChannels_, &nChans);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: entry, command=%d, signal=%d, value=%d\n", 
            driverName, functionName, command, signal, value);

  if (command == mcaStartAcquire_) {
    /* Start acquisition. */
    /* Nothing to do if we are already acquiring. */
    if (acquiring_) goto done;

    /* If the MCS is set to use internal channel advance, just start
     * collection. If it is set to use an external channel advance, arm
     * the scaler so that the next LNE pulse starts collection
     */

    acquiring_ = true;
    setIntegerParam(mcaAcquiring_, 1);
    prevAcquiring_ = true;
    erased_ = 0;

    setAcquireMode(MCS_MODE);
    setOpModeReg();

    /* Set the number of channels to acquire */
    registers_->acq_preset_reg = nChans;

    /* Set the acquisition start time */
    epicsTimeGetCurrent(&startTime_);

    if (lneSource_ == SIS3820_LNE_SOURCE_INTERNAL_10MHZ) {
      /* The SIS3820 requires the value in the LNE prescale register to be one
       * less than the actual number of incoming signals. We do this adjustment
       * here, so the user sees the actual number at the record level.
       */
      double dwellTime;
      getDoubleParam(mcaDwellTime_, &dwellTime);
      registers_->lne_prescale_factor_reg = 
        (epicsUInt32) (SIS3820_10MHZ_CLOCK * dwellTime) - 1;
      registers_->key_op_enable_reg = 1;

    }
    else if (lneSource_ == SIS3820_LNE_SOURCE_CONTROL_SIGNAL) {
      /* The SIS3820 requires the value in the LNE prescale register to be one
       * less than the actual number of incoming signals. We do this adjustment
       * here, so the user sees the actual number at the record level.
       */
      registers_->lne_prescale_factor_reg = lnePrescale_ - 1;
      registers_->key_op_arm_reg = 1;
    } 
    else {
      asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                "%s:%s: unsupported lneSource %d\n", 
                driverName, functionName, lneSource_);
    }
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: lneSource = %d\n",
              driverName, functionName, lneSource_);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: op mode register = 0x%08x\n", 
              driverName, functionName, registers_->op_mode_reg);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: control status register = 0x%08x\n",
              driverName, functionName, registers_->control_status_reg);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: lne prescale = %d\n", 
              driverName, functionName, registers_->lne_prescale_factor_reg);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: acq_preset_req = %d\n", 
              driverName, functionName, registers_->acq_preset_reg);
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: fifo_word_threshold_reg = %d\n", 
              driverName, functionName, registers_->fifo_word_threshold_reg);

    epicsEventSignal(readFIFOEventId_);
  }
    
  else if (command == mcaStopAcquire_) {
    /* stop data acquisition */
    registers_->key_op_disable_reg = 1;
    acquiring_ = false;
  }

  else if (command == mcaErase_) {
    /* Erase.
     * For the SIS3820, just do the complete erase, but we keep
     * a flag that says we have been erased since acquire was
     * last started, so we only do it once.
     */
    /* If SIS3820 is acquiring, turn if off */
    if (acquiring_)
      registers_->key_op_disable_reg = 1;

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "%s:%s: [%s signal=%d]: erased\n",
              driverName, functionName, portName, signal);

    /* Erase the buffer in the private data structure */
    erase();

    /* If SIS3820 was acquiring, turn it back on */
    /* If the MCS is set to use internal channel advance, just start
     * collection. If it is set to use an external channel advance, arm
     * the scaler so that the next LNE pulse starts collection
     */
    if (acquiring_) {
      if (lneSource_ == SIS3820_LNE_SOURCE_INTERNAL_10MHZ)
        registers_->key_op_enable_reg = 1;
      else
        registers_->key_op_arm_reg = 1;
      erased_ = 0;
    }
  }

  else if (command == mcaReadStatus_) {
    /* No-op?
     */
  }

  else if (command == mcaChannelAdvanceInternal_) {
    /* set channel advance source to internal (timed) */
    /* Just cache this setting here, set it when acquisition starts */
    lneSource_ = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: (mcaChannelAdvanceInternal) pPvt-lneSource = 0x%x\n", 
              driverName, functionName, lneSource_);
  }

  else if (command == mcaChannelAdvanceExternal_) {
    /* set channel advance source to external */
    /* Just cache this setting here, set it when acquisition starts */
    lneSource_ = SIS3820_LNE_SOURCE_CONTROL_SIGNAL;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: (mcaChannelAdvanceExternal) pPvt-lneSource = 0x%x\n", 
              driverName, functionName, lneSource_);
  }

  else if (command == mcaNumChannels_) {
    /* Terminology warning:
     * This is the number of channels that are to be acquired. Channels
     * correspond to time bins or external channel advance triggers, as
     * opposed to the 32 input signals that the SIS3820 supports.
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
    registers_->key_op_disable_reg = 1;
    resetFIFO();
    registers_->key_counter_clear = 1;
    acquiring_ = false;
    /* Clear all of the presets */
    for (i=0; i<maxSignals_; i++) {
      setIntegerParam(i, scalerPresets_, 0);
    }
  }

  else if (command == scalerArm_) {
    /* Arm or disarm scaler */
    setAcquireMode(SCALER_MODE);
    if (value != 0) {
      setScalerPresets();
      registers_->key_op_enable_reg = 1;
      acquiring_ = true;
      prevAcquiring_ = true;
    } else {
      registers_->key_op_disable_reg = 1;
      resetFIFO();
    }
    setIntegerParam(scalerDone_, 0);
  }

  else if (command == SIS3820LED_) {
    if (value > 0)
      registers_->control_status_reg = CTRL_USER_LED_ON;
    else
      registers_->control_status_reg = CTRL_USER_LED_OFF;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: scalerLedCommand channel %d=%d\n", 
              driverName, functionName, signal, value);
  }
    
  else if (command == SIS3820MuxOut_) {
    if (outputMode_ != 3) {
      asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s: scalerMuxOutCommand: output-mode %d does not support MUX_OUT.\n", 
                driverName, functionName, outputMode_);
      goto done;
    }
    if (value < 1 || value > 32) {
      asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s: scalerMuxOutCommand: channel %d is not supported on the board.\n", 
                driverName, functionName, value);
      goto done;
    }
    if (firmwareVersion_ < 0x010A) {
      asynPrint(pasynUser, ASYN_TRACE_ERROR,
                "%s:%s: scalerMuxOutCommand: MUX_OUT is not supported in firmware version 0x%4.4X\n", 
                driverName, functionName, firmwareVersion_);
      goto done;
    }
    registers_->mux_out_channel_select_reg = value - 1;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: scalerMuxOutCommand channel %d=%d\n", 
              driverName, functionName, signal, value);
  }

  status = asynSuccess;
  done:
  callParamCallbacks(signal);
  enableInterrupts();
  return status;
}



asynStatus drvSIS3820::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
  int command = pasynUser->reason;
  asynStatus status=asynSuccess;
  int signal;

  pasynManager->getAddr(pasynUser, &signal);

  disableInterrupts();
  if (command == scalerRead_) {
    /* Read a single scaler channel */
    *value = registers_->counter_regs[signal];
  }
  else if (command == SIS3820LED_) {
    *value = registers_->control_status_reg & 0x1;
  }
  else if (command == SIS3820MuxOut_) {
    *value = registers_->mux_out_channel_select_reg + 1;
  }
  else {
    status = asynPortDriver::readInt32(pasynUser, value);
  }
  enableInterrupts();
  return status;
}



asynStatus drvSIS3820::readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                                      size_t numRead, size_t *numActual)
{
  int signal;
  int command = pasynUser->reason;
  asynStatus status = asynSuccess;
  size_t i;
  static const char* functionName="readInt32Array";

  disableInterrupts();
  pasynManager->getAddr(pasynUser, &signal);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: entry, signal=%d, maxChans=%d\n", 
            driverName, functionName, signal, numRead);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: fifo word count = %d\n", 
            driverName, functionName, registers_->fifo_word_count_reg);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "%s:%s: status register = 0x%08x\n", 
            driverName, functionName, registers_->control_status_reg);

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
    memcpy(data, mcsBuffer_ + signal*maxChans_, *numActual*sizeof(epicsInt32));
    *numActual = numRead;
    if ((int)*numActual > nextChan_) *numActual = nextChan_;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "%s:%s: [signal=%d]: read %d chans (numRead=%d, numCopy=%d, nextChan=%d, nChans=%d)\n",  
              driverName, functionName, signal, *numActual, numRead, numCopy, nextChan_, nChans);
    }
  else if (command == scalerRead_) {
    for (i=0; (i<numRead && i<(size_t)maxSignals_); i++) {
      data[i] = registers_->counter_regs[i];
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
void drvSIS3820::report(FILE *fp, int details)
{

  fprintf(fp, "SIS3820: asyn port: %s, connected at VME base address %p, maxChans=%d\n",
          portName, registers_, maxChans_);
  if (details > 0) {
    fprintf(fp, "  moduleID         = %d\n",   moduleID_);
    fprintf(fp, "  firmware version = 0x%x\n", firmwareVersion_);
    fprintf(fp, "  acquire mode     = %d\n",   acquireMode_);
    fprintf(fp, "  max signals      = %d\n",   maxSignals_);
    fprintf(fp, "  max channels     = %d\n",   maxChans_);
    fprintf(fp, "  input mode       = 0x%x\n", inputMode_);
    fprintf(fp, "  output mode      = 0x%x\n", outputMode_);
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
  if (details > 1) {
    fprintf(fp, "  Registers:\n");
    fprintf(fp, "    control_status_reg         = 0x%x\n",   registers_->control_status_reg);
    fprintf(fp, "    moduleID_reg               = 0x%x\n",   registers_->moduleID_reg);
    fprintf(fp, "    irq_config_reg             = 0x%x\n",   registers_->irq_config_reg);
    fprintf(fp, "    irq_control_status_reg     = 0x%x\n",   registers_->irq_control_status_reg);
    fprintf(fp, "    acq_preset_reg             = 0x%x\n",   registers_->acq_preset_reg);
    fprintf(fp, "    acq_count_reg              = 0x%x\n",   registers_->acq_count_reg);
    fprintf(fp, "    lne_prescale_factor_reg    = 0x%x\n",   registers_->lne_prescale_factor_reg);
    fprintf(fp, "    preset_group1_reg          = 0x%x\n",   registers_->preset_group1_reg);
    fprintf(fp, "    preset_group2_reg          = 0x%x\n",   registers_->preset_group2_reg);
    fprintf(fp, "    preset_enable_reg          = 0x%x\n",   registers_->preset_enable_reg);
    fprintf(fp, "    cblt_setup_reg             = 0x%x\n",   registers_->cblt_setup_reg);
    fprintf(fp, "    sdram_page_reg             = 0x%x\n",   registers_->sdram_page_reg);
    fprintf(fp, "    fifo_word_count_reg        = 0x%x\n",   registers_->fifo_word_count_reg);
    fprintf(fp, "    fifo_word_threshold_reg    = 0x%x\n",   registers_->fifo_word_threshold_reg);
    fprintf(fp, "    hiscal_start_preset_reg    = 0x%x\n",   registers_->hiscal_start_preset_reg);
    fprintf(fp, "    hiscal_start_counter_reg   = 0x%x\n",   registers_->hiscal_start_counter_reg);
    fprintf(fp, "    hiscal_last_acq_count_reg  = 0x%x\n",   registers_->hiscal_last_acq_count_reg);
    fprintf(fp, "    op_mode_reg                = 0x%x\n",   registers_->op_mode_reg);
  }
      // Call the base class method
  asynPortDriver::report(fp, details);
}

void drvSIS3820::erase()
{
  int i;
  epicsTimeStamp begin, end;
  static const char *functionName="erase";
  int nChans;

  if (!exists_) return;
  
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
  memset(mcsBuffer_, 0, maxSignals_ * nChans * sizeof(epicsUInt32));
  epicsTimeGetCurrent(&end);
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: cleared local buffer (%d) in %fs\n",
            driverName, functionName, maxSignals_*nChans, epicsTimeDiffInSeconds(&end, &begin));

  /* Reset pointers to start of buffer */
  nextChan_ = 0;
  nextSignal_ = 0;

  /* Erase FIFO and counters on board */
  resetFIFO();
  registers_->key_counter_clear = 1;

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


bool drvSIS3820::checkDone()
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
        pData = mcsBuffer_ + signal*maxChans_;
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
    registers_->key_op_disable_reg = 1;
  }

  // Set elapsed times
  for (signal=0; signal<maxSignals_; signal++) {
    setDoubleParam(signal, mcaElapsedRealTime_, elapsedTime_);
    setDoubleParam(signal, mcaElapsedLiveTime_, elapsedTime_);
 }

  // Do callbacks on acquiring status when acquisition completes
  if ((acquiring_ == false) && (acquireMode_ == SCALER_MODE)) {
    setIntegerParam(scalerDone_, 1);
  }
  // We only set mcaAcquiring to 0 if acquiring_=false and count=0, which tells the MCA
  // records to process
  mcaDone = ((acquiring_ == false) && 
             (acquireMode_ == MCS_MODE) && 
             (registers_->fifo_word_count_reg==0));
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


void drvSIS3820::setAcquireMode(SIS3820AcquireMode acquireMode)
{
  int i;
  int ch1RefEnable;
  static const char* functionName="setAcquireMode";
  
  getIntegerParam(SIS3820Ch1RefEnable_, &ch1RefEnable);
  
  epicsUInt32 channelDisableRegister = SIS3820_CHANNEL_DISABLE_MASK;
  
  /* Enable or disable 50 MHz channel 1 reference pulses. */
  if (ch1RefEnable > 0)
    registers_->control_status_reg |= CTRL_REFERENCE_CH1_ENABLE;
  else
    registers_->control_status_reg |= CTRL_REFERENCE_CH1_DISABLE;

  if (acquireMode_ == acquireMode) return;  /* Nothing to do */
  acquireMode_ = acquireMode;

  
  /* Initialize board and set the control status register */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: initialising control status register\n",
            driverName, functionName);
  setControlStatusReg();
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: control status register = 0x%08x\n",
            driverName, functionName, registers_->control_status_reg);

  /* Set the interrupt control register */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: setting interrupt control register\n",
            driverName, functionName);
  setIrqControlStatusReg();
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: interrupt control register = 0x%08x\n",
            driverName, functionName, registers_->irq_control_status_reg);

  /* Set the operation mode of the scaler */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: setting operation mode\n",
            driverName, functionName);
  setOpModeReg();
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: operation mode register = 0x%08x\n",
            driverName, functionName, registers_->op_mode_reg);

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: control status register = 0x%08x\n",
            driverName, functionName, registers_->control_status_reg);

  /* Trigger an interrupt when 1024 FIFO registers have been filled */
  registers_->fifo_word_threshold_reg = 1024;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: FIFO threshold = %d\n",
            driverName, functionName, registers_->fifo_word_threshold_reg);

  /* Set the LNE channel */
  if (acquireMode_ == MCS_MODE) {
    registers_->lne_channel_select_reg = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
  } else {
    registers_->lne_channel_select_reg = SIS3820_LNE_CHANNEL;
  }
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: set LNE signal generation to channel %d\n",
            driverName, functionName, registers_->lne_channel_select_reg);

  /*
   * Set number of readout channels to maxSignals
   * Assumes that the lower channels will be used first, and the only unused
   * channels will be at the upper end of the channel range.
   * Create a mask with zeros in the rightmost maxSignals bits,
   * 1 in all higher order bits.
   */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: setting readout channels=%d\n",
            driverName, functionName, maxSignals_);
  for (i = 0; i < maxSignals_; i++)
  {
    channelDisableRegister <<= 1;
  }
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: setting readout disable register=0x%08x\n",
            driverName, functionName, channelDisableRegister);

  /* Disable channel in MCS mode. */
  registers_->copy_disable_reg = channelDisableRegister;
  /* Disable channel in scaler mode. */
  registers_->count_disable_reg = channelDisableRegister;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: setting copy disable register=0x%08x\n",
            driverName, functionName, registers_->copy_disable_reg);

  /* Set the prescale factor to the desired value. */
  registers_->lne_prescale_factor_reg = lnePrescale_ - 1;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: lne prescale register=%d\n",
            driverName, functionName, registers_->lne_prescale_factor_reg);



  if (acquireMode_ == MCS_MODE) {
    /* Clear all presets from scaler mode */
    clearScalerPresets();
  } else {
    /* Clear the preset register */
    registers_->acq_preset_reg = 0;
  }
}


void drvSIS3820::clearScalerPresets()
{
  registers_->preset_channel_select_reg &= ~SIS3820_FOUR_BIT_MASK;
  registers_->preset_channel_select_reg &= ~(SIS3820_FOUR_BIT_MASK << 16);
  registers_->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP1;
  registers_->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP2;
  registers_->preset_group2_reg = 0;
  registers_->preset_group2_reg = 0;
}

void drvSIS3820::setScalerPresets()
{
  int i;
  epicsUInt32 presetChannelSelectRegister;
  int preset;

  if (acquireMode_ == SCALER_MODE) {
    /* Disable all presets to start */
    clearScalerPresets();

    /* The logic implemented in this code is that the last channel with a non-zero
     * preset value in a given channel group (1-16, 17-32) will be the one that is
     * in effect
     */
    for (i=0; i<maxSignals_; i++) {
      getIntegerParam(i, scalerPresets_, &preset);
      if (preset != 0) {
        if (i < 16) {
          registers_->preset_group1_reg = preset;
          /* Enable this bank of counters for preset checking */
          registers_->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP1;
          /* Set the correct channel for checking against the preset value */
          presetChannelSelectRegister = registers_->preset_channel_select_reg;
          presetChannelSelectRegister &= ~SIS3820_FOUR_BIT_MASK;
          presetChannelSelectRegister |= i;
          registers_->preset_channel_select_reg = presetChannelSelectRegister;
        } else {
          registers_->preset_group2_reg = preset;
          /* Enable this bank of counters for preset checking */
          registers_->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP2;
          /* Set the correct channel for checking against the preset value */
          presetChannelSelectRegister = registers_->preset_channel_select_reg;
          presetChannelSelectRegister &= ~(SIS3820_FOUR_BIT_MASK << 16);
          presetChannelSelectRegister |= (i << 16);
          registers_->preset_channel_select_reg = presetChannelSelectRegister;
        }
      }
    }
  }
}


void drvSIS3820::setControlStatusReg()
{
  /* Set up the default behaviour of the card */
  /* Initially, this will have the following:
   * User LED off
   * Counter test modes disabled
   * Reference pulser enabled to Channel 1
   * LNE prescaler active
   */

  epicsUInt32 controlRegister = 0;

  controlRegister |= CTRL_USER_LED_OFF;
  controlRegister |= CTRL_COUNTER_TEST_25MHZ_DISABLE;
  controlRegister |= CTRL_COUNTER_TEST_MODE_DISABLE;
  /*controlRegister |= CTRL_REFERENCE_CH1_ENABLE;*/

  registers_->control_status_reg = controlRegister;
}


void drvSIS3820::setOpModeReg()
{
  /* Need to set this up to be accessed from asyn interface to allow changes on
   * the fly. Prior to that, I should split it out to allow individual changes
   * to be made from iocsh.
   */

  /* Set up the operation mode of the SIS3820.
     Initial mode will be:
   * Multichannel scaler
   * Front panel output mode from parameter
   * Front panel input mode from parameter
   * FIFO emulation mode
   * Arm/enable mode LNE front panel
   * LNE sourced from front panel
   * 32 bit data format
   * Clearing mode - want the incremental counts
   */

  epicsUInt32 operationRegister = 0;
  int maxOutputMode = 2;

  operationRegister |= SIS3820_MCS_DATA_FORMAT_32BIT;
  operationRegister |= SIS3820_SCALER_DATA_FORMAT_32BIT;

  if (acquireMode_ == MCS_MODE) {
    operationRegister |= SIS3820_CLEARING_MODE;
    operationRegister |= SIS3820_ARM_ENABLE_CONTROL_SIGNAL;
    operationRegister |= SIS3820_FIFO_MODE;
    operationRegister |= SIS3820_OP_MODE_MULTI_CHANNEL_SCALER;
    if (lneSource_ == SIS3820_LNE_SOURCE_INTERNAL_10MHZ) {
      operationRegister |= SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
    }
    if (lneSource_ == SIS3820_LNE_SOURCE_CONTROL_SIGNAL) {
      operationRegister |= SIS3820_LNE_SOURCE_CONTROL_SIGNAL;
    }
  } else {
    operationRegister |= SIS3820_NON_CLEARING_MODE;
    operationRegister |= SIS3820_LNE_SOURCE_VME;
    operationRegister |= SIS3820_ARM_ENABLE_CONTROL_SIGNAL;
    operationRegister |= SIS3820_FIFO_MODE;
    operationRegister |= SIS3820_HISCAL_START_SOURCE_VME;
    operationRegister |= SIS3820_OP_MODE_SCALER;
  }

  /* Check that input mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */

  if (inputMode_ < 0 || inputMode_ > 5)
    inputMode_ = 0;

  operationRegister |= inputMode_ << SIS3820_INPUT_MODE_SHIFT;

  /* Check that output mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */
  if (firmwareVersion_ >= 0x010A)
    maxOutputMode = 3;
  if (outputMode_ < 0 || outputMode_ > maxOutputMode)
    outputMode_ = 0;

  operationRegister |= outputMode_ << SIS3820_OUTPUT_MODE_SHIFT;

  registers_->op_mode_reg = operationRegister;
}


void drvSIS3820::setIrqControlStatusReg()
{
  /* Set the desired interrupts. The SIS3820 can generate interrupts on
   * the following conditions:
   * 0 = LNE/clock shadow
   * 1 = FIFO threshold
   * 2 = Acquisition completed
   * 3 = Overflow
   * 4 = FIFO almost full
   */
  epicsUInt32 interruptRegister = 0;

  /* This register is set up the same for MCS_MODE and SCALER_MODE) */

  interruptRegister |= SIS3820_IRQ_SOURCE0_DISABLE;
  /* NOTE: WE ARE NOT USING FIFO THRESHOLD INTERRUPTS FOR NOW */
  interruptRegister |= SIS3820_IRQ_SOURCE1_DISABLE;
  interruptRegister |= SIS3820_IRQ_SOURCE2_ENABLE;
  interruptRegister |= SIS3820_IRQ_SOURCE3_DISABLE;
  interruptRegister |= SIS3820_IRQ_SOURCE4_ENABLE;
  interruptRegister |= SIS3820_IRQ_SOURCE5_DISABLE;
  interruptRegister |= SIS3820_IRQ_SOURCE6_DISABLE;
  interruptRegister |= SIS3820_IRQ_SOURCE7_DISABLE;

  interruptRegister |= SIS3820_IRQ_SOURCE0_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE1_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE2_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE3_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE4_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE5_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE6_CLEAR;
  interruptRegister |= SIS3820_IRQ_SOURCE7_CLEAR;

  registers_->irq_control_status_reg = interruptRegister;
}

void drvSIS3820::enableInterrupts()
{
 registers_->irq_config_reg &= ~SIS3820_IRQ_ENABLE;
}

void drvSIS3820::disableInterrupts()
{
  registers_->irq_config_reg |= SIS3820_IRQ_ENABLE;
}



/**********************/
/* Interrupt handling */
/**********************/

static void intFuncC(void *drvPvt)
{
  drvSIS3820 *pSIS3820 = (drvSIS3820*)drvPvt;
  pSIS3820->intFunc();
}
  

void drvSIS3820::intFunc()
{
  /* Disable interrupts */
  registers_->irq_config_reg &= ~SIS3820_IRQ_ENABLE;

  /* Test which interrupt source has triggered this interrupt. */
  irqStatusReg_ = registers_->irq_control_status_reg;

  /* Check for the FIFO threshold interrupt */
  if (irqStatusReg_ & SIS3820_IRQ_SOURCE1_FLAG)
  {
    /* Note that this is a level-sensitive interrupt, not edge sensitive, so it can't be cleared */
    /* Disable this interrupt, since it is caused by FIFO threshold, and that
     * condition is only cleared in the readFIFO routine */
    registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE1_DISABLE;
  }

  /* Check for the data acquisition complete interrupt */
  else if (irqStatusReg_ & SIS3820_IRQ_SOURCE2_FLAG)
  {
    /* Reset the interrupt source */
    registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE2_CLEAR;
    acquiring_ = false;
  }

  /* Check for the FIFO almost full interrupt */
  else if (irqStatusReg_ & SIS3820_IRQ_SOURCE4_FLAG)
  {
    /* This interrupt represents an error condition of sorts. For the moment, I
     * will terminate data collection, as it is likely that data will have been
     * lost.
     * Note that this is a level-sensitive interrupt, not edge sensitive, so it can't be cleared.
     * Instead we disable the interrupt, and re-enable it at the end of readFIFO.
     */
    registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE4_DISABLE;
  }

  /* Send an event to intTask to read the FIFO and perform any requested callbacks */
  epicsEventSignal(readFIFOEventId_);

  /* Reenable interrupts */
  registers_->irq_config_reg |= SIS3820_IRQ_ENABLE;

}

/**********************/
/* DMA handling       */
/**********************/

static void dmaCallbackC(void *drvPvt)
{
  drvSIS3820 *pSIS3820 = (drvSIS3820*)drvPvt;
  pSIS3820->dmaCallback();
}

void drvSIS3820::dmaCallback()
{
  epicsEventSignal(dmaDoneEventId_);
}

void drvSIS3820::resetFIFO()
{
  epicsMutexLock(fifoLockId_);
  registers_->key_fifo_reset_reg= 1;
  epicsMutexUnlock(fifoLockId_);
}  

void readFIFOThreadC(void *drvPvt)
{
  drvSIS3820 *pSIS3820 = (drvSIS3820*)drvPvt;
  pSIS3820->readFIFOThread();
}

/** This thread is woken up by an interrupt or a request to read status
  * It loops calling readFIFO until acquiring_ goes to false.
  * readFIFO only reads a limited amount of FIFO data at once in order
  * to avoid blocking the device support threads. */
void drvSIS3820::readFIFOThread()
{
  bool acquiring;
  int status;
  int count;
  int signal;
  int chan;
  int i;
  epicsUInt32 *pIn, *pOut;
  epicsTimeStamp t1, t2, t3;
  static const char* functionName="readFIFOThread";

  while(true)
  {
    epicsEventWait(readFIFOEventId_);
    acquiring = true;
    while (acquiring) {
      disableInterrupts();
      lock();
      signal = nextSignal_;
      chan = nextChan_;
      // This block of code can be slow and does not require the asynPortDriver lock because we are not
      // accessing object data that could change.  
      // It does require the FIFO lock so no one resets the FIFO while it executes
      epicsMutexLock(fifoLockId_);
      unlock();
      count = registers_->fifo_word_count_reg;
      if (count > fifoBufferWords_) count = fifoBufferWords_;
      epicsTimeGetCurrent(&t1);
      if (useDma_ && (count >= MIN_DMA_TRANSFERS)) {
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                  "%s:%s: doing DMA transfer, fifoBuffer=%p, fifo_base=%p, count=%d\n",
                  driverName, functionName, fifoBuffer_, fifo_base_, count);
        status = sysDmaFromVme(dmaId_, fifoBuffer_, (int)fifo_base_, VME_AM_EXT_SUP_D64BLT, (count)*sizeof(int), 8);
        if (status) {
          asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                    "%s:%s: doing DMA transfer, error calling sysDmaFromVme, status=%d, buff=%p, fifo_base=%p, count=%d\n",
                    driverName, functionName, status, fifoBuffer_, fifo_base_, count);
        } 
        else {
          epicsEventWait(dmaDoneEventId_);
          status = sysDmaStatus(dmaId_);
          if (status)
             asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                       "%s:%s: DMA error, errno=%d, message=%s\n",
                       driverName, functionName, errno, strerror(errno));
        }
      } else {    
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                  "%s:%s: memcpy transfer, count=%d\n",
                  driverName, functionName, count);
        memcpy(fifoBuffer_, fifo_base_, count*sizeof(int));
      }
      epicsTimeGetCurrent(&t2);

      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: read FIFO (%d) in %fs, fifo word count after=%d\n",
                driverName, functionName, count, epicsTimeDiffInSeconds(&t2, &t1), registers_->fifo_word_count_reg);
      // Release the FIFO lock, we are done accessing the FIFO
      epicsMutexUnlock(fifoLockId_);
      
      // Copy the data from the FIFO buffer to the mcsBuffer
      pOut = mcsBuffer_ + signal*maxChans_ + chan;
      pIn = fifoBuffer_;
      for (i=0; i<count; i++) {
        *pOut = *pIn++;
        signal++;
        if (signal == maxSignals_) {
          signal = 0;
          chan++;
          pOut = mcsBuffer_ + chan;
        } else {
          pOut += maxChans_;
        }
      }
      
      epicsTimeGetCurrent(&t2);
      // Take the lock since we are now changing object data
      lock();
      nextChan_ = chan;
      nextSignal_ = signal;
      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: copied data to mcsBuffer in %fs, nextChan=%d, nextSignal=%d\n",
                driverName, functionName, epicsTimeDiffInSeconds(&t3, &t3), nextChan_, nextSignal_);

      acquiring = checkDone();
      /* Re-enable FIFO threshold and FIFO almost full interrupts */
      /* NOTE: WE ARE NOT USING FIFO THRESHOLD INTERRUPTS FOR NOW */
      /* registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE1_ENABLE; */
      registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE4_ENABLE;

      // Release the lock and sleep for a short time
      unlock();
      enableInterrupts();
      epicsThreadSleep(epicsThreadSleepQuantum());
    }
  }
}

extern "C" {
int drvSIS3820Config(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                     int maxChans, int maxSignals, int inputMode, int outputMode, 
                     int useDma, int fifoBufferWords)
{
  drvSIS3820 *pSIS3820 = new drvSIS3820(portName, baseAddress, interruptVector, interruptLevel,
                                        maxChans, maxSignals, inputMode, outputMode, 
                                        useDma, fifoBufferWords);
  pSIS3820 = NULL;
  return 0;
}

/* iocsh config function */
static const iocshArg drvSIS3820ConfigArg0 = { "Asyn port name",   iocshArgString};
static const iocshArg drvSIS3820ConfigArg1 = { "Base address",     iocshArgInt};
static const iocshArg drvSIS3820ConfigArg2 = { "Interrupt vector", iocshArgInt};
static const iocshArg drvSIS3820ConfigArg3 = { "Interrupt level",  iocshArgInt};
static const iocshArg drvSIS3820ConfigArg4 = { "MaxChannels",      iocshArgInt};
static const iocshArg drvSIS3820ConfigArg5 = { "MaxSignals",       iocshArgInt};
static const iocshArg drvSIS3820ConfigArg6 = { "InputMode",        iocshArgInt};
static const iocshArg drvSIS3820ConfigArg7 = { "OutputMode",       iocshArgInt};
static const iocshArg drvSIS3820ConfigArg8 = { "Use DMA",          iocshArgInt};
static const iocshArg drvSIS3820ConfigArg9 = { "FIFO buffer words", iocshArgInt};

static const iocshArg * const drvSIS3820ConfigArgs[] = 
{ &drvSIS3820ConfigArg0,
  &drvSIS3820ConfigArg1,
  &drvSIS3820ConfigArg2,
  &drvSIS3820ConfigArg3,
  &drvSIS3820ConfigArg4,
  &drvSIS3820ConfigArg5,
  &drvSIS3820ConfigArg6,
  &drvSIS3820ConfigArg7,
  &drvSIS3820ConfigArg8,
  &drvSIS3820ConfigArg9
};

static const iocshFuncDef drvSIS3820ConfigFuncDef = 
  {"drvSIS3820Config",10,drvSIS3820ConfigArgs};

static void drvSIS3820ConfigCallFunc(const iocshArgBuf *args)
{
  drvSIS3820Config(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
                   args[4].ival, args[5].ival, args[6].ival, args[7].ival,
                   args[8].ival, args[9].ival);
}

void drvSIS3820Register(void)
{
  iocshRegister(&drvSIS3820ConfigFuncDef,drvSIS3820ConfigCallFunc);
}

epicsExportRegistrar(drvSIS3820Register);

} // extern "C"

