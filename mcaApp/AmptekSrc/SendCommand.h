#pragma once
#include "DppConst.h"
#include "DP5Protocol.h"
#include "AsciiCmdUtilities.h"

typedef struct _CONFIG_OPTIONS
{
    bool PC5_PRESENT;				/// if true pc5 commands are supported
    int DppType;					/// device type indicator
	string HwCfgDP5Out;				/// custom configuration string
	bool SendCoarseFineGain;		/// false=Send GAIN, true=Send GAIA,GAIF
	bool isDP5_RevDxGains;			/// use dp5 dx gains
	unsigned char DPP_ECO;			/// holds eco indicator
} CONFIG_OPTIONS;

/** CSendCommand prepares all command packets to be sent.
	Call CSendCommand::DP5_CMD, CSendCommand::DP5_CMD_Config or CSendCommand::DP5_CMD_Data 
	along with the ::TRANSMIT_PACKET_TYPE and any additional data or options.  
	
	A command array of 8-bit bytes is generated.
	
*/
class CSendCommand
{
public:
	CSendCommand(void);
	~CSendCommand(void);
    CAsciiCmdUtilities AsciiCmdUtil;

	/// Packet checksum test for commands with data. 
	bool TestPacketCkSumOK(unsigned char Data[]);
	/// Creates a DPP command that does not require additional processing.
	bool DP5_CMD(unsigned char Buffer[], TRANSMIT_PACKET_TYPE XmtCmd);
	/// Creates a DPP command that requires configuration data options processing.
	bool DP5_CMD_Config(unsigned char Buffer[], TRANSMIT_PACKET_TYPE XmtCmd, CONFIG_OPTIONS CfgOptions);
	/// Creates a DPP command that requires data.
	bool DP5_CMD_Data(unsigned char Buffer[], TRANSMIT_PACKET_TYPE XmtCmd, unsigned char DataOut[]);
	/// Creates a packet output buffer from a command byte data array.
	bool POUT_Buffer(Packet_Out POUT, unsigned char Buffer[]);
};

