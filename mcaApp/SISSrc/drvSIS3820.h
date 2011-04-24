/* File:    drvSIS3820.h
 * Author:  Mark Rivers
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

#ifndef DRVMCASIS3820ASYN_H
#define DRVMCASIS3820ASYN_H

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynPortDriver.h>
#include <epicsEvent.h>
#include <epicsTypes.h>

#include "drvSIS38XX.h"

// My temporary file
#include <vmeDMA.h>


/***************/
/* Definitions */
/***************/
#define MIN_DMA_TRANSFERS   256

/* FIFO information */
#define SIS3820_FIFO_BYTE_SIZE    0x800000
#define SIS3820_FIFO_WORD_SIZE    0x200000

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

/* VME memory size */
#define SIS3820_VME_MEMORY_SIZE 0x01000000

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
  epicsUInt32 mux_out_channel_select_reg; /* Offset = 0x110 */

  epicsUInt32 unused110_1fc[59];
  
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

class drvSIS3820 : public drvSIS38XX
{
  
  public:
  drvSIS3820(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
             int maxChans, int maxSignals, int inputMode, int outputMode, 
             bool useDma, int fifoBufferWords);

  // Public methods we override from SIS38XX
  void report(FILE *fp, int details);
  // Public methods new to this class
  void intFunc();        // Should be private, but called from C callback function
  void readFIFOThread(); // Should be private, but called from C callback function


  protected:
  // Protected methods we override from SIS38XX
  void erase();
  // Protected pure virtual functions that we must implement
  void stopMCSAcquire();
  void startMCSAcquire();
  void setChannelAdvanceSource();
  void enableInterrupts();
  void disableInterrupts();
  void setAcquireMode(SIS38XXAcquireMode acquireMode);
  void resetScaler();
  void startScaler();
  void stopScaler();
  void readScalers();
  void clearScalerPresets();
  void setScalerPresets();
  void setOutputMode();
  void setInputMode();
  void setLED();
  int getLED();
  void setMuxOut();
  int getMuxOut();

  private:
  void resetFIFO();
  void setOpModeReg();
  void setControlStatusReg();
  void setIrqControlStatusReg();
  SIS3820_REGS *registers_;
};

/***********************/
/* Function prototypes */
/***********************/

/* External functions */
/* iocsh functions */
extern "C" {
int drvSIS3820Config(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                     int maxChans, int maxSignals, int inputMode, int outputMode, 
                     int useDma, int fifoBufferWords);
}
#endif

