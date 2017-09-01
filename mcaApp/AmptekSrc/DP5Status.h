/** CDP5Status CDP5Status */

#pragma once
#include <stdio.h>
#include <stdlib.h>
#include "DppImportExport.h"
#include "DP5Protocol.h"
#include "DppUtilities.h"

typedef enum _PX5_OPTIONS
{
	PX5_OPTION_NONE=0,
	PX5_OPTION_HPGe_HVPS=1,
	PX5_OPTION_TEST_TEK=4,
	PX5_OPTION_TEST_MOX=5,
	PX5_OPTION_TEST_AMP=6,
	PX5_OPTION_TEST_MODE_1=8,
	PX5_OPTION_TEST_MODE_2=9
} PX5_OPTIONS;

//PX5_OPTION_TEST_MODE_1 reserved for future use
//PX5_OPTION_TEST_MODE_2 reserved for future use

typedef struct DP5_DP4_FORMAT_STATUS
{
    unsigned char RAW[64];
    unsigned long SerialNumber;
    double FastCount;
    double SlowCount;
    unsigned char FPGA;
    unsigned char Firmware;
	unsigned char Build;
    double AccumulationTime;
    double RealTime;
    double LiveTime;
    double HV;
    double DET_TEMP;
    double DP5_TEMP;
    bool PX4;
    bool AFAST_LOCKED;
    bool MCA_EN;
    bool PRECNT_REACHED;
	bool PresetRtDone;
	bool PresetLtDone;
    bool SUPPLIES_ON;
    bool SCOPE_DR;
    bool DP5_CONFIGURED;
    double GP_COUNTER;
    bool AOFFSET_LOCKED;
    bool MCS_DONE;
    bool RAM_TEST_RUN;
    bool RAM_TEST_ERROR;
    double DCAL;
    unsigned char PZCORR;				// or single?
    unsigned char UC_TEMP_OFFSET;
    double AN_IN;
    double VREF_IN;
    unsigned long PC5_SN;
    bool PC5_PRESENT;
    bool PC5_HV_POL;
    bool PC5_8_5V;
    double ADC_GAIN_CAL;
    unsigned char ADC_OFFSET_CAL;
    long SPECTRUM_OFFSET;     // or single?
	bool b80MHzMode;
	bool bFPGAAutoClock;
	unsigned char DEVICE_ID;
	bool ReBootFlag;
	unsigned char DPP_options;
	bool HPGe_HV_INH;
	bool HPGe_HV_INH_POL;
	double TEC_Voltage;
	unsigned char DPP_ECO;
	bool AU34_2;
	bool isAscInstalled;
	bool isAscEnabled;
	bool bScintHas80MHzOption;
	bool isDP5_RevDxGains;
} DP4_FORMAT_STATUS, *PDP4_FORMAT_STATUS;

typedef struct _DiagDataType
{
    float ADC_V[12];
    float PC5_V[3];
    bool PC5_PRESENT;
    long PC5_SN;
    unsigned char Firmware;
    unsigned char FPGA;
    bool SRAMTestPass;
    long SRAMTestData;
    int TempOffset;
    string strTempRaw;
    string strTempCal;
    bool PC5Initialized;
    float PC5DCAL;
    bool IsPosHV;
    bool Is8_5VPreAmp;
    bool Sup9VOn;
    bool PreAmpON;
    bool HVOn;
    bool TECOn;
    unsigned char DiagData[192];
} DiagDataType, *PDDiagDataType;

class IMPORT_EXPORT CDP5Status
{
public:
	CDP5Status(void);
	~CDP5Status(void);

	/// Utilities to help data processing.
	CDppUtilities DppUtil;

	/// DPP status storage.
	DP4_FORMAT_STATUS m_DP5_Status;
	/// DPP diagnostic data storage.
	DiagDataType DiagData;
	/// Convert a DPP status packet into DP4_FORMAT_STATUS data.
	void Process_Status(DP4_FORMAT_STATUS *m_DP5_Status);
	string DP5_Dx_OptionFlags(unsigned char DP5_Dx_Options);
	/// Convert DP4_FORMAT_STATUS data into a status display string.
	string ShowStatusValueStrings(DP4_FORMAT_STATUS m_DP5_Status);
	string PX5_OptionsString(DP4_FORMAT_STATUS m_DP5_Status);
	string GetStatusValueStrings(DP4_FORMAT_STATUS m_DP5_Status);

	/// Convert a DPP diagnostic data packet into DiagDataType data.
	void Process_Diagnostics(Packet_In PIN, DiagDataType *dd, int device_type);
	/// Convert DiagDataType data into a diagnostic display string.
	string DiagnosticsToString(DiagDataType dd, int device_type);
	/// Convert PX5 Options into a display string.
	string DiagStrPX5Option(DiagDataType dd, int device_type);

	/// Format a high voltage power value.
	string FmtHvPwr(float fVal);
	/// Format a pc5 power value.
	string FmtPc5Pwr(float fVal);
	/// Format a pc5 temperature value.
	string FmtPc5Temp(float fVal) ;
	/// Format a hexadecimal value.
	string FmtHex(long FmtHex, long HexDig);
	/// Format a long value.
	string FmtLng(long lVal);
	/// Format a version number value.
	string VersionToStr(unsigned char bVersion);
	/// Format an OnOFF indicator value.
	string OnOffStr(bool bOn);
	/// Format a boolean indicator value.
	string IsAorB(bool bIsA, string strA, string strB);
	/// Format a device type indicator as a device name string.
	string GetDeviceNameFromVal(int DeviceTypeVal) ;
	/// Format an string array from a data byte array of values.
	string DisplayBufferArray(unsigned char buffer[], unsigned long bufSizeIn);
	/// Saves a data string to a default file (vcDP5_Data.txt).
	void SaveStringDataToFile(string strData);

};




















