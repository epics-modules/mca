/*
 * DSA2000.h
 * 
 * Asyn driver that inherits from the asynPortDriver class to control the high-voltage on Canberra DSA-2000.
 *
 * Author: Mark Rivers
 *
 * Created August 9, 2013
 */

#include "asynPortDriver.h"

/* These are the drvInfo strings that are used to identify the parameters.
 * They are used by asyn clients, including standard asyn device support */
#define P_HVControlString                "HV_CONTROL"                 /* asynUInt32Digital, r/w */
#define P_HVStatusString                 "HV_STATUS"                  /* asynUInt32Digital, r/o */
#define P_HVRangeSettingString           "HV_RANGE_SETTING"           /* asynInt32,         r/w */
#define P_HVRangeReadbackString          "HV_RANGE_READBACK"          /* asynInt32,         r/o */
#define P_DACSettingString               "DAC_SETTING"                /* asynFloat64,       r/w */
#define P_DACReadbackString              "DAC_READBACK"               /* asynFloat64,       r/o */
#define P_ADCReadbackString              "ADC_READBACK"               /* asynFloat64,       r/o */
#define P_HVResetString                  "HV_RESET"                   /* asynInt32,         r/w */
#define P_HVInhibitLevelString           "HV_INHIBIT_LEVEL"           /* asynInt32,         r/w */
#define P_ReadStatusString               "READ_STATUS"                /* asynInt32,         r/w */

/** Class that controls the HVPS of the Canberra DSA-2000. This uses the Canberra library for the AIM
  * for low-level network communication. */
class DSA2000 : public asynPortDriver {
public:
    DSA2000(const char *portName, int moduleAddress);
                 
    /* These are the methods that we override from asynPortDriver */
    virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
    virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
    virtual asynStatus writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask);

    /* These are the methods that are new to this class */

protected:
    /** Values used for pasynUser->reason, and indexes into the parameter library. */
    int P_HVControl;
    #define FIRST_COMMAND P_HVControl
    int P_HVStatus;
    int P_HVRangeSetting;
    int P_HVRangeReadback;
    int P_DACSetting;
    int P_DACReadback;
    int P_ADCReadback;
    int P_HVReset;
    int P_HVInhibitLevel;
    int P_ReadStatus;
    #define LAST_COMMAND P_ReadStatus

private:
    /* Our data */
    int module;
    
    /* Our functions */
    asynStatus setHVStatus();
    asynStatus getHVStatus();
};


#define NUM_PARAMS (&LAST_COMMAND - &FIRST_COMMAND + 1)

