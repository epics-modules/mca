// DppSocket.h: interface for the CDppSocket class.
//
//////////////////////////////////////////////////////////////////////
 
#if !defined(DppSocket_H__INCLUDED_)
#define DppSocket_H__INCLUDED_

#include <osiSock.h>

class CDppSocket  
{
public:
        CDppSocket();
        virtual ~CDppSocket();
		int CreateRand();
		void SetTimeOut(long tv_sec, long tv_usec);
        int SendBroadCast(int m_rand);
        int UDPSendTo(const unsigned char * buf, int len, const char* lpIP, int nPort);
        int UDPRecvFrom(unsigned char * buf, int len, char* lpIP, int &nPort);
		int BroadCastSendTo(const void* lpBuf, int nBufLen,
					unsigned int nHostPort, const char * lpszHostAddress = NULL, int nFlags = 0);
		int HaveDatagram(unsigned char * buf, int len);
		int UDP_recvfrom_TimeOut();
		int GetLocalSocketInfo();
		void AddAddress(const char * address, char addrArr[][20], int iIndex);
		bool HaveNetFinderPacket(const unsigned char * buffer, int m_rand, int num_bytes);
		bool deviceConnected;
		int NumDevices;
		int CurrentDevice;
		char DppAddr[20];
		void SetDppAddress(char addrArr[]);
		int m_rand;						// the netfinder algorithm requires a random number
		/// Sends a USB buffer and returns any data read back.
		bool SendPacketInet(unsigned char Buffer[], CDppSocket *DppSocket, unsigned char PacketIn[]);

protected:
        SOCKET m_hDppSocket;
        bool m_nStartupOK;
		struct timeval timeout;
};
 
#endif 