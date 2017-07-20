/** CParsePacket CParsePacket */
#pragma once

#include <string>
#include "DP5Protocol.h"
using namespace std; 

typedef enum _CommType {
    commNone = 0,
    commRS232 = 1,
    commUSB = 2,
    commSockets = 3
} CommType;

#define preqProcessNone 0x0
#define preqProcessStatus 0x1
#define preqProcessSpectrum 0x2
#define preqRequestScopeData 0x4
#define preqProcessScopeData 0x8
#define preqProcessTextData 0x10
#define preqProcessScopeDataOverFlow 0x20
#define preqProcessInetSettings 0x40
#define preqProcessDiagData 0x80
#define preqProcessHwDesc 0x100
#define preqProcessCfgRead 0x200
#define preqProcessNetFindRead 0x400
#define preqProcessPaCal 0x800
//...
#define preqProcessSCAData 0x2000
#define preqProcessAck 0x4000
#define preqProcessError 0x8000

typedef struct _DppStateType {
    CommType Interface;
    string strPort;
    unsigned long ReqProcess;
    bool HaveScopeData;
    bool ScopeAutoRearm;
	bool ScopeOverFlow;
} DppStateType;

class CParsePacket
{
public:
	CParsePacket(void);
	~CParsePacket(void);
	/// DPP packet parser state indicator.
	DppStateType DppState;
	/// Parses DPP packet status, determines further processing.
	void ParsePacketStatus(unsigned char P[], Packet_In *PIN);
	/// Processes DPP return ACKS, NAKS and errors.
	string PID2_TextToString(string strPacketSource, unsigned char PID2);
	/// Parses incoming DPP packet, tests packet, populates packet information, sets packet state.
	long ParsePacket(unsigned char P[], Packet_In *PIN);
};

