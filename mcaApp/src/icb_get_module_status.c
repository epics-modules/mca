/*******************************************************************************
*
* ICB_GET_MODULE_STATE returns the availability status of an ICB module. It
* is designed to be used in module directory commands (i.e., CONF/DEV ICB).
*
* The calling format is:
*
*	state=ICB_GET_MODULE_STATE(module index)
*
* where
*
*  "state" is the availability status of the module. The values are:
*
*	ICB_K_ASTAT_NOCTRLR	controller is unavailable/unreachable
*	ICB_K_ASTAT_NOMODULE	module is not there
*	ICB_K_ASTAT_MODPRESENT	module is out there
*	ICB_K_ASTAT_MODINUSE	module is out there and being used
*
*   The precedence is that the availability of the controller overrides that of
*   the module.
*
*  "module index" (longword, by reference) is the index into the module database
*   of the module in question.
*
********************************************************************************
*
* Revision History:
*
*	25-Oct-1988	RJH	Original
*
*******************************************************************************/

#include <stdio.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "nmc_sys_defs.h"

extern struct icb_module_info_struct *icb_module_info;
extern struct nmc_module_info_struct *nmc_module_info;

int icb_get_module_state(index)

int index;

{

	int ni_module;
	struct icb_module_info_struct *e;

/*
* Determine the availability of the controller first. How this is done depends
* on the type of controller.
*/

	e = &icb_module_info[index];

	if((*e).address[0] == 'N') {

   /*
   * Networked module. Find the NI module number of our ICB module's controller.
   * If the networked controller isn't reachable, return NOCTRLR.
   */

	   ni_module = (*e).ni_module;
	   if(ni_module == 0) return ICB_K_ASTAT_NOCTRLR;
	   if(nmc_module_info[ni_module-1].module_comm_state != NMC_K_MCS_REACHABLE ||
	      nmc_module_info[ni_module-1].module_ownership_state != NMC_K_MOS_OWNEDBYUS)
		return ICB_K_ASTAT_NOCTRLR;

	}

/*
* If we fall thru to here, the controller is available. Now return the
* appropriate status for the ICB module itself.
*/

	if(!(*e).present) return ICB_K_ASTAT_NOMODULE;
	return ICB_K_ASTAT_MODPRESENT;

}
