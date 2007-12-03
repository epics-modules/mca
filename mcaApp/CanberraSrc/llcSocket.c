/********************************************************
 * llcSocket.c
 *
 * a vxWorks driver to provide a socket interface to
 * the IEEE 802.2 LLC network service
 *
 * Note that this is a severely cut-down version, supporting
 * only the SNAP SAP (0xAA), and therefore only UI frames.
 *
 * In order to use this socket library, you need to load the library,
 * then call llcRegister(), to install this socket back-end, then
 * attach the network stack component to the correct interface e.g.
 *
 * ld < llcLibrary.o
 * llcRegister()
 * llcAttach("gei",0)
 * 
 * For more details on where this module fits into the VxWorks network
 * environment, see "VxWorks 5.5 Network Programmer's Guide"
 * DOC-14618-ND-00, pp 7, 223-247
 *
 * Peter Denison, Diamond Light Source Ltd.
 ********************************************************/

/* Revision History
 * 
 * 2005-11-10 0.01 Written
 */

#include <vxWorks.h>
#include <stdlib.h>
#include <iosLib.h>
#include <sockLib.h>
#include <sockFunc.h>
#include <errnoLib.h>
#include <errno.h>
#include <sys/socket.h>
#include <net/socketvar.h>
#include <net/if_llc.h>
#include <stdio.h>
#include "llcProtocol.h"

struct llc_socket {
    struct socket    so;
    struct sockaddr_llc addr;
    struct sockaddr_llc remote;
};

DEV_HDR socket_dev;

volatile int llcDebug = 0;

/*
 * Functions required by the socket layer
 */

int llcSockClose(struct socket *so)
{
    if (so) {
	free(so);
    }
    return 0;
}

int llcSockRead(struct socket *so, char *buf, int bufLen)
{
  return -1;
}

int llcSockWrite(struct socket *so, char *buf, int bufLen)
{
    int written;
    struct llc_socket *s = (struct llc_socket *)so;

    if (s->so.so_state != SS_ISCONNECTED) {
	errnoSet(ENOTCONN);
	return -1;
    }

    written = llcSendPacket(&(s->remote), s->addr.sllc_sap, LLC_UI, buf, bufLen);
    if (written == ERROR) {
	return -1;
    } else {
	return written;
    }
}

int llcSockIoctl(int fd, int function, int arg)
{
  return -1;
}

/*
 * Socket functions implemented by this back-end
 */

int llcSocket(int domain, int type, int protocol)
{
    struct llc_socket *s;

    if (llcDebug > 0) printf("llcSocket: domain=%d, type=%d, protocol=%d\n",
			     domain, type, protocol);
    /* Check that we are be called with a domain and socket type that
       are supported */
    if (domain != AF_LLC) {
	errnoSet(EAFNOSUPPORT);
	return ERROR;
    }

    if (type != SOCK_DGRAM) {
	errnoSet(ESOCKTNOSUPPORT);
	return ERROR;
    }

    /* Create the internal socket structure */
    s = malloc(sizeof(struct llc_socket));
    if (!s) {
	return ERROR;
    }

    /* Create a new file descriptor for this device, passing the internal
       socket structure as the private parameter */
    return iosFdNew(&socket_dev, socket_dev.name, (int)s);
}

STATUS llcSockBind(int fd, struct sockaddr *name, int namelen)
{
    struct sockaddr_llc *addr = (struct sockaddr_llc *)name;
    struct llc_socket *s = (struct llc_socket *)iosFdValue(fd);
    
    /* We only support binding to the SNAP Service Access Point */
    /* If this were expanded to be a fully-functional implementation,
       the steps would be something like:
       o) if sap==0, autoallocate a sap
       o) if sap not in use, mark as in use
       o) create a queue for receiving packets??
       o) maintain a list of active saps, and their queues
    */
    if (addr->sllc_sap != LLC_SNAP_LSAP) {
	errnoSet(EADDRNOTAVAIL);
	return ERROR;
    }
    /* Copy the address into the private socket structure */
    memcpy(&(s->addr), addr, sizeof(struct sockaddr_llc));
    return 0;
}

STATUS llcSockConnect(int fd, struct sockaddr *name, int namelen)
{
    errnoSet(EOPNOTSUPP);
    return ERROR;
}

STATUS llcSockListen(int fd, int backlog)
{
    errnoSet(EOPNOTSUPP);
    return ERROR;
}

STATUS llcSockRecvFrom(int fd, char *buf, int bufLen, int flags,
			struct sockaddr *from, int *pFromLen)
{
    struct llc_socket *s = (struct llc_socket *)iosFdValue(fd);
    int size;

    size = llcReceivePacket((struct sockaddr_llc *)from, buf, bufLen);

    /* If source address filled in, set its length */
    if (from) {
	*pFromLen = sizeof(struct sockaddr_llc);
    }
    return size;
}

STATUS llcSockSendTo(int fd, caddr_t buf, int bufLen, int flags,
		      struct sockaddr *to, int tolen)
{
    struct llc_socket *s = (struct llc_socket *)iosFdValue(fd);
    int sap;
    int type;
    
    sap = s->addr.sllc_sap;
    type = LLC_UI;
    return llcSendPacket((struct sockaddr_llc *)to, sap, type, buf, bufLen);
}

/**
 * We do not support Zbuf-style sockets, so return FALSE
 */
STATUS llcSockZbuf(void)
{
    return FALSE;
}

/**
 * Installs the socket back-end for the network service
 */
SOCK_FUNC * llcSockLibInit(void)
{
    SOCK_FUNC *llcSockFuncs;
    int driverNum;

    /* Install driver for socket */
    driverNum = iosDrvInstall( (FUNCPTR) NULL,
			       (FUNCPTR) NULL,
			       (FUNCPTR) NULL,
			       (FUNCPTR) llcSockClose,
			       (FUNCPTR) llcSockRead,
			       (FUNCPTR) llcSockWrite,
			       (FUNCPTR) llcSockIoctl);
    if (driverNum == ERROR) {
	perror("Socket backend:");
	return (SOCK_FUNC *)NULL;
    }

    if (llcDebug > 0) printf("LLC Socket back-end installed\n");

    /* Store the driver number */
    socket_dev.drvNum = driverNum;
    socket_dev.name = "(llc socket)";

    /* Initialize SOCK_FUNC table */
    llcSockFuncs = (SOCK_FUNC *) malloc(sizeof(SOCK_FUNC));
    if (!llcSockFuncs) {
	errnoSet(ENOMEM);
	return (SOCK_FUNC *)NULL;
    }

    llcSockFuncs->libInitRtn = (FUNCPTR)llcSockLibInit;
    llcSockFuncs->acceptRtn  = (FUNCPTR)NULL;
    llcSockFuncs->bindRtn    = (FUNCPTR)llcSockBind;
    llcSockFuncs->connectRtn = (FUNCPTR)llcSockConnect;
    llcSockFuncs->connectWithTimeoutRtn = (FUNCPTR)NULL;
    llcSockFuncs->getpeernameRtn = (FUNCPTR)NULL;
    llcSockFuncs->getsocknameRtn = (FUNCPTR)NULL;
    llcSockFuncs->listenRtn  = (FUNCPTR)llcSockListen;
    llcSockFuncs->recvRtn    = (FUNCPTR)NULL;
    llcSockFuncs->recvfromRtn = (FUNCPTR)llcSockRecvFrom;
    llcSockFuncs->recvmsgRtn = (FUNCPTR)NULL;
    llcSockFuncs->sendRtn    = (FUNCPTR)NULL;
    llcSockFuncs->sendtoRtn  = (FUNCPTR)llcSockSendTo;
    llcSockFuncs->sendmsgRtn = (FUNCPTR)NULL;
    llcSockFuncs->shutdownRtn = (FUNCPTR)NULL;
    llcSockFuncs->socketRtn  = (FUNCPTR)llcSocket;
    llcSockFuncs->getsockoptRtn = (FUNCPTR)NULL;
    llcSockFuncs->setsockoptRtn = (FUNCPTR)NULL;
    llcSockFuncs->zbufRtn    = (FUNCPTR)llcSockZbuf;
    return llcSockFuncs;
}

/**
 * register the socket back-end with the vxWorks socket interface
 *
 */
STATUS llcRegister(void)
{
    STATUS ret;
    ret = sockLibAdd((FUNCPTR)llcSockLibInit, AF_LLC, AF_LLC);
    if (ret != OK) {
	perror("Failed to install socket back-end");
    }
    return ret;
}

