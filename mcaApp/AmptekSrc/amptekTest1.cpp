/** amptekTest1.cpp */

#include <iostream>
#include <string.h>
using namespace std; 
#include "ConsoleHelper.h"
#ifndef WIN32
	#include <unistd.h>
	#define _getch getchar
#endif

bool bHaveStatusResponse = false;
CConsoleHelper chdpp;					// DPP communications functions

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
				bHaveStatusResponse = true;
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
	if (bHaveStatusResponse) {
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
					if (bDisplayCfg) {
						cout << "\t\t\tConfiguration Length: " << (unsigned int)chdpp.HwCfgDP5.length() << endl;
						cout << "\t================================================================" << endl;
						cout << chdpp.HwCfgDP5 << endl;
					}
				}
			}
		}
	}
}

void ReadSpectrum()
{
  if (chdpp.DppSocket_SendCommand(XMTPT_SEND_SPECTRUM_STATUS)) {	// request spectrum+status
    if (chdpp.DppSocket_ReceiveData()) {
				cout << "Read spectrum OK, numChannels=" << chdpp.DP5Proto.SPECTRUM.CHANNELS << " data[1000]=" << chdpp.DP5Proto.SPECTRUM.DATA[1000] << endl;
    } else {
      cout << "\t\tProblem calling DppSocket_ReceiveData()." << endl;
    } 
  } else {
    cout << "\t\tProblem calling DppSocket_SendCommand(XMTPT_SEND_SPECTRUM_STATUS)." << endl;
  }
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

int main(int argc, char * argv[])
{
	// DemonstrationMode(DEMO_ACQUIRE);			// DEMO_ALL, DEMO_ACQUIRE, DEMO_STATUS, DEMO_FIND

	// Find the DPP Device
	char szDPP_Send[20];
	
	strcpy(szDPP_Send, argv[1]);

	if (! chdpp.DppSocket_Connect_Default_DPP(szDPP_Send)) { return 1;}
	
  // Get the DPP Status
	chdpp.DP5Stat.m_DP5_Status.SerialNumber = 0;
	GetDppStatus();

	// Set and Read the DPP configuration
	if (chdpp.DP5Stat.m_DP5_Status.SerialNumber == 0) { return 1; }
	ReadDppConfigurationFromHardware(true);

	cout << "Read Spectrum" << endl;
	ReadSpectrum();

	CloseConnection();

	return 0;
}
