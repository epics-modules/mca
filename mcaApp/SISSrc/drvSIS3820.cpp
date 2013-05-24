/* File:    drvSIS3820.cpp
 * Author:  Mark Rivers, University of Chicago
 * Date:    22-Apr-2011
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for the SIS3820 and SIS3801 multichannel scalers.  This is the SIS3920 class.
 *
 * Acknowledgements:
 * This driver module is based on previous versions by Wayne Lewis and Ulrik Pedersen.
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

// Needed for memalign on vxWorks
#ifdef vxWorks
#include <memLib.h>
#endif

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

#include "vmeDMA.h"
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
                       int maxChans, int maxSignals, bool useDma, int fifoBufferWords)
  :  drvSIS38XX(portName, maxChans, maxSignals),
     useDma_(useDma)
{
  int status;
  epicsUInt32 controlStatusReg;
  epicsUInt32 moduleID;
  static const char* functionName="SIS3820";
  
  setIntegerParam(SIS38XXModel_, MODEL_SIS3820);
  
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
  fifoBaseVME_ = (epicsUInt32 *)(baseAddress + SIS3820_FIFO_BASE);
  status = devRegisterAddress("drvSIS3820",
                              SIS3820_ADDRESS_TYPE,
                              (size_t)fifoBaseVME_,
                              SIS3820_FIFO_BYTE_SIZE,
                              (volatile void **)&fifoBaseCPU_);

  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: %s, Can't register FIFO address %p\n", 
              driverName, functionName, portName, baseAddress + SIS3820_FIFO_BASE);
    return;
  }

  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: Registered VME FIFO address: %p to local address: %p size: 0x%X\n", 
            driverName, functionName, fifoBaseVME_, 
            fifoBaseCPU_, SIS3820_FIFO_BYTE_SIZE);

  /* Probe VME bus to see if card is there */
  status = devReadProbe(4, (char *) &(registers_->control_status_reg),
                       (char *) &controlStatusReg);
  if (status) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: devReadProbe failure for address %p = %d\n", 
              driverName, functionName, &registers_->control_status_reg, status);
    return;
  }

  /* Get the module info from the card */
  moduleID = (registers_->moduleID_reg & 0xFFFF0000) >> 16;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: module ID=%x\n", 
            driverName, functionName, moduleID);
  firmwareVersion_ = registers_->moduleID_reg & 0x0000FFFF;
  setIntegerParam(SIS38XXFirmware_, firmwareVersion_);
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: firmware=%d\n",
            driverName, functionName, firmwareVersion_);

  // Allocate FIFO readout buffer
  // fifoBufferWords input argument is in words, must be less than SIS3820_FIFO_WORD_SIZE
  if (fifoBufferWords == 0) fifoBufferWords = SIS3820_FIFO_WORD_SIZE;
  if (fifoBufferWords > SIS3820_FIFO_WORD_SIZE) fifoBufferWords = SIS3820_FIFO_WORD_SIZE;
  fifoBufferWords_ = fifoBufferWords;
#ifdef vxWorks
  fifoBuffer_ = (epicsUInt32*) memalign(8, fifoBufferWords_*sizeof(epicsUInt32));
#else
  void *ptr;
  status = posix_memalign(&ptr, 8, fifoBufferWords_*sizeof(epicsUInt32));
  if(status != 0) {
    printf("Error: posix_memalign status = %d\n", status);
  }
  fifoBuffer_ = (epicsUInt32*)ptr;
#endif
  if (fifoBuffer_ == NULL) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: posix_memalign failure for fifoBuffer_ = %d\n", 
              driverName, functionName, status);
    return;
  }
  
  dmaDoneEventId_ = epicsEventCreate(epicsEventEmpty);
  // Create the DMA ID
  if (useDma_) {
    dmaId_ = sysDmaCreate(dmaCallbackC, (void*)this);
    if (dmaId_ == 0 || (int)dmaId_ == -1) {
      asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: sysDmaCreate failed, errno=%d. Disabling use of DMA.\n",
                driverName, functionName, errno);
      useDma_ = false;
    }
  }
 
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
  
  // Disable 25MHz test pulses and test mode
  registers_->control_status_reg = CTRL_COUNTER_TEST_25MHZ_DISABLE;
  registers_->control_status_reg = CTRL_COUNTER_TEST_MODE_DISABLE;

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
  registers_->irq_config_reg &= ~SIS3820_IRQ_LEVEL_MASK;
  registers_->irq_config_reg |= (interruptLevel << 8);
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq after setting IntLevel= 0x%x\n", 
             driverName, functionName, registers_->irq_config_reg);

  /* Write interrupt vector to hardware */
  registers_->irq_config_reg &= ~SIS3820_IRQ_VECTOR_MASK;
  registers_->irq_config_reg |= interruptVector;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: irq = 0x%08x\n", 
            driverName, functionName, registers_->irq_config_reg);
            
  /* Initialize board in MCS mode. This will also set the initial value of the operation mode register. */
  setAcquireMode(ACQUIRE_MODE_MCS);

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
            driverName, functionName);

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



/* Report  parameters */
void drvSIS3820::report(FILE *fp, int details)
{

  fprintf(fp, "SIS3820: asyn port: %s, connected at VME base address %p, maxChans=%d\n",
          portName, registers_, maxChans_);
  if (details > 0) {
    int i;
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
    fprintf(fp, "    copy_disable_reg           = 0x%x\n",   registers_->copy_disable_reg);
    fprintf(fp, "    lne_channel_select_reg     = 0x%x\n",   registers_->lne_channel_select_reg);
    fprintf(fp, "    preset_channel_select_reg  = 0x%x\n",   registers_->preset_channel_select_reg);
    fprintf(fp, "    mux_out_channel_select_reg = 0x%x\n",   registers_->mux_out_channel_select_reg);
    fprintf(fp, "    count_disable_reg          = 0x%x\n",   registers_->count_disable_reg);
    fprintf(fp, "    count_clear_reg            = 0x%x\n",   registers_->count_clear_reg);
    fprintf(fp, "    counter_overflow_reg       = 0x%x\n",   registers_->counter_overflow_reg);
    fprintf(fp, "    ch1_17_high_bits_reg       = 0x%x\n",   registers_->ch1_17_high_bits_reg);
    fprintf(fp, "    sdram_prom_reg             = 0x%x\n",   registers_->sdram_prom_reg);
    fprintf(fp, "    xilinx_test_data_reg       = 0x%x\n",   registers_->xilinx_test_data_reg);
    fprintf(fp, "    xilinx_control             = 0x%x\n",   registers_->xilinx_control);
    for (i=0; i<32; i++) fprintf(fp,
                "    shadow_regs[%d]            = 0x%x\n",   i, registers_->shadow_regs[i]);         
    for (i=0; i<32; i++) fprintf(fp,
                "    counter_regs[%d]           = 0x%x\n",   i, registers_->counter_regs[i]);         
  }
  // Call the base class method
  drvSIS38XX::report(fp, details);
}

void drvSIS3820::erase()
{
  //static const char *functionName="erase";

  if (!exists_) return;
  
  // Call the base class method
  drvSIS38XX::erase();
  
  /* Erase FIFO and counters on board */
  resetFIFO();
  registers_->key_counter_clear = 1;

  return;
}

void drvSIS3820::startMCSAcquire()
{
  int countOnStart;
  int channelAdvanceSource;
  //static const char *functionName="startMCSAcquire";
  
  getIntegerParam(SIS38XXCountOnStart_, &countOnStart);
  getIntegerParam(mcaChannelAdvanceSource_, &channelAdvanceSource);

  setAcquireMode(ACQUIRE_MODE_MCS);

  if (channelAdvanceSource == mcaChannelAdvance_Internal) 
    registers_->key_op_enable_reg = 1;
  else if (channelAdvanceSource == mcaChannelAdvance_External) {
    if (countOnStart)
      registers_->key_op_enable_reg = 1;
    else
      registers_->key_op_arm_reg = 1;
  }
}

void drvSIS3820::stopMCSAcquire()
{
  /* Turn off hardware acquisition */
  registers_->key_op_disable_reg = 1;
}

void drvSIS3820::startScaler()
{
  setAcquireMode(ACQUIRE_MODE_SCALER);
  registers_->key_op_enable_reg = 1;
}

void drvSIS3820::stopScaler()
{
  registers_->key_op_disable_reg = 1;
  resetFIFO();
}

void drvSIS3820::readScalers()
{
  int i;
  for (i=0; i<maxSignals_; i++) {
    scalerData_[i] = registers_->counter_regs[i];
  }
}

void drvSIS3820::resetScaler()
{
  /* Reset scaler */
  setAcquireMode(ACQUIRE_MODE_SCALER);
  registers_->key_op_disable_reg = 1;
  resetFIFO();
  registers_->key_counter_clear = 1;
}


void drvSIS3820::clearScalerPresets()
{
  registers_->preset_channel_select_reg &= ~SIS3820_FOUR_BIT_MASK;
  registers_->preset_channel_select_reg &= ~(SIS3820_FOUR_BIT_MASK << 16);
  registers_->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP1;
  registers_->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP2;
  registers_->preset_group1_reg = 0;
  registers_->preset_group2_reg = 0;
}

void drvSIS3820::setScalerPresets()
{
  int i;
  epicsUInt32 presetChannelSelectRegister;
  int preset;

  if (acquireMode_ == ACQUIRE_MODE_SCALER) {
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

void drvSIS3820::setAcquireMode(SIS38XXAcquireMode_t acquireMode)
{
  SIS38XXChannel1Source_t channel1Source;
  int nChans;
  int prescale;
  int channelAdvanceSource;
  double dwellTime;
  static const char* functionName="setAcquireMode";

  acquireMode_ = acquireMode;
  setIntegerParam(SIS38XXAcquireMode_, acquireMode);

  getIntegerParam(SIS38XXChannel1Source_, (int*)&channel1Source);
  /* Enable or disable 50 MHz channel 1 reference pulses. */
  if (channel1Source == CHANNEL1_SOURCE_INTERNAL)
    registers_->control_status_reg |= CTRL_REFERENCE_CH1_ENABLE;
  else
    registers_->control_status_reg |= CTRL_REFERENCE_CH1_DISABLE;

  /* Set the interrupt control register */
  setIrqControlStatusReg();
  
  /* Set the operation mode register */
  setOpModeReg();

  switch (acquireMode_) {
    case ACQUIRE_MODE_MCS:
      getIntegerParam(mcaNumChannels_, &nChans);
      getIntegerParam(mcaChannelAdvanceSource_, &channelAdvanceSource);
      getIntegerParam(mcaPrescale_, &prescale);
      getDoubleParam(mcaDwellTime_, &dwellTime);

      /* Clear all presets from scaler mode */
      clearScalerPresets();
      
      /* Disable channel in MCS mode. We enable the first maxSignals_ inputs */
      registers_->copy_disable_reg = 0xFFFFFFFF << maxSignals_;
      
      /* Set the number of channels to acquire.  
       * We could be resuming acquisition so subtract nextChan_ */
      registers_->acq_preset_reg = nChans - nextChan_;

      /* Set the LNE channel NOTE: This should allow other sources in the future */
      registers_->lne_channel_select_reg = 0;

      if (channelAdvanceSource == mcaChannelAdvance_Internal) {
        /* The SIS3820 requires the value in the LNE prescale register to be one
         * less than the actual number of incoming signals. We do this adjustment
         * here, so the user sees the actual number at the record level.
         */
        registers_->op_mode_reg = (registers_->op_mode_reg & ~0xF0) | SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
        registers_->lne_prescale_factor_reg = 
          (epicsUInt32) (SIS3820_10MHZ_CLOCK * dwellTime) - 1;
      }
      else if (channelAdvanceSource == mcaChannelAdvance_External) {
        /* The SIS3820 requires the value in the LNE prescale register to be one
         * less than the actual number of incoming signals. We do this adjustment
         * here, so the user sees the actual number at the record level.
         */
        registers_->op_mode_reg = (registers_->op_mode_reg & ~0xF0) | SIS3820_LNE_SOURCE_CONTROL_SIGNAL;
        registers_->lne_prescale_factor_reg = prescale - 1;
      } 
      else {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                  "%s:%s: unsupported channel advance source %d\n", 
                  driverName, functionName, channelAdvanceSource);
      }
      break;
      
    case ACQUIRE_MODE_SCALER:
      /* Clear the preset register from MCS mode */
      registers_->acq_preset_reg = 0;
      
      /* Set the LNE channel */
      registers_->lne_channel_select_reg = 0;

      /* Disable channel in scaler mode. */
      registers_->count_disable_reg = 0xFFFFFFFF << maxSignals_;

      break;
  }

  callParamCallbacks();
}

void drvSIS3820::setOpModeReg()
{
  int inputMode;
  int outputMode;
  int outputPolarity;
  int maxOutputMode;
  epicsUInt32 operationRegister = 0;

  getIntegerParam(SIS38XXInputMode_, &inputMode);
  getIntegerParam(SIS38XXOutputMode_, &outputMode);
  getIntegerParam(SIS38XXOutputPolarity_, &outputPolarity);

  // We hard-code the data format for now, but support for other formats may be added in the future
  operationRegister   |= SIS3820_MCS_DATA_FORMAT_32BIT;
  operationRegister   |= SIS3820_SCALER_DATA_FORMAT_32BIT;

  /* Check that input mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register. */
  if (inputMode < 0 || inputMode > 5) inputMode = 0;
  operationRegister |= (inputMode << SIS3820_INPUT_MODE_SHIFT);

  /* Check that output mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register. */
  if (firmwareVersion_ >= 0x010A) maxOutputMode = 3;
  else maxOutputMode = 2;
  if (outputMode < 0 || outputMode > maxOutputMode) outputMode = 0;
  operationRegister |= (outputMode << SIS3820_OUTPUT_MODE_SHIFT);
  if (outputPolarity == OUTPUT_POLARITY_INVERTED) operationRegister |= SIS3820_CONTROL_OUTPUTS_INVERT;

  if (acquireMode_ == ACQUIRE_MODE_MCS) {
    operationRegister |= SIS3820_CLEARING_MODE;
    operationRegister |= SIS3820_ARM_ENABLE_CONTROL_SIGNAL;
    operationRegister |= SIS3820_FIFO_MODE;
    operationRegister |= SIS3820_OP_MODE_MULTI_CHANNEL_SCALER;
  } else {
    operationRegister |= SIS3820_NON_CLEARING_MODE;
    operationRegister |= SIS3820_LNE_SOURCE_VME;
    operationRegister |= SIS3820_ARM_ENABLE_CONTROL_SIGNAL;
    operationRegister |= SIS3820_FIFO_MODE;
    operationRegister |= SIS3820_HISCAL_START_SOURCE_VME;
    operationRegister |= SIS3820_OP_MODE_SCALER;
  }
  registers_->op_mode_reg = operationRegister;
}



void drvSIS3820::softwareChannelAdvance()
{
  epicsUInt32 regValue;

  // It appears to be necessary to set the LNE source to VME for this to work
  // Save the current value, clear the bits to set it to VME, restore
  regValue = registers_->op_mode_reg;
  registers_->op_mode_reg = regValue & ~0xF0;
  registers_->key_lne_pulse_reg = 1;
  registers_->op_mode_reg = regValue;
}

void drvSIS3820::setInputMode()
{
  setOpModeReg();
}

void drvSIS3820::setOutputMode()
{
  setOpModeReg();
}


void drvSIS3820::setLED()
{
  int value;
  
  getIntegerParam(SIS38XXLED_, &value);
  if (value == 0)
    registers_->control_status_reg = CTRL_USER_LED_OFF;
  else
    registers_->control_status_reg = CTRL_USER_LED_ON;
}

int drvSIS3820::getLED()
{
  int value;
  
  value = registers_->control_status_reg & CTRL_USER_LED_OFF;
  return (value == 0) ? 0:1;
}

void drvSIS3820::setMuxOut()
{
  int value;
  int outputMode;
  static const char *functionName="setMuxOut";
  
  getIntegerParam(SIS38XXMuxOut_, &value);
  getIntegerParam(SIS38XXOutputMode_, &outputMode);
  if (outputMode != 3) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: scalerMuxOutCommand: output-mode %d does not support MUX_OUT.\n", 
              driverName, functionName, outputMode);
     return;
  }
  if (value < 1 || value > 32) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: scalerMuxOutCommand: channel %d is not supported on the board.\n", 
              driverName, functionName, value);
    return;
  }
  if (firmwareVersion_ < 0x010A) {
    asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
              "%s:%s: scalerMuxOutCommand: MUX_OUT is not supported in firmware version 0x%4.4X\n", 
              driverName, functionName, firmwareVersion_);
    return;
  }
  registers_->mux_out_channel_select_reg = value - 1;
  asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
            "%s:%s: scalerMuxOutCommand %d\n", 
            driverName, functionName, value);
}

int drvSIS3820::getMuxOut()
{
  return registers_->mux_out_channel_select_reg + 1;
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

  /* This register is set up the same for ACQUIRE_MODE_MCS and ACQUIRE_MODE_SCALER) */

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


/**********************/
/* Interrupt handling */
/**********************/

void drvSIS3820::enableInterrupts()
{
  registers_->irq_config_reg |= SIS3820_IRQ_ENABLE;
}

void drvSIS3820::disableInterrupts()
{
  registers_->irq_config_reg &= ~SIS3820_IRQ_ENABLE;
}


static void intFuncC(void *drvPvt)
{
  drvSIS3820 *pSIS3820 = (drvSIS3820*)drvPvt;
  pSIS3820->intFunc();
}
  

void drvSIS3820::intFunc()
{
  /* Disable interrupts */
  disableInterrupts();

  /* Test which interrupt source has triggered this interrupt. */
  irqStatusReg_ = registers_->irq_control_status_reg;

  /* Check for the FIFO threshold interrupt */
  if (irqStatusReg_ & SIS3820_IRQ_SOURCE1_FLAG)
  {
    /* Note that this is a level-sensitive interrupt, not edge sensitive, so it can't be cleared */
    /* Disable this interrupt, since it is caused by FIFO threshold, and that
     * condition is only cleared in the readFIFO routine */
    registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE1_DISABLE;
    eventType_ = EventISR1;
  }

  /* Check for the data acquisition complete interrupt */
  else if (irqStatusReg_ & SIS3820_IRQ_SOURCE2_FLAG)
  {
    /* Reset the interrupt source */
    registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE2_CLEAR;
    // We only set acquiring_ false in scaler mode, in MCS mode we let checkMCSDone() handle this
    // otherwise we can stop reading the FIFO too soon
    if (acquireMode_ == ACQUIRE_MODE_SCALER) acquiring_ = false;
    eventType_ = EventISR2;
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
    eventType_ = EventISR4;
  }

  /* Send an event to readFIFOThread task to read the FIFO and perform any requested callbacks */
  epicsEventSignal(readFIFOEventId_);
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
  int status;
  int count;
  int signal;
  int chan;
  int i;
  bool acquiring;
  epicsUInt32 *pIn, *pOut;
  epicsTimeStamp t1, t2, t3;
  static const char* functionName="readFIFOThread";

  while(true)
  {
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s:%s: waiting for readFIFOEvent\n",
              driverName, functionName);
    enableInterrupts();
    epicsEventWait(readFIFOEventId_);
    asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
              "%s:%s: got readFIFOEvent, eventType=%d\n",
              driverName, functionName, eventType_);
    // We got an event.  This can come from:
    //  Acquisition complete in scaler mode
    //  FIFO full in MCS mode
    //  Acquisition start in MCS mode
    // For scaler mode we just do callbacks
    lock();
    // Do callbacks on acquiring status when acquisition completes
    if ((acquiring_ == false) && (acquireMode_ == ACQUIRE_MODE_SCALER)) {
      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: doing callbacks on scalerDone\n",
                driverName, functionName);
      setIntegerParam(scalerDone_, 1);
      callParamCallbacks();
      unlock();
      continue;
    }
    acquiring = acquiring_;
    unlock();
    // MCS mode
    while (acquiring && (acquireMode_ == ACQUIRE_MODE_MCS)) {
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
                  "%s:%s: doing DMA transfer, fifoBuffer=%p, fifoBaseVME_=%p, count=%d\n",
                  driverName, functionName, fifoBuffer_, fifoBaseVME_, count);
        status = sysDmaFromVme(dmaId_, fifoBuffer_, (int)fifoBaseVME_, VME_AM_EXT_SUP_D64BLT, (count)*sizeof(int), 8);
        if (status) {
          asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                    "%s:%s: doing DMA transfer, error calling sysDmaFromVme, status=%d, error=%d, buff=%p, fifoBaseVME_=%p, count=%d\n",
                    driverName, functionName, status, errno, fifoBuffer_, fifoBaseVME_, count);
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
                  "%s:%s: programmed transfer, count=%d\n",
                  driverName, functionName, count);
        // Note: we were previously using memcpy here.  But that is not guaranteed to do a word transfer, which the
        // SIS3820 requires for reading the FIFO.  In fact if the word count was 1 then a memcpy of 4 bytes was clearly
        // not doing a word transfer on vxWorks, and was generating bus errors.
        for (i=0; i<count; i++)
          fifoBuffer_[i] = fifoBaseCPU_[i];
      }
      epicsTimeGetCurrent(&t2);

      asynPrint(pasynUserSelf, ASYN_TRACE_FLOW, 
                "%s:%s: read FIFO (%d) in %fs, fifo word count after=%d, fifoBuffer_=%p, fifoBaseCPU_=%p\n",
                driverName, functionName, count, epicsTimeDiffInSeconds(&t2, &t1), registers_->fifo_word_count_reg, fifoBuffer_, fifoBaseCPU_);
      // Release the FIFO lock, we are done accessing the FIFO
      epicsMutexUnlock(fifoLockId_);
      
      // Copy the data from the FIFO buffer to the mcsBuffer
      pOut = mcsData_ + signal*maxChans_ + chan;
      pIn = fifoBuffer_;
      for (i=0; i<count; i++) {
        *pOut = *pIn++;
        signal++;
        if (signal == maxSignals_) {
          signal = 0;
          chan++;
          pOut = mcsData_ + chan;
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

      checkMCSDone();
      acquiring = acquiring_;
      /* Re-enable FIFO threshold and FIFO almost full interrupts */
      /* NOTE: WE ARE NOT USING FIFO THRESHOLD INTERRUPTS FOR NOW */
      /* registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE1_ENABLE; */
      registers_->irq_control_status_reg = SIS3820_IRQ_SOURCE4_ENABLE;

      // Release the lock 
      unlock();
      enableInterrupts();
      // If we are still acquiring sleep for a short time but wake up on interrupt
      if (acquiring) {
        status = epicsEventWaitWithTimeout(readFIFOEventId_, epicsThreadSleepQuantum());
        if (status == epicsEventWaitOK) 
          asynPrint(pasynUserSelf, ASYN_TRACE_FLOW,
                    "%s:%s: got interrupt in epicsEventWaitWithTimeout, eventType=%d\n",
                    driverName, functionName, eventType_);
      }
    }
  }
}

extern "C" {
int drvSIS3820Config(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                     int maxChans, int maxSignals, int useDma, int fifoBufferWords)
{
  drvSIS3820 *pSIS3820 = new drvSIS3820(portName, baseAddress, interruptVector, interruptLevel,
                                        maxChans, maxSignals, useDma, fifoBufferWords);
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
static const iocshArg drvSIS3820ConfigArg6 = { "Use DMA",          iocshArgInt};
static const iocshArg drvSIS3820ConfigArg7 = { "FIFO buffer words", iocshArgInt};

static const iocshArg * const drvSIS3820ConfigArgs[] = 
{ &drvSIS3820ConfigArg0,
  &drvSIS3820ConfigArg1,
  &drvSIS3820ConfigArg2,
  &drvSIS3820ConfigArg3,
  &drvSIS3820ConfigArg4,
  &drvSIS3820ConfigArg5,
  &drvSIS3820ConfigArg6,
  &drvSIS3820ConfigArg7
};

static const iocshFuncDef drvSIS3820ConfigFuncDef = 
  {"drvSIS3820Config",8,drvSIS3820ConfigArgs};

static void drvSIS3820ConfigCallFunc(const iocshArgBuf *args)
{
  drvSIS3820Config(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
                   args[4].ival, args[5].ival, args[6].ival, args[7].ival);
}

void drvSIS3820Register(void)
{
  iocshRegister(&drvSIS3820ConfigFuncDef,drvSIS3820ConfigCallFunc);
}

epicsExportRegistrar(drvSIS3820Register);

} // extern "C"

