#pragma once
#include "X11Color.h"

/////////////////////////////////////////////////////////////////////////////
// NetFinder Data

typedef struct NETDISPLAY_ENTRY {
	BYTE alert_level;
	
	UINT event1_days;
	BYTE event1_hours;
	BYTE event1_minutes;
	
	UINT event2_days;
	BYTE event2_hours;
	BYTE event2_minutes;

	BYTE event1_seconds;
	BYTE event2_seconds;

	BYTE mac[6];
	BYTE ip[4];
	USHORT port; // Get port from UDP header
	BYTE subnet[4];
	BYTE gateway[4];
	
	CString str_type;
	CString str_description;
	CString str_ev1;
	CString str_ev2;

	CString str_display;
	COLORREF ccConnectRGB;

	BOOL HasData;

	ULONG SockAddr;

} NETDISPLAY_ENTRY;

const int NO_ENTRIES = -1;
const int MAX_ENTRIES = 10;

class CNetFinder
{
public:
	CNetFinder(void);
	~CNetFinder(void);

	int m_rand;
	NETDISPLAY_ENTRY DppEntries[MAX_ENTRIES];	// 20140430 changed hardcode to const var
	int LastEntry;

	ULONG DppSockAddr;					// socket address of selected DPP
	int DppEntryIdx;					// entry index of selected DPP
	BOOL DppFound;						// selected DPP found

	CString EntryToStr(NETDISPLAY_ENTRY *pEntry);
	CString EntryToStrRS232(NETDISPLAY_ENTRY *pEntry, CString cstrPort);
	CString EntryToStrUSB(NETDISPLAY_ENTRY *pEntry, CString cstrPort);
	COLORREF AlertEntryToCOLORREF(NETDISPLAY_ENTRY *pEntry);
	COLORREF AlertByteToCOLORREF(BYTE alert_level);
	
	int FindEntry(ULONG SockAddr);

	CString SockAddr_DlgToStr(CDialog *dlg, int id1, int id2, int id3, int id4);
	UINT SockPort_DlgToUINT(CDialog *dlg, int id1);

	in_addr SockAddr_ByteToInAddr(BYTE *ip);
	ULONG SockAddr_ByteToULong(BYTE *ip);
	CString SockAddr_ByteToStr(BYTE *ip);

	in_addr SockAddr_StrToInAddr(CString cstrSockAddr);	// BYTES in_addr.S_un.S_un_b.s_b* 
	ULONG SockAddr_StrToULong(CString cstrSockAddr);

	in_addr SockAddr_ULongToInAddr(ULONG SockAddr);
	CString SockAddr_ULongToStr(ULONG SockAddr);

	CString SockAddr_InAddrToStr(in_addr inSockAddr);

	void InitEntry(NETDISPLAY_ENTRY *pEntry);
	void InitAllEntries();
	void AddEntry(NETDISPLAY_ENTRY *pEntry, BYTE *buffer, UINT destPort);
	
};

