/* mcaSISRegister.c */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "iocsh.h"
#include "epicsExport.h"

#include "drvSTR7201.h"

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
    STR7201Setup((int) args[0].sval, (int) args[1].sval,
                 (int) args[2].sval, (int) args[3].sval);
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
    STR7201Config((int) args[0].sval, (int) args[1].sval,
                  (int) args[2].sval, (int) args[3].sval);
}

extern int drvSTR7201Debug;
static const iocshArg drvSTR7201DebugArg0 = { "debugLevel",iocshArgInt};
static const iocshArg * const drvSTR7201DebugArgs[1] = {&drvSTR7201DebugArg0};
static const iocshFuncDef drvSTR7201DebugFuncDef = {"setdrvStr7201Debug",1,drvSTR7201DebugArgs};
static void drvSTR7201DebugCallFunc(const iocshArgBuf *args)
{
    drvSTR7201Debug = (int)args[0].sval;
}

extern int devSTR7201Debug;
static const iocshArg devSTR7201DebugArg0 = { "debugLevel",iocshArgInt};
static const iocshArg * const devSTR7201DebugArgs[1] = {&devSTR7201DebugArg0};
static const iocshFuncDef devSTR7201DebugFuncDef = {"setdevStr7201Debug",1,devSTR7201DebugArgs};
static void devSTR7201DebugCallFunc(const iocshArgBuf *args)
{
    devSTR7201Debug = (int)args[0].sval;
}

void mcaSISRegister(void)
{
    iocshRegister(&STR7201ConfigFuncDef,STR7201ConfigCallFunc);
    iocshRegister(&STR7201SetupFuncDef,STR7201SetupCallFunc);
    iocshRegister(&drvSTR7201DebugFuncDef,drvSTR7201DebugCallFunc);
    iocshRegister(&devSTR7201DebugFuncDef,devSTR7201DebugCallFunc);
}

epicsExportRegistrar(mcaSISRegister);
