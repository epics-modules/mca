/* File:    drvSIS3801.cpp
 * Author:  Mark Rivers, University of Chicago
 * Date:    22-Apr-2011
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for the SIS3801 and SIS3801 multichannel scalers.  This is the SIS3920 class.
 *
 * Acknowledgements:
 * This driver module is based on previous versions by Mark Rivers from 1997.
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
#include "drvSIS3801.h"

static const char *driverName="drvSIS3801";
static void intFuncC(void *drvPvt);
static void readFIFOThreadC(void *drvPvt);

/***************/
/* Definitions */
/***************/

/*Constructor */
drvSIS3801::drvSIS3801(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                       int maxChans, int maxSignals)
  :  drvSIS38XX(portName, maxChans, maxSignals)

{
  int status;
  int firmware;
  epicsUInt32 controlStatusReg;
  epicsUInt32 moduleID;
  static const char* functionName="SIS3801";
  
  setIntegerParam(SIS38XXModel_, MODEL_SIS3801);
  
  /* Call devLib to get the system address that corresponds to the VME
   * base address of the board.
   */
  status = devRegisterAddress("drvSIS3801",
                               SIS3801_ADDRESS_TYPE,
                               (size_t)baseAddress,
                               SIS3801_BOARD_SIZE,
                               (volatile void **)&registers_);

  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: %s, Can't register VME address %p\n", 
              driverName, functionName, portName, baseAddress);
    return;
  }
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: Registered VME address: %p to local address: %p size: 0x%X\n", 
            driverName, functionName, baseAddress, registers_, SIS3801_BOARD_SIZE);

  /* Probe VME bus to see if card is there */
  status = devReadProbe(4, (char *) registers_->csr_reg,
                       (char *) &controlStatusReg);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: devReadProbe failure = %d\n", 
              driverName, functionName, status);
    return;
  }

  /* Get the module info from the card */
  moduleID = (registers_->irq_reg & 0xFFFF0000) >> 16;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: module ID=%x\n", 
            driverName, functionName, moduleID);
  firmware = (registers_->irq_reg & 0x0000F000) >> 12;
  setIntegerParam(SIS38XXFirmware_, firmware);
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: firmware=%d\n",
            driverName, functionName, firmware);

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

  setControlStatusReg();

  /* Initialize board in MCS mode */
  setAcquireMode(ACQUIRE_MODE_MCS);

  /* Set number of readout channels to maxSignals
   * Assumes that the lower channels will be used first, and the only unused
   * channels will be at the upper end of the channel range.
   * Create a mask with zeros in the rightmost maxSignals bits,
   * 1 in all higher order bits. */
  registers_->copy_disable_reg = 0xffffffff<<maxSignals_;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: setting copy disable register=0x%08x\n",
            driverName, functionName, 0xffffffff<<maxSignals_);

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
            driverName, functionName, registers_->irq_reg);

  registers_->irq_reg |= (interruptLevel << 8);

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq after setting IntLevel= 0x%x\n", 
             driverName, functionName, registers_->irq_reg);

  registers_->irq_reg |= interruptVector;

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "m%s:%s: irq config register after setting interrupt vector = 0x%08x\n", 
            driverName, functionName, registers_->irq_reg);

  /* Create the thread that reads the FIFO */
  if (epicsThreadCreate("SIS3801FIFOThread",
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
            driverName, functionName);

  erase();

  /* Enable interrupts in hardware */  
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq before enabling interrupts= 0x%08x\n", 
            driverName, functionName, registers_->irq_reg);

  enableInterrupts();

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



/* Report  parameters */
void drvSIS3801::report(FILE *fp, int details)
{

  if (exists_) 
    fprintf(fp, "SIS3801: asyn port: %s, connected at VME base address %p, maxChans=%d\n",
            portName, registers_, maxChans_);
  else
    fprintf(fp, "SIS3801: asyn port: %s, not connected!!!\n",
            portName);
  if (details > 1) {
    fprintf(fp, "  Registers:\n");
    fprintf(fp, "    csr_reg              = 0x%x\n",   registers_->csr_reg);
    fprintf(fp, "    irq_reg              = 0x%x\n",   registers_->irq_reg);
    fprintf(fp, "    prescale_factor_reg  = 0x%x\n",   registers_->prescale_factor_reg);
  }
  // Call the base class method
  drvSIS38XX::report(fp, details);
}

void drvSIS3801::erase()
{
  //static const char *functionName="erase";

  if (!exists_) return;
  
    // Call the base class method
  drvSIS38XX::erase();
  
  /* Erase FIFO and counters on board */
  resetFIFO();
}


void drvSIS3801::startMCSAcquire()
{
  int nChans;
  int prescale;
  int channelAdvanceSource;
  int initialChannelAdvance;
  int firmwareVersion;
  static const char *functionName="startMCSAcquire";
 
  getIntegerParam(mcaNumChannels_, &nChans);
  getIntegerParam(mcaChannelAdvanceSource_, &channelAdvanceSource);
  getIntegerParam(SIS38XXInitialChannelAdvance_, &initialChannelAdvance);
  getIntegerParam(mcaPrescale_, &prescale);
  getIntegerParam(SIS38XXFirmware_, &firmwareVersion);

  if (firmwareVersion >= 5) {
    if (channelAdvanceSource == mcaChannelAdvance_Internal) {
        /* The SIS3801 requires the value in the LNE prescale register to be one
         * less than the actual number of incoming signals. We do this adjustment
         * here, so the user sees the actual number at the record level.
         */
        double dwellTime;
        getDoubleParam(mcaDwellTime_, &dwellTime);
        registers_->csr_reg = CONTROL_M_ENABLE_10MHZ_LNE_PRESCALER;
        prescale = (epicsUInt32) (SIS3801_10MHZ_CLOCK * dwellTime) - 1;
      }
    else if (channelAdvanceSource == mcaChannelAdvance_External) {
        getIntegerParam(mcaPrescale_, &prescale);
        /* The SIS3801 requires the value in the LNE prescale register to be one
         * less than the actual number of incoming signals. We do this adjustment
         * here, so the user sees the actual number at the record level.
         */
        prescale--;
        registers_->csr_reg = CONTROL_M_DISABLE_10MHZ_LNE_PRESCALER;
      } 
    else {
      asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                "%s:%s: unsupported channel advance source %d\n", 
                driverName, functionName, channelAdvanceSource);
    }

    /* Set the prescale factor. Note, it is necessary to do the following in
     * for the prescale counter to be properly cleared:
     *  - Disable prescaler
     *  - Write prescale factor
     *  - Enable prescaler
     */
    registers_->csr_reg = CONTROL_M_DISABLE_LNE_PRESCALER;
    registers_->prescale_factor_reg = prescale;
    registers_->csr_reg = CONTROL_M_ENABLE_LNE_PRESCALER;
  }

  /* Enable next logic */
  registers_->enable_next_reg = 1;

  /* Enable counting bit in CSR */
  registers_->csr_reg = CONTROL_M_CLEAR_SOFTWARE_DISABLE;

  /* Do one software next_clock. This starts the module counting without 
   * waiting for the first external next clock. 
   * Only do this if:
   *    Old firmware (not new SIS modules) OR
   *    (Using MCS (not scaler) MODE) AND
   *    ((Using internal channel advance) OR
   *    ((Using external channel advance) AND (softAdvance flag=1))
   */
  if (initialChannelAdvance != 0)
    softwareChannelAdvance();
}

void drvSIS3801::softwareChannelAdvance()
{
  //static const char *functionName="softwareChannelAdvance";
  registers_->soft_next_reg = 1; 
}


void drvSIS3801::stopMCSAcquire()
{
  //static const char *functionName="stopMCSAcquire";

  /* Turn off hardware acquisition */
  /* Disable counting bit in CSR */
  registers_->csr_reg = CONTROL_M_SET_SOFTWARE_DISABLE;

  /* Disable next logic */
  registers_->disable_next_reg = 1;
  acquiring_ = false;
}

void drvSIS3801::startScaler()
{
  //static const char *functionName="startScaler";

  registers_->csr_reg = CONTROL_M_ENABLE_10MHZ_LNE_PRESCALER;
  registers_->csr_reg = CONTROL_M_DISABLE_LNE_PRESCALER;
  registers_->prescale_factor_reg = ((SIS3801_10MHZ_CLOCK / SIS3801_SCALER_MODE_RATE) - 1);
  registers_->csr_reg = CONTROL_M_ENABLE_LNE_PRESCALER;
  registers_->enable_next_reg = 1;
  /* Enable counting bit in CSR */
  registers_->csr_reg = CONTROL_M_CLEAR_SOFTWARE_DISABLE;
  registers_->soft_next_reg = 1; 
  // Wake up the FIFO reading thread
  epicsEventSignal(readFIFOEventId_);
}

void drvSIS3801::stopScaler()
{
  //static const char *functionName="stopScaler";
  stopMCSAcquire();
}

void drvSIS3801::readScalers()
{
  // Nothing to do, readFIFO handles it
}

void drvSIS3801::resetScaler()
{
  //static const char *functionName="resetScaler";
  resetFIFO();
  nextChan_ = 0;
  nextSignal_ = 0;
}


void drvSIS3801::clearScalerPresets()
{
  // Nothing to do
}

void drvSIS3801::setScalerPresets()
{
  // Nothing to do
}

void drvSIS3801::setAcquireMode(SIS38XXAcquireMode_t acquireMode)
{
  SIS38XXChannel1Source_t channel1Source;
  int firmwareVersion;
  static const char* functionName="setAcquireMode";
  
  getIntegerParam(SIS38XXChannel1Source_, (int*)&channel1Source);
  getIntegerParam(SIS38XXFirmware_, &firmwareVersion);
  
  /* Enable or disable 25 MHz channel 1 reference pulses. */
  if ((channel1Source == CHANNEL1_SOURCE_INTERNAL) && (firmwareVersion >= 5))
    registers_->enable_ch1_pulser = 1;
  else
    registers_->disable_ch1_pulser = 1;

  if (acquireMode_ == acquireMode) return;  /* Nothing to do */
  acquireMode_ = acquireMode;
  setIntegerParam(SIS38XXAcquireMode_, acquireMode);
  callParamCallbacks();

    /* Initialize board and set the control status register */
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: initialising control status register\n",
            driverName, functionName);
  setControlStatusReg();
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
            "%s:%s: control status register = 0x%08x\n",
            driverName, functionName, registers_->csr_reg);

}

void drvSIS3801::setInputMode()
{
  int inputMode;
  //static const char* functionName="setInputMode";

  getIntegerParam(SIS38XXInputMode_, &inputMode);

  /* Check that input mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register. */
  if (inputMode < 0 || inputMode > 3) inputMode = 0;

  if (inputMode & 1) registers_->csr_reg = CONTROL_M_SET_INPUT_MODE_BIT_0;
  else               registers_->csr_reg = CONTROL_M_CLEAR_INPUT_MODE_BIT_0;
  if (inputMode & 2) registers_->csr_reg = CONTROL_M_SET_INPUT_MODE_BIT_1;
  else               registers_->csr_reg = CONTROL_M_CLEAR_INPUT_MODE_BIT_1;
}

void drvSIS3801::setOutputMode()
{
  // No-op for 3801
}


void drvSIS3801::setLED()
{
  int value;
  //static const char* functionName="setLED";
  
  getIntegerParam(SIS38XXLED_, &value);
  if (value == 0)
    registers_->csr_reg = CONTROL_M_CLEAR_USER_LED;
  else
    registers_->csr_reg = CONTROL_M_SET_USER_LED;
}

int drvSIS3801::getLED()
{
  int value;
  
  value = registers_->csr_reg & STATUS_M_USER_LED;
  return (value == 0) ? 0:1;
}

void drvSIS3801::setMuxOut()
{
  // No-op for 3801
}

int drvSIS3801::getMuxOut()
{
  // No-op for 3801
  return 1;
}



void drvSIS3801::setControlStatusReg()
{
  int firmwareVersion;
  //static const char* functionName="setControlStatusReg";

  getIntegerParam(SIS38XXFirmware_, &firmwareVersion);
  
  /* Set up the default behaviour of the card */
  registers_->csr_reg = CONTROL_M_DISABLE_FIFO_TEST_MODE;    /* Disable FIFO test mode */
  registers_->csr_reg = CONTROL_M_DISABLE_25MHZ_TEST_PULSES; /* No 25MHz test pulses */
  registers_->csr_reg = CONTROL_M_DISABLE_INPUT_TEST_MODE;   /* Disable test input */
  if (firmwareVersion < 5) {
  registers_->csr_reg = CONTROL_M_DISABLE_BROADCAST_MODE;    /* No broadcast mode */
  registers_->csr_reg = CONTROL_M_DISABLE_BROADCAST_HAND;    /* No broadcast handshake */
  }
  registers_->csr_reg = CONTROL_M_ENABLE_EXTERNAL_NEXT;      /* Enable external NEXT */
  registers_->csr_reg = CONTROL_M_ENABLE_EXTERNAL_CLEAR;     /* Enable external CLEAR */
  registers_->csr_reg = CONTROL_M_ENABLE_EXTERNAL_DISABLE;   /* Enable external DISABLE */
  registers_->csr_reg = CONTROL_M_SET_SOFTWARE_DISABLE;      /* Disable counting */
  registers_->csr_reg = CONTROL_M_DISABLE_IRQ_0;             /* Disable start of CIP IRQ */
  registers_->csr_reg = CONTROL_M_DISABLE_IRQ_1;             /* Disable FIFO almost empty (full?) */
  registers_->csr_reg = CONTROL_M_DISABLE_IRQ_2;             /* Disable FIFO half full */
  registers_->csr_reg = CONTROL_M_ENABLE_IRQ_3;              /* Enable FIFO almost full */
}


/**********************/
/* Interrupt handling */
/**********************/

void drvSIS3801::enableInterrupts()
{
  //static const char* functionName="enableInterrupts";

 registers_->irq_reg |= IRQ_M_VME_IRQ_ENABLE;
}

void drvSIS3801::disableInterrupts()
{
  //static const char* functionName="disableInterrupts";

 registers_->irq_reg &= ~IRQ_M_VME_IRQ_ENABLE;
}


static void intFuncC(void *drvPvt)
{
  drvSIS3801 *pSIS3801 = (drvSIS3801*)drvPvt;
  pSIS3801->intFunc();
}
  

void drvSIS3801::intFunc()
{
  /* Disable interrupts */
  disableInterrupts();

  /* Test which interrupt source has triggered this interrupt??? */

  /* Send an event to readFIFO thread to read the FIFO.  It will re-enable interrupts when the FIFO is empty */
  epicsEventSignal(readFIFOEventId_);

}

void drvSIS3801::resetFIFO()
{
  //static const char* functionName="resetFIFO";

  epicsMutexLock(fifoLockId_);
  registers_->clear_fifo_reg = 1;
  epicsMutexUnlock(fifoLockId_);
}  

void readFIFOThreadC(void *drvPvt)
{
  drvSIS3801 *pSIS3801 = (drvSIS3801*)drvPvt;
  pSIS3801->readFIFOThread();
}

/** This thread is woken up by an interrupt or a request to read status
  * It loops calling readFIFO until acquiring_ goes to false.
  * readFIFO only reads a limited amount of FIFO data at once in order
  * to avoid blocking the device support threads. */
void drvSIS3801::readFIFOThread()
{
  int count;
  int signal;
  int chan;
  int nChans;
  int status;
  int i;
  bool acquiring;  // We have a separate flag because we need to continue processing one last
                   // time even if acquiring_ goes to false because acquisition was manually stopped
  epicsUInt32 scalerPresets[SIS38XX_MAX_SIGNALS];
  epicsUInt32 *pOut=NULL;
  epicsTimeStamp t1, t2;
  static const char* functionName="readFIFOThread";

  while(true)
  {
    epicsEventWait(readFIFOEventId_);
    // We got an event, which can come from acquisition starting, or FIFO full interrupt
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s:%s: got readFIFOEvent, interrupt status=0x%8.8x\n",
              driverName, functionName, registers_->csr_reg & 0xFFF00000);
    lock();
    for (i=0; i<maxSignals_; i++) 
      getIntegerParam(i, scalerPresets_, (int *)&scalerPresets[i]);
    getIntegerParam(mcaNumChannels_, &nChans);
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
              "%s:%s: scaler presets[0]=%d, scalerData[0]=%d\n",
              driverName, functionName, scalerPresets[0], scalerData_[0]);
    acquiring = acquiring_;
    unlock();
    while (acquiring) {
      lock();
      signal = nextSignal_;
      chan = nextChan_;
      count = 0;
      // This block of code can be slow and does not require the asynPortDriver lock because we are not
      // accessing object data that could change.  
      // It does require the FIFO lock so no one resets the FIFO while it executes
      epicsMutexLock(fifoLockId_);
      unlock();
      epicsTimeGetCurrent(&t1);

      /* Read out FIFO. It would be more efficient not to check the empty
       * flag on each transfer, using the almost empty flag.  But this has gotten
       * too complex, and is unlikely to save time on new boards with lots of
       * memory.
       */
      if (acquireMode_== ACQUIRE_MODE_MCS) {
        // Copy the data from the FIFO to the mcsBuffer
        pOut = mcsData_ + signal*maxChans_ + chan;
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s:%s: pOut=%p, signal=%d, chan=%d\n",
                  driverName, functionName, pOut, signal, chan);
        while (((registers_->csr_reg & STATUS_M_FIFO_FLAG_EMPTY)==0) && (chan < nChans) && acquiring_) {
          *pOut = registers_->fifo_reg;
          signal++;
          count++;
          if (signal >= maxSignals_) {
            signal = 0;
            chan++;
            pOut = mcsData_ + chan;
          } else {
            pOut += maxChans_;
          }
        }
      } else if (acquireMode_ == ACQUIRE_MODE_SCALER) {
        while ((registers_->csr_reg & STATUS_M_FIFO_FLAG_EMPTY)==0 && acquiring_) {
          scalerData_[signal] += registers_->fifo_reg;
          signal++;
          count++;
          if (signal >= maxSignals_) {
            for (i=0; i<maxSignals_; i++) {
              if ((scalerPresets[i] != 0) && 
                  (scalerData_[i] >= scalerPresets[i]))
                acquiring = false;
            }
            asynPrintIO(pasynUserSelf, ASYN_TRACEIO_DRIVER, 
                      (const char*)scalerData_, nChans*sizeof(epicsUInt32), 
                      "%s:%s:\n",
                      driverName, functionName);
            if (!acquiring) break;
            signal = 0;
          }
        }
      }
      epicsTimeGetCurrent(&t2);

      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: read FIFO (%d) in %fs, acquiring=%d\n",
                driverName, functionName, count, epicsTimeDiffInSeconds(&t2, &t1), acquiring_);
      // Release the FIFO lock, we are done accessing the FIFO
      epicsMutexUnlock(fifoLockId_);
      
      // Take the lock since we are now changing object data
      lock();
      nextChan_ = chan;
      nextSignal_ = signal;
      if (acquireMode_ == ACQUIRE_MODE_MCS) {
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s:%s: pOut=%p,signal=%d, chan=%d\n",
                  driverName, functionName, pOut, signal, chan);
        checkMCSDone();
      } else if (acquireMode_ == ACQUIRE_MODE_SCALER) {
        if (!acquiring) acquiring_ = false;
        if (!acquiring_) {
          asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s:%s: scaler done, doing callbacks\n",
                    driverName, functionName);
          stopScaler();
          setIntegerParam(scalerDone_, 1);
          callParamCallbacks();
        }
      }
      /* Reenable interrupts in case we were woken up by an interrupt for FIFO almost full */
      enableInterrupts();
      acquiring = acquiring_;
      // Release the lock and sleep for a short time, but wake up if there is an interrupt
      unlock();
      status = epicsEventWaitWithTimeout(readFIFOEventId_, epicsThreadSleepQuantum());
      if (status == epicsEventWaitOK) 
        asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                  "%s:%s: got interrupt in epicsEventWaitWithTimeout\n",
                  driverName, functionName);
    }  
  }
}

extern "C" {
int drvSIS3801Config(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                     int maxChans, int maxSignals)
{
  drvSIS3801 *pSIS3801 = new drvSIS3801(portName, baseAddress, interruptVector, interruptLevel,
                                        maxChans, maxSignals);
  pSIS3801 = NULL;
  return 0;
}

/* iocsh config function */
static const iocshArg drvSIS3801ConfigArg0 = { "Asyn port name",   iocshArgString};
static const iocshArg drvSIS3801ConfigArg1 = { "Base address",     iocshArgInt};
static const iocshArg drvSIS3801ConfigArg2 = { "Interrupt vector", iocshArgInt};
static const iocshArg drvSIS3801ConfigArg3 = { "Interrupt level",  iocshArgInt};
static const iocshArg drvSIS3801ConfigArg4 = { "MaxChannels",      iocshArgInt};
static const iocshArg drvSIS3801ConfigArg5 = { "MaxSignals",       iocshArgInt};

static const iocshArg * const drvSIS3801ConfigArgs[] = 
{ &drvSIS3801ConfigArg0,
  &drvSIS3801ConfigArg1,
  &drvSIS3801ConfigArg2,
  &drvSIS3801ConfigArg3,
  &drvSIS3801ConfigArg4,
  &drvSIS3801ConfigArg5
};

static const iocshFuncDef drvSIS3801ConfigFuncDef = 
  {"drvSIS3801Config",6,drvSIS3801ConfigArgs};

static void drvSIS3801ConfigCallFunc(const iocshArgBuf *args)
{
  drvSIS3801Config(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
                   args[4].ival, args[5].ival);
}

void drvSIS3801Register(void)
{
  iocshRegister(&drvSIS3801ConfigFuncDef,drvSIS3801ConfigCallFunc);
}

epicsExportRegistrar(drvSIS3801Register);

} // extern "C"
