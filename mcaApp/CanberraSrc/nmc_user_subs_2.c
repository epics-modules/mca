/* NMC_USER_SUBS_2.C */

/******************************************************************************
*
* This module contains networked acquisition module data access routines for
* special purpose and list handling programs.
*
* As with all NMC series routines, the INITIALIZE routine must have been called
* previously to create or map the module database structure.
*
*******************************************************************************
*
* Revision History
*
*       28-Dec-1993     MLR     Modified from Nuclear Data source
*       15-OCT-1994     Added NMC_ACQU_SETPRESETS, NMC_ACQU_SETELAPSED
*       17-OCT-1994 Modified NMC_ACQU_SETUP. It now turns off acquisition before
                                sending the setup command and turns it back on again if it
                                was previously on. It also preserves the elapsed live and real
                                times. These must be cleared with nmc_acqu_setelapsed.
*       17-Nov-1997     MLR   Minor modifications to avoid compiler warnings
*       02-Aug-2000     MLR   Changed nmc_acque_setelapsed so that it does not
*                             turn acquisition off and back on.  This is not
*                             necessary, and it hurts performance.
*       07-Sep-2000     MLR   Cleaned up formatting
*       01-Feb-2003     jee   Changes for Release 3.14.and Linux port
*                             replaced semaphore by epicsEvent in 
*                             nmc_acqu_addeventsem/nmc_acqu_remeventsem
******************************************************************************/

#include "nmc_sys_defs.h"
#include <stdlib.h>

extern struct nmc_module_info_struct *nmc_module_info;
                                        /* Stores info about each module */

extern struct nmc_comm_info_struct *nmc_comm_info;
                                        /* Stores comm info for each network type */

extern ELLLIST nmc_sem_list;

char list_buffer_ready_array[2];        /* global array that indicates list buffer full status */

/*******************************************************************************
*
* NMC_ACQU_GETLISTBUF returns a networked acquisition module list data buffer.
* The buffer is released if requested.
*
* The calling format is:
*
*       status=NMC_ACQU_GETMEMORY(module,adc,buffer number,bytes,address,release)
*
* where
*
*  "status" is the status of the operation. Only errors from lower level routines
*   have been signaled.
*
*  "module" (longword) is the module ADC number.
*
*  "adc" (longword) is the module-relative ADC number.
*
*  "buffer number" (longword) is the number of the buffer to be read:
*   0 for the 1st buffer, 1 for the 2nd.
*
*  "bytes" (longword) is the number of bytes to be read from the
*   buffer.
*
*  "address" (address) is where the memory contents are to be returned.
*
*  "release" (longword) is 1 if the buffer is to be released, 0
*   otherwise.
*
* This routine is called from application programs.
*
* Errors detected in routine nmc_sendcmd are signalled there.
*
*******************************************************************************/

int nmc_acqu_getlistbuf(module,adc,buffer,bytes,address,release)

int module,adc,buffer,bytes,*address,release;

{

   int s,cur_bytes,bytes_left,max_bytes,actual,resp_buffer,released=0;
   char *mem_buffer;
   struct ncp_hcmd_retlistmem retlistmem;
   struct ncp_hcmd_rellistmem rellistmem;


   /*
    * Determine how many bytes can be returned per message from the module (must
    * be an even number!).
    */

      max_bytes = ((*nmc_module_info[module].comm_device).max_msg_size -
      sizeof(struct ncp_comm_header) -
      sizeof(struct ncp_comm_packet)) & ~1;

   /*
    * Now transfer the data. Basically, each time through the loop, we just get
    * either the number of bytes left or the max/message, whichever is smaller.
    *
    * LATER: Version 5 of the AIM firmware allows us to release the list
    * buffer by setting the top bit of the ADC field of the command buffer.
    */

   bytes_left = bytes;                             /* init bytes left to go */
   mem_buffer = (char *) address;                  /* init current dest address */
   retlistmem.adc = adc;                           /* set up the ADC number */
   retlistmem.buffer = buffer;                     /* set up buffer number */
   
   while (bytes_left > 0) {
      cur_bytes = bytes_left;                      /* Get number of bytes for this message */
      if(cur_bytes > max_bytes) cur_bytes = max_bytes;
      
      retlistmem.offset = bytes - bytes_left;      /* compute offset into buffer */
      retlistmem.size = cur_bytes;

      if(release && (bytes_left <= max_bytes) &&   /* release the buffer implicitly if possible and requested */
         (nmc_module_info[module].fw_revision >= 5)) {
         retlistmem.adc |= 0x8000;
         list_buffer_ready_array[buffer] = 0;    /* indicate that the buffer will be empty soon */
      }

      if((s = nmc_sendcmd(module,NCP_K_HCMD_RETLISTMEM, /* get the memory */
                          &retlistmem,sizeof(retlistmem),mem_buffer,
                          bytes_left,&actual,1)) == ERROR) return ERROR;
      if(s != NCP_K_MRESP_SUCCESS || actual == 0) {
         nmc_signal("nmc_acqu_getlistbuf:1",NMC__INVMODRESP);
         return ERROR;
      }

      bytes_left -= actual;                        /* Update bytes to go and dest address */
      mem_buffer += actual;
      if((bytes_left <= 0) && (retlistmem.adc & 0x8000)) released = 1; /* if buffer released, remember it */

   }

   /*
    * Got all the memory, release the buffer if requested (and necessary)
    */

   if(release && !released) {
      list_buffer_ready_array[buffer] = 0; /* indicate that the buffer will be empty soon */
      rellistmem.adc = adc;
      rellistmem.buffer = buffer;
      if((s = nmc_sendcmd(module,NCP_K_HCMD_RELLISTMEM,
                          &rellistmem,sizeof(rellistmem),&resp_buffer,
                          sizeof(resp_buffer),&actual,0)) == ERROR) return ERROR;
      if(s != NCP_K_MRESP_SUCCESS || actual != 0) {
         nmc_signal("nmc_acqu_getlistbuf:2",NMC__INVMODRESP);
         return ERROR;
      }
      
   }
   return OK;
}

/*******************************************************************************
*
* NMC_ACQU_ADDEVENTSEM adds or deletes a semaphore to be given upon receipt of
* a particular event message type from a particular module and ADC.
* It adds or deletes an entry from the linked list.
*
* The calling format is:
*
*       NMC_ACQU_ADDEVENTSEM(module, adc, event, sem)
*
* where
*
* "module" is the module number
*
* "adc" is the ADC number (0 or 1)
*
* "event" is the event type (NMC_C_EVTYPE_x)
*
* "evt_id"  is the epicsEventId to be signaled when the specified event message
*  arrives.
*
*******************************************************************************/

int nmc_acqu_addeventsem(module, adc, event_type, evt_id)
int module,adc,event_type;
epicsEventId evt_id;
{
   struct nmc_sem_node *node;
   int resp_buffer,actual;
   struct ncp_hcmd_setmodevsap set;
   struct nmc_comm_info_struct *info;

   /* Allocate memory for this node */
   node = (struct nmc_sem_node *) malloc(sizeof(struct nmc_sem_node));
   (*node).module = module;
   (*node).adc = adc;
   (*node).event_type = event_type;
   (*node).evt_id = evt_id;

   ellAdd(&nmc_sem_list, (ELLNODE *) node);

   /*
    * Send our asynchronous SAP address to the module.
    */

   info = nmc_module_info[module].comm_device;

   set.mev_dsap = (*info).status_sap;
   set.mev_ssap = (*info).status_sap;
   COPY_SNAP((*info).status_snap, set.snap_id);

   /*
    * Send the ADC number and SAP address
    */
   set.mevsource = adc;
   return nmc_sendcmd(module,NCP_K_HCMD_SETMODEVSAP,
                      &set, sizeof(set), &resp_buffer, sizeof(resp_buffer),
                      &actual, 0);
}

/*******************************************************************************
*
* NMC_ACQU_REMEVENT_SEM deletes a semaphore to be given upon receipt of
* a particular event message type from a particular module and ADC.
* It deletes an entry from the linked list.
*
* The calling format is:
*
*       NMC_ACQU_REMEVENTSEM(module, adc, event, evt)
*
* where
*
* "module" is the module number
*
* "adc" is the ADC number (0 or 1)
*
* "event" is the event type (NMC_C_EVTYPE_x)
*
* "evt_id"  is the epicsEventId to be signaled when the specified event message
*  arrives.
*
*******************************************************************************/

int nmc_acqu_remeventsem(module, adc, event_type, evt_id)
   int module,adc,event_type;
   epicsEventId evt_id;

{
   struct nmc_sem_node *p;

   /*
    *  Scan the list and delete the node if there is match.
    */
   p = (struct nmc_sem_node *) ellFirst(&nmc_sem_list);
   while (p != NULL) {
      if (((*p).module == module) &&
          ((*p).adc == adc) &&
          ((*p).event_type == event_type) &&
          ((*p).evt_id == evt_id))
      {
         ellDelete(&nmc_sem_list, (ELLNODE *) p);
         free(p);
         return OK;
      }

      p = (struct nmc_sem_node *) ellNext( (ELLNODE *) p);
   }

   /* We did not find the specified entry */
   return ERROR;

}

/*******************************************************************************
*
* NMC_ACQU_GETLISTSTAT returns the status of list acquisition for an ADC.
*
* The calling format is:
*
*       status=NMC_ACQU_GETLISTSTAT(module,adc,acquire,current,buff1stat,
*               buff1bytes,buff2stat,buff2bytes)
*
* where
*
*  "status" is the status of the operation.
*
*  "module" (longword) is the module number.
*
*  "adc" (longword) is the module ADC number.
*
*  "acquire" (returned longword, by reference) is the acquire status (0/1) for
*   the ADC.
*
*  "current" (returned longword, by reference) is the currently acquiring list
*   buffer (0 for the 1st, 1 for the 2nd). Note that this buffer may not
*   actually be taking any data if it has not yet been emptied.
*
*  "buff1stat" (returned longword, by reference) is true if the 1st buffer is full.
*
*  "buff1bytes" "returned longword, by reference) is the number of bytes in the
*   1st buffer.
*
*  "buff2stat" (returned longword, by reference) is true if the 2nd buffer is full.
*
*  "buff1bytes" "returned longword, by reference) is the number of bytes in the
*   1st buffer.
*
*******************************************************************************/

int nmc_acqu_getliststat(module,adc,acquire,current,
                         b1stat,b1bytes,b2stat,b2bytes)

int module,adc,*acquire,*current,*b1stat,*b1bytes,*b2stat,*b2bytes;

{

        int s,actual;
        struct ncp_hcmd_retliststat retlist;
        struct ncp_mresp_retliststat retstat;

/*
* Build the command packet and send it
*/

        retlist.adc = adc;
        if((s = nmc_sendcmd(module,NCP_K_HCMD_RETLISTSTAT,
                &retlist,sizeof(retlist),&retstat,
                sizeof(retstat),&actual,0)) == ERROR) return ERROR;
        if(s != NCP_K_MRESP_RETLISTSTAT || actual != sizeof(retstat)) {
           nmc_signal("nmc_acqu_getliststat",NMC__INVMODRESP);
           return ERROR;
        }

/*
* Get the info from the packet and return
*/

        *acquire = retstat.status;
        *current = retstat.current_buffer;
        *b1stat = retstat.buffer_1_full;
        *b1bytes = retstat.offset_1;
        *b2stat = retstat.buffer_2_full;
        *b2bytes = retstat.offset_2;

        return OK;

}

/*******************************************************************************
*
* NMC_ACQU_SETUP sets up a module ADC input for acquisition. The ADC will be
* turned off. The present elapsed times will be read from the module and then
* written back out. The module will be turned back on if it was previously on.
*
* Its calling format is:
*
*       status=NMC_ACQU_SETUP(module,adc,address,channels,plive,preal,ptotals,
*                               ptschan,ptechan,acqmod)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module number.
*
*  "adc" (longword) is the ADC input number (0 or 1).
*
*  "address" (longword) is the acquisition start address (in
*   bytes).
*
*  "channels" (longword) is the number of channels to be accepted.
*
*  "plive" (longword) is the preset live time, in centiseconds.
*
*  "preal" (longword) is the preset real time, in centiseconds.
*
*  "ptotals" (longword) is the preset totals value.
*
*  "ptschan" (longword) is the preset totals region start channel.
*
*  "ptechan" (longword) is the preset totals region end channel.
*
*  "acqmod" (longword) is the acquisition mode: 0=PHA, 1=LFC,
*   2=DBLS.
*
*******************************************************************************/

int nmc_acqu_setup(module,adc,address,channels,plive,preal,
                   ptotals,ptschan,ptechan,mode)

int module,adc,address,channels,plive,preal,ptotals,ptschan,ptechan,
                mode;

{
   int response,actual;
   struct ncp_hcmd_setupacq setup;
   int elive, ereal, etotals, acquiring, s;


   /* Get the current acquisition status - remember if it was acquiring */
   nmc_acqu_statusupdate(module,adc,0,0,0,&elive,&ereal,&etotals,&acquiring);

   /* Turn off acquisition */
   nmc_acqu_setstate(module, adc, 0);

   /* Get the current acquisition status again.
    * This will return accurate values for the elapsed live and real times */
   nmc_acqu_statusupdate(module,adc,0,0,0,&elive,&ereal,&etotals,&s);

   /* Fill in the module command structure */
    setup.adc = adc;
    setup.address = address;
    setup.alimit = address + ((channels)*4 - 1);
    setup.plive = plive;
    setup.preal = preal;
    setup.ptotals = ptotals;
    setup.start = ptschan;
    setup.end = ptechan;
    setup.plimit = 0;
    setup.elive = elive;
    setup.ereal = ereal;
    setup.mode = mode;

    /* Send the setup command */
    s = nmc_sendcmd(module,NCP_K_HCMD_SETUPACQ,&setup,sizeof(setup),
                    &response,sizeof(response),&actual,0);

    /* If acquisition was previously on, turn it on again */
    if (acquiring) nmc_acqu_setstate(module, adc, 1);

    return(s);
}

/*******************************************************************************
*
* NMC_ACQU_SETSTATE turns acquisition on or off for an input.
*
* Its calling format is:
*
*       status=NMC_ACQU_SETSTATE(module,adc,state)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module number.
*
*  "adc" (longword) is the ADC input number (0 or 1).
*
*  "state" (longword) is the desired state of the input (0=off,
*   1=on).
*
*******************************************************************************/

int nmc_acqu_setstate(module,adc,state)

int module,adc,state;

{
   int response,actual;
   struct ncp_hcmd_setacqstate setstate;

   /*
    * Build the command message
    */

   setstate.adc = adc;
   setstate.status = state;

   /*
    * Send it!
    */
   return nmc_sendcmd(module,NCP_K_HCMD_SETACQSTATUS,&setstate,
                      sizeof(setstate),&response,sizeof(response),&actual,0);

}

/*******************************************************************************
*
* NMC_ACQU_ERASE erases acquisition memory.
*
* Its calling format is:
*
*       status=NMC_ACQU_ERASE(module,address,size)
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module number.
*
*  "address" (longword) is the start address of the memory to be
*   erased.
*
*  "size" (longword) is the number of bytes of memory to be
*   erased.
*
*******************************************************************************/

int nmc_acqu_erase(module,address,size)

int module,address,size;

{
   int response,actual;
   struct ncp_hcmd_erasemem erase;

   /*
    * Build the command message
    */

   erase.address = address;
   erase.size = size;

   /*
    * Send it!
    */

   return nmc_sendcmd(module,NCP_K_HCMD_ERASEMEM,&erase,
                      sizeof(erase),&response,sizeof(response),&actual,0);

}



/*******************************************************************************
*
* NMC_ACQU_SETPRESETS sets the acquisition preset values
* NOTE THIS ROUTINE DOES NOT WORK DUE TO A BUG IN THE AIM FIRMWARE!!!!!
*
* Its calling format is:
*       status=NMC_ACQU_SETPRESETS(module,adc,plive,preal,ptotals,ptschan,ptechan)
*
* Note that unlike NMC_ACQU_SETUP this routine does not reset the elapsed values
* and it can be issued while the module is acquiring data.
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module number.
*
*  "adc" (longword) is the ADC input number (0 or 1).
*
*  "plive" (longword) is the preset live time, in centiseconds.
*
*  "preal" (longword) is the preset real time, in centiseconds.
*
*  "ptotals" (longword) is the preset totals value.
*
*  "ptschan" (longword) is the preset totals region start channel.
*
*  "ptechan" (longword) is the preset totals region end channel.
*
*******************************************************************************/

int nmc_acqu_setpresets(module,adc,plive,preal,ptotals,ptschan,ptechan)

int module,adc,plive,preal,ptotals,ptschan,ptechan;

{

        int response,actual;

        struct ncp_hcmd_setpresets presets;

/*
* Build the command message
*/
        presets.adc = adc;
        presets.live = plive;
        presets.real = preal;
        presets.totals = ptotals;
        presets.start = ptschan;
        presets.end = ptechan;
        presets.limit = 0;

/*
* Send it!
*/

        return nmc_sendcmd(module,NCP_K_HCMD_SETPRESETS,&presets,
                sizeof(presets),&response,sizeof(response),&actual,0);

}


/*******************************************************************************
*
* NMC_ACQU_SETELAPSED sets the acquisition elapsed values
*
* Its calling format is:
*       status=NMC_ACQU_SETELAPSED(module,adc,elive,ereal)
*
* Note that unlike NMC_ACQU_SETUP this routine can be issued while the
* module is acquiring data.
* Previously this routine turned off acquisition and turned it back on again
* if it was acquiring, but this is not necessary.
*
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "module" (longword) is the module number.
*
*  "adc" (longword) is the ADC input number (0 or 1).
*
*  "elive" (longword) is the elapsed live time, in centiseconds.
*
*  "ereal" (longword) is the elapsed real time, in centiseconds.
*
*******************************************************************************/

int nmc_acqu_setelapsed(module,adc,elive,ereal)

int module,adc,elive,ereal;

{

        int response,actual;
        int s;

        struct ncp_hcmd_setelapsed elapsed;

/* Build the command message */
        elapsed.adc = adc;
        elapsed.live = elive;
        elapsed.real = ereal;

/* Send it! */
        s = nmc_sendcmd(module,NCP_K_HCMD_SETELAPSED,&elapsed,
                sizeof(elapsed),&response,sizeof(response),&actual,0);

        return(s);
}
