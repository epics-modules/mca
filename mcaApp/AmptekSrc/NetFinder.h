#ifndef __NETFINDER_INCLUDED__
#define __NETFINDER_INCLUDED__

#pragma once
#include "stringex.h"
#ifdef WIN32
	#include <winsock2.h>
	typedef int socklen_t;
	typedef char raw_type;
	#include <time.h>
	#pragma comment(lib, "Ws2_32.lib")
	#pragma warning(disable:4309)
#else
	typedef int SOCKET;
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/time.h>
	#include <netdb.h>
	#include <arpa/inet.h>
	#include <unistd.h>
	#define Sleep(x) usleep((x)*1000)
	#include <netinet/in.h>
	typedef void raw_type;
#endif

/////////////////////////////////////////////////////////////////////////////
// NetFinder Data

typedef struct NETDISPLAY_ENTRY {
	unsigned char alert_level;
	
	unsigned int event1_days;
	unsigned char event1_hours;
	unsigned char event1_minutes;
	
	unsigned int event2_days;
	unsigned char event2_hours;
	unsigned char event2_minutes;

	unsigned char event1_seconds;
	unsigned char event2_seconds;

	unsigned char mac[6];
	unsigned char ip[4];
	unsigned short port; // Get port from UDP header
	unsigned char subnet[4];
	unsigned char gateway[4];
	
	string str_type;
	string str_description;
	string str_ev1;
	string str_ev2;

	string str_display;

	bool HasData;

	unsigned long SockAddr;

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

	unsigned long DppSockAddr;					// socket address of selected DPP
	int DppEntryIdx;					// entry index of selected DPP
	bool DppFound;						// selected DPP found

	string EntryToStr(NETDISPLAY_ENTRY *pEntry);
	string EntryToStrRS232(NETDISPLAY_ENTRY *pEntry, string strPort);
	string EntryToStrUSB(NETDISPLAY_ENTRY *pEntry, string strPort);
	
	int FindEntry(unsigned long SockAddr);

	//string SockAddr_DlgToStr(CDialog *dlg, int id1, int id2, int id3, int id4);
	//unsigned int SockPort_DlgToUINT(CDialog *dlg, int id1);

	unsigned long ByteToInaddr(unsigned char b1, unsigned char b2, unsigned char b3, unsigned char b4);
	in_addr SockAddr_ByteToInAddr(unsigned char *ip);
	unsigned long SockAddr_ByteToULong(unsigned char *ip);
	string SockAddr_ByteToStr(unsigned char *ip);

	in_addr SockAddr_StrToInAddr(string strSockAddr);	// BYTES in_addr.S_un.S_un_b.s_b* 
	unsigned long SockAddr_StrToULong(string strSockAddr);

	in_addr SockAddr_ULongToInAddr(unsigned long SockAddr);
	string SockAddr_ULongToStr(unsigned long SockAddr);

	string SockAddr_InAddrToStr(in_addr inSockAddr);

	void InitEntry(NETDISPLAY_ENTRY *pEntry);
	void InitAllEntries();
	void AddEntry(NETDISPLAY_ENTRY *pEntry, unsigned char *buffer, unsigned int destPort);
	
};

#endif
