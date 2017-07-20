// DppSocket.cpp: implementation of the CDppSocket class.
//
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <osiSock.h>
#include <epicsThread.h>

#include "DppSocket.h"
//#include <iostream>
//using namespace std;

CDppSocket::CDppSocket()
{
  if (osiSockAttach() == 0) {
			m_nStartupOK = false;
	} else {
			m_nStartupOK = true;
	}
	timeout.tv_sec = 3;			// 3 sec, 500 usec timeout for recvfrom
	timeout.tv_usec = 500;		// reset timeout with SetTimeOut
	m_hDppSocket = epicsSocketCreate(AF_INET,SOCK_DGRAM,IPPROTO_UDP);	// CDppSocket Socktype is always UDP

	sockaddr_in sin={0};
	socklen_t addrlen = sizeof(sin);
	int local_port=0;
	int iRes;
	iRes = getsockname(m_hDppSocket,(struct sockaddr *)&sin, &addrlen);
 	if(iRes == 0) {
		if (sin.sin_family == AF_INET) {
			if (addrlen == sizeof(sin)) {
				local_port = ntohs(sin.sin_port);
			}
		}
	} else {
		printf("%d\r\n",iRes);
	}
}
 
CDppSocket::~CDppSocket()
{
	if (m_hDppSocket != INVALID_SOCKET) epicsSocketDestroy(m_hDppSocket);
	if (m_nStartupOK == 1) osiSockRelease();
}

// for Sending Dpp Style NetFinder Broadcast Identity Request Packets
// save to netfinder for dpp response detection
// FindDpp.m_rand = m_rand;		
int CDppSocket::CreateRand()
{
	int m_rand;
	do{
		m_rand = (rand());	// rand returns a number between 0 and 0x7FFF
	} while(m_rand == 0);    // make sure m_rand is not zero
	return m_rand;
}

// Send Dpp Style NetFinder Broadcast Identity Request Packets
int CDppSocket::SendBroadCast(int m_rand)
{
	int iRetSock;
	bool broadcast = 1;
	unsigned char buff[6] = { 0,0,0,0,0xF4,0xFA };

	iRetSock = setsockopt(m_hDppSocket,SOL_SOCKET,SO_BROADCAST,(char*)&broadcast,sizeof(broadcast));
	if(iRetSock != 0) { return iRetSock; }
	buff[2] = (m_rand >> 8);
	buff[3] = (m_rand & 0x00FF);
	iRetSock = BroadCastSendTo(buff, 6, 3040, NULL);
	if(iRetSock !=0) { return iRetSock; }
	return 0;
}

int CDppSocket::BroadCastSendTo(const void* lpBuf, int nBufLen, unsigned int nHostPort, const char* lpszHostAddress, int nFlags)
{
	sockaddr_in sockAddr;
	int iRet;

	memset(&sockAddr,0,sizeof(sockAddr));

	const char* lpszAscii;
	if (lpszHostAddress != NULL)		// broadcast only
	{
		return 0;
	}
	else
	{
		lpszAscii = NULL;
	}

	sockAddr.sin_family = AF_INET;

	if (lpszAscii == NULL)
		sockAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
	else
	{
		return -1;
	}

	sockAddr.sin_port = htons((u_short)nHostPort);
	iRet = sendto(m_hDppSocket, (const char*)lpBuf, nBufLen, nFlags, (sockaddr*)&sockAddr, sizeof(sockAddr));
	return iRet;
}


int CDppSocket::UDPSendTo(const unsigned char * buf, int len, const char* lpIP, int nPort)
{      
    sockaddr_in SockAddress;
    SockAddress.sin_family=AF_INET;
    if (lpIP == NULL)
            SockAddress.sin_addr.s_addr = htonl (INADDR_BROADCAST);
    else
            SockAddress.sin_addr.s_addr=inet_addr(lpIP);
    SockAddress.sin_port=htons(nPort);
    int nSen=sendto(m_hDppSocket, (reinterpret_cast<const char*>(buf)), len, 0, (sockaddr*)&SockAddress,sizeof(SockAddress));
    return nSen;
}

int CDppSocket::UDPRecvFrom(unsigned char * buf, int len, char* lpIP, int &nPort)   
{
    sockaddr_in SockAddress={0};
    socklen_t fromlen=sizeof(sockaddr);
    int nRcv;

	nRcv = UDP_recvfrom_TimeOut();
	if (nRcv > 0) {
		nRcv = recvfrom(m_hDppSocket, (reinterpret_cast<char*>(buf)), len, 0, (sockaddr*)&SockAddress,&fromlen);
 		if (lpIP != NULL) strcpy(lpIP, inet_ntoa (SockAddress.sin_addr));
		nPort = ntohs(SockAddress.sin_port);
	}
    return nRcv;
}

// get buffer, return number of bytes, leave the data in the rcv socket buffer
int CDppSocket::HaveDatagram(unsigned char * buf, int len)
{
    sockaddr_in SockAddress={0};
    socklen_t fromlen=sizeof(sockaddr);
    int nRcv;
	nRcv = UDP_recvfrom_TimeOut();
	if (nRcv > 0) {
		nRcv = recvfrom(m_hDppSocket, (reinterpret_cast<char*>(buf)), len, MSG_PEEK, (sockaddr*)&SockAddress,&fromlen);
	}
    return nRcv;
}

// Returns: data ready if >0, timed out if == 0, error occurred if ==-1
int CDppSocket::UDP_recvfrom_TimeOut()
{
  struct timeval UDP_timeout;
  fd_set fds;
  SOCKET m_socket;
  m_socket = m_hDppSocket;
  UDP_timeout.tv_sec = timeout.tv_sec;
  UDP_timeout.tv_usec = timeout.tv_usec;
  FD_ZERO(&fds);
  FD_SET(m_socket, &fds);
  return select(0, &fds, 0, 0, &UDP_timeout);
}

void CDppSocket::SetTimeOut(long tv_sec, long tv_usec)
{
	timeout.tv_sec = tv_sec;
	timeout.tv_usec = tv_usec;
}

int CDppSocket::GetLocalSocketInfo()
{
	sockaddr_in sin={0};
	socklen_t addrlen = sizeof(sin);
	int local_port=0;
	if(getsockname(m_hDppSocket,(struct sockaddr *)&sin, &addrlen) == 0) {
		if (sin.sin_family == AF_INET) {
			if (addrlen == sizeof(sin)) {
				local_port = ntohs(sin.sin_port);
			}
		}
	}
	return local_port;
}

void CDppSocket::AddAddress(const char * address, char addrArr[][20], int iIndex)
{
	for (int idxCh=0;idxCh<20;idxCh++){
		addrArr[iIndex][idxCh] = address[idxCh];
	}
}

void CDppSocket::SetDppAddress(char addrArr[])
{
	for (int idxCh=0;idxCh<20;idxCh++){
		DppAddr[idxCh] = addrArr[idxCh];
	}
}

bool CDppSocket::HaveNetFinderPacket(const unsigned char buffer[], int m_rand, int num_bytes)
{
	bool bHaveNetFinderPacket=false;
	if ((num_bytes >= 32) && 
		(buffer[0] == 0x01) && 
		(buffer[2] == (m_rand >> 8)) && 
		(buffer[3] == (m_rand & 0x00FF))) 
	{		// have Silicon Labs Netfinder packet
		bHaveNetFinderPacket = true;	
	}  else if((buffer[0] == 0x00) && 
		(buffer[2] == (m_rand >> 8)) && 
		(buffer[3] == (m_rand & 0x00FF))) 
	{
		bHaveNetFinderPacket = false;
	}
	return bHaveNetFinderPacket;
}

bool CDppSocket::SendPacketInet(unsigned char Buffer[], CDppSocket *DppSocket, unsigned char PacketIn[])
{
    int success;
    long PLen;
	char szDPP[20]={0};
	int nPort;
	int iSize;
	int iTotal=0;
	int iDataLoops = 0;

	PLen = (Buffer[4] * 256) + Buffer[5] + 8;
	success = DppSocket->UDPSendTo(Buffer, PLen, DppSocket->DppAddr, 10001);
 	epicsThreadSleep(0.05);				// 20110907 Added to improve usb communications while running other processes
	if (success) {
		do {	// get all the spectrum packets and status
				// the data is directly loaded into the data buffer
				// this removes the need to copy the data from one buffer to another
			iSize = DppSocket->UDPRecvFrom(&PacketIn[iTotal], 1024, szDPP, nPort);
			if (iSize > 0) {
				//printf("bytes:%d\r\n", iSize);
				if (iTotal+iSize>24648) {
					iDataLoops += 50;	// exit loop when done
					break;				// this is an error, exit loop
				}
				iTotal += iSize;
			} else {
				iDataLoops += 50;		// exit loop when done
			}
			iDataLoops++;
		} while ((iSize > 0) && (iDataLoops < 50));  // there can be up to 48 packets
		success = iTotal;
 		if (success) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
	return false;
}
