/* devSTR7201.c */

/* devSTR7201.c - Device Support Routines for Struck STR7201 multiscaler
 * for EPICS MCA record
 *
 *      Author:  Mark Rivers
 *      Date:    11/9/97
 *
 * Modification Log:
 * -----------------
 * .01  11/09/97  mlr  Developed from AIM device support
 * .02  08/05/99  tmm  Call sendChannelAdvance from initRecord
 * .03  09/12/99  mlr  Changed #include from <mcaRecord.h> to "mcaRecord.h"
 *                     so gnumake depends would get it right
 * .04  12/06/99  mlr  Minor changes to eliminate compiler warnings
 * .05  02/04/00  mlr  Modifications to support V.09 of drvSTR7201.c, support
 *                     separate preset counts for each signal.
 * .06  10/03/00  mlr  Send all setup data each time acquisition is started. This
 *                     allows MCA record and scaler records to use the same hardware
 *                     "simultaneously", i.e. without rebooting the crate
 * .07  23/02/02  mlr  Add support for MSG_SET_SEQ, although this is a NO-OP.
 */


#include        <vxWorks.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <wdLib.h>
#include        <memLib.h>
#include        <string.h>

#include        <alarm.h>
#include        <callback.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <devSup.h>
#include        <link.h>

#include        "mcaRecord.h"
#include        "mca.h"
#include        "drvSTR7201.h"
#include        <epicsExport.h>

#define Debug(l,FMT,V...) {if (l <= devSTR7201Debug) \
                          { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,## V);}}
volatile int devSTR7201Debug = 0;
epicsExportAddress(int, devSTR7201Debug);

#define MAX(a, b) ((a) > (b) ? (a) : (b) )

/* Create DSET */
static long init_record();
static long send_msg();
static long read_array();
static long sendPresets(mcaRecord *pmca);
static long sendChannelAdvance(mcaRecord *pmca);
static long sendSetup(mcaRecord *pmca);

typedef struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        DEVSUPFUN       send_msg;
        DEVSUPFUN       read_array;
} STR7201_DSET;

STR7201_DSET devSTR7201 = {
        6,
        NULL,
        NULL,
        init_record,
        NULL,
        send_msg,
        read_array
};
epicsExportAddress(dset, devSTR7201);

static long init_record(mcaRecord *pmca)
{
    int card;
    Debug(5, "devSTR7201(init_record): entry\n");
    /* pmca->inp must be VME_IO */
    switch (pmca->inp.type) {
    case (VME_IO) :
        card = pmca->inp.value.vmeio.card;
        if (drvSTR7201Init(card) != OK) {
            recGblRecordError(S_db_badField,(void *)pmca,
                "devSTR7201 (init_record) Card not found");
            return(S_db_badField);
        }
        pmca->nord = 0;
        break;
    default :
        recGblRecordError(S_db_badField,(void *)pmca,
                "devSTR7201 (init_record) Illegal INP field");
        return(S_db_badField);
    }
    sendSetup(pmca);
    return(0);
}

static long send_msg(mcaRecord *pmca, unsigned long msg, void *parg)
{
        int card = pmca->inp.value.vmeio.card;
        int signal = pmca->inp.value.vmeio.signal;
        int s, acq_status;
        long ecounts;
        float ereal;

        switch (msg) {
        case mcaStartAcquire:
                /* start acquisition */
                sendSetup(pmca);
                s = drvSTR7201AcqOn(card);
                Debug(5, "devSTR7201(send_msg): start acquisition %d\n", s);
                break;
        case mcaData:
                /* start read operation */
                /* This is a no-op. Read-array does everything. */
                break;
        case mcaChannelAdvanceInternal:
                /* set channel advance source to internal (timed) */
                sendSetup(pmca);
                break;
        case mcaChannelAdvanceExternal:
                /* set channel advance source to external */
                sendSetup(pmca);
                break;
        case mcaNumChannels:
                /* set number of channels */
                sendSetup(pmca);
                break;
        case mcaDwellTime:
                /* set dwell time per channel. */
                sendSetup(pmca);
                break;
        case mcaPresetRealTime:
                /* set preset real time. */
                sendSetup(pmca);
                break;
        case mcaPresetLiveTime:
                /* set preset live time */
                sendSetup(pmca);
                break;
        case mcaPresetCounts:
                /* set preset counts */
                sendSetup(pmca);
                break;
        case mcaPresetLowChannel:
                /* set preset count low channel */
                sendSetup(pmca);
                break;
        case mcaPresetHighChannel:
                /* set preset count high channel */
                sendSetup(pmca);
                break;
        case mcaPresetSweeps:
                /* set number of sweeps (for MCS mode)
                 * This is a NOOP on STR7201 but should be implemented in
                 * software in the future
                 */
                break;
        case mcaModePHA:
                /* set mode to pulse height analysis */
                /* This is a NOOP for STR7201 */
                break;
        case mcaModeMCS:
                /* set mode to MultiChannel Scaler */
                /* This is a NOOP for STR7201 */
                break;
        case mcaModeList:
                /* set mode to LIST (record each incoming event) */
                /* This is a NOOP for STR7201 */
                break;
        case mcaReadStatus:
                /* Read the current status of the device */
                s = drvSTR7201GetAcqStatus(card, signal, &ereal, &ecounts, 
                                                &acq_status);
                Debug(5, "devSTR7201(send_msg): status update %d\n", s);
                pmca->ertm = ereal;
                pmca->eltm = ereal;
                pmca->act = ecounts;
                pmca->acqg = acq_status;
                break;
        case mcaStopAcquire:
                /* stop data acquisition */
                s = drvSTR7201AcqOff(card);
                Debug(5, "devSTR7201(send_msg): stop acquisition %d\n", s);
                break;
        case mcaErase:
                /* erase */
                sendSetup(pmca);
                s = drvSTR7201Erase(card);
                Debug(5, "devSTR7201(send_msg): erase %d\n", s);
                break;
        case mcaSequence:
                /* set sequence for time-resolved spectra */
                /* This is a NOOP for STR7201 */
                break;
        case mcaPrescale:
                /* set channel advance prescaler. */
                sendSetup(pmca);
                break;
        }
        return(0);
}

static long read_array(mcaRecord *pmca)
{
    int card = pmca->inp.value.vmeio.card;
    int signal = pmca->inp.value.vmeio.signal;
    int s, nuse = pmca->nuse;
    long *lptr = (long *)pmca->bptr;

    s = drvSTR7201Read(card, signal, nuse, lptr);
    Debug(5, "devSTR7201(read_array): %d\n", s);
    pmca->nord = nuse;
    return(0);
}

static long sendPresets(mcaRecord *pmca)
{
    int card = pmca->inp.value.vmeio.card;
    int signal = pmca->inp.value.vmeio.signal;
    float ptime;

    ptime = MAX(pmca->prtm, pmca->pltm);
    drvSTR7201SetPresets(card, signal, ptime, pmca->pctl, pmca->pcth, pmca->pct,
        MULTICHANNEL_SCALER_MODE);
    return (OK);
}

static long sendChannelAdvance(mcaRecord *pmca)
{
    int card = pmca->inp.value.vmeio.card;
    int source = LNE_EXTERNAL;
    int prescale = 0;

    if (pmca->chas == mcaCHAS_Internal) source = LNE_INTERNAL;
    if (source == LNE_INTERNAL) {
        prescale = NINT(pmca->dwel * INTERNAL_CLOCK) - 1;
    } else {
        prescale = pmca->pscl - 1;
    }
    drvSTR7201SetChannelAdvance(card, source, prescale);
    return (OK);
}

static long sendSetup(mcaRecord *pmca)
{
    int card = pmca->inp.value.vmeio.card;

    drvSTR7201SetNChans(card, pmca->nuse);
    sendPresets(pmca);
    sendChannelAdvance(pmca);
    return (OK);
}
