/** CConsoleHelper simplifies DPP spectrum acquisition.
 *  Coordinates DPP communications and spectrum acquisition.
 *
 */

#pragma once

#include <string>
#include <vector>
#include "DppSocket.h"			// Socket Support
#include "DP5Protocol.h"		// DPP Protocol Support
#include "ParsePacket.h"		// Packet Parser
#include "SendCommand.h"		// Command Generator
#include "DP5Status.h"			// Status Decoder
#include <time.h>				// time library for rand seed

typedef struct _SpectrumFileType {
    string strTag;
    string strDescription;
    short m_iNumChan;
	unsigned long SerialNumber;
	string strSpectrumConfig;
	string strSpectrumStatus;
} SpectrumFileType;

const bool bConsoleHelperDebug = false;

class CConsoleHelper
{
public:
	CConsoleHelper(void);
	~CConsoleHelper(void);

	/// dpp socket communications class.
	CDppSocket DppSocket;
	/// DppSocket is connected if true.
	bool DppSocket_isConnected;
	/// DppSocket number of devices found.
	int DppSocket_NumDevices;
	/// DppSocket connect to the default DPP.
	bool DppSocket_Connect_Default_DPP(char szDPP_Send[]);
	/// DppSocket connect directly to a selected DPP.
	bool DppSocket_Connect_Direct_DPP(char szDPP_Send[]);
	/// DppSocket close the current connection.
	void DppSocket_Close_Connection();
	/// DppSocket send a command that does not require additional processing.
	bool DppSocket_SendCommand(TRANSMIT_PACKET_TYPE XmtCmd);
	/// DppSocket send a command that requires configuration options processing.
	bool DppSocket_SendCommand_Config(TRANSMIT_PACKET_TYPE XmtCmd, CONFIG_OPTIONS CfgOptions);
	///  DppSocket receive data.
	bool DppSocket_ReceiveData();

	int doNetFinderBroadcast(CDppSocket *DppSock, char addrArr[][20], bool bNewSearch=false);
	//void doAmptekNetFinderPacket(CDppSocket *DppSock, char szDPP_Send[]);
	int doAmptekNetFinderPacket(CDppSocket *DppSock, char szDPP_Send[]);
	void doSpectrumAndStatus(CDppSocket *DppSock, char szDPP_Send[]);

	// communications helper functions

	/// Defines and implements DPP protocol.
	CDP5Protocol DP5Proto;
	/// Generates command packet to be sent.
	CSendCommand SndCmd;
	/// DPP packet parsing.
	CParsePacket ParsePkt;
	/// DPP status processing.
	CDP5Status DP5Stat;			
	
	// DPP packet processing functions.

	/// Processes DPP data from all communication interfaces (USB,RS232,INET)
	bool ReceiveData();
	/// Processes spectrum packets.
	void ProcessSpectrumEx(Packet_In PIN, DppStateType DppState);
	/// Clears configuration readback format flags. 
	void ClearConfigReadFormatFlags();
	/// Processes configuration packets.
	void ProcessCfgReadEx(Packet_In PIN, DppStateType DppState);
	/// Populates the configuration command options data structure.
	void CreateConfigOptions(CONFIG_OPTIONS *CfgOptions, string strCfg, CDP5Status DP5Stat, bool bUseCoarseFineGain);

	//display support

	/// Provides a low resolution text console graph.
	void ConsoleGraph(long lData[], long chan, bool bLog, std::string strStatus);
	/// DPP status display string.
	string DppStatusString;

	// DPP configuration information variables 

	/// FPGA 80MHz clock when true, 20MHz clock otherwise.
	bool b80MHzMode;
	/// Holds MCA MODE display string. (NORM=MCA, MCS, FAST, etc.)
	string strMcaMode;
	/// DPP configuration command array.
	vector<string> Dp5CmdList;

	// configuration readback format control flags
	// these flags control how the configuration readback is formatted and processed

	/// format configuration for display
	bool DisplayCfg;
	/// format sca for display (sca config sent separately)
	bool DisplaySca;
	/// format configuration for general readback
	bool CfgReadBack;
	/// format configuration for file save
	bool SaveCfg;
	/// format configuration for print
	bool PrintCfg;
	/// configuration ready flag
	bool HwCfgReady;
	/// sca readback ready flag
	bool ScaReadBack;

	// Tuning and display variables.

	/// Holds the hardware configuration readback.
	string HwCfgDP5;
	/// Number of data channels.
	int mcaCH;
	/// Slow threshold in percent.
	double SlowThresholdPct;
	/// Fast channel threshold.
	int FastChThreshold;
	/// Peaking time value.
	double RiseUS;
	/// Total gain display string.
	string strGainDisplayValue;
	/// Acquisition mode. (0=MCA, 1=MCS)
	int AcqMode;				

	// configuration presets for display (status updates preset progress)

	/// preset count setting
	int PresetCount;
	/// preset acquisition time setting
	double PresetAcq;
	/// preset real time setting
	double PresetRt;
	/// presets mode summary (counts,accum. time,real time)
	string strPresetCmd;
	/// presets settings summary (preset values, counts,times)
	string strPresetVal;

	// configuration support functions

	/// Returns the configuration command data from a configuration command string.
	string GetCmdData(string strCmd, string strCfgData);
	/// Replaces (or inserts a command description (comment) in a configuration command string.
	string ReplaceCmdDesc(string strCmd, string strCfgData);
	/// Appends a command description (comment) in a configuration command string.
	string AppendCmdDesc(string strCmd, string strCfgData);
	/// Returns the command decription (comment) in a configuration command string.
	string GetCmdDesc(string strCmd);

	// oscilloscope support


	bool UpdateScopeCfg;
	string strInputOffset;
	string strAnalogOut;
	string strOutputOffset;
	string strTriggerSource;
	string strTriggerSlope;
	string strTriggerPosition;
	string strScopeGain;

    string CreateMCAData(long m_larDataBuffer[], SpectrumFileType sfInfo, DP4_FORMAT_STATUS cfgStatusLst);
	/// Saves a spectrum data string to a default file (SpectrumData.mca).
	void SaveSpectrumStringToFile(string strData);
    string CreateSpectrumConfig(string strRawCfgIn) ;
	vector<string> MakeDp5CmdList();
	SpectrumFileType sfInfo;

};






















