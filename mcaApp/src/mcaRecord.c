/* mcaRecord.c - Record Support Routines for MCA records */
/*
 *      Original Author: Tim Mooney
 *      Current Author:  Mark Rivers
 *      Date:            2-22-94
 *
 *      Copyright 1991, the University of Chicago Board of Governors.
 *
 *      This software was produced under  U.S. Government contract
 *      W-31-109-ENG-38 at Argonne National Laboratory.
 *
 * Modification Log:
 * -----------------
 * .01  02-22-94  tmm  Initial development
 * .02  04-03-95  nda  fixed post_monitor on ->bptr
 * .03  07-11-96  tmm  v2.0 added support for regions of interest; allow
 *                     for asynchronous read
 * .04  07-15-96  tmm  v2.2 fixed ROI calcs and posting rules.
 * .05  09-17-96  tmm  v2.3 show ROIs.
 * .06  09-25-96  tmm  v2.4 force read on change of any ROI PV.
 * .06  09-25-96  tmm  v2.5 do misc. ctrl. first instead of last.
 * .07  10-28-96  tmm  v3.0 conversion to EPICS 3.13.
 * .08  11-17-97  mlr  v4.0 Removed ADDR and PORT fields.
 *                          Added SEQ and RMAP fields.
 *                          Removed monitor posting for fields which record
 *                          does not change.
 *                          Put some redundant code in macros.  
 *                          In "process" moved start and stop operations to 
 *                          occur before read status.
 * .09  05-05-98  mlr  v4.1 Changed record to make the timestamp be the time
 *                          when acquisition started, rather than when record
 *                          last processed.
 * .10  09-17-98  mlr  v4.2 Changed logic so ROI changes do not force read,
 *                          since this was too slow when setting many ROIs.
 *                          Instead, compute ROI sums whenever record processes
 *                          since this is a relatively quick operation.
 * .11  09-17-98  tmm  v4.3 Sum ROI's only when needed.  Don't call
 *                          recGblFwdLink() while data acquisition or read is in
 *                          progress, so ca_put_callback() can be used to wait
 *                          for completion of those operations.  Rearranged
 *                          MMAP and NEWV bitmaps.
 * .12  03-17-99  mlr  v4.4 Added PSCL field for prescale factor on MCS
 * .13  03-22-99  mlr  v4.5 Fixed bug in PSCL logic in special
 * .14  04-15-99  mlr  v4.6 Added STIM (start time) field
 * .15  05-07-99  tmm  v4.7 Fixed call to send_msg (only two args)
 * .16  05-16-99  mlr  v4.8 Added RTIM (read time) field.  This is used by 
 *                          clients to accurately compute counts/second.
 *                          Trimmed STIM field so precision is only .001 sec
 * .17  11-4-99   mlr  V4.9 Support asynchronous device support for read-status
 *                          and read-data.  Added RDNS,ACQP,ERTP,ELTP,ACTP,DWLP
 *                          fields.  Allowed device support to change DWEL
 *                          to reflect actual, vs requested, dwell time.
 * .18  08-2-00   mlr  V4.10 Changed logic on ERAS.  Previously a read was
 *                          forced after an erase.  This had serious
 *                          performance problems with the AIM and multi-element
 *                          detectors.  Changed code to erase data inside the
 *                          record and post monitor on the VAL field, rather
 *                          than reading data back from the device.
 * .19  09-06-00  mlr  V5.0 Added ERST (Erase and Start field).  This is more
 *                          efficient and much simpler than doing Erase and
 *                          Start sequentially from a database.
 *                          -Fixed bug in logic of .READ and .RDNG.  .READ could be
 *                          set externally while a .RDNG callback was pending.
 *                          Ignore the .READ in this case, because the record is
 *                          going to process again to handle the .READ.
 *                          Set flags on all fields of interest to device support
 *                          in init_record so that all of these fields will be sent
 *                          to device support the first time the record processes.
 *                          The record should be set to PINI="Yes" in all databases
 *                          so that device support is in sync with the record.
 *                          Changed debugging to use errlogPrintf for legibility
 * .20  07-21-01  mlr  V5.1 Increased number of ROIs from 10 to 32.
 */
#define VERSION 5.1

#include    <vxWorks.h>
#include    <types.h>
#include    <stdioLib.h>
#include    <lstLib.h>
#include    <stdlib.h>
#include    <string.h>

#include    <alarm.h>
#include    <dbDefs.h>
#include    <dbAccess.h>
#include    <dbFldTypes.h>
#include    <dbScan.h>
#include    <dbEvent.h>
#include    <devSup.h>
#include    <errMdef.h>
#include    <errlog.h>
#include    <recSup.h>
#include    <special.h>

#define GEN_SIZE_OFFSET
#include    "mcaRecord.h"
#undef GEN_SIZE_OFFSET
#include    "mca.h"

/* Debug support */
#if 0
#define Debug(l,FMT,V) ;
#else
#define Debug(l,FMT,V...) {if (l <= mcaRecordDebug) \
              { errlogPrintf("%s(%d)(%s):",__FILE__,__LINE__,pmca->name); \
                            errlogPrintf(FMT,##V);}}
#endif
volatile int mcaRecordDebug = 0;

/* Create RSET - Record Support Entry Table*/
#define report NULL
#define initialize NULL
static long init_record();
static long process();
static long special();
#define get_value NULL
static long cvt_dbaddr();
static long get_array_info();
static long put_array_info();
static long get_units();
static long get_precision();
#define get_enum_str NULL
#define get_enum_strs NULL
#define put_enum_str NULL
static long get_graphic_double();
static long get_control_double();
#define get_alarm_double NULL

struct rset mcaRSET={
    RSETNUMBER,
    report,
    initialize,
    init_record,
    process,
    special,
    get_value,
    cvt_dbaddr,
    get_array_info,
    put_array_info,
    get_units,
    get_precision,
    get_enum_str,
    get_enum_strs,
    put_enum_str,
    get_graphic_double,
    get_control_double,
    get_alarm_double };

struct mcaDSET { /* mca DSET */
    long            number;
    DEVSUPFUN       dev_report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN       get_ioint_info;
    DEVSUPFUN       send_msg; /*returns: (-1,0)=>(failure,success)*/
    DEVSUPFUN       read_array; /*returns: (-1,0)=>(failure,success)*/
};

/*sizes of field types (see dbFldTypes.h) */
static int sizeofTypes[] = {0,1,1,2,2,4,4,4,8};

static void monitor();
static long readValue();

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

/* Support for Regions Of Interest */
struct roi {
    long lo;
    long hi;
    short nbg;
    unsigned short isPreset;
};
/* The number of fields per ROI above */
#define FIELDS_PER_ROI 4
struct roiSum {
    double sum;
    double net;
    double preset;
};
static long sum_ROIs(mcaRecord *pmca, short *preset_reached);
#define NUM_ROI 32

/*******************************************************************************
Support for keeping track of which record fields have been changed, so we can
eliminate redundant db_post_events() without having to think, and without having
to keep lots of "last value of field xxx" fields in the record.  The idea is
to say...

    MARK(M_XXXX);

when you mean...

    db_post_events(pmr, &pmr->xxxx, monitor_mask);

Before leaving, you have to call post_MARKed_fields() to actually post the
field to all listeners.  monitor() does this.

    --- NOTE WELL ---
    The macros below assume that the variable "pmca" exists and points to a
    mca record, like so:
        mcaRecord *pmca;
    No check is made in this code to ensure that this really is true.
*******************************************************************************/

/* These bits are in the mmap field only.  MMAP records fields whose
 * values must be posted.
 */
#define M_VAL       0x00000001
#define M_NACK      0x00000002
#define M_BG        0x00000004
#define M_ACQG      0x00000008
#define M_READ      0x00000010
#define M_RDNG      0x00000020
#define M_ERTM      0x00000040
#define M_ELTM      0x00000080
#define M_ACT       0x00000100
#define M_STRT      0x00000200
#define M_STOP      0x00000400
#define M_STIM      0x00000800
#define M_RTIM      0x00001000
#define M_RDNS      0x00002000

/* These bits are in the mmap and newv fields */
#define M_ERAS      0x00004000
#define M_CHAS      0x00008000
#define M_NUSE      0x00010000
#define M_DWEL      0x00020000
#define M_PRTM      0x00040000
#define M_PLTM      0x00080000
#define M_PCT       0x00100000
#define M_PCTL      0x00200000
#define M_PCTH      0x00400000
#define M_PSWP      0x00800000
#define M_MODE      0x01000000
#define M_SEQ       0x02000000
#define M_PSCL      0x04000000
#define M_ERST      0x08000000


/* These bits are in the rmap and newr fields.  RMAP records ROI fields whose
 * values must be posted.  NEWR records ROI fields whose values are new and for
 * which processing is required.
 */
#define M_R0        0x00000001
#define M_R1        0x00000002
#define M_R2        0x00000004
#define M_R3        0x00000008
#define M_R4        0x00000010
#define M_R5        0x00000020
#define M_R6        0x00000040
#define M_R7        0x00000080
#define M_R8        0x00000100
#define M_R9        0x00000200
#define M_R10       0x00000400
#define M_R11       0x00000800
#define M_R12       0x00001000
#define M_R13       0x00002000
#define M_R14       0x00004000
#define M_R15       0x00008000
#define M_R16       0x00010000
#define M_R17       0x00020000
#define M_R18       0x00040000
#define M_R19       0x00080000
#define M_R20       0x00100000
#define M_R21       0x00200000
#define M_R22       0x00400000
#define M_R23       0x00800000
#define M_R24       0x01000000
#define M_R25       0x02000000
#define M_R26       0x04000000
#define M_R27       0x08000000
#define M_R28       0x10000000
#define M_R29       0x20000000
#define M_R30       0x40000000
#define M_R31       0x80000000
#define M_ROI_ALL   0xFFFFFFFF


#define MARK(a)     pmca->mmap |= (a);
#define MARKED(a)   (pmca->mmap & (a))
#define UNMARK(a)   pmca->mmap &= ~(a);
#define UNMARK_ALL  pmca->mmap = 0;

#define ROI_MARK(a) pmca->rmap |= (a);
#define ROI_MARKED(a)   (pmca->rmap & (a))
#define ROI_UNMARK(a)   pmca->rmap &= ~(a);
#define ROI_UNMARK_ALL  pmca->rmap = 0;

#define NEWV_MARK(a)    pmca->newv |= (a);
#define NEWV_MARKED(a)  (pmca->newv & (a))
#define NEWV_UNMARK(a)  pmca->newv &= ~(a);
#define NEWV_UNMARK_ALL pmca->newv = 0;

#define NEWR_MARK(a)    pmca->newr |= (a);
#define NEWR_MARKED(a)  (pmca->newr & (a))
#define NEWR_UNMARK(a)  pmca->newr &= ~(a);
#define NEWR_UNMARK_ALL pmca->newr = 0;

/* The "process" function in this record performs the following steps:
   1) Examine record to see what fields which control MCA setup have changed
      and send the approriate commands to device support (e.g. set preset live
      or real time, erase spectrum, turn acquisition on and off, etc.)
   2) Send command to read device status (MSG_GET_ACQ_STATUS)
      If device support sets .rdns=1 then return immediately
      2a) If .acqg makes a 1 to 0 transition (acquisition stopped) then set
          .read=1
   3) If .read=1 or .rdns=1 then do the following
      3a) If .read=1 then send command to read spectrum data (MSG_READ)
          If device support sets .rdng=1 then return immediately
      3b) Call device support routine to read the data (read_array)
   4) Sum ROIS if there is new data, check for presets reached
   5) Post monitors on new fields
   6) If .acqg=0 then process forward-linked record

   On entry to "process" only the following 4 states or combinations of the
   .read, .rdng, and .rdns fields are allowed (X=don't care):
   State   .read   .rdns    .rdng    Description
    (1)      0       0        0     Normal record process, go to step 1, read
                                    device status only, skip step 3
    (2)      1       0        0     Normal record process, go to step 1, read
                                    device status and data
    (3)      X       1        0     Callback from device support when read
                                    status completes.  Go to step 2a.
    (4)      X       0        1     Callback from device support when read
                                    data completes. Go to step 3b.

   Note that device support is allowed to do callbacks for MSG_READ and
   MSG_GET_ACQ_STATUS, which are the only commands which return data from
   the device to the record.  All other device support messages must be handled
   without a callback to record support.
   
   If .strt=1 or .erst=1 then a message is sent to device support to start 
   acquisition (MSG_ACQUIRE)
   If .stop=1 then a message is sent to device support to stop acquisition
   (MSG_STOP_ACQUISITION)
*/

/* The following macro is long, but it saves 6 pages of repetitive code */
#define PROCESS_ROI(DATA_TYPE) \
{\
    DATA_TYPE *pdat = (DATA_TYPE *)pmca->bptr;\
    for (i=0; i<max; i++) ymax = MAX(ymax, pdat[i]);\
}\
    for (i=0; i<NUM_ROI; i++, proi++, psum++) {\
        DATA_TYPE *p, *pdat = (DATA_TYPE *)pmca->bptr;\
        DATA_TYPE *pb, *pbg = (DATA_TYPE *)pmca->pbg;\
        DATA_TYPE *plo, *phi;\
        sum = net = 0.0;\
        lo = proi->lo; hi = proi->hi;\
        if (lo >= 0 && hi >= lo) {\
            bg_lo = bg_hi = 0.0;\
            if (proi->nbg >= 0) {\
                n = proi->nbg;\
                /* get lo-side background average */\
                plo = &pdat[MAX(lo-n, 0)];\
                phi = &pdat[MIN(lo+n, max)];\
                for (p=plo; p<=phi; p++) bg_lo += *p;\
                bg_lo /= 2*n + 1;\
                /* get hi-side background average */\
                plo = &pdat[MAX(hi-n, 0)];\
                phi = &pdat[MIN(hi+n, max)];\
                for (p=plo; p<=phi; p++) bg_hi += *p;\
                bg_hi /= 2*n + 1;\
            }\
            /* sum ROI */\
            n = hi - lo;\
            plo = &pdat[lo];\
            for (j=0, p=plo, pb=&pbg[lo]; j<=n; j++, p++, pb++) {\
                sum += *p;\
                if (proi->nbg >= 0)\
                    *pb = bg_lo + (n ? j*(bg_hi-bg_lo)/n : 0); /* linear */\
                net += *p - *pb;\
            }\
            MARK(M_BG);\
    }\
    if ((sum != psum->sum) || (net != psum->net)) ROI_MARK(M_R0<<i);\
    psum->sum = sum;\
    psum->net = net;\
    if (proi->isPreset) *preset_reached |= psum->net >= psum->preset;\
    NEWR_UNMARK(M_R0<<i);\
    }\
\
    proi = (struct roi *)&pmca->r0lo;\
    for (i=0; i<NUM_ROI; i++, proi++) {\
        DATA_TYPE *pbg = (DATA_TYPE *)pmca->pbg;\
        if (proi->lo >= 0 && proi->hi >= proi->lo) {\
            pbg[proi->lo] = pbg[proi->hi] = ymax;\
    }\
    }\



static long init_record(mcaRecord *pmca, int pass)
{
    struct mcaDSET *pdset;
    long status;

    /* Allocate memory for spectrum */
    if (pass==0) {
        pmca->vers = VERSION;
        if (pmca->nmax <= 0) pmca->nmax=1;
        if (pmca->ftvl == 0) {
            pmca->bptr = (char *)calloc(pmca->nmax,MAX_STRING_SIZE);
            pmca->pbg = (char *)calloc(pmca->nmax,MAX_STRING_SIZE);
        } else {
            if (pmca->ftvl > DBF_DOUBLE) pmca->ftvl=2;
            pmca->bptr = (char *)calloc(pmca->nmax,sizeofTypes[pmca->ftvl]);
            pmca->pbg = (char *)calloc(pmca->nmax,sizeofTypes[pmca->ftvl]);
        }
        pmca->nord = 0;
        return(0);
    }

    /* simulation links */
    if (pmca->siml.type == CONSTANT) {
        recGblInitConstantLink(&pmca->siml, DBF_USHORT, &pmca->simm);
    }
    if (pmca->siol.type ==  CONSTANT) {
        pmca->nord = 0;
    }

    /* must have dset defined */
    if (!(pdset = (struct mcaDSET *)(pmca->dset))) {
        recGblRecordError(S_dev_noDSET,(void *)pmca,"mca: init_record1");
        return(S_dev_noDSET);
    }
    /* must have read_array function defined */
    if ( (pdset->number < 6) || (pdset->read_array == NULL) ) {
        recGblRecordError(S_dev_missingSup,(void *)pmca,"mca: init_record2");
        printf("%ld %p\n",pdset->number, pdset->read_array);
        return(S_dev_missingSup);
    }
    if (pdset->init_record) {
        if ((status=(*pdset->init_record)(pmca))) return(status);
    }
    /* Mark all device-dependent fields in record as changed so the device support
     * will be initialized to agree with the record the first time the record
     * processes */
    NEWV_MARK(M_CHAS);
    NEWV_MARK(M_NUSE);
    NEWV_MARK(M_DWEL);
    NEWV_MARK(M_PRTM);
    NEWV_MARK(M_PLTM);
    NEWV_MARK(M_PCT);
    NEWV_MARK(M_PCTL);
    NEWV_MARK(M_PCTH);
    NEWV_MARK(M_PSWP);
    NEWV_MARK(M_MODE);
    NEWV_MARK(M_SEQ);
    NEWV_MARK(M_PSCL);
    return(0);
}

static long process(mcaRecord *pmca)
{
    struct mcaDSET *pdset = (struct mcaDSET *)(pmca->dset);
    long status;
    short preset_reached = 0;
    int rdns;

    /*** Check existence of device support ***/
    if ( (pdset==NULL) || (pdset->read_array==NULL) ) {
        pmca->pact=TRUE;
        recGblRecordError(S_dev_missingSup,(void *)pmca,"read_array");
        return(S_dev_missingSup);
    }

    /* init Not ACKnowledged flag */
    if (pmca->nack) {
       Debug(1,"process: clearing NACK field\n")
       pmca->nack = 0; 
       MARK(M_NACK);
    }

    /*** Handle miscellaneous control fields ***
    * Note that we are allowing these values to be changed even
    * if acquisition is in progress. It is up to device support to
    * prohibit this if necessary.
    */
    Debug(2,"process: entry, rdng=%d, rdns=%d, read=%d\n", 
                  pmca->rdng, pmca->rdns, pmca->read);
    if (pmca->rdns) goto read_status;
    if (pmca->rdng) goto read_data;
    if (pmca->newv) {
        if (NEWV_MARKED(M_CHAS)) {
            MARK(M_CHAS);
            status = (*pdset->send_msg)
                (pmca, (pmca->chas==mcaCHAS_Internal) ?
                MSG_SET_CHAS_INT : MSG_SET_CHAS_EXT, NULL);
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_CHAS);
        }
        if (NEWV_MARKED(M_NUSE)) {
            MARK(M_NUSE);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_NCHAN, (void *)(&pmca->nuse));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_NUSE);
        }
        if (NEWV_MARKED(M_SEQ)) {
            MARK(M_SEQ);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_SEQ, (void *)(&pmca->seq));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_SEQ);
        }
        if (NEWV_MARKED(M_DWEL)) {
            MARK(M_DWEL);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_DWELL, (void *)(&pmca->dwel));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_DWEL);
        }
        if (NEWV_MARKED(M_PSCL)) {
            MARK(M_PSCL);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_PSCL, (void *)(&pmca->pscl));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PSCL);
        }
        if (NEWV_MARKED(M_PRTM)) {
            MARK(M_PRTM);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_REAL_TIME, (void *)(&pmca->prtm));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PRTM);
        }
        if (NEWV_MARKED(M_PLTM)) {
            MARK(M_PLTM);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_LIVE_TIME, (void *)(&pmca->pltm));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PLTM);
        }
        if (NEWV_MARKED(M_PCT)) {
            MARK(M_PCT);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_COUNTS, (void *)(&pmca->pct));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PCT);
        }
        if (NEWV_MARKED(M_PCTL)) {
            MARK(M_PCTL);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_LO_CHAN, (void *)(&pmca->pctl));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PCTL);
        }
        if (NEWV_MARKED(M_PCTH)) {
            MARK(M_PCTH);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_HI_CHAN, (void *)(&pmca->pcth));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PCTH);
        }
        if (NEWV_MARKED(M_PSWP)) {
            MARK(M_PSWP);
            status = (*pdset->send_msg)
                (pmca,  MSG_SET_NSWEEPS, (void *)(&pmca->pswp));
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_PSWP);
        }
        if (NEWV_MARKED(M_ERAS) || NEWV_MARKED(M_ERST)) {
            status = (*pdset->send_msg)
                (pmca,  MSG_ERASE, NULL);
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            if (NEWV_MARKED(M_ERAS)) {
               pmca->eras = 0;
               MARK(M_ERAS);
               NEWV_UNMARK(M_ERAS);
            }
            /* Use TimeStamp to record beginning of acquisition */
            recGblGetTimeStamp(pmca);
            tsStampToText(&pmca->time, TS_TEXT_MONDDYYYY,
                           pmca->stim);
            /* Trim STIM to 25 characters = .001 sec precision */
            pmca->stim[25]='\0';
            MARK(M_STIM);
            /* Erase the data array.  Do this inside the record rather than
             * forcing a read from device support for perfomance reasons. */
            memset(pmca->bptr, 0, pmca->nuse*sizeofTypes[pmca->ftvl]);
            MARK(M_VAL);           /* Post monitor on VAL field */
            NEWR_MARK(M_ROI_ALL);  /* Mark all ROI's for recalculation. */
        }
        if (NEWV_MARKED(M_MODE)) {
            MARK(M_MODE);
            switch (pmca->mode) {
            case mcaMODE_PHA:
            default:
                status = (*pdset->send_msg) (pmca, MSG_SET_MODE_PHA, NULL);
                break;
            case mcaMODE_MCS:
                status = (*pdset->send_msg) (pmca, MSG_SET_MODE_MCS, NULL);
                break;
            case mcaMODE_List:
                status = (*pdset->send_msg) (pmca, MSG_SET_MODE_LIST, NULL);
                break;
            }
            if (status) {pmca->nack = 1; MARK(M_NACK);}
            NEWV_UNMARK(M_MODE);
        }
    }

    /* Turn acquisition on or off.  Do this before reading device status */
    if (pmca->strt || NEWV_MARKED(M_ERST)) {
       Debug(5,"process: start acquisition.\n");
       status = (*pdset->send_msg) (pmca, MSG_ACQUIRE, NULL);
       if (pmca->strt) {
          pmca->strt=0; 
          MARK(M_STRT);
       }
       if (NEWV_MARKED(M_ERST)) {
          pmca->erst = 0;
          MARK(M_ERST);
          NEWV_UNMARK(M_ERST);
       }
       /* Use TimeStamp to record beginning of acquisition */
       /* Don't reset the clock if we are already acquiring */
       if (!pmca->acqg) {
          recGblGetTimeStamp(pmca);
          tsStampToText(&pmca->time, TS_TEXT_MONDDYYYY, pmca->stim);
          /* Trim STIM to 25 characters = .001 sec precision */
          pmca->stim[25]='\0';
          MARK(M_STIM);
       }
    }
    if (pmca->stop) {
       Debug(5,"process: stop acquisition.\n");
       status = (*pdset->send_msg) (pmca, MSG_STOP_ACQUISITION, NULL);
       pmca->stop=0; MARK(M_STOP);
    }


    /* Handle read status cycle
     * This includes the elapsed live time, elapsed real time, actual counts
     * in preset region and the current acquisition status. 
     */
read_status:
    /* Save fields updated by MSG_GET_ACQ_STATUS */
    pmca->acqp = pmca->acqg;
    pmca->ertp = pmca->ertm;
    pmca->eltp = pmca->eltm;
    pmca->actp = pmca->act;
    pmca->dwlp = pmca->dwel;
    rdns = pmca->rdns;  /* Save current state of rdns */

    Debug(5,"process: reading elapsed time, etc.\n");
    status = (*pdset->send_msg)(pmca, MSG_GET_ACQ_STATUS, NULL);
    if (status) {
       Debug(1,"process: error sending MSG_GET_ACQ_STATUS\n");
       pmca->nack = 1; MARK(M_NACK);
       pmca->acqg = 0;
    } else {
       /* If read status requires a callback, device support will 
        * tell us so by setting pmca->rdns = 1, and we'll do nothing 
        * until we get processed again.  Otherwise, we can get the 
        * data immediately.
        */
       if (!rdns && pmca->rdns) {
          Debug(5,"process: waiting for read status callback.\n");
          MARK(M_RDNS);
          return(0);   /* Exit and wait for callback */
       }
    } 
    pmca->rdns = 0;

    if (pmca->ertm != pmca->ertp) MARK(M_ERTM);
    if (pmca->eltm != pmca->eltp) MARK(M_ELTM);
    if (pmca->act  != pmca->actp) MARK(M_ACT);
    if (pmca->dwel != pmca->dwlp) MARK(M_DWEL);

    Debug(5,"process: acqg=%d, acqp=%d\n", pmca->acqg, pmca->acqp);
    if (pmca->acqg != pmca->acqp) {
       MARK(M_ACQG);
       /* Force a read when acquire turns off */
       if (!pmca->acqg) {
          pmca->read = 1; MARK(M_READ);
       }
    }

read_data:
    /* Handle read data cycle.
     * If pmca->rdng == 1 then this is a callback from device support, 
     * get data */
    if (pmca->rdng) {
       Debug(5,"process: get data\n");
       status = readValue(pmca); /* read the new value */
       if (status) {
          Debug(1,"process: error reading data\n");
          pmca->nack = 1; MARK(M_NACK);
       } else {
          MARK(M_VAL);
       }
       pmca->rdng = 0; MARK(M_RDNG);
    } else if (pmca->read) {
        /* Start a read */
       Debug(5,"process: initiate read.\n");
       /* Record the time when read was begun */
       recGblGetTimeStamp(pmca);
       pmca->rtim = pmca->time.secPastEpoch + pmca->time.nsec*1.e-9;
       MARK(M_RTIM);
       status = (*pdset->send_msg)(pmca, MSG_READ, NULL);
       pmca->read = 0; MARK(M_READ);
       if (status) {
          /* no ack from device support; go quiescent */
          pmca->nack = 1; MARK(M_NACK);
       } 
       /*
        * If read requires a callback, device support will tell us so
        * by setting pmca->rdng = 1, and we'll do nothing until we get
        * processed again.  Otherwise, we can get the data.
        */
        if (pmca->rdng) {
            Debug(5,"process: waiting for read data callback.\n");
            MARK(M_RDNG);
            return(0);   /* Exit and wait for callback */
        } else {
            /* Data available immediately, get it */
            Debug(5,"process: get data\n");
            status = readValue(pmca); /* read the new value */
            if (status) {
                pmca->nack = 1; MARK(M_NACK);
            } else {
                MARK(M_VAL);
            }
        }
    }

    /* If any ROI is marked, sumROIs */
    if (NEWR_MARKED(M_ROI_ALL)) {
        (void)sum_ROIs(pmca, &preset_reached);
    }

    if (preset_reached) {
        Debug(5,"process: stop acquisition.\n");
        status = (*pdset->send_msg) (pmca, MSG_STOP_ACQUISITION, NULL);
    }

    pmca->udf = FALSE;

    monitor(pmca);

    /*
     * Process forward-linked record.  Tell EPICS dbPutNotify mechanism
     * that processing is finished.
     */
    if (!pmca->acqg) recGblFwdLink(pmca);

    pmca->pact=FALSE;
    return(0);
}

static long cvt_dbaddr(struct dbAddr *paddr)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == mcaRecordVAL) {
        paddr->pfield = (void *)(pmca->bptr);
    } else if (fieldIndex == mcaRecordBG) {
        paddr->pfield = (void *)(pmca->pbg);
    }
    paddr->no_elements = pmca->nmax;
    paddr->field_type = pmca->ftvl;
    if (pmca->ftvl==0)  paddr->field_size = MAX_STRING_SIZE;
    else paddr->field_size = sizeofTypes[pmca->ftvl];
    paddr->dbr_field_type = pmca->ftvl;
    return(0);
}

static long get_array_info(struct dbAddr *paddr, long *no_elements, long *offset)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;

    *no_elements =  pmca->nord;
    *offset = 0;
    return(0);
}

static long put_array_info(struct dbAddr *paddr, long nNew)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;

    pmca->nord = nNew;
    if (pmca->nord > pmca->nmax) pmca->nord = pmca->nmax;
    return(0);
}

static long get_units(struct dbAddr *paddr, char *units)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;

    strncpy(units,pmca->egu,sizeof(pmca->egu));
    return(0);
}

static long get_precision(struct dbAddr *paddr, long *precision)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;

    *precision = pmca->prec;
    if (paddr->pfield==(void *)pmca->bptr) return(0);
    recGblGetPrec(paddr,precision);
    return(0);
}

static long get_graphic_double(struct dbAddr *paddr, struct dbr_grDouble *pgd)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == mcaRecordBPTR) {
        pgd->upper_disp_limit = pmca->hopr;
        pgd->lower_disp_limit = pmca->lopr;
    } else recGblGetGraphicDouble(paddr,pgd);
    return(0);
}
static long get_control_double(paddr,pcd)
struct dbAddr *paddr;
struct dbr_ctrlDouble *pcd;
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;
    int fieldIndex = dbGetFieldIndex(paddr);

    if (fieldIndex == mcaRecordBPTR) {
        pcd->upper_ctrl_limit = pmca->hopr;
        pcd->lower_ctrl_limit = pmca->lopr;
    } else recGblGetControlDouble(paddr,pcd);
    return(0);
}

static void monitor(mcaRecord *pmca)
{
    unsigned short monitor_mask;
    struct roiSum *psum = (struct roiSum *)&pmca->r0;
    int i;

    monitor_mask = recGblResetAlarms(pmca);
    monitor_mask |= (DBE_VALUE|DBE_LOG);
    if (MARKED(M_VAL)) db_post_events(pmca,pmca->bptr,monitor_mask);
    if (MARKED(M_BG))   db_post_events(pmca,pmca->pbg,monitor_mask);
    if (MARKED(M_NACK)) db_post_events(pmca,&pmca->nack,monitor_mask);
    if (MARKED(M_ACQG)) db_post_events(pmca,&pmca->acqg,monitor_mask);
    if (MARKED(M_READ)) db_post_events(pmca,&pmca->read,monitor_mask);
    if (MARKED(M_RDNG)) db_post_events(pmca,&pmca->rdng,monitor_mask);
    if (MARKED(M_ERTM)) db_post_events(pmca,&pmca->ertm,monitor_mask);
    if (MARKED(M_ELTM)) db_post_events(pmca,&pmca->eltm,monitor_mask);
    if (MARKED(M_ACT )) db_post_events(pmca,&pmca->act ,monitor_mask);
    if (MARKED(M_STRT)) db_post_events(pmca,&pmca->strt,monitor_mask);
    if (MARKED(M_STOP)) db_post_events(pmca,&pmca->stop,monitor_mask);
    if (MARKED(M_STIM)) db_post_events(pmca,&pmca->stim,monitor_mask);
    if (MARKED(M_RTIM)) db_post_events(pmca,&pmca->rtim,monitor_mask);
    if (MARKED(M_DWEL)) db_post_events(pmca,&pmca->dwel,monitor_mask);
    
    for (i=0; i<NUM_ROI; i++) {
       if (ROI_MARKED(M_R0 << i)) {
          db_post_events(pmca,&psum[i].sum ,monitor_mask);
          db_post_events(pmca,&psum[i].net ,monitor_mask);
       }
    }
    UNMARK_ALL;
    ROI_UNMARK_ALL;
    return;

}

static long special(struct dbAddr *paddr, int after)
{
    mcaRecord *pmca=(mcaRecord *)paddr->precord;
    int i, fieldIndex = dbGetFieldIndex(paddr);

    if (!after) return(0);

    /* new dir */
    switch (fieldIndex) {
    case mcaRecordERAS: NEWV_MARK(M_ERAS); break;
    case mcaRecordERST: NEWV_MARK(M_ERST); break;
    case mcaRecordCHAS: NEWV_MARK(M_CHAS); break;
    case mcaRecordNUSE: NEWV_MARK(M_NUSE); break;
    case mcaRecordSEQ:  NEWV_MARK(M_SEQ); break;
    case mcaRecordDWEL: NEWV_MARK(M_DWEL); break;
    case mcaRecordPSCL: NEWV_MARK(M_PSCL); break;
    case mcaRecordPRTM: NEWV_MARK(M_PRTM); break;
    case mcaRecordPLTM: NEWV_MARK(M_PLTM); break;
    case mcaRecordPCT:  NEWV_MARK(M_PCT); break;
    case mcaRecordPCTL: NEWV_MARK(M_PCTL); break;
    case mcaRecordPCTH: NEWV_MARK(M_PCTH); break;
    case mcaRecordPSWP: NEWV_MARK(M_PSWP); break;
    case mcaRecordMODE: NEWV_MARK(M_MODE); break;
    default:
        if ((fieldIndex >= mcaRecordR0LO) && 
            (fieldIndex <= mcaRecordR0IP + NUM_ROI*FIELDS_PER_ROI)) {
            /* Which ROI is affected? */
            i = (fieldIndex - mcaRecordR0LO)/FIELDS_PER_ROI;
            /* Mark the ROI for recalculation. */
            NEWR_MARK(M_R0 << i);
        }
        break;
    }

    /* Do we need to read the device after command gets executed?
     * Ideally we would like to force a read after changing number of
     * channels, erasing or changing sequence.  Changing
     * number of channels is typically done infrequently so forcing a read
     * is OK in that case too.  However, changing sequence is typically
     * done for time-resolved spectroscopy, so performance really matters.
     * Erasing is done frequently during scanning, so performance is also
     * important, so we don't want the overhead of reading the MCA after an
     * erase. (This was done in an earlier version of record).
     * Thus, we don't force a read on change of seq field or erase.
     */
    if (NEWV_MARKED(M_NUSE)) {
        pmca->read = 1;
        MARK(M_READ);
    }
    return(0);
}

static long readValue(mcaRecord *pmca)
{
    long status;
    struct mcaDSET *pdset = (struct mcaDSET *) (pmca->dset);
    long nRequest = 1;

    NEWR_MARK(M_ROI_ALL);  /* Mark all ROI's for recalculation. */

    status = dbGetLink(&(pmca->siml), DBR_ENUM, &(pmca->simm), NULL, NULL);
    if (status) return(status);

    if (pmca->simm == NO) {
        status = (*pdset->read_array)(pmca);
        return(status);
    }
    if (pmca->simm == YES) {
        nRequest = pmca->nmax;
        status = dbGetLink(&(pmca->siol), pmca->ftvl, pmca->bptr,NULL, &nRequest);
        /* nord set only for db links: needed for old db_access */
        if (pmca->siol.type == DB_LINK) pmca->nord = nRequest;
        if (status == 0) {
            pmca->udf = FALSE;
        }
    } else {
        status=-1;
        recGblSetSevr(pmca,SOFT_ALARM,INVALID_ALARM);
        return(status);
    }
    recGblSetSevr(pmca,SIMM_ALARM,pmca->sims);

    return(status);
}


static long sum_ROIs(mcaRecord *pmca, short *preset_reached)
{
    int i, j, n, max, lo, hi;
    double sum, bg_lo, bg_hi, net, ymax=0.0;
    struct roi *proi = (struct roi *)&pmca->r0lo;
    struct roiSum *psum = (struct roiSum *)&pmca->r0;

    Debug(5,"sum_ROIs: entry\n");
    (void)memset(pmca->pbg, 0, pmca->nmax*sizeofTypes[pmca->ftvl]);
    *preset_reached = 0;
    max = pmca->nord-1;

    switch (pmca->ftvl) {
    case DBF_CHAR:
    case DBF_UCHAR:
    default:
        break;

    case DBF_SHORT:
        {PROCESS_ROI(short);
         break;
        }
    case DBF_USHORT:
        {PROCESS_ROI(unsigned short);
         break;
        }
    case DBF_LONG:
        {PROCESS_ROI(long);
         break;
        }
    case DBF_ULONG:
        {PROCESS_ROI(unsigned long);
         break;
        }

    case DBF_FLOAT:
        {PROCESS_ROI(float);
         break;
        }
    case DBF_DOUBLE:
        {PROCESS_ROI(double);
         break;
        }
    }
    return(0);
}
