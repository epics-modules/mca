
/* NMC_COMM_SUBS_2.C */

/*******************************************************************************
*
* This module contains routines used to talk to networked modules.
* See NMC_COMM_SUBS_1.C.
*
********************************************************************************
*
* Revision History:
*
*    28-Dec-1993    MLR     Modified from Nuclear Data routine NMC_COMM_SUBS_3.C
*    17-Nov-1997    mlr     Minor modifications to eliminate compiler warnings
*    06-Dec-1999    mlr     Minor modifications to eliminate compiler warnings
*    06-May-2000    mlr     Removed memmove() calls because we now used packed
*                           structures.
*                           Changed nmc_build_enet_addr to be architecture
*                           independent.
*    03-Sept-2000   mlr     Changed interlocks to per-module.  Made a separate
*                           task for nmc_broadcast_inq, rather than vxWorks
*                           period().  This is more legible in vxWorks task
*                           list.  Improved debugging.
*    01-Feb-2003    jee     Changes for Release 3.14.and Linux port
*******************************************************************************/

#include "nmc_sys_defs.h"
#include <string.h>
#include <stdio.h>
#include <errlog.h>

extern struct nmc_module_info_struct *nmc_module_info;  /* Keeps info on modules */

extern struct nmc_comm_info_struct *nmc_comm_info;      /* Keeps comm info */

static unsigned char ni_broadcast_address[6] = {1,0,0xaf,0,0,0};
                                                /* Basic Ethernet multicast address */

extern epicsMutexId nmc_global_mutex;           /* Mutual exclusion semaphore
                                                   to interlock access to
                                                   global variables */

extern char sys_node_name[9];                   /* our system's node name */

/*******************************************************************************
*
* NMC_ALLOCMODNUM allocates a module number. It does this by looking through the
* module database for an unused entry.
*
* The calling format is:
*
*       status=NMC_ALLOCMODNUM(module)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (returned longword, by reference) is the allocated module number.
*
* This routine is called from nmc_status_hdl.
*
* Intelocks for global variables are already on when this routine is called.
*
*******************************************************************************/

int nmc_allocmodnum(int *module)
{

   struct nmc_module_info_struct *p;

   /*
    * Just look through the module info entries till we find one that's free
    */
      
   for (*module=0; *module < NMC_K_MAX_MODULES; (*module)++) {
      p = &nmc_module_info[*module];
      if(!(*p).valid) {
         (*p).valid = 1; 
         return OK;
      }
   }

   nmc_signal("nmc_allocmodnum",NMC__TOOMANYMODULES);
   return ERROR;

}

/*******************************************************************************
*
* NMC_BUYMODULE attempts to make our system the owner of a module. It simply
* sends a SETOWNER (or SENDOWNEROVER) command to the module. It returns a
* SUCCESS status if we already own the module. It returns an error if the
* module is already owned by someone else.
*
* The calling format is:
*
*       status=NMC_BUYMODULE(module,flag)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module ID number.
*
*  "flag" (longword) is TRUE if the SETOWNEROVER command is to be
*   sent; this overrides the current ownership status of the module.
*
* This routine is called from application programs.
* It interlocks access to the global variables.
*
*******************************************************************************/

int nmc_buymodule(int module, int override)
{

   int i,command,s;
   struct nmc_module_info_struct *p;
   struct nmc_comm_info_struct *netinfo;
   struct ncp_hcmd_setowner packet;

   /*      Interlock access to global variables */
   MODULE_INTERLOCK_ON(module);
   /*
    * Make sure the module number is reasonable
    */

   if(nmc_check_module(module, &s, &netinfo) != NMC_K_MCS_REACHABLE) {
      MODULE_INTERLOCK_OFF(module);
	  goto done;
   }
   p = &nmc_module_info[module];
   /*
    * Return success if we already own the module
    */

   if((*p).module_ownership_state == NMC_K_MOS_OWNEDBYUS) {
      s = OK; 
      MODULE_INTERLOCK_OFF(module);
      goto done;
   }

   /*
    * Return an error if someone else already owns the module
    */

   if((*p).module_ownership_state == NMC_K_MOS_NOTBYUS && !(override)) {
      s = NMC__OWNERNOTSET;
      MODULE_INTERLOCK_OFF(module);
      goto done;
   }

   /*
    * Build the command packet
    */

   COPY_ENET_ADDR((*netinfo).sys_address, packet.owner_id);
   for (i=0; i < sizeof(packet.owner_name); i++) 
      packet.owner_name[i] = sys_node_name[i];

   /*
    * Send it.
    * Any actual change in the ownership of the module will be handled by the
    * ownership change handler, which is called by nmc_getmsg.
    */
   if (override) command = NCP_K_HCMD_SETOWNEROVER;
   else command = NCP_K_HCMD_SETOWNER;

   MODULE_INTERLOCK_OFF(module);  /* Must turn off before calling NMC_SENDCMD */
   
   s = nmc_sendcmd(module,command,&packet,sizeof(packet),&i,sizeof(i),&i,1);
   if (s == NCP_K_MRESP_SUCCESS) s = OK;

done:
   if (s  == OK)
      return OK;
   else {
      nmc_signal("nmc_buymodule",s);
      return ERROR;
   }
}

/*******************************************************************************
*
* NMC_ALLOCATE_MEMORY allocates memory in the AIM.  The routine returns to the
* caller the base address of the requested block of memory, or -1 if the memory
* cannot be allocated. The routine marks this memory as allocated.
*
*       status=NMC_ALOOCATE_MEMORY(module, size, base_address)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*   "module" (longword) is the module ID number.
*
*   "size" is the number of bytes of memory to allocate. PHA modes requires 4
*   bytes per channel, SVA (LIST) requires 2 bytes per channel.
*
*       "base_address" is the starting address in memory of the requested block
*
* This routine is called from application programs. It implements a simple
* non-reversable memory allocation scheme. There is a pointer to the next
* non-allocated memory address in each module. Once allocated, memory is never
* freed.
*
*******************************************************************************/

int nmc_allocate_memory(int module, int size, int *base_address)
{
   int s;
   struct nmc_module_info_struct *p;
   struct nmc_comm_info_struct *netinfo;

   /*      Interlock access to global variables */
   MODULE_INTERLOCK_ON(module);

   /*
    * Make sure the module number is reasonable
    */
   if(nmc_check_module(module, &s, &netinfo) != NMC_K_MCS_REACHABLE) goto done;
   p = &nmc_module_info[module];

   /*
    * Make sure enough memory is available
    */
   if ((p->free_address + size) > p->acq_mem_size) {
      s = NMC__RQSTMEMSIZETOOLG;
      *base_address = -1;
      goto done;
   }
   *base_address = p->free_address;
   p->free_address += size;
   s = OK;

done:
   MODULE_INTERLOCK_OFF(module);
   if (s  == OK)
      return OK;
   else {
      nmc_signal("nmc_allocate_memory",s);
      return ERROR;
   }
}



/*******************************************************************************
*
* NMC_BUILD_ENET_ADDR copies an Ethernet address from an integer which
* contains only the low order 2 bytes of the Ethernet address, to a complete 6
* byte Ethernet address, including the Canberra company code.
* The calling format is:
*
*       status=NMC_BUILD_ENET_ADDR(input_addr, output_addr)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "input_addr" (short integer) is the module ID number.
*
*  "output_addr" (6 byte array, by reference)
*
* This routine is called from application programs.
*
*******************************************************************************/

int nmc_build_enet_addr(int input_addr, unsigned char *output_addr)
{
   union {
      int i;
      char c[4];
   } temp;
    
   temp.i = input_addr;
   LSWAP(temp.i);

   output_addr[0] = 0;
   output_addr[1] = 0;
   output_addr[2] = 0XAF;
   output_addr[3] = temp.c[2];
   output_addr[4] = temp.c[1];
   output_addr[5] = temp.c[0];
   return OK;
}
/*******************************************************************************
*
* NMC_BROADCAST_INQ_TASK simply calls NMC_BROADCAST_INQ periodically.  It runs
* as a separate vxWorks task
* The calling format is:
*
*     status=NMC_BROADCAST_INQ_TASK(net device,inquiry type,multicast address)
*******************************************************************************/
int nmc_broadcast_inq_task(struct nmc_comm_info_struct *i)
{
   int inqtype=NCP_C_INQTYPE_ALL;
   int addr=0;
   while (1) {
      nmc_broadcast_inq(i, inqtype, addr);
	  epicsThreadSleep((double)NCP_BROADCAST_PERIOD);
   }
   return(0);  /* Never get here */
}
  

/*******************************************************************************
*
* NMC_BROADCAST_INQ broadcasts a host inquiry message. This tells modules
* "out there" to send a module status message back to us. The requests are
* sent with the STATUS_SNAP id.
* This is so that the returning status messages go to the status queue and
* status handler.
*
* The calling format is:
*
*       status=NMC_BROADCAST_INQ(net device,inquiry type,multicast address)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "net device" (longword) is a pointer to the nmc_comm_info structure
*   for the network over which the inquiry is to be broadcast.
*
*  "inquiry type" (longword) determines the type of inquiry:
*
*       NCP_C_INQTYPE_ALL:      all modules
*                     UNOWNED:  unowned modules only
*                     NOTMINE:  all modules except the ones owned by this host
*
*  "multicast address" (byte) is the least significant byte of
*   the multicast address.
*
* This runs as a separate vxWorks task
*
* It interlocks access to the global memory variables.
*
*******************************************************************************/

int nmc_broadcast_inq(struct nmc_comm_info_struct *i, int inqtype, int addr)
{
   int s, module, length;
   struct ether_header ether_header;
   struct inquiry_packet ipkt;
   struct enet_header *e;
   struct ncp_comm_header *h;
   struct ncp_comm_inquiry *p;
   struct nmc_module_info_struct *c;

   /* Interlock access to the global variables */
   GLOBAL_INTERLOCK_ON;

   /*
    * Build the command packet
    */

   memset(&ipkt,0,sizeof(ipkt));

   e = &ipkt.enet_header;
   (*e).dsap = (*i).status_sap;
   (*e).ssap = (*i).status_sap;
   (*e).control = 3;
   COPY_SNAP((*i).status_snap, (*e).snap_id);

   h = &ipkt.ncp_comm_header;
   (*h).checkword = NCP_K_CHECKWORD;
   (*h).protocol_type = NCP_C_PRTYPE_NAM;
   (*h).message_type = NCP_C_MSGTYPE_INQUIRY;
   /*
    * There is a bug(?) in the Gnu C compiler. The correct form of the
    * following statement is:
    *       (*h).data_size = sizeof(*p);
    * However, the Gnu compiler says the sizeof(*p) is 2, not 1. The AIM
    * doesn't like this.
    */
   (*h).data_size = 1;

   p = &ipkt.ncp_comm_inquiry;
   (*p).inquiry_type = inqtype;

   /*
    * Now broadcast the message. Split up according to each network device type.
    */

   switch ((*i).type) {
   case NMC_K_DTYPE_ETHERNET:
      /*
       * Ethernet: just multicast the message using the status SAP
       */

      COPY_ENET_ADDR(ni_broadcast_address, ether_header.ether_dhost);
      /* put in the LSB of the multicast address */
      ether_header.ether_dhost[5] = addr;
      /*
       * The same bug in the Gnu compiler means the following won't work:
       *     ether_heder.ether_type = sizeof(ipkt) - 14;
       */
      ether_header.ether_type = 41;

      /* Change byte order */
      nmc_byte_order_out(&ipkt);
      AIM_DEBUG(2, "nmc_broadcast_inq, sending inquiry\n");
	  length=sizeof(ipkt);
#ifdef vxWorks
      s=etherOutput((*i).pIf, &ether_header,
               (char *) &ipkt.enet_header.dsap, sizeof(ipkt)-14);
	  if (s==OK) s=length;
#else
	  /* fill the ethernet header of ipkt */
	  /* copy only addresses */
	  if ( libnet_build_ethernet(ether_header.ether_dhost, i->pIf->hw_address->ether_addr_octet,
	     	      ether_header.ether_type, &(e->dsap), 0, (unsigned char *)e) < 0) {
	  	 printf("\n error building ethernet packet");
	  }
	  if ((s=libnet_write_link_layer(i->pIf->netlnk, i->pIf->if_name, (unsigned char *) &ipkt, length)) != length) {
	  	 printf("\n error writing ethernet broadcasting packet");
	  }
#endif
      AIM_DEBUG(1, "(nmc_broadcast_inq): wrote %d bytes of %d\n",s,length);

      /*
       * If we sent one of the "conditional" inquiry messages, nothing to
       * do, else increment the "unanswered message" counter for each 
       * REACHABLE module. If the counter exceeds the maximum, mark the 
       * module UNREACHABLE.
       */

      if (inqtype == NCP_C_INQTYPE_ALL) {
         for (module=0; module < NMC_K_MAX_MODULES; module++) {
            c = &nmc_module_info[module];
            /* terminate the loop upon the last module */
            if(!(*c).valid) break; 
            if ((*c).module_comm_state == NMC_K_MCS_REACHABLE) {
               if((*c).inqmsg_counter > NMC_K_MAX_UNANSMSG)
                  (*c).module_comm_state = NMC_K_MCS_UNREACHABLE;
               (*c).inqmsg_counter++;
            }
         }
      }

      s = OK;
      goto done;
   }

   /*
    * We should never get here (we fell thru the CASE)
    */

   s = NMC__INVNETYPE;                     /* Fall thru to SIGNAL */

   /*
    * Signal errors
    */
done:
  GLOBAL_INTERLOCK_OFF;
   if (s == OK)
      return OK;
   else {
      nmc_signal("nmc_broadcast_inq",s);
      return ERROR;
   }
}

/*******************************************************************************
*
* NMC_FREEMODULE releases ownership of a module. It returns if the module is
* not currently owned by us.
*
* The calling format is:
*
*       status=NMC_FREEMODULE(module, override)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module ID number.
*
*  "override" is true if the module is to be freed even if we don't own it.
*
* This routine is called from application programs.
*
* It interlocks access to the global memory variables.
*
*******************************************************************************/

int nmc_freemodule(int module, int oflag)
{
   int i,command,s;
   struct nmc_module_info_struct *p;
   struct nmc_comm_info_struct *netinfo;
   struct ncp_hcmd_setowner packet;

   /* Interlock access to the global variables */
   MODULE_INTERLOCK_ON(module);

   /*
    * Make sure the module number is reasonable
    */

   if (nmc_check_module(module, &s, &netinfo) != NMC_K_MCS_REACHABLE) {
   	  MODULE_INTERLOCK_OFF(module);
   	  goto done;
   }
   p = &nmc_module_info[module];

   /*
    * Return if we don't own the module and the override flag is not set
    */
   if (!oflag && (*p).module_ownership_state != NMC_K_MOS_OWNEDBYUS) {
      s = OK;
   	  MODULE_INTERLOCK_OFF(module);
      goto done;
   }
   /*
    * Build the command packet
    */

   if (oflag)
      command = NCP_K_HCMD_SETOWNEROVER;
   else
      command = NCP_K_HCMD_SETOWNER;

   for(i=0; i < sizeof(packet.owner_id); i++) packet.owner_id[i] = 0;
   for(i=0; i < sizeof(packet.owner_name); i++) packet.owner_name[i] = 32;

   /*
    * Send it.
    */
   MODULE_INTERLOCK_OFF(module); /* Must turn off before calling nmc_sendcmd */

   s = nmc_sendcmd(module,command,&packet,sizeof(packet),&i,sizeof(i),&i,1);
   /* The owner_hdl routine will change the owner_id and owner_name fields
    * for the module when the response comes back */
   if (s == NCP_K_MRESP_SUCCESS) s = OK;

done:
   if (s == OK)
      return OK;
   else {
      nmc_signal("nmc_freemodule",s);
      return ERROR;
   }
}

/*************************************************************************
* nmc_byte_order_in
*
* This routine swaps the byte order and fixes alignment problems in an
* incoming Ethernet packet. It is called just after receiving a packet.
*
* It is called from:
*       nmc_sendcmd()
*       nmc_status_dispatch()
*
* It does not need interlocking for global variables, since it does not
* access any.
*
* Previously we used memmove() to re-align structures which were packed on AIM
* but not packed by gcc.  Now we use the __attribute ((packed)) directive to
* gcc this is not necessary.
*
****************************************************************************/
int nmc_byte_order_in(void *inpkt)
{
   struct enet_packet *pkt;
   struct event_packet *epkt;
   struct status_packet *spkt;
   struct response_packet *rpkt;
   
   struct ncp_comm_header *h;
   struct ncp_comm_mevent *e;
   struct ncp_comm_mstatus *s;
   struct ncp_comm_packet *r;
   
   int status;

   /* First fix the ncp_comm_header structure which is common to all
      packet types */
   pkt = (struct enet_packet *) inpkt;
   h = &(*pkt).ncp_comm_header;
   LSWAP((*h).checkword);
   LSWAP((*h).data_size);
   SSWAP((*h).checksum);

   /* Now fix the structures which depend upon the packet type */
   
   switch ((*h).message_type) {
      
   case NCP_C_MSGTYPE_MSTATUS:
      spkt = (struct status_packet *) pkt;
      s = &(*spkt).ncp_comm_mstatus;
      /* memmove(&(*s).acq_memory, &(*s).num_inputs+1, 20); */
      LSWAP((*s).comm_flags);
      LSWAP((*s).acq_memory);
      /* If the spares are ever used they get done here */
      break;
      
   case NCP_C_MSGTYPE_MEVENT:
      epkt = (struct event_packet *) pkt;
      e = &(*epkt).ncp_comm_mevent;
      /* memmove(&(*e).event_id1, &(*e).event_type+1, 8); */
      LSWAP((*e).event_id1);
      LSWAP((*e).event_id2);
      break;

   case NCP_C_MSGTYPE_PACKET:
      rpkt = (struct response_packet *) pkt;
      r = &(*rpkt).ncp_comm_packet;
      LSWAP((*r).packet_size);
      SSWAP((*r).packet_code);

      /* Switch according to packet type */
      switch ((*r).packet_code) {
      case NCP_K_MRESP_SUCCESS:
         /* This needs work, since the module returns this
            code for many commands. The resuturned structure
            depends upon what host command was sent. Luckily
            most of these responses don't return any data
            so we don't need to do anything, but some do.
          */
         break;

      case NCP_K_MRESP_RETMEMCMP:
      {
         struct ncp_mresp_retmemcmp *d;
         d = (struct ncp_mresp_retmemcmp *) (*rpkt).ncp_packet_data;
         LSWAP((*d).channels);
         break;
      }

      case NCP_K_MRESP_ADCSTATUS:
      {
         struct ncp_mresp_retadcstatus *d;
         d = (struct ncp_mresp_retadcstatus *) (*rpkt).ncp_packet_data;
         /* memmove(&(*d).live, &((*d).status)+1, 12); */
         LSWAP((*d).live);
         LSWAP((*d).real);
         LSWAP((*d).totals);
         break;
      }

      case NCP_K_MRESP_RETACQSETUP:
      {
         struct ncp_mresp_retacqsetup *d;
         d = (struct ncp_mresp_retacqsetup *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).alimit);
         LSWAP((*d).plive);
         LSWAP((*d).preal);
         LSWAP((*d).ptotals);
         LSWAP((*d).start);
         LSWAP((*d).end);
         LSWAP((*d).plimit);
         break;
      }
      
      case NCP_K_MRESP_RETLISTSTAT:
      {
         struct ncp_mresp_retliststat *d;
         d = (struct ncp_mresp_retliststat *) (*rpkt).ncp_packet_data;
         /* memmove(&(*d).offset_1, &(*d).buffer_1_full+1, 9); */
         /* memmove(&(*d).offset_2, &(*d).buffer_2_full+1, 4); */
         LSWAP((*d).offset_1);
         LSWAP((*d).offset_2);
         break;
      }

      default:
         /*
          * There are many other possible module responses.
          * They are mainly error returns and don't require
          * any byte swapping
          */
         return OK;
         break;
      }            /* End switch (packet_code) */
      break;       /* End case (response packet) */

   default:
      status = NMC__INVMODRESP; /* Not a recognized message type */
      goto signal;
      break;
   }       /* End case (message type) */
   return OK;

signal:
   nmc_signal("nmc_byte_order_in",status);
   return ERROR;
}

/*************************************************************************
* nmc_byte_order_out
*
* This routine swaps the byte order of an outgoing Ethernet packet.
* It is called just before sending a packet.
*
* It is called from:
*       nmc_sendcmd()
*       nmc_broadcast_inq()
*
* It does not need interlocking for global variables, since it does not
* access any.
*
****************************************************************************/
int nmc_byte_order_out(void *outpkt)
{
   struct enet_packet *pkt;
   struct response_packet *rpkt;
   
   struct ncp_comm_header *h;
   struct ncp_comm_packet *r;
   
   int s, packet_code;
   
   /* First fix the ncp_comm_header structure which is common to all
      packet types */
   pkt = (struct enet_packet *) outpkt;
   h = &(*pkt).ncp_comm_header;
   LSWAP((*h).checkword);
   LSWAP((*h).data_size);
   SSWAP((*h).checksum);

   /* Now fix the structures which depend upon the packet type */
   
   switch ((*h).message_type) {

   case NCP_C_MSGTYPE_INQUIRY:
      /* There is nothing which needs to be done in this case */
      break;

   case NCP_C_MSGTYPE_PACKET:
      rpkt = (struct response_packet *) pkt;
      r = &(*rpkt).ncp_comm_packet;
      LSWAP((*r).packet_size);
      /* Save the host byte ordered version of packet_code */
      packet_code = (*r).packet_code;
      SSWAP((*r).packet_code);

      switch (packet_code) {
      case NCP_K_HCMD_SETACQADDR:
      {
         struct ncp_hcmd_setacqaddr *d;
         d = (struct ncp_hcmd_setacqaddr *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         LSWAP((*d).address);
         LSWAP((*d).limit);
         break;
      }

      case NCP_K_HCMD_SETELAPSED:
      {
         struct ncp_hcmd_setelapsed *d;
         d = (struct ncp_hcmd_setelapsed *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         LSWAP((*d).live);
         LSWAP((*d).real);
         break;
      }

      case NCP_K_HCMD_SETMEMORY:
      {
         struct ncp_hcmd_setmemory *d;
         unsigned int *pdata;
         int i;
         d = (struct ncp_hcmd_setmemory *) (*rpkt).ncp_packet_data;
         pdata = (unsigned int *) &(*d).size + 1;
         for (i=0; i < (*d).size/4; i++) {
            LSWAP((pdata[i]));
         }
         LSWAP((*d).address);
         LSWAP((*d).size);
         break;
      }

      case NCP_K_HCMD_SETMEMCMP:
      {
         struct ncp_hcmd_setmemcmp *d;
         d = (struct ncp_hcmd_setmemcmp *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         break;
      }

      case NCP_K_HCMD_SETPRESETS:
      {
         struct ncp_hcmd_setpresets *d;
         d = (struct ncp_hcmd_setpresets *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         LSWAP((*d).live);
         LSWAP((*d).real);
         LSWAP((*d).totals);
         LSWAP((*d).start);
         LSWAP((*d).end);
         LSWAP((*d).limit);
         break;
      }

      case NCP_K_HCMD_SETACQSTATUS:
      {
         struct ncp_hcmd_setacqstate *d;
         d = (struct ncp_hcmd_setacqstate *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }

      case NCP_K_HCMD_ERASEMEM:
      {
         struct ncp_hcmd_erasemem *d;
         d = (struct ncp_hcmd_erasemem *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         break;
      }

      case NCP_K_HCMD_SETACQMODE:
      {
         struct ncp_hcmd_setacqmode *d;
         d = (struct ncp_hcmd_setacqmode *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }
      
      case NCP_K_HCMD_RETMEMORY:
      {
         struct ncp_hcmd_retmemory *d;
         d = (struct ncp_hcmd_retmemory *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         break;
      }

      case NCP_K_HCMD_RETMEMCMP:
      {
         struct ncp_hcmd_retmemcmp *d;
         d = (struct ncp_hcmd_retmemcmp *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         break;
      }

      case NCP_K_HCMD_RETADCSTATUS:
      {
         struct ncp_hcmd_retadcstatus *d;
         d = (struct ncp_hcmd_retadcstatus *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }

      case NCP_K_HCMD_SETHOSTMEM:
      {
         struct ncp_hcmd_sethostmem *d;
         d = (struct ncp_hcmd_sethostmem *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         /* What is the memory we write - do we need to swap it ? */
         break;
      }

      case NCP_K_HCMD_RETHOSTMEM:
      {
         struct ncp_hcmd_rethostmem *d;
         d = (struct ncp_hcmd_rethostmem *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         break;
      }
      
      case NCP_K_HCMD_SETOWNER:
      {
         struct ncp_hcmd_setowner *d;
         d = (struct ncp_hcmd_setowner *) (*rpkt).ncp_packet_data;
         /* Nothing to do in this case */
         break;
      }

      case NCP_K_HCMD_SETOWNEROVER:
      {
         struct ncp_hcmd_setownerover *d;
         d = (struct ncp_hcmd_setownerover *) (*rpkt).ncp_packet_data;
         /* Nothing to do in this case */
         break;
      }

      case NCP_K_HCMD_RESET:
      {
         /* Nothing to do in this case */
         break;
      }
      
      case NCP_K_HCMD_SETDISPLAY:
      {
         /* Nothing to do in this case */
         break;
      }

      case NCP_K_HCMD_RETDISPLAY:
      {
         /* Nothing to do in this case */
         break;
      }
      
      case NCP_K_HCMD_DIAGNOSE:
      {
         /* Nothing to do in this case */
         break;
      }
      
      case NCP_K_HCMD_SENDICB:
      {
         struct ncp_hcmd_sendicb *d;
         d = (struct ncp_hcmd_sendicb *) (*rpkt).ncp_packet_data;
         LSWAP((*d).registers);
         break;
      }

      case NCP_K_HCMD_RECVICB:
      {
         struct ncp_hcmd_recvicb *d;
         d = (struct ncp_hcmd_recvicb *) (*rpkt).ncp_packet_data;
         LSWAP((*d).registers);
         break;
      }

      case NCP_K_HCMD_SETUPACQ:
      {
         struct ncp_hcmd_setupacq *d;
         d = (struct ncp_hcmd_setupacq *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         LSWAP((*d).address);
         LSWAP((*d).alimit);
         LSWAP((*d).plive);
         LSWAP((*d).preal);
         LSWAP((*d).ptotals);
         LSWAP((*d).start);
         LSWAP((*d).end);
         LSWAP((*d).plimit);
         LSWAP((*d).elive);
         LSWAP((*d).ereal);
         break;
      }
      
      case NCP_K_HCMD_RETACQSETUP:
      {
         struct ncp_hcmd_retacqsetup *d;
         d = (struct ncp_hcmd_retacqsetup *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }
      
      case NCP_K_HCMD_SETMODEVSAP:
      {
         struct ncp_hcmd_setmodevsap *d;
         d = (struct ncp_hcmd_setmodevsap *) (*rpkt).ncp_packet_data;
         SSWAP((*d).mevsource);
         break;
      }
      
      case NCP_K_HCMD_RETLISTMEM:
      {
         struct ncp_hcmd_retlistmem *d;
         d = (struct ncp_hcmd_retlistmem *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         LSWAP((*d).offset);
         LSWAP((*d).size);
         break;
      }

      case NCP_K_HCMD_RELLISTMEM:
      {
         struct ncp_hcmd_rellistmem *d;
         d = (struct ncp_hcmd_rellistmem *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }

      case NCP_K_HCMD_RETLISTSTAT:
      {
         struct ncp_hcmd_retliststat *d;
         d = (struct ncp_hcmd_retliststat *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }
                
      case NCP_K_HCMD_RETMEMSEP:
      {
         struct ncp_hcmd_retmemsep *d;
         d = (struct ncp_hcmd_retmemsep *) (*rpkt).ncp_packet_data;
         LSWAP((*d).address);
         LSWAP((*d).size);
         LSWAP((*d).offset);
         LSWAP((*d).chunks);
         break;
      }

      case NCP_K_HCMD_RESETLIST:
      {
         struct ncp_hcmd_resetlist *d;
         d = (struct ncp_hcmd_resetlist *) (*rpkt).ncp_packet_data;
         SSWAP((*d).adc);
         break;
      }

      default:
         s = NMC__INVALCMD; /* Not a valid packet code */
         goto signal;
         break;
         
      }            /* End switch (packet_code) */
      break;       /* End case (response packet) */

   default:
      s = NMC__INVALCMD; /* Not a valid message type */
      goto signal;
   }       /* End case (message type) */
   return OK;

signal:
   nmc_signal("nmc_byte_order_out",s);
   return ERROR;
}
