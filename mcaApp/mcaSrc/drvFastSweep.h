/* File:    drvFastSweep.h
 * Author:  Mark Rivers, University of Chicago
 * Date:   18-June-2012
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for fast sweep, i.e. drivers that do callbacks on asynInt32Array.
 *
 */

#ifndef DRVFASTSWEEP_H
#define DRVFASTSWEEP_H

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynPortDriver.h>
#include <epicsEvent.h>
#include <epicsTypes.h>

#define fastSweepMaxChannelsString     "FAST_SWEEP_MAX_CHANNELS"
#define fastSweepCurrentChannelString  "FAST_SWEEP_CURRENT_CHANNEL"


class drvFastSweep : public asynPortDriver
{
  
  public:
  drvFastSweep(const char *portName, const char *inputName, 
               int maxSignals, int maxPoints, const char *dataString, const char *intervalString);

  // These are the methods we override from asynPortDriver
  asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
  asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
  asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                            size_t maxChans, size_t *nactual);
  virtual void report(FILE *fp, int details);

  // These are the methods that are new to this class
  void intervalCallback(double seconds);
  void dataCallback(epicsInt32 *newData, size_t nelem);
  void nextPoint(int *newData);
  void computeNumAverage();
  void stopAcquire();
  
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
  int fastSweepMaxChannels_;
  int fastSweepCurrentChannel_;
  #define LAST_FAST_SWEEP_PARAM fastSweepCurrentChannel_

  private:
  char *inputName_;
  char *dataString_;
  char *intervalString_;
  int maxSignals_;
  int maxPoints_;
  int numPoints_;
  int acquiring_;
  int numAcquired_;
  double realTime_;
  double elapsedTime_;
  double dwellTime_;
  epicsTimeStamp startTime_;
  double callbackInterval_;
  int *pData_;
  int numAverage_;
  int accumulated_;
  double *pAverageStore_;
  bool erased_;
  asynUser *pasynUserInt32Array_;
  asynInt32Array *pint32Array_;
  void *int32ArrayRegistrarPvt_;
  void *int32ArrayPvt_;
  asynUser *pasynUserFloat64_;
  asynUser *pasynUserFloat64SyncIO_;
  void *float64RegistrarPvt_;
};

#define NUM_FAST_SWEEP_PARAMS (int)(&LAST_FAST_SWEEP_PARAM - &FIRST_FAST_SWEEP_PARAM + 1)

#endif

