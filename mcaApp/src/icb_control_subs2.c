/*******************************************************************************
*
* This module contains additional subroutines used to control ICB modules
* connected to ND9900 Systems. See ICB_CONTROL_SUBS.C for an overview.
*
********************************************************************************
*
* Revision History:
*
*       21-Jul-1988     RJH     Original (started)
*       04-Oct-1988     RJH         "    (completed)
*       06-Oct-1988     RJH     Excised ICB_CRMPSC to make it a separate module
*       11-Oct-1988     RJH     Added check in FINDMOD to ensure that module
*                               database has been created.
*       24-Oct-1988     RJH     Fixed a bug in POLL_CONTROLLER where ICB
*                               addresses A-F were improperly converted to ASCII;
*                               jazz up the code there so that modules which are
*                               no longer on the bus are marked unpresent.
*       17-Jan-1989     RJH     Pass INITIALIZE "flags" argument thru to ICB_CRMPSC.
*       06-Mar-1992     RJH     Disable ASTs around calls to the NMC series
*                               routines; AICONTROL can call these from both
*                               main and AST levels simultaneously!
*       Spring 1992     TLG     Add support for "ICB The Next Generation"
*       12-Oct-1993     RJH     Excised this module from ICB_CONTROL_SUBS so
*                               that simple programs don't have to haul
*                               around all the ICB NIM baggage.
*       1994            MLR     Conversion to vxWorks
*       19-Nov-1997     MLR     Minor mods to eliminate compiler warnings
*       06-Dec-1999     MLR     Minor mods to eliminate compiler warnings
*       06-May-2000     MLR     Changes to be endian-independent.
*       25-Jul-2000     MLR     Removed tabs, reformatted, adding debugging.
*                               Added check for module ownership in
*                               icb_scan_controller
*
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <errlog.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "icb_bus_defs.h"
#include "nmc_sys_defs.h"
#include "mcamsgdef.h"
#include "nmcmsgdef.h"

extern struct icb_module_info_struct *icb_module_info;  /* pointer to ICB module info structure */

extern struct nmc_module_info_struct *nmc_module_info;  /* pointer to networked module info */

/* Debug support */
#ifdef NODEBUG
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V...) {if (l <= icbDebug) \
                          { errlogPrintf("%s(%d):",__FILE__,__LINE__); \
                            errlogPrintf(FMT,## V);}}
#endif
extern volatile int icbDebug;



/*******************************************************************************
*
* ICB_POLL_CONTROLLER looks for new modules connected to an ICB controller.
* Previously unknown modules are inserted into the ICB database. If a new
* module has asserted service request, it is cleared. If an existing module
* has asserted service request due to module initialization, its registers
* are rewritten via ICB_OUTPUT.
*
* Its calling format is:
*
*       status=ICB_POLL_CONTROLLER(controller)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "controller" (string, by descriptor) is the normalized controller device
*   name.
*
*******************************************************************************/

int icb_poll_controller(dsc_controller)

char *dsc_controller;

{

   int s,space,i,index,should_exist;
   int type;
   signed char array[32];
   char temp[16];
   char int_2_hex[] = {"0123456789ABCDEF"};

/* First, call ICB_SCAN_CONTROLLER to find out what's going on */

   s=icb_scan_controller(dsc_controller,array);
   if (s != OK) return s;

/* Set up to build the module addresses of the modules SCAN found */

   strcpy(temp, dsc_controller);
   for(space=0; space < strlen(temp) && temp[space] != ':'; space++);
   space++;

/* Look for modules in the array from SCAN */

   for(i=0; i < 16; i++) {

      temp[space]  = int_2_hex[i];         /* build the module address */
      temp[space+1] = 0;  /* Null terminate string */
      type         = array[i*2] & ICB_K_MTYPE_MASK;
      should_exist = (icb_findmod_by_address(temp,&index) == OK);

      if(array[i*2] != -1) {
         Debug(5, "icb_poll_controller: address=%s, type=%d (%d)\n",
                  temp, array[i*2], type);

         if(!should_exist) {

            /* Found an unknown module. Insert that baby! */

            s=icb_find_free_entry(&index);
            if (s != OK) return s;
            s=icb_insert_module(index,temp,type,0);
            if (s != OK) return s;
            array[i*2] &= ~ICB_M_STAT_RESET;

            /*
             * If the module is an RPI (ND554), set the INITIALIZE flag. This
             * is because there is no module initialization routine for this
             * type of module, so the INITIALIZE flag would never get set
             * unless we did it here.
             */

            if(icb_module_info[index].module_type == ICB_K_MTYPE_RPI)
                                  icb_module_info[index].initialize = 1;

         }

         /* Set the PRESENT flag for this module (since it is there) */

         icb_module_info[index].present = 1;

         /*
          * If the module is saying that it has been reset, refresh its
          * registers if the INITIALIZE flag is set.
          */

         if (type != ICB_K_MTYPE_ICB2) {
            if((array[i*2+1] & ICB_M_STAT_INIT) &&
               icb_module_info[index].initialize) {
               s=icb_refresh_module(0,index);
               if (s != OK) return s;
            }

            /*
             * If the module error flag is set, set the FAILED flag in the
             * database; clear it otherwise.
             */

            if(array[i*2+1] & ICB_M_STAT_ERROR)
               icb_module_info[index].failed = 1;
            else
               icb_module_info[index].failed = 0;

         }

         /*
          * Check for a reset of a 2nd generation ICB device.  If it is
          * reset, refetch it's type & id since these may have changed.
          * Initialize it.
          */

         else {              /* if a 2nd generation ICB module       */

             if (array[i*2] & ICB_M_STAT_RESET) {

                s = icb_get_module_id (index);
                if (s != OK) return s;

                if (icb_module_info[index].handler != NULL) {
                   Debug(5, "icb_poll_controller: index=%d, calling handler\n",
                                 index);
                   s = (*icb_module_info[index].handler) (
                                 index,
                                 0, ICB_M_HDLR_INITIALIZE);
                   if (s != OK) return s;
                }
             }
         }

      } else

      /*
       * Here, no module exists at this address. If one should, clear its
       * PRESENT flag.
       */

      if(should_exist) icb_module_info[index].present = 0;

   }

/*
* All done, exit
*/

   return OK;

}

/*******************************************************************************
*
* ICB_SCAN_CONTROLLER returns the ID and status registers of the 16 module
* "slots" of an ICB controller. This routine is called by routines which need to
* find out what's going on on a particular ICB. The CONTROLLER_AVAILABLE flag
* of each module currently on this controller is set or cleared depending
* on the I/O status of the scan operation.
*
* The calling format is:
*
*       status=ICB_SCAN_CONTROLLER(controller,status array)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "controller" (string, by descriptor) is the normalized controller device
*   name.
*
*  "status array" (pointer to returned array of char[32]) is where the results
*   are stored; there are 16 pairs of the register 0 and 1 values for each
*   possible module.
*
*******************************************************************************/

int icb_scan_controller(dsc_controller,array)

char *dsc_controller;
signed char array[];

{

   int s,actual;
   struct icb_module_info_struct entry;
   struct ncp_hcmd_recvicb recvicb;
   int module;

/*
* Split up into different cases for each type of controller (each case should
* return independently).
*/

   switch (dsc_controller[0]) {

   case 'N':

      /*
       * NI controller: first, find the index in the NI database of this module
       * (cheat a little: build a dummy ICB module database entry and use
       * GET_NI_MODULE to get the NI module number).
       */

      strcpy(entry.address,dsc_controller);
      s=icb_get_ni_module(&entry);
      if (s != OK) return s;

      /* Make sure this module belongs to us.  If not we cannot scan it */
      module = entry.ni_module;
      if (nmc_module_info[module].module_ownership_state !=
                NMC_K_MOS_OWNEDBYUS) return(ERROR);

      /*
       * Now build the "Receive to ICB" command packet; we want to read icb addresses
       * 0 and 1, 16 and 17, for a total of 16 pairs.
       */

      recvicb.registers = 32;
      for(s=0; s < 16; s++) {
         recvicb.address[s*2] = s*16;
         recvicb.address[s*2+1] = s*16 + 1;
      }

      /*
       * Send the command; set it up so that the return status info will go
       * directly to the caller's array. Then return.
       */

      /*         asts = sys$setast(0); */
      s=nmc_sendcmd(entry.ni_module,NCP_K_HCMD_RECVICB,&recvicb,
                     sizeof(recvicb),array,32,&actual,0);
      /*         if(asts == SS$_WASSET) sys$setast(1); */
      if(s != OK) {
         if(actual != 32 || s != NCP_K_MRESP_SUCCESS) {
            s=NMC__INVMODRESP; nmc_signal("icb_scan_controller:1",s);
         }
      }
      return OK;

   }

   /*
    * We should never get here: unknown controller type
    */

   nmc_signal("icb_scan_controller:2",MCA__INVICBDEV);
   return MCA__INVICBDEV;

}

/*******************************************************************************
*
* ICB_POLL_NI_CONTROLLER is a utility used by NMC_ACQ_SETUP. It allows that
* routine to poll the ICB of a networked acquisition module knowing only the
* networked module's module number. This routine converts the module number into
* a normalized ICB controller address and calls ICB_POLL_CONTROLLER. The routine
* will return without action if the ICB package has not been initialized.
*
* The routine's calling format is:
*
*       status=ICB_POLL_NI_CONTROLLER(module)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "module" (longword, by reference) is the number of the networked acquisition
*   module whose ICB is to be polled.
*
*******************************************************************************/

int icb_poll_ni_controller(module)

int module;

{
   union {
      char chars[4];
      int integer;
   } temp;
   char string[33];

   /*
    * First, if the ICB package has not been initialized, just return
    */

   if(icb_module_info == 0) return 1;

   /*
    * Find the module's entry the network database and pick up the lower bytes
    * of its address.
    */

   temp.chars[3] = 0;
   temp.chars[2] = nmc_module_info[module].address[3];
   temp.chars[1] = nmc_module_info[module].address[4];
   temp.chars[0] = nmc_module_info[module].address[5];
   LSWAP(temp.integer);

   /*
    * Build the normalized form of the module's address
    */

   sprintf(string,"NI%X:",temp.integer);

   /*
    * Call POLL_CONTROLLER and return
    */

   return icb_poll_controller(string);

}

/*******************************************************************************
*
* ICB_GET_MODULE_ID Fetches the ICB module's type (NVRAM locations 0 (LSN)
* and 1 (MSN)) and  serial number (NVRAM locations 2 (LSN) through 7 (MSN)).
*
* This function works only for 2nd generation ICB modules.
*
* The calling format is:
*
*       status=ICB_GET_MODULE_ID (index);
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "index" (longword, by reference) is the index to the module's database entry.
*
*******************************************************************************/

int icb_get_module_id (index)

int index;

{
   int i;
   int s;
   int lsn, msn;
   struct icb_module_info_struct *entry;
   static int (*handler_list[])() = {
         NULL,           /* Type 0:              */
         NULL,           /* Type 1: RPI (none)   */
         NULL,           /* Type 2:              */
         icb_hvps_hdlr,  /* Type 3: 9641 HVPS    */
         icb_hvps_hdlr,  /* Type 4: 9645 HVPS    */
         icb_adc_hdlr,   /* Type 5: 9633 ADC     */
         icb_adc_hdlr,   /* Type 6: 9635 ADC     */
         icb_amp_hdlr,   /* Type 7: 9615 AMP     */
   };

   /*
    * Get the entry pointer
    */
   entry = &icb_module_info[index];

   /*
    * Fetch each nibble of the type from the module's NVRAM
    */
   s = icb_read_nvram (index, 0, &lsn);
   if (s != OK) return s;
   s = icb_read_nvram (index, 1, &msn);
   if (s != OK) return s;
   (*entry).module_type = lsn | (msn * 16);

   Debug(5, "icb_get_module_id: module_type = %d\n", (*entry).module_type);
   if (((*entry).module_type >= 0) &&
       ((*entry).module_type <= ICB_K_MAX_MTYPE))
      (*entry).handler = handler_list[(int) (*entry).module_type];
   else
      (*entry).handler = NULL;


   /*
    * Bring in the serial number (24 bit integer) one nibble at a time
    * from the MSN (NVRAM loc 7) to the LSN (NVRAM loc 2).
    */
   (*entry).module_sn = 0;
   for (i = 7; i >= 2; i--) {
      s = icb_read_nvram (index, i, &msn);
      if (s != OK) return s;
      (*entry).module_sn = ((*entry).module_sn * 16) | msn;
   }

   return OK;

}


/*******************************************************************************
*
* ICB_READ_NVRAM returns the value of the specified NVRAM location for the
* specified ICB module entry.
*
* The calling format is:
*
*       status=ICB_READ_NVRAM (index, nvram address, value);
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "index" (longword, by reference) is the index to the module's database entry.
*
*  "nvram address" (longword, by reference) is the NVRAM location to be read.
*
*  "value" (longword, by reference) is the contents of the specified NVRAM
*  location.
*
*******************************************************************************/

int icb_read_nvram (index, addr, value)

int index;
int addr;
int *value;

{
   int s;
   unsigned char reg0, reg;
   struct icb_module_info_struct *entry;

   /*
    * Get the entry pointer
    */
   entry = &icb_module_info[index];

   /*
    * Write the requested address to register 0.  Since we don't want to affect
    * the other CSR control lines, use their existing settings.
    */
   reg0  = (addr) << 5;
   reg0 |= (*entry).r0_write;
   s = icb_output (index, 0, 1, &reg0);
   if (s != OK) return s;

   /*
    * Read in the selected NVRAM location from register 1.
    */
   s = icb_input (index, 1, 1, &reg);
   *value = (reg & 0x0000000FL);

   /*
    * That's all
    */
   return s;
}


/*******************************************************************************
*
* ICB_WRITE_NVRAM writes the specified value to the specified NVRAM location
* for the specified ICB module entry.
*
* The calling format is:
*
*       status=ICB_WRITE_NVRAM (index, nvram address, value);
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "index" (longword, by reference) is the index to the module's database entry.
*
*  "nvram address" (longword, by reference) is the NVRAM location to be written.
*
*  "value" (longword, by reference) is the value to be written to the
*  specified NVRAM location.
*
*******************************************************************************/

int icb_write_nvram (int index, int addr, unsigned char value)

{
   int s;
   unsigned char reg0;
   struct icb_module_info_struct *entry;

   /*
    * Get the entry pointer
    */
   entry = &icb_module_info[index];

   /*
    * Write the requested address to register 0.  Since we don't want to affect
    * the other CSR control lines, use their existing settings.
    */
   reg0  = (addr) << 5;
   reg0 |= (*entry).r0_write;
   s = icb_output (index, 0, 1, &reg0);
   if (s != OK) return s;

   /*
    * Write to the selected NVRAM location from register 1.
    */
   s = icb_output (index, 1, 1, &value);
   if (s != OK) return s;

   /*
    * That's all
    */
   return s;
}
