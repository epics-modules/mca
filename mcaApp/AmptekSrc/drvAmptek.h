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

typedef enum {
  amptekInterfaceEthernet,
  amptekInterfaceUSB,
  amptekInterfaceSerial
} amptekInterface_t;

#define MAX_MODULES 16
#define MAX_IPNAME_LEN 16
#define MAX_PORTNAME_LEN 32

typedef struct {
    int serialNumber;
    char moduleIP[MAX_IPNAME_LEN];
} moduleInfo_t;


class drvAmptek : public asynPortDriver
{
  
  public:
  drvAmptek(const char *portName, int interfaceType, const char *addressInfo, int serialNumber);

  // These are the methods we override from asynPortDriver
  asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
  asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                            size_t maxChans, size_t *nactual);
  virtual void report(FILE *fp, int details);

  // These are the methods that are new to this class
  
  protected:
  #define FIRST_FAST_SWEEP_PARAM mcaStartAcquire_
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

  private:
  asynStatus connectDevice();
  asynStatus findModule();
  bool acquiring_;
  amptekInterface_t interfaceType_;
  moduleInfo_t moduleInfo_[MAX_MODULES];
  char *addressInfo_;
  int serialNumber_;
  int numModules_;
  char udpBroadcastPortName_[MAX_PORTNAME_LEN];
  char udpCommandPortName_[MAX_PORTNAME_LEN];
  asynUser *pasynUserUDPBroadcast_;
  asynUser *pasynUserUDPCommand_;
  epicsInt32 *pData_;
  size_t numChannels_;
};

#endif

