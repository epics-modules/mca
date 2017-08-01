/* File:    drvAmptek.cpp
 * Author:  Mark Rivers, University of Chicago
 * Date:    24-July-2017
 *
 * Purpose: 
 * This module provides the driver support for the MCA asyn device support layer
 * for Amptek D5 based MCAs.
 *
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsTime.h>
#include <epicsTypes.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <iocsh.h>
#include <cantProceed.h>

#include <drvAsynIPPort.h>
#include <asynOctetSyncIO.h>

#include <epicsExport.h>
#include <drvMca.h>
#include <drvAmptek.h>

#define BROADCAST_TIMEOUT 0.2
#define COMMAND_PORT    10001
#define BROADCAST_PORT  3040

static const char *driverName = "drvAmptek";

drvAmptek::drvAmptek(const char *portName, int interfaceType, const char *addressInfo)
   : asynPortDriver(portName, 
                    1, /* Maximum address */
                    0, /* Unused, number of parameters */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask,                   /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver can block and is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0) /* Default stack size*/

{
    const char *functionName = "drvAmptek";
    
    asynStatus status;

    // Uncomment this line to enable asynTraceFlow during the constructor
    //pasynTrace->setTraceMask(pasynUserSelf, 0x11);

    createParam(mcaStartAcquireString,                asynParamInt32, &mcaStartAcquire_);
    createParam(mcaStopAcquireString,                 asynParamInt32, &mcaStopAcquire_);            /* int32, write */
    createParam(mcaEraseString,                       asynParamInt32, &mcaErase_);                  /* int32, write */
    createParam(mcaDataString,                        asynParamInt32, &mcaData_);                   /* int32Array, read/write */
    createParam(mcaReadStatusString,                  asynParamInt32, &mcaReadStatus_);             /* int32, write */
    createParam(mcaChannelAdvanceSourceString,        asynParamInt32, &mcaChannelAdvanceSource_);   /* int32, write */
    createParam(mcaNumChannelsString,                 asynParamInt32, &mcaNumChannels_);            /* int32, write */
    createParam(mcaDwellTimeString,                 asynParamFloat64, &mcaDwellTime_);              /* float64, write */
    createParam(mcaPresetLiveTimeString,            asynParamFloat64, &mcaPresetLiveTime_);         /* float64, write */
    createParam(mcaPresetRealTimeString,            asynParamFloat64, &mcaPresetRealTime_);         /* float64, write */
    createParam(mcaPresetCountsString,              asynParamFloat64, &mcaPresetCounts_);           /* float64, write */
    createParam(mcaPresetLowChannelString,            asynParamInt32, &mcaPresetLowChannel_);       /* int32, write */
    createParam(mcaPresetHighChannelString,           asynParamInt32, &mcaPresetHighChannel_);      /* int32, write */
    createParam(mcaPresetSweepsString,                asynParamInt32, &mcaPresetSweeps_);           /* int32, write */
    createParam(mcaAcquireModeString,                 asynParamInt32, &mcaAcquireMode_);            /* int32, write */
    createParam(mcaSequenceString,                    asynParamInt32, &mcaSequence_);               /* int32, write */
    createParam(mcaPrescaleString,                    asynParamInt32, &mcaPrescale_);               /* int32, write */
    createParam(mcaAcquiringString,                   asynParamInt32, &mcaAcquiring_);              /* int32, read */
    createParam(mcaElapsedLiveTimeString,           asynParamFloat64, &mcaElapsedLiveTime_);        /* float64, read */
    createParam(mcaElapsedRealTimeString,           asynParamFloat64, &mcaElapsedRealTime_);        /* float64, read */
    createParam(mcaElapsedCountsString,             asynParamFloat64, &mcaElapsedCounts_);          /* float64, read */

    interfaceType_ = (amptekInterface_t)interfaceType;
    switch(interfaceType_) {
        case amptekInterfaceEthernet:
        case amptekInterfaceUSB:
        case amptekInterfaceSerial:
        break;
        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s unknown interface type=%d\n", 
                driverName, functionName, interfaceType_);
            return;
    }
    addressInfo_ = epicsStrDup(addressInfo);
    
    status = connectDevice();
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s cannot connect to device on interface type=%d addressInfo=%s, status=%d\n",
            driverName, functionName, interfaceType_, addressInfo_, status);
    }
}

asynStatus drvAmptek::connectDevice()
{
    static const char *functionName = "connectDevice";

    isConnected_ = false;
    switch(interfaceType_) {
        case amptekInterfaceEthernet:
            if (consoleHelper.DppSocket_Connect_Direct_DPP(addressInfo_)) {
                asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
                    "%s::%s Network DPP device %s connected, total devices found=%d\n",
                    driverName, functionName, addressInfo_, consoleHelper.DppSocket_NumDevices);
                isConnected_ = true;
          	} else {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s ERROR: Network DPP device %s not found, total devices found=%d\n",
                    driverName, functionName, addressInfo_, consoleHelper.DppSocket_NumDevices);
                isConnected_ = false;
          	}

            break;

        case amptekInterfaceUSB:
            break;
        
        case amptekInterfaceSerial:
            break;
    }
    consoleHelper.DP5Stat.m_DP5_Status.SerialNumber = 0;
    if (consoleHelper.DppSocket_SendCommand(XMTPT_SEND_STATUS) == false) {	// request status
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling SendCommand for XMTPT_SEND_STATUS\n",
            driverName, functionName);
        return asynError;
    }
    if (consoleHelper.DppSocket_ReceiveData() == false) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling ReceiveData() for XMTPT_SEND_STATUS\n",
            driverName, functionName);
        return asynError;
    }
    readConfigurationFromHardware();
    
    return asynSuccess;

}

asynStatus drvAmptek::sendCommand(TRANSMIT_PACKET_TYPE command)
{
    static const char *functionName = "sendCommand";
    bool status=true;
    
    switch(interfaceType_) {
        case amptekInterfaceEthernet:
		        status = consoleHelper.DppSocket_SendCommand(command);
            break;
        
        case amptekInterfaceUSB:
            break;
        
        case amptekInterfaceSerial:
            break;
    }
    if (status == false) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling consoleHelper.DppSocket_SendCommand(%d)\n",
            driverName, functionName, command);
        return asynError;
    }
    return asynSuccess;
}

asynStatus drvAmptek::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;

    /* Set the parameter in the parameter library. */
    status = setIntegerParam(command, value);
    if (command == mcaStartAcquire_) {
        if (!acquiring_) {
            acquiring_ = true;
		        status = sendCommand(XMTPT_ENABLE_MCA_MCS);
            setIntegerParam(mcaAcquiring_, acquiring_);
        }
    }  
    else if (command == mcaStopAcquire_) {
        status = sendCommand(XMTPT_DISABLE_MCA_MCS);
    }
    else if (command == mcaErase_) {
        status = sendCommand(XMTPT_SEND_CLEAR_SPECTRUM_STATUS);
        memset(pData_, 0, numChannels_ * sizeof(epicsInt32));
    }
    else if (command == mcaReadStatus_) {
        status = sendCommand(XMTPT_SEND_STATUS);
			  consoleHelper.DppSocket_ReceiveData();
    }
    else if (command == mcaNumChannels_) {
        if ((value >0) && (value != (int)numChannels_)) {
            numChannels_ = value;
            if (pData_) free(pData_);
            pData_ = (epicsInt32 *)calloc(numChannels_, sizeof(epicsInt32));
        }
    }
    callParamCallbacks();
    return status;
}

asynStatus drvAmptek::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    static const char *functionName = "readInt32";

    if (command == mcaAcquiring_) {
        *value = consoleHelper.DP5Stat.m_DP5_Status.MCA_EN;
        acquiring_ = *value;
    }
    else {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::%s got illegal command %d\n",
            driverName, functionName, command);
        status = asynError;
    }
    return status;
}

asynStatus drvAmptek::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    static const char *functionName = "readFloat64";

    if (command == mcaDwellTime_) {
        *value = 0.;
    }
    else if (command == mcaElapsedLiveTime_) {
        *value = consoleHelper.DP5Stat.m_DP5_Status.AccumulationTime;
    }
    else if (command == mcaElapsedRealTime_) {
        *value = consoleHelper.DP5Stat.m_DP5_Status.RealTime;
    }
    else if (command == mcaElapsedCounts_) {
        *value = consoleHelper.DP5Stat.m_DP5_Status.SlowCount;
    }
    else {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
            "%s::%s got illegal command %d\n",
            driverName, functionName, command);
        status = asynError;
    }
    return status;
}


asynStatus drvAmptek::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int command = pasynUser->reason;
    CONFIG_OPTIONS CfgOptions;
    asynStatus status=asynSuccess;
    char tempString[20];

    /* Set the parameter in the parameter library. */
    status = setDoubleParam(command, value);
    if ((command == mcaPresetRealTime_) || 
        (command == mcaPresetLiveTime_)) {
        consoleHelper.CreateConfigOptions(&CfgOptions, "", consoleHelper.DP5Stat, false);
        sprintf(tempString, "%s =%f", command == mcaPresetRealTime_ ? "PREL" : "PRET", value);
        CfgOptions.HwCfgDP5Out = tempString;
printf("drvAmptek::writeFloat64 CfgOptions.HwCfgDP5Out=%s\n", CfgOptions.HwCfgDP5Out.c_str());
epicsThreadSleep(1.0);
        consoleHelper.DppSocket_SendCommand_Config(XMTPT_SEND_CONFIG_PACKET_EX, CfgOptions);
        readConfigurationFromHardware();
    }
    callParamCallbacks();
    return status;
}

asynStatus drvAmptek::readInt32Array(asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans, 
                                     size_t *nactual)
{
    int numChannels;
    int i;
    static const char *functionName="readInt32Array";

    sendCommand(XMTPT_SEND_SPECTRUM_STATUS);
    if (consoleHelper.DppSocket_ReceiveData() == false) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling ReceiveData() for XMTPT_SEND_SPECTRUM_STATUS\n",
            driverName, functionName);
        *nactual = 0;
        return asynError;
    }
    numChannels = consoleHelper.DP5Proto.SPECTRUM.CHANNELS;
    if (numChannels > (int)maxChans) numChannels = maxChans;
 printf("numChannels=%d, channel 1000=%ld\n", numChannels, consoleHelper.DP5Proto.SPECTRUM.DATA[1000]);
    for (i=0; i<numChannels; i++) {
        data[i] = consoleHelper.DP5Proto.SPECTRUM.DATA[i];
    }
    *nactual = numChannels;
    return asynSuccess;
}

asynStatus drvAmptek::readConfigurationFromHardware()
{
    static const char *functionName="readConfigurationFromHardware";
    CONFIG_OPTIONS CfgOptions;
 
    consoleHelper.CreateConfigOptions(&CfgOptions, "", consoleHelper.DP5Stat, false);
		consoleHelper.ClearConfigReadFormatFlags();	// clear all flags, set flags only for specific readback properties
		consoleHelper.CfgReadBack = true; // requesting general readback format
    if (consoleHelper.DppSocket_SendCommand_Config(XMTPT_FULL_READ_CONFIG_PACKET, CfgOptions) == false) {	// request full configuration
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling SendCommand_Config() for XMTPT_FULL_READ_CONFIG_PACKET\n",
            driverName, functionName);
        return asynError;
    }
    if (consoleHelper.DppSocket_ReceiveData() == false) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s error calling ReceiveData() for XMTPT_FULL_READ_CONFIG_PACKET\n",
            driverName, functionName);
        return asynError;
    }
    if (consoleHelper.HwCfgReady) {		// config is ready
      haveConfigFromHW_ = true;
    }
    return asynSuccess;
}


/* Report  parameters */
void drvAmptek::report(FILE *fp, int details)
{
    fprintf(fp, "drvAmptek %s interfaceType=%d, addressInfo=%s, serial number=%d\n", 
            portName, interfaceType_, addressInfo_, (int)consoleHelper.DP5Stat.m_DP5_Status.SerialNumber);
    fprintf(fp, "  Number of modules found on network=%d\n", consoleHelper.DppSocket_NumDevices);
    if (details > 0) {
        if (haveConfigFromHW_) {
            fprintf(fp, "  Preset mode:      %s\n", consoleHelper.strPresetCmd.c_str());
            fprintf(fp, "  Preset settings:  %s\n", consoleHelper.strPresetVal.c_str());
            fprintf(fp, "  Accumulation time:  %f\n", consoleHelper.DP5Stat.m_DP5_Status.AccumulationTime);
            fprintf(fp, "  Real time:          %f\n", consoleHelper.DP5Stat.m_DP5_Status.RealTime);
            fprintf(fp, "  Live time:          %f\n", consoleHelper.DP5Stat.m_DP5_Status.LiveTime);
            fprintf(fp, "  MCA_EN:             %d\n", consoleHelper.DP5Stat.m_DP5_Status.MCA_EN);
            fprintf(fp, "  PRECNT_REACHED:     %d\n", consoleHelper.DP5Stat.m_DP5_Status.PRECNT_REACHED);
            fprintf(fp, "  PresetRtDone:       %d\n", consoleHelper.DP5Stat.m_DP5_Status.PresetRtDone);
            fprintf(fp, "  PresetRtDone:       %d\n", consoleHelper.DP5Stat.m_DP5_Status.PresetRtDone);
            fprintf(fp, "  Status:\n%s\n", consoleHelper.DP5Stat.GetStatusValueStrings(consoleHelper.DP5Stat.m_DP5_Status).c_str());
            fprintf(fp, "  Configuration:\n%s\n", consoleHelper.HwCfgDP5.c_str());
            
        }
    }
    asynPortDriver::report(fp, details);
}

extern "C" {
int drvAmptekConfigure(const char *portName, int interfaceType, const char *addressInfo)
{
    new drvAmptek(portName, interfaceType, addressInfo);
    return asynSuccess;
}


static const iocshArg configArg0 = { "portName", iocshArgString};
static const iocshArg configArg1 = { "interface", iocshArgInt};
static const iocshArg configArg2 = { "address info", iocshArgString};
static const iocshArg * const configArgs[] = {&configArg0,
                                              &configArg1,
                                              &configArg2};
static const iocshFuncDef configFuncDef = {"drvAmptekConfigure", 3, configArgs};
static void configCallFunc(const iocshArgBuf *args)
{
    drvAmptekConfigure(args[0].sval, args[1].ival, args[2].sval);
}

void drvAmptekRegister(void)
{
    iocshRegister(&configFuncDef,configCallFunc);
}

epicsExportRegistrar(drvAmptekRegister);

}
