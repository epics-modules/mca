/* File: 	  drvMcaSIS3820Asyn.c
 * Author:  Wayne Lewis
 * Date:    04-sep-2006
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for the SIS3820 multichannel scaler.
 *
 * Acknowledgements:
 * This driver module is based in part on drvMcaAIMAsyn.c by Mark Rivers.
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
 * - Fix up real time / elapsed time handling
 * - Resolve timing error where last data is not always read by MCA record
 * - Test external channel advance
 * - Resolve issue where number of sweeps are not initially downloaded to the
 * SIS3820
 * - Set up preset real time function
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

#include <asynDriver.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynInt32Array.h>
#include <asynDrvUser.h>
#include <cantProceed.h>
#include <devLib.h>
#include <epicsMessageQueue.h>
#include <epicsMutex.h>
#include <epicsString.h>
#include <epicsThread.h>
#include <epicsTime.h>
#include <epicsExport.h>
#include <errlog.h>
#include <iocsh.h>

/*******************/
/* Custom includes */
/*******************/

#include "mca.h"
#include "drvMca.h"
#include "drvMcaSIS3820Asyn.h"
#include "sis3820.h"

/***************/
/* Definitions */
/***************/

/* Define macros to interlock access to card, including interrupts */
# define INTERLOCK_ON \
{epicsMutexLock(pPvt->lock); \
    pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE1_DISABLE; \
    pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE2_DISABLE; \
    pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE4_DISABLE;}

# define INTERLOCK_OFF \
{pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE1_ENABLE; \
 pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE2_ENABLE; \
 pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE4_ENABLE; \
    epicsMutexUnlock(pPvt->lock);}


/**************/
/* Structures */
/**************/

/*******************************/
/* Global variable declaration */
/*******************************/

epicsUInt32 *mcaSIS3820BaseAddress;
int mcaSIS3820IntVector;
int mcaSIS3820IntLevel;

/**************/
/* Prototypes */
/**************/
static epicsUInt32 setControlStatusReg();
static epicsUInt32 setOpModeReg(int inputMode, int outputMode);
static epicsUInt32 setIrqControlStatusReg();

static void intFunc(void *drvPvt);
static void intTask(mcaSIS3820Pvt *pPvt);

static int readFIFO(mcaSIS3820Pvt *pPvt, asynUser *pasynUser);

static int mcaSIS3820Erase(mcaSIS3820Pvt *pPvt);

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

static void SIS3820Report               (void *drvPvt, FILE *fp, int details);
static asynStatus SIS3820Connect        (void *drvPvt, asynUser *pasynUser);
static asynStatus SIS3820Disconnect     (void *drvPvt, asynUser *pasynUser);

/* Private methods */
static asynStatus SIS3820Write(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus SIS3820Read(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue);

/*
 * asynCommon methods
 */
static struct asynCommon mcaSIS3820Common = {
    SIS3820Report,
    SIS3820Connect,
    SIS3820Disconnect
};

/* asynInt32 methods */
static asynInt32 mcaSIS3820Int32 = {
    int32Write,
    int32Read,
    getBounds,
    NULL,
    NULL
};

/* asynFloat64 methods */
static asynFloat64 mcaSIS3820Float64 = {
    float64Write,
    float64Read,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array mcaSIS3820Int32Array = {
    int32ArrayWrite,
    int32ArrayRead,
    NULL,
    NULL
};

static asynDrvUser mcaSIS3820DrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};

/********/
/* Code */
/********/

/* iocsh commands */

/* This command sets up all the SIS3820s in the crate */
int mcaSIS3820Setup(int baseAddress, int intVector, int intLevel)
{
  mcaSIS3820BaseAddress = (epicsUInt32 *) baseAddress;
  mcaSIS3820IntVector = intVector;
  mcaSIS3820IntLevel = intLevel;
  return (0);
}

/* This command needs to be called once for each SIS3820 in the crate */
int mcaSIS3820Config(
    char *portName,
    int card,
    int maxChans,
    int maxSignals,
    int inputMode,
    int outputMode,
    int lnePrescale)
{
  mcaSIS3820Pvt *pPvt;
  int status;
  int i;
  epicsUInt32 controlStatusReg;
  epicsUInt32 irqControlStatusReg;
  epicsUInt32 opModeReg;
  epicsUInt32 channelDisableRegister = SIS3820_CHANNEL_DISABLE_MASK;

  /* Allocate and initialize mcaSIS3820Pvt if not done yet */

  pPvt = callocMustSucceed(1, sizeof(mcaSIS3820Pvt), "mcaSIS3820Config");

  /* Create asynUser for debugging */
  pPvt->pasynUser = pasynManager->createAsynUser(0, 0);

  pPvt->vmeBaseAddress = mcaSIS3820BaseAddress;

  /* Call devLib to get the system address that corresponds to the VME
   * base address of the board.
   */
  status = devRegisterAddress("drvSIS3820",
      SIS3820_ADDRESS_TYPE,
      (size_t)pPvt->vmeBaseAddress,
      SIS3820_BOARD_SIZE,
      (volatile void **)&pPvt->address);

  if (!RTN_SUCCESS(status))
  {
    errPrintf(status, __FILE__, __LINE__,
        "Can't register address for SIS3820 card\n");
    return (ERROR);
  }

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: local address=%p\n", pPvt->address);

  /* Call devLib to get the system address that corresponds to the VME
   * FIFO address of the board.
   */
  status = devRegisterAddress("drvSIS3820",
      SIS3820_ADDRESS_TYPE,
      (size_t)(pPvt->vmeBaseAddress + SIS3820_FIFO_BASE/4),
      SIS3820_BOARD_SIZE,
      (volatile void **)&pPvt->fifo_base);

  if (!RTN_SUCCESS(status))
  {
    errPrintf(status, __FILE__, __LINE__,
        "Can't register FIFO address for SIS3820 card\n");
    return (ERROR);
  }

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: FIFO address=%p\n", pPvt->fifo_base);

  /* Probe VME bus to see if card is there */
  if (devReadProbe(4, (char *) pPvt->address->control_status_reg,
        (char *) &controlStatusReg) == OK)
    pPvt->exists = 1;

  if (pPvt->exists == 0)
    return (ERROR);

  pPvt->portName = portName;

  /* Get the module info from the card */
  pPvt->moduleID = (pPvt->address->moduleID_reg & 0xFFFF0000) >> 16;
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: module ID=%x\n", pPvt->moduleID);
  pPvt->firmwareVersion = (pPvt->address->moduleID_reg & 0x0000FFFF);
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: firmwareVersion=%d\n",
      pPvt->firmwareVersion);

  pPvt->maxChans = maxChans;
  pPvt->nchans = maxChans;

  /* Preset the number of sweeps to the maximum channels */
  pPvt->numSweeps = pPvt->nchans;
  pPvt->address->acq_preset_reg = pPvt->numSweeps;

  pPvt->maxSignals = maxSignals;
  pPvt->acquiring = 0;
  epicsTimeGetCurrent(&pPvt->startTime);
  pPvt->nextChan = 0;
  pPvt->nextSignal = 0;
  
  /* Assume initially that the LNE is internal */
  pPvt->lneSource = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
  pPvt->softAdvance = 1;
  pPvt->maxStatusTime = 2.0;

  /* Trigger an interrupt when 64 FIFO registers have been filled */
  pPvt->address->fifo_word_threshold_reg = 64;
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: FIFO threshold = %d\n",
      pPvt->address->fifo_word_threshold_reg);

  /* If no prescale is set, use a 1 second channel advance to avoid flooding the
   * memory
   */
  if (lnePrescale == 0)
    lnePrescale = 10000000;
  pPvt->lnePrescale = lnePrescale;

  /* Route the reference pulser into channel 1 */
  pPvt->ch1RefEnable = 1;

  /* Allocate sufficient memory space to hold all of the data collected from the
   * SIS3820.
   */
  pPvt->buffer = (epicsUInt32 *)malloc(maxSignals*maxChans*sizeof(epicsUInt32));

  /* Initialise the buffer pointer to the start of the buffer area */
  pPvt->buffPtr = pPvt->buffer;

  /* Create semaphore to interlock access to card */
  pPvt->lock = epicsMutexCreate();

  /* Create semaphore to interlock access to FIFO */
  pPvt->fifoLock = epicsMutexCreate();

  /* Reset card */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: resetting card %d\n", card);
  pPvt->address->key_reset_reg = 1;

  /* Clear FIFO */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: clearing FIFO\n");
  pPvt->address->key_fifo_reset_reg= 1;

  /* Initialize board and set the control status register */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: initialising control status register\n");
  controlStatusReg = setControlStatusReg();
  pPvt->address->control_status_reg = controlStatusReg;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: control status register = 0x%08x\n",
      pPvt->address->control_status_reg);

  /* Set the interrupt control register */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: setting interrupt control register\n");

  irqControlStatusReg = setIrqControlStatusReg();
  pPvt->address->irq_control_status_reg = irqControlStatusReg;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: interrupt control register = 0x%08x\n",
      pPvt->address->irq_control_status_reg);

  /* Set the operation mode of the scaler */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: setting operation mode\n");

  opModeReg = setOpModeReg(inputMode, outputMode);
  pPvt->address->op_mode_reg = opModeReg;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: operation mode register = 0x%08x\n",
      pPvt->address->op_mode_reg);

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: control status register = 0x%08x\n",
      pPvt->address->control_status_reg);

  /* Set the LNE channel */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: setting LNE signal generation to channel %d\n",
      SIS3820_LNE_CHANNEL);

  pPvt->address->lne_channel_select_reg = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: set LNE signal generation to channel %d\n",
      pPvt->address->lne_channel_select_reg);


  /* 
   * Set number of readout channels to maxSignals
   * Assumes that the lower channels will be used first, and the only unused
   * channels will be at the upper end of the channel range.
   * Create a mask with zeros in the rightmost maxSignals bits, 
   * 1 in all higher order bits.
   */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: setting readout channels=%d\n",maxSignals);

  for (i = 0; i < maxSignals; i++)
  {
    channelDisableRegister <<= 1;
  }

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: setting readout disable register=0x%08x\n",
      channelDisableRegister);

  /* Disable channel in MCS mode. */
  pPvt->address->copy_disable_reg = channelDisableRegister;
  /* Disable channel in scaler mode. */
  pPvt->address->count_disable_reg = channelDisableRegister;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: setting copy disable register=0x%08x\n",
      pPvt->address->copy_disable_reg);

  /* Set the prescale factor to the desired value. */
  pPvt->address->lne_prescale_factor_reg = lnePrescale - 1;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: lne prescale register=%d\n",
      pPvt->address->lne_prescale_factor_reg);

  /* Enable or disable 50 MHz channel 1 reference pulses. */
  if (pPvt->ch1RefEnable)
    pPvt->address->control_status_reg |= CTRL_REFERENCE_CH1_ENABLE;
  else
    pPvt->address->control_status_reg |= CTRL_REFERENCE_CH1_DISABLE;

  /* The interrupt service routine can do floating point, need to save
   * fpContext */
#ifdef vxWorks
  if (fppProbe()==OK)
    pPvt->pFpContext = (FP_CONTEXT *)calloc(1, sizeof(FP_CONTEXT));
  else
    pPvt->pFpContext = NULL;
#endif

  /* Set up the interrupt service routine */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: interruptServiceRoutine pointer %p\n",
      intFunc);

  status = devConnectInterruptVME(mcaSIS3820IntVector,
      (void *)intFunc, (int) pPvt);
  if (!RTN_SUCCESS(status)) 
  {
    errPrintf(status, __FILE__, __LINE__, "Can't connect to vector % d\n", 
        mcaSIS3820IntVector + card);
    return (ERROR);
  }

  /* Enable interrupt level in EPICS */
  status = devEnableInterruptLevel(intVME, mcaSIS3820IntLevel);
  if (!RTN_SUCCESS(status)) {
    errPrintf(status, __FILE__, __LINE__,
        "Can't enable enterrupt level %d\n", mcaSIS3820IntLevel);
    return (ERROR);
  }

  /* Write interrupt level to hardware */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq before setting IntLevel= 0x%x\n", 
      pPvt->address->irq_config_reg);

  pPvt->address->irq_config_reg &= ~SIS3820_IRQ_LEVEL_MASK;
  pPvt->address->irq_config_reg |= (mcaSIS3820IntLevel << 8);

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: IntLevel mask= 0x%x\n", (mcaSIS3820IntLevel << 8));
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq after setting IntLevel= 0x%x\n", 
      pPvt->address->irq_config_reg);

  /* Write interrupt vector to hardware */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq before setting IntLevel= 0x%x\n", 
      pPvt->address->irq_config_reg);

  pPvt->address->irq_config_reg &= ~SIS3820_IRQ_VECTOR_MASK;
  pPvt->address->irq_config_reg |= (mcaSIS3820IntVector + card);

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq = 0x%08x\n", pPvt->address->irq_config_reg);

  /* Enable interrupts in hardware */
  
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq before enabling interrupts= 0x%08x\n", 
      pPvt->address->irq_config_reg);

  pPvt->address->irq_config_reg |= SIS3820_IRQ_ENABLE;

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq config register after enabling interrupts= 0x%08x\n", 
      pPvt->address->irq_config_reg);

  /* Create the interrupt handling task in its own thread */
  if (epicsThreadCreate("mcaSIS3820intTask",
        epicsThreadPriorityHigh,
        epicsThreadGetStackSize(epicsThreadStackMedium),
        (EPICSTHREADFUNC)intTask,
        pPvt) == NULL)
    errlogPrintf("mcaSIS3820intTask epicsThreadCreate failure\n");
  else
    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
        "mcaSIS3820Config: mcaSIS3820intTask epicsThreadCreate success\n");

  /* Create the message queue required by the interrupt task */
  pPvt->intMsgQId = epicsMessageQueueCreate(MAX_MESSAGES,
      SIS3820_MAX_SIGNALS * sizeof(int));

  mcaSIS3820Erase(pPvt);

  /* Link with higher level routines */
  pPvt->common.interfaceType = asynCommonType;
  pPvt->common.pinterface  = (void *)&mcaSIS3820Common;
  pPvt->common.drvPvt = pPvt;
  pPvt->int32.interfaceType = asynInt32Type;
  pPvt->int32.pinterface  = (void *)&mcaSIS3820Int32;
  pPvt->int32.drvPvt = pPvt;
  pPvt->float64.interfaceType = asynFloat64Type;
  pPvt->float64.pinterface  = (void *)&mcaSIS3820Float64;
  pPvt->float64.drvPvt = pPvt;
  pPvt->int32Array.interfaceType = asynInt32ArrayType;
  pPvt->int32Array.pinterface  = (void *)&mcaSIS3820Int32Array;
  pPvt->int32Array.drvPvt = pPvt;
  pPvt->drvUser.interfaceType = asynDrvUserType;
  pPvt->drvUser.pinterface = (void *)&mcaSIS3820DrvUser;
  pPvt->drvUser.drvPvt = pPvt;
  
  status = pasynManager->registerPort(pPvt->portName,
      ASYN_MULTIDEVICE,
      1, /* autoconnect */
      0, /* medium priority */
      0); /* default stacksize */
  if (status != asynSuccess)
  {
    errlogPrintf("mcaSIS3820Config ERROR: Can't register myself.\n");
    return -1;
  }

  status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config : Can't register common.\n");
    return -1;
  }

  status = pasynInt32Base->initialize(pPvt->portName, &pPvt->int32);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config: Can't register int32.\n");
    return -1;
  }

  status = pasynFloat64Base->initialize(pPvt->portName, &pPvt->float64);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config: Can't register float64.\n");
    return -1;
  }

  status = pasynInt32ArrayBase->initialize(pPvt->portName, &pPvt->int32Array);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config: Can't register int32Array.\n");
    return -1;
  }

  pasynManager->registerInterruptSource(portName, &pPvt->int32,
      &pPvt->int32InterruptPvt);

  pasynManager->registerInterruptSource(portName, &pPvt->float64,
      &pPvt->float64InterruptPvt);

  pasynManager->registerInterruptSource(portName, &pPvt->int32Array,
      &pPvt->int32ArrayInterruptPvt);

  status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config ERROR: Can't register drvUser\n");
    return -1;
  }

  /* Connect to device */
  status = pasynManager->connectDevice(pPvt->pasynUser, portName, 0);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config, connectDevice failed for mcaSIS3820\n");
    return -1;
  }
  return (OK);
}

/* Routines to write to the SIS3820 */

static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
    epicsInt32 value)
{
  return(SIS3820Write(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
    epicsFloat64 value)
{
  return(SIS3820Write(drvPvt, pasynUser, 0, value));
}

static asynStatus SIS3820Write(void *drvPvt, asynUser *pasynUser,
    epicsInt32 ivalue, epicsFloat64 dvalue)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;
  mcaCommand command=pasynUser->reason;
  int signal;
  epicsTimeStamp now;
  epicsUInt32 operationModeRegister;

  pasynManager->getAddr(pasynUser, &signal);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820AsynDriver::SIS3820Write entry, command=%d, signal=%d, "
      "ivalue=%d, dvalue=%f\n", command, signal, ivalue, dvalue);
  switch (command) 
  {
    case mcaStartAcquire:
      /* Start acquisition. */
      /* If the MCS is set to use internal channel advance, just start
       * collection. If it is set to use an external channel advance, arm
       * the scaler so that the next LNE pulse starts collection
       */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write lneSource = %d\n",
          pPvt->lneSource);
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write SIS3820_LNE_SOURCE_INTERNAL_10MHZ = %d\n",
          SIS3820_LNE_SOURCE_INTERNAL_10MHZ);

      pPvt->acquiring = 1;

      if (pPvt->lneSource == SIS3820_LNE_SOURCE_INTERNAL_10MHZ)
      {
        pPvt->address->key_op_enable_reg = 1;
        
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "mcaSIS3820AsynDriver::SIS3820Write control status register = 0x%08x\n",
            pPvt->address->control_status_reg);
      }
      else
      {
        pPvt->address->key_op_arm_reg = 1;
      }
      break;

    case mcaStopAcquire:
      /* stop data acquisition */
      {
        pPvt->address->key_op_disable_reg = 1;
        pPvt->acquiring = 0;
      }
      break;

    case mcaErase:
      /* Erase. If this is signal 0 then erase memory for all signals.
       * Databases take advantage of this for performance with
       * multielement detectors with multiplexors */
      /* For the SIS3820, just do the complete erase. 
       * The alternative would be to erase sections of the buffer, but this
       * could get messy, so will keep it simple at the moment.
       */

      /* If SIS3820 is acquiring, turn if off */
      if (pPvt->acquiring)
        pPvt->address->key_op_disable_reg = 1;

      pPvt->address->key_reset_reg;
      pPvt->address->key_fifo_reset_reg;

      asynPrint(pasynUser, ASYN_TRACE_FLOW,
          "(mcaSIS3820AsynDriver::command [%s signal=%d]):"
          " erased\n",
          pPvt->portName, signal);

      /* Reset times */
      pPvt->elapsedTime = 0.;
      epicsTimeGetCurrent(&pPvt->startTime);

      /* Erase the buffer in the private data structure */
      mcaSIS3820Erase(pPvt);

      /* If SIS3820 was acquiring, turn it back on */
      /* If the MCS is set to use internal channel advance, just start
       * collection. If it is set to use an external channel advance, arm
       * the scaler so that the next LNE pulse starts collection
       */
      if (pPvt->acquiring)
      {
        if (pPvt->lneSource == SIS3820_LNE_SOURCE_INTERNAL_10MHZ)
          pPvt->address->key_op_enable_reg = 1;
        else
          pPvt->address->key_op_arm_reg = 1;
      }
      break;

    case mcaReadStatus:
      /* There's not much status to be read from the SIS3820. For the moment,
       * I'll just ignore this one.
       */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write mcaReadStatus\n");

      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write control status register = 0x%08x\n", 
          pPvt->address->control_status_reg);
      
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write operation mode register = 0x%08x\n", 
          pPvt->address->op_mode_reg);
      
      epicsTimeGetCurrent(&now);
      /*
      if ((signal == 0) || 
          (epicsTimeDiffInSeconds(&now, &pPvt->statusTime)
           > pPvt->maxStatusTime)) 
      {
      */
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "mcaSIS3820AsynDriver::SIS3820Write updating status\n");

        pPvt->elive = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
        pPvt->ereal = epicsTimeDiffInSeconds(&now, &pPvt->startTime);

        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "mcaSIS3820AsynDriver::SIS3820Write live time = %f\n", pPvt->elive);

        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "mcaSIS3820AsynDriver::SIS3820Write real time = %f\n", pPvt->ereal);

        /*
           status = nmc_acqu_statusupdate(pPvt->module, pPvt->adc, 0, 0, 0,
           &pPvt->elive, &pPvt->ereal, 
           &pPvt->etotals, &pPvt->acquiring);
         asynPrint(pasynUser, ASYN_TRACE_FLOW,
         "(mcaSIS3820AsynDriver [%s signal=%d]): get_acq_status=%d\n",
         pPvt->portName, signal, status);
       */
         epicsTimeGetCurrent(&pPvt->statusTime);
         /*
      }
      */
      break;
    case mcaChannelAdvanceInternal:
      /* set channel advance source to internal (timed) */
      
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write(mcaChannelAdvanceInternal) op mode register = 0x%08x\n", 
          pPvt->address->op_mode_reg);

      operationModeRegister = pPvt->address->op_mode_reg;
      operationModeRegister &= ~SIS3820_OP_MODE_REG_LNE_MASK;
      operationModeRegister |= SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
      pPvt->address->op_mode_reg = operationModeRegister;

      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write(mcaChannelAdvanceInternal) op mode register = 0x%08x\n", 
          pPvt->address->op_mode_reg);

      break;

    case mcaChannelAdvanceExternal:
      /* set channel advance source to external */

      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write(mcaChannelAdvanceExternal) op mode register = 0x%08x\n", 
          pPvt->address->op_mode_reg);
      
      operationModeRegister = pPvt->address->op_mode_reg;
      operationModeRegister &= ~SIS3820_OP_MODE_REG_LNE_MASK;
      operationModeRegister |= SIS3820_LNE_SOURCE_CONTROL_SIGNAL;
      pPvt->address->op_mode_reg = operationModeRegister;

      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Write(mcaChannelAdvanceExternal) op mode register = 0x%08x\n", 
          pPvt->address->op_mode_reg);

      break;

    case mcaNumChannels:
      /* Terminology warning:
       * This is the number of channels that are to be acquired. Channels
       * correspond to time bins or external channel advance triggers, as
       * opposed to the 32 input signals that the SIS3820 supports.
       */
      /* set number of channels */
      pPvt->nchans = ivalue;
      if (pPvt->nchans > pPvt->maxChans) 
      {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
            "mcaSIS3820Server Illegal nuse field");
        pPvt->nchans = pPvt->maxChans;
      }

      /* set number of sweeps (for MCS mode) */
      pPvt->numSweeps = pPvt->nchans;
      pPvt->address->acq_preset_reg = pPvt->numSweeps;

      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaPresetSweeps = %d\n", pPvt->address->acq_preset_reg);

      break;

    case mcaModePHA:
      /* set mode to Pulse Height Analysis */
      /* This is a NOOP for the SIS3820 */
      /* To make sure, we set the mode to MCS anyway */

      pPvt->acqmod = 1;

      operationModeRegister = pPvt->address->op_mode_reg;
      operationModeRegister &= ~SIS3820_OP_MODE_REG_MODE_MASK;
      operationModeRegister |= SIS3820_OP_MODE_MULTI_CHANNEL_SCALER;
      pPvt->address->op_mode_reg = operationModeRegister;

      break;

    case mcaModeMCS:
      /* set mode to MultiChannel Scaler */
      pPvt->acqmod = 1;

      operationModeRegister = pPvt->address->op_mode_reg;
      operationModeRegister &= ~SIS3820_OP_MODE_REG_MODE_MASK;
      operationModeRegister |= SIS3820_OP_MODE_MULTI_CHANNEL_SCALER;
      pPvt->address->op_mode_reg = operationModeRegister;
      break;

    case mcaModeList:
      /* set mode to LIST (record each incoming event) */
      /* This is a NOOP for the SIS3820 */
      break;

    case mcaSequence:
      /* set sequence number */
      /* This is a NOOP for the SIS3820 */
      break;

    case mcaPrescale:
      /* The SIS3820 requires the value in the LNE prescale register to be one
       * less than the actual number of incoming signals. We do this adjustment
       * here, so the user sees the actual number at the record level.
       * Only do this if we are in external LNE mode. Otherwise, this register
       * gets used to create the timer for the dwell time LNE advance.
       */
      if (pPvt->lneSource == SIS3820_LNE_SOURCE_CONTROL_SIGNAL)
      {
        pPvt->lnePrescale = ivalue;
        pPvt->address->lne_prescale_factor_reg = pPvt->lnePrescale - 1;
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "lne prescale = %d\n", pPvt->address->lne_prescale_factor_reg);
      }
      break;

    case mcaPresetSweeps:
      /* set number of sweeps (for MCS mode) */
      pPvt->numSweeps = ivalue;
      pPvt->address->acq_preset_reg = pPvt->numSweeps;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaPresetSweeps = %d\n", pPvt->address->acq_preset_reg);
      break;

    case mcaPresetLowChannel:
      /* set lower side of region integrated for preset counts */
      /* This is a NOOP for the SIS3820 */
      break;

    case mcaPresetHighChannel:
      /* set high side of region integrated for preset counts */
      /* This is a NOOP for the SIS3820 */
      break;

    case mcaDwellTime:
      /* set dwell time */
      if (pPvt->lneSource == SIS3820_LNE_SOURCE_INTERNAL_10MHZ)
      {
        pPvt->dwellTime = dvalue;
        pPvt->address->lne_prescale_factor_reg = 
          (epicsUInt32) (SIS3820_10MHZ_CLOCK * pPvt->dwellTime);
      }
      break;

    case mcaPresetRealTime:
      /* set preset real time. Convert to centiseconds */
      /* This is a NOOP for the SIS3820 */
      break;

    case mcaPresetLiveTime:
      /* set preset live time. Convert to centiseconds */
      /* This is a NOOP for the SIS3820 */
      break;

    case mcaPresetCounts:
      /* set preset counts */
      /* This is a NOOP for the SIS3820 */
      /* Possibly this can be implemented in software somehow, but I don't think
       * the SIS3820 can do a running total of all channels
       */
      break;

    default:
      asynPrint(pasynUser, ASYN_TRACE_ERROR, 
          "mcaSIS3820AsynDriver::command port %s got illegal command %d\n",
          pPvt->portName, command);
      break;
  }
  return(asynSuccess);
}


static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *value)
{
  return(SIS3820Read(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
    epicsFloat64 *value)
{
  return(SIS3820Read(drvPvt, pasynUser, NULL, value));
}

static asynStatus SIS3820Read(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;
  mcaCommand command = pasynUser->reason;
  asynStatus status=asynSuccess;

  switch (command) {
    case mcaAcquiring:
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Read mcaAcquiring\n");
      *pivalue = pPvt->acquiring;
      break;

    case mcaDwellTime:
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Read mcaDwellTime\n");
      *pfvalue = pPvt->dwellTime;
      break;

    case mcaElapsedLiveTime:
      /* The SIS3820 does not support live time recording */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Read mcaElapsedLiveTime\n");
      *pfvalue = pPvt->elive;
      break;

    case mcaElapsedRealTime:
      /* The SIS3820 does not support real time recording */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Read mcaElapsedRealTime\n");
      *pfvalue = pPvt->ereal;
      break;

    case mcaElapsedCounts:
      /* The SIS3820 does not support elapsed count recording */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "mcaSIS3820AsynDriver::SIS3820Read mcaElapsedCounts\n");
      *pfvalue = 0;
      break;

    default:
      asynPrint(pasynUser, ASYN_TRACE_ERROR,
          "drvMcaSIS3820Asyn::SIS3820Read got illegal command %d\n",
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

static asynStatus int32ArrayRead(
    void *drvPvt, 
    asynUser *pasynUser,
    epicsInt32 *data, 
    size_t maxChans, 
    size_t *nactual)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;
  int signal;
  int maxSignals = pPvt->maxSignals;
  int numSamples;
  epicsInt32 *pBuff = data;
  epicsUInt32 *pPvtBuff;

  pasynManager->getAddr(pasynUser, &signal);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820AsynDriver::int32ArrayRead entry, signal=%d, maxChans=%d\n", 
      signal, maxChans);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820AsynDriver::int32ArrayRead fifo word count = %d\n", 
      pPvt->address->fifo_word_count_reg);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820AsynDriver::int32ArrayRead status register = 0x%08x\n", 
      pPvt->address->control_status_reg);

  /* Update the private data buffer by reading the data from the FIFO memory */
  readFIFO(pPvt, pasynUser);

  /* Transfer the data from the private driver structure to the supplied data
   * buffer. The private data structure will have the information for all the
   * signals, so we need to just extract the signal being requested.
   */
  for (pPvtBuff = pPvt->buffer + signal, numSamples = 0; 
      pPvtBuff < pPvt->buffPtr; 
      pPvtBuff += maxSignals, numSamples++)
  {
    *pBuff++ = (epicsInt32) *pPvtBuff;
  }

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "(mcaSIS3820AsynDriver [signal=%d]): read %d chans\n", signal, numSamples);

  *nactual = numSamples;
  return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *data, size_t maxChans)
{
  /* There is no way to write to a SIS3820 FIFO (other than in a special card test mode) 
   * so this function is not implemented in this driver.
   */
  asynPrint(pasynUser, ASYN_TRACE_ERROR,
      "drvMcaSIS3820::SIS3820WriteData, write to SIS3820 not implemented\n");
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
      "drvMcaSIS3820Asyn::drvUserCreate, unknown command=%s\n", drvInfo);
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


/***********************/
/* asynCommon routines */
/***********************/

/* Report  parameters */
static void SIS3820Report(void *drvPvt, FILE *fp, int details)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;

  assert(pPvt);
  fprintf(fp, "SIS3820: asyn port: %s, connected at VME base address %p",
      pPvt->portName, pPvt->address);
  if (details >= 1) {
    fprintf(fp, "              maxChans: %d\n", pPvt->maxChans);
  }
}

/* Connect */
static asynStatus SIS3820Connect(void *drvPvt, asynUser *pasynUser)
{
  /* Does nothing for now.  
   * May be used if connection management is implemented */
  pasynManager->exceptionConnect(pasynUser);
  return(asynSuccess);
}

/* Disconnect */
static asynStatus SIS3820Disconnect(void *drvPvt, asynUser *pasynUser)
{
  /* Does nothing for now.  
   * May be used if connection management is implemented */
  pasynManager->exceptionDisconnect(pasynUser);
  return(asynSuccess);
}

/**********************/
/* Internal functions */
/**********************/

static int mcaSIS3820Erase(mcaSIS3820Pvt *pPvt)
{
  epicsTimeStamp timeStamp;
  int i;

  if (pPvt->exists == 0) return (ERROR);
  INTERLOCK_ON;

  /* Erase buffer in driver */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "drvSIS3820Erase: pPvt=%p\n", pPvt);

  pPvt->buffPtr = pPvt->buffer;
  for (i=0; i < (pPvt->maxSignals * pPvt->nchans); i++) 
    *pPvt->buffPtr++ = 0;

  /* Reset buffer pointer to start of buffer */
  pPvt->buffPtr = pPvt->buffer;
  pPvt->nextChan = 0;
  pPvt->nextSignal = 0;

  /* Erase FIFO and counters on board */
  pPvt->address->key_fifo_reset_reg = 1;
  pPvt->address->key_counter_clear = 1;

  /* Reset the elapsed time */
  pPvt->elapsedTime = 0.0;

  /* Reset the elapsed counts */
  for (i=0; i<pPvt->maxSignals; i++)
    pPvt->elapsedCounts[i] = 0;

  /* Reset the start time.  This is necessary here because we may be
   * acquiring, and AcqOn will not be called. Normally this is set in AcqOn.
   */

  epicsTimeGetCurrent(&timeStamp);
  pPvt->startTime = timeStamp;

  pPvt->elapsedPrevious = 0.;

  INTERLOCK_OFF;
  return (OK);
}

static int readFIFO(mcaSIS3820Pvt *pPvt, asynUser *pasynUser)
{
  /* Read the data stored in the FIFO to the buffer in the private driver data
   * structure.
   */

  epicsUInt32 *pBuff;
  int count = 0;
  int fifoWordCount;

  asynPrint(pasynUser, ASYN_TRACE_FLOW, "drvMcaSIS3820Asyn::readFIFO entering\n");

  /* Current end of buffer stored in the private data structure */
  pBuff = pPvt->buffPtr;

  fifoWordCount = pPvt->address->fifo_word_count_reg;

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::readFIFO fifo word count = %d\n",
      fifoWordCount);

  /* Test that access to the FIFO is allowed */
  if (epicsMutexTryLock(pPvt->fifoLock) == epicsMutexLockOK)
  {
    /* Check if the FIFO actually contains anything. If so, read out the FIFO to
     * the local buffer.
     */
    while (pPvt->address->fifo_word_count_reg != 0)
    {
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::readFIFO fifo word count = %d\n",
          pPvt->address->fifo_word_count_reg);

      *pBuff = *pPvt->fifo_base;

      pBuff++;
      count++;
    }

    epicsMutexUnlock(pPvt->fifoLock);
  }

  /* Save the new pointer to the start of the empty section of the data buffer
   */

  pPvt->buffPtr = pBuff;

  return count;
}

static epicsUInt32 setControlStatusReg()
{
  /* Set up the default behaviour of the card */
  /* Initially, this will have the following:
   * User LED off
   * Counter test modes disabled
   * Reference pulser enabled to Channel 1
   * LNE prescaler active
   */

  epicsUInt32 controlRegister = 0;

  controlRegister += CTRL_USER_LED_OFF;
  controlRegister += CTRL_COUNTER_TEST_25MHZ_DISABLE;
  controlRegister += CTRL_COUNTER_TEST_MODE_DISABLE;
  controlRegister += CTRL_REFERENCE_CH1_ENABLE;

  return (controlRegister);
}

static epicsUInt32 setOpModeReg(int inputMode, int outputMode)
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
   * Arm/enable mode 0
   * LNE sourced from front panel
   * 32 bit data format
   * Clearing mode - want the incremental counts
   */

  epicsUInt32 operationRegister = 0;

  operationRegister += SIS3820_CLEARING_MODE;
  operationRegister += SIS3820_MCS_DATA_FORMAT_32BIT;
  operationRegister += SIS3820_LNE_SOURCE_CONTROL_SIGNAL;
  operationRegister += SIS3820_ARM_ENABLE_CHANNEL_N;
  operationRegister += SIS3820_FIFO_MODE;

  /* Check that input mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */

  if (inputMode < 0 || inputMode > 5)
    inputMode = 0;

  operationRegister += inputMode << SIS3820_INPUT_MODE_SHIFT;

  /* Check that output mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */

  if (outputMode < 0 || outputMode > 2)
    outputMode = 0;

  operationRegister += outputMode << SIS3820_OUTPUT_MODE_SHIFT;

  /* Set the scaler mode to multichannel scaler */

  operationRegister += SIS3820_OP_MODE_MULTI_CHANNEL_SCALER;

  return (operationRegister);
}

static epicsUInt32 setIrqControlStatusReg()
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

  interruptRegister += SIS3820_IRQ_SOURCE2_ENABLE;

  interruptRegister += SIS3820_IRQ_SOURCE0_DISABLE;
  /*interruptRegister += SIS3820_IRQ_SOURCE1_DISABLE;*/
  /*interruptRegister += SIS3820_IRQ_SOURCE2_DISABLE;*/
  interruptRegister += SIS3820_IRQ_SOURCE3_DISABLE;
  /*interruptRegister += SIS3820_IRQ_SOURCE4_DISABLE;*/
  interruptRegister += SIS3820_IRQ_SOURCE5_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE6_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE7_DISABLE;

  interruptRegister += SIS3820_IRQ_SOURCE0_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE1_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE2_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE3_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE4_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE5_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE6_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE7_CLEAR;

  return interruptRegister;
}

/**********************/
/* Interrupt handling */
/**********************/

static void intFunc(void *drvPvt)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;
  epicsUInt32 irqStatusReg;

  /* Disable interrupts */
  pPvt->address->irq_config_reg &= ~SIS3820_IRQ_ENABLE;

  /* Test which interrupt source has triggered this interrupt. */
  irqStatusReg = pPvt->address->irq_control_status_reg;

  /* Check for the FIFO threshold interrupt */
  if (irqStatusReg & SIS3820_IRQ_SOURCE1_FLAG)
  {
    /* Reset the interrupt source */
    pPvt->address->irq_control_status_reg |= SIS3820_IRQ_SOURCE1_CLEAR;

    /* Get the data from the FIFO memory */
    /*
    readFIFO(pPvt, pPvt->pasynUser);
    */
  }
  /* Check for the data acquisition complete interrupt */
  else if (irqStatusReg & SIS3820_IRQ_SOURCE2_FLAG)
  {
    /* Reset the interrupt source */
    pPvt->address->irq_control_status_reg |= SIS3820_IRQ_SOURCE2_CLEAR;

    /* Clear the software acquiring flag */
    pPvt->acquiring = 0;

    /* Get the data from the FIFO memory */
    /*
    readFIFO(pPvt, pPvt->pasynUser);
    */
  }
  /* Check for the FIFO almost full interrupt */
  else if (irqStatusReg & SIS3820_IRQ_SOURCE4_FLAG)
  {
    /* This interrupt represents an error condition of sorts. For the moment, I
     * will terminate data collection, as it is likely that data will have been
     * lost.
     */
    /* Reset the interrupt source */
    pPvt->address->irq_control_status_reg |= SIS3820_IRQ_SOURCE4_CLEAR;

    /* Clear the software acquiring flag */
    pPvt->acquiring = 0;

    /* Get the data from the FIFO memory */
    /*
    readFIFO(pPvt, pPvt->pasynUser);
    */
  }

  /* Send a message to the function to notify all the attached records of the
   * interrupt generation
   */
  if (epicsMessageQueueTrySend(pPvt->intMsgQId, 0, 0) == 0)
    pPvt->messagesSent++;
  else 
    pPvt->messagesFailed++;

  /* Reenable interrupts */
  pPvt->address->irq_config_reg |= SIS3820_IRQ_ENABLE;

}

static void intTask(mcaSIS3820Pvt *pPvt)
{
  int addr, reason;
  ELLLIST *pclientList;
  interruptNode *pNode;

  while(1)
  {
    epicsMessageQueueReceive(pPvt->intMsgQId, 0, 0);

    /* ??? */
    /* Do stuff with the data that's been passed in. It looks like it needs to
     * be stored in the private data structure somewhere. 
     */
    /* I don't actually think I need to do anything with the data, unless it
     * needs to be given the MCA channel data. If so, I'll need separate array
     * structures in the device support to handle each attached MCA record. For
     * the moment, just let the normal record scanning handle getting the data
     * from the device support via the read_array function.
     */

    /* ??? */
    /* Need to fix up the data that's being passed out in the interrupt handling
     */

    /* Read the accumulated data from the FIFO */
    readFIFO(pPvt, pPvt->pasynUser);

    /* Pass int32 interrupts */
    pasynManager->interruptStart(pPvt->int32InterruptPvt, &pclientList);
    pNode = (interruptNode *)ellFirst(pclientList);
    while (pNode) 
    {
      asynInt32Interrupt *pint32Interrupt = pNode->drvPvt;
      addr = pint32Interrupt->addr;
      reason = pint32Interrupt->pasynUser->reason;
      /*
      if (reason == ip330Data)
      {
        pint32Interrupt->callback(pint32Interrupt->userPvt,
            pint32Interrupt->pasynUser,
            pPvt->correctedData[addr]);
      }
      */
      pNode = (interruptNode *)ellNext(&pNode->node);
    }
    pasynManager->interruptEnd(pPvt->int32InterruptPvt);

    /* Pass float64 interrupts */
    pasynManager->interruptStart(pPvt->float64InterruptPvt, &pclientList);
    pNode = (interruptNode *)ellFirst(pclientList);
    while (pNode) 
    {
      asynFloat64Interrupt *pfloat64Interrupt = pNode->drvPvt;
      addr = pfloat64Interrupt->addr;
      reason = pfloat64Interrupt->pasynUser->reason;
      /*
      if (reason == ip330Data)
      {
        pfloat64Interrupt->callback(pfloat64Interrupt->userPvt,
            pfloat64Interrupt->pasynUser,
            (double)pPvt->correctedData[addr]);
      }
      */
      pNode = (interruptNode *)ellNext(&pNode->node);
    }
    pasynManager->interruptEnd(pPvt->float64InterruptPvt);

    /* Pass int32Array interrupts */
    pasynManager->interruptStart(pPvt->int32ArrayInterruptPvt, &pclientList);
    pNode = (interruptNode *)ellFirst(pclientList);
    while (pNode) 
    {
      asynInt32ArrayInterrupt *pint32ArrayInterrupt = pNode->drvPvt;
      reason = pint32ArrayInterrupt->pasynUser->reason;
      /*
      if (reason == ip330Data)
      {
        pint32ArrayInterrupt->callback(pint32ArrayInterrupt->userPvt,
            pint32ArrayInterrupt->pasynUser,
            pPvt->correctedData,
            MAX_IP330_CHANNELS);
      }
      */
      pNode = (interruptNode *)ellNext(&pNode->node);
    }
    pasynManager->interruptEnd(pPvt->int32ArrayInterruptPvt);
  }
}

