#include "ParsePacket.h"

CParsePacket::CParsePacket(void)
{
}

CParsePacket::~CParsePacket(void)
{
}

void CParsePacket::ParsePacketStatus(unsigned char P[], Packet_In *PIN)
{
    short X;
    long CSum;

    CSum = 0;
    if (P[0] == SYNC1_) {
        if (P[1] == SYNC2_) {
            if (P[4] < 128) {  // LEN MSB < 128, i.e. LEN < 32768
                PIN->LEN = (P[4] * 256) + P[5];
                PIN->PID1 = P[2];
                PIN->PID2 = P[3];
				for (X=0;X <=(PIN->LEN+5);X++) {	// add up all the bytes except checksum
                    CSum = CSum + P[X];
                }
                CSum = CSum + 256 * (long)(P[PIN->LEN + 6]) + (long)(P[PIN->LEN + 7]);
                PIN->CheckSum = CSum;
                if ((CSum & 0xFFFF) == 0) {
                    PIN->STATUS = 0;      // packet is OK
                    if (PIN->LEN > 0) {
						for (X=6;X <=(PIN->LEN+5);X++) {	
                            PIN->DATA[X - 6] = P[X];
						}
                    }
                } else {
                    PIN->STATUS = PID2_ACK_CHECKSUM_ERROR;    // checksum error
                }
            } else {
                PIN->STATUS = PID2_ACK_LEN_ERROR;        // length error
            }
        } else {
            PIN->STATUS = PID2_ACK_SYNC_ERROR;            // sync error
        }
    } else {
        PIN->STATUS = PID2_ACK_SYNC_ERROR ;               // sync error
    }
}

string CParsePacket::PID2_TextToString(string strPacketSource, unsigned char PID2)
{
	string strPID2;
	strPID2 = "";
	switch (PID2) {
		case PID2_ACK_OK:    // ACK OK
            //PID2_TextToString = strPacketSource + ": OK\t";
            //strPID2 = "ACK_OK";
			strPID2 = "";
            //ACK_Received = True
 			break;
        case PID2_ACK_SYNC_ERROR:
            strPID2 = strPacketSource + ": Sync Error\t";
			break;
        case PID2_ACK_PID_ERROR:
            strPID2 = strPacketSource + ": PID Error\t";
			break;
        case PID2_ACK_LEN_ERROR:
            strPID2 = strPacketSource + ": Length Error\t";
			break;
        case PID2_ACK_CHECKSUM_ERROR:
            strPID2 = strPacketSource + ": Checksum Error\t";
			break;
        case PID2_ACK_BAD_PARAM:	// now msg includes bad parameter
			// put the error extractor in a new function
    		//[] If PIN.LEN > 0 Then ' BAD CMD (PID2=7), BAD PARAM (PID2=5), NO PC5 (PID2=11) errors return offending command string
      //      	[] T$ = ""
      //      	[] For X = 0 To PIN.LEN - 1
      //      		[] T$ = T$ + Chr(PIN.DATA(X))
      //      	[] Next X
      //      	[] End If
            strPID2 = strPacketSource + ": Bad Parameter\t";
			break;
        case PID2_ACK_BAD_HEX_REC:
            strPID2 = strPacketSource + ": Bad HEX Record\t";
			break;
        case PID2_ACK_FPGA_ERROR:
            strPID2 = strPacketSource + ": FPGA not initialized\t";
			break;
        case PID2_ACK_CP2201_NOT_FOUND:
            strPID2 = strPacketSource + ": CP2201 not found\t";
			break;
        case PID2_ACK_SCOPE_DATA_NOT_AVAIL:
            strPID2 = strPacketSource + ": No scope data\t";
			break;
        case PID2_ACK_PC5_NOT_PRESENT:
            strPID2 = strPacketSource + ": PC5 not present\t";
			break;
        case PID2_ACK_OK_ETHERNET_SHARE_REQ:
            strPID2 = strPacketSource + ": Ethernet sharing request\t";
			break;
        case PID2_ACK_ETHERNET_BUSY:
            strPID2 = strPacketSource + ": Ethernet sharing request\t";
			break;
    		//[] Case :TextLog "ACK: PC5 NOT DETECTED '" + T$ + "'" + vbCrLf
        case PID2_ACK_CAL_DATA_NOT_PRESENT:
            strPID2 = strPacketSource + ": Calibration data not present\t";
			break;
        case PID2_ACK_UNRECOG:
            strPID2 = strPacketSource + ": Unrecognized Command\t";
			break;
		default:
			strPID2 = strPacketSource + ": Unrecognized Error\t";
			break;
	}
	return strPID2;
}

long CParsePacket::ParsePacket(unsigned char P[], Packet_In *PIN)
{
	long ParsePkt;
    ParsePkt = preqProcessNone;
    ParsePacketStatus (P, PIN);
    if (PIN->STATUS == PID2_ACK_OK) { // no errors
        if ((PIN->PID1 == PID1_RCV_STATUS) && (PIN->PID2 == PID2_SEND_DP4_STYLE_STATUS)) { // DP4-style status
            ParsePkt = preqProcessStatus;
        } else if ((PIN->PID1 == PID1_RCV_SPECTRUM) && ((PIN->PID2 >= RCVPT_256_CHANNEL_SPECTRUM) && (PIN->PID2 <= RCVPT_8192_CHANNEL_SPECTRUM_STATUS))) { // spectrum / spectrum+status
            ParsePkt = preqProcessSpectrum;
        //} else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == PID2_SEND_SCOPE_DATA)) { //scope data packet
        //    ParsePkt = preqProcessScopeData;
        //} else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == PID2_SEND_512_BYTE_MISC_DATA)) { //text data packet
        //    ParsePkt = preqProcessTextData;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_SCOPE_DATA)) { //scope data packet
            //ScopeOverFlow = false;
            ParsePkt = preqProcessScopeData;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_512_BYTE_MISC_DATA)) { //text data packet
            ParsePkt = preqProcessTextData;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_SCOPE_DATA_WITH_OVERFLOW)) { //scope data with overflow packet
            //ScopeOverFlow = true;
            //ParsePkt = preqProcessScopeData;
            ParsePkt = preqProcessScopeDataOverFlow;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_ETHERNET_SETTINGS)) { //ethernet settings packet
            //ParsePkt = preqProcessInetSettings;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_DIAGNOSTIC_DATA)) { //diagnostic data  packet
            ParsePkt = preqProcessDiagData;
        //} else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_HARDWARE_DESCRIPTION)) { //hardware description packet
            //ParsePkt = preqProcessHwDesc;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_CONFIG_READBACK)) {
            ParsePkt = preqProcessCfgRead;
        //} else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_NETFINDER_READBACK)) {
            //ParsePkt = preqProcessNetFindRead;
        } else if ((PIN->PID1 == PID1_RCV_SCOPE_MISC) && (PIN->PID2 == RCVPT_OPTION_PA_CALIBRATION)) {
            ParsePkt = preqProcessPaCal;
        } else if (PIN->PID1 == PID1_ACK) {
            ParsePkt = preqProcessAck;
        } else {
            PIN->STATUS = PID2_ACK_PID_ERROR;  // unknown PID
            ParsePkt = preqProcessError;
        }
    } else {
        ParsePkt = preqProcessError;
    }

    //PIN->PID1 = 0;    // overwrite the PIDs so packet can't be erroneously processed again
    //PIN->PID2 = 0;

	return ParsePkt;
}

//ElseIf PIN.PID2 = 7 Then ' Config readback response packet
//    txtConfig.Text = ""
//    For X = 0 To PIN.LEN - 1
//        txtConfig.Text = txtConfig.Text + Chr(PIN.DATA(X))
//        If Chr(PIN.DATA(X)) = ";" Then
//            txtConfig.Text = txtConfig.Text + vbCrLf
//        End If
//    Next X








