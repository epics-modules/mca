#pragma once
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string>
#include <cctype> // std::toupper, std::tolower
using namespace std; 
#include "stringex.h"
#include "DppImportExport.h"
#include "DppConst.h"

#ifndef LINE_MAX
	#define LINE_MAX 256
#endif
#define DP4_PX4_OLD_CFG_SIZE 64
#define DP5_MAX_CFG_SIZE 512		/// 512 + 8 Bytes (2 SYNC,2 PID,2 LEN,2 CHKSUM)
#define Whitespace "\t\n\v\f\r\0x20"	/// $ = Chr$(0) + Chr$(9) + Chr$(10) + Chr$(11) + Chr$(12) + Chr$(13) + Chr$(32)

class IMPORT_EXPORT CAsciiCmdUtilities
{
public:
	CAsciiCmdUtilities(void);
	~CAsciiCmdUtilities(void);
	/// Forces all characters to uppercase. (All DPP command must be in upper case.)
	std::string MakeUpper(std::string myString);
	/// Removes Whitespace characters from a command string.
	std::string RemWhitespace(std::string strLine);
	/// Reads a DPP configuration from a file.
	std::string GetDP5CfgStr(std::string strFilename);
	std::string CreateResTestReadBackCmd(bool bSendCoarseFineGain, int DppType);
	/// Generates a configuration readback command from a list of all commands.
	std::string CreateFullReadBackCmd(bool PC5_PRESENT, int DppType, bool isDP5_RevDxGains, unsigned char DPP_ECO);
	/// Generates a configuration readback command from a list of all commands.
	std::string CreateFullReadBackCmdMCA8000D(int DppType);
	/// Remove a specified command from the command stream.
	std::string RemoveCmd(std::string strCmd, std::string strCfgData);
	/// Remove illegal commands from the command stream by device type.
	std::string RemoveCmdByDeviceType(std::string strCfgDataIn, bool PC5_PRESENT, int DppType, bool isDP5_RevDxGains, unsigned char DPP_ECO);
	/// Remove illegal commands from the command stream by device type.
	std::string RemoveCmdByDeviceTypeDP5DxK(std::string strCfgDataIn, bool PC5_PRESENT, int DppType);
	////removes MCA8000D commands
	std::string Remove_MCA8000D_Cmds(std::string strCfgDataIn, int DppType);
	// replaces all occurrences of substring in string
	std::string ReplaceCmdText(std::string strInTextIn, std::string strFrom, std::string strTo);
	// breaks ASCII Command string into two chuncks, returns split position
	int GetCmdChunk(std::string strCmd);
	/// Force string to ASCII bytes.
	bool CopyAsciiData(unsigned char Data[], string strCfg, long lLen);
};
