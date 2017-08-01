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

string CNetFinder::EntryToStr(NETDISPLAY_ENTRY *pEntry)
{	
	string strAlertLevel;
	string strDeviceType;
	string strIpAddress;
	string strEvent1, strEvent2;
	string strAdditionalDesc;
	string strMacAddress;
	string strEntry;
	string temp_str, temp_str2;	
	stringex strfn;

	strAlertLevel = "Connection: ";
	switch(pEntry->alert_level){			// Alert Level
		case 0x00:	//0 = Interface is open (unconnected);
			strAlertLevel += "Interface is open (unconnected)";
			break;
		case 0x01:	//1 = Interface is connected (sharing is allowed);
			strAlertLevel += "Interface is connected (sharing is allowed)";
			break;
		case 0x02:	//2 = Interface is connected (sharing is not allowed);
			strAlertLevel += "Interface is connected (sharing is not allowed)";
			break;
		case 0x03:	//3 = Interface is locked
			strAlertLevel += "Interface is locked";
			break;
		case 0x04:	//4 = Interface is unavailable because USB is connected
			strAlertLevel += "Interface is unavailable because USB is connected";
			break;
		default:
			strAlertLevel += "Interface configuration unknown";
			break;
	}
	strDeviceType = pEntry->str_type;		// Device Name/Type
	// IP Address String
	strIpAddress = strfn.Format("IP Address: %u.%u.%u.%u", (unsigned short) pEntry->ip[0], (unsigned short) pEntry->ip[1],(unsigned short) pEntry->ip[2],(unsigned short) pEntry->ip[3]); 
	strAdditionalDesc = pEntry->str_description;	// Additional Description
	// MacAddress
	strMacAddress = strfn.Format("MAC Address: %02x-%02x-%02x-%02x-%02x-%02x", (unsigned short) pEntry->mac[0], (unsigned short) pEntry->mac[1],(unsigned short) pEntry->mac[2],(unsigned short) pEntry->mac[3],(unsigned short) pEntry->mac[4],(unsigned short) pEntry->mac[5]); 
	// Event1 Time
	temp_str = pEntry->str_ev1 + " ";
	if(pEntry->event1_days) temp_str2 = strfn.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event1_days,pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_hours) temp_str2 = strfn.Format("%i hours, %i minutes, %i seconds", pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_minutes) temp_str2 = strfn.Format("%i minutes, %i seconds", pEntry->event1_minutes, pEntry->event1_seconds );
	else temp_str2 = strfn.Format("%i seconds", pEntry->event1_seconds );
	strEvent1 = temp_str + temp_str2;
	// Event2 Time
	temp_str = pEntry->str_ev2 + " ";
	if(pEntry->event2_days) temp_str2 = strfn.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event2_days,pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_hours) temp_str2 = strfn.Format("%i hours, %i minutes, %i seconds", pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_minutes) temp_str2 = strfn.Format("%i minutes, %i seconds", pEntry->event2_minutes, pEntry->event2_seconds );
	else temp_str2 = strfn.Format("%i seconds", pEntry->event2_seconds );
	strEvent2 = temp_str + temp_str2;
	
	strEntry = strDeviceType + "\r\n";
	strEntry += strAlertLevel + "\r\n";
	strEntry += strIpAddress + "\r\n";
	strEntry += strAdditionalDesc + "\r\n";
	strEntry += strMacAddress + "\r\n";
	strEntry += strEvent1 + "\r\n";
	strEntry += strEvent2 + "\r\n";
	return strEntry;
}

string CNetFinder::EntryToStrRS232(NETDISPLAY_ENTRY *pEntry, string strPort)
{	
	string strAlertLevel;
	string strDeviceType;
	string strIpAddress;
	string strEvent1, strEvent2;
	string strAdditionalDesc;
	string strMacAddress;
	string strEntry;
	string temp_str, temp_str2;	
	stringex strfn;

	strAlertLevel = "Connection: ";
	strAlertLevel += strPort;
	//switch(pEntry->alert_level){			// Alert Level
	//	case 0x00:	//0 = Interface is open (unconnected);
	//		strAlertLevel += "Interface is open (unconnected)";
	//		break;
	//	case 0x01:	//1 = Interface is connected (sharing is allowed);
	//		strAlertLevel += "Interface is connected (sharing is allowed)";
	//		break;
	//	case 0x02:	//2 = Interface is connected (sharing is not allowed);
	//		strAlertLevel += "Interface is connected (sharing is not allowed)";
	//		break;
	//	case 0x03:	//3 = Interface is locked
	//		strAlertLevel += "Interface is locked";
	//		break;
	//	case 0x04:	//4 = Interface is unavailable because USB is connected
	//		strAlertLevel += "Interface is unavailable because USB is connected";
	//		break;
	//	default:
	//		strAlertLevel += "Interface configuration unknown";
	//		break;
	//}
	strDeviceType = pEntry->str_type;		// Device Name/Type
	// IP Address String
	strIpAddress = strfn.Format("IP Address: %u.%u.%u.%u", (unsigned short) pEntry->ip[0], (unsigned short) pEntry->ip[1],(unsigned short) pEntry->ip[2],(unsigned short) pEntry->ip[3]); 
	strAdditionalDesc = pEntry->str_description;	// Additional Description
	// MacAddress
	strMacAddress = strfn.Format("MAC Address: %02x-%02x-%02x-%02x-%02x-%02x", (unsigned short) pEntry->mac[0], (unsigned short) pEntry->mac[1],(unsigned short) pEntry->mac[2],(unsigned short) pEntry->mac[3],(unsigned short) pEntry->mac[4],(unsigned short) pEntry->mac[5]); 
	// Event1 Time
	temp_str = pEntry->str_ev1 + " ";
	if(pEntry->event1_days) temp_str2 = strfn.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event1_days,pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_hours) temp_str2 = strfn.Format("%i hours, %i minutes, %i seconds", pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_minutes) temp_str2 = strfn.Format("%i minutes, %i seconds", pEntry->event1_minutes, pEntry->event1_seconds );
	else temp_str2 = strfn.Format("%i seconds", pEntry->event1_seconds );
	strEvent1 = temp_str + temp_str2;
	// Event2 Time
	temp_str = pEntry->str_ev2 + " ";
	if(pEntry->event2_days) temp_str2 = strfn.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event2_days,pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_hours) temp_str2 = strfn.Format("%i hours, %i minutes, %i seconds", pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_minutes) temp_str2 = strfn.Format("%i minutes, %i seconds", pEntry->event2_minutes, pEntry->event2_seconds );
	else temp_str2 = strfn.Format("%i seconds", pEntry->event2_seconds );
	strEvent2 = temp_str + temp_str2;
	
	strEntry = strDeviceType + "\r\n";
	strEntry += strAlertLevel + "\r\n";
	//strEntry += strIpAddress + "\r\n";
	strEntry += strAdditionalDesc + "\r\n";
	strEntry += strMacAddress + "\r\n";
	strEntry += strEvent1 + "\r\n";
	//strEntry += strEvent2 + "\r\n";
	return strEntry;
}

string CNetFinder::EntryToStrUSB(NETDISPLAY_ENTRY *pEntry, string strPort)
{	
	string strAlertLevel;
	string strDeviceType;
	string strIpAddress;
	string strEvent1, strEvent2;
	string strAdditionalDesc;
	string strMacAddress;
	string strEntry;
	string temp_str, temp_str2;	
	stringex strfn;

	strAlertLevel = "Connection: ";
	strAlertLevel += strPort;
	//switch(pEntry->alert_level){			// Alert Level
	//	case 0x00:	//0 = Interface is open (unconnected);
	//		strAlertLevel += "Interface is open (unconnected)";
	//		break;
	//	case 0x01:	//1 = Interface is connected (sharing is allowed);
	//		strAlertLevel += "Interface is connected (sharing is allowed)";
	//		break;
	//	case 0x02:	//2 = Interface is connected (sharing is not allowed);
	//		strAlertLevel += "Interface is connected (sharing is not allowed)";
	//		break;
	//	case 0x03:	//3 = Interface is locked
	//		strAlertLevel += "Interface is locked";
	//		break;
	//	case 0x04:	//4 = Interface is unavailable because USB is connected
	//		strAlertLevel += "Interface is unavailable because USB is connected";
	//		break;
	//	default:
	//		strAlertLevel += "Interface configuration unknown";
	//		break;
	//}
	strDeviceType = pEntry->str_type;		// Device Name/Type
	// IP Address String
	strIpAddress = strfn.Format("IP Address: %u.%u.%u.%u", (unsigned short) pEntry->ip[0], (unsigned short) pEntry->ip[1],(unsigned short) pEntry->ip[2],(unsigned short) pEntry->ip[3]); 
	strAdditionalDesc = pEntry->str_description;	// Additional Description
	// MacAddress
	strMacAddress = strfn.Format("MAC Address: %02x-%02x-%02x-%02x-%02x-%02x", (unsigned short) pEntry->mac[0], (unsigned short) pEntry->mac[1],(unsigned short) pEntry->mac[2],(unsigned short) pEntry->mac[3],(unsigned short) pEntry->mac[4],(unsigned short) pEntry->mac[5]); 
	// Event1 Time
	temp_str = pEntry->str_ev1 + " ";
	if(pEntry->event1_days) temp_str2 = strfn.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event1_days,pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_hours) temp_str2 = strfn.Format("%i hours, %i minutes, %i seconds", pEntry->event1_hours,pEntry->event1_minutes, pEntry->event1_seconds );
	else if(pEntry->event1_minutes) temp_str2 = strfn.Format("%i minutes, %i seconds", pEntry->event1_minutes, pEntry->event1_seconds );
	else temp_str2 = strfn.Format("%i seconds", pEntry->event1_seconds );
	strEvent1 = temp_str + temp_str2;
	// Event2 Time
	temp_str = pEntry->str_ev2 + " ";
	if(pEntry->event2_days) temp_str2 = strfn.Format("%i days, %i hours, %i minutes, %i seconds", pEntry->event2_days,pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_hours) temp_str2 = strfn.Format("%i hours, %i minutes, %i seconds", pEntry->event2_hours,pEntry->event2_minutes, pEntry->event2_seconds );
	else if(pEntry->event2_minutes) temp_str2 = strfn.Format("%i minutes, %i seconds", pEntry->event2_minutes, pEntry->event2_seconds );
	else temp_str2 = strfn.Format("%i seconds", pEntry->event2_seconds );
	strEvent2 = temp_str + temp_str2;
	
	strEntry = strDeviceType + "\r\n";
	strEntry += strAlertLevel + "\r\n";
	//strEntry += strIpAddress + "\r\n";
	strEntry += strAdditionalDesc + "\r\n";
	strEntry += strMacAddress + "\r\n";
	strEntry += strEvent1 + "\r\n";
	//strEntry += strEvent2 + "\r\n";
	return strEntry;
}

int CNetFinder::FindEntry(unsigned long SockAddr)
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
	//pEntry->ccConnectRGB = colorWhite;

	pEntry->HasData = false;

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

void CNetFinder::AddEntry(NETDISPLAY_ENTRY *pEntry, unsigned char *buffer, unsigned int destPort)
{
	stringex strfn;

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

	pEntry->str_type = strfn.Format("%s",&buffer[i]);
	i += (int)(pEntry->str_type.length()+1);

	pEntry->str_description = strfn.Format("%s",&buffer[i]);
	i += (int)(pEntry->str_description.length()+1);

	// Copy the Event1+2 text descripion without the ": "
	pEntry->str_ev1 = strfn.Format("%s",&buffer[i]);
	i += (int)(pEntry->str_ev1.length()+1);

	pEntry->str_ev2 = strfn.Format("%s",&buffer[i]);
	i += (int)(pEntry->str_ev2.length()+1);

	// Add ": " to str_ev1 and str_ev2
	pEntry->str_ev1 += ": ";
	pEntry->str_ev2 += ": ";

	pEntry->HasData = true;
	pEntry->SockAddr = SockAddr_ByteToULong(&buffer[20]);
	pEntry->str_display = EntryToStr(pEntry);	// Convert Entry infor to string
}

unsigned long CNetFinder::ByteToInaddr(unsigned char b1, unsigned char b2, unsigned char b3, unsigned char b4) {
    unsigned long s_addr_ul;
    s_addr_ul = htonl(((unsigned long)b1 << 24) | ((unsigned long)b2 << 16) | ((unsigned long)b3 << 8) | (unsigned long)b4);
    return s_addr_ul;
}

in_addr CNetFinder::SockAddr_ByteToInAddr(unsigned char *ip)
{
	in_addr inSockAddr;
	inSockAddr.s_addr = ByteToInaddr(ip[0], ip[1], ip[2], ip[3]);
	return (inSockAddr);
}

unsigned long CNetFinder::SockAddr_ByteToULong(unsigned char *ip)
{
	in_addr inSockAddr;
	inSockAddr = SockAddr_ByteToInAddr(ip);
	return (inSockAddr.s_addr);
}

string CNetFinder::SockAddr_ByteToStr(unsigned char *ip)
{
	string strSockAddr;
	stringex strfn;
	strSockAddr = strfn.Format("%d.%d.%d.%d",ip[0],ip[1],ip[2],ip[3]);
	return (strSockAddr);
}

in_addr CNetFinder::SockAddr_StrToInAddr(string strSockAddr)
{
	unsigned long SockAddr;
	in_addr inSockAddr;
	SockAddr = inet_addr(strSockAddr.c_str());
	inSockAddr.s_addr = SockAddr;
	return (inSockAddr);
}

unsigned long CNetFinder::SockAddr_StrToULong(string strSockAddr)
{
	unsigned long SockAddr;
	SockAddr = inet_addr(strSockAddr.c_str());
	return (SockAddr);
}

in_addr CNetFinder::SockAddr_ULongToInAddr(unsigned long SockAddr)
{
	in_addr inSockAddr;
	inSockAddr.s_addr = SockAddr;
	return (inSockAddr);
}

string CNetFinder::SockAddr_ULongToStr(unsigned long SockAddr)
{
	string strSockAddr;
	stringex strfn;
	in_addr inSockAddr;
	inSockAddr.s_addr = SockAddr;
	strSockAddr = strfn.Format("%s",inet_ntoa(inSockAddr));
	return (strSockAddr);
}

string CNetFinder::SockAddr_InAddrToStr(in_addr inSockAddr)
{
	string strSockAddr;
	stringex strfn;
	strSockAddr = strfn.Format("%s",inet_ntoa(inSockAddr));
	return (strSockAddr);
}




























