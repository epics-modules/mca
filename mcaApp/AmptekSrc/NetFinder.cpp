#include "NetFinder.h"
#include <time.h>
#include <math.h>

CNetFinder::CNetFinder(void)
{
	InitAllEntries();
	m_rand = 0;
	LastEntry = NO_ENTRIES;
}

CNetFinder::~CNetFinder(void)
{

}

COLORREF CNetFinder::AlertEntryToCOLORREF(NETDISPLAY_ENTRY *pEntry)
{
	COLORREF DppConnectionColor;
	DppConnectionColor = AlertByteToCOLORREF(pEntry->alert_level);
	return (DppConnectionColor);
}

COLORREF CNetFinder::AlertByteToCOLORREF(BYTE alert_level)
{
	COLORREF DppConnectionColor;
	switch(alert_level){			// Alert Level
		case 0x00:	//0 = Interface is open (unconnected);
			DppConnectionColor = colorGreen;
			break;
		case 0x01:	//1 = Interface is connected (sharing is allowed);
			DppConnectionColor = colorYellow;
			break;
		case 0x02:	//2 = Interface is connected (sharing is not allowed);
			DppConnectionColor = colorRed;
			break;
		case 0x03:	//3 = Interface is locked
			DppConnectionColor = colorRed;
			break;
		case 0x04:	//4 = Interface is unavailable because USB is connected
			DppConnectionColor = colorSilver;
			break;
		default:	//Interface configuration unknown
			DppConnectionColor = colorWhite;
			break;
	}
	return (DppConnectionColor);
}

CString CNetFinder::EntryToStr(NETDISPLAY_ENTRY *pEntry)
{	
	CString cstrAlertLevel;
	CString cstrDeviceType;
	CString cstrIpAddress;
	CString cstrEvent1, cstrEvent2;
	CString cstrAdditionalDesc;
	CString cstrMacAddress;
	CString cstrEntry;
	CString temp_str, temp_str2;	

	cstrAlertLevel = "Connection: ";
	switch(pEntry->alert_level){			// Alert Level
		case 0x00:	//0 = Interface is open (unconnected);
			cstrAlertLevel += "Interface is open (unconnected)";
			break;
		case 0x01:	//1 = Interface is connected (sharing is allowed);
			cstrAlertLevel += "Interface is connected (sharing is allowed)";
			break;
		case 0x02:	//2 = Interface is connected (sharing is not allowed);
			cstrAlertLevel += "Interface is connected (sharing is not allowed)";
			break;
		case 0x03:	//3 = Interface is locked
			cstrAlertLevel += "Interface is locked";
			break;
		case 0x04:	//4 = Interface is unavailable because USB is connected
			cstrAlertLevel += "Interface is unavailable because USB is connected";
			break;
		default:
			cstrAlertLevel += "Interface configuration unknown";
			break;
	}
	cstrDeviceType = pEntry->str_type;		// Device Name/Type
	// IP Address String
	cstrIpAddress.Format("IP Address: %u.%u.%u.%u", (unsigned short) pEntry->ip[0], (unsigned short) pEntry->ip[1],(unsigned short) pEntry->ip[2],(unsigned short) pEntry->ip[3]); 
	cstrAdditionalDesc = pEntry->str_description;	// Additional Description
	// MacAddress
	cstrMacAddress.Format("MAC Address: %02x-%02x-%02x-%02x-%02x-%02x", (unsigned short) pEntry->mac[0], (unsigned short) pEntry->mac[1],(unsigned short) pEntry->mac[2],(unsigned short) pEntry->mac[3],(unsigned short) pEntry->mac[4],(unsigned short) pEntry->mac[5]); 
	// Event1 Time
	temp_str = pEntry->str_ev1 + " ";
	if(pEntry->event1_days) temp_str2.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event1_days,pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_hours) temp_str2.Format("%i hours, %i minutes, %i seconds", pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_minutes) temp_str2.Format("%i minutes, %i seconds", pEntry->event1_minutes, pEntry->event1_seconds );
	else temp_str2.Format("%i seconds", pEntry->event1_seconds );
	cstrEvent1 = temp_str + temp_str2;
	// Event2 Time
	temp_str = pEntry->str_ev2 + " ";
	if(pEntry->event2_days) temp_str2.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event2_days,pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_hours) temp_str2.Format("%i hours, %i minutes, %i seconds", pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_minutes) temp_str2.Format("%i minutes, %i seconds", pEntry->event2_minutes, pEntry->event2_seconds );
	else temp_str2.Format("%i seconds", pEntry->event2_seconds );
	cstrEvent2 = temp_str + temp_str2;
	
	cstrEntry = cstrDeviceType + "\r\n";
	cstrEntry += cstrAlertLevel + "\r\n";
	cstrEntry += cstrIpAddress + "\r\n";
	cstrEntry += cstrAdditionalDesc + "\r\n";
	cstrEntry += cstrMacAddress + "\r\n";
	cstrEntry += cstrEvent1 + "\r\n";
	cstrEntry += cstrEvent2 + "\r\n";
	return cstrEntry;
}

CString CNetFinder::EntryToStrRS232(NETDISPLAY_ENTRY *pEntry, CString cstrPort)
{	
	CString cstrAlertLevel;
	CString cstrDeviceType;
	CString cstrIpAddress;
	CString cstrEvent1, cstrEvent2;
	CString cstrAdditionalDesc;
	CString cstrMacAddress;
	CString cstrEntry;
	CString temp_str, temp_str2;	

	cstrAlertLevel = "Connection: ";
	cstrAlertLevel += cstrPort;
	//switch(pEntry->alert_level){			// Alert Level
	//	case 0x00:	//0 = Interface is open (unconnected);
	//		cstrAlertLevel += "Interface is open (unconnected)";
	//		break;
	//	case 0x01:	//1 = Interface is connected (sharing is allowed);
	//		cstrAlertLevel += "Interface is connected (sharing is allowed)";
	//		break;
	//	case 0x02:	//2 = Interface is connected (sharing is not allowed);
	//		cstrAlertLevel += "Interface is connected (sharing is not allowed)";
	//		break;
	//	case 0x03:	//3 = Interface is locked
	//		cstrAlertLevel += "Interface is locked";
	//		break;
	//	case 0x04:	//4 = Interface is unavailable because USB is connected
	//		cstrAlertLevel += "Interface is unavailable because USB is connected";
	//		break;
	//	default:
	//		cstrAlertLevel += "Interface configuration unknown";
	//		break;
	//}
	cstrDeviceType = pEntry->str_type;		// Device Name/Type
	// IP Address String
	cstrIpAddress.Format("IP Address: %u.%u.%u.%u", (unsigned short) pEntry->ip[0], (unsigned short) pEntry->ip[1],(unsigned short) pEntry->ip[2],(unsigned short) pEntry->ip[3]); 
	cstrAdditionalDesc = pEntry->str_description;	// Additional Description
	// MacAddress
	cstrMacAddress.Format("MAC Address: %02x-%02x-%02x-%02x-%02x-%02x", (unsigned short) pEntry->mac[0], (unsigned short) pEntry->mac[1],(unsigned short) pEntry->mac[2],(unsigned short) pEntry->mac[3],(unsigned short) pEntry->mac[4],(unsigned short) pEntry->mac[5]); 
	// Event1 Time
	temp_str = pEntry->str_ev1 + " ";
	if(pEntry->event1_days) temp_str2.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event1_days,pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_hours) temp_str2.Format("%i hours, %i minutes, %i seconds", pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_minutes) temp_str2.Format("%i minutes, %i seconds", pEntry->event1_minutes, pEntry->event1_seconds );
	else temp_str2.Format("%i seconds", pEntry->event1_seconds );
	cstrEvent1 = temp_str + temp_str2;
	// Event2 Time
	temp_str = pEntry->str_ev2 + " ";
	if(pEntry->event2_days) temp_str2.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event2_days,pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_hours) temp_str2.Format("%i hours, %i minutes, %i seconds", pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_minutes) temp_str2.Format("%i minutes, %i seconds", pEntry->event2_minutes, pEntry->event2_seconds );
	else temp_str2.Format("%i seconds", pEntry->event2_seconds );
	cstrEvent2 = temp_str + temp_str2;
	
	cstrEntry = cstrDeviceType + "\r\n";
	cstrEntry += cstrAlertLevel + "\r\n";
	//cstrEntry += cstrIpAddress + "\r\n";
	cstrEntry += cstrAdditionalDesc + "\r\n";
	cstrEntry += cstrMacAddress + "\r\n";
	cstrEntry += cstrEvent1 + "\r\n";
	//cstrEntry += cstrEvent2 + "\r\n";
	return cstrEntry;
}

CString CNetFinder::EntryToStrUSB(NETDISPLAY_ENTRY *pEntry, CString cstrPort)
{	
	CString cstrAlertLevel;
	CString cstrDeviceType;
	CString cstrIpAddress;
	CString cstrEvent1, cstrEvent2;
	CString cstrAdditionalDesc;
	CString cstrMacAddress;
	CString cstrEntry;
	CString temp_str, temp_str2;	

	cstrAlertLevel = "Connection: ";
	cstrAlertLevel += cstrPort;
	//switch(pEntry->alert_level){			// Alert Level
	//	case 0x00:	//0 = Interface is open (unconnected);
	//		cstrAlertLevel += "Interface is open (unconnected)";
	//		break;
	//	case 0x01:	//1 = Interface is connected (sharing is allowed);
	//		cstrAlertLevel += "Interface is connected (sharing is allowed)";
	//		break;
	//	case 0x02:	//2 = Interface is connected (sharing is not allowed);
	//		cstrAlertLevel += "Interface is connected (sharing is not allowed)";
	//		break;
	//	case 0x03:	//3 = Interface is locked
	//		cstrAlertLevel += "Interface is locked";
	//		break;
	//	case 0x04:	//4 = Interface is unavailable because USB is connected
	//		cstrAlertLevel += "Interface is unavailable because USB is connected";
	//		break;
	//	default:
	//		cstrAlertLevel += "Interface configuration unknown";
	//		break;
	//}
	cstrDeviceType = pEntry->str_type;		// Device Name/Type
	// IP Address String
	cstrIpAddress.Format("IP Address: %u.%u.%u.%u", (unsigned short) pEntry->ip[0], (unsigned short) pEntry->ip[1],(unsigned short) pEntry->ip[2],(unsigned short) pEntry->ip[3]); 
	cstrAdditionalDesc = pEntry->str_description;	// Additional Description
	// MacAddress
	cstrMacAddress.Format("MAC Address: %02x-%02x-%02x-%02x-%02x-%02x", (unsigned short) pEntry->mac[0], (unsigned short) pEntry->mac[1],(unsigned short) pEntry->mac[2],(unsigned short) pEntry->mac[3],(unsigned short) pEntry->mac[4],(unsigned short) pEntry->mac[5]); 
	// Event1 Time
	temp_str = pEntry->str_ev1 + " ";
	if(pEntry->event1_days) temp_str2.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event1_days,pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_hours) temp_str2.Format("%i hours, %i minutes, %i seconds", pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_minutes) temp_str2.Format("%i minutes, %i seconds", pEntry->event1_minutes, pEntry->event1_seconds );
	else temp_str2.Format("%i seconds", pEntry->event1_seconds );
	cstrEvent1 = temp_str + temp_str2;
	// Event2 Time
	temp_str = pEntry->str_ev2 + " ";
	if(pEntry->event2_days) temp_str2.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event2_days,pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_hours) temp_str2.Format("%i hours, %i minutes, %i seconds", pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_minutes) temp_str2.Format("%i minutes, %i seconds", pEntry->event2_minutes, pEntry->event2_seconds );
	else temp_str2.Format("%i seconds", pEntry->event2_seconds );
	cstrEvent2 = temp_str + temp_str2;
	
	cstrEntry = cstrDeviceType + "\r\n";
	cstrEntry += cstrAlertLevel + "\r\n";
	//cstrEntry += cstrIpAddress + "\r\n";
	cstrEntry += cstrAdditionalDesc + "\r\n";
	cstrEntry += cstrMacAddress + "\r\n";
	cstrEntry += cstrEvent1 + "\r\n";
	//cstrEntry += cstrEvent2 + "\r\n";
	return cstrEntry;
}

int CNetFinder::FindEntry(ULONG SockAddr)
{
	int idxEntry;
	int EntryFound;
	EntryFound = NO_ENTRIES;
	for (idxEntry=0;idxEntry<=LastEntry;idxEntry++){
		if (SockAddr == DppEntries[idxEntry].SockAddr) {
			EntryFound = idxEntry;
			break;
		}
	}
	return (EntryFound);
}

void CNetFinder::InitEntry(NETDISPLAY_ENTRY *pEntry)
{
	int idxInit;
	pEntry->alert_level = 0;
	
	pEntry->event1_days = 0;
	pEntry->event1_hours = 0;
	pEntry->event1_minutes = 0;
	
	pEntry->event2_days = 0;
	pEntry->event2_hours = 0;
	pEntry->event2_minutes = 0;

	pEntry->event1_seconds = 0;
	pEntry->event2_seconds = 0;

	pEntry->port = 0; // Get port from UDP header

	for (idxInit=0;idxInit<4;idxInit++){
		pEntry->mac[idxInit] = 0;
		pEntry->ip[idxInit] = 0;
		pEntry->subnet[idxInit] = 0;
		pEntry->gateway[idxInit] = 0;
	}

	pEntry->mac[4] = 0;
	pEntry->mac[5] = 0;

	pEntry->str_type = "";
	pEntry->str_description = "";
	pEntry->str_ev1 = "";
	pEntry->str_ev2 = "";

	pEntry->str_display = "";
	pEntry->ccConnectRGB = colorWhite;

	pEntry->HasData = FALSE;

	pEntry->SockAddr = 0;

}

void CNetFinder::InitAllEntries()
{
	for (int idxEntry=0;idxEntry<MAX_ENTRIES;idxEntry++){
		InitEntry(&DppEntries[idxEntry]);
	}
	LastEntry = NO_ENTRIES;
	m_rand = 0;
}

void CNetFinder::AddEntry(NETDISPLAY_ENTRY *pEntry, BYTE *buffer, UINT destPort)
{
	pEntry->alert_level = buffer[1];
	pEntry->port = destPort;

	int i = 4; // Start buffer index at 4

	pEntry->event1_days = ( (int) buffer[i++]<<8 );
	pEntry->event1_days |= buffer[i++];

	pEntry->event1_hours = buffer[i++];
	pEntry->event1_minutes = buffer[i++];

	pEntry->event2_days = ( (int) buffer[i++]<<8 );	
	pEntry->event2_days |= buffer[i++];

	pEntry->event2_hours = buffer[i++];
	pEntry->event2_minutes = buffer[i++];

	pEntry->event1_seconds = buffer[i++];
	pEntry->event2_seconds = buffer[i++];

	pEntry->mac[0] = buffer[i++];
	pEntry->mac[1] = buffer[i++];
	pEntry->mac[2] = buffer[i++];
	pEntry->mac[3] = buffer[i++];
	pEntry->mac[4] = buffer[i++];
	pEntry->mac[5] = buffer[i++];

	pEntry->ip[0] = buffer[i++];
	pEntry->ip[1] = buffer[i++];
	pEntry->ip[2] = buffer[i++];
	pEntry->ip[3] = buffer[i++];

	pEntry->subnet[0] = buffer[i++];
	pEntry->subnet[1] = buffer[i++];
	pEntry->subnet[2] = buffer[i++];
	pEntry->subnet[3] = buffer[i++];

	pEntry->gateway[0] = buffer[i++];
	pEntry->gateway[1] = buffer[i++];
	pEntry->gateway[2] = buffer[i++];
	pEntry->gateway[3] = buffer[i++];

	pEntry->str_type = &buffer[i];
	i += (pEntry->str_type.GetLength()+1);

	pEntry->str_description = &buffer[i];
	i += (pEntry->str_description.GetLength()+1);

	// Copy the Event1+2 text descripion without the ": "
	pEntry->str_ev1 = &buffer[i];
	i += (pEntry->str_ev1.GetLength()+1);

	pEntry->str_ev2 = &buffer[i];
	i += (pEntry->str_ev2.GetLength()+1);

	// Add ": " to str_ev1 and str_ev2
	pEntry->str_ev1 += ": ";
	pEntry->str_ev2 += ": ";

	pEntry->ccConnectRGB = AlertByteToCOLORREF(pEntry->alert_level);
	pEntry->HasData = TRUE;
	pEntry->SockAddr = SockAddr_ByteToULong(&buffer[20]);
	pEntry->str_display = EntryToStr(pEntry);	// Convert Entry infor to string
}

CString CNetFinder::SockAddr_DlgToStr(CDialog *dlg, int id1, int id2, int id3, int id4)
{
	char szAddr[10];
	CString cstrAddr;
	CString cstrID;
	
	memset(&szAddr, 0, sizeof(szAddr));
	::GetDlgItemText(dlg->GetSafeHwnd(), id1, szAddr, sizeof(szAddr));
	cstrID.Format("%s",szAddr);
	cstrAddr = cstrID.Trim();
	memset(&szAddr, 0, sizeof(szAddr));
	::GetDlgItemText(dlg->GetSafeHwnd(), id2, szAddr, sizeof(szAddr));
	cstrID.Format(".%s",szAddr);
	cstrAddr += cstrID.Trim();
	memset(&szAddr, 0, sizeof(szAddr));
	::GetDlgItemText(dlg->GetSafeHwnd(), id3, szAddr, sizeof(szAddr));
	cstrID.Format(".%s",szAddr);
	cstrAddr += cstrID.Trim();
	memset(&szAddr, 0, sizeof(szAddr));
	::GetDlgItemText(dlg->GetSafeHwnd(), id4, szAddr, sizeof(szAddr));
	cstrID.Format(".%s",szAddr);
	cstrAddr += cstrID.Trim();
	return(cstrAddr);

}


UINT CNetFinder::SockPort_DlgToUINT(CDialog *dlg, int id1)
{
	char szPort[10];
	UINT Port;
	CString cstrPort;
	memset(&szPort, 0, sizeof(szPort));
	::GetDlgItemText(dlg->GetSafeHwnd(), id1, szPort, sizeof(szPort));
	Port = atoi(szPort);
	return(Port);
}

in_addr CNetFinder::SockAddr_ByteToInAddr(BYTE *ip)
{
	in_addr inSockAddr;
	inSockAddr.S_un.S_un_b.s_b1 = ip[0];
	inSockAddr.S_un.S_un_b.s_b2 = ip[1];
	inSockAddr.S_un.S_un_b.s_b3 = ip[2];
	inSockAddr.S_un.S_un_b.s_b4 = ip[3];
	return (inSockAddr);
}

ULONG CNetFinder::SockAddr_ByteToULong(BYTE *ip)
{
	in_addr inSockAddr;
	inSockAddr = SockAddr_ByteToInAddr(ip);
	return (inSockAddr.S_un.S_addr);
}

CString CNetFinder::SockAddr_ByteToStr(BYTE *ip)
{
	CString cstrSockAddr;
	cstrSockAddr.Format("%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
	return (cstrSockAddr);
}

in_addr CNetFinder::SockAddr_StrToInAddr(CString cstrSockAddr)
{
	ULONG SockAddr;
	in_addr inSockAddr;
	SockAddr = inet_addr(cstrSockAddr);
	inSockAddr.S_un.S_addr = SockAddr;
	return (inSockAddr);
}

ULONG CNetFinder::SockAddr_StrToULong(CString cstrSockAddr)
{
	ULONG SockAddr;
	SockAddr = inet_addr(cstrSockAddr);
	return (SockAddr);
}

in_addr CNetFinder::SockAddr_ULongToInAddr(ULONG SockAddr)
{
	in_addr inSockAddr;
	inSockAddr.S_un.S_addr = SockAddr;
	return (inSockAddr);
}

CString CNetFinder::SockAddr_ULongToStr(ULONG SockAddr)
{
	CString cstrSockAddr;
	in_addr inSockAddr;
	inSockAddr.S_un.S_addr = SockAddr;
	cstrSockAddr.Format("%s",_T(inet_ntoa(inSockAddr)));
	return (cstrSockAddr);
}

CString CNetFinder::SockAddr_InAddrToStr(in_addr inSockAddr)
{
	CString cstrSockAddr;
	cstrSockAddr.Format("%s",_T(inet_ntoa(inSockAddr)));
	return (cstrSockAddr);
}




























