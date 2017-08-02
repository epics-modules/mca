// DppSocket.cpp: implementation of the CDppSocket class.
//
//////////////////////////////////////////////////////////////////////

#include "DppSocket.h"
#include <string.h>
#include <stdlib.h>
CDppSocket::CDppSocket()
{
	m_rand = 0;

	#ifdef WIN32
	    WSADATA wsaData;
		WORD wVersionRequested = MAKEWORD( 2, 2 );
		int iResult = WSAStartup( wVersionRequested, &wsaData);
		if (iResult != 0) {
				m_nStartupOK = false;
		} else {
				m_nStartupOK = true;
		}
		m_hDppSocket = WSASocket(AF_INET,SOCK_DGRAM,0, 0, 0, 0);	// CDppSocket Socktype is always UDP
	#else
		//if ((m_hDppSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		//if ((m_hDppSocket = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		if ((m_hDppSocket = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
			fcntl(m_hDppSocket, F_SETFL, O_NONBLOCK); // set to non-blocking
			cout << "DppSocket creation failed (socket())" << endl;
			m_nStartupOK = false;
		} else {
			cout << "DppSocket created" << endl;
			m_nStartupOK = true;
		}
	#endif
	timeout.tv_sec = 0;			// 0 sec, 50 msec timeout for recvfrom
	timeout.tv_usec = 50000;		// reset timeout with SetTimeOut
	sockaddr_in sin;
	socklen_t addrlen = sizeof(sin);
	//int local_port=0;
	uint16_t local_port=0;
	int iRes;
	memset(&sin,0,sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = htonl(INADDR_ANY);
	sin.sin_port = 0;

	bind(m_hDppSocket, (struct sockaddr *)&sin, sizeof(sin));
	
	iRes = getsockname(m_hDppSocket,(struct sockaddr *)&sin, &addrlen);
 	if(iRes == 0) {
		if (sin.sin_family == AF_INET) {
			if (addrlen == sizeof(sin)) {
				local_port = ntohs(sin.sin_port);
				cout << local_port << endl;
			}
		}
	} else {
		printf("%d\r\n",iRes);
	}
}
 
CDppSocket::~CDppSocket()
{
	#ifdef WIN32
		if (m_hDppSocket != INVALID_SOCKET) closesocket(m_hDppSocket);
		if (m_nStartupOK == 1) WSACleanup();
	#else
		close(m_hDppSocket);
	#endif
}

// CDppSocket Code
//CDppSocket::CDppSocket() {
void CDppSocket::CDppSocket2() {
	int type = SOCK_DGRAM;
	int protocol = IPPROTO_UDP;
  #ifdef WIN32
    if (!initialized) {
      WORD wVersionRequested;
      WSADATA wsaData;

      wVersionRequested = MAKEWORD(2, 0);              // Request WinSock v2.0
      if (WSAStartup(wVersionRequested, &wsaData) != 0) {  // Load WinSock DLL
		  cout << "Unable to load WinSock DLL" << endl;
      }
      initialized = true;
    }
  #endif

  // Make a new socket
  if ((m_hDppSocket = socket(PF_INET, type, protocol)) < 0) {
	  cout << "DppSocket creation failed (socket())" << endl;
  }
}

//CDppSocket::~CDppSocket() {
void CDppSocket::NotCDppSocket2() {
  #ifdef WIN32
	if (initialized) {
		::closesocket(m_hDppSocket);
		if (WSACleanup() != 0) {
			cout << "WSACleanup() failed" << endl;
		}
	}
  #else
    ::close(m_hDppSocket);
  #endif
  m_hDppSocket = -1;
}

// Send Dpp Style NetFinder Broadcast Identity Request Packets
int CDppSocket::SendNetFinderBroadCast(int m_rand)
{
	int iRetSock;
#ifdef WIN32
	char broadcast = 1;
#else
	int broadcast = 1;
#endif
	int iPort = 3040;
	char buff[6] = { 0,0,0,0,0xF4,0xFA };
	char szIP[100]={"255.255.255.255"};

#ifdef WIN32
	//windows
	iRetSock = setsockopt(m_hDppSocket,SOL_SOCKET,SO_BROADCAST,(char*)&broadcast,sizeof(broadcast));
#else
	// linux/unix
	iRetSock = setsockopt(m_hDppSocket,SOL_SOCKET,SO_BROADCAST,(void*)&broadcast,sizeof(broadcast));
#endif
	if(iRetSock==SOCKET_ERROR) { 
		//printf("setsockopt return error\n");
		return iRetSock; 
	}
	buff[2] = (m_rand >> 8);
	buff[3] = (m_rand & 0x00FF);
	iRetSock = BroadCastSendTo(buff, 6, 3040, NULL);
	if(iRetSock==SOCKET_ERROR) { return iRetSock; }
	return 0;
}

int CDppSocket::BroadCastSendTo(const void* lpBuf, int nBufLen, unsigned int nHostPort, const char *lpszHostAddress, int nFlags)
{
	sockaddr_in sockAddr;
	int iRet;

	memset(&sockAddr,0,sizeof(sockAddr));

	char *lpszAscii;
	if (lpszHostAddress != NULL)		// broadcast only
	{
		return false;
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
		return SOCKET_ERROR;
	}

	sockAddr.sin_port = htons((u_short)nHostPort);
	iRet = sendto(m_hDppSocket, (char *)lpBuf, nBufLen, nFlags, (sockaddr *)&sockAddr, sizeof(sockAddr));
	return iRet;
}

int CDppSocket::UDPSendTo(const unsigned char * buf, int len, const char* lpIP, int nPort)
{      
    sockaddr_in SockAddress;
    SockAddress.sin_family=AF_INET;
	if (lpIP == NULL) {
            SockAddress.sin_addr.s_addr = htonl (INADDR_BROADCAST);
	} else {
            //SockAddress.sin_addr.S_un.S_addr=inet_addr(lpIP);
            SockAddress.sin_addr.s_addr = inet_addr(lpIP);
	}
    SockAddress.sin_port=htons(nPort);
    int nSen=sendto(m_hDppSocket, (reinterpret_cast<const char*>(buf)), len, 0, (sockaddr *)&SockAddress,sizeof(SockAddress));
    return nSen;
}

void CDppSocket::SetBlockingMode()
{
	#ifdef WIN32
         unsigned long nMode = 1; // 1: NON-BLOCKING
    	 if (ioctlsocket (m_hDppSocket, FIONBIO, &nMode) == SOCKET_ERROR)
    	 {
				// error
		 }
	#else
		fcntl(m_hDppSocket, F_SETFL, O_NONBLOCK); // set to non-blocking
	#endif
}

int CDppSocket::UDPRecvFrom(unsigned char * buf, int len, char* lpIP, int &nPort)   
{
    sockaddr_in SockAddress;
    socklen_t fromlen=sizeof(sockaddr);
    int nRcv = 0;
	memset(&SockAddress, 0, sizeof(SockAddress));
	SetBlockingMode();
	nRcv = recvfrom(m_hDppSocket, (reinterpret_cast<char*>(buf)), len, 0, (sockaddr *)&SockAddress,&fromlen);
	if (nRcv > 0) {
		if (bSocketDebug) printf("UDPRecvFrom data done %d\r\n", nRcv);
		if (lpIP != NULL) strcpy(lpIP, inet_ntoa (SockAddress.sin_addr));
		nPort = ntohs(SockAddress.sin_port);
		nPort = 3040;
		printf("UDPRecvFrom Address: %s  Port: %d   bytes:%d\r\n",lpIP,nPort,nRcv);
	} else {
		nRcv = 0;
	}
    return nRcv;
}

// read/return address from NetFinder Binary Buffer
// set port to netfinder port
int CDppSocket::UDPRecvFromNfAddr(unsigned char * buf, int len, char* lpIP, int &nPort)   
{
    struct sockaddr_in SockAddress;
    //struct sockaddr_storage SockAddress;
    socklen_t fromlen=sizeof(SockAddress); 
    unsigned char nf_ip[4];
    int nRcv = 0;
	memset(&SockAddress, 0, sizeof(SockAddress));
	SetBlockingMode();
	nRcv = recvfrom(m_hDppSocket, (reinterpret_cast<char*>(buf)), len, 0, (struct sockaddr *)&SockAddress,&fromlen);
	if (nRcv > 0) {
		if (bSocketDebug) printf("UDPRecvFrom data done %d\r\n", nRcv);
		//if (lpIP != NULL) strcpy(lpIP, inet_ntoa (SockAddress.sin_addr));
		//nPort = ntohs(SockAddress.sin_port);
		nPort = 3040;
		if (nRcv > 67) {
			nf_ip[0] = buf[20];
			nf_ip[1] = buf[21];
			nf_ip[2] = buf[22];
			nf_ip[3] = buf[23];
		    sprintf(lpIP,"%d.%d.%d.%d",nf_ip[0],nf_ip[1],nf_ip[2],nf_ip[3]);
		}
		
		printf("UDPRecvFrom Address: %s  Port: %d   bytes:%d\r\n",lpIP,nPort,nRcv);
	} else {
		nRcv = 0;
	}
    return nRcv;
}

// Returns: data ready if >0, timed out if == 0, error occurred if ==-1
int CDppSocket::UDP_recvfrom_TimeOut()
{
  timeval UDP_timeout;							//177
  fd_set fds;									//178
  SOCKET m_socket;								//179
  m_socket = m_hDppSocket;						//180
  UDP_timeout.tv_sec = timeout.tv_sec;			//181
  UDP_timeout.tv_usec = timeout.tv_usec;		//182
  FD_ZERO(&fds);								//183
  FD_SET(m_socket, &fds);						//184
  return select(1, &fds, (fd_set *)0, (fd_set *)0, &UDP_timeout);	//185
}

void CDppSocket::SetTimeOut(long tv_sec, long tv_usec)
{
	timeout.tv_sec = tv_sec;
	timeout.tv_usec = tv_usec;
}

//int CDppSocket::GetLocalSocketInfo()
//{
	//sockaddr_in sin;
	//socklen_t addrlen = sizeof(sin);
	//int local_port=0;
	//memset(&sin, 0, sizeof(sin));
	
	//bind(m_hDppSocket, (sockaddr *)&sin, sizeof(sin));
	//if(getsockname(m_hDppSocket, (sockaddr *) &sin, &addrlen) == 0) {
		//if (sin.sin_family == AF_INET) {
			//if (addrlen == sizeof(sin)) {
				//local_port = ntohs(sin.sin_port);
				//printf("Local Port: %d\r\n", local_port);
			//}
		//}
	//} else {
		//cout << "Fetch of local port failed (getsockname())" << endl;
	//}
	//return local_port;
//}

// Function to fill in address structure given an address and port
//static void CDppSocket::fillAddr(const string &address, unsigned short port, sockaddr_in &addr) {
static void fillAddr(const string &address, unsigned short port, sockaddr_in &addr) {
  memset(&addr, 0, sizeof(addr));  // Zero out address structure
  addr.sin_family = AF_INET;       // Internet address

  hostent *host;  // Resolve name
  if ((host = gethostbyname(address.c_str())) == NULL) {
	  cout << "Failed to resolve name (gethostbyname())" << endl;
  }
  addr.sin_addr.s_addr = *((unsigned long *) host->h_addr_list[0]);

  addr.sin_port = htons(port);     // Assign port in network byte order
}

string CDppSocket::getLocalAddress() {
  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  bind(m_hDppSocket, (struct sockaddr *)&addr, addr_len);
  if (getsockname(m_hDppSocket, (struct sockaddr *) &addr, (socklen_t *) &addr_len) < 0) {
		cout << "Fetch of local address failed (getsockname())" << endl;
  }
  return inet_ntoa(addr.sin_addr);
}

unsigned short CDppSocket::getLocalPort() {
  sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  memset(&addr, 0, sizeof(addr));
  
  addr.sin_family = AF_INET;
addr.sin_addr.s_addr = htonl(INADDR_ANY);
addr.sin_port = 0;

  //if (getsockname(m_hDppSocket, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) {
bind(m_hDppSocket, (sockaddr *)&addr, addr_len);
  if (getsockname(m_hDppSocket, (sockaddr *) &addr, (socklen_t *) &addr_len) < 0) {
		cout << "Fetch of local port failed (getsockname())" << endl;
  }
  return ntohs(addr.sin_port);
}

int CDppSocket::GetLocalSocketInfo()
{
	
	string strLocalAddr;
	  strLocalAddr = getLocalAddress();
	  cout << strLocalAddr << endl;
	  return (int)(getLocalPort());
}

void CDppSocket::setLocalPort(unsigned short localPort) {
  // Bind the socket to its port
  sockaddr_in localAddr;
  memset(&localAddr, 0, sizeof(localAddr));
  localAddr.sin_family = AF_INET;
  localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  localAddr.sin_port = htons(localPort);

  if (bind(m_hDppSocket, (sockaddr *) &localAddr, sizeof(localAddr)) < 0) {
		cout << "Set of local port failed (bind())" << endl;
  }
}

void CDppSocket::setLocalAddressAndPort(const string &localAddress, unsigned short localPort) {
  // Get the address of the requested host
  sockaddr_in localAddr;
  fillAddr(localAddress, localPort, localAddr);

  if (bind(m_hDppSocket, (sockaddr *) &localAddr, sizeof(sockaddr_in)) < 0) {
		cout << "Set of local address and port failed (bind())" << endl;
  }
}

// CDppSocket Code

void CDppSocket::sendTo(const void *buffer, int bufferLen, const string &foreignAddress, unsigned short foreignPort) {
  sockaddr_in destAddr;
  fillAddr(foreignAddress, foreignPort, destAddr);

  // Write out the whole buffer as a single message.
  if (sendto(m_hDppSocket, (raw_type *) buffer, bufferLen, 0, (sockaddr *) &destAddr, sizeof(destAddr)) != bufferLen) {
		cout << "Send failed (sendto())" << endl;
  }
}

int CDppSocket::recvFrom(void *buffer, int bufferLen, string &sourceAddress, unsigned short &sourcePort) {
	sockaddr_in clntAddr;
	socklen_t addrLen = sizeof(clntAddr);
	int rtn;
	if ((rtn = recvfrom(m_hDppSocket, (raw_type *) buffer, bufferLen, 0, (sockaddr *) &clntAddr, (socklen_t *) &addrLen)) < 0) {
		cout << "Receive failed (recvfrom())" << endl;
	}
	sourceAddress = inet_ntoa(clntAddr.sin_addr);
	sourcePort = ntohs(clntAddr.sin_port);

	return rtn;
}

// UDPSocket Code
void CDppSocket::UDPSocket(unsigned short localPort) {	
	//CDppSocket(SOCK_DGRAM, IPPROTO_UDP);
  setLocalPort(localPort);
  int broadcastPermission = 1;
  setsockopt(m_hDppSocket, SOL_SOCKET, SO_BROADCAST, (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
}

void CDppSocket::UDPSocketClient() {
  int broadcastPermission = 1;
  setsockopt(m_hDppSocket, SOL_SOCKET, SO_BROADCAST, (raw_type *) &broadcastPermission, sizeof(broadcastPermission));
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

// for Sending Dpp Style NetFinder Broadcast Identity Request Packets
// save to netfinder for dpp response detection
// FindDpp.m_rand = m_rand;		
int CDppSocket::CreateRand()
{
	int m_rand;
	srand((unsigned)time(NULL));
	do{
		m_rand = (rand());		// rand returns a number between 0 and 0x7FFF
		m_rand &= 0x7FFF;		// only use 0x0000 to 0x7FFF bits of m_rand
	} while(m_rand == 0);		// make sure m_rand is not zero
	return m_rand;
}

bool CDppSocket::HaveNetFinderPacket(const unsigned char buffer[], int m_rand, int num_bytes)
{
	bool bHaveNetFinderPacket=false;
	printf("m_rand: %X b0: %X b1: %X b2: %X\n",m_rand,buffer[0],buffer[2],buffer[3]);
	if ((num_bytes >= 32) && 
		(buffer[0] == 0x01) && 
		(buffer[2] == (m_rand >> 8)) && 
		(buffer[3] == (m_rand & 0x00FF))) 
	{		// have Silicon Labs Netfinder packet
		printf("NetFinder Packet Found\n");
		bHaveNetFinderPacket = true;	
	}  else if((buffer[0] == 0x00) && 
		(buffer[2] == (m_rand >> 8)) && 
		(buffer[3] == (m_rand & 0x00FF))) 
	{
		printf("unknown packet type\n");
		bHaveNetFinderPacket = false;
	} else {
		bHaveNetFinderPacket = true;
		printf("no packet test\n");
	}
	return bHaveNetFinderPacket;
}

bool CDppSocket::SendPacketInet(unsigned char Buffer[], CDppSocket *DppSocket, unsigned char PacketIn[], int iRequestedSize)
{
    int success;
    long PLen;
	char szDPP[20]={0};
	int nPort;
	int iSize=0;
	int iTotal=0;
	int iDataLoops = 0;

	PLen = (Buffer[4] * 256) + Buffer[5] + 8;
	success = DppSocket->UDPSendTo(Buffer, PLen, DppSocket->DppAddr, 10001);
 	Sleep(50);				// 20110907 Added to improve usb communications while running other processes
	if (success) {
		do {	// get all the spectrum packets and status
				// the data is directly loaded into the data buffer
				// this removes the need to copy the data from one buffer to another
			if (bSocketDebug) printf("before UDPRecvFrom\r\n");
			iSize = DppSocket->UDPRecvFrom(&PacketIn[iTotal], 1024, szDPP, nPort);
			if (bSocketDebug) printf("SendPacketInet New Bytes Received:%d\r\n", iSize);
			if (bSocketDebug) printf("after UDPRecvFrom\r\n");
			if (iSize > 0) {
				if (bSocketDebug) printf("bytes:%d\r\n", iSize);
				if (iTotal+iSize>24648) {
					if (bSocketDebug) printf("SendPacketInet Overflow\r\n");
					iDataLoops += 50;	// exit loop when done
					break;				// this is an error, exit loop
				}
				iTotal += iSize;
				if (iTotal == iRequestedSize) {
					if (bSocketDebug) printf("SendPacketInet Message Found\r\n");
					iDataLoops += 50;	// exit loop when done
					break;				// this is an error, exit loop	
				}
			}
			//} else {
				//iDataLoops += 50;		// exit loop when done
				//printf("Socket Return W/O Size\r\n");
			//}
			iDataLoops++;
			if (bSocketDebug) printf("DataLoops Looping:%d\r\n", iDataLoops);
		} while ((iSize > 0) && (iDataLoops < 50));  // there can be up to 48 packets
		if (bSocketDebug) printf("DataLoops Done:%d\r\n", iDataLoops);
		success = iTotal;
		if (bSocketDebug) printf("SendPacketInet Total Bytes Received:%d\r\n", iTotal);
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
