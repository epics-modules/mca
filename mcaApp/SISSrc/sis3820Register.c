/* mcaSIS3820Register.c */

#include "iocsh.h"
#include "epicsExport.h"

#include "drvSIS3820.h"

extern int drvSIS3820Debug;
extern int devSIS3820Debug;

static const iocshArg SIS3820SetupArg0 = { "numCards",iocshArgInt};
static const iocshArg SIS3820SetupArg1 = { "baseAddress",iocshArgInt};
static const iocshArg SIS3820SetupArg2 = { "intVector",iocshArgInt};
static const iocshArg SIS3820SetupArg3 = { "intLevel",iocshArgInt};
static const iocshArg * const SIS3820SetupArgs[4] = {&SIS3820SetupArg0,
                                                     &SIS3820SetupArg1,
                                                     &SIS3820SetupArg2,
                                                     &SIS3820SetupArg3};
static const iocshFuncDef SIS3820SetupFuncDef = {"SIS3820Setup",4,SIS3820SetupArgs};
static void SIS3820SetupCallFunc(const iocshArgBuf *args)
{
    SIS3820Setup(args[0].ival, args[1].ival,
                 args[2].ival, args[3].ival);
}

static const iocshArg SIS3820ConfigArg0 = { "board",iocshArgInt};
static const iocshArg SIS3820ConfigArg1 = { "maxSignals",iocshArgInt};
static const iocshArg SIS3820ConfigArg2 = { "maxChans",iocshArgInt};
static const iocshArg SIS3820ConfigArg3 = { "ch1RefEnable",iocshArgInt};
static const iocshArg SIS3820ConfigArg4 = { "softAdvance",iocshArgInt};
static const iocshArg SIS3820ConfigArg5 = { "inputMode",iocshArgInt};
static const iocshArg SIS3820ConfigArg6 = { "outputMode",iocshArgInt};
static const iocshArg SIS3820ConfigArg7 = { "lnePrescale",iocshArgInt};

static const iocshArg * const SIS3820ConfigArgs[8] = {&SIS3820ConfigArg0,
  &SIS3820ConfigArg1, &SIS3820ConfigArg2, &SIS3820ConfigArg3,
  &SIS3820ConfigArg4, &SIS3820ConfigArg5, &SIS3820ConfigArg6, 
  &SIS3820ConfigArg7};

static const iocshFuncDef SIS3820ConfigFuncDef = {"SIS3820Config",8,SIS3820ConfigArgs};

static void SIS3820ConfigCallFunc(const iocshArgBuf *args)
{
    SIS3820Config(args[0].ival, args[1].ival,
                  args[2].ival, args[3].ival, 
                  args[4].ival, args[5].ival, 
                  args[6].ival, args[7].ival);
}

void mcaSIS3820Register(void)
{
    iocshRegister(&SIS3820ConfigFuncDef,SIS3820ConfigCallFunc);
    iocshRegister(&SIS3820SetupFuncDef,SIS3820SetupCallFunc);
}

epicsExportRegistrar(mcaSIS3820Register);
