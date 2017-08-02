/** gccDppConsoleInet.cpp */

// gccDppConsoleInet.cpp : Defines the entry point for the console application.
#include <iostream>
using namespace std; 
#include "ConsoleHelper.h"
#ifndef WIN32
	#include <unistd.h>
	#define _getch getchar
#endif

#define INET_ACQUIRE_DEMO 1

CConsoleHelper chdpp;					// DPP communications functions
bool bRunSpectrumTest = false;			// run spectrum test
bool bRunConfigurationTest = false;		// run configuration test
bool bHaveStatusResponse = false;		// have status response
bool bHaveConfigFromHW = false;			// have configuration from hardware
int iPreset = 11;

bool bRunAll = true;
bool bRunUpToAcquire = true;
bool bRunUpToStatus = true;
bool bRunFindDppOnly = true;

enum DemoMode {
	DEMO_ALL,
	DEMO_ACQUIRE,
	DEMO_STATUS,
	DEMO_FIND
};

void DemonstrationMode(DemoMode dmDemoSetup)
{
	switch(dmDemoSetup) {
	case DEMO_ALL:
		bRunAll = true;
		bRunUpToAcquire = true;
		bRunUpToStatus = true;
		bRunFindDppOnly = true;
		break;
	case DEMO_ACQUIRE:
		bRunAll = false;
		bRunUpToAcquire = true;
		bRunUpToStatus = true;
		bRunFindDppOnly = true;
		break;
	case DEMO_STATUS:
		bRunAll = false;
		bRunUpToAcquire = false;
		bRunUpToStatus = true;
		bRunFindDppOnly = true;
		break;
	case DEMO_FIND:
		bRunAll = false;
		bRunUpToAcquire = false;
		bRunUpToStatus = false;
		bRunFindDppOnly = true;
		break;
	default:           //if unknown set to DP5
		bRunAll = true;
		bRunUpToAcquire = true;
		bRunUpToStatus = true;
		bRunFindDppOnly = true;
		break;
	}

}

void ConsoleClear()
{
#ifdef WIN32
	system("cls");
#else
	system("clear");
#endif
}

void ConsolePause()
{
#ifdef WIN32
	system("Pause");
#else
	cout << "Press the Enter key to continue . . .";
	_getch(); 
#endif
}

// connect to default dpp
//		CConsoleHelper::DppSocket_Connect_Default_DPP	// Network connect to default DPP
bool ConnectToDefaultDPP(char szDPP_Send[])
{
	bool isConnected=false;
	cout << endl;
	cout << "Running DPP Network tests from console..." << endl;
	cout << endl;
	cout << "\tConnecting to default Network device..." << endl;
	//if (chdpp.DppSocket_Connect_Default_DPP(szDPP_Send)) {
	if (chdpp.DppSocket_Connect_Direct_DPP(szDPP_Send)) {
		cout << "\t\tNetwork DPP device connected." << endl;
		cout << "\t\tNetwork DPP devices present: "  << chdpp.DppSocket_NumDevices << endl;
		isConnected = true;
	} else {
		cout << "\t\tNetwork DPP device not connected." << endl;
		cout << "\t\tNo Network DPP device present." << endl;
	}
	return isConnected;
}

// Get DPP Status
//		CConsoleHelper::DppSocket_isConnected						// check if DPP is connected
//		CConsoleHelper::DppSocket_SendCommand(XMTPT_SEND_STATUS)	// request status
//		CConsoleHelper::DppSocket_ReceiveData()					// parse the status
//		CConsoleHelper::DppStatusString							// display status string
void GetDppStatus()
{
	if (chdpp.DppSocket_isConnected) { // send and receive status
		cout << "Connected, attempting to get status..." << endl;
		cout << endl;
		cout << "\tRequesting Status..." << endl;
		if (chdpp.DppSocket_SendCommand(XMTPT_SEND_STATUS)) {	// request status
			cout << "\t\tStatus sent." << endl;
			cout << "\t\tReceiving status..." << endl;
			if (chdpp.DppSocket_ReceiveData()) {
				cout << "\t\t\tStatus received..." << endl;
				cout << chdpp.DppStatusString << endl;
				bRunSpectrumTest = true;
				bHaveStatusResponse = true;
				bRunConfigurationTest = true;
			} else {
				cout << "\t\tError receiving status." << endl;
			}
		} else {
			cout << "\t\tError sending status." << endl;
		}
	} else {
		cout << "Not Connected" << endl;
	}
}

// Read Full DPP Configuration From Hardware			// request status before sending/receiving configurations
//		CONFIG_OPTIONS									// holds configuration command options
//		CConsoleHelper::CreateConfigOptions				// creates configuration options from last status read
//      CConsoleHelper::ClearConfigReadFormatFlags();	// clear configuration format flags, for cfg readback
//      CConsoleHelper::CfgReadBack = true;				// requesting general readback format
//		CConsoleHelper::DppSocket_SendCommand_Config		// send command with options
//		CConsoleHelper::DppSocket_ReceiveData()			// parse the configuration
//		CConsoleHelper::HwCfgReady						// config is ready
void ReadDppConfigurationFromHardware(bool bDisplayCfg)
{
	CONFIG_OPTIONS CfgOptions;
	if (bHaveStatusResponse && bRunConfigurationTest) {
		//test configuration functions
		// Set options for XMTPT_FULL_READ_CONFIG_PACKET
		chdpp.CreateConfigOptions(&CfgOptions, "", chdpp.DP5Stat, false);
		cout << endl;
		cout << "\tRequesting Full Configuration..." << endl;
		chdpp.ClearConfigReadFormatFlags();	// clear all flags, set flags only for specific readback properties
		//chdpp.DisplayCfg = false;	// DisplayCfg format overrides general readback format
		chdpp.CfgReadBack = true;	// requesting general readback format
		if (chdpp.DppSocket_SendCommand_Config(XMTPT_FULL_READ_CONFIG_PACKET, CfgOptions)) {	// request full configuration
			if (chdpp.DppSocket_ReceiveData()) {
				if (chdpp.HwCfgReady) {		// config is ready
					bHaveConfigFromHW = true;
					if (bDisplayCfg) {
						cout << "\t\t\tConfiguration Length: " << (unsigned int)chdpp.HwCfgDP5.length() << endl;
						cout << "\t================================================================" << endl;
						cout << chdpp.HwCfgDP5 << endl;
						cout << "\t================================================================" << endl;
						cout << "\t\t\tScroll up to see configuration settings." << endl;
						cout << "\t================================================================" << endl;
					} else {
						cout << "\t\tFull configuration received." << endl;
					}
				}
			}
		}
	}
}

// Display Preset Settings
//		CConsoleHelper::strPresetCmd	// preset mode
//		CConsoleHelper::strPresetVal	// preset setting
void DisplayPresets()
{
	if (bHaveConfigFromHW) {
		cout << "\t\t\tPreset Mode: " << chdpp.strPresetCmd << endl;
		cout << "\t\t\tPreset Settings: " << chdpp.strPresetVal << endl;
	}
}

// Display Preset Settings
//		CONFIG_OPTIONS								// holds configuration command options
//		CConsoleHelper::CreateConfigOptions			// creates configuration options from last status read
//		CConsoleHelper::HwCfgDP5Out					// preset setting
//		CConsoleHelper::DppSocket_SendCommand_Config	// send command with options
void SendPresetAcquisitionTime(string strPRET)
{
	CONFIG_OPTIONS CfgOptions;
	string strCmdData;
	cout << "\tSetting Preset Acquisition Time..." << strPRET << endl;
	chdpp.CreateConfigOptions(&CfgOptions, "", chdpp.DP5Stat, false);
	CfgOptions.HwCfgDP5Out = strPRET;
	strCmdData = chdpp.GetCmdData("PRET",strPRET); //preset actual time
	iPreset = atoi(strCmdData.c_str());
	// send PresetAcquisitionTime string, bypass any filters, read back the mode and settings
	if (chdpp.DppSocket_SendCommand_Config(XMTPT_SEND_CONFIG_PACKET_EX, CfgOptions)) {
		ReadDppConfigurationFromHardware(true);	// read setting back
		DisplayPresets();							// display new presets
	} else {
		cout << "\t\tPreset Acquisition Time NOT SET" << strPRET << endl;
	}
}

// Acquire Spectrum
//		CConsoleHelper::DppSocket_SendCommand(XMTPT_DISABLE_MCA_MCS)		//disable for data/status clear
//		CConsoleHelper::DppSocket_SendCommand(XMTPT_SEND_CLEAR_SPECTRUM_STATUS)  //clear spectrum/status
//		CConsoleHelper::DppSocket_SendCommand(XMTPT_ENABLE_MCA_MCS);		// enabling MCA for spectrum acquisition
//		CConsoleHelper::DppSocket_SendCommand(XMTPT_SEND_SPECTRUM_STATUS)) // request spectrum+status
//		CConsoleHelper::DppSocket_ReceiveData()							// process spectrum and data
//		CConsoleHelper::ConsoleGraph()	(low resolution display)		// graph data on console with status
//		CConsoleHelper::DppSocket_SendCommand(XMTPT_DISABLE_MCA_MCS)		// disable mca after acquisition
void AcquireSpectrum()
{
	int MaxMCA = 11;
	bool bDisableMCA;
	MaxMCA = iPreset + 1;
	//bRunSpectrumTest = false;		// disable test
	if (bRunSpectrumTest) {
		cout << "\tRunning spectrum test..." << endl;
		cout << "\t\tDisabling MCA for spectrum data/status clear." << endl;
		chdpp.DppSocket_SendCommand(XMTPT_DISABLE_MCA_MCS);
		Sleep(1000);
		cout << "\t\tClearing spectrum data/status." << endl;
		chdpp.DppSocket_SendCommand(XMTPT_SEND_CLEAR_SPECTRUM_STATUS);
		Sleep(1000);
		cout << "\t\tEnabling MCA for spectrum data acquisition with status ." << endl;
		chdpp.DppSocket_SendCommand(XMTPT_ENABLE_MCA_MCS);
		Sleep(1000);
		for(int idxSpectrum=0;idxSpectrum<MaxMCA;idxSpectrum++) {
			//cout << "\t\tAcquiring spectrum data set " << (idxSpectrum+1) << " of " << MaxMCA << endl;
			if (chdpp.DppSocket_SendCommand(XMTPT_SEND_SPECTRUM_STATUS)) {	// request spectrum+status
				if (chdpp.DppSocket_ReceiveData()) {
					bDisableMCA = true;				// we are aquiring data, disable mca when done
					ConsoleClear();
					chdpp.ConsoleGraph(chdpp.DP5Proto.SPECTRUM.DATA,chdpp.DP5Proto.SPECTRUM.CHANNELS,true,chdpp.DppStatusString);
					Sleep(1000);
				}
			} else {
				cout << "\t\tProblem acquiring spectrum." << endl;
				break;
			}
		}
		if (bDisableMCA) {
			//ConsolePause();
			//cout << "\t\tSpectrum acquisition with status done. Disabling MCA." << endl;
			chdpp.DppSocket_SendCommand(XMTPT_DISABLE_MCA_MCS);
			Sleep(1000);
		}
	}
}

// Read Configuration File
//		CConsoleHelper::SndCmd.GetDP5CfgStr("PX5_Console_Test.txt");
void ReadConfigFile()
{
	std::string strCfg;
	strCfg = chdpp.SndCmd.AsciiCmdUtil.GetDP5CfgStr("PX5_Console_Test.txt");
	cout << "\t\t\tConfiguration Length: " << (unsigned int)strCfg.length() << endl;
	cout << "\t================================================================" << endl;
	cout << strCfg << endl;
	cout << "\t================================================================" << endl;
}

//Following is an example of loading a configuration from file 
//then sending the configuration to the DPP device.
//	SendConfigFileToDpp("NaI_detector_cfg.txt");    // calls SendCommandString
//	AcquireSpectrum();
//
bool SendCommandString(string strCMD) {
    CONFIG_OPTIONS CfgOptions;
    chdpp.CreateConfigOptions(&CfgOptions, "", chdpp.DP5Stat, false);
    CfgOptions.HwCfgDP5Out = strCMD;
    // send ASCII command string, bypass any filters, read back the mode and settings
    if (chdpp.DppSocket_SendCommand_Config(XMTPT_SEND_CONFIG_PACKET_EX, CfgOptions)) {
        // command sent
    } else {
        cout << "\t\tASCII Command String NOT SENT" << strCMD << endl;
        return false;
    }
    return true;
}

std::string ShortenCfgCmds(std::string strCfgIn) {
    std::string strCfg("");
	strCfg = strCfgIn;
	long lCfgLen=0;						//ASCII Configuration Command String Length
    lCfgLen = (long)strCfg.length();
	if (lCfgLen > 0) {		
        strCfg = chdpp.SndCmd.AsciiCmdUtil.ReplaceCmdText(strCfg, "US;", ";");
        strCfg = chdpp.SndCmd.AsciiCmdUtil.ReplaceCmdText(strCfg, "OFF;", "OF;");
        strCfg = chdpp.SndCmd.AsciiCmdUtil.ReplaceCmdText(strCfg, "RISING;", "RI;");
        strCfg = chdpp.SndCmd.AsciiCmdUtil.ReplaceCmdText(strCfg, "FALLING;", "FA;");
	}
	return strCfg;
}

// run GetDppStatus(); first to get PC5_PRESENT, DppType
// Includes Configuration Oversize Fix 20141224
bool SendConfigFileToDpp(string strFilename){
    std::string strCfg;
	long lCfgLen=0;						//ASCII Configuration Command String Length
    bool bCommandSent=false;
	bool isPC5Present=false;
	int DppType=0;
	int idxSplitCfg=0;					//Configuration split position, only if necessary
	bool bSplitCfg=false;				//Configuration split flag
	std::string strSplitCfg("");		//Configuration split string second buffer
	bool isDP5_RevDxGains;
	unsigned char DPP_ECO;

	isPC5Present = chdpp.DP5Stat.m_DP5_Status.PC5_PRESENT;
	DppType = chdpp.DP5Stat.m_DP5_Status.DEVICE_ID;
	isDP5_RevDxGains = chdpp.DP5Stat.m_DP5_Status.isDP5_RevDxGains;
	DPP_ECO = chdpp.DP5Stat.m_DP5_Status.DPP_ECO;

    strCfg = chdpp.SndCmd.AsciiCmdUtil.GetDP5CfgStr(strFilename);
	strCfg = chdpp.SndCmd.AsciiCmdUtil.RemoveCmdByDeviceType(strCfg,isPC5Present,DppType,isDP5_RevDxGains,DPP_ECO);
    lCfgLen = (long)strCfg.length();
    if ((lCfgLen > 0) && (lCfgLen <= 512)) {		// command length ok
        cout << "\t\t\tConfiguration Length: " << lCfgLen << endl;
    } else if (lCfgLen > 512) {	// configuration too large, needs fix
		cout << "\t\t\tConfiguration Length (Will Shorten): " << lCfgLen << endl;
		strCfg = ShortenCfgCmds(strCfg);
		lCfgLen = (long)strCfg.length();
		if (lCfgLen > 512) {	// configuration still too large, split config
			cout << "\t\t\tConfiguration Length (Will Split): " << lCfgLen << endl;
            bSplitCfg = true;
            idxSplitCfg = chdpp.SndCmd.AsciiCmdUtil.GetCmdChunk(strCfg);
			cout << "\t\t\tConfiguration Split at: " << idxSplitCfg << endl;
            strSplitCfg = strCfg.substr(idxSplitCfg);
			strCfg = strCfg.substr(0, idxSplitCfg);
		}
    } else {
        cout << "\t\t\tConfiguration Length Error: " << lCfgLen << endl;
        return false;
    }
	bCommandSent = SendCommandString(strCfg);
	if (bSplitCfg) {
		// Sleep(40);			// may need delay here
		bCommandSent = SendCommandString(strSplitCfg);
	}
    return bCommandSent;
}

// Close Connection
//		CConsoleHelper::DppSocket_isConnected			// Network DPP connection indicator
//		CConsoleHelper::DppSocket_Close_Connection()	// close connection
void CloseConnection()
{
	if (chdpp.DppSocket_isConnected) { // send and receive status
		cout << endl;
		cout << "\tClosing connection to default Network device..." << endl;
		chdpp.DppSocket_Close_Connection();
		cout << "\t\tDPP device connection closed." << endl;
	}
}

// Helper functions for saving spectrum files
void SaveSpectrumConfig()
{
	string strSpectrumConfig;
	chdpp.Dp5CmdList = chdpp.MakeDp5CmdList();	// ascii text command list for adding comments
	strSpectrumConfig = chdpp.CreateSpectrumConfig(chdpp.HwCfgDP5);	// append configuration comments
	chdpp.sfInfo.strSpectrumConfig = strSpectrumConfig;
}

// Saving spectrum file
void SaveSpectrumFile()
{
	string strSpectrum;											// holds final spectrum file
	chdpp.sfInfo.strSpectrumStatus = chdpp.DppStatusString;		// save last status after acquisition
	chdpp.sfInfo.m_iNumChan = chdpp.mcaCH;						// number channels in spectrum
	chdpp.sfInfo.SerialNumber = chdpp.DP5Stat.m_DP5_Status.SerialNumber;	// dpp serial number
	chdpp.sfInfo.strDescription = "Amptek Spectrum File";					// description
    chdpp.sfInfo.strTag = "TestTag";										// tag
	// create spectrum file, save file to string
    strSpectrum = chdpp.CreateMCAData(chdpp.DP5Proto.SPECTRUM.DATA,chdpp.sfInfo,chdpp.DP5Stat.m_DP5_Status);
	chdpp.SaveSpectrumStringToFile(strSpectrum);	// save spectrum file string to file
}

int main(int argc, char * argv[])
{
	// DemonstrationMode(DEMO_ACQUIRE);			// DEMO_ALL, DEMO_ACQUIRE, DEMO_STATUS, DEMO_FIND

	// Find the DPP Device
	char szDPP_Send[20]={"10.87.112.238"};	// set the test DPP IP address here	
	ConsoleClear();

//=================================================================================================
// Added for Linux Broadcast Testing
//=================================================================================================
	//if (! chdpp.DppSocket_Connect_Default_DPP(szDPP_Send)) {
	//	ConsolePause();
	//	return 1;
	//} else {
	//	ConsolePause();
	//}
//=================================================================================================
// Added for Linux Broadcast Testing
//=================================================================================================

//  #ifdef INET_ACQUIRE_DEMO
//=================================================================================================
// Commented Out for Linux Broadcast Testing
//=================================================================================================
	if (! ConnectToDefaultDPP(szDPP_Send)) {
		ConsolePause();
		return 1;
	} else {
		ConsolePause();
	}
	if (!bRunUpToStatus) {
		CloseConnection();
		return 0;			// quit here if DEMO_FIND mode
	}

	// Get the DPP Status
	ConsoleClear();
	chdpp.DP5Stat.m_DP5_Status.SerialNumber = 0;
	GetDppStatus();
	ConsolePause();
	if (!bRunUpToAcquire) {
		CloseConnection();
		return 0;
	}

	// Set and Read the DPP configuration
	if (chdpp.DP5Stat.m_DP5_Status.SerialNumber == 0) { return 1; }
	////	ConsoleClear();
	////	SendConfigFileToDpp("PX5_Console_Test.txt");    // calls SendCommandString
	////	ConsolePause();
	ConsoleClear();
	ReadDppConfigurationFromHardware(false);
	ConsolePause();

	// Display and Set Presets
	ConsoleClear();
	DisplayPresets();
	ConsolePause();
	ConsoleClear();
	cout << "SendPresetAcquisitionTime" << endl;
	SendPresetAcquisitionTime("PRET=20;");
	SaveSpectrumConfig();
	ConsolePause();

	// Acquire and Save a Spectrum
	ConsoleClear();
	cout << "AcquireSpectrum" << endl;
	AcquireSpectrum();
	ConsolePause();
	SaveSpectrumFile();
	ConsolePause();
	if (!bRunAll) {
		CloseConnection();
		return 0;
	}

	// Display a configuration from file
	ConsoleClear();
	ReadConfigFile();
//  #endif
	ConsolePause();
//=================================================================================================
// Commented Out for Linux Broadcast Testing
//=================================================================================================

	// close the connection
	ConsoleClear();
	CloseConnection();
	ConsolePause();

	return 0;
}
