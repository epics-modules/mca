/* NMC_DEMO.C */

/*******************************************************************************
*
*	Demonstration Program for User Control of Networked Modules
*
* This program is a demonstration of how to directly access and control
* Nuclear Data networked acquisition modules (i.e., without using the
* ND Integrated MCA Control System software). The program:
*
*	1. Sets up for networked module I/O, finds and "buys" the module whose
*	   network address is specified by "address". You must change the last
*	   three bytes of this variable to match that of your module!
*	2. Sets up acquisition for the first ADC on the module (4K channels, 10
*	   seconds preset real time; the ADC number is determined by the 
*	   variable "adc"), and tells the module to send an event message when 
*	   the preset is reached.
*	3. Erases the acquisition memory.
*	4. Turns on acquisition for the ADC.
*	5. Waits for the acquire off event message. The loop checks the 
*	   acquisition status every three seconds just in case the event
*	   message was lost.
*	6. Obtains the acquisition memory.
*	7. Totalizes the data and prints the results.
*
* NOTES:
*
*	1. The module's network address is specified by "address". Only the
*	   last three bytes should ever be changed. For example, if the 
*	   module's address is 00-10-5E (NI105E), the last two bytes of the
*	   initialization of "address" should be changed to 0x10 and 0x5E.
*	2. The acquisition start address for an ADC can be set to any value 
*	   desired. If both ADCs are set up, make sure that the combination of
*	   start address and limit address for the ADCs do not overlap!
*
********************************************************************************
*
* Revision History:
*
*	10-Jan-1994	MLR	Rewrote based upon Nuclear Data source	
*
*******************************************************************************/

/* Definitions */

#include "nmc_sys_defs.h"
#include "epicsTime.h"
#include <stdio.h>
#include <stdlib.h>

#ifdef vxWorks
int nmc_demo(char *intfc, int low_address)
{
#else
int main(int argc, char** argv)
{
	int low_address;		/* Low order part of Ethernet address */
	char *intfc;
#endif

/*
* Declarations
*/

	int i,s,module,total=0,elive,ereal,etotals,acq;
	epicsEventId acqoff_evt;
	int adc=0;				/* The ADC we're dealing with; 0 is the first, 1 the second */
	double wait_period = 30.0;	/* Interval in secs for "deadman" polling */
	int acq_time = 10;			/* Acquisition time (seconds) */
	static int data[4096];			/* Buffer for the spectrum = 4K channels */
	unsigned char address[6];	/* Buffer for full Ethernet address */
	int low_address_default=0x675;		/* Low order part of Ethernet address */
        epicsTimeStamp startTime, endTime;

#ifdef vxWorks
#else
	if (argc < 3){
		printf("Usage: %s <ethernet-interface> <aim-ether-address>\n",argv[0]);
		printf("eg: nmc_demo eth0 0xe26\n");
		exit(1); 
	}
	intfc = argv[1];
	low_address = strtol(argv[2],NULL,16);
	printf("Using AIM-Module %x\n",low_address);
#endif

	if (low_address == 0) low_address = low_address_default;
/*
* First, setup to perform network module I/O.
*/

	printf("Calling nmc_initialize ...\n");
	s=nmc_initialize(intfc);
	if(s == ERROR) return ERROR;

/*
* Build the full Ethernet address from the low order portion
*/
	printf("Calling nmc_build_enet_addr ...\n");
	nmc_build_enet_addr(low_address, address);
	
/*
* Find the "module number" of the networked module whose address is specified
* by "address".
*/

	printf("Calling nmc_findmod_by_addr ...\n");
	s=nmc_findmod_by_addr(&module,address);
	if(s==ERROR) {nmc_signal("nmc_demo",s); return ERROR;}

/*
* Buy the module
*/

	printf("Calling nmc_buymodule ...\n");
	s=nmc_buymodule(module, 0);
	if(s==ERROR) return ERROR;
/*
* Show all modules on network
*/
	printf("Calling nmc_show_modules ...\n");
	s=nmc_show_modules();

/*
* Get an event to wait for acquisition off events
*/
	acqoff_evt = epicsEventCreate(epicsEventEmpty);

/*
* Set up to receive acquire off event messages from the module.  Semaphore
* acqoff_sem will be set whenever an acquisition off message is received for
* the first ADC.
*/

	printf("Calling nmc_addeventsem ...\n");
	nmc_acqu_addeventsem(module, adc, NCP_C_EVTYPE_ACQOFF, acqoff_evt);

/*
* Setup the first ADC on that module for acquisition (note that ADCs are 
* numbered starting with 0). We'll set it up for 4K channels, PHA mode, with
* preset real time of "acq_time" seconds (all timing is in centiseconds!).
*/
	printf("Calling nmc_acqu_setup ...\n");
	s=nmc_acqu_setup(module, adc, 0, 4096, 0, acq_time*100, 0, 0, 0, NCP_C_AMODE_PHA);
	if(s==ERROR) return ERROR;

/*
* Erase the memory and turn on acquisition
*/
	printf("Calling nmc_acqu_setstate(0) ...\n");
        s = nmc_acqu_setstate(module, adc, 0);  /* Make sure acquisition if off */
	printf("Calling nmc_acqu_erase ...\n");
	s = nmc_acqu_erase(module, 0, 4096*4);
        /* Set the elapsed live and real time back to zero. */
	printf("Calling nmc_acqu_setelapsed ...\n");
        s = nmc_acqu_setelapsed(module, adc, 0, 0);
	if(s==ERROR) return ERROR;

 
	epicsEventTryWait(acqoff_evt);	/* make sure the acquisition off event is clear */
	printf("Calling nmc_setstate(1) ...\n");
	s = nmc_acqu_setstate(module, adc, 1); /* Turn on acquire */
	if(s==ERROR) return ERROR;

/*
* Wait until acquisition turns off; check the status every "wait_period" ticks. 
* This is in case we drop the acquisition off event message. 
* Get the elapsed times at the end of acquisition.
*/

	acq = 1;
	while (acq) {
           printf("Waiting for event flag or timeout.  Should complete in %d seconds.\n", acq_time);
	   epicsEventWaitWithTimeout(acqoff_evt, wait_period);
	   printf("Calling nmc_statusupdate ...\n");
	   s=nmc_acqu_statusupdate(module,adc,0,0,0,&elive,&ereal,&etotals,&acq);
	   if(s==ERROR) return ERROR;
	}
	
/*
* Get the data into "data"
*/

	printf("Calling nmc_getmemory_cmp ...\n");
        epicsTimeGetCurrent(&startTime);
	s=nmc_acqu_getmemory_cmp(module,adc,0,1,1,1,4096,data);
	if(s==ERROR) return ERROR;
        epicsTimeGetCurrent(&endTime);
        printf("Time for nmc_getmemory_cmp = %f\n", epicsTimeDiffInSeconds(&endTime, &startTime));

	printf("Calling nmc_getmemory ...\n");
        epicsTimeGetCurrent(&startTime);
	s=nmc_acqu_getmemory(module,adc,0,1,1,1,4096,data);
	if(s==ERROR) return ERROR;
        epicsTimeGetCurrent(&endTime);
        printf("Time for nmc_getmemory = %f\n", epicsTimeDiffInSeconds(&endTime, &startTime));
/*
* Totalize the memory, print the results, release the module, and exit
*/

	for(i=0; i < 4096; i++) total += data[i];
	printf("Total counts were %d; Real time = %.2f, Live time=%.2f\n",total,ereal/100.,elive/100.);

	printf("Calling nmc_freemodule ...\n");
	nmc_freemodule(module, 0);
/*
* Show all modules on network
*/
	printf("Calling nmc_show_modules ...\n");
	s=nmc_show_modules();

        return(OK);
}
