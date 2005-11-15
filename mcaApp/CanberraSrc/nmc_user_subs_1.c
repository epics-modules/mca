/* NMC_USER_SUBS_1.C */

/******************************************************************************
*
* This module contains networked acquisition module data access routines.
* These are designed to be called from the ND9910 Display Process, as well as
* special purpose programs.
*
* As with all NMC series routines, the INITIALIZE routine must have been called
* previously to create the module database structure and start the required
* support tasks.
*
*******************************************************************************
*
* Revision History
*
*       21-Dec-1993     MLR   Modified from Nuclear Data source
*       17-Nov-1997     mlr   Minor mods to eliminate compilier warnings
*       12-May-2000     mlr   Added "signed" qualifier to char in ndl_diffdecm
*       06-Sep-2000     mlr   Added some debugging, improved formatting.
******************************************************************************/

#include "nmc_sys_defs.h"
#include <stdio.h>

extern struct nmc_module_info_struct *nmc_module_info;


/*******************************************************************************
*
* NMC_ACQU_STATUSUPDATE returns the elapsed times and acquisition status for
* a networked ADC.
*
* The calling format is:
*
*       status=NMC_ACQU_STATUSUPDATE(module,adc,group,address,mode,live,real,totals,status)
*
* where
*
*  "status" is the status of the operation.
*
*  "module" (longword is the module number.
*
*  "adc" (longword) is the module ADC number.
*
*  "group" (longword) is the group number of this group. This
*   argument is not currently used.
*
*  "address" (longword) is the address of the group's acquisition
*   memory (module-relative). This argument is not currently used.
*
*  "mode" (longword) is the acquisition mode (i.e., PHA, GAR, etc.).
*   This argument is not currently used.
*
*  "live" (returned longword, by reference) is the elapsed live time.
*
*  "real" (returned longword, by reference) is the elapsed real time.
*
*  "totals" (returned longword, by reference) are the total counts left to go.
*
*  "status" (returned longword, by reference) is the acquisition status of the
*   ADC: 1 for acquire on, 0 otherwise.
*
*******************************************************************************/

int nmc_acqu_statusupdate(module,adc,group,address,mode,live,real,totals,status)

int module,adc,group,address,mode,*live,*real,*totals,*status;

{
   int s,respsize;
   struct ncp_hcmd_retadcstatus getstatus;
   struct ncp_mresp_retadcstatus retstatus;

   /*
    * Get the acquire status and elapsed times. Note that this is not correct
    * for GAR or MX mode configurations.
    */

   getstatus.adc = adc;
   s=nmc_sendcmd(module,NCP_K_HCMD_RETADCSTATUS,
                 &getstatus,sizeof(getstatus),&retstatus,
                 sizeof(retstatus),&respsize,0);
   if(s != NCP_K_MRESP_ADCSTATUS) {
      if (aimDebug > 0) errlogPrintf("(nmc_acqu_statusupdate): bad response expected=%d, actual=%d\n", 
                NCP_K_MRESP_ADCSTATUS, s);
      nmc_signal("nmc_acqu_statusupdate",NMC__INVMODRESP);
      return ERROR;
   }

   *live = retstatus.live;
   *real = retstatus.real;
   *totals = retstatus.totals;

   if(retstatus.status) *status = 1; else *status = 0;
   
   return OK;

}

/*******************************************************************************
*
* NMC_ACQU_GETMEMORY_CMP returns the contents of networked module acquisition
* memory using the differential protocol.
*
* The calling format is:
*
*       status=NMC_ACQU_GETMEMORY_CMP(module,adc,address,nrows,start ch,start row,channels,address)
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
*  "saddress" (longword) is the start address (module-relative) of
*   the memory of the group.
*
*  "nrows" (longword) is the number of rows/group.
*
*  "start ch" (longword) is the start channel of the memory to be
*   returned.
*
*  "start row" (longword) is the start row of the memory to be
*   returned.
*
*  "channels" (longword) is the number of channels to be returned.
*
*  "address" (address) is where the memory contents are to be returned.
*
*******************************************************************************/

int nmc_acqu_getmemory_cmp(module,adc,saddress,nrows,start,srow,channels,address)

int module,adc,saddress,nrows,start,srow,channels,*address;

{

        int s,chans_left,actual,act_chans;
        unsigned char buffer[NMC_K_MAX_NIMSG];
        unsigned char *difdata;
        int *mem_buffer;
        struct ncp_hcmd_retmemory retmemory;
        struct ncp_mresp_retmemcmp *r;

/*
* Transfer the data. Basically, each time through the loop, we try to get
* the number of channels left. After decompressing the data, we know the
* actual number of channels, which gives us a new number of channels left.
*/

        chans_left = channels;                          /* init channels left to go */
        mem_buffer = address;           /* init current dest address */
        r = (struct ncp_mresp_retmemcmp *) buffer;      /* set up pointer to returned number of channels */
        difdata = buffer + sizeof(*r);                  /* set up pointer to encoded data */

        while (chans_left > 0) {

           retmemory.address = saddress +               /* compute source address and size */
                        ((channels - chans_left) +
                        (start-1) + (nrows * (srow-1))) * 4;
           retmemory.size = chans_left * 5;
           if(retmemory.size > NMC_K_MAX_NIMSG) retmemory.size = NMC_K_MAX_NIMSG;

           s = nmc_sendcmd(module,NCP_K_HCMD_RETMEMCMP, /* get the memory */
                        &retmemory,sizeof(retmemory),buffer,
                        sizeof(buffer),&actual,1);
           if (s != NCP_K_MRESP_RETMEMCMP ||
               actual == 0 ||
               (*r).channels > NMC_K_MAX_NIMSG)
           {
                if (aimDebug > 0) errlogPrintf("(nmc_acqu_getmemory_cmp): bad response expected=%d, actual=%d\n",
                        NCP_K_MRESP_RETMEMCMP, s);
                nmc_signal("nmc_acqu_getmemory_cmp",NMC__INVMODRESP);
                return ERROR;
           }

           ndl_diffdecm(difdata,(int)(*r).channels,mem_buffer,chans_left,&act_chans); /* convert differential to absolute */

           chans_left -= act_chans;                     /* Update channels to go and dest address */
           mem_buffer += act_chans;

        }
        return OK;
}


/*******************************************************************************
*
* This routine decodes 4 byte differential spectral data. It is specialized for
* the situation where the data comes from an ND556 AIM, where we know the
* number of channels (almost) to convert. The AIM can tell us that there is
* one more channel than there really is if its buffer is full, so we knock one
* off in this case.
*
* The calling format is:
*
*       status=NDL_DIFFDECM(input,channels in,output,max channels,actual channels)
*
* where
*
*  "status" is the status of the operation.
*
*  "input" (address) is the address of the encoded data.
*
*  "channels in" (longword) is the number of channels of encoded data.
*
*  "output" (address) is the address of the output longword array.
*
*  "max channels" (longword) is the number of channels in "output".
*
*  "actual channels" (returned longword, by reference) is the number of channels
*   produced by the routine.
*
********************************************************************************
*
* Revision History:
*
*       31-Dec-1993     MLR     Modified from Nuclear Data source
*       12-May-2000     MLR     Added "signed" keyword to "char".  Was not
*                               portable, and failed on PowerPC.
*
*******************************************************************************/

int ndl_diffdecm(input,channels,output,max_channels,actual_channels)
   unsigned char *input;
   int channels;
   int *output;
   int max_channels;
   int *actual_channels;

{
        unsigned char *input_ptr;       /* points to item we're converting */
        int value;                      /* current channel's value */
        int *output_ptr;                /* points to current output channel */
        int channels_left;              /* number of channels left to process */
        short sdiff;
/*
* First, could the AIM have truncated a channel, or will the caller's buffer
* overflow?
*/
        *actual_channels = channels;
        if(channels > 285) *actual_channels -= 1;
        if(*actual_channels > max_channels) *actual_channels = max_channels;
/*
* Set up to start the loop
*/
        channels_left = *actual_channels;
        input_ptr = input;
        output_ptr = output;
        value = 0;
/*
* Loop while there are channels to decompress
*/
        while(channels_left) {
           switch (*input_ptr)
           {

           /*
           * Is this a 16 bit value?
           */
           case (unsigned char) 0x7f:
                sdiff = *(short *) (input_ptr + 1);
                SSWAP(sdiff);
                value += sdiff;
                input_ptr += 3;
                break;

           /*
           * Is this a 32 bit value?
           */
           case (unsigned char) 0x80:
                value = *(int *) (input_ptr + 1);
                LSWAP(value);
                input_ptr += 5;
                break;

           /*
           * No, it's a 8 bit diff, so add it to the current value
           */
           default:
                value += *(signed char *) input_ptr;
                input_ptr++;
                break;
           }
           /*
           * Store the current value and bump the output pointer, etc.
           */
           *output_ptr = value;
           output_ptr++;
           channels_left -= 1;
        }

        return OK;
}

/******************************************************************************
* NMC_SHOW_MODULES
*
* Usage: NMC_SHOW_MODULES()
*
* This routine prints out a list of all of the AIM modules which are known
* to the system, i.e. those which have responded with Status messages in
* response to Inquiry messages since nmc_initialize was called.
*
*/
int nmc_show_modules()

{
        int i;
        struct nmc_module_info_struct *p;

        printf("Module     Owner name      Owner ID       Status      Memory size Free address\n");

        for (i=0; i < NMC_K_MAX_MODULES; i++)
        {
           p = &nmc_module_info[i];
           if ((*p).module_comm_state == NMC_K_MCS_UNKNOWN) break;
                printf("NI%2.2x%2.2x%2.2x",
                        (*p).address[3],
                        (*p).address[4],
                        (*p).address[5]);
                printf("    %8.8s  %2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X",
                        (*p).owner_name,
                        (*p).owner_id[0],
                        (*p).owner_id[1],
                        (*p).owner_id[2],
                        (*p).owner_id[3],
                        (*p).owner_id[4],
                        (*p).owner_id[5]);
                if ((*p).module_comm_state == NMC_K_MCS_REACHABLE)
                   printf("  Reachable");
                else
                   printf("  Unreachable");
                printf("   %8d      %8.8X\n", (*p).acq_mem_size, (*p).free_address);
        }
        return OK;
}
