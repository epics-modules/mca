/*******************************************************************************
*
* This module contains subroutines used to control ICB modules connected to
* ND9900 Systems. See specification 00-1187 for an overview.
*
* Notes:
*
*    1) The key structure used in this module is ICB_MODULE_INFO, sometimes
*       referred to as the module database. It is defined in ICB_SYS_DEFS.H,
*       and created by ICB_CRMPSC. The database is a "module hotel": once an
*       entry is entered for a module, it is never removed. The entries are
*       assigned in strictly ascending order.
*
*    2) Module addresses are stored in the database in "normalized" format.
*       This just means that the addresses are left justified, uppercase,
*       with no leading zeroes in any numeric value.
*
*    3) Once assumption made here (as well in other ND9900 modules) is that
*       device names are unique in the first character (i.e., 'N' always means
*       NI devices).
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
*       12-Oct-1993     RJH     Break out POLL_CONTROLLER and related routines
*                               into a separate module so that simple programs
*                               don't have to bring in HANDLER_SUBS and CAMSHR.
*       1994            MLR     Conversion to vxWorks
*       17-Nov-1997     MLR     Minor mods to eliminate compiler warnings
*       06-Dec-1999     MLR     Minor mods to eliminate compiler warnings
*       06-May-2000     MLR     Changes to be endian-independent
*       25-Jul-2000     MLR     Reformatted, removed tabs.  Change
*                               icb_initialize to poll all owned AIM modules
*                               for ICB modules.  icb_initialize no longer
*                               takes any parameters
*
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "icb_bus_defs.h"
#include "nmc_sys_defs.h"
#include "nmcmsgdef.h"
#include "mcamsgdef.h"

extern struct icb_module_info_struct *icb_module_info;  /* pointer to ICB module info structure */

extern struct nmc_module_info_struct *nmc_module_info;  /* pointer to networked module info */

/*******************************************************************************
*
* ICB_INITIALIZE sets up a process to control ICB modules.
*
* Its calling format is:
*
*       status=ICB_INITIALIZE()
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*******************************************************************************/

int icb_initialize()

{

   int i, s;
   static char initialized = 0;            /* remember if we've been initialized */
   struct nmc_module_info_struct *p;

   /*
    * If we've already been called, just exit
    */

   if(initialized) return OK;

   /*
    * Create and/or map the ICB database section
    */

   s=icb_crmpsc();
   if (s != OK) return s;

   /* Step through all known AIM modules, poll for attached ICB modules */
   for (i=0; i < NMC_K_MAX_MODULES; i++) {
      p = &nmc_module_info[i];
      if ((*p).module_comm_state == NMC_K_MCS_UNKNOWN) break;
      s = icb_poll_ni_controller(i);
   }

   /*
    * Remember that we've initialized
    */

   initialized = 1;
   return OK;
}

/*******************************************************************************
*
* ICB_FINDMOD_BY_ADDRESS finds a module in the ICB database by its address; it
* returns the module entry index.
*
* Its calling format is:
*
*       status=ICB_FINDMOD_BY_ADDRESS(address,index)
*
* where
*
*  "status" is the status of the operation; errors are NOT signaled.
*
*  "address" (string, by reference) is the address of the module to be found.
*   This string must be in the "normalized" address format.
*
*  "index" (returned longword, by reference) is the returned module index.
*
*******************************************************************************/

int icb_findmod_by_address(dsc_address,index)

char *dsc_address;
int *index;

{

   int i;
   char *addr;

   /*
    * First, make sure there is a module database; AICONTROL can call this
    * routine without having INITIALIZEd.
    */

   if(icb_module_info == NULL) return MCA__NOSUCHICBMODULE;

   /*
    * Just start looking from the beginning. Stop looking when we come to the
    * first invalid entry.
    */

   addr = dsc_address;

   for(i=0; i < ICB_K_MAX_MODULES; i++) {

      if(icb_module_info[i].valid) {
         if (strcmp(addr, icb_module_info[i].address) == 0) {
            *index = i;
            return OK;
         }
      } else
      break;
   }

   return MCA__NOSUCHICBMODULE;
}

/*******************************************************************************
*
* ICB_FIND_FREE_ENTRY finds a free spot in the module database for a new
* module.
*
* Its calling format is:
*
*       status=ICB_FIND_FREE_ENTRY(index)
*
* where
*
*  "status" is the status of the operation; errors have been signaled.
*
*  "index" (returned longword, by reference) is the index of the empty entry;
*   the valid bit is NOT set.
*
*******************************************************************************/

int icb_find_free_entry(index)

int *index;

{

   int i;

   /*
    * Just look for a spot with VALID false
    */

   for(i=0;i < ICB_K_MAX_MODULES && icb_module_info[i].valid; i++);

   if(i >= ICB_K_MAX_MODULES) {
      nmc_signal("icb_find_free_entry",MCA__NOFREEICBSPOTS);
      return MCA__NOFREEICBSPOTS;
   } else
   *index = i;
   return OK;

}

/*******************************************************************************
*
* ICB_INSERT_MODULE inserts information about a module into the database. It is
* called after FIND_FREE_ENTRY has found a place to put the information. INSERT
* also fills in the BASE_ADDRESS and NI_MODULE fields of the entry. It also
* calls icb_ccnim_init_cache to allocate and initialize the cache.
*
* Its calling sequence is:
*
*       status=ICB_INSERT_MODULE(index,address,type,flags)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "index" (longword, by value) is the index of the entry for this module
*   in the module database.
*
*  "address" (string, by reference) is the address of the module in normalized
*   form.
*
*  "type" (longword, by value) is the module ID type.
*
*  "flags" (longword, by value) are unused.
*
*******************************************************************************/

int icb_insert_module(index,dsc_address,type,flags)

int index,type,flags;
char *dsc_address;

{

   int i;
   int s;
   struct icb_module_info_struct *entry;
   char scratch[2];

   entry = &icb_module_info[index];

   /*
    * Stuff the passed info into the database
    */

   strcpy((*entry).address,dsc_address);
   (*entry).module_type = type;

   /*
    * Calculate the base address of the 1st register on the module and save that
    */

   for(i=2;i < 10 && (*entry).address[i] != ':'; i++);
   scratch[0]=(*entry).address[i+1];
   scratch[1]=0;
   sscanf(scratch, "%x", &i);
   (*entry).base_address = i * 16;

   /*
    * If this module is on a networked ICB controller, find the controller's
    * number in the network database.
    */

   if((*entry).address[0] == 'N') icb_get_ni_module(entry);

   /*
    * If this is a 2nd generation ICB module, get the real type
    * and serial number and initialize the cache
    */
   if (type == ICB_K_MTYPE_ICB2) {

      /*
       * Fetch the module type and serial number
       */
      s = icb_get_module_id(index);
      if (s != OK) return s;

      /* Initialize the cache */
      s = icb_init_ccnim_cache(index, 0);
      if (s != OK) return s;
   }

   /*
    * All done, set the VALID bit and return
    */

   (*entry).valid = 1;
   return OK;
}

/*******************************************************************************
*
* ICB_OUTPUT is the base routine for outputting data to an ICB module. It is
* generally called from ICB_OUTPUT_ALL or _REGISTER, or where the module
* ID register needs to be written. The values written will be stored in the
* ICB database for this module (except for registers 0 and 1).
*
* Its calling format is:
*
*       status=ICB_OUTPUT(index,1st register,number of registers,values)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "index" (longword, by value) is the index to the module's database entry.
*
*  "1st register" (longword, by value) is the number (0-15) of the 1st
*   module register to write.
*
*  "number of registers" (longword, by value) is the number (1-16) of
*   registers to write.
*
*  "values" (pointer to array of char) are the values to write.
*
*******************************************************************************/

int icb_output(index,reg,registers,values)

int index;
unsigned int reg,registers;
unsigned char values[];

{

   int s,base_address,response,actual,i;
   struct icb_module_info_struct *entry;
   struct ncp_hcmd_sendicb sendicb;

   entry = &icb_module_info[index];

   /*
    * Make sure that the combination of start register and number of registers
    * doesn't go past the end of this module's registers (there are 16 per module).
    */

   if((reg + registers) > 16 || registers == 0) {
      nmc_signal("icb_output:1",MCA__INVICBREGNUM);
      return MCA__INVICBREGNUM;
   }

   /*
    * Store the values in the ICB database (except for registers 0 and 1)
    */

   i = reg;
   while (i < (registers + reg)) {
      if(i > 1) (*entry).registers[i-2] = values[i - reg];
      i++;
   }

   /*
    * Split into cases depending on the type of ICB controller. Each case should
    * handle its own return.
    */

   switch ((*entry).address[0]) {

   case 'N':

      /*
       * NI controller: first, make sure we have the network controller's database
       * index.
       */

      if((*entry).ni_module == 0) {
         s=icb_get_ni_module(entry);
         if(s != OK) {
            nmc_signal("icb_output:2",s); return s;
         }
      }

      /*
       * Build the NI command packet to "Send to ICB"
       */

      base_address = reg + (*entry).base_address;
      for(i=0;i < registers; i++) {
         sendicb.addresses[i].address = base_address + i;
         sendicb.addresses[i].data = values[i];
      }
      sendicb.registers = registers;

      /*
       * Send the packet and return
       */

      /* asts = sys$setast(0); */
      s=nmc_sendcmd((*entry).ni_module,NCP_K_HCMD_SENDICB,&sendicb,
          sizeof(sendicb),&response,sizeof(response),&actual,0);
      /* if(asts == SS$_WASSET) sys$setast(1); */
      if(actual != 0 || s != NCP_K_MRESP_SUCCESS) {
         nmc_signal("icb_output:3",NMC__INVMODRESP); return NMC__INVMODRESP;
      }

      return OK;

   }

   /*
    * If we get here, it is an unknown device type
    */

   nmc_signal("icb_output:4",MCA__INVICBDEV);
   return MCA__INVICBDEV;

}

/*******************************************************************************
*
* ICB_GET_NI_MODULE looks up the index in the network database of the
* controller for an ICB module connected to that controller. The network index
* will be stored in NI_MODULE of the entry if the controller exists; NI_MODULE
* will be zeroed otherwise.
*
* The calling format is:
*
*       status=ICB_GET_NI_MODULE(entry)
*
* where
*
*  "status" is the status of the operation; 1 if successful, NMC__NOSUCHMODULE
*   if not.
*
*  "entry" (pointer to ICB_MODULE_INFO_STRUCT) is a pointer to the entry for the
*   ICB module.
*
*******************************************************************************/

int icb_get_ni_module(entry)

struct icb_module_info_struct *entry;

{

   int s;
   union {
      char chars[4];
      int integer;
   } temp_int;
   unsigned char temp_array[6];
   register UINT8 tmp, *p;

   /*
    * Isolate the network address: the part between the "NI" and the colon,
    * and convert it to binary.
    */

   temp_int.integer = strtol(&(*entry).address[2], NULL, 16);
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
   s=nmc_findmod_by_addr(&(*entry).ni_module,temp_array);
   if (s != OK) (*entry).ni_module = 0;

   /*
    * Return the status from the FINDMOD routine (NI_MODULE is now setup).
    */

   return s;

}

/*******************************************************************************
*
* ICB_OUTPUT_ALL takes a series of register values and dumps them to the module.
* The register values are also stored in the ICB database entry for this module.
* The registers are written starting with the 3rd one.
*
* Its calling format is:
*
*       status=ICB_OUTPUT_ALL([address],[index],number of registers,values)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "address" (string, by reference) is the normalized address of the module.
*   If the index of the module is known, this argument may be omitted (i.e.,
*   use a void pointer).
*
*  "index" (longword, by value) is the index to the module's database entry.
*   This argument is used only when "address" is omitted.
*
*  "number of registers" (longword, by value) is the number (1-16) of
*   registers to write.
*
*  "values" (pointer to array of char) are the values to write.
*
*******************************************************************************/

int icb_output_all(address,index,registers,values)

char *address;
int index,registers;
unsigned char values[];

{

   int s,ourindex;

   /*
    * Get the index for this module
    */

   if(address != 0) {

      s=icb_findmod_by_address(address,&ourindex);
      if(s != OK) {
         nmc_signal("icb_output_all",s); return s;
      }

   } else

   ourindex = index;

   /*
    * Call ICB_OUTPUT to do the operation and return
    */

   return icb_output(ourindex,2,registers,values);

}

/*******************************************************************************
*
* ICB_OUTPUT_REGISTER sends a single register value to the module. The
* register value is also stored in the ICB database entry for this module.
*
* Its calling format is:
*
*       status=ICB_OUTPUT_REGISTER([address],[index],register,value)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "address" (string, by reference) is the normalized address of the module.
*   If the index of the module is known, this argument may be omitted (i.e.,
*   use a void pointer).
*
*  "index" (longword, by value) is the index to the module's database entry.
*   This argument is used only when "address" is omitted.
*
*  "register" (longword, by value) is the number (0-15) of the register to
*   be written.
*
*  "value" (char, by reference) is the value to write.
*
*******************************************************************************/

int icb_output_register(address,index,reg,value)

char *address;
int index,reg;
unsigned char value[];

{

   int s,ourindex;

   /*
    * Get the index for this module
    */

   if(address != 0) {

      s=icb_findmod_by_address(address,&ourindex);
      if(s != OK) {
         nmc_signal("icb_output_register",s);
         return s;
      }

   } else

   ourindex = index;

   /*
    * Call ICB_OUTPUT to do the operation and return
    */

   return icb_output(ourindex,reg,1,value);

}

/*******************************************************************************
*
* ICB_INPUT is the base routine for inputting data from an ICB module(s). It is
* generally called from ICB_INPUT_ALL or _REGISTER.
*
* Its calling format is:
*
*       status=ICB_INPUT(index,1st register,number of registers,values)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "index" (longword, by value) is the index to the module's database entry.
*
*  "1st registers" (longword, by value) is the number (0-15) of the 1st
*   module register to read.
*
*  "number of registers" (longword, by value) is the number (1-16) of
*   registers to read.
*
*  "values" (pointer to returned array of char) are the values read.
*
*******************************************************************************/

int icb_input(index,reg,registers,values)

int index;
unsigned int reg,registers;
unsigned char values[];

{

   int s,base_address,actual,i;
   struct icb_module_info_struct *entry;
   struct ncp_hcmd_recvicb recvicb;

   entry = &icb_module_info[index];

   /*
    * Make sure that the combination of start register and number of registers
    * doesn't go past the end of this module's registers (there are 16 per
    * module).
    */

   if((reg + registers) > 16 || registers == 0) {
      nmc_signal("icb_input:1",MCA__INVICBREGNUM);
      return MCA__INVICBREGNUM;
   }

   /*
    * Split into cases depending on the type of ICB controller. Each case should
    * handle its own return.
    */

   switch ((*entry).address[0]) {

   case 'N':

      /*
       * NI controller: first, make sure we have the network controller's database
       * index.
       */

      if((*entry).ni_module == 0) {
         s=icb_get_ni_module(entry);
         if(s != OK) {
            nmc_signal("icb_input:2",s);
            return s;
         }
      }

      /*
       * Build the NI command packet to "Receive from ICB"
       */

      base_address = reg + (*entry).base_address;
      recvicb.registers = registers;
      for(i=0;i < registers; i++) recvicb.address[i] = base_address + i;

      /*
       * Send the packet, get the response data back in "value" (note: there
       * should be "registers" bytes in "value") and return.
       */

      /* asts = sys$setast(0); */
      s=nmc_sendcmd((*entry).ni_module,NCP_K_HCMD_RECVICB,&recvicb,
                     sizeof(recvicb),values,registers,&actual,0);
      /* if(asts == SS$_WASSET) sys$setast(1); */
      if(actual != registers || s != NCP_K_MRESP_SUCCESS) {
         nmc_signal("icb_input:3",NMC__INVMODRESP);
         return NMC__INVMODRESP;
      }

      return OK;

   }

   /*
    * If we get here, it is an unknown device type
    */

   nmc_signal("icb_input:4",MCA__INVICBDEV);
   return MCA__INVICBDEV;

}

/*******************************************************************************
*
* ICB_INPUT_ALL reads a series of register values from the module. The
* registers are read starting with the 3rd one.
*
* Its calling format is:
*
*       status=ICB_INPUT_ALL([address],[index],number of registers,values)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "address" (string, by reference) is the normalized address of the module.
*   If the index of the module is known, this argument may be omitted (i.e.,
*   use a void pointer).
*
*  "index" (longword, by value) is the index to the module's database entry.
*   This argument is used only when "address" is omitted.
*
*  "number of registers" (longword, by value) is the number (1-16) of
*   registers to read.
*
*  "values" (pointer to returned array of char) are the values read from the
*   module.
*
*******************************************************************************/

int icb_input_all(address,index,registers,values)

char *address;
int index,registers;
unsigned char values[];

{

   int s,ourindex;

   /*
    * Get the index for this module
    */

   if(address != 0) {

      s=icb_findmod_by_address(address,&ourindex);
      if(s != OK) {
         nmc_signal("icb_input_all",s);
         return s;
      }

   } else

   ourindex = index;

   /*
    * Call ICB_INPUT to do the operation and return
    */

   return icb_input(ourindex,2,registers,values);

}

/*******************************************************************************
*
* ICB_INPUT_REGISTER reads a single register value from a module.
*
* Its calling format is:
*
*       status=ICB_INPUT_REGISTER([address],[index],register,value)
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "address" (string, by reference) is the normalized address of the module.
*   If the index of the module is known, this argument may be omitted (i.e.,
*   use a void pointer).
*
*  "index" (longword, by value) is the index to the module's database entry.
*   This argument is used only when "address" is omitted.
*
*  "registers" (longword, by value) is the number (0-15) of the register
*   to read.
*
*  "value" (returned char, by reference) is the value read from the
*   module.
*
*******************************************************************************/

int icb_input_register(address,index,reg,value)

char *address;
int index,reg;
unsigned char value[];

{

   int s,ourindex;

   /*
    * Get the index for this module
    */

   if(address != 0) {

      s=icb_findmod_by_address(address,&ourindex);
      if(s != OK) {
         nmc_signal("icb_input_register",s);
         return s;
      }

   } else

   ourindex = index;

   /*
    * Call ICB_INPUT to do the operation and return
    */

   return icb_input(ourindex,reg,1,value);

}

/*******************************************************************************
*
* ICB_REFRESH_MODULE reinitializes a module, using the register values in the
* ICB database. The first two module registers are written with zeroes.
*
* The calling format is:
*
*       status=ICB_REFRESH_MODULE([address],[index])
*
* where
*
*  "status" is the status of the operation; any errors have been signaled.
*
*  "address" (string, by reference) is the normalized address of the module.
*   If the index of the module is known, this argument may be omitted (i.e.,
*   use a void pointer).
*
*  "index" (longword, by value) is the index to the module's database entry.
*   This argument is used only when "address" is omitted.
*
*******************************************************************************/

int icb_refresh_module(address,index)

char *address;
int index;

{

   int s,ourindex;
   unsigned char values[16];

   /*
    * Get the index for this module
    */

   if(address != 0) {

      s=icb_findmod_by_address(address,&ourindex);
      if(s != OK) {
         nmc_signal("icb_refresh_module",s);
         return s;
      }

   } else

   ourindex = index;

   /*
    * Copy the register values from the database to our local array
    */

   for(s=0; s < 14; s++)
      values[s+2] = icb_module_info[ourindex].registers[s];

   /*
    * Zero out the first two spots and call ICB_OUTPUT to reset the module
    */

   values[0] = values[1] = 0;
   return icb_output(ourindex,0,16,values);

}

/*******************************************************************************
*
* ICB_NORMALIZE_ADDRESS takes a possibly sloppy ICB address specification and
* turns it into the "normalized" address format: left justified, blank filled
* on the right, uppercase, numbers with leading zeroes gone, controller names
* terminated by a colon. Sample changer bit specifications are handled also.
*
* The calling format of the routine is:
*
*       status=ICB_NORMALIZE_ADDRESS(input,output)
*
* where
*
*  "status" is the status of the operation; errors have been signaled.
*
*  "input" (string, by reference) is the input address string.
*
*  "output" (returned string, by reference) is the output normalized address
*   string.
*
*******************************************************************************/

int icb_normalize_address(dsc_input,dsc_output)

char *dsc_input,*dsc_output;

{

   int pcolon,pdash,address,module=0,bit=0,slength,olength;
   char *p,got_module=0,got_bit=0;
   char temp[33];

   /*
    * First, get a working copy of the input string; upcase and left justify it
    */
   StrTrim(temp, dsc_input, strlen(dsc_input), &slength);
   StrUpCase(temp, temp, slength);

   /*
    * Split into different cases depending on the device type
    */
   p = temp;
   switch (p[0]) {

   case 'N':

      /*
       * NI: first, isolate the network address
       */

      if(p[1] != 'I') goto bad_address;
      for(pcolon=2; pcolon < slength && p[pcolon] != ':'; pcolon++);
      if(pcolon == 2) goto bad_address;
      address = strtol(&p[2], NULL, 16);

      /*
       * If there is a ICB module number and sample changer bit , isolate them
       */

      if(pcolon < slength-1) {
         for(pdash=2; pdash < slength && p[pdash] != '-'; pdash++);
         if(pdash == slength-1 || pdash == pcolon+1) goto bad_address;
         module = strtol(&p[pcolon+1], NULL, 16);
         got_module = 1;
         if(pdash < slength) {
            bit = strtol(&p[pdash+1], NULL, 16);
            got_bit = 1;
         }
      }

      /*
       * Now rebuild the string in normalized form, copy to the output, and return
       */

      sprintf(temp,"NI%X:",address);
      olength = strlen(temp);
      if(got_module) sprintf(&temp[olength],"%X",module);
      olength = strlen(temp);
      if(got_bit) sprintf(&temp[olength],"-%X",bit);
      strcpy(dsc_output,temp);
      return OK;

   }                               /* end of switch */

   /*
    * Bad/unknown address formats come to here
    */

bad_address:
      nmc_signal("icb_normalize_address",MCA__INVDEVADDR);
      return MCA__INVDEVADDR;

}
