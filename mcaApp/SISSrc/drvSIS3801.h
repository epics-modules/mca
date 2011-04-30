/* File:    drvSIS3801.h
 * Author:  Mark Rivers, University of Chicago
 * Date:    22-Apr-2011
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for the SIS3801 and SIS3801 multichannel scalers.  This is the SIS3920 class.
 *
 * Acknowledgements:
 * This driver module is based on previous versions by Wayne Lewis and Ulrik Pedersen.
 *
 */

#ifndef DRVMCASIS3801ASYN_H
#define DRVMCASIS3801ASYN_H

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynPortDriver.h>
#include <epicsEvent.h>
#include <epicsTypes.h>

#include "drvSIS38XX.h"

/***************/
/* Definitions */
/***************/

/* FIFO information */
#define SIS3801_FIFO_BYTE_SIZE    0x100
#define SIS3801_FIFO_WORD_SIZE    0x040

#define SIS3801_SCALER_MODE_RATE     100   /* 100 Hz readout of FIFO */

#define SIS3801_INTERNAL_CLOCK  10000000  /* The internal clock on the SIS3801 */
#define SIS3801_10MHZ_CLOCK     10000000  /* The internal LNE clock on the SIS38xx */

#define SIS3801_ADDRESS_TYPE atVMEA32
#define SIS3801_BOARD_SIZE   2048

/* Status register definitions */
#define STATUS_M_USER_LED                  0x00000001
#define STATUS_M_FIFO_TEST_MODE            0x00000002
#define STATUS_M_INPUT_MODE_BIT_0          0x00000004
#define STATUS_M_INPUT_MODE_BIT_1          0x00000008
#define STATUS_M_TEST_PULSES               0x00000010
#define STATUS_M_INPUT_TEST_MODE           0x00000020
#define STATUS_M_BROADCAST_MODE            0x00000040
#define STATUS_M_10MHZ_LNE_MODE            0x00000040
#define STATUS_M_BROADCAST_HAND            0x00000080
#define STATUS_M_ENABLE_PRESCALER          0x00000080
#define STATUS_M_FIFO_FLAG_EMPTY           0x00000100
#define STATUS_M_FIFO_FLAG_ALMOST_EMPTY    0x00000200
#define STATUS_M_FIFO_FLAG_HALF_FULL       0x00000400
#define STATUS_M_FIFO_FLAG_ALMOST_FULL     0x00000800
#define STATUS_M_FIFO_FLAG_FULL            0x00001000
#define STATUS_M_ENABLE_NEXT_LOGIC         0x00008000
#define STATUS_M_ENABLE_EXT_NEXT           0x00010000
#define STATUS_M_ENABLE_EXT_CLEAR          0x00020000
#define STATUS_M_ENABLE_EXT_DIS            0x00040000
#define STATUS_M_ENABLE_EXTERNAL_DISABLE   0x00080000
#define STATUS_M_ENABLE_IRQ_0              0x00100000
#define STATUS_M_ENABLE_IRQ_1              0x00200000
#define STATUS_M_ENABLE_IRQ_2              0x00400000
#define STATUS_M_ENABLE_IRQ_3              0x00800000
#define STATUS_M_INTERNAL_VME_IRQ          0x04000000
#define STATUS_M_VME_IRQ                   0x08000000
#define STATUS_M_VME_IRQS_0                0x10000000
#define STATUS_M_VME_IRQS_1                0x20000000
#define STATUS_M_VME_IRQS_2                0x40000000
#define STATUS_M_VME_IRQS_3                0x80000000

/* Control register definitions */
#define CONTROL_M_SET_USER_LED                0x00000001
#define CONTROL_M_ENABLE_FIFO_TEST_MODE       0x00000002
#define CONTROL_M_SET_INPUT_MODE_BIT_0        0x00000004
#define CONTROL_M_SET_INPUT_MODE_BIT_1        0x00000008
#define CONTROL_M_ENABLE_25MHZ_TEST_PULSES    0x00000010
#define CONTROL_M_ENABLE_INPUT_TEST_MODE      0x00000020
#define CONTROL_M_ENABLE_BROADCAST_MODE       0x00000040  // Pre version 5 firmware
#define CONTROL_M_ENABLE_10MHZ_LNE_PRESCALER  0x00000040  // Version 5 firmware
#define CONTROL_M_ENABLE_BROADCASE_HAND       0x00000080  // Pre version 5 firmware
#define CONTROL_M_ENABLE_LNE_PRESCALER        0x00000080  // Version 5 firmware
#define CONTROL_M_CLEAR_USER_LED              0x00000100
#define CONTROL_M_DISABLE_FIFO_TEST_MODE      0x00000200
#define CONTROL_M_CLEAR_INPUT_MODE_BIT_0      0x00000400
#define CONTROL_M_CLEAR_INPUT_MODE_BIT_1      0x00000800
#define CONTROL_M_DISABLE_25MHZ_TEST_PULSES   0x00001000
#define CONTROL_M_DISABLE_INPUT_TEST_MODE     0x00002000
#define CONTROL_M_DISABLE_BROADCAST_MODE      0x00004000  // Pre version 5 firmware
#define CONTROL_M_DISABLE_10MHZ_LNE_PRESCALER 0x00004000  // Version 5 firmware
#define CONTROL_M_DISABLE_BROADCAST_HAND      0x00008000  // Pre version 5 firmware
#define CONTROL_M_DISABLE_LNE_PRESCALER       0x00008000  // Version 5 firmware
#define CONTROL_M_ENABLE_EXTERNAL_NEXT        0x00010000
#define CONTROL_M_ENABLE_EXTERNAL_CLEAR       0x00020000
#define CONTROL_M_ENABLE_EXTERNAL_DISABLE     0x00040000
#define CONTROL_M_SET_SOFTWARE_DISABLE        0x00080000
#define CONTROL_M_ENABLE_IRQ_0                0x00100000
#define CONTROL_M_ENABLE_IRQ_1                0x00200000
#define CONTROL_M_ENABLE_IRQ_2                0x00400000
#define CONTROL_M_ENABLE_IRQ_3                0x00800000
#define CONTROL_M_DISABLE_EXTERNAL_NEXT       0x01000000
#define CONTROL_M_DISABLE_EXTERNAL_CLEAR      0x02000000
#define CONTROL_M_DISABLE_EXTERNAL_DISABLE    0x04000000
#define CONTROL_M_CLEAR_SOFTWARE_DISABLE      0x08000000
#define CONTROL_M_DISABLE_IRQ_0               0x10000000
#define CONTROL_M_DISABLE_IRQ_1               0x20000000
#define CONTROL_M_DISABLE_IRQ_2               0x40000000
#define CONTROL_M_DISABLE_IRQ_3               0x80000000


#define IRQ_M_VME_IRQ_ENABLE                  0x00000800

/**************/
/* Structures */
/**************/

/* This structure duplicates the control and status register structure of the
 * SIS3801. 
 */
typedef volatile struct {
    epicsUInt32 csr_reg;               /* Offset = 0x00 */
    epicsUInt32 irq_reg;               /* Offset = 0x04 */
    epicsUInt32 unused08;
    epicsUInt32 copy_disable_reg;      /* Offset = 0x0c */
    epicsUInt32 fifo_write_reg;        /* Offset = 0x10 */
    epicsUInt32 unused14;
    epicsUInt32 unused18;
    epicsUInt32 unused1c;
    epicsUInt32 clear_fifo_reg;        /* Offset = 0x20 */
    epicsUInt32 soft_next_reg;         /* Offset = 0x24 */
    epicsUInt32 enable_next_reg;       /* Offset = 0x28 */
    epicsUInt32 disable_next_reg;      /* Offset = 0x2c */
    epicsUInt32 brd_clear_fifo_reg;    /* Offset = 0x30 */
    epicsUInt32 brd_soft_next_reg;     /* Offset = 0x34 */
    epicsUInt32 brd_enable_next_reg;   /* Offset = 0x38 */
    epicsUInt32 brd_disable_next_reg;  /* Offset = 0x3c */
    epicsUInt32 unused40_4c[4];
    epicsUInt32 enable_ch1_pulser;     /* Offset - 0x50 */
    epicsUInt32 disable_ch1_pulser;    /* Offset - 0x54 */
    epicsUInt32 unused58_5c[2];
    epicsUInt32 key_reset_reg;         /* Offset = 0x60 */
    epicsUInt32 unused64;     
    epicsUInt32 test_pulse_reg;        /* Offset = 0x68 */
    epicsUInt32 unused6c;     
    epicsUInt32 unused70_7c[4];
    epicsUInt32 prescale_factor_reg;   /* Offset = 0x80 */
    epicsUInt32 unused84_fc[31];
    epicsUInt32 fifo_reg;              /* Offset = 0x100 */
    epicsUInt32 unused104_7fc[447];    /* Unused space to 2KB per board */
} SIS3801_REGS;

class drvSIS3801 : public drvSIS38XX
{
  public:
  drvSIS3801(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
             int maxChans, int maxSignals);

  // Public methods we override from drvSIS38XX
  void report(FILE *fp, int details);
  // Public methods new to this class
  void intFunc();        // Should be private, but called from C callback function
  void readFIFOThread(); // Should be private, but called from C callback function

  protected:
  // Protected methods we override from drvSIS38XX
  void erase();
  // Protected pure virtual functions from drvSIS38XX that we implement
  void stopMCSAcquire();
  void startMCSAcquire();
  void setChannelAdvanceSource();
  void enableInterrupts();
  void disableInterrupts();
  void setAcquireMode(SIS38XXAcquireMode_t acquireMode);
  void resetScaler();
  void startScaler();
  void stopScaler();
  void readScalers();
  void clearScalerPresets();
  void setScalerPresets();
  void setOutputMode();
  void setInputMode();
  void softwareChannelAdvance();
  void setLED();
  int getLED();
  void setMuxOut();
  int getMuxOut();

  private:
  void resetFIFO();
  void setOpModeReg();
  void setControlStatusReg();
  SIS3801_REGS *registers_;
};

/***********************/
/* Function prototypes */
/***********************/

/* External functions */
/* iocsh functions */
extern "C" {
int drvSIS3801Config(const char *portName, int baseAddress, int interruptVector, int interruptLevel, 
                     int maxChans, int maxSignals);
}
#endif

