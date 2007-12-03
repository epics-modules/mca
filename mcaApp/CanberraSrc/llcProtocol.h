#ifndef LLC_PROTOCOL_H
#define LLC_PROTOCOL_H

#include "llc.h"
int llcSendPacket(struct sockaddr_llc *addr, int sap, int type, 
		  char *data, int datalen);
int llcReceivePacket(struct sockaddr_llc *addr, char *data, int len);

#endif
