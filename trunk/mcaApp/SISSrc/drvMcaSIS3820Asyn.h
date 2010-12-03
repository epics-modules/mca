/* File: 	  drvMcaSIS3820Asyn.h
 * Author:  Wayne Lewis
 * Date:    04-sep-2006
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for the SIS3820 multichannel scaler.
 *
 * Acknowledgements:
 * This driver module is based heavily on drvMcaSIS3820Asyn.c by Mark Rivers.
 *
 * Revisions:
 * 0.1  Wayne Lewis               Original version
 * 0.2  Mark Rivers               Modifications for vxWorks, external channel advance, etc.
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
 * FOR A PARTICULAR PURPOSE ARE DISCLSIS3820ED. IN NO EVENT SHALL THE
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

#ifndef DRVMCASIS3820ASYN_H
#define DRVMCASIS3820ASYN_H

/***************/
/* Definitions */
/***************/

#define SIS3820_MAX_SIGNALS 32
#define SIS3820_MAX_CARDS 4

#define MULTICHANNEL_SCALER_MODE 0
#define SIMPLE_SCALER_MODE       1
#define SIS3820_LNE_CHANNEL 32
#define SIS3820_INTERNAL_CLOCK 50000000  /* The internal clock on the SIS38xx */
#define SIS3820_10MHZ_CLOCK 10000000  /* The internal LNE clock on the SIS38xx */

#define SIS3820_ADDRESS_TYPE atVMEA32
#define SIS3820_BOARD_SIZE 0x400000

/* Additional SIS3820 flags */
#define SIS3820_IRQ_ENABLE 0x800
#define SIS3820_IRQ_ROAK 0x1000

/* Bit masks */
#define SIS3820_IRQ_LEVEL_MASK 0x00000700
#define SIS3820_IRQ_VECTOR_MASK 0x000000ff
#define SIS3820_FOUR_BIT_MASK 0x0000000f

#define SIS3820_OP_MODE_REG_LNE_MASK 0x70
#define SIS3820_OP_MODE_REG_MODE_MASK 0x70000000

#define SIS3820_CHANNEL_DISABLE_MASK 0xffffffff;

/* Bit shifts */
#define SIS3820_INPUT_MODE_SHIFT 16
#define SIS3820_OUTPUT_MODE_SHIFT 20

/* FIFO information */
#define SIS3820_FIFO_SIZE 0x800000

#ifndef ERROR
#define ERROR -1
#endif
#ifndef OK
#define OK 0
#endif


typedef enum {
    UNDEFINED_MODE,
    MCS_MODE,
    SCALER_MODE
} SIS3820AcquireMode;

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynDriver.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynInt32Array.h>
#include <asynDrvUser.h>
#include <epicsEvent.h>
#include <epicsMutex.h>
#include <epicsTime.h>
#include <epicsTypes.h>
#include <drvMca.h>
#include <devScalerAsyn.h>


/**************/
/* Structures */
/**************/

/* This structure duplicates the control and status register structure of the
 * SIS3820. Note that it does not extend to the FIFO/SDRAM area, as this would
 * chew a lot of memory. Access to the FIFO/SDRAM needs to be via an explicit
 * offset from the base address. FIFO address space is from 0x800000 to
 * 0xfffffc. 
 */
typedef volatile struct {
  epicsUInt32 control_status_reg;         /* Offset = 0x00 */
  epicsUInt32 moduleID_reg;               /* Offset = 0x04 */
  epicsUInt32 irq_config_reg;             /* Offset = 0x08 */
  epicsUInt32 irq_control_status_reg;     /* Offset = 0x0c */

  epicsUInt32 acq_preset_reg;             /* Offset = 0x10 */
  epicsUInt32 acq_count_reg;              /* Offset = 0x14 */
  epicsUInt32 lne_prescale_factor_reg;    /* Offset = 0x18 */
  epicsUInt32 unused1c;
  
  epicsUInt32 preset_group1_reg;          /* Offset = 0x20 */
  epicsUInt32 preset_group2_reg;          /* Offset = 0x24 */
  epicsUInt32 preset_enable_reg;          /* Offset = 0x28 */
  epicsUInt32 unused2c;

  epicsUInt32 cblt_setup_reg;             /* Offset = 0x30 */
  epicsUInt32 sdram_page_reg;             /* Offset = 0x34 */
  epicsUInt32 fifo_word_count_reg;        /* Offset = 0x38 */
  epicsUInt32 fifo_word_threshold_reg;    /* Offset = 0x3c */

  epicsUInt32 hiscal_start_preset_reg;    /* Offset = 0x40 */
  epicsUInt32 hiscal_start_counter_reg;   /* Offset = 0x44 */
  epicsUInt32 hiscal_last_acq_count_reg;  /* Offset = 0x48 */
  epicsUInt32 unused4c;

  epicsUInt32 unused50_fc[44];
  
  epicsUInt32 op_mode_reg;                /* Offset = 0x100 */
  epicsUInt32 copy_disable_reg;           /* Offset = 0x104 */
  epicsUInt32 lne_channel_select_reg;     /* Offset = 0x108 */
  epicsUInt32 preset_channel_select_reg;  /* Offset = 0x10c */

  epicsUInt32 unused110_1fc[60];
  
  epicsUInt32 count_disable_reg;          /* Offset = 0x200 */
  epicsUInt32 count_clear_reg;            /* Offset = 0x204 */
  epicsUInt32 counter_overflow_reg;       /* Offset = 0x208 */
  epicsUInt32 unused20c;

  epicsUInt32 ch1_17_high_bits_reg;       /* Offset = 0x210 */

  epicsUInt32 unused214_2fc[59];

  epicsUInt32 sdram_prom_reg;             /* Offset = 0x300 */
  epicsUInt32 unused304;
  epicsUInt32 unused308;
  epicsUInt32 unused30c;

  epicsUInt32 xilinx_test_data_reg;       /* Offset = 0x310 */
  epicsUInt32 xilinx_control;             /* Offset = 0x314 */

  epicsUInt32 unused318_3fc[58];

  epicsUInt32 key_reset_reg;              /* Offset = 0x400 */
  epicsUInt32 key_fifo_reset_reg;         /* Offset = 0x404 */
  epicsUInt32 key_test_pulse_reg;         /* Offset = 0x408 */
  epicsUInt32 key_counter_clear;          /* Offset = 0x40c */

  epicsUInt32 key_lne_clock_reg;          /* Offset = 0x410 */
  epicsUInt32 key_op_arm_reg;             /* Offset = 0x414 */
  epicsUInt32 key_op_enable_reg;          /* Offset = 0x418 */
  epicsUInt32 key_op_disable_reg;         /* Offset = 0x41c */

  epicsUInt32 unused420_4fc[56];
  
  epicsUInt32 unused500_7fc[192];

  epicsUInt32 shadow_regs[32];            /* Offset = 0x800 to 0x87c */
  epicsUInt32 unused880_8fc[32];

  epicsUInt32 unused900_9fc[64];

  epicsUInt32 counter_regs[32];           /* Offset = 0xa00 to 0xa7c */
  epicsUInt32 unuseda80_afc[32];

  epicsUInt32 unusedb00_ffc[320];         /* Unused space to 4KB per board */

  /*
  epicsUInt32 unused001000_0ffffc[261120]; 
  epicsUInt32 unused100000_7ffffc[0x1c0000];

  epicsUInt32 fifo_reg;          
  */
} SIS3820_REGS;


typedef struct mcaSIS3820Pvt {
  char *portName;
  epicsUInt32 *vmeBaseAddress;
  int exists;
  int moduleID;
  int firmwareVersion;
  SIS3820_REGS *address;
  epicsUInt32 *fifo_base;
  epicsUInt32 irqStatusReg;
  SIS3820AcquireMode acquireMode;
  int maxSignals;
  int maxChans;
  int nchans;
  int inputMode;
  int outputMode;
  int nextChan;
  int nextSignal;
  double presetReal;
  double presetLive;
  epicsTimeStamp startTime;
  epicsTimeStamp statusTime;
  double maxStatusTime;
  double elapsedTime;
  double elapsedPrevious;
  int presetStartChan[SIS3820_MAX_SIGNALS];
  int presetEndChan[SIS3820_MAX_SIGNALS];
  long presetCounts[SIS3820_MAX_SIGNALS];
  long elapsedCounts[SIS3820_MAX_SIGNALS];
  long scalerPresets[SIS3820_MAX_SIGNALS];
  int lneSource;
  int lnePrescale;
  int numSweeps;
  double dwellTime;
  int ch1RefEnable;
  int softAdvance;
  epicsUInt32 *buffer;  /* maxSignals * maxChans */
  epicsUInt32 *buffPtr;
  int acquiring;
  int prevAcquiring;
  epicsMutexId lock;
  epicsMutexId fifoLock;

  asynUser *pasynUser;
  asynInterface common;
  asynInterface int32;
  void *int32InterruptPvt;
  asynInterface float64;
  void *float64InterruptPvt;
  asynInterface int32Array;
  void *int32ArrayInterruptPvt;
  asynInterface drvUser;
  
  epicsEventId intEventId;
} mcaSIS3820Pvt;

/***********************/
/* Function prototypes */
/***********************/

/* External functions */
/* iocsh functions */
int mcaSIS3820Setup(int baseAddress, int intVector, int intLevel);
int mcaSIS3820Config(char *portName,
                      int card, 
                      int maxSignals, 
                      int maxChans, 
                      int inputMode, 
                      int outputMode, 
                      int lnePrescale);
#endif

