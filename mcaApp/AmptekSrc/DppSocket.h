// DppSocket.h: interface for the CDppSocket class.
//
//////////////////////////////////////////////////////////////////////
#ifndef __DPPSOCKET_INCLUDED__
#define __DPPSOCKET_INCLUDED__

#include <iostream>          // For cout and cerr
#include <string>            // For string
#include <stdio.h>
#ifdef WIN32
	#include <winsock2.h>
	typedef int socklen_t;
	typedef char raw_type;
	#include <time.h>
	#pragma comment(lib, "Ws2_32.lib")
	#pragma warning(disable:4309)
	static bool initialized = false;
	typedef unsigned char uint8_t;
	typedef unsigned short uint16_t;
	typedef unsigned int uint32_t;
#else
	typedef void raw_type;
	typedef int SOCKET;
	#include <netinet/in.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <sys/fcntl.h>
	#include <sys/time.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <unistd.h>
	#define Sleep(x) usleep((x)*1000)
#endif
using namespace std;

#ifndef SOCKET_ERROR
	#define SOCKET_ERROR (-1)
#endif

#ifndef IPPROTO_UDP
	#define IPPROTO_UDP             17              /* user datagram protocol */
#endif

const bool bSocketDebug = false;

class CDppSocket {
public:
	CDppSocket();
	~CDppSocket();

	void CDppSocket2();
	void NotCDppSocket2();

	void SetTimeOut(long tv_sec, long tv_usec);		// set socket timout to new value
	int SendNetFinderBroadCast(int m_rand);

	int UDPSendTo(const unsigned char * buf, int len, const char* lpIP, int nPort);
	void SetBlockingMode();
	int UDPRecvFrom(unsigned char * buf, int len, char* lpIP, int &nPort);
	int UDPRecvFromNfAddr(unsigned char * buf, int len, char* lpIP, int &nPort);

	int BroadCastSendTo(const void* lpBuf, int nBufLen, unsigned int nHostPort, const char *lpszHostAddress = NULL, int nFlags = 0);
	int UDP_recvfrom_TimeOut();
	int GetLocalSocketInfo();

	void AddAddress(const char * address, char addrArr[][20], int iIndex);	// add address to device list
	void SetDppAddress(char addrArr[]);					// set socket destination address

	/// Sends a USB buffer and returns any data read back.
	bool SendPacketInet(unsigned char Buffer[], CDppSocket *DppSocket, unsigned char PacketIn[], int iRequestedSize = 24648);

	//static void fillAddr(const string &address, unsigned short port, sockaddr_in &addr);
	string getLocalAddress();
	unsigned short getLocalPort();
	void setLocalPort(unsigned short localPort);
	void setLocalAddressAndPort(const string &localAddress, unsigned short localPort = 0);

	// socket
	void sendTo(const void *buffer, int bufferLen, const string &foreignAddress, unsigned short foreignPort);
	int recvFrom(void *buffer, int bufferLen, string &sourceAddress, unsigned short &sourcePort);

	void UDPSocket(unsigned short localPort);
	void UDPSocketClient();

	// NetFinder Support
	int CreateRand();
	bool HaveNetFinderPacket(const unsigned char * buffer, int m_rand, int num_bytes);

	// Socket Data Storage
	int m_rand;						// the netfinder algorithm requires a random number
	bool deviceConnected;			// device coneected indicator
	int NumDevices;					// Number of devices found by NetFinder
	char DppAddr[20];				// hold DPP device IP Address
	unsigned long ulNetFinderAddr;  // 

protected:
	SOCKET m_hDppSocket;			// holds current socket descriptor
	bool m_nStartupOK;				// windows socket startup ok, test for cleanup
	struct timeval timeout;			// current socket timeout
};

#endif
