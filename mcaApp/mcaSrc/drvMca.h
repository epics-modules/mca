/* drvMca.h -- mca driver support */

#ifndef drvMcaH
#define drvMcaH

#include "mca.h"

typedef struct {
    mcaCommand command;
    char *commandString;
} mcaCommandStruct;

static mcaCommandStruct mcaCommands[MAX_MCA_COMMANDS] = {
    {mcaStartAcquire,           "START_ACQUIRE"}, /* int32, write */
    {mcaStopAcquire,            "STOP_ACQUIRE"},  /* int32, write */
    {mcaErase,                  "ERASE"},         /* int32, write */
    {mcaData,                   "DATA"},          /* int32Array, read/write */
    {mcaReadStatus,             "READ_STATUS"},   /* int32, write */
    {mcaChannelAdvanceInternal, "CHAS_INTERNAL"}, /* int32, write */
    {mcaChannelAdvanceExternal, "CHAS_EXTERNAL"}, /* int32, write */
    {mcaNumChannels,            "NUM_CHANNELS"},  /* int32, write */
    {mcaDwellTime,              "DWELL_TIME"},    /* float64, write */
    {mcaPresetLiveTime,         "PRESET_LIVE"},   /* float64, write */
    {mcaPresetRealTime,         "PRESET_REAL"},   /* float64, write */
    {mcaPresetCounts,           "PRESET_COUNTS"}, /* float64, write */
    {mcaPresetLowChannel,       "PRESET_LOW_CHANNEL"}, /* int32, write */
    {mcaPresetHighChannel,      "PRESET_HIGH_CHANNEL"}, /* int32, write */
    {mcaPresetSweeps,           "PRESET_SWEEPS"}, /* int32, write */
    {mcaModePHA,                "MODE_PHA"},      /* int32, write */
    {mcaModeMCS,                "MODE_MCS"},      /* int32, write */
    {mcaModeList,               "MODE_LIST"},     /* int32, write */
    {mcaSequence,               "SEQUENCE"},      /* int32, write */
    {mcaPrescale,               "PRESCALE"},      /* int32, write */
    {mcaAcquiring,              "ACQUIRING"},     /* int32, read */
    {mcaElapsedLiveTime,        "ELAPSED_LIVE"},  /* float64, read */
    {mcaElapsedRealTime,        "ELAPSED_REAL"},  /* float64, read */
    {mcaElapsedCounts,          "ELAPSED_COUNTS"} /* float64, read */
};

#endif /* drvMcaH */
