#include <stdio.h>
#include <stdlib.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "nmc_sys_defs.h"

struct icb_module_info_struct *icb_module_info;	/* pointer to ICB module info structure */

/*******************************************************************************
*
* ICB_CRMPSC creates and/or maps the global section used to store ICB_MODULE_INFO,
* which contains the ICB module database. It can be called multiple times per
* image; it will map the section only once. The pointer to the section will be
* stored in "icb_module_info".
*
* Its calling format is:
*
*	status=ICB_CRMPSC(mode)
*
* where
*
*  "status" is the status of the operation; errors have been signaled.
*
*  "mode" (longword, by reference) is used as follows:
*
*	bit 0 (ICB_M_INIT_CLRDB) clear the database section if it already exists
*	    1 (           RONLY) map the section read-only
*
********************************************************************************
*
* Revision History:
*
*	06-Oct-1988	RJH	Excised from ICB_CONTROL_SUBS.C
*	17-Jan-1989	RJH	Add "mode" argument bit CLRDB logic to force clearing
*				of the database if the section already exists;
*				add bit RONLY logic to map the section read-only.
*	19-Feb-1992	TLG	Added support for CCNIM.
*	04-Aug-1992	TLG	Now use angle brackets to enclose system
*				include file names.
*	12-Aug-2000	MLR	Eliminated mode parameter to icb_crmpsc
*
*******************************************************************************/

int icb_crmpsc()

{

	static char mapped = 0;			/* remember if we've been called */
	int size;

/*
* If we've been called before, just return
*/

	if(mapped) return OK;

/*
* Create/map the section; remember that we've done so
*/

	mapped = 1;
	size = sizeof(struct icb_module_info_struct) * ICB_K_MAX_MODULES;
	icb_module_info = (struct icb_module_info_struct *) calloc(size, 1);

/*
* Return
*/

	return OK;

}
