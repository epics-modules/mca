#include <stdio.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "campardef.h"

extern struct icb_module_info_struct *icb_module_info;  /* pointer to ICB module
info struc
ture */

extern struct nmc_module_info_struct *nmc_module_info;  /* pointer to networked
module info
 */
 
/*
* Read the module's 16 registers.  R0 is the Module's CSR.  R1 is
* NVRAM data and need not be stored.
*/

unsigned char register_list[16];

int icb_read_regs(char *addr)
{
        int s, i;
        int adc_index;
	ICB_MODULE_INFO *entry;

        s = icb_findmod_by_address(addr, &adc_index);
	if (s != OK) {printf("Error looking up addr\n"); return 1;}

	entry = &icb_module_info[adc_index];

	s = icb_input(adc_index, 0, 16, register_list);
	if (s != OK) return s;

	for (i=0;i<16;i++) {
	    printf("Register %2d = 0x%.2hx\n", i, (unsigned short)register_list[i]);
        }

	return 0;
}
