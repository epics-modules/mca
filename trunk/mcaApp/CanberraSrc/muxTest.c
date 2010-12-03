
/********************************************************
 * muxTest.c
 *
 * Test program for mux API
 *
 * Mark Rivers
 ********************************************************/


#include <vxWorks.h>
#include <muxLib.h>
#include <stdio.h>
#include <stdlib.h>

/* Function prototypes */
BOOL muxTestRcvRtn( void *netCallbackId, long type, M_BLK_ID pNetBuf,
		    LL_HDR_INFO *ll_hdr_info, void *pSpareData);
STATUS muxTestShutdownRtn(void *netCallbackId, void *extra);
STATUS muxTestRestartRtn( void *netCallbackId, void *extra);
void muxTestErrorRtn(END_OBJ *end_obj,
		    END_ERR *pError, void *extra);
int muxTestAttach(char *device, int unit);



/** Debug level. Set to non-zero for output of messages
 */
volatile int muxTestDebug=0;

/* Cookie used for detach */
void *muxTestCookie=NULL;

/**
 * Callback from the MUX to receive packets
 * netCallbackId the handle/ID installed at bind time
 * type network service type
 * pNetBuf network service datagram
 * pSpareData pointer to optional data from driver
 * TRUE if the packet was "consumed", and FALSE if the packet was ignored
 */
BOOL muxTestRcvRtn( void *netCallbackId, long type, M_BLK_ID pNetBuf,
		    LL_HDR_INFO *ll_hdr_info, void *pSpareData)
{
    if (muxTestDebug > 0) {
        printf("muxTestRcvRtn: received packet, passed type: %04lx \n", type); 
    }
    return FALSE;
}

/**
 * called by MUX in response to muxDevUnload()
 * @param netCallbackId the handle/ID installed at bind time
 *
 * Disconnect this module from the MUX stack
 */
STATUS muxTestShutdownRtn(void * netCallbackId, /* the handle/ID installed at bind time */
                          void *extra)
{
    return (muxUnbind(netCallbackId, MUX_PROTO_SNARF, muxTestRcvRtn));
}

/**
 * called by MUX to restart network services
 * @param netCallbackId the handle/ID installed at bind time
 *
 * We hold no connection-oriented information, so do nothing
 */
STATUS muxTestRestartRtn( void *netCallbackId, /* the handle/ID installed at bind time */
                          void *extra)
{
    return OK;
}

/**
 * called by MUX to handle error conditions encountered by network drivers
 * @param void *netCallbackId the handle/ID installed at bind time
 * @param END_ERR *pError an error structure, containing code and message
 */
void muxTestErrorRtn(END_OBJ *end_obj, /* the handle/ID installed at bind time */
		    END_ERR *pError, /* pointer to struct containing error */
                    void *extra)
{ 
    printf("Error in protocol layer: %d\n", pError->errCode);
    if (pError->pMesg) {
	printf(pError->pMesg);
    }
}

/**
 * initialise muxTest network service
 * device The device name of the interface to attach this protocol to
 * unit The unit number of the interface
 * return 1 if the device was correctly opened, 0 otherwise
 *
 * The service has to be bound as a "SNARF" protocol device, since the MUX
 * decides which protocol to call based on the ethernet type field, and for
 * LLC, this can be variable.
 */
int muxTestAttach(char *device, int unit)
{
    if (muxTestCookie != NULL) return -1;  /* Already attached */
    
    muxTestCookie= muxBind (device, /* char * pName, interface name, for example: ln, ei */
		     unit, /* int unit, unit number */
		     &muxTestRcvRtn, /* muxTestRcvRtn( ) Receive data from the MUX */
		     &muxTestShutdownRtn, /* muxTestShutdownRtn( ) Shut down the network service. */
		     &muxTestRestartRtn, /* muxTestRestartRtn( ) Restart a suspended network service.*/
		     &muxTestErrorRtn, /* muxTestErrorRtn( ) Receive an error notification from the MUX. */
		     MUX_PROTO_SNARF, /* long type, from RFC1700 or user-defined */
		     "IEEE802.2 LLC", /* char * pProtoName, string name of service */
		     NULL /* void * pNetCallBackId, returned to svc sublayer during recv */
		     );
    if (muxTestCookie==NULL) {
	perror("Failed to bind service:");
	return -1;
    }

    if (muxTestDebug > 0) printf("Attached muxTest interface to %s unit %d\n",
			     device, unit);
    return 0;
}
/**
 * Detach from the device
 */
int muxTestDetach(void)
{
    if (muxTestCookie == NULL) return -1;  /* Not attached */
    muxUnbind(muxTestCookie, MUX_PROTO_SNARF, muxTestRcvRtn);
    return 0;
}

