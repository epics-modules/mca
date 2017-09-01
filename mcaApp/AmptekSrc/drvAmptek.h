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

#define amptekInputPolarityString   "AMPTEK_INPUT_POLARITY"
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
#define amptekFlatTopTimeString     "AMPTEK_FLAT_TOP_TIME"
#define amptekConfigFileString      "AMPTEK_CONFIG_FILE"
#define amptekSaveConfigFileString  "AMPTEK_SAVE_CONFIG_FILE"
#define amptekLoadConfigFileString  "AMPTEK_LOAD_CONFIG_FILE"
#define amptekSlowCountsString      "AMPTEK_SLOW_COUNTS"
#define amptekFastCountsString      "AMPTEK_FAST_COUNTS"
#define amptekDetTempString         "AMPTEK_DET_TEMP"
#define amptekSetDetTempString      "AMPTEK_SET_DET_TEMP"
#define amptekBoardTempString       "AMPTEK_BOARD_TEMP"
#define amptekHighVoltageString     "AMPTEK_HIGH_VOLTAGE"
#define amptekSetHighVoltageString  "AMPTEK_SET_HIGH_VOLTAGE"
#define amptekMCSLowChannelString   "AMPTEK_MCS_LOW_CHANNEL"
#define amptekMCSHighChannelString  "AMPTEK_MCS_HIGH_CHANNEL"
#define amptekModelString           "AMPTEK_MODEL"
#define amptekFirmwareString        "AMPTEK_FIRMWARE"
#define amptekBuildString           "AMPTEK_BUILD"
#define amptekFPGAString            "AMPTEK_FPGA"
#define amptekSerialNumberString    "AMPTEK_SERIAL_NUMBER"
#define amptekAuxOut1String         "AMPTEK_AUX_OUT1"
#define amptekAuxOut2String         "AMPTEK_AUX_OUT2"
#define amptekAuxOut34String        "AMPTEK_AUX_OUT34"
#define amptekConnect1String        "AMPTEK_CONNECT1"
#define amptekConnect2String        "AMPTEK_CONNECT2"
#define amptekSCAOutputWidthString  "AMPTEK_SCA_OUTPUT_WIDTH"
#define amptekSCALowChannelString   "AMPTEK_SCA_LOW_CHANNEL"
#define amptekSCAHighChannelString  "AMPTEK_SCA_HIGH_CHANNEL"
#define amptekSCAOutputLevelString  "AMPTEK_SCA_OUTPUT_LEVEL"


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
  void exitHandler();

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
  int amptekFlatTopTime_;
  int amptekConfigFile_;
  int amptekSaveConfigFile_;
  int amptekLoadConfigFile_;
  int amptekSlowCounts_;
  int amptekFastCounts_;
  int amptekDetTemp_;
  int amptekSetDetTemp_;
  int amptekBoardTemp_;
  int amptekHighVoltage_;
  int amptekSetHighVoltage_;
  int amptekMCSLowChannel_;
  int amptekMCSHighChannel_;
  int amptekModel_;
  int amptekFirmware_;
  int amptekBuild_;
  int amptekFPGA_;
  int amptekSerialNumber_;
  int amptekAuxOut1_;
  int amptekAuxOut2_;
  int amptekAuxOut34_;
  int amptekConnect1_;
  int amptekConnect2_;
  int amptekSCAOutputWidth_;
  int amptekSCALowChannel_;
  int amptekSCAHighChannel_;
  int amptekSCAOutputLevel_;
 
  private:
  CConsoleHelper CH_;
  CONFIG_OPTIONS configOptions_;
  asynStatus connectDevice();
  asynStatus findModule();
  asynStatus sendCommand(TRANSMIT_PACKET_TYPE command);
  asynStatus sendCommandString(string commandString);
  asynStatus sendConfiguration();
  asynStatus sendSCAs();
  asynStatus sendConfigurationFile(string fileName);
  asynStatus saveConfigurationFile(string fileName);
  asynStatus readConfigurationFromHardware();
  asynStatus parseConfiguration();
  asynStatus parseConfigDouble(const char *str, int param);
  asynStatus parseConfigInt(const char *str, int param);
  asynStatus parseConfigEnum(const char *str, const char *enumStrs[], int numEnums, int param);
  dp5DppTypes dppType_;
  bool acquiring_;
  bool haveConfigFromHW_;
  DppInterface_t interfaceType_;
  char *addressInfo_;
  epicsInt32 *pData_;
  size_t numChannels_;
};

#endif

