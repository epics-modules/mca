/*******************************************************************************
devScalerSTR7201.c
Device-support routines for scaler record for Struck 7201 and SIS 38xx 
32-channel, 32-bit scalers

Original Author: Mark Rivers, based on devScaler.c by Tim Mooney
Date: 3/25/00

-----------------
.01  3/25/00    mlr     Based on devScaler.c by Tim Mooney
.02  9/28/00    mlr     Set PACT=1 in initRecord if device not found
.03  10/03/00   mlr     Clear preset counts and reset channel advance time in reset()
.04  10/24/00   mlr     Read number of channels to use from driver

*******************************************************************************/
/* version 1.0 */
#include        <vxWorks.h>
#include        <vme.h>
#include        <types.h>
#include        <stdlib.h>
#include        <stdioLib.h>
#include        <iv.h>
#include        <wdLib.h>

#include        <devLib.h>
#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include        <dbCommon.h>
#include        <dbScan.h>
#include        <recSup.h>
#include        <devSup.h>
#include        <drvSup.h>
#include        "scalerRecord.h"
#include        "devScaler.h"
#include        "drvSTR7201.h"

/* The channel advance rate of the module in seconds */
#define CHANNEL_ADVANCE_RATE 0.01

/*** Debug support ***/
#define PRIVATE_FUNCTIONS 1     /* normal:1, debug:0 */
#if PRIVATE_FUNCTIONS
#define STATIC static
#else
#define STATIC
#endif
#ifdef NODEBUG
#define Debug(l,f,v) ;
#else
#define Debug(l,FMT,V...) {  if(l <= devScalerSTR7201Debug) \
                        { printf("%s(%d):",__FILE__,__LINE__); \
                          printf(FMT,## V); } }
#endif
volatile int devScalerSTR7201Debug=0;
extern int STR7201NumCards;  /* Defined in drvStr7201.c */

STATIC long scaler_report(int level);
STATIC long scaler_init(int after);
STATIC long scaler_init_record(struct scalerRecord *psr);
STATIC long scaler_reset(int card);
STATIC long scaler_read(int card, long *val);
STATIC long scaler_write_preset(int card, int signal, long val);
STATIC long scaler_arm(int card, int val);
STATIC long scaler_done(int card);

SCALERDSET devScalerSTR7201 = {
    7,
    scaler_report,
    scaler_init,
    scaler_init_record,
    NULL,
    scaler_reset,
    scaler_read,
    scaler_write_preset,
    scaler_arm,
    scaler_done
};

STATIC int scaler_total_cards;
struct scaler_state {
    int num_channels;
    int prev_counting;
    int done;
    struct callback *pcallback;
};

STATIC struct scaler_state **scaler_state = 0;

/**************************************************
* scaler_report()
***************************************************/
STATIC long scaler_report(int level)
{
    return (OK);
}


/***************************************************
* initialize all software and hardware
* scaler_init()
****************************************************/
STATIC long scaler_init(int after)
{
    int card;
    
    Debug(2,"scaler_init(): entry, after = %d\n", after);
    if (after) return(0);

    /* allocate scaler_state structures, array of pointers */
    if (scaler_state == NULL) {
        scaler_total_cards = STR7201NumCards;
        scaler_state = (struct scaler_state **)
                            calloc(1, scaler_total_cards * 
                                sizeof(struct scaler_state *));
        for (card=0; card<scaler_total_cards; card++) {
            scaler_state[card] = (struct scaler_state *)
                    calloc(1, sizeof(struct scaler_state));
        }
    }
    return(OK);
}

/***************************************************
* scaler_init_record()
****************************************************/
STATIC long scaler_init_record(struct scalerRecord *psr)
{
    int card = psr->out.value.vmeio.card;
    struct callback *pcallbacks;
    int maxSignals, maxChans, ch1RefEnable;

    Debug(5,"scaler_init_record: card %d\n", card);

    /* out must be an VME_IO */
    if (psr->out.type != VME_IO) {
        recGblRecordError(S_dev_badBus,(void *)psr,
            "devScaler (init_record) Illegal OUT Bus Type");
        psr->pact = 1;
        return(S_dev_badBus);
    }

    if (drvSTR7201Init(card) != OK) {
        recGblRecordError(S_dev_badCard,(void *)psr,
            "devScaler (init_record) card does not exist!");
        psr->pact = 1;
        return(S_dev_badCard);
    }

    scaler_state[card]->prev_counting = 0;

    drvSTR7201GetConfig(card, &maxSignals, &maxChans, &ch1RefEnable);
    if (maxSignals > MAX_SCALER_CHANNELS) maxSignals = MAX_SCALER_CHANNELS;
    scaler_state[card]->num_channels = maxSignals;
    psr->nch = scaler_state[card]->num_channels;

    scaler_reset(card);

    pcallbacks = (struct callback *)psr->dpvt;
    scaler_state[card]->pcallback = (struct callback *)&(pcallbacks[3]);

    return(OK);
}


/***************************************************
* scaler_reset()
****************************************************/
STATIC long scaler_reset(int card)
{
    int signal;

    Debug(5,"scaler_reset: card %d\n", card);
    if ((card+1) > scaler_total_cards) return(ERROR);

    /* clear hardware-done flag */
    scaler_state[card]->done = 0;

    drvSTR7201SetNChans(card, 1);
    for (signal=0; signal<scaler_state[card]->num_channels; signal++) {
        drvSTR7201SetPresets(card, signal, 0., 0, 0, 0, SIMPLE_SCALER_MODE);
    }
    drvSTR7201SetChannelAdvance(card, LNE_INTERNAL, 
            NINT(CHANNEL_ADVANCE_RATE*INTERNAL_CLOCK)-1);

    return(OK);
}

/***************************************************
* scaler_read()
* return pointer to array of scaler values (on the card)
****************************************************/
STATIC long scaler_read(int card, long *val)
{
    int i;
    float etime;
    long ecounts;
    int counting;

    Debug(5,"scaler_read: card %d\n", card);
    if ((card+1) > scaler_total_cards) return(ERROR);
    for (i=0; i < scaler_state[card]->num_channels; i++) {
        drvSTR7201Read(card, i, 1, &val[i]);
        Debug(10,"scaler_read: ...(chan %d = %ld)\n", i, val[i]);
    }

    /* See if acquisition has completed.  If so, issue callback request */
    drvSTR7201GetAcqStatus(card, 0, &etime, &ecounts, &counting);
    Debug(8,"scaler_read: counting = %d\n", counting);
    if ( (scaler_state[card]->prev_counting == 1) && (counting == 0)) {
        Debug(8,"scaler_read: issuing callback request\n");
        scaler_state[card]->done = 1;
        callbackRequest(scaler_state[card]->pcallback);
    }
    scaler_state[card]->prev_counting = counting;
    return(OK);
}

/***************************************************
* scaler_write_preset()
****************************************************/
STATIC long scaler_write_preset(int card, int signal, long val)
{
    Debug(5,"scaler_write_preset: card=%d, signal=%d, val=%ld\n", 
                            card, signal, val);

    if ((card+1) > scaler_total_cards) return(ERROR);
    if ((signal+1) > scaler_state[card]->num_channels) return(ERROR);

    drvSTR7201SetPresets(card, signal, 0., 0, 0, val, SIMPLE_SCALER_MODE);
    return(OK);
}

/***************************************************
* scaler_arm()
* Make scaler ready to count.  If GATE input permits, the scaler will
* actually start counting.
****************************************************/
STATIC long scaler_arm(int card, int val)
{
    Debug(5,"scaler_arm: card=%d, val=%d\n", card, val);
    if ((card+1) > scaler_total_cards) return(ERROR);

    /* clear hardware-done flag */
    scaler_state[card]->done = 0;

    if (val == 1) {
        /* Erase scaler */
        drvSTR7201Erase(card);

        /* Start scaler */
        drvSTR7201AcqOn(card);
    } else {
        /* Stop scaler */
        drvSTR7201AcqOff(card);
    }
    return(OK);
}


/***************************************************
* scaler_done()
****************************************************/
STATIC long scaler_done(int card)
{

    Debug(5,"scaler_done: card=%d, scaler_state[card]->done=%d\n", card,
                    scaler_state[card]->done);
    if ((card+1) > scaler_total_cards) return(ERROR);

    if (scaler_state[card]->done) {
        scaler_state[card]->done = 0;
        return(1);
    } else {
        return(0);
    }
}
