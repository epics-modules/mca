/*******************************************************************************
devScalerSIS3820.c
Device-support routines for scaler record for SIS 3820 
32-channel, 32-bit scaler

Original Author: Mark Rivers, based on devScaler.c by Tim Mooney
Date: 3/25/00

-----------------
.01  3/25/00    mlr     Based on devScaler.c by Tim Mooney
.02  9/28/00    mlr     Set PACT=1 in initRecord if device not found
.03  10/03/00   mlr     Clear preset counts and reset channel advance time in reset()
.04  10/24/00   mlr     Read number of channels to use from driver
.05  4-feb-06   wkl     Modified to work with the SIS3820
.06  17-mar-06  wkl     Reset all preset counters prior to setting new values

*******************************************************************************/
/* version 1.0 */
/*
#include        <vxWorks.h>
#include        <vme.h>
#include        <types.h>
#include        <stdlib.h>
#include        <stdioLib.h>
#include        <iv.h>
#include        <wdLib.h>
*/

/************/
/* Includes */
/************/

/*******************/
/* System Includes */
/*******************/
    
/*#include        <sys/types.h>*/
#include        <stdlib.h>
#include        <stdio.h>

/******************/
/* EPICS Includes */
/******************/

#include        <devLib.h>
#include        <alarm.h>
#include        <dbDefs.h>
#include        <dbAccess.h>
#include		<dbEvent.h>
#include        <dbCommon.h>
#include        <dbScan.h>
#include        <recSup.h>
#include        <recGbl.h>
#include        <callback.h>
#include        <devSup.h>
#include        <drvSup.h>
#include        <epicsTimer.h>
#include		<epicsTypes.h>
#include        <epicsExport.h>

/*******************/
/* Custom Includes */
/*******************/

#include        "scalerRecord.h"
#include        "devScaler.h"
#include        "drvSIS3820.h"
#include        "devScalerSIS3820.h"

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
#define Debug(l,FMT,V...) {  if(l <= devScalerSIS3820Debug) \
                        { printf("%s(%d):",__FILE__,__LINE__); \
                          printf(FMT,## V); } }
#endif
volatile int devScalerSIS3820Debug=0;
epicsExportAddress(int, devScalerSIS3820Debug);

extern int SIS3820NumCards;  /* Defined in drvSIS3820.c */

STATIC long scaler_report(int level);
STATIC long scaler_init(int after);
STATIC long scaler_init_record(struct scalerRecord *psr, CALLBACK *pcallback);
STATIC long scaler_reset(scalerRecord *psr);
STATIC long scaler_read(scalerRecord *psr, long *val);
STATIC long scaler_write_preset(scalerRecord *psr, int signal, long val);
STATIC long scaler_arm(scalerRecord *psr, int val);
STATIC long scaler_done(scalerRecord *psr);

SCALERDSET devScalerSIS3820 = {
    9,
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
epicsExportAddress(dset, devScalerSIS3820);

/*
STATIC int scaler_total_cards;
*/
/**************/
/* Structures */
/**************/

typedef struct {
        int card;
} devScalerPvt;

struct scaler_state **scaler_state = 0;

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
  /* Set up an array with all the possible cards */

  int card;
  int i;

  Debug(2,"scaler_init(): entry, after = %d\n", after);
  if (after) return(0);

  if (scaler_state == NULL) 
  {
    scaler_state = (struct scaler_state **)
      calloc(1, SIS3820_MAX_CARDS * 
          sizeof(struct scaler_state *));
    for (card = 0; card < SIS3820_MAX_CARDS; card++) 
    {
      scaler_state[card] = (struct scaler_state *)
        calloc(1, sizeof(struct scaler_state));

      /* Set the scaler preset channels to zero */

      for (i=0; i<SIS3820_MAX_SIGNALS; i++)
        scaler_state[card]->preset[i] = 0;

    }
  }

  return(OK);
}

/***************************************************
* scaler_init_record()
****************************************************/
STATIC long scaler_init_record(struct scalerRecord *psr, CALLBACK *pcallback)
{
  int card;
  int maxSignals, maxChans, ch1RefEnable, softAdvance;
  devScalerPvt *dpvt;

  /* Check that the IO type is correct. For the moment, I will use INST_IO, but may look at moving to VME_IO later to keep things compatible with the other work that has been done.
   */
  

  if (psr->out.type != VME_IO)
  {
    recGblRecordError(S_dev_badBus,(void *)psr,
        "devScaler3820 (init_record) Illegal OUT Bus Type");
    psr->pact = 1;
    return(S_dev_badBus);
  }

  card = psr->out.value.vmeio.card;

  dpvt = (devScalerPvt *)calloc(1, sizeof(devScalerPvt));
  dpvt->card = card;
  psr->dpvt = dpvt;
  scaler_state[card]->psr = psr;
  Debug(1,"scaler_init_record: card %d\n", card);

  if (drvSIS3820Init(card) != OK) {
    recGblRecordError(S_dev_badCard,(void *)psr,
        "devScaler (init_record) card does not exist!");
    psr->pact = 1;
    return(S_dev_badCard);
  }

  scaler_state[card]->prev_counting = 0;

  drvSIS3820GetConfig(card, &maxSignals, &maxChans, &ch1RefEnable, &softAdvance);
  if (maxSignals > SIS3820_MAX_SIGNALS) maxSignals = SIS3820_MAX_SIGNALS;
  scaler_state[card]->num_channels = maxSignals;
  psr->nch = scaler_state[card]->num_channels;

  scaler_reset(psr);

  /* Set up interrupt handler */
  scaler_state[card]->pcallback = pcallback;

  return(OK);
}


/***************************************************
* scaler_reset()
****************************************************/
STATIC long scaler_reset(scalerRecord *psr)
{
  devScalerPvt *dpvt = psr->dpvt;
  int card = dpvt->card;
  int signal;

  Debug(5,"scaler_reset: card %d\n", card);
  if ((card+1) > SIS3820_MAX_CARDS) return(ERROR);

  /* clear hardware-done flag */
  scaler_state[card]->done = 0;

  Debug(5,"scaler_reset: number of channels %d\n",
      scaler_state[card]->num_channels);
  drvSIS3820SetNChans(card, 1);

  /* zero local copy of scaler presets */
  for (signal=0; signal<MAX_SCALER_CHANNELS; signal++)
    scaler_state[card]->preset[signal] = 0;


  /* clear hardware done flag */
  scaler_state[card]->done = 0;
  
  /* Not sure if I want to do this. Don't think I need it. I imagine these will
   * be set somewhere else.

  for (signal=0; signal<scaler_state[card]->num_channels; signal++)
  {
    drvSIS3820SetPresets(card, signal, 0., 0, 0, 0, SIMPLE_SCALER_MODE);
  }
  */
  /* Used for MCA support */
  /*
     drvSIS3820SetChannelAdvance(card, LNE_INTERNAL, 
     NINT(CHANNEL_ADVANCE_RATE*INTERNAL_CLOCK)-1);
   */

  return(OK);
}

/***************************************************
* scaler_read()
* return pointer to array of scaler values read from
* the card.
***************************************************/
STATIC long scaler_read(scalerRecord *psr, long *val)
{
  devScalerPvt *dpvt = psr->dpvt;
  int card = dpvt->card;
  int counting;

  Debug(5,"scaler_read: card %d\n", card);
  Debug(5,"scaler_read: number of channels %d\n",
      scaler_state[card]->num_channels);

  if ((card+1) > SIS3820_MAX_CARDS) return(ERROR);

  drvSIS3820Read(card, val);

  Debug(5,"scaler_read: scaler channels read\n");

  /* See if acquisition has completed.  If so, issue callback request */
  drvSIS3820GetAcqStatus(card, 0, &counting);
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
STATIC long scaler_write_preset(scalerRecord *psr, int signal, long val)
{
  devScalerPvt *dpvt = psr->dpvt;
  int card = dpvt->card;
  scalerRecord *pscal;
  unsigned short *pgate;
  epicsUInt32 *ppreset;
  int scalerBank;
  int scalerChannel;
  int i;
  int count;

  /* Check the validity of the call */

  if ((card+1) > SIS3820_MAX_CARDS) return(ERROR);
  if ((signal+1) > scaler_state[card]->num_channels) return(ERROR);

  Debug(5,"scaler_write_preset: card=%d, signal=%d, val=%ld\n", 
      card, signal, val);
  Debug(5,"scaler_write_preset: max channels = %d\n",
      scaler_state[card]->num_channels);

  /* Get a pointer to the scaler record and to the gate and preset fields in the
   * scaler record 
   */

  callbackGetUser(pscal, scaler_state[card]->pcallback);

  pgate = &(pscal->g1);
  ppreset = &(pscal->pr1);

  /* For each subsequent channel with the gate set active, deactivate the gate
   * setting and set the preset value to zero.
   */

  scalerBank = signal / 16;

  for (i = (signal % 16) + 1; i < 16; i++)
  {
    scalerChannel = 16 * scalerBank + i;
    
    if (pgate[scalerChannel] != 0)
    {
      pgate[scalerChannel] = 0;

      /* Post monitors so CA clients know about these changes */
      db_post_events(pscal,&(pgate[scalerChannel]),DBE_VALUE);
    }

    if (ppreset[scalerChannel] != 0)
    {
      ppreset[scalerChannel] = 0;

      /* Post monitors so CA clients know about these changes */
      db_post_events(pscal,&(ppreset[scalerChannel]),DBE_VALUE);
    }
  }

  /* Check for an active gate in each bank of sixteen. If there is no active
   * channel, then deactivate scaling for that bank.
   */

  for (scalerBank = 0; scalerBank < 2; scalerBank++)
  {
    count = 0;
    for (i = 0; i < 16; i++)
      count += pgate[scalerBank * 16 + i];
    
    if (count == 0)
    {
      Debug(8, "scaler_write_preset: disabling bank %d\n", scalerBank);
      drvSIS3820DisablePresetBank(card, scalerBank);
    }
    else
    {
      Debug(8, "scaler_write_preset: enabling bank %d\n", scalerBank);
      drvSIS3820EnablePresetBank(card, scalerBank);
    }
  }

  /* Call the driver routine to set the preset in the card */

  drvSIS3820SetPresets(card, signal, val);

  return(OK);
}

/***************************************************
* scaler_arm()
* Make scaler ready to count.  If GATE input permits, the scaler will
* actually start counting.
****************************************************/

STATIC long scaler_arm(scalerRecord *psr, int val)
{
    devScalerPvt *dpvt = psr->dpvt;
    int card = dpvt->card;

    Debug(5,"scaler_arm: card=%d, val=%d\n", card, val);
    if ((card+1) > SIS3820_MAX_CARDS) return(ERROR);

    /* clear hardware-done flag */
    scaler_state[card]->done = 0;

    if (val == 1) {
        /* Erase scaler */
        drvSIS3820Erase(card);
        Debug(5,"scaler_arm: erased scaler\n");

        /* Start scaler */
        drvSIS3820AcqOn(card);
        Debug(5,"scaler_arm: started scaler\n");
    } else {
        /* Stop scaler */
        drvSIS3820AcqOff(card);
        Debug(5,"scaler_arm: stopped scaler\n");
    }
    return(OK);
}


/***************************************************
* scaler_done()
****************************************************/
STATIC long scaler_done(scalerRecord *psr)
{

  devScalerPvt *dpvt = psr->dpvt;
  int card = dpvt->card;

  Debug(5,"scaler_done: card=%d, scaler_state[card]->done=%d\n", card,
      scaler_state[card]->done);
  
     if ((card+1) > SIS3820NumCards) return(ERROR);

     if (scaler_state[card]->done) {
     scaler_state[card]->done = 0;
     return(1);
     } else {
     return(0);
     }
  return(0);
}
