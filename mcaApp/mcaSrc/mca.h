/* mca.h -- 
 * Parameters for communication between mca record and mca device support.
 * NOTE: These parameters must not be used by device support
 * to set pasynUser->reason for asyn drivers.  
 * Only the strings defined in drvMca.h should be used by device support, which must use the
 * asynDrvUser->create() function to fetch the pasynUser->reason.
 * Drivers that only implement the mca interface are free to use these values for pasynUser->reason,
 * but they are not required to do so.
 */

#ifndef mcaH
#define mcaH

typedef enum {
    mcaData,                   /* int32Array, write/read */
    mcaStartAcquire,           /* int32, write */
    mcaStopAcquire,            /* int32, write */
    mcaErase,                  /* int32, write */
    mcaReadStatus,             /* int32, write */
    mcaChannelAdvanceSource,   /* int32, write */
    mcaNumChannels,            /* int32, write */
    mcaAcquireMode,            /* int32, write */
    mcaSequence,               /* int32, write */
    mcaPrescale,               /* int32, write */
    mcaPresetSweeps,           /* int32, write */
    mcaPresetLowChannel,       /* int32, write */
    mcaPresetHighChannel,      /* int32, write */
    mcaDwellTime,              /* float64, write/read */
    mcaPresetLiveTime,         /* float64, write */
    mcaPresetRealTime,         /* float64, write */
    mcaPresetCounts,           /* float64, write */
    mcaAcquiring,              /* int32, read */
    mcaElapsedLiveTime,        /* float64, read */
    mcaElapsedRealTime,        /* float64, read */
    mcaElapsedCounts,          /* float64, read */
    lastMcaCommand
} mcaCommand;

#define MAX_MCA_COMMANDS lastMcaCommand

typedef struct {
    int acquiring;
    double elapsedReal;
    double elapsedLive;
    double totalCounts;
    double dwellTime;
    double deadTime;
} mcaStatus;
    
#endif /* mcaH */
