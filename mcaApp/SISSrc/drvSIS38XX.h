/* File:    drvSIS38XX.h
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

#ifndef DRVSIS38XXASYN_H
#define DRVSIS38XXASYN_H

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynPortDriver.h>
#include <epicsEvent.h>
#include <epicsTypes.h>

// My temporary file
#include <vmeDMA.h>


/***************/
/* Definitions */
/***************/

#define SIS38XXLEDString              "SIS38XX_LED"
#define SIS38XXMuxOutString           "SIS38XX_MUX_OUT"
#define SIS38XXCh1RefEnableString     "SIS38XX_CH1_REF_ENABLE"
#define SIS38XXAcquireModeString      "SIS38XX_ACQUIRE_MODE"
#define SIS38XXInputModeString        "SIS38XX_INPUT_MODE"
#define SIS38XXOutputModeString       "SIS38XX_OUTPUT_MODE"
#define SIS38XXModelString            "SIS38XX_MODEL"
#define SIS38XXFirmwareString         "SIS38XX_FIRMWARE"

#define SIS38XX_MAX_SIGNALS 32

typedef enum {
    MCS_MODE,
    SCALER_MODE
} SIS38XXAcquireMode;

class drvSIS38XX : public asynPortDriver
{
  
  public:
  drvSIS38XX(const char *portName, int maxChans, int maxSignals);

  // These are the methods we override from asynPortDriver
  asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
  asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                                    size_t maxChans, size_t *nactual);
  virtual void report(FILE *fp, int details);
  virtual void dmaCallback();    // Should be private, but called from C callback function
  bool checkDone(int fifoCount);
  virtual void erase();

  protected:
  // Pure virtual functions, derived class must implement these
  virtual void stopMCSAcquire() = 0;
  virtual void startMCSAcquire() = 0;
  virtual void setChannelAdvanceSource() = 0;
  virtual void enableInterrupts() = 0;
  virtual void disableInterrupts() = 0;
  virtual void setAcquireMode(SIS38XXAcquireMode acquireMode) = 0;
  virtual void resetScaler() = 0;
  virtual void startScaler() = 0;
  virtual void stopScaler() = 0;
  virtual void readScalers() = 0;
  virtual void clearScalerPresets() = 0;
  virtual void setScalerPresets() = 0;
  virtual void setOutputMode() = 0;
  virtual void setInputMode() = 0;
  virtual void setLED() = 0;
  virtual int getLED() = 0;
  virtual void setMuxOut() = 0;
  virtual int getMuxOut() = 0;

   #define FIRST_SIS38XX_PARAM mcaStartAcquire_
  int mcaStartAcquire_;
  int mcaStopAcquire_;
  int mcaErase_;
  int mcaData_;
  int mcaReadStatus_;
  int mcaChannelAdvanceSource_;
  int mcaNumChannels_;
  int mcaDwellTime_;
  int mcaPresetLiveTime_;
  int mcaPresetRealTime_;
  int mcaPresetCounts_;
  int mcaPresetLowChannel_;
  int mcaPresetHighChannel_;
  int mcaPresetSweeps_;
  int mcaAcquireMode_;
  int mcaSequence_;
  int mcaPrescale_;
  int mcaAcquiring_;
  int mcaElapsedLiveTime_;
  int mcaElapsedRealTime_;
  int mcaElapsedCounts_;
  int scalerReset_;
  int scalerChannels_;
  int scalerRead_;
  int scalerPresets_;
  int scalerArm_;
  int scalerDone_;
  int SIS38XXLED_;
  int SIS38XXMuxOut_;
  int SIS38XXCh1RefEnable_;
  int SIS38XXAcquireMode_;
  int SIS38XXInputMode_;
  int SIS38XXOutputMode_;
  int SIS38XXModel_;
  int SIS38XXFirmware_;
  #define LAST_SIS38XX_PARAM SIS38XXFirmware_

  bool exists_;
  int moduleID_;
  epicsUInt32 *fifo_base_;
  epicsUInt32 irqStatusReg_;
  SIS38XXAcquireMode acquireMode_;
  int maxSignals_;
  int maxChans_;
  epicsTimeStamp startTime_;
  double elapsedTime_;
  double elapsedPrevious_;
  bool erased_;
  int lneSource_;
  int lnePrescale_;
  int softAdvance_;
  epicsUInt32 *mcsData_;     /* maxSignals * maxChans */
  epicsUInt32 *scalerData_;  /* maxSignals */
  int nextChan_;
  int nextSignal_;
  epicsUInt32 *fifoBuffer_;
  int fifoBufferWords_;
  epicsUInt32 *fifoBuffPtr_;
  bool acquiring_;
  bool prevAcquiring_;
  epicsEventId readFIFOEventId_;
  bool useDma_;
  DMA_ID dmaId_;
  epicsEventId dmaDoneEventId_;
  epicsMutexId fifoLockId_;
};

#define NUM_SIS38XX_PARAMS (int)(&LAST_SIS38XX_PARAM - &FIRST_SIS38XX_PARAM + 1)

#endif

