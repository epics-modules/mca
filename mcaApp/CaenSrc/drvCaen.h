/* File:    drvCaen.cpp
 * Author:  Pedro Nariyoshi, Facility for Rare Isotope Beams
 * Date:    18-January-2024
 *
 * Purpose:
 * This module provides the driver support for the MCA asyn device support layer
 * for C.A.E.N. MCAs that support the CAENDigitizer library.
 *
 */

#ifndef DRVCAEN_H
#define DRVCAEN_H

/************/
/* Includes */
/************/

/* EPICS includes */
#include <asynPortDriver.h>
#include <epicsTypes.h>
#include <epicsThread.h>

/* CAEN includes */
#include <CAENDigitizerType.h>
#include <CAENDigitizer.h>

/* Local includes */
#include "ParamStruct.h"
#include "DigitizerFunctions.h"

/* Std libraries */
#include <string>
#include <string>
#include <vector>
#include <cinttypes>

// CAEN-specific board parameter strings
#define caenModelNameString   "CAEN_MODEL_NAME"
#define caenModelString   "CAEN_MODEL"
#define caenNADCsString   "CAEN_CHANNELS"
#define caenFormFactorString   "CAEN_FORM_FACTOR"
#define caenFamilyCodeString   "CAEN_FAMILY_CODE"
#define caenROCFirmwareString   "CAEN_ROC_FIRMWARE"
#define caenAMCFirmwareString   "CAEN_AMC_FIRMWARE"
#define caenSerialNumberString   "CAEN_SERIAL_NUMBER"
#define caenMezzanineSerNumString   "CAEN_MEZZ_SER_NUM"
#define caenPCBRevisionString   "CAEN_PCB_REV"
#define caenADCNBITSString   "CAEN_ADC_NBITS"
#define caenSAMCorrectionString   "CAEN_SAM_CORRECTION"
#define caenCommHandleString   "CAEN_COMM_HANDLE"
#define caenVMEHandleString   "CAEN_VME_HANDLE"
#define caenLicenseString   "CAEN_LICENSE"
#define caenDCOffsetString   "CAEN_DC_OFFSET"
#define caenPreTriggerSizeString   "CAEN_PRETRIG_SIZE"
#define caenPulsePolarityString "CAEN_PULSE_POL"
#define caenBoardTempString   "CAEN_BOARD_TEMP"
// CAEN-specific channel parameter strings
#define caenTrigThreshString   "CAEN_TRIG_THRESH" // Trigger Threshold (in LSB)
#define caenTrapRiseString   "CAEN_TRAP_RISE" // Trapezoid Rise Time (ns) 
#define caenTrapFlatTopString   "CAEN_TRAP_FT"  // Trapezoid Flat Top  (ns) 
#define caenDecayTimeString   "CAEN_DECAY_TIME" // Decay Time Constant (ns) 
#define caenFlatTopDelayString   "CAEN_FLAT_DELAY" // Flat top delay (peaking time) (ns) 
#define caenTrigFiltSmooString   "CAEN_TRIG_FILT_SMOO" // Trigger Filter smoothing factor (number of samples to average for RC-CR2 filter) Options: 1; 2; 4; 8; 16; 32
#define caenInputRiseTimeString   "CAEN_INPUT_RISE" // Input Signal Rise time (ns) 
#define caenTrigHoldOffString   "CAEN_TRIG_HOFF" // Trigger Hold Off
#define caenNBaselineString   "CAEN_NSBL" //number of samples for baseline average calculation. Options: 1->16 samples; 2->64 samples; 3->256 samples; 4->1024 samples; 5->4096 samples; 6->16384 samples
#define caenNSPeakString   "CAEN_NSPK" //Peak mean (number of samples to average for trapezoid height calculation). Options: 0-> 1 sample; 1->4 samples; 2->16 samples; 3->64 samples
#define caenPeakHoldOffString   "CAEN_PKHO" //peak holdoff (ns)
#define caenBLHoldOffString   "CAEN_BLHO" //Baseline holdoff (ns)
#define caenEnergyNFString   "CAEN_ENF" // Energy Normalization Factor
#define caenDecimationString   "CAEN_DECIMATION" //decimation (the input signal samples are averaged within this number of samples): 0 ->disabled; 1->2 samples; 2->4 samples; 3->8 samples
#define caenDecGainString   "CAEN_DEC_GAIN" //decimation gain. Options: 0->DigitalGain=1; 1->DigitalGain=2 (only with decimation >= 2samples); 2->DigitalGain=4 (only with decimation >= 4samples); 3->DigitalGain=8( only with decimation = 8samples).
#define caenTrigWinString   "CAEN_TRIG_WIN" //Enable Rise time Discrimination. Options: 0->disabled; 1->enabled
#define caenTWWDTString   "CAEN_TW_WDT" // Rise Time Validation Window (ns)
                                        // CAEN-specific miscellaneous parameter strings
#define caenRegAddrString   "CAEN_REGADDR" // Select the register address
#define caenRegisterString   "CAEN_REGISTER" // Direct manipulation of registers
#define caenPollPeriodString     "CAEN_POLLPERIOD" // Select the poll period for the internal thread

// Maximum number of channels
#define MAX_ADCS MAX_DPP_PHA_CHANNEL_SIZE
// Number of failed sends needed to disconnect
#define MAX_FAILED_SENDS 10

class drvCaen : public asynPortDriver
{
    public:
        drvCaen(const char *portName, CAEN_DGTZ_ConnectionType LinkType, int LinkNum, int ConetNode, unsigned int VMEBaseAddress);
        ~drvCaen();
        void exitHandler();

        // These are the methods we override from asynPortDriver
        //  virtual asynStatus getBounds(asynUser *pasynUser, epicsInt32 *low, epicsInt32 *high);
        virtual asynStatus writeInt32(asynUser *pasynUser, epicsInt32 value);
        virtual asynStatus readInt32(asynUser *pasynUser, epicsInt32 *value);
        virtual asynStatus writeFloat64(asynUser *pasynUser, epicsFloat64 value);
        virtual asynStatus readFloat64(asynUser *pasynUser, epicsFloat64 *value);
        virtual asynStatus readInt32Array(asynUser *pasynUser, epicsInt32 *data, 
                size_t maxChans, size_t *nactual);
        //  virtual void report(FILE *fp, int details);
        virtual asynStatus connect(asynUser *pasynUser);
        //  virtual asynStatus disconnect(asynUser *pasynUser);
        //  virtual asynStatus readOption(asynUser *pasynUser, const char *key, char *value, int maxChars);
        //  virtual asynStatus writeOption(asynUser *pasynUser, const char *key, const char *value);

        // These are the methods that are new to this class
        void readThread(void);

    protected:
        // These are the standard MCA commands
#define FIRST_CAEN_PARAM mcaStartAcquire_
        int mcaStartAcquire_;
        int mcaStopAcquire_;
        int mcaErase_;
        int mcaData_;
        int mcaReadStatus_;
        int mcaChannelAdvanceSource_;
        int mcaNumChannels_;
        int mcaDwellTime_;
        int mcaPresetLiveTime_;
        int mcaPresetRealTime_;
        int mcaPresetCounts_;
        int mcaPresetLowChannel_;
        int mcaPresetHighChannel_;
        int mcaPresetSweeps_;
        int mcaAcquireMode_;
        int mcaSequence_;
        int mcaPrescale_;
        int mcaAcquiring_;
        int mcaElapsedLiveTime_;
        int mcaElapsedRealTime_;
        int mcaElapsedCounts_;

        // These are the CAEN-specific board commands
        int caenModelName_;
        int caenModel_;
        int caenNADCs_;
        int caenFormFactor_;
        int caenFamilyCode_;
        int caenROC_FirmwareRel_;
        int caenAMC_FirmwareRel_;
        int caenSerialNumber_;
        int caenMezzSerNum_; //used only for x743 boards
        int caenPCB_Revision_;
        int caenADC_NBits_;
        int caenSAMCorrectionDataLoaded_; //used only for x743 boards;
        int caenCommHandle_;
        int caenVMEHandle_;
        int caenLicense_;
        int caenBoardTemp_;

        // CAEN-specific channel parameter strings
        int caenDCOffset_;
        int caenPulsePolarity_;
        int caenPreTriggerSize_;
        int caenTrigThresh_;
        int caenTrapRise_;
        int caenTrapFlatTop_;
        int caenDecayTime_;
        int caenFlatTopDelay_;
        int caenTrigFiltSmoo_;
        int caenInputRiseTime_;
        int caenTrigHoldOff_;
        int caenNBaseline_;
        int caenNSPeak_;
        int caenPeakHoldOff_;
        int caenBLHoldOff_;
        int caenEnergyNF_;
        int caenDecimation_;
        int caenDecGain_;
        int caenOTRej_;
        int caenTrigWin_;
        int caenTWWDT_;

        // CAEN-specific miscellaneous parameter strings
        int caenRegAddr_;
        int caenRegister_;
        int caenPollPeriod_;
#define LAST_CAEN_PARAM caenPollPeriod_

    private:
        epicsEventId eventId_;
        epicsThreadId threadId_;
        bool exiting_;

        // Setting/parameter variables
        DigitizerParams_t Params_; // Parameter struct
        CAEN_DGTZ_DPP_PHA_Params_t DPPParams_; // DPP Parameter struct

        // Data-analysis variables/buffers
        uint64_t StartTime_[MAX_ADCS];
        uint64_t LastReadTime_;
        uint32_t AllocatedSize_, BufferSize_;
        char *buffer_ = NULL;                                 // readout buffer
        CAEN_DGTZ_DPP_PHA_Event_t       *Events_[MAX_ADCS];  // events buffer
        CAEN_DGTZ_DPP_PHA_Waveforms_t   *Waveform_=NULL;     // waveforms buffer
        uint64_t PrevTime[MAX_ADCS];
        uint64_t ExtendedTT[MAX_ADCS];
        uint32_t NumEvents_[MAX_ADCS]; // Energy Histograms
        uint32_t *EHisto[MAX_ADCS]; // Energy Histograms
        uint32_t *THisto[MAX_ADCS]; // Timing Histograms
        uint64_t TrgCnt[MAX_ADCS]; // Cumulative Trigger Counts
        uint64_t ECnt[MAX_ADCS];   // Trigger Counts with valid energy
        uint64_t PurCnt[MAX_ADCS]; // Trigger Counts with invalid energy (Pile-Up)
                                   // Driver variables
        int handle_;
        int BitMask_;
        //  asynStatus disconnectDevice();
        asynStatus connectDevice(asynUser *pasynUser);
        bool       directConnect(char* address);
        void       checkFailedComm(const char *functionName);
        //  size_t numChannels_;
        //  asynStatus findModule();
        //  asynStatus sendCommand(TRANSMIT_PACKET_TYPE command);
        //  asynStatus sendCommandString(string commandString);
        //  asynStatus sendConfiguration();
        //  asynStatus sendSCAs();
        //  asynStatus sendConfigurationFile(string fileName);
        //  asynStatus saveConfigurationFile(string fileName);
        //  asynStatus readConfigurationFromHardware();
        //  asynStatus parseConfiguration();
        //  asynStatus parseConfigDouble(const char *str, int param);
        //  asynStatus parseConfigInt(const char *str, int param);
        asynStatus allocateBuffers();
        //  asynStatus parseConfigEnum(const char *str, const char *enumStrs[], int numEnums, int param);
        //  dp5DppTypes dppType_;
        bool acquiring_;
        //  bool haveConfigFromHW_;
        //  DppInterface_t interfaceType_;
        //  char *addressInfo_;
        //  bool directMode_;
        int  failedSends_;
        bool isConnected_;
        void       setParamsAlarm(int alarmStatus, int alarmSeverity);
};

#define NUM_PARAMS (&LAST_CAEN_PARAM - &FIRST_CAEN_PARAM + 1)

#endif

