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

drvAmptek::drvAmptek(const char *portName, int interfaceType, const char *addressInfo, int serialNumber)
   : asynPortDriver(portName, 
                    1, /* Maximum address */
                    0, /* Unused, number of parameters */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynDrvUserMask, /* Interface mask */
                    asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask,                   /* Interrupt mask */
                    ASYN_CANBLOCK, /* asynFlags.  This driver can block and is not multi-device */
                    1, /* Autoconnect */
                    0, /* Default priority */
                    0), /* Default stack size*/
    numModules_(0)
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
    serialNumber_ = serialNumber;
    addressInfo_ = epicsStrDup(addressInfo);
    
    status = connectDevice();
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s::%s cannot connect to device on interface type=%d addressInfo=%s, serialNumber=%d status=%d\n",
            driverName, functionName, interfaceType_, addressInfo_, serialNumber_, status);
    }
}

asynStatus drvAmptek::connectDevice()
{
    char tempString[256];
    asynStatus status;
    static const char *functionName = "connectDevice";

    switch(interfaceType_) {
        case amptekInterfaceEthernet:
            strcpy(udpBroadcastPortName_, "UDP_Broadcast_");
            strcat(udpBroadcastPortName_, portName);
            strcpy(udpCommandPortName_, "UDP_Command_");
            strcat(udpCommandPortName_, portName);
            sprintf(tempString, "%s:%d UDP*", addressInfo_, BROADCAST_PORT);
            status = (asynStatus)drvAsynIPPortConfigure(udpBroadcastPortName_, tempString, 0, 0, 0);
            if (status) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling drvAsynIPPortConfigure for broadcast port=%s, IP=%s, status=%d\n", 
                    driverName, functionName, udpBroadcastPortName_, tempString, status);
                return asynError;
            }
        
            // Connect to the broadcast port
            status = pasynOctetSyncIO->connect(udpBroadcastPortName_, 0, &pasynUserUDPBroadcast_, NULL);
            if (status) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error connecting to UDP port, status=%d, error=%s\n", 
                    driverName, functionName, status, pasynUserUDPBroadcast_->errorMessage);
                return asynError;
            }
            
            // Find module on network
            status = findModule();
            if (status) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error finding module, running asynReport to list modules found\n", 
                    driverName, functionName);
                report(stdout, 1);
                return asynError;
            }
            break;

        case amptekInterfaceUSB:
        
        case amptekInterfaceSerial:
        
        default:
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s unknown interface type=%d\n", 
                driverName, functionName, interfaceType_);
    }
    return asynSuccess;

}

asynStatus drvAmptek::findModule()
{
    size_t nwrite;
    size_t nread;
    epicsTimeStamp start;
    epicsTimeStamp now;
    epicsFloat64 deltaTime;
    int status;
    int eomReason;
  	unsigned char outBuff[6] = {0,0,0,0,0xF4,0xFA};
    unsigned char inBuff[1024];
    char tempString[256];
    int i;
    static const char *functionName="findModule";

    epicsTimeGetCurrent(&start);
    
    // Put the  current nanoseconds in the buffer as a "random" number
    outBuff[2] = (start.nsec >> 8);
    outBuff[3] = (start.nsec & 0x00FF);
    status = pasynOctetSyncIO->write(pasynUserUDPBroadcast_, (char*)outBuff, sizeof(outBuff), 1.0, &nwrite);
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s::%s error calling pasynOctetSyncIO->write to send broadcast messages\n", 
            driverName, functionName);
        return asynError;
    }

    while (1) {
        epicsTimeGetCurrent(&now);
        deltaTime = epicsTimeDiffInSeconds(&now, &start);
        if (deltaTime > BROADCAST_TIMEOUT) break;
        status = pasynOctetSyncIO->read(pasynUserUDPBroadcast_, (char*)inBuff, sizeof(inBuff), 0.01, &nread, &eomReason);
        if ((status == asynTimeout) && (nread > 32)) {
printf("drvAmptek::findModule nsec1=0x%x, nsec2=0x%x, nread=%d, inBuff=0x%x 0x%x 0x%x 0x%x\n", 
(int)((start.nsec >> 8) & 0x00FF), (int)(start.nsec & 0x00FF), (int)nread, inBuff[0], inBuff[1], inBuff[2], inBuff[3]);
            if ((inBuff[0] == 0x01) && 
                (inBuff[2] == ((start.nsec >> 8) & 0x00FF)) && 
                (inBuff[3] == (start.nsec & 0x00FF))) {
printf("Found an Amptek module\n");
                   // This is an Amptek module
                   // Get the IP address and serial number
                   sprintf(moduleInfo_[numModules_].moduleIP, "%d.%d.%d.%d", inBuff[20], inBuff[21], inBuff[22], inBuff[23]);
                   sscanf((char *)&inBuff[49], "%d", &moduleInfo_[numModules_].serialNumber);
                   numModules_++;
            }
        } else if (status != asynTimeout) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
                "%s::%s error calling pasynOctetSyncIO->read to receive broadcast reply\n", 
                driverName, functionName);
            return asynError;
        }
    }

    // See if the specified module was found
    for (i=0; i<numModules_; i++) {
        if (moduleInfo_[i].serialNumber == serialNumber_) break;
    }
    
    if (i == numModules_) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR, 
            "%s:%s: cannot find requested module %d on network\n", 
            driverName, functionName, serialNumber_);
        return asynError;
    }
    
    // Create UDP command port
    epicsSnprintf(tempString, sizeof(tempString), "%s:%d UDP", moduleInfo_[i].moduleIP, COMMAND_PORT);
    status = drvAsynIPPortConfigure(udpCommandPortName_, tempString, 0, 0, 0);
    if (status) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
            "%s:%s: error calling drvAsyIPPortConfigure for UDP command port %s, IP=%s, status=%d\n", 
            driverName, functionName, udpCommandPortName_, moduleInfo_[i].moduleIP, status);
        return asynError;
    }

    // Connect to UDP command port
    status = pasynOctetSyncIO->connect(udpCommandPortName_, 0, &pasynUserUDPCommand_, NULL);
    if (status) {
        printf("%s:%s: error calling pasynOctetSyncIO->connect for UDP command port, status=%d, error=%s\n", 
               driverName, functionName, status, pasynUserUDPCommand_->errorMessage);
        return asynError;
    }
    pasynOctetSyncIO->setInputEos(pasynUserUDPCommand_, "\r\n", 2);
    pasynOctetSyncIO->setOutputEos(pasynUserUDPCommand_, "\r", 1);
 
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
            setIntegerParam(mcaAcquiring_, acquiring_);
        }
    }  
    else if (command == mcaStopAcquire_) {
        ;
    }
    else if (command == mcaErase_) {
        memset(pData_, 0, numChannels_ * sizeof(epicsInt32));
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

asynStatus drvAmptek::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;

    /* Set the parameter in the parameter library. */
    status = setDoubleParam(command, value);
    if (command == mcaPresetRealTime_) {
    }
    else if (command == mcaPresetLiveTime_) {
    }
    callParamCallbacks();
    return status;
}

asynStatus drvAmptek::readInt32Array(asynUser *pasynUser,
                                        epicsInt32 *data, size_t maxChans, 
                                        size_t *nactual)
{

    if (maxChans > numChannels_) maxChans = numChannels_;
    memcpy(data, pData_, maxChans * sizeof(epicsInt32));
    *nactual = maxChans;
    return asynSuccess;
}


/* Report  parameters */
void drvAmptek::report(FILE *fp, int details)
{
    int i;
    fprintf(fp, "drvAmptek %s interfaceType=%d, addressInfo=%s, serial number=%d\n", 
            portName, interfaceType_, addressInfo_, serialNumber_);
    fprintf(fp, "  Number of modules found on network=%d\n", numModules_);
    for (i=0; i<numModules_; i++) {
        fprintf(fp, "    Module %d\n", i);
        fprintf(fp, "      Serial #:  %d\n", moduleInfo_[i].serialNumber);
        fprintf(fp, "      IP address: %s\n", moduleInfo_[i].moduleIP);
    }
    asynPortDriver::report(fp, details);
}

extern "C" {
int drvAmptekConfigure(const char *portName, int interfaceType, const char *addressInfo, int serialNumber)
{
    new drvAmptek(portName, interfaceType, addressInfo, serialNumber);
    return asynSuccess;
}


static const iocshArg configArg0 = { "portName", iocshArgString};
static const iocshArg configArg1 = { "interface", iocshArgInt};
static const iocshArg configArg2 = { "address info", iocshArgString};
static const iocshArg configArg3 = { "serial number", iocshArgInt};
static const iocshArg * const configArgs[] = {&configArg0,
                                              &configArg1,
                                              &configArg2,
                                              &configArg3};
static const iocshFuncDef configFuncDef = {"drvAmptekConfigure", 4, configArgs};
static void configCallFunc(const iocshArgBuf *args)
{
    drvAmptekConfigure(args[0].sval, args[1].ival, args[2].sval, args[3].ival);
}

void drvAmptekRegister(void)
{
    iocshRegister(&configFuncDef,configCallFunc);
}

epicsExportRegistrar(drvAmptekRegister);

}
