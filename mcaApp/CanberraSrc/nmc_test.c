/* NMC_TEST.C */

/* Mark Rivers
 * Original date unknown
 * Modifications:
 * 06-Dec-1999 mlr    Minor changes to avoid compiler warnings
 * 05-May-2000 mlr    Minor changes to avoid compiler warnings
 * 01-Feb-2003 jee    Changes for Release 3.14.and Linux port
 *
 */

#include "nmc_sys_defs.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef vxWorks
int nmc_test1(int adc)
{
	int low_address=0x6e6;		/* Low order part of Ethernet address */
	char *intfc = "ei0";
#else
int main(int argc, char** argv)
{
	int adc=0;		
	int low_address;		/* Low order part of Ethernet address */
	char *intfc;
#endif

	int s,module,response,actual;
	unsigned char address[6];	/* Buffer for full Ethernet address */

	struct ncp_hcmd_setpresets presets; 
	struct ncp_hcmd_setelapsed elapsed;
	struct ncp_hcmd_retacqsetup setacqsetup;
	struct ncp_mresp_retacqsetup acqsetup;

#ifdef vxWorks
#else
	if (argc < 3){
		printf("\nUsage: %s <ethernet-interface> <aim-ether-address>",argv[0]);
		printf("\neg: nmcTest eth0 0xe26");
		exit(1); 
	}
	intfc = argv[1];
	low_address = strtol(argv[2],NULL,0);
	printf("\n Using AIM-Module %x",low_address);

#endif

/*
* First, setup to perform network module I/O.
*/
	s=nmc_initialize(intfc);
	if(s == ERROR) {
		printf("nmc_initialize() returns ERROR\n");
		return ERROR;
	}

/*
* Build the full Ethernet address from the low order portion
*/
	nmc_build_enet_addr(low_address, address);
	printf("\nenet address %x-%x-%x-%x-%x-%x\n", address[0],address[1],address[2],address[3],address[4],address[5]);

/*
* wait 
*/
	epicsThreadSleep((double) 5.0); 
	
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
		if ((s=nmc_freemodule(module, 1)) == OK) 
			printf("successfully released module %d\n", module);
		else
			printf("error releasing module %d\n", module);
		return ERROR;
	}

	elapsed.adc = adc;
	elapsed.live = 0;
	elapsed.real = 0;
	s = nmc_sendcmd(module,NCP_K_HCMD_SETELAPSED,&elapsed,
		sizeof(elapsed),&response,sizeof(response),&actual,0);
    printf("nmc_sendcmd: SETELAPSED = %d\n", s);

/* Build the structure to send to AIM */

	presets.adc = adc;
	presets.live = 0;
	presets.real = 200;
	presets.totals = 100;
	presets.start = 10;
	presets.end = 80;
	presets.limit = 0;

	printf("Address of .adc=%p\n", &presets.adc);
	printf("Address of .live=%p\n", &presets.live);
	printf("Address of .real=%p\n", &presets.real);
	
	s = nmc_sendcmd(module,NCP_K_HCMD_SETPRESETS,&presets,
		sizeof(presets),&response,sizeof(response),&actual,0);
	printf("nmc_sendcmd: SETPRESETS = %d\n", s);

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


	if ((s=nmc_freemodule(module, 1)) == OK) 
		printf("successfully released module %d\n", module);
	else
		printf("error releasing module %d\n", module);
	
	return OK;
}
