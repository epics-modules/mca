/* drvMcaAsyn.h */

#ifndef drvMcaAsynH
#define drvMcaAsynH

#include <asynDriver.h>
#include <mca.h>

/* Interface supported by mcaAsyn drivers. */
#define asynMcaType "asynMca"
typedef struct {
    asynStatus (*command)(void *drvPvt, asynUser *pasynUser, 
                          mcaCommand command, int ivalue, double dvalue);
    asynStatus (*readStatus)(void *drvPvt, asynUser *pasynUser,
                             mcaAsynAcquireStatus *status);
    asynStatus (*readData)(void *drvPvt, asynUser *pasynUser, int maxChans, 
                           int *nactual, int *data);
} asynMca;

#endif /* drvMcaAsynH */

