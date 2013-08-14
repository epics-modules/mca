/*
 * DSA2000.cpp
 * 
 * Asyn driver that inherits from the asynPortDriver class to control the Canberra DSA-2000.
 *
 * Currently this only controls the High Voltage Power Suppy, but it could easily be enhanced to support
 * the sample changer, pulser, etc.
 *
 * This driver must be intialized after an AIMConfig command has been done for this DSA2000 in the IOC.
 *
 * Author: Mark Rivers
 *
 * Created August 9, 2013
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <iocsh.h>

#include "nmc_sys_defs.h"
#include "DSA2000.h"
#include <epicsExport.h>

static const char *driverName="DSA2000";

typedef enum {
    rangePlus1300,
    rangePlus5000,
    rangeMinus5000
} DSA2000_HVPS_Range_t;

#define HVPS_Negative 0x2
#define HVPS_5000V    0x4


/** Constructor for the DSA2000 class.
  * Calls constructor for the asynPortDriver base class.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] moduleAddress The low-order 16 bits of the Ethernet address */
DSA2000::DSA2000(const char *portName, int moduleAddress) 
   : asynPortDriver(portName, 
                    1, /* maxAddr */ 
                    (int)NUM_PARAMS,
                    asynInt32Mask | asynFloat64Mask | asynUInt32DigitalMask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynFloat64Mask | asynUInt32DigitalMask,  /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver blocks and it is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0) /* Default stack size*/    
{
    unsigned char enet_address[6];
    int status;
    epicsUInt32 HVStatus, HVControl;
    double voltage;
    int range;
    int retries=0;
    const char *functionName = "DSA2000";

    createParam(P_HVControlString,          asynParamUInt32Digital, &P_HVControl);
    createParam(P_HVStatusString,           asynParamUInt32Digital, &P_HVStatus);
    createParam(P_HVRangeSettingString,     asynParamInt32,         &P_HVRangeSetting);
    createParam(P_HVRangeReadbackString,    asynParamInt32,         &P_HVRangeReadback);
    createParam(P_DACSettingString,         asynParamFloat64,       &P_DACSetting);
    createParam(P_DACReadbackString,        asynParamFloat64,       &P_DACReadback);
    createParam(P_ADCReadbackString,        asynParamFloat64,       &P_ADCReadback);
    createParam(P_HVResetString,            asynParamInt32,         &P_HVReset);
    createParam(P_ReadStatusString,         asynParamInt32,         &P_ReadStatus);
    
    /* Compute the module Ethernet address */
    nmc_build_enet_addr(moduleAddress, enet_address);

    /* Find the particular module on the net - may have to retry in case
     * the module database is still being built after initialisation.
     * This is not possible to resolve other than by waiting, since there are
     * an unknown number of modules on the local subnet, and they can reply
     * to the initial inquiry broadcast in an arbitrary order. */
    do {
      epicsThreadSleep(0.1);
      status = nmc_findmod_by_addr(&this->module, enet_address);
    } while ((status != OK) && (retries++ < 5));

    if (status != OK) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
          "%s:%s: ERROR, cannot find module %d on the network!\n",
          driverName, functionName, moduleAddress);
        return;
    }
    
    /* Read the status of the module */
    getHVStatus();
    
    /* Set the value of the output parameters from the initial readbacks */
    getUIntDigitalParam(P_HVStatus, &HVStatus, 0xFFFFFFFF);
    // Force callbacks on all bits first time
    setUIntDigitalParam(P_HVStatus, HVStatus, 0xFFFFFFFF, 0xFFFFFFFF);
    // Copy bits 0-2 and 9 of status to control
    HVControl = HVStatus & 0x207;
    setUIntDigitalParam(P_HVControl, HVControl, 0xFFFFFFFF, 0xFFFFFFFF);
    getDoubleParam(P_DACReadback, &voltage);
    setDoubleParam(P_DACSetting, voltage);
    getIntegerParam(P_HVRangeReadback, &range);
    setIntegerParam(P_HVRangeSetting, range);
}



/** Called when asyn clients call pasynInt32->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus DSA2000::writeUInt32Digital(asynUser *pasynUser, epicsUInt32 value, epicsUInt32 mask)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName;
    const char* functionName = "writeUInt32Digital";

    /* Set the parameter in the parameter library. */
    setUIntDigitalParam(function, value, mask);
    
    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    if (function == P_HVControl) {
      status = setHVStatus();
    } 
    else {
        /* All other parameters just get set in parameter list, no need to
         * act on them here */
    }
    
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=0x%x, mask=0x%x", 
                  driverName, functionName, status, function, paramName, value, mask);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=0x%x, mask=0x%x\n", 
              driverName, functionName, function, paramName, value, mask);
    return status;
}

/** Called when asyn clients call pasynInt32->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus DSA2000::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    struct ncp_hcmd_resethvstatus resetCommand;
    int sendStatus;
    int response;
    int actual;
    const char *paramName;
    const char* functionName = "writeInt32";

    /* Set the parameter in the parameter library. */
    setIntegerParam(function, value);
    
    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    if (function == P_HVRangeSetting) {
        /* Set the range */
        status = setHVStatus();
    }
    if (function == P_HVReset) {
        /* Send the HV reset command */
        sendStatus = nmc_sendcmd(this->module, NCP_K_HCMD_RESETHVSTATUS, &resetCommand, sizeof(resetCommand),
                                 &response, sizeof(response), &actual, 0);
        if (sendStatus != 9) status = asynError;
    }
    else if (function == P_ReadStatus) {
        status = getHVStatus();
    }
    else {
        /* All other parameters just get set in parameter list, no need to
         * act on them here */
    }
    
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%d", 
                  driverName, functionName, status, function, paramName, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%d\n", 
              driverName, functionName, function, paramName, value);
    return status;
}

/** Called when asyn clients call pasynFloat64->write().
  * For all parameters it sets the value in the parameter library and calls any registered callbacks..
  * \param[in] pasynUser pasynUser structure that encodes the reason and address.
  * \param[in] value Value to write. */
asynStatus DSA2000::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int function = pasynUser->reason;
    asynStatus status = asynSuccess;
    const char *paramName;
    const char* functionName = "writeInt32";

    /* Set the parameter in the parameter library. */
    setDoubleParam(function, value);
    
    /* Fetch the parameter string name for possible use in debugging */
    getParamName(function, &paramName);

    if (function == P_DACSetting) {
        status = setHVStatus();
    } 
    else {
        /* All other parameters just get set in parameter list, no need to
         * act on them here */
    }
    
    /* Do callbacks so higher layers see any changes */
    callParamCallbacks();
    
    if (status) 
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize, 
                  "%s:%s: status=%d, function=%d, name=%s, value=%f", 
                  driverName, functionName, status, function, paramName, value);
    else        
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER, 
              "%s:%s: function=%d, name=%s, value=%f\n", 
              driverName, functionName, function, paramName, value);
    return status;
}

asynStatus DSA2000::setHVStatus()
{
    struct ncp_hcmd_sethvstatus setCommand;
    epicsUInt32 control;
    asynStatus status = asynSuccess;
    int sendStatus;
    int range;
    double DACVolts, fullScale=0;;
    int response;
    int actual;
    static const char *functionName = "setHVStatus";
    
    getUIntDigitalParam(P_HVControl, &control, 0xFFFFFFFF);
    getIntegerParam(P_HVRangeSetting, &range);
    getDoubleParam(P_DACSetting, &DACVolts);
    switch (range) {
        case  rangePlus5000:
            control &= ~HVPS_Negative;
            control |= HVPS_5000V;
            fullScale = 5000.;
            break;
        case rangePlus1300:
            control &= ~HVPS_Negative;
            control &= ~HVPS_5000V;
            fullScale = 1300.;
            break;
        case rangeMinus5000:
            control |= HVPS_Negative;
            control |= HVPS_5000V;
            fullScale = 5000.;
            break;
        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s:%s: error, unknown range=%d\n",
                driverName, functionName, range);
            break;
    }
    setUIntDigitalParam(P_HVControl, control, 0xFFFFFFFF);
    setCommand.control = control;   
    setCommand.DAC = (epicsInt16)((DACVolts/fullScale * 4095) + 0.5);
    sendStatus = nmc_sendcmd(this->module, NCP_K_HCMD_SETHVSTATUS, &setCommand, sizeof(setCommand),
                             &response, sizeof(response), &actual, 0);
    if (sendStatus != 9) status = asynError;
    return status;
}

asynStatus DSA2000::getHVStatus()
{
    struct ncp_hcmd_rethvstatus getCommand;
    struct ncp_mresp_rethvstatus getResponse;
    epicsUInt32 HVStatus;
    asynStatus status = asynSuccess;
    int sendStatus;
    int range;
    double DACVolts, ADCVolts, fullScale;
    int actual;
    
    sendStatus = nmc_sendcmd(this->module, NCP_K_HCMD_RETHVSTATUS, &getCommand, sizeof(getCommand),
                             &getResponse, sizeof(getResponse), &actual, 0);
    if (sendStatus != 233) status = asynError;
    HVStatus = getResponse.status;
    setUIntDigitalParam(P_HVStatus, HVStatus, 0xFFFFFFFF);
    if (HVStatus & HVPS_Negative) { 
        fullScale = 5000.;
        range = rangeMinus5000;
    } else {
        if (HVStatus & HVPS_5000V) {
            fullScale = 5000;
            range = rangePlus5000;
        } else {
            fullScale = 1300.;
            range = rangePlus1300;
        }
    }
    setIntegerParam(P_HVRangeReadback, range);
    DACVolts = getResponse.DACValue * fullScale / 4095.;
    setDoubleParam(P_DACReadback, DACVolts);
    ADCVolts = getResponse.ADCValue * fullScale / 255.;
    setDoubleParam(P_ADCReadback, ADCVolts);
    callParamCallbacks();
    
    return status;
}


/* Configuration routine.  Called directly, or from the iocsh function below */

extern "C" {

/** EPICS iocsh callable function to call constructor for the DSA2000 class.
  * \param[in] portName The name of the asyn port driver to be created.
  * \param[in] moduleAddress The low-order 16-bit Ethernet address of the DSA2000 */
int DSA2000Config(const char *portName, int moduleAddress)
{
    new DSA2000(portName, moduleAddress);
    return(asynSuccess);
}


/* EPICS iocsh shell commands */

static const iocshArg AIMConfigArg0 = { "Asyn port name",iocshArgString};
static const iocshArg AIMConfigArg1 = { "Module address",iocshArgInt};
static const iocshArg * const DSA2000ConfigArgs[] = {&AIMConfigArg0,
                                                          &AIMConfigArg1};
static const iocshFuncDef DSA2000ConfigFuncDef = {"DSA2000Config",2,DSA2000ConfigArgs};
static void DSA2000ConfigCallFunc(const iocshArgBuf *args)
{
    DSA2000Config(args[0].sval, args[1].ival);
}

void DSA2000Register(void)
{
    iocshRegister(&DSA2000ConfigFuncDef,DSA2000ConfigCallFunc);
}

epicsExportRegistrar(DSA2000Register);
 
}
