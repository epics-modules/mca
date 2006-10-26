/* mcaSIS3820Register.c */

#include "iocsh.h"
#include "epicsExport.h"

#include "drvMcaSIS3820Asyn.h"

/* iocsh setup function */
static const iocshArg mcaSIS3820SetupArg0 = { "BaseAddress",iocshArgInt};
static const iocshArg mcaSIS3820SetupArg1 = { "InterruptVector",iocshArgInt};
static const iocshArg mcaSIS3820SetupArg2 = { "InterruptLevel",iocshArgInt};

static const iocshArg * const mcaSIS3820SetupArgs[3] = 
{ &mcaSIS3820SetupArg0,
  &mcaSIS3820SetupArg1,
  &mcaSIS3820SetupArg2};

static const iocshFuncDef mcaSIS3820SetupFuncDef =
{"mcaSIS3820Setup",3,mcaSIS3820SetupArgs};

static void mcaSIS3820SetupCallFunc(const iocshArgBuf *args)
{
  mcaSIS3820Setup(args[0].ival, args[1].ival, args[2].ival);
}

/* iocsh config function */
static const iocshArg mcaSIS3820ConfigArg0 = { "Asyn port name",iocshArgString};
static const iocshArg mcaSIS3820ConfigArg1 = { "Card",iocshArgInt};
static const iocshArg mcaSIS3820ConfigArg2 = { "MaxChan",iocshArgInt};
static const iocshArg mcaSIS3820ConfigArg3 = { "MaxSign",iocshArgInt};
static const iocshArg mcaSIS3820ConfigArg4 = { "InputMode",iocshArgInt};
static const iocshArg mcaSIS3820ConfigArg5 = { "OutputMode",iocshArgInt};
static const iocshArg mcaSIS3820ConfigArg6 = { "LNEPrescale",iocshArgInt};

static const iocshArg * const mcaSIS3820ConfigArgs[7] = 
{ &mcaSIS3820ConfigArg0,
  &mcaSIS3820ConfigArg1,
  &mcaSIS3820ConfigArg2,
  &mcaSIS3820ConfigArg3,
  &mcaSIS3820ConfigArg4,
  &mcaSIS3820ConfigArg5,
  &mcaSIS3820ConfigArg6 };

static const iocshFuncDef mcaSIS3820ConfigFuncDef =
{"mcaSIS3820Config",7,mcaSIS3820ConfigArgs};

static void mcaSIS3820ConfigCallFunc(const iocshArgBuf *args)
{
  mcaSIS3820Config(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
      args[4].ival, args[5].ival, args[6].ival);
}

void mcaSIS3820Register(void)
{
  iocshRegister(&mcaSIS3820ConfigFuncDef,mcaSIS3820ConfigCallFunc);
  iocshRegister(&mcaSIS3820SetupFuncDef,mcaSIS3820SetupCallFunc);
}

epicsExportRegistrar(mcaSIS3820Register);



