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
*	2. Sets up acquisition for the first ADC on the module (4K channels, 60
*	   seconds preset live time; the ADC number is determined by the 
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


nmc_demo(int low_address)

{

/*
* Declarations
*/

	int i,s,module,response,actual,total=0,elive,ereal,etotals,acq;
	SEM_ID acqoff_sem;
	int adc=0;				/* The ADC we're dealing with; 0 is the first, 1 the second */
	int wait_period = 30 * sysClkRateGet();	/* Interval for "deadman" polling */
	int acq_time = 10;			/* Acquisition time (seconds) */
	static int data[4096];			/* Buffer for the spectrum = 4K channels */
	unsigned char address[6];	/* Buffer for full Ethernet address */
	int low_address_default=0x675;		/* Low order part of Ethernet address */

	if (low_address == 0) low_address = low_address_default;
/*
* First, setup to perform network module I/O.
*/

	s=nmc_initialize("ei0");
	if(s == ERROR) return ERROR;

/*
* Build the full Ethernet address from the low order portion
*/
	nmc_build_enet_addr(low_address, &address);
	
/*
* Find the "module number" of the networked module whose address is specified
* by "address".
*/

	s=nmc_findmod_by_addr(&module,address);
	if(s==ERROR) {nmc_signal("nmc_demo",s); return ERROR;}

/*
* Buy the module
*/

	s=nmc_buymodule(module, 0);
	if(s==ERROR) return ERROR;

/*
* Get a semphore  to wait for acquisition off events
*/
	acqoff_sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

/*
* Set up to receive acquire off event messages from the module.  Semaphore
* acqoff_sem will be set whenever an acquisition off message is received for
* the first ADC.
*/

	nmc_acqu_addeventsem(module, adc, NCP_C_EVTYPE_ACQOFF, acqoff_sem);

/*
* Setup the first ADC on that module for acquisition (note that ADCs are 
* numbered starting with 0). We'll set it up for 4K channels, PHA mode, with
* preset live time of "acq_time" seconds (all timing is in centiseconds!).
*/
	s=nmc_acqu_setup(module, adc, 0, 4096, acq_time*100, 0, 0, 0, 0, NCP_C_AMODE_PHA);
	if(s==ERROR) return ERROR;

/*
* Erase the memory and turn on acquisition
*/
	s = nmc_acqu_erase(module, 0, 4096*4);
	if(s==ERROR) return ERROR;

	semTake(acqoff_sem, NO_WAIT);	/* make sure the acquisition off semaphore is clear */
	s = nmc_acqu_setstate(module, adc, 1); /* Turn on acquire */
	if(s==ERROR) return ERROR;

/*
* Wait until acquisition turns off; check the status every "wait_period" ticks. 
* This is in case we drop the acquisition off event message. 
* Get the elapsed times at the end of acquisition.
*/

	acq = 1;
	while (acq) {
	   s=semTake(acqoff_sem, wait_period);
	   s=nmc_acqu_statusupdate(module,adc,0,0,0,&elive,&ereal,&etotals,&acq);
	   if(s==ERROR) return ERROR;
	}
	
/*
* Get the data into "data"
*/

	s=nmc_acqu_getmemory_cmp(module,adc,0,1,1,1,4096,data);
	if(s==ERROR) return ERROR;

/*
* Totalize the memory, print the results, release the module, and exit
*/

	for(i=0; i < 4096; i++) total += data[i];
	printf("\nTotal counts were %d; Real time was %.2f seconds.\n",total,ereal/100.00);

	nmc_freemodule(module, 0);
}
