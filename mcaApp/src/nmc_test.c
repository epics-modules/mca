/* NMC_TEST.C */

/* Mark Rivers
 * Original date unknown
 * Modifications:
 * 06-Dec-1999 mlr    Minor changes to avoid compiler warnings
 * 05-May-2000 mlr    Minor changes to avoid compiler warnings
 *
 */

#include "nmc_sys_defs.h"
#include <stdio.h>


int nmc_test1(int adc)

{
	int s,module,response,actual;
	unsigned char address[6];	/* Buffer for full Ethernet address */
	int low_address=0x6e6;		/* Low order part of Ethernet address */

/*	struct ncp_hcmd_setpresets presets; */
	struct ncp_hcmd_setelapsed elapsed;
	struct ncp_hcmd_retacqsetup setacqsetup;
	struct ncp_mresp_retacqsetup acqsetup;

/*
* First, setup to perform network module I/O.
*/

	s=nmc_initialize("ei0");
	if(s == ERROR) {
		printf("nmc_initialize() returns ERROR\n");
		return ERROR;
	}

/*
* Build the full Ethernet address from the low order portion
*/
	nmc_build_enet_addr(low_address, address);
	printf("enet address %x-%x-%x-%x-%x-%x\n", address[0],address[1],address[2],address[3],address[4],address[5]);
	
/*
* Find the "module number" of the networked module whose address is specified
* by "address".
*/

	s=nmc_findmod_by_addr(&module,address);
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

	elapsed.adc = adc;
	elapsed.live = 0;
	elapsed.real = 0;
	s = nmc_sendcmd(module,NCP_K_HCMD_SETELAPSED,&elapsed,
		sizeof(elapsed),&response,sizeof(response),&actual,0);
    printf("nmc_sendcmd: SETELAPSED = %d\n", s);

/* Build the structure to send to AIM */
/*
*	presets.adc = adc;
*	presets.live = 0;
*	presets.real = 200;
*	presets.totals = total;
*	presets.start = start;
*	presets.end = end;
*	presets.limit = 0;
*
*	printf("Address of .adc=%p\n", &presets.adc);
*	printf("Address of .live=%p\n", &presets.live);
*	printf("Address of .real=%p\n", &presets.real);
*	
*	s = nmc_sendcmd(module,NCP_K_HCMD_SETPRESETS,&presets,
*		sizeof(presets),&response,sizeof(response),&actual,0);
*	printf("nmc_sendcmd: SETPRESETS = %d\n", s);
*/
	setacqsetup.adc = adc;
	s = nmc_sendcmd(module,NCP_K_HCMD_RETACQSETUP,&setacqsetup,
		sizeof(setacqsetup),&acqsetup,sizeof(acqsetup),&actual,0);
	printf("nmc_sendcmd: RETACQSETUP = %d\n", s);
	printf(" address=%d\n", acqsetup.address);
	printf(" alimit=%d\n", acqsetup.alimit);
	printf(" plive=%d\n", acqsetup.plive);
	printf(" preal=%d\n", acqsetup.preal);
	printf(" start=%d\n", acqsetup.start);
	printf(" end=%d\n", acqsetup.end);
	printf(" ptotals=%d\n", acqsetup.ptotals);
	printf(" plimit=%d\n", acqsetup.plimit);


	return OK;
}
