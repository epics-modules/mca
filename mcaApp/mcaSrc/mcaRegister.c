/* mcaRegister.c */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "iocsh.h"
#include "epicsExport.h"

extern int mcaRecordDebug;
extern int devMcaMpfDebug;

static const iocshArg MCADebugArg0 = { "mcaRecordDebug",iocshArgInt};
static const iocshArg MCADebugArg1 = { "devMcaMpfDebug",iocshArgInt};
static const iocshArg * const MCADebugArgs[2] = {&MCADebugArg0,
                                                 &MCADebugArg1};
static const iocshFuncDef MCADebugFuncDef = {"setMcaDebug",2,MCADebugArgs};
static void MCADebugCallFunc(const iocshArgBuf *args)
{
    mcaRecordDebug = (int)args[0].sval;
    devMcaMpfDebug = (int)args[1].sval;
}

extern int fastSweepDebug;
static const iocshArg fastDebugArg0 = { "debugLevel",iocshArgInt};
static const iocshArg * const fastDebugArgs[1] = {&fastDebugArg0};
static const iocshFuncDef fastDebugFuncDef = {"setFastSweepDebug",1,fastDebugArgs};
static void fastDebugCallFunc(const iocshArgBuf *args)
{
    fastSweepDebug = (int)args[0].sval;
}

void mcaRegister(void)
{
    iocshRegister(&MCADebugFuncDef,MCADebugCallFunc);
    iocshRegister(&fastDebugFuncDef,fastDebugCallFunc);
}

epicsExportRegistrar(mcaRegister);
