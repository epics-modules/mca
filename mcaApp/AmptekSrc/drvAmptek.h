/* File:    drvAmptek.h
 * Author:  Mark Rivers, University of Chicago
 * Date:    24-July-2017
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for Amptek D5 based MCAs.
 *
 */

#ifndef DRVAMPTEK_H
#define DRVAMPTEK_H

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynPortDriver.h>
#include <epicsTypes.h>

#include <ConsoleHelper.h>

typedef enum {
  amptekInterfaceEthernet,
  amptekInterfaceUSB,
  amptekInterfaceSerial
} amptekInterface_t;

#define amptekInputPolarityString   "AMPTEK_POLARITY"
#define amptekClockString           "AMPTEK_CLOCK"
#define amptekGainString            "AMPTEK_GAIN"
#define amptekGateString            "AMPTEK_GATE"
#define amptekMCASourceString       "AMPTEK_MCA_SOURCE"
#define amptekPUREnableString       "AMPTEK_PUR_ENABLE"
#define amptekRTDEnableString       "AMPTEK_RTD_ENABLE"
#define amptekFastThresholdString   "AMPTEK_FAST_THRESHOLD"
#define amptekSlowThresholdString   "AMPTEK_SLOW_THRESHOLD"
#define amptekPeakingTimeString     "AMPTEK_PEAKING_TIME"
#define amptekFastPeakingTimeString "AMPTEK_FAST_PEAKING_TIME"

#define MAX_MODULES 16
#define MAX_IPNAME_LEN 16
#define MAX_PORTNAME_LEN 32

class drvAmptek : public asynPortDriver
{
  
  public:
  drvAmptek(const char *portName, int interfaceType, const char *addressInfo);

  // These are the methods we override from asynPortDriver
  asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
  asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
  asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
  asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                            size_t maxChans, size_t *nactual);
  virtual void report(FILE *fp, int details);

  // These are the methods that are new to this class
  
  protected:
  // These are the standard MCA commands
  #define FIRST_AMPTEK_PARAM mcaStartAcquire_
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
  
  // These are the Amptek specific commands
  int amptekInputPolarity_;
  int amptekClock_;
  int amptekGain_;
  int amptekGate_;
  int amptekMCASource_;
  int amptekPUREnable_;
  int amptekRTDEnable_;
  int amptekFastThreshold_;
  int amptekSlowThreshold_;
  int amptekPeakingTime_;
  int amptekFastPeakingTime_;
  
  private:
  CConsoleHelper consoleHelper;
  CONFIG_OPTIONS configOptions_;
  asynStatus connectDevice();
  asynStatus findModule();
  asynStatus sendCommand(TRANSMIT_PACKET_TYPE command);
  asynStatus sendConfigString(const char *configCommand);
  asynStatus readConfigurationFromHardware();
  bool isConnected_;
  bool acquiring_;
  bool haveConfigFromHW_;
  amptekInterface_t interfaceType_;
  char *addressInfo_;
  epicsInt32 *pData_;
  size_t numChannels_;
};

#endif

