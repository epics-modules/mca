/* mcaSISRegister.c */

#include "iocsh.h"
#include "epicsExport.h"

#include "drvSTR7201.h"
#include "drvMcaSIS3820Asyn.h"

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
static const iocshArg STR7201ConfigArg4 = { "softAdvance",iocshArgInt};
static const iocshArg * const STR7201ConfigArgs[5] = {&STR7201ConfigArg0,
                                                      &STR7201ConfigArg1,
                                                      &STR7201ConfigArg2,
                                                      &STR7201ConfigArg3,
                                                      &STR7201ConfigArg4};
static const iocshFuncDef STR7201ConfigFuncDef = {"STR7201Config",5,STR7201ConfigArgs};
static void STR7201ConfigCallFunc(const iocshArgBuf *args)
{
    STR7201Config(args[0].ival, args[1].ival,
                  args[2].ival, args[3].ival, args[4].ival);
}


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

void mcaSISRegister(void)
{
    iocshRegister(&STR7201ConfigFuncDef,STR7201ConfigCallFunc);
    iocshRegister(&STR7201SetupFuncDef,STR7201SetupCallFunc);
    iocshRegister(&mcaSIS3820ConfigFuncDef,mcaSIS3820ConfigCallFunc);
    iocshRegister(&mcaSIS3820SetupFuncDef,mcaSIS3820SetupCallFunc);
}

epicsExportRegistrar(mcaSISRegister);
