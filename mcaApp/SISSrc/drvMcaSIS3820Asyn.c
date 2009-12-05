/* File: 	  drvMcaSIS3820Asyn.c
 * Author:  Wayne Lewis
 * Date:    04-sep-2006
 *
 * Purpose: 
 * This module provides the driver support for the MCA and scaler record asyn device 
 * support layer for the SIS3820 multichannel scaler.
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
 * - Track down problem with FIFO threshold interrupts.  
 *     Works fine with 1024 word threshold down to 100 microsecond dwell time,
 *     below that it messes up, 100% CPU time.
 * - There looks like a problem with BaseAddresss with multiple cards.  Each card needs
 *   it own base address
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
#include <epicsEvent.h>
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
 pPvt->address->irq_config_reg &= ~SIS3820_IRQ_ENABLE;}

# define INTERLOCK_OFF \
{pPvt->address->irq_config_reg |= SIS3820_IRQ_ENABLE; \
 epicsMutexUnlock(pPvt->lock);}



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

/**************/
/* Structures */
/**************/
typedef enum{
    scalerResetCommand=MAX_MCA_COMMANDS + 1,
    scalerChannelsCommand,
    scalerReadCommand,
    scalerPresetCommand,
    scalerArmCommand,
    scalerDoneCommand
} sis3820ScalerCommand;

#define MAX_SIS3820_SCALER_COMMANDS 6

typedef struct {
    sis3820ScalerCommand command;
    char *commandString;
} sis3820ScalerCommandStruct;

static sis3820ScalerCommandStruct sis3820ScalerCommands[MAX_SIS3820_SCALER_COMMANDS] = {
    {scalerResetCommand,        SCALER_RESET_COMMAND_STRING},    /* int32, write */
    {scalerChannelsCommand,     SCALER_CHANNELS_COMMAND_STRING}, /* int32, read */
    {scalerReadCommand,         SCALER_READ_COMMAND_STRING},     /* int32Array, read */
    {scalerPresetCommand,       SCALER_PRESET_COMMAND_STRING},   /* int32, write */
    {scalerArmCommand,          SCALER_ARM_COMMAND_STRING},      /* int32, write */
    {scalerDoneCommand,         SCALER_DONE_COMMAND_STRING},     /* int32, read */
};


/*******************************/
/* Global variable declaration */
/*******************************/

epicsUInt32 *mcaSIS3820BaseAddress;
int mcaSIS3820IntVector;
int mcaSIS3820IntLevel;

/**************/
/* Prototypes */
/**************/
static void setControlStatusReg(mcaSIS3820Pvt *pPvt);
static void setOpModeReg(mcaSIS3820Pvt *pPvt);
static void setIrqControlStatusReg(mcaSIS3820Pvt *pPvt);
static void setAcquireMode(mcaSIS3820Pvt *pPvt, SIS3820AcquireMode acquireMode);
static void clearScalerPresets(mcaSIS3820Pvt *pPvt);
static void setScalerPresets(mcaSIS3820Pvt *pPvt);

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
  epicsUInt32 controlStatusReg;

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
        "Can't register address for SIS3820 card %d\n", card);
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
        "Can't register FIFO address for SIS3820 card %d\n", card);
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

  pPvt->maxSignals = maxSignals;
  pPvt->inputMode = inputMode;
  pPvt->outputMode = outputMode;
  pPvt->acquiring = 0;
  pPvt->prevAcquiring = 0;
  pPvt->nextChan = 0;
  pPvt->nextSignal = 0;
  
  /* Assume initially that the LNE is internal */
  pPvt->lneSource = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
  pPvt->softAdvance = 1;
  pPvt->maxStatusTime = 2.0;

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

  /* Create the EPICS event used to wake up the interrupt task */
  pPvt->intEventId = epicsEventCreate(epicsEventFull);

  /* Reset card */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: resetting card %d\n", card);
  pPvt->address->key_reset_reg = 1;

  /* Clear FIFO */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: clearing FIFO\n");
  pPvt->address->key_fifo_reset_reg= 1;

  /* Initialize board in MCS mode */
  pPvt->acquireMode = UNDEFINED_MODE;
  setAcquireMode(pPvt, MCS_MODE);

  /* Set up the interrupt service routine */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: interruptServiceRoutine pointer %p\n",
      intFunc);

  status = devConnectInterruptVME(mcaSIS3820IntVector,
      (void *)intFunc, pPvt);
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

  /* Connect pasynUser to device for debugging */
  status = pasynManager->connectDevice(pPvt->pasynUser, portName, 0);
  if (status != asynSuccess) 
  {
    errlogPrintf("mcaSIS3820Config, connectDevice failed for mcaSIS3820\n");
    return -1;
  }

  /* Enable interrupts in hardware */
  
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
      "mcaSIS3820Config: irq before enabling interrupts= 0x%08x\n", 
      pPvt->address->irq_config_reg);

  pPvt->address->irq_config_reg |= SIS3820_IRQ_ENABLE;

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
  int command=pasynUser->reason;
  int signal;
  int i;

  INTERLOCK_ON;
  pasynManager->getAddr(pasynUser, &signal);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::SIS3820Write entry, command=%d, signal=%d, "
      "ivalue=%d, dvalue=%f\n", command, signal, ivalue, dvalue);
  switch (command) 
  {
    case mcaStartAcquire:
      /* Start acquisition. */
      /* Note that we don't start acquisition except for channel 0. */
      if (signal != 0) break;

      /* If the MCS is set to use internal channel advance, just start
       * collection. If it is set to use an external channel advance, arm
       * the scaler so that the next LNE pulse starts collection
       */

      pPvt->acquiring = 1;
      pPvt->prevAcquiring = 1;

      setAcquireMode(pPvt, MCS_MODE);
      setOpModeReg(pPvt);

      /* Set the number of channels to acquire */
      pPvt->address->acq_preset_reg = pPvt->nchans;

      if (pPvt->lneSource == SIS3820_LNE_SOURCE_INTERNAL_10MHZ)
      {
        /* The SIS3820 requires the value in the LNE prescale register to be one
         * less than the actual number of incoming signals. We do this adjustment
         * here, so the user sees the actual number at the record level.
         */
        pPvt->address->lne_prescale_factor_reg = 
          (epicsUInt32) (SIS3820_10MHZ_CLOCK * pPvt->dwellTime) - 1;
        pPvt->address->key_op_enable_reg = 1;
        
      }
      else if (pPvt->lneSource == SIS3820_LNE_SOURCE_CONTROL_SIGNAL)
      {
        /* The SIS3820 requires the value in the LNE prescale register to be one
         * less than the actual number of incoming signals. We do this adjustment
         * here, so the user sees the actual number at the record level.
         */
        pPvt->address->lne_prescale_factor_reg = pPvt->lnePrescale - 1;
        pPvt->address->key_op_arm_reg = 1;
      } 
      else
      {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
            "drvMcaSIS3820Asyn::SIS3820Write unsupported lneSource %d\n", pPvt->lneSource);
      }
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write lneSource = %d\n",
          pPvt->lneSource);
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write(mcaStartAcquire) op mode register = 0x%08x\n", 
          pPvt->address->op_mode_reg);
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write control status register = 0x%08x\n",
          pPvt->address->control_status_reg);
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write lne prescale = %d\n", 
          pPvt->address->lne_prescale_factor_reg);
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write acq_preset_req = %d\n", 
          pPvt->address->acq_preset_reg);
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write fifo_word_threshold_reg = %d\n", 
          pPvt->address->fifo_word_threshold_reg);
      break;

    case mcaStopAcquire:
      /* stop data acquisition */
      {
        pPvt->address->key_op_disable_reg = 1;
        readFIFO(pPvt, pPvt->pasynUser);
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

      asynPrint(pasynUser, ASYN_TRACE_FLOW,
          "drvMcaSIS3820Asyn::SIS3820Write [%s signal=%d]:"
          " erased\n",
          pPvt->portName, signal);

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
      /* Read the FIFO.  This is needed to see if acquisition is complete and to update the
       * elapsed times.
       */
      readFIFO(pPvt, pasynUser);
      break;

    case mcaChannelAdvanceInternal:
      /* set channel advance source to internal (timed) */
      /* Just cache this setting here, set it when acquisition starts */
      pPvt->lneSource = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write(mcaChannelAdvanceInternal) pPvt-lneSource = 0x%x\n", 
          pPvt->lneSource);

      break;

    case mcaChannelAdvanceExternal:
      /* set channel advance source to external */
      /* Just cache this setting here, set it when acquisition starts */
      pPvt->lneSource = SIS3820_LNE_SOURCE_CONTROL_SIGNAL;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write(mcaChannelAdvanceExternal) pPvt-lneSource = 0x%x\n", 
          pPvt->lneSource);
      break;

    case mcaNumChannels:
      /* Terminology warning:
       * This is the number of channels that are to be acquired. Channels
       * correspond to time bins or external channel advance triggers, as
       * opposed to the 32 input signals that the SIS3820 supports.
       */
      /* Just cache this value for now, set it when acquisition starts */
      pPvt->nchans = ivalue;
      if (pPvt->nchans > pPvt->maxChans) 
      {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
            "drvMcaSIS3820Asyn::SIS3820Write Illegal nuse field %d", pPvt->nchans);
        pPvt->nchans = pPvt->maxChans;
      }
      break;

    case mcaModePHA:
      /* set mode to Pulse Height Analysis */
      /* This is a NOOP for the SIS3820 */
      /* To make sure, we set the mode to MCS anyway */
      pPvt->acquireMode = MCS_MODE;
      break;

    case mcaModeMCS:
      /* set mode to MultiChannel Scaler */
      pPvt->acquireMode = MCS_MODE;
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
      /* We just cache the value here.  It gets written to the hardware when acquisition
       * starts if we are in external channel advance mode 
       */
      pPvt->lnePrescale = ivalue;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820::SIS3820Write lne prescale = %d\n", pPvt->lnePrescale);
      break;

    case mcaPresetSweeps:
      /* set number of sweeps (for MCS mode) */
      /* This is not implemented yet, but we cache the value anyway */
      pPvt->numSweeps = ivalue;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write mcaPresetSweeps = %d\n", pPvt->numSweeps);
      break;

    case mcaPresetLowChannel:
      /* set lower side of region integrated for preset counts */
      pPvt->presetStartChan[signal] = ivalue;
      break;

    case mcaPresetHighChannel:
      /* set high side of region integrated for preset counts */
      pPvt->presetEndChan[signal] = ivalue;
      break;

    case mcaDwellTime:
      /* set dwell time */
      /* We just cache the value here and set in mcaStartAquire if using internal channel advance */
      pPvt->dwellTime = dvalue;
      break;

    case mcaPresetRealTime:
      /* set preset real time. */
      pPvt->presetReal = dvalue;
      break;

    case mcaPresetLiveTime:
      /* set preset live time */
      pPvt->presetLive = dvalue;
      break;

    case mcaPresetCounts:
      /* set preset counts */
      pPvt->presetCounts[signal] = ivalue;
      break;

    case scalerResetCommand:
      /* Reset scaler */
      pPvt->address->key_op_disable_reg = 1;
      pPvt->address->key_fifo_reset_reg = 1;
      pPvt->address->key_counter_clear = 1;
      pPvt->acquiring = 0;
      /* Clear all of the presets */
      for (i=0; i<pPvt->maxSignals; i++) {
        pPvt->scalerPresets[i] = 0;
      }
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write scalerResetCommand\n");
      break;

    case scalerArmCommand:
      /* Arm or disarm scaler */
      setAcquireMode(pPvt, SCALER_MODE);
      if (ivalue != 0) {
        setScalerPresets(pPvt);
        pPvt->address->key_op_enable_reg = 1;
        pPvt->acquiring = 1;
        pPvt->prevAcquiring = 1;
      } else {
        pPvt->address->key_op_disable_reg = 1;
        pPvt->address->key_fifo_reset_reg = 1;
      }
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write scalerArmCommand=%d\n", ivalue);
      break;

    case scalerPresetCommand:
      /* Set scaler preset */
      pPvt->scalerPresets[signal] = ivalue;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Write scalerPresetCommand channel %d=%d\n", signal, ivalue);
      break;

    default:
      asynPrint(pasynUser, ASYN_TRACE_ERROR, 
          "drvMcaSIS3820Asyn::SIS3820Write port %s got illegal command %d\n",
          pPvt->portName, command);
      break;
  }
  INTERLOCK_OFF;
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
  int command = pasynUser->reason;
  asynStatus status=asynSuccess;
  int signal;

  pasynManager->getAddr(pasynUser, &signal);

  INTERLOCK_ON;
  switch (command) {
    case mcaAcquiring:
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read mcaAcquiring\n");
      *pivalue = pPvt->acquiring;
      break;

    case mcaDwellTime:
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read mcaDwellTime\n");
      *pfvalue = pPvt->dwellTime;
      break;

    case mcaElapsedLiveTime:
      /* The SIS3820 does not support live time recording */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read mcaElapsedLiveTime\n");
      *pfvalue = pPvt->elapsedTime;
      break;

    case mcaElapsedRealTime:
      /* The SIS3820 does not support real time recording */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read mcaElapsedRealTime\n");
      *pfvalue = pPvt->elapsedTime;
      break;

    case mcaElapsedCounts:
      /* The SIS3820 does not support elapsed count recording */
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read mcaElapsedCounts\n");
      *pfvalue = 0;
      break;

    case scalerChannelsCommand:
      /* Return the number of scaler channels */
      *pivalue = pPvt->maxSignals;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read scalerChanneksCommand %d\n", *pivalue);
      break;

    case scalerReadCommand:
      /* Read a single scaler channel */
      *pivalue = pPvt->address->counter_regs[signal];
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read scalerReadCommand channel%d=%d\n", signal, *pivalue);
      break;

    case scalerDoneCommand:
      /* Return scaler done, which is opposite of pPvt->acquiring */
      *pivalue = 1 - pPvt->acquiring;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::SIS3820Read scalerDoneCommand, returning %d\n", *pivalue);
      break;

    default:
      asynPrint(pasynUser, ASYN_TRACE_ERROR,
          "drvMcaSIS3820Asyn::SIS3820Read got illegal command %d\n",
          command);
      status = asynError;
      break;
  }
  INTERLOCK_OFF;
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
  int command = pasynUser->reason;
  asynStatus status = asynSuccess;
  epicsInt32 *pBuff = data;
  epicsUInt32 *pPvtBuff;
  volatile epicsUInt32 *in;
  epicsUInt32 *out;
  int i;

  INTERLOCK_ON;
  pasynManager->getAddr(pasynUser, &signal);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::int32ArrayRead entry, signal=%d, maxChans=%d\n", 
      signal, maxChans);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::int32ArrayRead fifo word count = %d\n", 
      pPvt->address->fifo_word_count_reg);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::int32ArrayRead status register = 0x%08x\n", 
      pPvt->address->control_status_reg);

  switch (command) {
    case mcaData:
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
          "drvMcaSIS3820Asyn::int32ArrayRead [signal=%d]: read %d chans\n", signal, numSamples);

      *nactual = numSamples;
      break;

    case scalerReadCommand:
      if (maxChans > pPvt->maxSignals) maxChans = pPvt->maxSignals;
      in = pPvt->address->counter_regs;
      out = data;
      for (i=0; i<maxChans; i++) {
        *out++ = *in++;
      }
      *nactual = maxChans;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::int32ArrayRead scalerReadCommand: read %d chans, channel[0]=%d\n", 
          maxChans, data[0]);
      break;

    default:
      asynPrint(pasynUser, ASYN_TRACE_ERROR,
          "drvMcaSIS3820Asyn::int32ArrayRead got illegal command %d\n",
          command);
      status = asynError;
  }
  INTERLOCK_OFF;
  return(status);
}


static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
    epicsInt32 *data, size_t maxChans)
{
  /* There is no way to write to a SIS3820 FIFO (other than in a special card test mode) 
   * so this function is not implemented in this driver.
   */
  asynPrint(pasynUser, ASYN_TRACE_ERROR,
      "drvMcaSIS3820Asyn::int32ArrayWrite, write to SIS3820 not implemented\n");
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
          "drvMcaSIS3820Asyn::drvUserCreate, command=%s\n", pstring);
      return(asynSuccess);
    }
  }
  for (i=0; i<MAX_SIS3820_SCALER_COMMANDS; i++) {
    pstring = sis3820ScalerCommands[i].commandString;
    if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
      pasynUser->reason = sis3820ScalerCommands[i].command;
      if (pptypeName) *pptypeName = epicsStrDup(pstring);
      if (psize) *psize = sizeof(sis3820ScalerCommands[i].command);
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
  int command = pasynUser->reason;

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
  fprintf(fp, "SIS3820: asyn port: %s, connected at VME base address %p, maxChans=%d\n",
      pPvt->portName, pPvt->address, pPvt->maxChans);
}

/* Connect */
static asynStatus SIS3820Connect(void *drvPvt, asynUser *pasynUser)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;
  int signal;
  
  pasynManager->getAddr(pasynUser, &signal);
  if (signal < pPvt->maxSignals) {
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
  } else {
    return(asynError);
  }
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
  int i;

  if (pPvt->exists == 0) return (ERROR);

  /* Erase buffer in driver */

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

  /* Reset elapsed times */
  pPvt->elapsedTime = 0.;
  pPvt->elapsedPrevious = 0.;

  /* Reset the elapsed counts */
  for (i=0; i<pPvt->maxSignals; i++)
    pPvt->elapsedCounts[i] = 0;

  /* Reset the start time.  This is necessary here because we may be
   * acquiring, and AcqOn will not be called. Normally this is set in AcqOn.
   */
  epicsTimeGetCurrent(&pPvt->startTime);

  return (OK);
}


static int readFIFO(mcaSIS3820Pvt *pPvt, asynUser *pasynUser)
{
  /* Read the data stored in the FIFO to the buffer in the private driver data
   * structure.
   */

  epicsUInt32 *pBuff;
  epicsUInt32 *endBuff;
  int count = 0;
  int signal, i, j;
  epicsTimeStamp now;
  int reason;
  ELLLIST *pclientList;
  interruptNode *pNode;
  asynInt32Interrupt *pint32Interrupt;


  asynPrint(pasynUser, ASYN_TRACE_FLOW, "drvMcaSIS3820Asyn::readFIFO entering\n");

  /* Current end of buffer stored in the private data structure */
  pBuff = pPvt->buffPtr;
  endBuff = pPvt->buffer + (pPvt->maxSignals * pPvt->nchans);

  epicsTimeGetCurrent(&now);

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::readFIFO enter: fifo word count = %d, irq_csr=%x, acquiring=%d, prevAcquiring=%d\n",
      pPvt->address->fifo_word_count_reg,
      pPvt->address->irq_control_status_reg,
      pPvt->acquiring,
      pPvt->prevAcquiring);

  /* Check if the FIFO actually contains anything. If so, read out the FIFO to
   * the local buffer.  Put a sanity check on output pointer so we don't
   * overwrite memory.
   */
  while ((pPvt->address->fifo_word_count_reg != 0) &&
         (pBuff < endBuff))
  {
    *pBuff = *pPvt->fifo_base;
    pBuff++;
    count++;
  }

  if (pPvt->acquiring) {
    pPvt->elapsedTime = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
    if ((pPvt->presetReal > 0) && (pPvt->elapsedTime >= pPvt->presetReal)) {
      pPvt->acquiring = 0;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "drvMcaSIS3820Asyn::readFIFO, stopped acquisition by preset real time\n");
    }
  }


  if (pPvt->acquiring) {
    if ((pPvt->presetLive > 0) && (pPvt->elapsedTime >= pPvt->presetLive)) {
      pPvt->acquiring = 0;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
        "drvMcaSIS3820Asyn::readFIFO, stopped acquisition by preset live time\n");
    }
  }

  /* Check that acquisition is complete by buffer pointer.  This ensures
   * that it will be detected even if interrupts are disabled.
   */
  if (pPvt->acquiring) {
    if (pBuff >= endBuff) {
      pPvt->acquiring = 0;
      asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::readFIFO, stopped acquisition by buffer pointer = %p\n",
          endBuff);
    }
  }

  /* Check that acquisition is complete by preset counts */
  if (pPvt->acquiring) {
    for (signal=0; signal<pPvt->maxSignals; signal++) {
      if (pPvt->presetCounts[signal] > 0) {
        pPvt->elapsedCounts[signal] = 0;
        for (i = pPvt->presetStartChan[signal],
             j = pPvt->presetStartChan[signal] * pPvt->maxSignals + signal;
             i <= pPvt->presetEndChan[signal];
             i++, j += pPvt->maxSignals) {
               pPvt->elapsedCounts[signal] += pPvt->buffer[j];
        }
        if (pPvt->elapsedCounts[signal] >= pPvt->presetCounts[signal]) {
          pPvt->acquiring=0;
          asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "drvMcaSIS3820Asyn::readFIFO, stopped acquisition by preset counts\n");
        }
      }
    }
  }
 
  /* If acquisition just stopped then ... */
  if ((pPvt->prevAcquiring == 1) && (pPvt->acquiring == 0)) {
    /* Turn off hardware acquisition */
    pPvt->address->key_op_disable_reg = 1;
    pPvt->elapsedTime = epicsTimeDiffInSeconds(&now, &pPvt->startTime);
    /* Make callbacks to any clients that have requested notification when acquisition completes */
    pasynManager->interruptStart(pPvt->int32InterruptPvt, &pclientList);
    pNode = (interruptNode *)ellFirst(pclientList);
    while (pNode) 
    {
      pint32Interrupt = pNode->drvPvt;
      reason = pint32Interrupt->pasynUser->reason;
      if (((pPvt->acquireMode == MCS_MODE)     && (reason == mcaAcquiring)) || 
          ((pPvt->acquireMode == SCALER_MODE)  && (reason == scalerDoneCommand)))
      {
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
          "drvMcaSIS3820Asyn::readFIFO, making pint32Interrupt->Callback\n");
        pint32Interrupt->callback(pint32Interrupt->userPvt,
            pint32Interrupt->pasynUser,
            0);
      }
      pNode = (interruptNode *)ellNext(&pNode->node);
    }
    pasynManager->interruptEnd(pPvt->int32InterruptPvt);
  }

  /* Re-enable FIFO threshold and FIFO almost full interrupts */
  /* NOTE: WE ARE NOT USING FIFO THRESHOLD INTERRUPTS FOR NOW */
  /* pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE1_ENABLE; */
  pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE4_ENABLE;

  /* Save the new pointer to the start of the empty section of the data buffer */
  pPvt->buffPtr = pBuff;

  asynPrint(pasynUser, ASYN_TRACE_FLOW, 
      "drvMcaSIS3820Asyn::readFIFO exit: fifo word count = %d, irq_csr=%x, acquiring=%d, prevAcquiring=%d\n",
      pPvt->address->fifo_word_count_reg,
      pPvt->address->irq_control_status_reg,
      pPvt->acquiring,
      pPvt->prevAcquiring);

  /* Save the acquisition status */
  pPvt->prevAcquiring = pPvt->acquiring;

  return count;
}


static void setAcquireMode(mcaSIS3820Pvt *pPvt, SIS3820AcquireMode acquireMode)
{
  int i;
  epicsUInt32 channelDisableRegister = SIS3820_CHANNEL_DISABLE_MASK;

  if (pPvt->acquireMode == acquireMode) return;  /* Nothing to do */
  pPvt->acquireMode = acquireMode;

  /* Initialize board and set the control status register */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: initialising control status register\n");
  setControlStatusReg(pPvt);
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: control status register = 0x%08x\n",
      pPvt->address->control_status_reg);

  /* Set the interrupt control register */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: setting interrupt control register\n");
  setIrqControlStatusReg(pPvt);
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: interrupt control register = 0x%08x\n",
      pPvt->address->irq_control_status_reg);

  /* Set the operation mode of the scaler */
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: setting operation mode\n");
  setOpModeReg(pPvt);
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: operation mode register = 0x%08x\n",
      pPvt->address->op_mode_reg);

  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: control status register = 0x%08x\n",
      pPvt->address->control_status_reg);

  /* Trigger an interrupt when 1024 FIFO registers have been filled */
  pPvt->address->fifo_word_threshold_reg = 1024;
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: FIFO threshold = %d\n",
      pPvt->address->fifo_word_threshold_reg);

  /* Set the LNE channel */
  if (pPvt->acquireMode == MCS_MODE) {
    pPvt->address->lne_channel_select_reg = SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
  } else {
    pPvt->address->lne_channel_select_reg = SIS3820_LNE_CHANNEL;
  }
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
      "mcaSIS3820Config: setting readout channels=%d\n",pPvt->maxSignals);
  for (i = 0; i < pPvt->maxSignals; i++)
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
  pPvt->address->lne_prescale_factor_reg = pPvt->lnePrescale - 1;
  asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW,
      "mcaSIS3820Config: lne prescale register=%d\n",
      pPvt->address->lne_prescale_factor_reg);

  /* Enable or disable 50 MHz channel 1 reference pulses. */
  if (pPvt->ch1RefEnable)
    pPvt->address->control_status_reg |= CTRL_REFERENCE_CH1_ENABLE;
  else
    pPvt->address->control_status_reg |= CTRL_REFERENCE_CH1_DISABLE;


  if (pPvt->acquireMode == MCS_MODE) {
    /* Clear all presets from scaler mode */
    clearScalerPresets(pPvt);
  } else {
    /* Clear the preset register */
    pPvt->address->acq_preset_reg = 0;
  }
}


static void clearScalerPresets(mcaSIS3820Pvt *pPvt)
{
  pPvt->address->preset_channel_select_reg &= ~SIS3820_FOUR_BIT_MASK;
  pPvt->address->preset_channel_select_reg &= ~(SIS3820_FOUR_BIT_MASK << 16);
  pPvt->address->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP1;
  pPvt->address->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP2;
  pPvt->address->preset_group2_reg = 0;
  pPvt->address->preset_group2_reg = 0;
}

static void setScalerPresets(mcaSIS3820Pvt *pPvt)
{
  int i;
  epicsUInt32 presetChannelSelectRegister;

  if (pPvt->acquireMode == SCALER_MODE) {
    /* Disable all presets to start */
    clearScalerPresets(pPvt);

    /* The logic implemented in this code is that the last channel with a non-zero
     * preset value in a given channel group (1-16, 17-32) will be the one that is
     * in effect
     */
    for (i=0; i<pPvt->maxSignals; i++) {
      if (pPvt->scalerPresets[i] != 0) {
        if (i < 16) {
          pPvt->address->preset_group1_reg = pPvt->scalerPresets[i];
          /* Enable this bank of counters for preset checking */
          pPvt->address->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP1;
          /* Set the correct channel for checking against the preset value */
          presetChannelSelectRegister = pPvt->address->preset_channel_select_reg;
          presetChannelSelectRegister &= ~SIS3820_FOUR_BIT_MASK;
          presetChannelSelectRegister |= i;
          pPvt->address->preset_channel_select_reg = presetChannelSelectRegister;
        } else {
          pPvt->address->preset_group2_reg = pPvt->scalerPresets[i];
          /* Enable this bank of counters for preset checking */
          pPvt->address->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP2;
          /* Set the correct channel for checking against the preset value */
          presetChannelSelectRegister = pPvt->address->preset_channel_select_reg;
          presetChannelSelectRegister &= ~(SIS3820_FOUR_BIT_MASK << 16);
          presetChannelSelectRegister |= (i << 16);
          pPvt->address->preset_channel_select_reg = presetChannelSelectRegister;
        }
      }
    }
  }
}


static void setControlStatusReg(mcaSIS3820Pvt *pPvt)
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
  controlRegister |= CTRL_REFERENCE_CH1_ENABLE;

  pPvt->address->control_status_reg = controlRegister;
}


static void setOpModeReg(mcaSIS3820Pvt *pPvt)
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

  operationRegister |= SIS3820_MCS_DATA_FORMAT_32BIT;
  operationRegister |= SIS3820_SCALER_DATA_FORMAT_32BIT;

  if (pPvt->acquireMode == MCS_MODE) {
    operationRegister |= SIS3820_CLEARING_MODE;
    operationRegister |= SIS3820_ARM_ENABLE_CONTROL_SIGNAL;
    operationRegister |= SIS3820_FIFO_MODE;
    operationRegister |= SIS3820_OP_MODE_MULTI_CHANNEL_SCALER;
    if (pPvt->lneSource == SIS3820_LNE_SOURCE_INTERNAL_10MHZ) {
      operationRegister |= SIS3820_LNE_SOURCE_INTERNAL_10MHZ;
    }
    if (pPvt->lneSource == SIS3820_LNE_SOURCE_CONTROL_SIGNAL) {
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

  if (pPvt->inputMode < 0 || pPvt->inputMode > 5)
    pPvt->inputMode = 0;

  operationRegister |= pPvt->inputMode << SIS3820_INPUT_MODE_SHIFT;

  /* Check that output mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */

  if (pPvt->outputMode < 0 || pPvt->outputMode > 2)
    pPvt->outputMode = 0;

  operationRegister |= pPvt->outputMode << SIS3820_OUTPUT_MODE_SHIFT;

  pPvt->address->op_mode_reg = operationRegister;
}


static void setIrqControlStatusReg(mcaSIS3820Pvt *pPvt)
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

  pPvt->address->irq_control_status_reg = interruptRegister;
}


/**********************/
/* Interrupt handling */
/**********************/

static void intFunc(void *drvPvt)
{
  mcaSIS3820Pvt *pPvt = (mcaSIS3820Pvt *)drvPvt;

  /* Disable interrupts */
  pPvt->address->irq_config_reg &= ~SIS3820_IRQ_ENABLE;

  /* Test which interrupt source has triggered this interrupt. */
  pPvt->irqStatusReg = pPvt->address->irq_control_status_reg;

  /* Check for the FIFO threshold interrupt */
  if (pPvt->irqStatusReg & SIS3820_IRQ_SOURCE1_FLAG)
  {
    /* Note that this is a level-sensitive interrupt, not edge sensitive, so it can't be cleared */
    /* Disable this interrupt, since it is caused by FIFO threshold, and that
     * condition is only cleared in the readFIFO routine */
    pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE1_DISABLE;
  }

  /* Check for the data acquisition complete interrupt */
  else if (pPvt->irqStatusReg & SIS3820_IRQ_SOURCE2_FLAG)
  {
    /* Reset the interrupt source */
    pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE2_CLEAR;
    pPvt->acquiring = 0;
  }

  /* Check for the FIFO almost full interrupt */
  else if (pPvt->irqStatusReg & SIS3820_IRQ_SOURCE4_FLAG)
  {
    /* This interrupt represents an error condition of sorts. For the moment, I
     * will terminate data collection, as it is likely that data will have been
     * lost.
     * Note that this is a level-sensitive interrupt, not edge sensitive, so it can't be cleared.
     * Instead we disable the interrupt, and re-enable it at the end of readFIFO.
     */
    pPvt->address->irq_control_status_reg = SIS3820_IRQ_SOURCE4_DISABLE;
  }

  /* Send an event to intTask to read the FIFO and perform any requested callbacks */
  epicsEventSignal(pPvt->intEventId);

  /* Reenable interrupts */
  pPvt->address->irq_config_reg |= SIS3820_IRQ_ENABLE;

}


static void intTask(mcaSIS3820Pvt *pPvt)
{

  while(1)
  {
    epicsEventWait(pPvt->intEventId);

    /* Read the accumulated data from the FIFO */
    INTERLOCK_ON;
    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
        "drvMcaSIS3820Asyn::intTask, got interrupt event, irq_csr in intFunc=%x, now=%x\n", 
        pPvt->irqStatusReg, pPvt->address->irq_control_status_reg);
    readFIFO(pPvt, pPvt->pasynUser);
    INTERLOCK_OFF;
  }

}

