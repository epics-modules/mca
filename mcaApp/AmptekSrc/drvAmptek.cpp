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

static const char *clockStrings[]           = {"AUTO", "20", "80"};
static const char *polarityStrings[]        = {"POS", "NEG"};
static const char *gateStrings[]            = {"OFF", "HIGH", "LOW"};
static const char *purEnableStrings[]       = {"ON", "OFF", "MAX"};
static const char *mcaSourceStrings[]       = {"NORM", "MCS", "FAST", "PUR", "RTD"};
static const char *fastPeakingTimeStrings[] = {"50", "100", "200", "400", "800", "1600", "3200"};

#define TIMEOUT         0.01
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

    createParam(amptekInputPolarityString,            asynParamInt32, &amptekInputPolarity_);
    createParam(amptekClockString,                    asynParamInt32, &amptekClock_);
    createParam(amptekGainString,                   asynParamFloat64, &amptekGain_);
    createParam(amptekGateString,                     asynParamInt32, &amptekGate_);
    createParam(amptekMCASourceString,                asynParamInt32, &amptekMCASource_);
    createParam(amptekPUREnableString,                asynParamInt32, &amptekPUREnable_);
    createParam(amptekRTDEnableString,                asynParamInt32, &amptekRTDEnable_);
    createParam(amptekFastThresholdString,          asynParamFloat64, &amptekFastThreshold_);
    createParam(amptekSlowThresholdString,          asynParamFloat64, &amptekSlowThreshold_);
    createParam(amptekPeakingTimeString,            asynParamFloat64, &amptekPeakingTime_);
    createParam(amptekFastPeakingTimeString,          asynParamInt32, &amptekFastPeakingTime_);
    createParam(amptekFlatTopTimeString,            asynParamFloat64, &amptekFlatTopTime_);

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
          	consoleHelper.DppSocket.SetTimeOut((long)(TIMEOUT), (long)((TIMEOUT-(int)TIMEOUT)*1e6));

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
    consoleHelper.CreateConfigOptions(&configOptions_, "", consoleHelper.DP5Stat, false);
    
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

asynStatus drvAmptek::sendConfiguration()
{
    static const char *functionName = "sendConfiguration";
    bool status=true;
    int itemp;
    double dtemp;
    char tempString[20];
    string configString;
    
    // Clock rate
    getIntegerParam(amptekClock_, &itemp);
    sprintf(tempString, "CLCK=%s;", clockStrings[itemp]);
    configString.append(tempString);
    
    // Analog input polarity
    getIntegerParam(amptekInputPolarity_, &itemp);
    sprintf(tempString, "AINP=%s;", polarityStrings[itemp]);
    configString.append(tempString);
    
    // Peaking time
    getDoubleParam(amptekPeakingTime_, &dtemp);
    sprintf(tempString, "TPEA=%f;", dtemp);
    configString.append(tempString);
    
    // Fast peaking time
    getIntegerParam(amptekFastPeakingTime_, &itemp);
    sprintf(tempString, "TPFA=%s;", fastPeakingTimeStrings[itemp]);
    configString.append(tempString);

    // Flat top time
    getDoubleParam(amptekFlatTopTime_, &dtemp);
    sprintf(tempString, "TFLA=%f;", dtemp);
    configString.append(tempString);

    //  Gain
    getDoubleParam(amptekGain_, &dtemp);
    sprintf(tempString, "GAIN=%f;", dtemp);
    configString.append(tempString);

    // Slow threshold
    getDoubleParam(amptekSlowThreshold_, &dtemp);
    sprintf(tempString, "THSL=%f;", dtemp);
    configString.append(tempString);
    
    // Fast threshold
    getDoubleParam(amptekFastThreshold_, &dtemp);
    sprintf(tempString, "THFA=%f;", dtemp);
    configString.append(tempString);
    
    // Number of MCA channels
    getIntegerParam(mcaNumChannels_, &itemp);
    sprintf(tempString, "MCAC=%d;", itemp);
    configString.append(tempString);
    
    // Gate
    getIntegerParam(amptekGate_, &itemp);
    sprintf(tempString, "GATE=%s;", gateStrings[itemp]);
    configString.append(tempString);
    
    //  Preset real time
    getDoubleParam(mcaPresetRealTime_, &dtemp);
    sprintf(tempString, "PRER=%f;", dtemp);
    configString.append(tempString);
    
    //  Preset live time
    getDoubleParam(mcaPresetLiveTime_, &dtemp);
    sprintf(tempString, "PRET=%f;", dtemp);
    configString.append(tempString);

    // MCA source
    getIntegerParam(amptekMCASource_, &itemp);
    sprintf(tempString, "MCAS=%s;", mcaSourceStrings[itemp]);
    configString.append(tempString);

    // PUR enable
    getIntegerParam(amptekPUREnable_, &itemp);
    sprintf(tempString, "PURE=%s;", purEnableStrings[itemp]);
    configString.append(tempString);
    
    configOptions_.HwCfgDP5Out = configString;
    asynPrint(pasynUserSelf, ASYN_TRACEIO_DRIVER,
      "%s::%s sending configuration string:\n%s\n",
      driverName, functionName, configString.c_str());

    switch(interfaceType_) {
        case amptekInterfaceEthernet:
            status = consoleHelper.DppSocket_SendCommand_Config(XMTPT_SEND_CONFIG_PACKET_EX, configOptions_);
            if (status == false) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error callingg consoleHelper.DppSocket_SendCommand_Config(XMTPT_SEND_CONFIG_PACKET_EX, %s)\n",
                    driverName, functionName, configString.c_str());
                return asynError;
            }
            break;
        
        case amptekInterfaceUSB:
            break;
        
        case amptekInterfaceSerial:
            break;
    }
    readConfigurationFromHardware();
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
    else if ((command == mcaNumChannels_)        ||
             (command == amptekInputPolarity_)   ||
             (command == amptekClock_)           ||
             (command == amptekGate_)            ||
             (command == amptekFastPeakingTime_) ||
             (command == amptekPUREnable_)       ||
             (command == amptekMCASource_)) {
        status = sendConfiguration();
    }
    if (command == mcaNumChannels_) {
        if (value > 0) {
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
    //static const char *functionName = "readInt32";

    if (command == mcaAcquiring_) {
        *value = consoleHelper.DP5Stat.m_DP5_Status.MCA_EN;
        acquiring_ = *value;
    }
    else {
        status = asynPortDriver::readInt32(pasynUser, value);
    }
    return status;
}

asynStatus drvAmptek::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    //static const char *functionName = "readFloat64";

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
        status = asynPortDriver::readFloat64(pasynUser, value);
    }
    return status;
}


asynStatus drvAmptek::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;

    /* Set the parameter in the parameter library. */
    status = setDoubleParam(command, value);
    if ((command == mcaPresetRealTime_)   || 
        (command == mcaPresetLiveTime_)   ||
        (command == amptekGain_)          ||
        (command == amptekPeakingTime_)   ||
        (command == amptekFlatTopTime_)   ||
        (command == amptekSlowThreshold_) ||
        (command == amptekFastThreshold_)) {
        status = sendConfiguration();
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
    int i;
 
    consoleHelper.CreateConfigOptions(&CfgOptions, "", consoleHelper.DP5Stat, false);
		consoleHelper.ClearConfigReadFormatFlags();	// clear all flags, set flags only for specific readback properties
		consoleHelper.CfgReadBack = true; // requesting general readback format
		// This function is normally called after sending the new configuration, which can take time before the unit will respond
		// to the next command.  Loop for up to 1 second waiting.
		for (i=0; i<100; i++) {
		    // request full configuration
        if (consoleHelper.DppSocket_SendCommand_Config(XMTPT_FULL_READ_CONFIG_PACKET, CfgOptions) == true) {
            break;
        }
        epicsThreadSleep(0.01);
    }
    if (i == 100) {
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
