/* icb_user_subs.c */
/*
 * Author:  Mark Rivers (based on code by Tim Mooney)
 * Date:    July 23, 2000
 *
 * These routines provide a simple way to communicate with ICB modules which
 * are not currently supported by icb_handler_subs.c
 */
#include        <stdio.h>
#include        <stdlib.h>
#include        <string.h>

#include "ndtypes.h"
#include "nmc_sys_defs.h"
#include "mcamsgdef.h"
#include "icb_user_subs.h"

/* Debug support */

#ifdef NODEBUG
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V...) {if (l <= icbDebug) \
                          { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,## V);}}
#endif
volatile int icbDebug = 0;

/* pointer to networked module info */
extern struct nmc_module_info_struct *nmc_module_info;


int parse_ICB_address(char *address, int *ni_module, int *icb_addr)
{
   int s;
   union {
      unsigned char chars[4];
      int integer;
   } temp_int;
   unsigned char temp_array[6];
   char *pc;

   *ni_module = *icb_addr = 0;

   /*
    * Isolate the network address: the part between the "NI" and the colon,
    * and convert it to binary.
    */
      
   Debug(1, "parse_ICB_address: address='%s'\n", address);

   if (nmc_module_info == NULL) return(NMC__NOSUCHMODULE);

   temp_int.integer = strtol(&address[2], NULL, 16);
   LSWAP(temp_int.integer);

   /*
    * Build the network-style address of the network controller and find its
    * database index.
    */

   temp_array[0] = 0;
   temp_array[1] = 0;
   temp_array[2] = 0xAF;                   /* ND's IEEE assigned prefix */
   temp_array[3] = temp_int.chars[2];
   temp_array[4] = temp_int.chars[1];
   temp_array[5] = temp_int.chars[0];
   Debug(1, "parse_ICB_address: temp_array[2..5]=%x,%x,%x,%x\n",
             temp_array[2], temp_array[3], temp_array[4], temp_array[5]);
   s = nmc_findmod_by_addr(ni_module, temp_array);
   if (s != OK) {
      Debug(1, "parse_ICB_address: nmc_findmod_by_addr() returns %d\n", s);
      return(s);
   }
   pc = strchr(address, (int)':');
   if (pc) *icb_addr = atoi(pc+1);

   Debug(1, "parse_ICB_address: ni_module=%d, icb_addr=%d\n",
             *ni_module, *icb_addr);
    /* Make sure we own the module by trying to "buy" it */
    s = nmc_buymodule(*ni_module, 0);
    if (s != OK) {
      Debug(1, "parse_ICB_address: nmc_buymodule() returns %d\n", s);
      return(s);
    }
   return(OK);
}



/*******************************************************************************
*
* Write to one or more consecutive registers in an ICB device
* Calling format:
*       status = write_icb(ni_module, icb_addr, 1st_register_#,
*                                number_of_registers, array_of_values)
*
* where
*
*  "status" is an error return.
*
*  "ni_module" number of AIM module as set by nmc_findmod_by_addr().
*
*  "icb_addr" number ICB module is set to.
*
*  "1st_register_#" (longword, by value) is the number (0-15) of the 1st
*   module register to write.
*
*  "number_of_registers" (longword, by value) is the number (1-16) of
*   registers to write.
*
*  "array_of_values" (pointer to array of char) are the values to write.
*
*******************************************************************************/

int write_icb(int ni_module, int icb_addr, int reg, int registers,
                unsigned char *values)
{
   int s,base_address,response,actual,i;
   struct ncp_hcmd_sendicb sendicb;

   /*
    * Make sure that the combination of start register and number of registers
    * doesn't go past the end of this module's registers (16 per module).
    */
   if ((reg + registers) > 16 || registers == 0) {
      nmc_signal("write_icb", MCA__INVICBREGNUM);
      return(MCA__INVICBREGNUM);
   }

   /* Build the NI command packet to "Send to ICB" */
   base_address = reg + icb_addr * 16;
   for (i=0; i < registers; i++) {
      sendicb.addresses[i].address = base_address + i;
      sendicb.addresses[i].data = values[i];
   }
   sendicb.registers = registers;

   /* Send the packet to the ICB module */
   s = nmc_sendcmd(ni_module, NCP_K_HCMD_SENDICB, &sendicb,
                   sizeof(sendicb), &response, sizeof(response), &actual, 0);
   if (actual != 0 || s != NCP_K_MRESP_SUCCESS) {
      nmc_signal("write_icb",NMC__INVMODRESP); 
      return NMC__INVMODRESP;
   }

   return(OK);
}


/*******************************************************************************
*
* Read from one or more consecutive registers in an ICB device
*
* Calling format:
*       status = my_icb_input(ni_module, icb_addr, 1st_register_#, number_of_registers,
*               array_of_values)
*
* where
*
*  "status" is an error return.
*
*  "ni_module" number of AIM module as set by nmc_findmod_by_addr().
*
*  "icb_addr" number ICB module is set to.
*
*  "1st_register_#" (longword, by value) is the number (0-15) of the 1st
*   module register to read.
*
*  "number_of_registers" (longword, by value) is the number (1-16) of
*   registers to read.
*
*  "array_of_values" (pointer to array of char) are the locations to fill.
*
*******************************************************************************/

int read_icb(int ni_module, int icb_addr, int reg, int registers, 
             unsigned char *values)
{
   int s, base_address, actual, i;
   struct ncp_hcmd_recvicb recvicb;

   /*
    * Make sure that the combination of start register and number of registers
    * doesn't go past the end of this module's registers (16 reg per module).
    */

   if ((reg + registers) > 16 || registers == 0) {
      nmc_signal("read_icb", MCA__INVICBREGNUM);
      return(MCA__INVICBREGNUM);
   }

   /* Build the NI command packet to "Send to ICB" */

   base_address = reg + icb_addr * 16;
   recvicb.registers = registers;
   for (i=0; i < registers; i++) {
      recvicb.address[i] = base_address + i;
   }

   /* Send the packet and return */
   s = nmc_sendcmd(ni_module, NCP_K_HCMD_RECVICB, &recvicb,
      sizeof(recvicb), values, registers, &actual, 0);
   if ((actual != registers) || (s != NCP_K_MRESP_SUCCESS)) {
      nmc_signal("read_icb:",NMC__INVMODRESP); 
      return NMC__INVMODRESP;
   }

   return(OK);
}
