/* mcaCanberraRegister.c */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <iocsh.h>
#include <epicsExport.h>
#include <epicsTypes.h>

#include "symTable.h"

extern int aimDebug;
extern int mcaAIMServerDebug;
extern int icbDebug;

int nmc_show_modules();
int nmc_freemodule(int, int);
static const iocshFuncDef AIMShowModulesFuncDef = {"mcaAIMShowModules",0,0};
static void AIMShowModulesCallFunc(const iocshArgBuf *args)
{
    nmc_show_modules();
}

static const iocshArg AIMFreeArg0 = { "module #",iocshArgInt};
static const iocshArg AIMFreeArg1 = { "override flag",iocshArgInt};
static const iocshArg * const AIMFreeArgs[4] = {&AIMFreeArg0,
                                                &AIMFreeArg1};
static const iocshFuncDef AIMFreeModuleFuncDef = {"mcaAIMFreeModule",2,AIMFreeArgs};
static void AIMFreeModuleCallFunc(const iocshArgBuf *args)
{
    nmc_freemodule(args[0].ival, args[1].ival);
}


int icb_show_modules();
static const iocshFuncDef ICBShowModulesFuncDef = {"icbShowModules",0,0};
static void ICBShowModulesCallFunc(const iocshArgBuf *args)
{
    icb_show_modules();
}

void mcaCanberraRegister(void)
{
    addSymbol("icbDebug", &icbDebug, epicsInt32T);
    addSymbol("aimDebug", &aimDebug, epicsInt32T);
    addSymbol("mcaAIMServerDebug", &mcaAIMServerDebug, epicsInt32T);
    iocshRegister(&AIMShowModulesFuncDef,AIMShowModulesCallFunc);
    iocshRegister(&AIMFreeModuleFuncDef,AIMFreeModuleCallFunc);
    iocshRegister(&ICBShowModulesFuncDef,ICBShowModulesCallFunc);
}

epicsExportRegistrar(mcaCanberraRegister);
