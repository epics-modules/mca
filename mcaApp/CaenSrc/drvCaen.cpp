// File:    drvCaen.cpp
// Author:  Pedro Nariyoshi, Facility for Rare Isotope Beams
// Date:    18-January-2024
//
// Purpose: 
// This module provides the driver support for the MCA asyn device support layer
// for C.A.E.N. MCAs that support the CAENDigitizer library.
//
//

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsTime.h>
#include <epicsTypes.h>
#include <epicsThread.h>
#include <epicsString.h>
#include <epicsExit.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <osiSock.h>
#include <alarm.h>

#include <drvAsynIPPort.h>
#include <asynOctetSyncIO.h>

#include <epicsExport.h>
#include <drvMca.h>
#include <drvCaen.h>

static const char *driverName = "drvCaen";
void readThread(void *drvPvt);

extern "C" {
    static void exitHandlerC(void *arg)
    {
        drvCaen *p = (drvCaen *)arg;
        p->exitHandler();
    }
}

drvCaen::drvCaen(const char *portName, CAEN_DGTZ_ConnectionType LinkType, int LinkNum,int ConetNode, unsigned int VMEBaseAddress)
    : asynPortDriver(portName, 
            MAX_ADCS, /* Maximum address */
            asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynOctetMask | asynDrvUserMask | asynOptionMask, /* Interface mask */
            asynInt32Mask | asynInt32ArrayMask | asynFloat64Mask | asynOctetMask,                                    /* Interrupt mask */
            ASYN_CANBLOCK, /* asynFlags.  This driver can block and is not multi-device */
            1, /* Autoconnect */
            0, /* Default priority */
            0) /* Default stack size*/
{
    const char *functionName = "drvCaen";

    // Initialize parameters
    LastReadTime_ = 0;
    for(unsigned int chan = 0; chan < MAX_ADCS; chan++){
        StartTime_[chan] = LastReadTime_;
        NumEvents_[chan] = 0;
        Events_[chan] = NULL;
        EHisto[chan] = NULL;
        THisto[chan] = NULL;
        TrgCnt[chan] = 0;
        ECnt[chan] = 0;
        PrevTime[chan] = 0;
        ExtendedTT[chan] = 0;
        PurCnt[chan] = 0;
    }
    memset(&Params_, 0, sizeof(DigitizerParams_t));
    memset(&DPPParams_, 0, sizeof(CAEN_DGTZ_DPP_PHA_Params_t));

    // Initialize board parameters
    Params_.LinkType = LinkType;
    Params_.LinkNum = LinkNum;
    Params_.ConetNode = ConetNode;
    Params_.VMEBaseAddress = VMEBaseAddress;

    if(Params_.LinkType == CAEN_DGTZ_USB && Params_.ConetNode!=0){
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s For USB, ConetMode must be equal to 0\n", 
                driverName, functionName);
        return;
    }

    // Initialize internal variables
    acquiring_ = false;
    isConnected_ = false;
    exiting_ = false;
    // Uncomment this line to enable asynTraceFlow during the constructor
    //pasynTrace->setTraceMask(pasynUserSelf, 0x11);

    createParam(mcaStartAcquireString,                asynParamInt32, &mcaStartAcquire_);
    createParam(mcaStopAcquireString,                 asynParamInt32, &mcaStopAcquire_);            /* int32, write */
    createParam(mcaEraseString,                       asynParamInt32, &mcaErase_);                  /* int32, write */
    createParam(mcaDataString,                        asynParamInt32Array, &mcaData_);                   /* int64Array, read/write */
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

    // CAEN-specific board parameters
    createParam(caenModelNameString,                asynParamOctet, &caenModelName_);
    createParam(caenModelString,                    asynParamInt32, &caenModel_);
    createParam(caenNADCsString,                    asynParamInt32, &caenNADCs_);
    createParam(caenFormFactorString,               asynParamInt32, &caenFormFactor_);
    createParam(caenFamilyCodeString,               asynParamInt32, &caenFamilyCode_);
    createParam(caenROCFirmwareString,              asynParamOctet, &caenROC_FirmwareRel_);
    createParam(caenAMCFirmwareString,              asynParamOctet, &caenAMC_FirmwareRel_);
    createParam(caenSerialNumberString,             asynParamInt32, &caenSerialNumber_);
    createParam(caenMezzanineSerNumString,          asynParamOctet, &caenMezzSerNum_);
    createParam(caenPCBRevisionString,              asynParamInt32, &caenPCB_Revision_);
    createParam(caenADCNBITSString,                 asynParamInt32, &caenADC_NBits_);
    createParam(caenSAMCorrectionString,            asynParamInt32, &caenSAMCorrectionDataLoaded_);
    createParam(caenCommHandleString,               asynParamInt32, &caenCommHandle_);
    createParam(caenVMEHandleString,                asynParamInt32, &caenVMEHandle_);
    createParam(caenLicenseString,                  asynParamOctet, &caenLicense_);
    createParam(caenDCOffsetString,                 asynParamInt32, &caenDCOffset_);
    createParam(caenPreTriggerSizeString,           asynParamInt32, &caenPreTriggerSize_);
    createParam(caenPulsePolarityString,            asynParamInt32, &caenPulsePolarity_);
    createParam(caenBoardTempString,                asynParamInt32, &caenBoardTemp_);

    // CAEN-specific channel parameters
    createParam(caenTrigThreshString,               asynParamInt32, &caenTrigThresh_);
    createParam(caenTrapRiseString,                 asynParamInt32, &caenTrapRise_);
    createParam(caenTrapFlatTopString,              asynParamInt32, &caenTrapFlatTop_);
    createParam(caenDecayTimeString,                asynParamInt32, &caenDecayTime_);
    createParam(caenFlatTopDelayString,             asynParamInt32, &caenFlatTopDelay_);
    createParam(caenTrigFiltSmooString,             asynParamInt32, &caenTrigFiltSmoo_);
    createParam(caenInputRiseTimeString,            asynParamInt32, &caenInputRiseTime_);
    createParam(caenTrigHoldOffString,              asynParamInt32, &caenTrigHoldOff_);
    createParam(caenNBaselineString,                asynParamInt32, &caenNBaseline_);
    createParam(caenNSPeakString,                   asynParamInt32, &caenNSPeak_);
    createParam(caenPeakHoldOffString,              asynParamInt32, &caenPeakHoldOff_);
    createParam(caenBLHoldOffString,                asynParamInt32, &caenBLHoldOff_);
    createParam(caenEnergyNFString,                 asynParamFloat64, &caenEnergyNF_);
    createParam(caenDecimationString,               asynParamInt32, &caenDecimation_);
    createParam(caenDecGainString,                  asynParamInt32, &caenDecGain_);
    createParam(caenTrigWinString,                  asynParamInt32, &caenTrigWin_);
    createParam(caenTWWDTString,                    asynParamInt32, &caenTWWDT_);

    // CAEN-specific miscelaneous parameters
    createParam(caenRegAddrString,                  asynParamInt32, &caenRegAddr_);
    createParam(caenRegisterString,                 asynParamInt32, &caenRegister_);
    createParam(caenPollPeriodString,               asynParamFloat64, &caenPollPeriod_);

    failedSends_ = 0;

    eventId_ = epicsEventCreate(epicsEventEmpty);
    setDoubleParam(caenPollPeriod_, 0.005); // Default poll every 5ms
    /* Create the thread that reads from the board in the background */
#ifdef EPICS_THREAD_CAN_JOIN  // Sadly, not all OS and EPICS versions can do things properly
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.joinable = true;  // We need to be able to stop the thread
    opts.priority = epicsThreadPriorityHigh; //
    threadId_ = epicsThreadCreateOpt("drvCaenReadBoardTask",
            (EPICSTHREADFUNC)::readThread,
            this,
            &opts);
#else
    threadId_ = epicsThreadCreate("drvCaenReadBoardTask",
            epicsThreadPriorityHigh,
            epicsThreadGetStackSize(epicsThreadStackMedium),
            (EPICSTHREADFUNC)::readThread,
            this);
#endif
    if (threadId_ == NULL) {
        epicsSnprintf(pasynUserSelf->errorMessage, pasynUserSelf->errorMessageSize,
                "%s:%s: epicsThreadCreate failure\n", driverName, functionName);
        return;
    }
    epicsAtExit(exitHandlerC, this);
}

drvCaen::~drvCaen()
{
    exitHandler();
}

void drvCaen::exitHandler()
{
    int ret;
    const char *functionName = "exitHandler";
    lock();
    ret = CAEN_DGTZ_SWStopAcquisition(handle_);
    isConnected_ = false;
    exiting_ = true;
    acquiring_ = false;
    unlock();

#ifdef EPICS_THREAD_CAN_JOIN  // Sadly, not all OS and EPICS versions can do things properly
    epicsEventSignal(eventId_);
    epicsThreadMustJoin(threadId_);
#endif

    ret |= CAEN_DGTZ_CloseDigitizer(handle_);
    CAEN_DGTZ_FreeReadoutBuffer(&buffer_);
    CAEN_DGTZ_FreeDPPEvents(handle_, (void**) Events_);
    CAEN_DGTZ_FreeDPPWaveforms(handle_, Waveform_);

    if (ret != CAEN_DGTZ_Success){
        epicsSnprintf(pasynUserSelf->errorMessage, pasynUserSelf->errorMessageSize,
                "%s:%s: Error closing digitizer\n", driverName, functionName);
    }
}


asynStatus drvCaen::connect(asynUser *pasynUser)
{
    static const char *functionName = "connect";
    int addr;
    getAddress(pasynUser, &addr);

    //    if (addr > 0)
    //        return (handle_ != 0) ? asynSuccess : asynError;

    if (isConnected_ == false) {
        asynStatus status = connectDevice(pasynUser);
        if (status != asynSuccess) {
            /* the connection itself might be successful but the subsequent initialization might have failed */
            return status;
        }

        isConnected_ = true;
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) handle is %d\n",
                driverName, functionName, portName, handle_);
        failedSends_ = 0;
        pasynManager->exceptionConnect(pasynUser);
    }

    // Check if ADC number could exist
    // TODO: Maybe use specific number of channels for this board
    if (addr >= 0 && addr < MAX_ADCS){
        return asynSuccess;
    }
    else{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) ADC Index %d does not exist\n",
                driverName, functionName, portName, addr);
        return asynError;
    }
    // Catchall at the end
    return asynSuccess;
}

//asynStatus drvCaen::disconnect(asynUser *pasynUser)
//{
//    static const char *functionName = "disconnectDevice";
//    asynStatus ret;
//
//    // Disconnect from the digitizer
//    if(isConnected_ == true){
//        ret = disconnectDevice();
//
//        int addr;
//        getAddress(pasynUser, &addr);
//
//        if (ret == asynSuccess){
//            isConnected_ = false;
//            asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
//                    "%s::%s Digitizer (%s) disconnected\n",
//                    driverName, functionName, portName);
//        }
//        else{
//            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
//                    "%s::%s Digitizer (%s) not disconnected\nError code = %d\n",
//                    driverName, functionName, portName, ret);
//            return asynError;
//        }
//    }
//
//    pasynManager->exceptionDisconnect(pasynUser);
//    setParamsAlarm(COMM_ALARM, INVALID_ALARM);
//
//    return asynSuccess;
//}

void drvCaen::setParamsAlarm(int alarmStatus, int alarmSeverity)
{
    for (int addr = 0; addr < MAX_ADCS; ++addr) {
        int numParams = 0;
        getNumParams(addr, &numParams);
        for (int i = 0; i < numParams; ++i) {
            setParamAlarmStatus(addr, i, alarmStatus);
            setParamAlarmSeverity(addr, i, alarmSeverity);
        }
        callParamCallbacks(addr);
    }
}

void drvCaen::checkFailedComm(const char *functionName)
{
    if (++failedSends_ > MAX_FAILED_SENDS) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s too many failed status queries, disconnecting\n",
                driverName, functionName);
        disconnect(pasynUserSelf);
    }
}

asynStatus drvCaen::connectDevice(asynUser *pasynUser)
{
    static const char *functionName = "connectDevice";
    CAEN_DGTZ_ErrorCode ret;
    CAEN_DGTZ_BoardInfo_t BoardInfo;

    if (isConnected_ == true){
        return asynSuccess;
    }

    /// Connect to digitizer
    ret = CAEN_DGTZ_OpenDigitizer(Params_.LinkType, Params_.LinkNum, Params_.ConetNode,
            Params_.VMEBaseAddress, &handle_);
    if (ret == CAEN_DGTZ_Success){
        asynPrint(pasynUser, ASYN_TRACEIO_DRIVER,
                "%s::%s Digitizer (%d, %d, %d, %d) connected\n",
                driverName, functionName, Params_.LinkType, Params_.LinkNum, Params_.ConetNode, Params_.VMEBaseAddress);
    }
    else{
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%d, %d, %d, %d) not connected\nError code = %d\n",
                driverName, functionName, Params_.LinkType, Params_.LinkNum, Params_.ConetNode, Params_.VMEBaseAddress,ret);
        disconnect(pasynUser);
        return asynError;
    }

    // Get board info
    ret = CAEN_DGTZ_GetInfo(handle_, &BoardInfo);
    if (ret != CAEN_DGTZ_Success){
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Failed to get BoardInfo for %s (error = %d)\n",
                driverName, functionName, portName, ret);
        disconnect(pasynUser);
        return asynError;
    }

    setStringParam(caenModelName_,BoardInfo.ModelName);
    setIntegerParam(caenModel_,BoardInfo.Model);
    setIntegerParam(caenNADCs_,BoardInfo.Channels);
    setIntegerParam(caenFormFactor_,BoardInfo.FormFactor);
    setIntegerParam(caenFamilyCode_,BoardInfo.FamilyCode);
    setStringParam(caenROC_FirmwareRel_,BoardInfo.ROC_FirmwareRel);
    setStringParam(caenAMC_FirmwareRel_,BoardInfo.AMC_FirmwareRel);
    setIntegerParam(caenSerialNumber_,BoardInfo.SerialNumber);
    setIntegerParam(caenPCB_Revision_,BoardInfo.PCB_Revision);
    setIntegerParam(caenADC_NBits_,BoardInfo.ADC_NBits);
    setIntegerParam(caenSAMCorrectionDataLoaded_,BoardInfo.SAMCorrectionDataLoaded);
    setIntegerParam(caenCommHandle_,BoardInfo.CommHandle);
    setIntegerParam(caenVMEHandle_,BoardInfo.VMEHandle);
    setStringParam(caenLicense_,BoardInfo.License);

    BitMask_ = (1<<BoardInfo.ADC_NBits) - 1; // Mask for maximum value for ADC readings
                                             //  Special case for Mezzanine Serial Number
    if(Params_.VMEBaseAddress != 0){
        char mezzSerialNum[48];
        mezzSerialNum[0] = '\0';
        for(unsigned int index = 0; index < 4; index++){
            strcat(mezzSerialNum,BoardInfo.MezzanineSerNum[index]);
            strcat(mezzSerialNum,",");
        }
        setStringParam(caenMezzSerNum_,mezzSerialNum);
    }
    else{
        setStringParam(caenMezzSerNum_,"N/A");
    }
    // Clear alarms
    setParamsAlarm(NO_ALARM, NO_ALARM);

    // Verify firmware type
    int FirmwareCode;
    sscanf(BoardInfo.AMC_FirmwareRel, "%d", &FirmwareCode);
    if (FirmwareCode != V1730_DPP_PHA_CODE && FirmwareCode != V1724_DPP_PHA_CODE) {
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s Digitizer firmware is not DPP-PHA, functionality is limited\n",
                driverName, functionName);
    }

    for(unsigned int chan = 0; chan < MAX_ADCS; chan++){
        if(EHisto[chan] != NULL){
            free(EHisto[chan]);
        }
        EHisto[chan] = (uint32_t *)calloc((1 << BoardInfo.ADC_NBits),sizeof(uint32_t));
        if(EHisto[chan] == NULL){
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                    "%s::%s Could not allocate memory for energy histogram\n",
                    driverName, functionName);
            disconnect(pasynUser);
            return asynError;
        }
        if(THisto[chan] != NULL){
            free(THisto[chan]);
        }
        THisto[chan] = (uint32_t *)calloc((1 << BoardInfo.ADC_NBits),sizeof(uint32_t));
        if(THisto[chan] == NULL){
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                    "%s::%s Could not allocate memory for timing histogram\n",
                    driverName, functionName);
            disconnect(pasynUser);
            return asynError;
        }
    }

    SetDefaultParams(&Params_, &DPPParams_);
    ProgramDigitizer(handle_, Params_, DPPParams_);
    allocateBuffers();

    return asynSuccess;
}

asynStatus drvCaen::allocateBuffers(){
    // This function allocates buffers that the CAEN driver uses
    // It must be called after programming the digitizer
    int ret = 0;
    static const char *functionName = "allocateBuffers";
    /* Allocate readout buffer */
    if (buffer_ != NULL){
        CAEN_DGTZ_FreeReadoutBuffer(&buffer_);
    }
    ret = CAEN_DGTZ_MallocReadoutBuffer(handle_, &buffer_, &AllocatedSize_);
    /* Allocate event buffer */
    if (Events_[0] != NULL){
        CAEN_DGTZ_FreeDPPEvents(handle_, (void **)Events_);
    }
    ret |= CAEN_DGTZ_MallocDPPEvents(handle_,(void **)Events_, &AllocatedSize_);
    /* Allocate waveform buffer */
    if (Waveform_ != NULL){
        ret |= CAEN_DGTZ_FreeDPPWaveforms(handle_,(void **)&Waveform_);
    }
    ret |= CAEN_DGTZ_MallocDPPWaveforms(handle_,(void **)&Waveform_, &AllocatedSize_);

    if (ret != 0){
        asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                "%s::%s Could not allocate memory. Error %d\n",
                driverName, functionName, ret);
        return asynError;
    }

    return asynSuccess;
}

asynStatus drvCaen::writeInt32(asynUser *pasynUser, epicsInt32 value)
{
    int ret = 0;
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    int addr;
    static const char *functionName = "writeInt32";

    if (isConnected_ == false)
        return asynDisconnected;

    getAddress(pasynUser, &addr);
    /* Set the parameter in the parameter library. */
    status = setIntegerParam(addr, command, value);

    if (command == mcaStartAcquire_){
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "Request to start acquiring\n");
        if (!acquiring_) {
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                    "Not acquiring yet, requesting");
            ret = CAEN_DGTZ_SWStartAcquisition(handle_);
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                    "Requested");
            if (ret == CAEN_DGTZ_Success) {
                acquiring_ = true;
                setIntegerParam(mcaAcquiring_, acquiring_);
                LastReadTime_ = get_time_ns();
                for(unsigned int i=0; i<MAX_ADCS; i++){
                    StartTime_[i] = LastReadTime_;
                }
                epicsEventSignal(eventId_);
            }
            else {
                epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                        "Error starting acquisition");
                checkFailedComm("writeInt32");
                status = asynError;
            }
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                    "Done requesting");
        }
    }
    else if (command == mcaStopAcquire_){
        CAEN_DGTZ_ErrorCode ret;
        ret = CAEN_DGTZ_SWStopAcquisition(handle_);
        if (ret == CAEN_DGTZ_Success) {
            acquiring_ = false;
            setIntegerParam(mcaAcquiring_, acquiring_);
        }
        else {
            epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                    "Error stopping acquisition");
            checkFailedComm("writeInt32");
            status = asynError;
        }
    }
    else if (command == mcaErase_){
        int numChannels;
        StartTime_[addr] = LastReadTime_;
        getIntegerParam(caenADC_NBits_,&numChannels);
        numChannels = 1<<numChannels;
        memset(EHisto[addr], 0, numChannels*sizeof(uint32_t));
        memset(THisto[addr], 0, numChannels*sizeof(uint32_t));
        TrgCnt[addr] = 0;
        ECnt[addr] = 0;
        PurCnt[addr] = 0;
    }
    else if (command == mcaReadStatus_){
        //TODO
    }
    else if (command == caenTrigThresh_){
        DPPParams_.thr[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting threshold, ret = %d\n",ret);
    }
    else if (command == caenTrapRise_){
        DPPParams_.k[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting trapezoid rise time, ret = %d\n",ret);
    }
    else if (command == caenTrapFlatTop_){
        DPPParams_.m[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting trapezoid flat top time, ret = %d\n",ret);
    }
    else if (command == caenDecayTime_){
        DPPParams_.M[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting decay time constant, ret = %d\n",ret);
    }
    else if (command == caenFlatTopDelay_){
        DPPParams_.ftd[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting flat top delay, ret = %d\n",ret);
    }
    else if (command == caenTrigFiltSmoo_){
        DPPParams_.a[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting trigger filter smoothing factor, ret = %d\n",ret);
    }
    else if (command == caenInputRiseTime_){
        DPPParams_.b[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting input signal rise time, ret = %d\n",ret);
    }
    else if (command == caenTrigHoldOff_){
        DPPParams_.trgho[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting trigger hold off, ret = %d\n",ret);
    }
    else if (command == caenNBaseline_){
        DPPParams_.nsbl[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting number of baseline samples, ret = %d\n",ret);
    }
    else if (command == caenNSPeak_){
        DPPParams_.nspk[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting number of sample to average for trapezoid height estimation, ret = %d\n",ret);
    }
    else if (command == caenPeakHoldOff_){
        DPPParams_.pkho[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting peak hold off, ret = %d\n",ret);
    }
    else if (command == caenBLHoldOff_){
        DPPParams_.blho[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting baseline hold off, ret = %d\n",ret);
    }
    else if (command == caenDecimation_){
        DPPParams_.decimation[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting decimation factor, ret = %d\n",ret);
    }
    else if (command == caenDecGain_){
        DPPParams_.dgain[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting decimation gain, ret = %d\n",ret);
    }
    else if (command == caenTrigWin_){
        DPPParams_.trgwin[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting rise time discrimination, ret = %d\n",ret);
    }
    else if (command == caenTWWDT_){
        DPPParams_.twwdt[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting rise time validation window, ret = %d\n",ret);
    }
    else if (command == caenDCOffset_){
        Params_.DCOffset[addr] = value;
        ret = CAEN_DGTZ_SetChannelDCOffset(handle_, addr, Params_.DCOffset[addr]);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetChannelDCOffset(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting DC offset, ret = %d\n",ret);
    }
    else if (command == caenPreTriggerSize_){
        Params_.PreTriggerSize[addr] = value;
        ret = CAEN_DGTZ_SetDPPPreTriggerSize(handle_, addr, Params_.PreTriggerSize[addr]);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPreTriggerSize(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting pre-trigger size, ret = %d\n",ret);
    }
    else if (command == caenPulsePolarity_){
        if (value == 0){
            Params_.PulsePolarity[addr] = CAEN_DGTZ_PulsePolarityPositive;
        }
        else if (value == 1){
            Params_.PulsePolarity[addr] = CAEN_DGTZ_PulsePolarityNegative;
        }
        else {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s Invalid Option (%d) for Pulse Polarity (must be 0 or 1).\n",
                    driverName, functionName, value);
            status = asynError;
        }

        ret = CAEN_DGTZ_SetChannelPulsePolarity(handle_, addr, Params_.PulsePolarity[addr]);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetChannelPulsePolarity(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting pulse-polarity, ret = %d\n",ret);
    }
    else if (command == caenRegAddr_){
        setIntegerParam(caenRegAddr_, value);
    }
    else if (command == caenRegister_){
        int regAddr;
        getIntegerParam(caenRegAddr_, &regAddr);
        ret = CAEN_DGTZ_WriteRegister(handle_, regAddr, value);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling WriteRegister(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Writing register, ret = %d\n",ret);
    }
    //TODO
    //    else if (command == mcaNumChannels_) {
    //        if (value > 0) {
    //            numChannels_ = value;
    //            if (pData_[addr]) free(pData_[addr]);
    //            pData_[addr] = (epicsInt32 *)calloc(numChannels_, sizeof(epicsInt32));
    //        }
    //    }
    //    // All other commands are parameters so we send the configuration
    else {
        //TODO
    }
    callParamCallbacks(addr);
    return status;
}

asynStatus drvCaen::readInt32(asynUser *pasynUser, epicsInt32 *value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    int mcaEnable, liveTimeDone, realTimeDone, countDone, mcsDone;
    static const char *functionName = "readInt32";
    int ret = 0;

    int addr;

    if (isConnected_ == false)
        return asynDisconnected;

    getAddress(pasynUser, &addr);

    if (command == mcaAcquiring_) {
        double Preset;
        //TODO: Look into mcsDone/mcaEnable
        //TODO: Not currently evaluating PCTL and PCTH for PresetCounts
        getDoubleParam(addr, mcaPresetCounts_, &Preset);
        if (Preset > 0) {
            countDone = TrgCnt[addr] > Preset;
        }
        else {
            countDone = 0;
        }
        getDoubleParam(addr, mcaPresetRealTime_, &Preset);
        if (Preset > 0) {
            realTimeDone = LastReadTime_ - StartTime_[addr] > 1000.0 * Preset;
        }
        else {
            realTimeDone = 0;
        }
        getDoubleParam(addr, mcaPresetLiveTime_, &Preset);
        if (Preset > 0) {
            liveTimeDone = LastReadTime_ - StartTime_[addr] > 1000.0 * Preset;
        }
        else {
            liveTimeDone = 0;
        }
        mcsDone = 0;
        mcaEnable = 1;
        if (countDone || realTimeDone || liveTimeDone || mcsDone || !mcaEnable) {
            // Some preset is reached.  If acquiring_ is true then stop detector
            if (acquiring_) {
                ret = CAEN_DGTZ_SWStopAcquisition(handle_);
                if (ret == CAEN_DGTZ_Success) {
                    acquiring_ = false;
                    setIntegerParam(mcaAcquiring_, acquiring_);
                }
                else {
                    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                            "Error stopping acquisition");
                    checkFailedComm("writeInt32");
                    status = asynError;
                }
            }
        }
        *value = acquiring_;
    }
    else if (command == caenBoardTemp_){
        uint32_t board_temp;
        ret = CAEN_DGTZ_ReadTemperature(handle_, addr, &board_temp);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling ReadTemperature(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        *value = board_temp;
    }
    else if (command == caenTrigThresh_){
        *value = DPPParams_.thr[addr];
    }
    else if (command == caenTrapRise_){
        *value = DPPParams_.k[addr];
    }
    else if (command == caenTrapFlatTop_){
        *value = DPPParams_.m[addr];
    }
    else if (command == caenDecayTime_){
        *value = DPPParams_.M[addr];
    }
    else if (command == caenFlatTopDelay_){
        *value = DPPParams_.ftd[addr];
    }
    else if (command == caenTrigFiltSmoo_){
        *value = DPPParams_.a[addr];
    }
    else if (command == caenInputRiseTime_){
        *value = DPPParams_.b[addr];
    }
    else if (command == caenTrigHoldOff_){
        *value = DPPParams_.trgho[addr];
    }
    else if (command == caenNBaseline_){
        *value = DPPParams_.nsbl[addr];
    }
    else if (command == caenNSPeak_){
        *value = DPPParams_.nspk[addr];
    }
    else if (command == caenPeakHoldOff_){
        *value = DPPParams_.pkho[addr];
    }
    else if (command == caenBLHoldOff_){
        *value = DPPParams_.blho[addr];
    }
    else if (command == caenDecimation_){
        *value = DPPParams_.decimation[addr];
    }
    else if (command == caenDecGain_){
        *value = DPPParams_.dgain[addr];
    }
    else if (command == caenTrigWin_){
        *value = DPPParams_.trgwin[addr];
    }
    else if (command == caenTWWDT_){
        *value = DPPParams_.twwdt[addr];
    }
    else if (command == caenDCOffset_){
        *value = Params_.DCOffset[addr];
    }
    else if (command == caenPreTriggerSize_){
        *value = Params_.PreTriggerSize[addr];
    }
    else if (command == caenPulsePolarity_){
        *value = Params_.PulsePolarity[addr];
    }
    else if (command == caenRegAddr_){
        getIntegerParam(caenRegAddr_, value);
    }
    else if (command == caenRegister_){
        int regAddr;
        getIntegerParam(caenRegAddr_, &regAddr);
        ret = CAEN_DGTZ_ReadRegister(handle_, regAddr, (uint32_t*) value);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling ReadRegister(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Read register, ret = %d\n",ret);
    }
    else {
        status = asynPortDriver::readInt32(pasynUser, value);
    }
    if (status != asynSuccess){
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) Error reading Int32 (command = %d)\n",
                driverName, functionName, portName, command);
    }
    return status;
}

asynStatus drvCaen::readFloat64(asynUser *pasynUser, epicsFloat64 *value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    static const char *functionName = "readFloat64";

    int addr;

    if (isConnected_ == false)
        return asynDisconnected;
    getAddress(pasynUser, &addr);

    if (command == mcaElapsedLiveTime_) {
        *value = 1e-9 * (LastReadTime_ - StartTime_[addr]);
    }
    else if (command == mcaElapsedRealTime_) {
        *value = 1e-9 * (LastReadTime_ - StartTime_[addr]);
    }
    else if (command == mcaElapsedCounts_) {
        *value = TrgCnt[addr];
    }
    else if (command == caenEnergyNF_){
        *value = DPPParams_.enf[addr];
    }
    else {
        status = asynPortDriver::readFloat64(pasynUser, value);
    }
    if (status != asynSuccess){
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) Error reading Float64 (command = %d)\n",
                driverName, functionName, portName, command);
    }
    return status;
}


asynStatus drvCaen::writeFloat64(asynUser *pasynUser, epicsFloat64 value)
{
    int command = pasynUser->reason;
    asynStatus status=asynSuccess;
    static const char *functionName = "writeFloat64";
    int ret = 0;

    int addr;
    if (isConnected_ == false)
        return asynDisconnected;

    getAddress(pasynUser, &addr);

    if (command == caenEnergyNF_){
        DPPParams_.enf[addr] = value;
        ret = CAEN_DGTZ_SetDPPParameters(handle_, 1<<addr, &DPPParams_);
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling SetDPPPParameters(). Err %d\n",
                    driverName, functionName, ret);
            status = asynError;
        }
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Setting energy normalization factor, ret = %d\n",ret);
    }
//    else (command == caenPollPeriod_){


    /* Set the parameter in the parameter library. */
    status = setDoubleParam(addr, command, value);

    callParamCallbacks(addr);
    if (status != asynSuccess){
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) Error writing Float64 (command = %d)\n",
                driverName, functionName, portName, command);
    }
    return status;
}

asynStatus drvCaen::readInt32Array(asynUser *pasynUser,
        epicsInt32 *data, size_t maxChans, 
        size_t *nactual)
{
    unsigned int numChannels;
    static const char *functionName="readInt32Array";
    int addr;

    if (isConnected_ == false)
        return asynDisconnected;

    getAddress(pasynUser, &addr);
    /* Check ADC index */
    if(addr >= MAX_ADCS || addr < 0){
        *nactual = 0;
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) ADC Index %d does not exist\n",
                driverName, functionName, portName, addr);
        return asynError;
    }

    /* If channel is disabled, don't process data */
    if (!(Params_.ChannelMask & (1<<addr))){
        *nactual = 0;
        return asynSuccess;
    }

    getIntegerParam(caenADC_NBits_,(epicsInt32*) &numChannels);
    numChannels = 1<<numChannels;
    if (numChannels > (unsigned int)maxChans){
        epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,
                "%s::%s Digitizer (%s) Board provided more channels than available: cropping histogram.\n",
                driverName, functionName, portName);
        numChannels = maxChans;
    }
    for (unsigned int i=0; i<numChannels; i++) {
        data[i] = EHisto[addr][i];
        //        EHisto[addr][i] = 0; // Clear buffer
    }
    *nactual = numChannels;
    epicsSnprintf(pasynUser->errorMessage, pasynUser->errorMessageSize,"Numchannels = %d\n",numChannels);
    return asynSuccess;
}

void readThread(void *drvPvt)
{
    drvCaen *pPvt = (drvCaen *)drvPvt;
    pPvt->readThread();
}

void drvCaen::readThread(void)
{
    double pollPeriod;
    int ret;
    static const char *functionName = "readThread";

    lock();
    /* Loop until it's time to exit */
    while (1) {
        if (exiting_) break;
        // TODO: make it so that pollPeriod accounts for thread run time
        getDoubleParam(caenPollPeriod_, &pollPeriod);
        // Release the lock while we wait for a command to start or wait for pollPeriod
        unlock();
        if (acquiring_) epicsEventWaitWithTimeout(eventId_, pollPeriod);
        else     (void) epicsEventWait(eventId_);
        // Take the lock again
        lock();
        /* acquisition might have stopped while we were waiting */
        if (!acquiring_) continue;
        ret = CAEN_DGTZ_ReadData(handle_, CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT, buffer_, &BufferSize_);
        LastReadTime_ = get_time_ns();
        if (ret) {
            asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                    "%s::%s error calling ReadData(). Err %d\n",
                    driverName, functionName, ret);
            checkFailedComm(functionName);
        }

        if(BufferSize_>0){
            ret |= CAEN_DGTZ_GetDPPEvents(handle_, buffer_, BufferSize_, (void**)Events_, NumEvents_);
            if (ret) {
                asynPrint(pasynUserSelf, ASYN_TRACE_ERROR,
                        "%s::%s error calling GetDPPEvents(). Err %d\n",
                        driverName, functionName, ret);
            }
            epicsSnprintf(pasynUserSelf->errorMessage, pasynUserSelf->errorMessageSize,"Reading board, ret = %d\n",ret);
        }
        for (unsigned int addr = 0; addr < MAX_ADCS; addr++){
            /* If channel is disabled, don't process data */
            if (!(Params_.ChannelMask & (1<<addr))){
                continue;
            }

            /* Update Histograms */
            for (unsigned int ev = 0; ev < NumEvents_[addr]; ev++) {
                TrgCnt[addr]++;
                /* Time Tag */
                if (Events_[addr][ev].TimeTag < PrevTime[addr]) 
                    ExtendedTT[addr]++;
                /* Energy */
                if (Events_[addr][ev].Energy >= 0) {
                    // Fill the histograms
                    //EHisto[addr][(Events[addr][ev].TimeTag)&BitMask]++;
                    EHisto[addr][(Events_[addr][ev].Energy)&BitMask_]++;
                    ECnt[addr]++;
                    THisto[addr][(Events_[addr][ev].TimeTag-PrevTime[addr])&BitMask_]++;
                } else {  /* PileUp */
                    PurCnt[addr]++;
                }
                PrevTime[addr] = Events_[addr][ev].TimeTag;
            } // loop on events
              // Make sure events don't get read twice
              //NumEvents_[addr] = 0;
        }
    }
    unlock();
}

extern "C" {
    int drvCaenConfigure(const char *portName, int LinkType, int LinkNum, int ConetNode, int VMEBaseAddress)
    {
        new drvCaen(portName, (CAEN_DGTZ_ConnectionType) LinkType, LinkNum, ConetNode, VMEBaseAddress);
        return asynSuccess;
    }

    static const iocshArg configArg0 = { "portName", iocshArgString};
    static const iocshArg configArg1 = { "LinkType", iocshArgInt};
    static const iocshArg configArg2 = { "LinkNum", iocshArgInt};
    static const iocshArg configArg3 = { "ConetNode", iocshArgInt};
    static const iocshArg configArg4 = { "VMEBaseAddress", iocshArgInt};
    static const iocshArg * const configArgs[] = {&configArg0,
        &configArg1,
        &configArg2,
        &configArg3,
        &configArg4};
    static const iocshFuncDef configFuncDef = {"drvCaenConfigure", 5, configArgs};
    static void configCallFunc(const iocshArgBuf *args)
    {
        drvCaenConfigure(args[0].sval, args[1].ival, args[2].ival, args[3].ival, args[4].ival);
    }

    void drvCaenRegister(void)
    {
        iocshRegister(&configFuncDef,configCallFunc);
    }

    epicsExportRegistrar(drvCaenRegister);

}
