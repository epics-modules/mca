/* devMCA_soft.c */

/* devMCA_soft.c - Soft Device Support Routines for MCA record */
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
#include	<errlog.h>
#include	<recSup.h>
#include	<devSup.h>
#include	<link.h>
/*#include	<rec/dbCommon.h>*/

#include	"mcaRecord.h"
#include	"mca.h"
#include        "epicsExport.h"

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

	if (devMCA_softDebug > 5) errlogPrintf("(init_record): entry for '%s'\n", pmca->name);


	pmca->nord = 0;
	return(0);
}

static long send_msg(mcaRecord *pmca, unsigned long msg, void *parg)
{
	int s=0, seq, nchans;

	switch (msg) {
	case mcaStartAcquire:
		/* start acquisition */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): start acquisition %d\n", s);
		break;
	case mcaData:
		/* start read operation */
		/* This is a no-op. Read-array does everything. */
		break;
	case mcaChannelAdvanceInternal:
		/* set channel advance source to internal (timed) */
		/* This is a NOOP for current MCS hardware - done manually */
		break;
	case mcaChannelAdvanceExternal:
		/* set channel advance source to external */
		/* This is a NOOP for current MCS hardware - done manually */
		break;
	case mcaNumChannels:
		/* set number of channels */
		nchans = *(long *)parg;
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setup %d\n", s);
		break;
	case mcaSequence:
		/* set sequence number */
		seq = *(long *)parg;
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setup %d\n", s);
		break;
	case mcaDwellTime:
		/* set dwell time */
		/* This is a NOOP for current MCS hardware - done manually */
		break;
	case mcaPresetRealTime:
		/* set preset real time. Convert to centiseconds */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setpresets %d\n", s);
		break;
	case mcaPresetLiveTime:
		/* set preset live time. Convert to centiseconds */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setpresets %d\n", s);
		break;
	case mcaPresetCounts:
		/* set preset counts */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setpresets %d\n", s);
		break;
	case mcaPresetLowChannel:
		/* set lower side of region integrated for preset counts */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setpresets %d\n", s);
		break;
	case mcaPresetHighChannel:
		/* set high side of region integrated for preset counts */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setpresets %d\n", s);
		break;
	case mcaPresetSweeps:
		/* set number of sweeps (for MCS mode) */
		/* This is a NOOP on current version of MCS */
		break;
	case mcaModePHA:
		/* set mode to Pulse Height Analysis */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setup %d\n", s);
		break;
	case mcaModeMCS:
		/* set mode to MultiChannel Scaler */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setup %d\n", s);
		break;
	case mcaModeList:
		/* set mode to LIST (record each incoming event) */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): acqu setup %d\n", s);
		break;
	case mcaReadStatus:
		/* Read the current status of the device */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): status update %d\n", s);
		/* pmca->ertm = ereal/100.0; */
		/* pmca->eltm = elive/100.0; */
		/* pmca->act = etotals; */
		/* pmca->acqg = acq_status; */
		break;
	case mcaStopAcquire:
		/* stop data acquisition */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): stop acquisition %d\n", s);
		break;
	case mcaErase:
		/* erase */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): erase %d\n", s);
		/* Set the elapsed live and real time back to zero. */
		if (devMCA_softDebug > 5) errlogPrintf("(send_msg): set elapsed %d\n", s);
		break;
	}
	return(0);
}

static long read_array(mcaRecord *pmca)
{
	int nuse = pmca->nuse;

	if (devMCA_softDebug > 5) errlogPrintf("(read_array): doing nothing\n");
	pmca->nord = nuse;
	return(0);
}
