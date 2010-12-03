/* NMC_TEST2.C */

/* Mark Rivers
 * 12-May-2000  Debug problem with PPC
 */

#define MEM_SIZE 2048
#include "nmc_sys_defs.h"
#include <stdio.h>

int ndl_diffdecm_test(unsigned char *input, int channels, int *output,
               int chans_left, int *actual_chans);

int nmc_test2(char *interface, int low_address)

{
        int s,module,actual;
        unsigned char enet_address[6]; /* Buffer for full Ethernet address */
        int saddress=0;
        int nrows=1;
        int start=1;
        int srow=1;
        int channels=MEM_SIZE;
        int data[MEM_SIZE];
        int *address = data;
        int chans_left,act_chans;
        unsigned char buffer[NMC_K_MAX_NIMSG];
        unsigned char *difdata;
        int *mem_buffer;
        struct ncp_hcmd_retmemory retmemory;
        struct ncp_mresp_retmemcmp *r;
        int i, j;

/*
* First, setup to perform network module I/O.
*/

        if (interface == NULL) interface="ei0";
        s=nmc_initialize(interface);
        if(s == ERROR) {
                printf("nmc_initialize() returns ERROR\n");
                return ERROR;
        }

/*
* Build the full Ethernet address from the low order portion
*/
        nmc_build_enet_addr(low_address, enet_address);
        printf("enet address %x-%x-%x-%x-%x-%x\n", 
            enet_address[0],enet_address[1],enet_address[2],enet_address[3],
            enet_address[4],enet_address[5]);

/*
* Find the "module number" of the networked module whose address is specified
* by "address".
*/

        s=nmc_findmod_by_addr(&module,enet_address);
        if(s==ERROR) {
                printf("nmc_findmod_by_addr() returns ERROR\n");
                nmc_signal("nmc_test",s);
                return ERROR;
        }

/*
* Buy the module
*/
        s=nmc_buymodule(module, 0);
        if(s==ERROR) {
                printf("nmc_buymodule() returns ERROR\n");
                return ERROR;
        }

/*
* Transfer the data. Basically, each time through the loop, we try to get
* the number of channels left. After decompressing the data, we know the
* actual number of channels, which gives us a new number of channels left.
*/

        chans_left = channels;                          /* init channels left to go */
        mem_buffer = address;           /* init current dest address */
        r = (struct ncp_mresp_retmemcmp *) buffer;      /* set up pointer to returned number of channels */
        difdata = buffer + sizeof(*r);                  /* set up pointer to encoded data */
        printf("difdata=%p, buffer=%p, sizeof(*r)=%ld\n", 
                  difdata, buffer, sizeof(*r));
        while (chans_left > 0) {
           printf("chans_left=%d mem_buffer=%p\n", chans_left, mem_buffer);

           retmemory.address = saddress +               /* compute source address and size */
                        ((channels - chans_left) +
                        (start-1) + (nrows * (srow-1))) * 4;
           retmemory.size = chans_left * 5;
           if(retmemory.size > NMC_K_MAX_NIMSG) retmemory.size = NMC_K_MAX_NIMSG;
           printf(
         "retmemory.address=%d, retmemory.size=%d sizeof(retmemory)=%ld\n",
                     retmemory.address, retmemory.size, sizeof(retmemory));
           s = nmc_sendcmd(module,NCP_K_HCMD_RETMEMCMP, /* get the memory */
                        &retmemory,sizeof(retmemory),buffer,
                        sizeof(buffer),&actual,1);
           if (s != NCP_K_MRESP_RETMEMCMP ||
               actual == 0 ||
               (*r).channels > NMC_K_MAX_NIMSG)
           {
                nmc_signal("nmc_acqu_getmemory_cmp",NMC__INVMODRESP);
                return ERROR;
           }

           printf("channels returned=%d\n", (*r).channels);
           ndl_diffdecm_test(
               difdata,(int)(*r).channels,mem_buffer,chans_left,&act_chans); /* convert differential to absolute */

           printf("channels returned=%d, actual channels=%d\n", 
                     (*r).channels, act_chans);
        for (i=0; i<sizeof(buffer); i+=16) {
           for (j=0; j<16; j++) printf("%x(%d) ", difdata[i+j], difdata[i+j]);
           printf("\n");
        }
        for (i=0; i<act_chans; i+=16) {
           for (j=0; j<16; j++) printf("%d ", mem_buffer[i+j]);
           printf("\n");
        }
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
*
*******************************************************************************/

int ndl_diffdecm_test(input,channels,output,max_channels,actual_channels)
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
        register unsigned char tmp, *p;
        short sdiff;
        unsigned char *sptr = (unsigned char *)&sdiff;
        unsigned char *iptr = (unsigned char *)&value;
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
                /* Copy two bytes to sdiff, reverse byte order if necessary) */
                *sptr     = *(input_ptr + 1);
                *(sptr+1) = *(input_ptr + 2);
                SSWAP(sdiff);
                value += sdiff;
                input_ptr += 3;
                break;

           /*
           * Is this a 32 bit value?
           */
           case (unsigned char) 0x80:
                *iptr     = *(input_ptr + 1);
                *(iptr+1) = *(input_ptr + 2);
                *(iptr+2) = *(input_ptr + 3);
                *(iptr+3) = *(input_ptr + 4);
                LSWAP(value);
                input_ptr += 5;
                break;

           /*
           * No, it's a 8 bit diff, so add it to the current value
           */
           default:
                value += *(char *) input_ptr;
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

