
/********************************************************
 * muxTkTest.c
 *
 * Test program for muxTk API
 *
 * Mark Rivers
 ********************************************************/


#include <vxWorks.h>
#include <muxLib.h>
#include <muxTkLib.h>
#include <usrLib.h>
#include <netinet/if_ether.h>
#include <stdio.h>
#include <stdlib.h>

#define HEADER_OFFSET 40  /* This is a kludge */

/* Function prototypes */
BOOL muxTkTestRcvRtn( void *netCallbackId, long type, M_BLK_ID pNetBuf,
		      void *pSpareData);
STATUS muxTkTestShutdownRtn(void * netCallbackId);
STATUS muxTkTestRestartRtn( void *netCallbackId);
void muxTkTestErrorRtn( void *netCallbackId,
		    END_ERR *pError);
int muxTkTestAttach(char *device, int unit);



/** Debug level. Set to non-zero for output of messages
 */
volatile int muxTkTestDebug=0;

/* Cookie used for detach */
void *muxTkTestCookie=NULL;

/**
 * Callback from the MUX to receive packets
 * netCallbackId the handle/ID installed at bind time
 * type network service type
 * pNetBuf network service datagram
 * pSpareData pointer to optional data from driver
 * TRUE if the packet was "consumed", and FALSE if the packet was ignored
 */
BOOL muxTkTestRcvRtn( void *netCallbackId, long type, M_BLK_ID pNetBuf,
		      void *pSpareData)
{
    if (muxTkTestDebug > 0) {
        printf("muxTkTestRcvRtn: received packet, passed type: %04lx \n", type); 
    }
    return FALSE;
}

/**
 * called by MUX in response to muxDevUnload()
 * @param netCallbackId the handle/ID installed at bind time
 *
 * Disconnect this module from the MUX stack
 */
STATUS muxTkTestShutdownRtn(void * netCallbackId /* the handle/ID installed at bind time */)
{
    return (muxUnbind(netCallbackId, MUX_PROTO_SNARF, muxTkTestRcvRtn));
}

/**
 * called by MUX to restart network services
 * @param netCallbackId the handle/ID installed at bind time
 *
 * We hold no connection-oriented information, so do nothing
 */
STATUS muxTkTestRestartRtn( void *netCallbackId /* the handle/ID installed at bind time */)
{
    return OK;
}

/**
 * called by MUX to handle error conditions encountered by network drivers
 * @param void *netCallbackId the handle/ID installed at bind time
 * @param END_ERR *pError an error structure, containing code and message
 */
void muxTkTestErrorRtn( void *netCallbackId, /* the handle/ID installed at bind time */
		    END_ERR *pError) /* pointer to struct containing error */
{
    printf("Error in protocol layer: %d\n", pError->errCode);
    if (pError->pMesg) {
	printf(pError->pMesg);
    }
}

/**
 * initialise muxTkTest network service
 * device The device name of the interface to attach this protocol to
 * unit The unit number of the interface
 * return 1 if the device was correctly opened, 0 otherwise
 *
 * The service has to be bound as a "SNARF" protocol device, since the MUX
 * decides which protocol to call based on the ethernet type field, and for
 * LLC, this can be variable.
 */
int muxTkTestAttach(char *device, int unit)
{
    if (muxTkTestCookie != NULL) return -1;  /* Already attached */
    
    muxTkTestCookie= muxTkBind (device, /* char * pName, interface name, for example: ln, ei */
		       unit, /* int unit, unit number */
		       &muxTkTestRcvRtn, /* muxTkTestRcvRtn( ) Receive data from the MUX */
		       &muxTkTestShutdownRtn, /* muxTkTestShutdownRtn( ) Shut down the network service. */
		       &muxTkTestRestartRtn, /* muxTkTestRestartRtn( ) Restart a suspended network service.*/
		       &muxTkTestErrorRtn, /* muxTkTestErrorRtn( ) Receive an error notification from the MUX. */
		       MUX_PROTO_SNARF, /* long type, from RFC1700 or user-defined */
		       "IEEE802.2 LLC", /* char * pProtoName, string name of service */
		       NULL, /* void * pNetCallBackId, returned to svc sublayer during recv */
		       NULL, /* void * pNetSvcInfo, ref to netSrvInfo structure */
		       NULL /* void * pNetDrvInfo ref to netDrvInfo structure */
		       );
    if (muxTkTestCookie==NULL) {
	perror("Failed to bind service:");
	return -1;
    }

    if (muxTkTestDebug > 0) printf("Attached muxTkTest interface to %s unit %d\n",
			     device, unit);
    return 0;
}
/**
 * Detach from the device
 */
int muxTkTestDetach(void)
{
    if (muxTkTestCookie == NULL) return -1;  /* Not attached */
    muxUnbind(muxTkTestCookie, MUX_PROTO_SNARF, muxTkTestRcvRtn);
    return 0;
}

