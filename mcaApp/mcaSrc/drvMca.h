/* drvMca.h -- mca driver support */

#ifndef drvMcaH
#define drvMcaH

#include "mca.h"

typedef struct {
    mcaCommand command;
    const char *commandString;
} mcaCommandStruct;

static mcaCommandStruct mcaCommands[MAX_MCA_COMMANDS] = {
    {mcaStartAcquire,           "MCA_START_ACQUIRE"}, /* int32, write */
    {mcaStopAcquire,            "MCA_STOP_ACQUIRE"},  /* int32, write */
    {mcaErase,                  "MCA_ERASE"},         /* int32, write */
    {mcaData,                   "MCA_DATA"},          /* int32Array, read/write */
    {mcaReadStatus,             "MCA_READ_STATUS"},   /* int32, write */
    {mcaChannelAdvanceInternal, "MCA_CHAS_INTERNAL"}, /* int32, write */
    {mcaChannelAdvanceExternal, "MCA_CHAS_EXTERNAL"}, /* int32, write */
    {mcaNumChannels,            "MCA_NUM_CHANNELS"},  /* int32, write */
    {mcaDwellTime,              "MCA_DWELL_TIME"},    /* float64, write */
    {mcaPresetLiveTime,         "MCA_PRESET_LIVE"},   /* float64, write */
    {mcaPresetRealTime,         "MCA_PRESET_REAL"},   /* float64, write */
    {mcaPresetCounts,           "MCA_PRESET_COUNTS"}, /* float64, write */
    {mcaPresetLowChannel,       "MCA_PRESET_LOW_CHANNEL"}, /* int32, write */
    {mcaPresetHighChannel,      "MCA_PRESET_HIGH_CHANNEL"}, /* int32, write */
    {mcaPresetSweeps,           "MCA_PRESET_SWEEPS"}, /* int32, write */
    {mcaModePHA,                "MCA_MODE_PHA"},      /* int32, write */
    {mcaModeMCS,                "MCA_MODE_MCS"},      /* int32, write */
    {mcaModeList,               "MCA_MODE_LIST"},     /* int32, write */
    {mcaSequence,               "MCA_SEQUENCE"},      /* int32, write */
    {mcaPrescale,               "MCA_PRESCALE"},      /* int32, write */
    {mcaAcquiring,              "MCA_ACQUIRING"},     /* int32, read */
    {mcaElapsedLiveTime,        "MCA_ELAPSED_LIVE"},  /* float64, read */
    {mcaElapsedRealTime,        "MCA_ELAPSED_REAL"},  /* float64, read */
    {mcaElapsedCounts,          "MCA_ELAPSED_COUNTS"} /* float64, read */
};

#endif /* drvMcaH */
