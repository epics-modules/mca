/* mcaSISRegister.c */

#include "iocsh.h"
#include "epicsExport.h"
#include "epicsTypes.h"
#include "symTable.h"

#include "drvSTR7201.h"

extern int drvSTR7201Debug;
extern int devSTR7201Debug;

static const iocshArg STR7201SetupArg0 = { "maxCards",iocshArgInt};
static const iocshArg STR7201SetupArg1 = { "baseAddress",iocshArgInt};
static const iocshArg STR7201SetupArg2 = { "intVector",iocshArgInt};
static const iocshArg STR7201SetupArg3 = { "intLevel",iocshArgInt};
static const iocshArg * const STR7201SetupArgs[4] = {&STR7201SetupArg0,
                                                     &STR7201SetupArg1,
                                                     &STR7201SetupArg2,
                                                     &STR7201SetupArg3};
static const iocshFuncDef STR7201SetupFuncDef = {"STR7201Setup",4,STR7201SetupArgs};
static void STR7201SetupCallFunc(const iocshArgBuf *args)
{
    STR7201Setup(args[0].ival, args[1].ival,
                 args[2].ival, args[3].ival);
}

static const iocshArg STR7201ConfigArg0 = { "card",iocshArgInt};
static const iocshArg STR7201ConfigArg1 = { "maxSignals",iocshArgInt};
static const iocshArg STR7201ConfigArg2 = { "maxChans",iocshArgInt};
static const iocshArg STR7201ConfigArg3 = { "ch1RefEnable",iocshArgInt};
static const iocshArg * const STR7201ConfigArgs[4] = {&STR7201ConfigArg0,
                                                      &STR7201ConfigArg1,
                                                      &STR7201ConfigArg2,
                                                      &STR7201ConfigArg3};
static const iocshFuncDef STR7201ConfigFuncDef = {"STR7201Config",4,STR7201ConfigArgs};
static void STR7201ConfigCallFunc(const iocshArgBuf *args)
{
    STR7201Config(args[0].ival, args[1].ival,
                  args[2].ival, args[3].ival);
}

void mcaSISRegister(void)
{
    iocshRegister(&STR7201ConfigFuncDef,STR7201ConfigCallFunc);
    iocshRegister(&STR7201SetupFuncDef,STR7201SetupCallFunc);
    addSymbol("drvSTR7201Debug", &drvSTR7201Debug, epicsInt32T);
    addSymbol("devSTR7201Debug", &devSTR7201Debug, epicsInt32T);
}

epicsExportRegistrar(mcaSISRegister);
