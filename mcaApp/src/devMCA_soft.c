/* devMCA_soft.c */

/* devMCA_soft.c - Device Support Routines for Canberra AIM MCA */
/*
 *      Author:  Tim Mooney
 *      Date:    11/29/00
 *
 *      Copyright 1994, the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contract
 *      W-31-109-ENG-38 at Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01  11/29/00  tmm  Developed from devMCA_AIM, mostly by erasing
 */


#include	<stdio.h>
#include	<string.h>
#include	<stdlib.h>

#include	<alarm.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
/*#include	<rec/dbCommon.h>*/

#include	"mcaRecord.h"
#include	"mca.h"
#include        "epicsExport.h"

/* Debug support */
#if 0
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V...) {if (l <= devMCA_softDebug) \
			  { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,## V);}}
#endif
volatile int devMCA_softDebug = 0;

/* Create DSET */
static long init_record();
static long send_msg();
static long read_array();
typedef struct {
	long		number;
	DEVSUPFUN	report;
	DEVSUPFUN	init;
	DEVSUPFUN	init_record;
	DEVSUPFUN	get_ioint_info;
	DEVSUPFUN	send_msg;
	DEVSUPFUN	read_array;
} MCA_SOFT_DSET;

MCA_SOFT_DSET devMCA_soft = {
	6,
	NULL,
	NULL,
	init_record,
	NULL,
	send_msg,
	read_array
};
epicsExportAddress(dset, devMCA_soft);


static long init_record(mcaRecord *pmca)
{

	Debug(5, "(init_record): entry for '%s'\n", pmca->name);


	pmca->nord = 0;
	return(0);
}

static long send_msg(mcaRecord *pmca, unsigned long msg, void *parg)
{
	int s=0, seq, nchans;

	switch (msg) {
	case MSG_ACQUIRE:
		/* start acquisition */
		Debug(5, "(send_msg): start acquisition %d\n", s);
		break;
	case MSG_READ:
		/* start read operation */
		/* This is a no-op. Read-array does everything. */
		break;
	case MSG_SET_CHAS_INT:
		/* set channel advance source to internal (timed) */
		/* This is a NOOP for current MCS hardware - done manually */
		break;
	case MSG_SET_CHAS_EXT:
		/* set channel advance source to external */
		/* This is a NOOP for current MCS hardware - done manually */
		break;
	case MSG_SET_NCHAN:
		/* set number of channels */
		nchans = *(long *)parg;
		Debug(5, "(send_msg): acqu setup %d\n", s);
		break;
	case MSG_SET_SEQ:
		/* set sequence number */
		seq = *(long *)parg;
		Debug(5, "(send_msg): acqu setup %d\n", s);
		break;
	case MSG_SET_DWELL:
		/* set dwell time */
		/* This is a NOOP for current MCS hardware - done manually */
		break;
	case MSG_SET_REAL_TIME:
		/* set preset real time. Convert to centiseconds */
		Debug(5, "(send_msg): acqu setpresets %d\n", s);
		break;
	case MSG_SET_LIVE_TIME:
		/* set preset live time. Convert to centiseconds */
		Debug(5, "(send_msg): acqu setpresets %d\n", s);
		break;
	case MSG_SET_COUNTS:
		/* set preset counts */
		Debug(5, "(send_msg): acqu setpresets %d\n", s);
		break;
	case MSG_SET_LO_CHAN:
		/* set lower side of region integrated for preset counts */
		Debug(5, "(send_msg): acqu setpresets %d\n", s);
		break;
	case MSG_SET_HI_CHAN:
		/* set high side of region integrated for preset counts */
		Debug(5, "(send_msg): acqu setpresets %d\n", s);
		break;
	case MSG_SET_NSWEEPS:
		/* set number of sweeps (for MCS mode) */
		/* This is a NOOP on current version of MCS */
		break;
	case MSG_SET_MODE_PHA:
		/* set mode to Pulse Height Analysis */
		Debug(5, "(send_msg): acqu setup %d\n", s);
		break;
	case MSG_SET_MODE_MCS:
		/* set mode to MultiChannel Scaler */
		Debug(5, "(send_msg): acqu setup %d\n", s);
		break;
	case MSG_SET_MODE_LIST:
		/* set mode to LIST (record each incoming event) */
		Debug(5, "(send_msg): acqu setup %d\n", s);
		break;
	case MSG_GET_ACQ_STATUS:
		/* Read the current status of the device */
		Debug(5, "(send_msg): status update %d\n", s);
		/* pmca->ertm = ereal/100.0; */
		/* pmca->eltm = elive/100.0; */
		/* pmca->act = etotals; */
		/* pmca->acqg = acq_status; */
		break;
	case MSG_STOP_ACQUISITION:
		/* stop data acquisition */
		Debug(5, "(send_msg): stop acquisition %d\n", s);
		break;
	case MSG_ERASE:
		/* erase */
		Debug(5, "(send_msg): erase %d\n", s);
		/* Set the elapsed live and real time back to zero. */
		Debug(5, "(send_msg): set elapsed %d\n", s);
		break;
	}
	return(0);
}

static long read_array(mcaRecord *pmca)
{
	int nuse = pmca->nuse;

	Debug(5, "(read_array): doing nothing\n");
	pmca->nord = nuse;
	return(0);
}
