/* NMC_COMM_SUBS_1.C */

/*******************************************************************************
*
* This module contains base-level routines which talk to networked modules. See
* specification 00-1184 for the networked module (AIM) communications
* specification.
*
* Two basic data structures are used by the NMC family of routines:
*
*       1. NMC_MODULE_INFO is an array of NMC_MODULE_INFO_STRUCT stored in
*          global memory. As modules are
*          seen (via module status messages), an entry in the database is
*          setup with the information for that module. The entries are
*          assigned in strict ascending order; once entered, the entry for a
*          module is never released.
*
*       2. NMC_COMM_INFO is an array of NMC_COMM_INFO_STRUCT stored in global
*          memory. It stores the necessary information about
*          communicating over the various network communications devices with
*          which we may talk to modules. There is one entry for every network
*          interface by which we may communicate with AIM modules. This allows
*          for multiple Ethernets and potentially other networks (FDDI, token
*          ring) in the future. The device dependent code presently only
*          supports Ethernet (as do all present AIM modules).
*
*          The present code should work with multiple Ethernets in the same
*          crate, each with its own controller. However, routine nmc_get_niaddr
*          needs to be changed to support this, since it presently only returns
*          the Ethernet hardware address Ethernet controller on the CPU board.
*          Each module entry in NMC_MODULE_INFO contains a pointer to the
*          NMC_COMM_INFO entry for th network interface over which we communicate
*          with that module.
*
*
********************************************************************************
*
* Revision History:
*
*   19-Dec-1993    MLR   Converted from Nuclear Data code
*   18-April-1995  nda   added aimDebug to check before doing printf's
*   25-Sept-1996   tmm   force response and status SNAPs to be different
*   17-Nov-1997    mlr   minor cleanup to eliminate compiler warnings
*   17-Jul-2000    mlr   Added code to allow use with new SENS network stack in
*                        vxWorks
*   03-Sept-2000   mlr   Major rewrite to change interlock macro calls, changes to 
*                        use response queue per module.  Allows messages to be sent
*                        to more than 1 module, wait for responses in parallel.
*   05-Sept-2000   mlr   Improved debugging, added debugging levels
*   25-Sept-2000   mlr   Changed priority of nmcMessages and nmcInquiry tasks.
*   28-Nov-2000    mlr   Removed calls to take and release global interlock
*                        from nmc_findmod_by_addr.  It was causing vxWorks
*                        network to hang.
*   13-Feb-2001    mlr   Documented 28-Nov change, which was provisional until
*                        proven to work.
*   27-Aug-2001    mlr   Removed test for valid network in nmcEtherGrab to work
*                        around a bug in Tornado 2.02
*   01-Feb-2003    jee   Major changes for  Release 3.14.; Semaphores, Time and Tasks
*                        use the EPICS osi-Api. Replaced msgQ with EpicsRingBuffer.
*                        Linux port
*   25-May-2003    mlr   Changed from EpicsRingBuffer and events (which did not work
*                        reliably with multiple AIMS on the subnet) to 
*                        EpicsMessageQueue, which is new to 3.14.2.
*******************************************************************************/

#include "nmc_sys_defs.h"

#ifdef vxWorks
#include <vme.h>
#include <sysLib.h>
#include <taskLib.h>
#include <hostLib.h>
#include <usrLib.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct nmc_module_info_struct *nmc_module_info; /* Keeps info on modules */
struct nmc_comm_info_struct *nmc_comm_info;     /* Keeps comm info */
char sys_node_name[9] = {"        "};           /* System node name */
static int nmc_event_hdl(struct event_packet *epkt);
volatile int aimDebug = 0;
extern int errno;
extern char list_buffer_ready_array[2];   /* Is this needed ? */

epicsMutexId nmc_global_mutex = NULL;   /* Mutual exclusion semaphore to
                                           interlock global memory references */
ELLLIST nmc_sem_list;                      /* Linked list of event semaphores */

/*******************************************************************************
*
* NMC_INITIALIZE initializes the subroutine package to operate on a given
* network I/O device.
*
* The calling format is:
*
*       status=NMC_INITIALIZE(device)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "device" (string, by reference) is the device name.
*
* Operation
*   nmc_initialize starts a routine which looks at all incoming Ethernet
*   packets. Packets which are not from AIM modules are ignorred and are
*   passed on to be examined by other protocol routines. AIM "response" packets
*   are written to the response queue. Status and event packets are writtten to
*   the status queue.
*
*   nmc_initialize also creates two vxWorks tasks:
*
*   nmcStatusDispatch reads packets from the nmcStatusQ. It converts the byte
*   order of the packets and then calls the appropriate handler routine,
*   (status handler or event handler).
*
*   nmcStatusPoller periodically multicasts status requests to all modules on
*   the network. The responses are handled by the nmcStatusDispatch task.
*
*******************************************************************************/

int nmc_initialize(char *device)
{
   int s, id;
   int pid;
   struct nmc_comm_info_struct *i;
#ifdef vxWorks
#else
   char ebuf;
#endif
   /*
    * Allocate memory for the module and communications database structures
    *  if we have not done this already on a previous call
    */
   if (nmc_module_info == NULL) {
      nmc_module_info = (struct nmc_module_info_struct *)
         calloc(NMC_K_MAX_MODULES, sizeof(struct nmc_module_info_struct));

      nmc_comm_info = (struct nmc_comm_info_struct *)
         calloc(NMC_K_MAX_IDS, sizeof(struct nmc_comm_info_struct));

      /* Initialize the linked list for event semaphores */
      ellInit(&nmc_sem_list);
   }

   /*
    * If we have already been called for this device then return immediately
    */
   for (id=0; id<NMC_K_MAX_IDS; id++) {
      i = &nmc_comm_info[id];
      if ((*i).valid && (strcmp(device, (*i).name) == 0))
         return(OK);
   }

   /*
    * Find an unused id
    */
   for (id=0; id<NMC_K_MAX_IDS; id++) {
      i = &nmc_comm_info[id];
      if ((*i).valid == 0) goto found;
   }
   s = NMC__INVNETADDR;
   goto signal;

found:
   /*
    * Network type - only Ethernet supported for now. If others are added
    * we need to find a way to determine the network type here.
    */
   (*i).type = NMC_K_DTYPE_ETHERNET;

   /* Copy the device name of this network interface */
   strncpy((*i).name, device, sizeof((*i).name));

   /*
    * Call gethostname() to get sys_node_name.
    */
   if(sys_node_name[0] == ' ') {
      gethostname(sys_node_name, sizeof(sys_node_name));
      for(s=strlen(sys_node_name); s<sizeof(sys_node_name); s++)
         sys_node_name[s] = ' ';
   }

   /* Create a semaphore to interlock access to the global memory variables*/
   if (nmc_global_mutex == NULL) nmc_global_mutex = epicsMutexCreate(); 
	
   /*
    * Split up into the different cases for different network types
    */
   switch ((*i).type) {
      case NMC_K_DTYPE_ETHERNET:
         /*
          * Set up the timeout time (for response messages from modules), the
          * largest legal message size, and the number of communications
          * tries allowed.
          */
         (*i).timeout_time = 1000;        	 /* 1000 ms */
         (*i).max_msg_size = NMC_K_MAX_NIMSG; /* Maximum Ethernet message size */
         (*i).max_tries = 3;                  /* we try to send commands trice */

         /*
          * Use the "extended 802" protocol (SNAP), using as our ID
          * 00-00-AF-xx-yy, where xx-yy are this process's PID.
          * For status and event messages use the same ID but set the top bit.
          */

#ifdef vxWorks
         pid = taskIdSelf();
#else
         /*pid = epicsThreadGetIdSelf();*/
         pid = (int)getpid();
#endif
         AIM_DEBUG(1, "(nmc_initialize): task ID: 0x%8x\n", pid);

         (*i).response_sap = 0xAA;               /* Remember the SNAP SAP */
         (*i).response_snap[0] = 0;
         (*i).response_snap[1] = 0;
         (*i).response_snap[2] = 0xAF;   /* Nuclear Data company code*/
         memcpy(&(*i).response_snap[3], &pid, 2);  /* Copy the first word of the process ID */
         (*i).response_snap[4] &= 0x7f;  /* clear msbit so response and status differ */

         AIM_DEBUG(1, "(nmc_initialize): response SNAP: %2x %2x %2x %2x %2x\n",
            (*i).response_snap[0],(*i).response_snap[1],(*i).response_snap[2],
            (*i).response_snap[3],(*i).response_snap[4]);

         /*
          * The SNAP protocol ID for status and event messages
          * is the same as before except that the top bit of the last byte is set.
          */

         (*i).status_sap = 0xAA;         /* Remember the SNAP SAP */
         COPY_SNAP((*i).response_snap, (*i).status_snap);
         (*i).status_snap[4] |= 0x80;

         AIM_DEBUG(1, "(nmc_initialize): status SNAP: %2x %2x %2x %2x %2x\n",
            (*i).status_snap[0],(*i).status_snap[1],(*i).status_snap[2],
            (*i).status_snap[3],(*i).status_snap[4]);

         /* Create the message queue for status packets */
      	if (((i->statusQ = epicsMessageQueueCreate(MAX_STATUS_Q_MESSAGES, 
                                                   MAX_STATUS_Q_MSG_SIZE))) == 0) {
            s = errno;
            goto signal;
         }	 

         /* Define the size of the device dependent header */
         (*i).header_size = sizeof(struct enet_header);

#ifdef vxWorks
          
         /* Get our Ethernet address */
         if((s=nmc_get_niaddr(device,(*i).sys_address)) == ERROR) goto signal;
         /* Get a pointer to the ifnet structure for this device */
         if ((i->pIf= ifunit(device)) == NULL) 
				errlogPrintf("nmc_initialize: unknown ethernet device\n");
#else
	  
         if ((i->pIf=malloc(sizeof(struct ifnet))) == NULL) {
     	      printf("\nunable to alloc memory for ifnet");
         }
         if ((i->pIf->if_name=strdup(device)) == NULL) {
	  	      printf("\nunable to alloc memory for interface name");
         }
         if ((i->pIf->netlnk = libnet_open_link_interface(device, &ebuf)) == NULL) {
	  	      printf("\nunable to open ethernet interface; error: %d",ebuf);
         }
         if ((i->pIf->hw_address = libnet_get_hwaddr( i->pIf->netlnk, device, &ebuf)) == NULL) {
	  	      printf("\nunable to detemine MAC-Address; error: %d",ebuf);
         }
         AIM_DEBUG(1, "(nmc_initialize): MAC=%2x:%2x:%2x:%2x:%2x:%2x\n", 
	     	      	 	i->pIf->hw_address->ether_addr_octet[0],
	     	      	 	i->pIf->hw_address->ether_addr_octet[1],
	     	      	 	i->pIf->hw_address->ether_addr_octet[2],
	     	      	 	i->pIf->hw_address->ether_addr_octet[3],
	     	      	 	i->pIf->hw_address->ether_addr_octet[4],
	     	      	 	i->pIf->hw_address->ether_addr_octet[5]);
         COPY_ENET_ADDR(i->pIf->hw_address->ether_addr_octet, i->sys_address);	  
#endif
         AIM_DEBUG(1, "(nmc_initialize): pIf=%p\n", (*i).pIf);

         (*i).valid = 1;
         /*
          * Start the task which reads messages from the status message queue and calls
          * the status and event handler routines
          */
      	 i->status_pid = epicsThreadCreate("nmcMessages", epicsThreadPriorityHigh, 
               10000, (EPICSTHREADFUNC)nmcStatusDispatch, (void*) i);

         /*
          * Start the routine which intercepts incoming Ethernet messages and
          * writes them to the message queues
          * This function is called differently with and without the SENS stack
          */
#ifdef vxWorks
         /* this is sens */
         etherInputHookAdd(nmcEtherGrab,(*i).pIf->if_name,(*i).pIf->if_unit);

#else
      	 /* start a thread to check incoming ethernet packets */
      	 i->capture_pid = epicsThreadCreate("nmcEthCap", epicsThreadPriorityHigh, 
            10000, (EPICSTHREADFUNC)nmcEthCapture, (void*) i);
#endif

         /*
          *  Start the task which periodically multicasts inquiry messages
          */
      	 i->broadcast_pid = epicsThreadCreate("nmcInquiry", epicsThreadPriorityMedium, 10000, 
		 	      	 	   (EPICSTHREADFUNC)nmc_broadcast_inq_task, (void*) i);

         /*
          * Wait for 0.1 seconds for the first multicast inquiry to go out and for
          * responses to come back. This will allow time for the module database to be
          * built
          */
         epicsThreadSleep(0.1);
         AIM_DEBUG(1, "(nmc_initialize): returning\n");
         return OK;
   }

   /*
    * We should never get here (we fell through the CASE)
    */

   s = NMC__INVNETYPE;                     /* Fall thru to SIGNAL */

   /*
    * Signal errors
    */

signal:
   nmc_signal("nmc_initialize",s);
   return ERROR;

}

/*****************************************************************************
* NMC_CLEANUP()
*
* Does the opposite of nmc_initialize. Deletes the tasks and frees the
* resources allocated by nmc_initialize.
* This routine can be used to clean things up prior to downloading a new
* version of the routines when debugging. It can also turn off the AIM
* related software when it is not needed.
*/
void nmc_cleanup()
{
   int net;
   struct nmc_comm_info_struct *i = NULL;
   struct nmc_module_info_struct *p;
   int module;

#ifdef vxWorks
   etherInputHookDelete();
#else
    /* FIXME,  */
    /* missing:
       delete capture thread
       free if_name
	    free if_struct
	 */
	 if (libnet_close_link_interface(i->pIf->netlnk )< 0) {
	  	 printf("\n error closing link interface");
	 }
#endif
/* FIXME
We should shut down Status and Broadcast thread first */

   for (net=0; net < NMC_K_MAX_IDS; net++) {
      i = &nmc_comm_info[net];
      if (i->valid) {
         if (i->statusQ != NULL) {
            epicsMessageQueueDestroy(i->statusQ);
			   i->statusQ = NULL;
         }
      }
   }
   if (nmc_module_info != NULL) {
      for (module=0; module<NMC_K_MAX_MODULES; module++) {
         p = &nmc_module_info[module];
         if (!(*p).valid) break;
         if (p->responseQ != NULL) {
            epicsMessageQueueDestroy(p->responseQ);
            p->responseQ = NULL;
         }
         if ((*p).module_mutex != NULL) {
            epicsMutexDestroy((*p).module_mutex);
            (*p).module_mutex =NULL;
         }
      }
   }
   if (nmc_comm_info != NULL) free(nmc_comm_info);
   if (nmc_module_info != NULL) free(nmc_module_info);
   nmc_comm_info = NULL;
   nmc_module_info = NULL;
   ellFree(&nmc_sem_list);
   if (nmc_global_mutex != NULL) {
     epicsMutexDestroy(nmc_global_mutex);
     nmc_global_mutex=NULL;
   }
}

#ifdef vxWorks
#else /* not VxWorks */
void nmcEthCapture(struct nmc_comm_info_struct *i)
{
   char errbuf[PCAP_ERRBUF_SIZE];
   char *dev;
   pcap_t* descr;
   struct bpf_program bpfprog;      /* hold compiled program     */
   bpf_u_int32 netp =0;           /* ip                        */
   char *bpfstr="ether[6]=0 and ether[7]=0 and ether[8]=0xaf"; /* first 3 bytes of source address */
 
   if (i->pIf == NULL) {
	 printf("nmcEthCapture: Invalid if_net structure\n");
	 return;
   }
   AIM_DEBUG(5, "(nmcEtherCapture): EthCapture started \n");

   dev=i->pIf->if_name;
   errbuf[0]='\0';
   descr = pcap_open_live(dev, NMC_K_CAPTURESIZE,0,-1,errbuf);
   if (errbuf[0]) {
	 printf("nmcEthCapture: pcap_open_live: %s\n",errbuf);	   
   }
   if (descr == NULL) return;
	
   if(pcap_compile(descr,&bpfprog,bpfstr,0,netp) == -1){ 
	  printf("nmcEthCapture: pcap_compile: %s \n",pcap_geterr(descr)); 
	  return;
   }
   if(pcap_setfilter(descr,&bpfprog) == -1){ 
	  printf("nmcEthCapture: pcap_setfilter: %s \n",pcap_geterr(descr)); 
	  return;
   }

    /* ... and loop forever */ 
    pcap_loop(descr, -1, (pcap_handler) nmcEtherGrab,(unsigned char*)i);

    /* we should never reach this */
	printf("nmcEthCapture: pcap_loop: %s\n",pcap_geterr(descr));
	
	return;
}
#endif  
/******************************************************************************
* nmcEtherGrab()
*
* nmcEtherGrab looks at all Ethernet packets.
* AIM response packets are written to the nmcResponseQ
* AIM status and event packets are written to the nmcStatusQ
* All other packets are ignored.
*
*******************************************************************************/
#ifdef vxWorks
BOOL nmcEtherGrab(struct ifnet *pIf, char *buffer, int length)
{
   struct enet_header *h;
   struct nmc_comm_info_struct *net;
   int module;

   h = (struct enet_header *) buffer;
   net = &nmc_comm_info[0];
   /*
    * If this packet is not from an AIM module return immediately.
    * This test is based upon the first 3 bytes of the source address being
    * 00 00 AF, which is the Nuclear Data (Canberra) company code.
    */
   if ( (*h).source[0] != 0 ||
        (*h).source[1] != 0 ||
        (*h).source[2] != 0xAF ) {
      AIM_DEBUG(10, "(nmcEtherGrab): Got non-AIM packet, source=%d %d %d\n",
         (*h).source[0], (*h).source[1], (*h).source[2]);
      return FALSE;
   }
#else
pcap_handler nmcEtherGrab(unsigned char* usrdata, const struct pcap_pkthdr* pkthdr, const unsigned char *buffer)
{
   struct enet_header *h;
   struct nmc_comm_info_struct *net;
   int  length, module;

   h = (struct enet_header *) buffer;
   net = (struct nmc_comm_info_struct *) usrdata;
   length = pkthdr->caplen;
#endif

   AIM_DEBUG(5, "(nmcEtherGrab): got a packet from AIM\n");


   /* If the packet has the statusSNAP ID then write the message to the statusQ */
   if (COMPARE_SNAP((*h).snap_id, (*net).status_snap)) {
      if (epicsMessageQueueSend(net->statusQ, h, length) == -1) {
         nmc_signal("nmcEtherGrab: Status Queue full",0);
	 return FALSE;  
      }
   }
   /* If the packet has the responseSNAP ID then write the message to the responseQ for this module */
   else if (COMPARE_SNAP((*h).snap_id, (*net).response_snap)) {
      AIM_DEBUG(5, "(nmcEtherGrab): ...response packet\n");
      if (nmc_findmod_by_addr(&module, (*h).source) == OK) {
      	 AIM_DEBUG(5, "(nmcEtherGrab): sending %d bytes to module %d (%d)\n",
                   length, module,nmc_module_info[module].responseQ);
      	 if (epicsMessageQueueSend(nmc_module_info[module].responseQ, h, length) == -1) {
            nmc_signal("nmcEtherGrab: Message Queue of module full",module);
	    return FALSE;
	 }
      }
	  else { 
	  	  nmc_signal("nmcEtherGrab: Can't find module",module);
         return FALSE;
      }
   } 
   else {
      AIM_DEBUG(1, "(nmcEtherGrab): ...unrecognized SNAP ID\n");
      nmc_signal("nmcEtherGrab: unrecognized SNAP ID",NMC__INVMODRESP);
      return FALSE;
   }
   return TRUE;
}

/*******************************************************************************
*
* nmcStatusDispatch runs as a separate task. It reads messages from the
* nmcStatusQ and calls the status or event handlers as appropriate.
*
* It does not need to interlock access to global variables becuase it doesn't
* access any.
*
*******************************************************************************/

int nmcStatusDispatch(struct nmc_comm_info_struct *i)
{
   int s, len;
   struct status_packet pkt;   /* This assumes a staus_packet is larger */
   struct status_packet *spkt; /*  than an event packet, which is true */
   struct event_packet *epkt;
   struct ncp_comm_header *p;

   while (1) {
      len = epicsMessageQueueReceive(i->statusQ, &pkt, sizeof(pkt));
      if (len < 0)
         s=ERROR;
      else
	  s=OK;
      if (s==ERROR) {
         nmc_signal("nmcStatusDispatch:1",errno);
      }

      /* Swap byte order */
      nmc_byte_order_in(&pkt);
      p = &pkt.ncp_comm_header;
      if ((*p).checkword != NCP_K_CHECKWORD ||
          (*p).protocol_type != NCP_C_PRTYPE_NAM)
         nmc_signal("nmcStatusDispatch:2",NMC__INVMODRESP);

      /*
       * See what kind of message we've got and call the appropriate handler
       */
      spkt = (struct status_packet *) &pkt;
      epkt = (struct event_packet *) &pkt;

      if((*p).message_type == NCP_C_MSGTYPE_MSTATUS)
         nmc_status_hdl(i, spkt);

      else if((*p).message_type == NCP_C_MSGTYPE_MEVENT)
         nmc_event_hdl(epkt);

   }
   return OK;
}


/*******************************************************************************
*
* NMC_EVENT_HDL is called whenever an event message is received.
* It gives a semaphore if one has been requested from this event message.
*
* This routine is called from nmc_status_dispatch.
* It interlocks access to global variables.
*
*******************************************************************************/

static int nmc_event_hdl(struct event_packet *epkt)
{
   int module;
   struct ncp_comm_mevent *message;
   struct nmc_sem_node *p;

   AIM_DEBUG(8, "(nmc_event_hdl) received status (event) message \n");
  /* Figure out what module this event message is from */   
   if (nmc_findmod_by_addr(&module, (*epkt).enet_header.source) == ERROR)
      return ERROR;

   message = &(*epkt).ncp_comm_mevent;

   /*
    *  Scan the list of event semaphores and give the semaphore if there
    *  is a match.
    */
   GLOBAL_INTERLOCK_ON;
   p = (struct nmc_sem_node *) ellFirst(&nmc_sem_list);
   while (p != NULL) {
      if (((*p).module == module) &&
          ((*p).adc == (*message).event_id1) &&
          ((*p).event_type == (*message).event_type)) {
             epicsEventSignal((*p).evt_id);
             AIM_DEBUG(6, "(nmc_event_hdl) signaling event \n");
      }
      p = (struct nmc_sem_node *) ellNext( (ELLNODE *) p);
   }
   if ((*message).event_type == NCP_C_EVTYPE_BUFFER)
      list_buffer_ready_array[(*message).event_id2] = 1;

   GLOBAL_INTERLOCK_OFF;
   return OK;
}


/*******************************************************************************
*
* NMC_STATUS_HDL is called from NMC_STATUS_DISPATCH when a module status
* message is received.
* Its argument is the address of the status packet.
*
* It interlocks access to global variables.
*
*******************************************************************************/

int nmc_status_hdl(struct nmc_comm_info_struct *net, struct status_packet *pkt)
{
   int module,s=0;
   struct nmc_module_info_struct *p;
   struct enet_header *e;
   struct ncp_comm_mstatus *m;
   struct ncp_comm_header *h;

   AIM_DEBUG(8, "(nmc_status_hdl) received status (status) message \n");
   /*
    * First, see if we already know about this module
    */
   e = &(*pkt).enet_header;
   h = &(*pkt).ncp_comm_header;
   m = &(*pkt).ncp_comm_mstatus;
   if (nmc_findmod_by_addr(&module, (*pkt).enet_header.source) == ERROR) {
      /*
       * This is a new module. Allocate a module number for it and fill in
       * stuff in its info entry. Note that previously unused entries will
       * be all zeros.
       */

      GLOBAL_INTERLOCK_ON;
      if (nmc_allocmodnum(&module) == ERROR) {
         /* hope this never happens! */
         s = NMC__INVMODNUM;
         goto done;
      }
      p = &nmc_module_info[module];

      (*p).comm_device = net;
      COPY_ENET_ADDR((*pkt).enet_header.source, (*p).address);
      (*p).module_type = (*m).module_type;
      (*p).num_of_inputs = (*m).num_inputs;
      (*p).hw_revision = (*m).hw_revision;
      (*p).fw_revision = (*m).fw_revision;

      (*p).acq_mem_size = (*m).acq_memory;
      /* All acquisition memory is available for allocation */
      (*p).free_address = 0;
      (*p).current_message_number = 0;
      /* Create the message queue for response packets */
      	 if ((p->responseQ = epicsMessageQueueCreate(MAX_RESPONSE_Q_MESSAGES, 
                                                     MAX_RESPONSE_Q_MSG_SIZE)) == 0) {
         nmc_signal("Unable to create response Queue",0);
         goto done;
      }
      /* Create a semaphore to interlock access to this module */
      (*p).module_mutex = epicsMutexCreate();
      
      /* Allocate buffers for input and output packets */
      (*p).in_pkt = (struct response_packet *)
                        calloc(1, sizeof(struct response_packet));
      (*p).out_pkt = (struct response_packet *)
                        calloc(1, sizeof(struct response_packet));
      GLOBAL_INTERLOCK_OFF;
   }

   /* Call the module ownership change handler */
   GLOBAL_INTERLOCK_ON;
   s=nmc_owner_hdl(module, h);

done:
   GLOBAL_INTERLOCK_OFF;
   if (s == OK)
      return OK;
   else {
      nmc_signal("nmc_status_hdl",s);
      return ERROR;
   }
}

/*****************************************************************************
* nmc_owner_hdl(module, *ncp_comm_header)
*
* This routine maintains the ownership status of all modules. It is called
* from nmc_status_hdl (for status packets) and from nmc_getmsg (for response
* packets).
*
* Interlocks for global variables are already on when this routine is called.
*
******************************************************************************/
int nmc_owner_hdl(int module, struct ncp_comm_header *h)
{
   int s;
   struct nmc_module_info_struct *p;
   struct nmc_comm_info_struct *net;

   p = &nmc_module_info[module];
   (*p).inqmsg_counter = 0;
   (*p).module_comm_state = NMC_K_MCS_REACHABLE;
   net = (*p).comm_device;

   COPY_ENET_ADDR((*h).owner_id, (*p).owner_id);
   for (s=0; s < sizeof((*h).owner_name); s++)
      (*p).owner_name[s] = (*h).owner_name[s];

   if ((*p).owner_id[0] == 0 &&             /* is the module unowned? */
       (*p).owner_id[1] == 0 &&
       (*p).owner_id[2] == 0 &&
       (*p).owner_id[3] == 0 &&
       (*p).owner_id[4] == 0 &&
       (*p).owner_id[5] == 0)
        (*p).module_ownership_state = NMC_K_MOS_UNOWNED;
   /* is the module owned by us? */
   else if (COMPARE_ENET_ADDR((*p).owner_id, (*net).sys_address))
      (*p).module_ownership_state = NMC_K_MOS_OWNEDBYUS;
   else
      (*p).module_ownership_state = NMC_K_MOS_NOTBYUS;

   return OK;
}


/*******************************************************************************
*
* NMC_GETMSG receives a message from a module.
*
* Its calling format is:
*
*       status=NMC_GETMSG(module,buffer,buffer size,message size)
*
* where
*
*  "status" is the status of the operation.  All errors are signalled.
*
*  "module" (longword) is the module number.
*
*  "buffer" (array of char) is where the received message should be put.
*
*  "buffer size" (longword) is the size of "buffer".
*
*  "message size" (returned longword, by reference) is the size of the received
*   message.
*
* This routine is called from nmc_sendcmd.
*
* Interlocks for global variables are already on when this routine is called.
*
*******************************************************************************/
int nmc_getmsg(int module, struct response_packet *pkt, int size, int *actual)
{
   int  s=0, len;
   struct nmc_comm_info_struct *i;
   struct enet_header *e;
   struct ncp_comm_header *p;
   struct nmc_module_info_struct *m;

   /* The module is known to be valid and reachable - checked in nmc_sendcmd */
   m = &nmc_module_info[module];
   i = m->comm_device;

   /*
    * Split up into the different code for different network types
    */

   switch ((*i).type) {
      case NMC_K_DTYPE_ETHERNET:

      /*
       * ETHERNET: Read a message from the response queue with timeout
       */
read:
      /* The timeout_time is in milliseconds, convert to seconds */
      len = epicsMessageQueueReceiveWithTimeout(m->responseQ, pkt, sizeof(*pkt), (double)(i->timeout_time/1000.));
         AIM_DEBUG(6, "(nmc_getmsg): message length:%d (%d)\n", len,m->responseQ);
      if (len < 0) {
         AIM_DEBUG(1, "(nmc_getmsg): timeout while waiting for message\n");
         s = errno;
         goto signal;
      }

      /*
       * Make sure the message came from the right module:
       *  if not, throw it away and try again
       */
      e = &(*pkt).enet_header;
      p = &(*pkt).ncp_comm_header;
      if (!COMPARE_ENET_ADDR( (*e).source, m->address)) {
         AIM_DEBUG(1, "(nmc_getmsg): message from wrong module!\n");
         AIM_DEBUG(1, "              actual: %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n",
            e->source[0], e->source[1], e->source[2], e->source[3], e->source[4], 
            e->source[5]);
         AIM_DEBUG(1, "           should be: %2.2x%2.2x%2.2x%2.2x%2.2x%2.2x\n", 
            m->address[0], m->address[1], m->address[2], m->address[3], m->address[4], 
            m->address[5]);
         goto read;
      }

      /*
       * Zero out the "unanswered message" counter
       */
      nmc_module_info[module].inqmsg_counter = 0;

      /*
       * If the module's owner is different from that in the database call
       * nmc_owner_hdl. Note that we have not yet swapped the byte order in
       * the packet (that is done in nmc_sendcmd on return), but the
       * owner handler does not use any fields in the packet which need
       * to have byte order swapped.
       */

      if (!COMPARE_ENET_ADDR((*p).owner_id, nmc_module_info[module].owner_id))
         nmc_owner_hdl(module, p);

      /*
       * Return the received message size to the caller
       */
      *actual = len;
      return OK;
   }

   /*
    * We should never get here (we fell through the CASE)
    */

   s = NMC__INVNETYPE;                     /* Fall thru to SIGNAL */
   AIM_DEBUG(1, "(nmc_getmsg): wrong network type !\n");

   /*
    * Signal errors
    */

signal: 
   nmc_signal("nmc_getmsg",s);
   return ERROR;
}

/*******************************************************************************
*
* NMC_FLUSH_INPUT gets rid of any queued messages in the response queue.
*
* Its calling format is:
*
*       status=NMC_FLUSH_INPUT(module)
*
* where
*
*  "status" is the status of the operation. Any errors are NOT signaled.
*
*  "module" (longword) is the module number.
*
* This routine is called from nmc_sendcmd.
* Interlocks for global variables are already on when this routine is called.
*
*******************************************************************************/

int nmc_flush_input(int module)
{
   int s, len;
   struct nmc_comm_info_struct *i;
   char temp[MAX_RESPONSE_Q_MSG_SIZE];

   /* The module is known to be valid and reachable - checked in nmc_sendcmd */
   i = nmc_module_info[module].comm_device;

   /*
    * Split up into the different code for different network types
    */

   switch ((*i).type) {
      case NMC_K_DTYPE_ETHERNET:

      /*
       * ETHERNET: Read messages from the queue until none remain.
       */
      do len = epicsMessageQueueReceiveWithTimeout(nmc_module_info[module].responseQ, temp, sizeof(temp), 0.);
      while (len != ERROR);
      return OK;
   }

   /*
    * We should never get here (we fell through the CASE)
    */
   s = NMC__INVNETYPE;

   nmc_signal("nmc_flush_input",s);
   return ERROR;
}

/*******************************************************************************
*
* NMC_PUTMSG sends a message to a module.
*
* Its calling format is:
*
*       status=NMC_PUTMSG(module,buffer,buffer size)
*
* where
*
*  "status" is the status of the operation. Any errors are signaled.
*
*  "module" (longword) is the module ID number.
*
*  "buffer" (array of char) is the message to be sent.
*
*  "buffer size" (longword) is the size of "buffer".
*
* This routine is called from nmc_sendcmd.
* Interlocks for global variables are already on when this routine is called.
*
*******************************************************************************/

int nmc_putmsg(int module, struct response_packet *pkt, int size)
{

   int s,length;
   struct nmc_comm_info_struct *i;
   struct ether_header ether_header;
   struct enet_header *e;
#ifdef vxWorks
#else
   unsigned char *packet;
#endif

   /* The module is known to be valid and reachable - checked in nmc_sendcmd */
   i = nmc_module_info[module].comm_device;

   /*
    * Split up for the different network types
    */

   switch ((*i).type) {
      case NMC_K_DTYPE_ETHERNET:

      /*
       * Ethernet: Just send the message.
       */
      COPY_ENET_ADDR(nmc_module_info[module].address, ether_header.ether_dhost);
      /* The length of the data part of the packet */
      length = size + sizeof(struct enet_header) - 14;
      ether_header.ether_type = length;
      e = &(*pkt).enet_header;
      (*e).dsap = (*i).response_sap;
      (*e).ssap = (*i).response_sap;
      (*e).control = 0x03;
      COPY_SNAP((*i).response_snap, (*e).snap_id);

      /* Send the packet */
#ifdef vxWorks
      etherOutput((*i).pIf, &ether_header, (char *) &(*e).dsap, length);
#else
      COPY_ENET_ADDR(i->pIf->hw_address->ether_addr_octet, ether_header.ether_shost);

	  if (libnet_init_packet(length+14, &packet) < 0) {
	  	 printf("\n error allocating packet memory");
	  }
	  if ( libnet_build_ethernet(ether_header.ether_dhost, ether_header.ether_shost,
	     	      ether_header.ether_type, &(e->dsap), length, packet)< 0) {
	  	 printf("\n error building ethernet packet");
	  }
	  if ( (s=libnet_write_link_layer(i->pIf->netlnk, i->pIf->if_name, packet, length+14))< 0) {
	  	 printf("\n error writing ethernet packet");
	  }
      AIM_DEBUG(1, "(nmc_putmsg): wrote %d bytes of %d\n",s,length+14);
	  libnet_destroy_packet(&packet);

#endif
     return OK;
   }
   /*
    * We should never get here (we fell thru the CASE)
    */

   s = NMC__INVNETYPE;                     /* Fall thru to SIGNAL */

   /*
    * Signal errors
    */

   nmc_signal("nmc_putmsg",s);
   return ERROR;

}

/*******************************************************************************
*
* NMC_SENDCMD sends a command message to a module and gets the response.
*
* The calling format is:
*
*       status=NMC_SENDCMD(module,command code,packet data,data size,response buffer,
*                       response buffer size,actual response size,not owned flag)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*   If an error occurs status = ERROR. If the operation was sucessful then
*   status = the module response packet code. (e.g. NCP_K_MRESP_xxxxx)
*
*  "module" (longword) is the number of the module.
*
*  "command code" (longword) is the host command code.
*
*  "packet data" (array) is the data to be put in the data area of the packet.
*   These are the arguments of the host command.
*
*  "data size" (longword) is the size of the packet data array.
*   The maximum size allowed is currently 1457 bytes.
*
*  "response buffer" (returned array) is the buffer where the module response
*   data is put. The module response code is returned as the status of the
*   routine. Any errors from the module have been signaled.
*
*  "response buffer size" (longword) is the size in bytes of
*   "response buffer".
*
*  "actual response size" (returned longword, by reference) is the actual size
*   of the module response data.
*
*  "not owned flag" (longword) is a flag which indicates that is
*   is legal to send the command to a module which is not owned by us.
*
* This routine is called by application programs.
*
* It interlocks access to global variables.
*
*******************************************************************************/

int nmc_sendcmd(int module, int command, void *data, int dsize, void *response,
                int rsize, int *size, int oflag)
{
   int s,tries,rmsgsize,cmdsize;
   struct ncp_comm_header *h;
   struct ncp_comm_packet *p=NULL;
   unsigned char *d;
   struct nmc_comm_info_struct *i;
   struct nmc_module_info_struct *m;
   /* Note, this code is specific to Ethernet. It will need
    * a little work if other networks are ever supported */

   AIM_DEBUG(8, "(nmc_sendcmd): enter\n");

   MODULE_INTERLOCK_ON(module);

   m = &nmc_module_info[module];
   /*
    * Make sure the module number is reasonable, and that the data size isn't too
    * big.
    */

   if (nmc_check_module(module, &s, &i) != NMC_K_MCS_REACHABLE) {
         AIM_DEBUG(1, "(nmc_sendcmd): module not reachable on entry\n");
         goto done;
   }
   if(dsize > ((*i).max_msg_size - sizeof(*h) - sizeof(*p))) {
      AIM_DEBUG(1, "(nmc_sendcmd): message too big\n");
      s=NMC__MSGTOOBIG;
      goto done;
   }

   /*
    * If the module is not owned by us, return an error unless OFLAG is true.
    */

   if(m->module_ownership_state
            != NMC_K_MOS_OWNEDBYUS && !oflag) {
      AIM_DEBUG(1, "(nmc_sendcmd): module not owned by us\n");
      s=NMC__UNOWNEDMOD; 
      goto done;
   }
      

   /*
    * Build the protocol and command packet headers, and copy the command arguments
    * into the packet data area.
    */
   h = &m->out_pkt->ncp_comm_header;
   p = &m->out_pkt->ncp_comm_packet;
   d = &m->out_pkt->ncp_packet_data[0];

   s = sizeof(*h) + sizeof(*p);
   memset(h, 0, s);
   (*h).checkword = NCP_K_CHECKWORD;
   (*h).protocol_type = NCP_C_PRTYPE_NAM;
   (*h).message_type = NCP_C_MSGTYPE_PACKET;
   /* advance the current message number */
   m->current_message_number++;       
   (*h).message_number = m->current_message_number;
   (*h).data_size = sizeof(*p) + dsize;
   cmdsize = sizeof(*h) + (*h).data_size;
   (*p).packet_size = dsize;
   (*p).packet_type = NCP_C_PTYPE_HCOMMAND;
   (*p).packet_code = command;

   memcpy(d, data, dsize);
   /*Swap byte order */
   nmc_byte_order_out(m->out_pkt);
   
   nmc_flush_input(module);        /* make sure there are no queued messages */

   /*
    * Send the command - try (*i).max_tries times to get a valid response
    */
   for (tries=0; tries < (*i).max_tries; tries++) {
      if ((s=nmc_putmsg(module, m->out_pkt, cmdsize)) == ERROR) goto done;

      /*
       * Receive the module's response message. Retry if there was an error.
       */
      if (nmc_getmsg(module,m->in_pkt,sizeof(*(m->in_pkt)),&rmsgsize) == ERROR) 
         goto retry;
      m->module_comm_state = NMC_K_MCS_REACHABLE;
      /* Swap byte order */
      nmc_byte_order_in(m->in_pkt);

      /*
       * If the message came from the right module, make sure it's
       * basically a valid message.
       */

      h = &m->in_pkt->ncp_comm_header;
      p = &m->in_pkt->ncp_comm_packet;
      *size = (*p).packet_size;
      if((*h).message_number != m->current_message_number ||
         (*h).checkword != NCP_K_CHECKWORD ||
         (*h).protocol_type != NCP_C_PRTYPE_NAM ||
         (*p).packet_type != NCP_C_PTYPE_MRESPONSE) {
         AIM_DEBUG(1, "(nmc_sendcmd): message_number=%d, current=%d\n", 
            (*h).message_number, m->current_message_number);
         goto retry;
      }

      /*
       * Pick out the module response and return the data. If the
       * module returned a false status, signal it.
       * LATER: add a check for NMC__APUTIMEOUT; retry if this is the module
       * status.
       */

      if(*size > rsize) *size = rsize;
      if(*size != 0) memcpy(response, m->in_pkt->ncp_packet_data, *size);
      /* Success ! */
      s = OK;
      goto done;

retry:
      AIM_DEBUG(1, "(nmc_sendcmd): tries=%d, max_tries=%d, timeout_time=%dms\n", 
             tries, i->max_tries, i->timeout_time);
      s = NMC__INVMODRESP;
   }

   /* If we get here then the module failed to respond after max_tries tries. */
   m->module_comm_state = NMC_K_MCS_UNREACHABLE;
   s = NMC__MODNOTREACHABLE;
   AIM_DEBUG(1, "(nmc_sendcmd): module %d is unreachable\n", module);

done:
   MODULE_INTERLOCK_OFF(module);
   if (s == OK)
      /* Return the module packet code */
      return (*p).packet_code;
   else {
      nmc_signal("nmc_sendcmd",s);      
      return ERROR;
   }
}

/*******************************************************************************
*
* NMC_GET_NIADDR returns the Ethernet network address of this system.
*
* The calling format is:
*
*       status=NMC_GET_NIADDR(channel,address)
*
* where
*
*  "status" is the status of the operation. Errors have NOT been signaled.
*
*  "device" (character buffer) is the name of this network interface
*
*  "address" (returned 6 byte array) is the Ethernet address of the interface.
*
* This routine is called from nmc_initialize.
*
*******************************************************************************/
#ifdef vxWorks
int nmc_get_niaddr(char *device, unsigned char *address)
{   
   struct ifnet *ifPtr;
   ifPtr = ifunit(device);
   memcpy(address, ((struct arpcom *) ifPtr)->ac_enaddr, 6);
  return OK;
}
#endif

/*******************************************************************************
*
* NMC_FINDMOD_BY_ADDR looks through the module database to find the module with
* the specified address.
*
* The calling sequence is:
*
*       status=NMC_FINDMOD_BY_ADDR(module,address)
*
* where
*
*  "status" is the status of the operation. Errors have NOT been signaled.
*
*  "module" (returned longword, by reference) is the module number found by the
*   search.
*
*  "address" (6 byte array) is the address of the module to find.
*
* This routine is called from application programs.
* It interlocks access to global variables.
*
*******************************************************************************/

int nmc_findmod_by_addr(int *module, unsigned char *address)
{

   struct nmc_module_info_struct *p;
   int s;
   
   AIM_DEBUG(8, "nmc_findmod_by_addr enter\n");
   /*
    * Make sure there is a module database to make sure we don't embarass
    * ourselves (in particular, AICONTROL, which may call this routine without
    * having INITIALIZEd).
    */

   if(nmc_module_info == NULL) return NMC__NOSUCHMODULE;

   /* GLOBAL_INTERLOCK_ON; */
   /* Above call commented out, since this function is called from 
    * nmcEtherGrab, which is the callback registered with etherInputHookAdd.
    * It is not clear if the callback is called at interrupt level (in which
    * case semTake(), done by GLOBAL_INTERLOCK_ON, is definitely illegal. In
    * any case, using GLOBAL_INTERLOCK_ON in this function causes the vxWorks
    * network to lock up from time to time.  We live with the low-risk chance
    * that a new AIM module was added to the database while this routine is
    * being called.
    */
    
   /*
    * Just look through the module database for the specified module
    */

   for (*module=0; *module < NMC_K_MAX_MODULES; (*module)++) {
      p = &nmc_module_info[*module];
      if(!(*p).valid) break;       /* terminate after the last valid module */
      if (COMPARE_ENET_ADDR(address, (*p).address)) {
         s = OK;
         goto done;
      }
   }

   s = ERROR;
done:
   /* GLOBAL_INTERLOCK_OFF; */
   /* Not needed, see comment under GLOBAL_INTERLOCK_ON */
   return s;       /* We don't signal errors, since sometimes we just
                    *  want to check and see if a module is currently
                    *  in the database.
                    */
}


/******************************************************************************
* NMC_CHECK_MODULE(module, err, net);
* Checks that "module" is a valid number. Returns the module comm_status field
* and an error indicator in "err" if the status is not NMC_K_MCS_REACHABLE
* Returns the network comm device pointer in net
*******************************************************************************/
int nmc_check_module(int module, int *err, struct nmc_comm_info_struct **net)
{
   int c;
   
   AIM_DEBUG(8, "nmc_check_module enter\n");
   if ((module < 0) || (module >= NMC_K_MAX_MODULES)) {
      *err = NMC__INVMODNUM;
      *net = NULL;
      return NMC_K_MCS_UNKNOWN;
   }
   c = nmc_module_info[module].module_comm_state;
   if (c == NMC_K_MCS_UNKNOWN)
      *err = NMC__UNKMODULE;
   else if (c != NMC_K_MCS_REACHABLE)
      *err = NMC__MODNOTREACHABLE;
   *net = nmc_module_info[module].comm_device;
   return c;
}


/******************************************************************************
* nmc_signal(from,error)
* This routine handles error reporting. For now it just prints an error number
* and returns. In the future it will need to use the logging facilities.
*/
int nmc_signal(char *from, int err)
{
   AIM_DEBUG(1, "nmc_signal called by '%s' with error %d = 0x%x\n", from, err, err);
   return ERROR;
}
