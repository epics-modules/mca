/* asynMca.h */

#ifndef asynMcaH
#define asynMcaH

#include <asynDriver.h>
#include <mca.h>

/* Interface supported by mcaAsyn drivers. */
#define asynMcaType "asynMca"
typedef struct {
    asynStatus (*command)(void *drvPvt, asynUser *pasynUser, 
                          mcaCommand command, int ivalue, double dvalue);
    asynStatus (*readStatus)(void *drvPvt, asynUser *pasynUser,
                             mcaAsynAcquireStatus *status);
} asynMca;

#endif /* asynMcaH */

