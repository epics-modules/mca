/* drvMca.h -- mca driver support */

#ifndef drvMcaH
#define drvMcaH

#define mcaStartAcquireString           "MCA_START_ACQUIRE" /* int32, write */
#define mcaStopAcquireString            "MCA_STOP_ACQUIRE"  /* int32, write */
#define mcaEraseString                  "MCA_ERASE"         /* int32, write */
#define mcaDataString                   "MCA_DATA"          /* int32Array, read/write */
#define mcaReadStatusString             "MCA_READ_STATUS"   /* int32, write */
#define mcaChannelAdvanceInternalString "MCA_CHAS_INTERNAL" /* int32, write */
#define mcaChannelAdvanceExternalString "MCA_CHAS_EXTERNAL" /* int32, write */
#define mcaNumChannelsString            "MCA_NUM_CHANNELS"  /* int32, write */
#define mcaDwellTimeString              "MCA_DWELL_TIME"    /* float64, write */
#define mcaPresetLiveTimeString         "MCA_PRESET_LIVE"   /* float64, write */
#define mcaPresetRealTimeString         "MCA_PRESET_REAL"   /* float64, write */
#define mcaPresetCountsString           "MCA_PRESET_COUNTS" /* float64, write */
#define mcaPresetLowChannelString       "MCA_PRESET_LOW_CHANNEL" /* int32, write */
#define mcaPresetHighChannelString      "MCA_PRESET_HIGH_CHANNEL" /* int32, write */
#define mcaPresetSweepsString           "MCA_PRESET_SWEEPS" /* int32, write */
#define mcaModePHAString                "MCA_MODE_PHA"      /* int32, write */
#define mcaModeMCSString                "MCA_MODE_MCS"      /* int32, write */
#define mcaModeListString               "MCA_MODE_LIST"     /* int32, write */
#define mcaSequenceString               "MCA_SEQUENCE"      /* int32, write */
#define mcaPrescaleString               "MCA_PRESCALE"      /* int32, write */
#define mcaAcquiringString              "MCA_ACQUIRING"     /* int32, read */
#define mcaElapsedLiveTimeString        "MCA_ELAPSED_LIVE"  /* float64, read */
#define mcaElapsedRealTimeString        "MCA_ELAPSED_REAL"  /* float64, read */
#define mcaElapsedCountsString          "MCA_ELAPSED_COUNTS" /* float64, read */

#endif /* drvMcaH */
