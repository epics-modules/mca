#ifndef _LLC_H
#define _LLC_H

#ifndef IFHWADDRLEN
#define IFHWADDRLEN 6
#endif

#define __LLC_SOCK_SIZE__ 16    /* sizeof(sockaddr_llc), word align. */
struct sockaddr_llc {
  u_char          sa_len;
  u_char          sllc_family;    /* AF_LLC */
  u_char          sllc_arphrd;    /* ARPHRD_ETHER */
  unsigned char   sllc_test;
  unsigned char   sllc_xid;
  unsigned char   sllc_ua;        /* UA data, only for SOCK_STREAM. */
  unsigned char   sllc_sap;
  unsigned char   sllc_mac[IFHWADDRLEN];
  unsigned char   __pad[__LLC_SOCK_SIZE__ - sizeof(u_char) * 3 -
			sizeof(unsigned char) * 4 - IFHWADDRLEN];
};

#define AF_LLC 26

int llcAttach(char *device, int unit);
STATUS llcRegister(void);

#endif
