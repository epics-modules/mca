/* NMC_DEMO_2.C */

/*******************************************************************************
*
*	Advanced Demonstration Program for User Control of Networked Modules
*
* This program is a demonstration of how to directly access and control
* Nuclear Data networked acquisition modules (i.e., without using the
* ND Integrated MCA Control System software). It is more advanced than the
* "simple" demo program, NMC_DEMO.C. 
*
* The basic idea is to run the two ADCs on an AIM as continuously as possible, 
* "ping-ponging" between two groups on each ADC; think of it as a dual-ADC 
* multi-spectral scaling experiment. Each group is 4096 channels (16384 bytes)
* in size, and are at the following addresses in AIM memory:
*
*	1st group 1st ADC:	0
*	2nd       1st    :	16384
*	1st       2nd    :	32768
*	2nd       2nd    :      49152
*
* Acquisition is started in the 1st group for each ADC; when acquire turns off
* the timers are read and acquisition is started in the 2nd groups. Then the
* spectra are read from the 1st two groups, the groups erased, and spectra 
* processed. When acquire turns off again, the process is repeated with 
* acquisition started in the 1st groups and the 2nd groups being processed.
* This entire loop is repeated twenty (actually, NUM_OF_LOOPS) times.
*
* The basic steps of the program are:
*
*	1. Sets up for networked module I/O, finds and "buys" the module whose
*	   network address is specified by "address". You must change the last
*	   three bytes of this variable to match that of your module!
*	2. Tells the module to send an event message when acquire turns off
*	   for the first ADC. (Since we'll be turning on acquisition (almost)
*	   simultaneously, and since they're both running for the same preset
*	   real time, we can assume that both will turn off simultaneously).
*	3. Sets up two sets (one for each ADC) of dual structures which define 
*	   the acquisition setup and memory erase operations to be performed. 
*	   The index for each structure array is the group number.
*	4. Sets up the module to acquire into the first "group" of the two 
*	   ADCs and starts acquisition. This makes the ping-pong acquisition
*	   loop simple.
*	5. The acquisition loop erases the "next" groups, waits for acquisition 
*	   to go off, gets the timers, sets up and starts acquisition for the
*	   next groups. Then the data is read from the previous groups and
*	   processed: the counts per live second is computed and printed.
*	6. Once all passes through the acquisition loop have been completed, the
*	   module is released and the program exits.
*
* NOTES:
*
*	1. The module's network address is specified by "address". Only the
*	   last three bytes should ever be changed. For example, the module
*	   whose network address is 00-01-B8 (NI1B8) should have the last two
*	   bytes of "address" changed to 1 and 0xB8.
*	2. Acquisition is paused about 65 milliseconds
*	   between groups; there is about 15 milliseconds skew between
*	   acquire going on for the 1st and 2nd ADCs. This measurement was done
*	   with a MicroVAX II and AIM firmware version 5.
*	3. The ADCs on an AIM are numbered 0 and 1; the code below defines the
*	   first group as being number 0 and the second as 1 (this follows C
*	   array index conventions).
*	4. While the memory arrangement for the groups is contiguous in this
*	   program, this does not have to be so. The only real requirement is
*	   that the memory for the groups (as defined by the acquisition start
*	   and limit addresses) not overlap.
*
********************************************************************************
*
* Revision History:
*
*	28-Dec-1993	MLR	Modified from Nuclear Data source
*
*******************************************************************************/

/* Definitions */

#include "nmc_sys_defs.h"


int nmc_demo_2(unsigned int low_address)

{

/*
* Declarations
*/

	int i,s,module,response,actual,total0=0,total1=0,elive0,ereal0,
		etotals0,elive1,ereal1,etotals1,acq;
	SEM_ID acqoff_sem;
	int loop = 0;				/* Acquisition loop counter */
	int group = 0;				/* Current acquire group */
	int nextgroup = 1;			/* Next acquire group */
	int num_of_loops = 20;			/* Number of acquire loops to do */
	int wait_period = 10 * sysClkRateGet();	/* Interval for "deadman" polling */
	static int data0[4096];			/* Buffer for the spectrum for the 1st ADC = 4K channels */
	static int data1[4096];			/* Buffer for the spectrum for the 2nd ADC = 4K channels */
	unsigned char address[6] = {0,0,0xaf,0,6,0x75}; /* The address of the module we're working with */
	struct ncp_hcmd_setupacq setupadc0[2];	/* Setup parameters for the 1st ADC (1st and 2nd group) */
	struct ncp_hcmd_setupacq setupadc1[2];	/*                          2nd                         */
	struct ncp_hcmd_erasemem eraseadc0[2];	/* Memory erase info for the 1st ADC (1st and 2nd group) */
	struct ncp_hcmd_erasemem eraseadc1[2];	/*                           2nd                         */
	struct ncp_hcmd_setacqstate setacq;

	extern nmc_acqu_event_hdl();

	if (low_address != 0) {
		address[4] = (low_address&0x0000ff00)) >> 8;
		address[5] = low_address&0x000000ff;
	}
*
* First, setup to perform network module I/O
*/

	s=nmc_initialize("ei0");
	if(s == ERROR) return ERROR;

/*
* Find the "module number" of the networked module whose address is specified
* by "address".
*/

	s=nmc_findmod_by_addr(&module,address);
	if(s == ERROR) {nmc_signal("nmc_demo_2",s); return ERROR;}

/*
* Buy the module. 
*/

	s=nmc_buymodule(module, 0);
	if(s == ERROR) return ERROR;

/*
* Get a semaphore we'll need later
*/

	acqoff_sem = semBCreate(SEM_Q_PRIORITY, SEM_EMPTY);

/*
* Set up to receive acquire off event messages from the module. Semaphore 
* acqoff_sem will be set whenever an acquisition off message is received for
* the first ADC.
*/

	nmc_acqu_addeventsem(module, 0, NCP_C_EVTYPE_ACQOFF, acqoff_sem);

/*
* Build structures that setup and erase each of the four memory groups we're
* dealing with.
*/

   /* Parameters for the 1st group of the 1st ADC */

	setupadc0[0].adc = 0;
	setupadc0[0].address = 0;	/* acquire starting at the beginning of AIM memory */
	setupadc0[0].alimit = 16383;	/* allow 4096 channels (alimit is address of last byte) */
	setupadc0[0].plive = 0;
	setupadc0[0].preal = 500;	/* 5 seconds realtime */
	setupadc0[0].ptotals = 0;
	setupadc0[0].start = 0;
	setupadc0[0].end = 0;
	setupadc0[0].plimit = 0;
	setupadc0[0].elive = 0;		/* set starting elapsed times to 0 */
	setupadc0[0].ereal = 0;
	setupadc0[0].mode = NCP_C_AMODE_PHA;

	eraseadc0[0].address = 0;	/* where to start erasing */
	eraseadc0[0].size = 16384;	/* bytes to erase (4096 channels = 16384 bytes */

   /* Parameters for the 2nd group of the 1st ADC */

	setupadc0[1].adc = 0;
	setupadc0[1].address = 16384;	/* acquire starts at the end of the 1st group */
	setupadc0[1].alimit = 32767;	/* allow 4096 channels */
	setupadc0[1].plive = 0;
	setupadc0[1].preal = 500;
	setupadc0[1].ptotals = 0;
	setupadc0[1].start = 0;
	setupadc0[1].end = 0;
	setupadc0[1].plimit = 0;
	setupadc0[1].elive = 0;
	setupadc0[1].ereal = 0;
	setupadc0[1].mode = NCP_C_AMODE_PHA;

	eraseadc0[1].address = 16384;
	eraseadc0[1].size = 16384;

   /* Parameters for the 1st group of the 2nd ADC */

	setupadc1[0].adc = 1;		/* set up the 2nd ADC */
	setupadc1[0].address = 32768;	/* acquire starts at the end of the 2nd group of the 1st ADC */
	setupadc1[0].alimit = 49151;	/* allow 4096 channels */
	setupadc1[0].plive = 0;
	setupadc1[0].preal = 500;
	setupadc1[0].ptotals = 0;
	setupadc1[0].start = 0;
	setupadc1[0].end = 0;
	setupadc1[0].plimit = 0;
	setupadc1[0].elive = 0;
	setupadc1[0].ereal = 0;
	setupadc1[0].mode = NCP_C_AMODE_PHA;

	eraseadc1[0].address = 32768;
	eraseadc1[0].size = 16384;

   /* Parameters for the 2nd group of the 2nd ADC */

	setupadc1[1].adc = 1;
	setupadc1[1].address = 49152;	/* acquire starts at the end of the 1st group of the 2nd ADC */
	setupadc1[1].alimit = 65535;	/* allow 4096 channels */
	setupadc1[1].plive = 0;
	setupadc1[1].preal = 500;
	setupadc1[1].ptotals = 0;
	setupadc1[1].start = 0;
	setupadc1[1].end = 0;
	setupadc1[1].plimit = 0;
	setupadc1[1].elive = 0;
	setupadc1[1].ereal = 0;
	setupadc1[1].mode = NCP_C_AMODE_PHA;

	eraseadc1[1].address = 49152;
	eraseadc1[1].size = 16384;
/*
* Set up to acquire into the first group of both ADCs
*/

	s=nmc_sendcmd(module,NCP_K_HCMD_SETUPACQ,&setupadc0[0],
		sizeof(setupadc0[0]),&response,sizeof(response),&actual,0);
	if (s == ERROR) return ERROR;

	s=nmc_sendcmd(module,NCP_K_HCMD_SETUPACQ,&setupadc1[0],
		sizeof(setupadc1[0]),&response,sizeof(response),&actual,0);
	if (s == ERROR) return ERROR;

/*
* Erase the memory for the first group of both ADCs
*/

	s=nmc_sendcmd(module,NCP_K_HCMD_ERASEMEM,&eraseadc0[0],
		sizeof(eraseadc0[0]),&response,sizeof(response),&actual,0);
	if (s == ERROR) return ERROR;

	s=nmc_sendcmd(module,NCP_K_HCMD_ERASEMEM,&eraseadc1[0],
		sizeof(eraseadc1[0]),&response,sizeof(response),&actual,0);
	if (s == ERROR) return ERROR;

/*
* Start acquisition into the first group of both ADCs
*/

	semTake(acqoff_sem, NO_WAIT);		/* make sure the acquisition off semaphore is clear */

	setacq.adc = 0;
	setacq.status = 1;		/* 1 = acquire on */
	s=nmc_sendcmd(module,NCP_K_HCMD_SETACQSTATUS,&setacq,sizeof(setacq),
		&response,sizeof(response),&actual,0);
	if (s == ERROR) return ERROR;

	setacq.adc = 1;
	s=nmc_sendcmd(module,NCP_K_HCMD_SETACQSTATUS,&setacq,sizeof(setacq),
		&response,sizeof(response),&actual,0);
	if (s == ERROR) return ERROR;
/*
* Start the acquisition loop here. The basic process is:
*
*	1. erase the "next" group of both ADCs
*	2. wait for acquisition to go off
*	3. setup to acquire into the next groups
*	4. start acquisition into the next groups
*	5. get the data for "current" groups
*	6. Compute and print counts per live second for both ADCs
*	7. Toggle the variables which indicate the "current" and "next"
*	   group number.
*
* The loop starts with "current" group 0, "next" 1, with acquisition on for
* for the current group (the variables are initialized in their declaration).
* The erase, setup, and starting of the next groups is skipped the last time
* through the loop.
*/

	while (loop < num_of_loops) {

   /* Erase the memory for the next group of both ADCs */

	   if(loop < num_of_loops-1) {		/* don't do it on the last loop */
		s=nmc_sendcmd(module,NCP_K_HCMD_ERASEMEM,&eraseadc0[nextgroup],
		   sizeof(eraseadc0[0]),&response,sizeof(response),&actual,0);
		if (s == ERROR) return ERROR;

		s=nmc_sendcmd(module,NCP_K_HCMD_ERASEMEM,&eraseadc1[nextgroup],
		   sizeof(eraseadc1[0]),&response,sizeof(response),&actual,0);
		if (s == ERROR) return ERROR;
	   }

/*
* Wait until acquisition turns off; check the status every three seconds (this 
* is in case we drop the acquisition off event message). Get the elapsed times 
* for both ADCs at the end of acquisition.
*
* NOTE: This program assumes that the second ADC turns off almost simultaneously
* with the first; therefore, the acquire status of the second ADC is ignored.
* There may be pathological circumstances where this is not the case, however;
* if it's felt this could be a problem, some code could be added to test the
* status of the second ADC (see the "if(acq);" statement below) and delay 
* until it turns off.
*/

	   acq = 1;
	   while (acq) {
		semTake(acqoff_sem, wait_period);
		s=nmc_acqu_statusupdate(module,0,0,0,0,&elive0,&ereal0,&etotals0,&acq);
		if (s == ERROR) return ERROR;
	   }
	   s=nmc_acqu_statusupdate(module,1,0,0,0,&elive1,&ereal1,&etotals1,&acq);	/* get status and timers for 2nd ADC */
	   if (s == ERROR) return ERROR;
	   if(acq);					/* 2nd ADC is still on: this really should never happen */
	
   /* Setup acquisition for the next group on both ADCs */

	   if(loop < num_of_loops-1) {			/* don't do it on the last loop */
		s=nmc_sendcmd(module,NCP_K_HCMD_SETUPACQ,&setupadc0[nextgroup],
		   sizeof(setupadc0[0]),&response,sizeof(response),&actual,0);
	   	if (s == ERROR) return ERROR;
		s=nmc_sendcmd(module,NCP_K_HCMD_SETUPACQ,&setupadc1[nextgroup],
		   sizeof(setupadc1[0]),&response,sizeof(response),&actual,0);
	   	if (s == ERROR) return ERROR;

   /* Start acquisition on the next group for both ADCs */

		semTake(acqoff_sem, NO_WAIT);		/* make sure the acquisition off semaphore is clear */

		setacq.adc = 0;
		s=nmc_sendcmd(module,NCP_K_HCMD_SETACQSTATUS,&setacq,
		   sizeof(setacq),&response,sizeof(response),&actual,0);
	   	if (s == ERROR) return ERROR;

		setacq.adc = 1;
		s=nmc_sendcmd(module,NCP_K_HCMD_SETACQSTATUS,&setacq,
		   sizeof(setacq),&response,sizeof(response),&actual,0);
	   	if (s == ERROR) return ERROR;
	   }

   /* Get the data for the previous groups into DATA0 and DATA1 */

	   s=nmc_acqu_getmemory_cmp(module,0,(setupadc0[group].address),1,1,1,4096,data0);
	   if (s == ERROR) return ERROR;

	   s=nmc_acqu_getmemory_cmp(module,1,(setupadc1[group].address),1,1,1,4096,data1);
	   if (s == ERROR) return ERROR;

   /* Compute counts/live second for both ADCs and print the results */

	   total0 = total1 = 0;
	   for(i=0; i < 4096; i++) {
		total0 += data0[i];
		total1 += data1[i];
	   }

	   printf("\nCounts/livesecond for 1st ADC = %.2f; for 2nd ADC = %.2f\n",
		total0/(elive0/100.00),total1/(elive1/100.00));

   /* We're done with the current group; toggle GROUP and NEXTGROUP */

	   group = nextgroup;
	   nextgroup = (~group) & 1;		/* toggle next between 1 and 0 */
	   loop += 1;				/* advance the loop counter */

	}					/* end of acquire loop */

/*
* Release the module and exit
*/

	nmc_freemodule(module, 0);
	return OK;
}
