/* mcaCanberraRegister.c */

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "iocsh.h"
#include "epicsExport.h"

int AIMConfig(const char *, int, int , int, int, int, char *, int);

extern int aimDebug;
extern int mcaAIMServerDebug;

static const iocshArg AIMConfigArg0 = { "Server name",iocshArgString};
static const iocshArg AIMConfigArg1 = { "Address",iocshArgInt};
static const iocshArg AIMConfigArg2 = { "Port",iocshArgInt};
static const iocshArg AIMConfigArg3 = { "MaxChan",iocshArgInt};
static const iocshArg AIMConfigArg4 = { "MaxSign",iocshArgInt};
static const iocshArg AIMConfigArg5 = { "MaxSeq",iocshArgInt};
static const iocshArg AIMConfigArg6 = { "Eth. dev",iocshArgString};
static const iocshArg AIMConfigArg7 = { "QueSiz",iocshArgInt};
static const iocshArg * const AIMConfigArgs[8] = {&AIMConfigArg0,
                                                  &AIMConfigArg1,
                                                  &AIMConfigArg2,
                                                  &AIMConfigArg3,
                                                  &AIMConfigArg4,
                                                  &AIMConfigArg5,
                                                  &AIMConfigArg6,
                                                  &AIMConfigArg7};
static const iocshFuncDef AIMConfigFuncDef = {"AIMConfig",8,AIMConfigArgs};
static void AIMConfigCallFunc(const iocshArgBuf *args)
{
    AIMConfig(args[0].sval, (int) args[1].sval,(int) args[2].sval,(int) args[3].sval,
             (int) args[4].sval,(int) args[5].sval,args[6].sval,(int) args[7].sval);
}

static const iocshArg AIMDebugArg0 = { "aimDebug",iocshArgInt};
static const iocshArg AIMDebugArg1 = { "mcaAIMServerDebug",iocshArgInt};
static const iocshArg * const AIMDebugArgs[2] = {&AIMDebugArg0,
                                                 &AIMDebugArg1};
static const iocshFuncDef AIMDebugFuncDef = {"setAIMDebug",4,AIMDebugArgs};
static void AIMDebugCallFunc(const iocshArgBuf *args)
{
    aimDebug = (int)args[0].sval;
    mcaAIMServerDebug = (int)args[1].sval;
}

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
    nmc_freemodule((int)args[0].sval, (int)args[1].sval);
}


static const iocshArg ICBConfigArg0 = { "Server name",iocshArgString};
static const iocshArg ICBConfigArg1 = { "MaxModules",iocshArgInt};
static const iocshArg ICBConfigArg2 = { "Address",iocshArgString};
static const iocshArg ICBConfigArg3 = { "QueSiz",iocshArgInt};
static const iocshArg * const ICBConfigArgs[4] = {&ICBConfigArg0,
                                                  &ICBConfigArg1,
                                                  &ICBConfigArg2,
                                                  &ICBConfigArg3};

void *icbConfig(const char *serverName, int maxModules, char *address, int queueSize);
static const iocshFuncDef ICBConfigFuncDef = {"icbConfig",4,ICBConfigArgs};
static void ICBConfigCallFunc(const iocshArgBuf *args)
{
    icbConfig(args[0].sval, (int) args[1].sval, args[2].sval,(int) args[3].sval);
}

int icb_show_modules();
static const iocshFuncDef ICBShowModulesFuncDef = {"icbShowModules",0,0};
static void ICBShowModulesCallFunc(const iocshArgBuf *args)
{
    icb_show_modules();
}

extern int icbDebug;
static const iocshArg ICBDebugArg0 = { "debugLevel",iocshArgInt};
static const iocshArg * const ICBDebugArgs[1] = {&ICBDebugArg0};
static const iocshFuncDef ICBDebugFuncDef = {"setIcbDebug",1,ICBDebugArgs};
static void ICBDebugCallFunc(const iocshArgBuf *args)
{
    icbDebug = (int)args[0].sval;
}

static const iocshArg ICBTcaConfigArg0 = { "Server name",iocshArgString};
static const iocshArg ICBTcaConfigArg1 = { "MaxModules",iocshArgInt};
static const iocshArg ICBTcaConfigArg2 = { "Address",iocshArgString};
static const iocshArg ICBTcaConfigArg3 = { "QueSiz",iocshArgInt};
static const iocshArg * const ICBTcaConfigArgs[4] = {&ICBTcaConfigArg0,
                                                     &ICBTcaConfigArg1,
                                                     &ICBTcaConfigArg2,
                                                     &ICBTcaConfigArg3};

void *icbTcaConfig(const char *serverName, int maxModules, char *address, int queueSize);
static const iocshFuncDef ICBTcaConfigFuncDef = {"icbTcaConfig",4,ICBTcaConfigArgs};
static void ICBTcaConfigCallFunc(const iocshArgBuf *args)
{
    icbTcaConfig(args[0].sval, (int) args[1].sval, args[2].sval,(int) args[3].sval);
}

extern int icbTcaServerDebug;
static const iocshArg TCADebugArg0 = { "debugLevel",iocshArgInt};
static const iocshArg * const TCADebugArgs[1] = {&TCADebugArg0};
static const iocshFuncDef TCADebugFuncDef = {"setTcaDebug",1,TCADebugArgs};
static void TCADebugCallFunc(const iocshArgBuf *args)
{
    icbTcaServerDebug = (int)args[0].sval;
}

/* DSP 9660 commands */
static const iocshArg ICBDspConfigArg0 = { "Server name",iocshArgString};
static const iocshArg ICBDspConfigArg1 = { "MaxModules",iocshArgInt};
static const iocshArg ICBDspConfigArg2 = { "Address",iocshArgString};
static const iocshArg ICBDspConfigArg3 = { "QueSiz",iocshArgInt};
static const iocshArg * const ICBDspConfigArgs[4] = {&ICBDspConfigArg0,
                                                     &ICBDspConfigArg1,
                                                     &ICBDspConfigArg2,
                                                     &ICBDspConfigArg3};

void *icbDspConfig(const char *serverName, int maxModules, char *address, int queueSize);
static const iocshFuncDef ICBDspConfigFuncDef = {"icbDspConfig",4,ICBDspConfigArgs};
static void ICBDspConfigCallFunc(const iocshArgBuf *args)
{
    icbDspConfig(args[0].sval, (int) args[1].sval, args[2].sval,(int) args[3].sval);
}

extern int icbDspServerDebug;
static const iocshArg DSPDebugArg0 = { "debugLevel",iocshArgInt};
static const iocshArg * const DSPDebugArgs[1] = {&DSPDebugArg0};
static const iocshFuncDef DSPDebugFuncDef = {"setDspDebug",1,DSPDebugArgs};
static void DSPDebugCallFunc(const iocshArgBuf *args)
{
    icbDspServerDebug = (int)args[0].sval;
}

void mcaCanberraRegister(void)
{
    iocshRegister(&AIMConfigFuncDef,AIMConfigCallFunc);
    iocshRegister(&AIMDebugFuncDef,AIMDebugCallFunc);
    iocshRegister(&AIMShowModulesFuncDef,AIMShowModulesCallFunc);
    iocshRegister(&AIMFreeModuleFuncDef,AIMFreeModuleCallFunc);
    iocshRegister(&ICBConfigFuncDef,ICBConfigCallFunc);
    iocshRegister(&ICBShowModulesFuncDef,ICBShowModulesCallFunc);
    iocshRegister(&ICBDebugFuncDef,ICBDebugCallFunc);
    iocshRegister(&ICBTcaConfigFuncDef,ICBTcaConfigCallFunc);
    iocshRegister(&TCADebugFuncDef,TCADebugCallFunc);
    iocshRegister(&ICBDspConfigFuncDef,ICBDspConfigCallFunc);
    iocshRegister(&DSPDebugFuncDef,DSPDebugCallFunc);
}

epicsExportRegistrar(mcaCanberraRegister);
