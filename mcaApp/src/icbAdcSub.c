/*****************************************************************************
* This routine provides an interface between the EPICS subroutine record and the
* icb routines for the specified module.                 
*****************************************************************************/

/* CI9635 8K ADC  */
/*
 *
 * Subroutine fields ...
 *
 * DESC - contains the address string of the ICB module, e.g. NI674:1
 * A - Range
 * B - Offset
 * C - Acq Mode; 0=PHA, 1=SVA
 * D - Conversion Gain
 * E - Lower level discriminator
 * F - Upper level discriminator
 * G - Zero
 * H - ADC mode flags
 * I - Anti-coincidence Mode
 * J - Late (vs early) Coincidence Mode
 * K - Delayed (vs auto) Peak Detect
 * L - 0=ADC Off-line, 1=ADC On-line, 2= On-line and requires attention
 *
 *
 *
 * Modification History ....
 *
 * .00  03/10/95    nda    created
 * .01  03/16/95    nda    added control over some flags (inputs I,J,K,L)
 * .02  04/19/95    nda    check aimDebug flag before printing messages
 * .03  07/25/00    mlr    Changed call to icb_initialize, removed ICB search
 *                         which is now done in icb_initialize
 * .04  09/06/00    mlr    Replaced printf with errlogPrintf
 *
 *
 */

#include        <vxWorks.h>
#include        <vme.h>
#include        <types.h>
#include        <stdioLib.h>
#include        <string.h>

#include        <dbDefs.h>
#include        <subRecord.h>
#include        <taskwd.h>

#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "nmc_sys_defs.h"
#include "campardef.h"

#define ARG_MAX 12

/* pointer to ICB module info structure */
extern struct icb_module_info_struct *icb_module_info;

/* pointer to networked module info */
extern struct nmc_module_info_struct *nmc_module_info;

/* debug flag for aim stuff */
extern int aimDebug;


long icbAdcSubI(psub)
    struct subRecord    *psub;
{
        return(OK);
}
		

long icbAdcSub(psub)
    struct subRecord    *psub;
{
	int adc_index;
	int s;

        double          *pnew;
        double          *pprev;
        int             i;
	unsigned short  changed;

	static long  adcrange;
	static long  adcoffset;
	static char  adcacqmode[10];
	static long  cnvgain;
	static float lld;
	static float uld;
	static float zero;
	static unsigned long adcflags;

	static long  antiCflag;
	static long  lateCflag;
	static long  delayedFlag;
	static long  olFlag;
	static long  attnFlag;

	ICB_PARAM_LIST adc_write_list[] = {
	{CAM_L_ADCRANGE,    0,	&adcrange},	/* Range	      */
	{CAM_L_ADCOFFSET,   0,	&adcoffset},	/* Offset	      */
	{CAM_T_ADCACQMODE, 'S',	&adcacqmode},	/* Acq Mode	      */
	{CAM_L_CNVGAIN,	    0,	&cnvgain},	/* Conversion Gain    */
	{CAM_F_LLD,	    0,	&lld},	 /* Lower level discriminator */
	{CAM_F_ULD,	    0,	&uld},	 /* Upper level discriminator */
	{CAM_F_ZERO,	    0,	&zero},	 /* Zero		      */
	{CAM_L_ADCFANTIC,   0,	&antiCflag},	/* Anti-coincidence mode      */
	{CAM_L_ADCFLATEC,   0,	&lateCflag},	/* Late (vs early) coinc mode */
	{CAM_L_ADCFDELPK,   0,	&delayedFlag},	/* Delayed (vs auto) peak detect */
	{0,		    0,	0}};	 /* End of list		      */

	ICB_PARAM_LIST adc_read_list[] = {
	{CAM_L_ADCRANGE,    0,	&adcrange},	/* Range	      */
	{CAM_L_ADCOFFSET,   0,	&adcoffset},	/* Offset	      */
	{CAM_T_ADCACQMODE, 'S',	&adcacqmode},	/* Acq Mode	      */
	{CAM_L_CNVGAIN,	    0,	&cnvgain},	/* Conversion Gain    */
	{CAM_F_LLD,	    0,	&lld},	 /* Lower level discriminator */
	{CAM_F_ULD,	    0,	&uld},	 /* Upper level discriminator */
	{CAM_F_ZERO,	    0,	&zero},	 /* Zero		      */
	{CAM_L_ADCFANTIC,   0,	&antiCflag},	/* Anti-coincidence mode      */
	{CAM_L_ADCFLATEC,   0,	&lateCflag},	/* Late (vs early) coinc mode */
	{CAM_L_ADCFDELPK,   0,	&delayedFlag},	/* Delayed (vs auto) peak detect */
	{CAM_L_ADCFLAGS,    0,	&adcflags}, /* ADC mode flags	      */
	{CAM_L_ADCFONLINE,  0,	&olFlag},	/* ADC on line		      */
	{CAM_L_ADCFATTEN,   0,	&attnFlag},	/* Module requires attention  */
	{0,		    0,	0}};	 /* End of list		      */

        changed = 0;

	if(psub->udf) {
	    /* Initialize ICB data structures */
	    if(aimDebug) {
	        errlogPrintf("(icbAdcSub): icb_initialize\n");
            }
	    icb_initialize();
	    psub->udf = 0;
	}

        /* check all input fields for changes*/
        for(i=0, pnew=&psub->a, pprev=&psub->la; i<ARG_MAX; \
             i++, pnew++, pprev++) {
            if(*pnew != *pprev) {
                changed = 1;
            }
        }

	s = icb_findmod_by_address(psub->desc, &adc_index);
	if (s != OK) {
	    if(aimDebug) {
	        errlogPrintf("(icbAdcSub): Error looking up ICB_ADC %s\n", 
	        	psub->desc);
            }
	    psub->udf = 1;
	    return(ERROR);
	}
        if(changed) {
	    /* Send new ADC settings */
	    adcrange = psub->a;
	    adcoffset= psub->b;
	    if(psub->c) strcpy(adcacqmode, "SVA");
            else strcpy(adcacqmode, "PHA");
	    cnvgain  = psub->d;
	    lld = psub->e;     
	    uld = psub->f;
	    zero = psub->g;
	    antiCflag = psub->i;
	    lateCflag = psub->j;
            delayedFlag = psub->k;

	    s = icb_adc_hdlr(adc_index, adc_write_list, ICB_M_HDLR_WRITE);
	    if (s != OK) {
	        if(aimDebug) {
	            errlogPrintf("(icbAdcSub): Error writing to ICB_ADC %s\n", 
	            	psub->desc);
                }
		return(ERROR) ;
            }
        }

        /* Read ADC parameters */
	s = icb_adc_hdlr(adc_index, adc_read_list, ICB_M_HDLR_READ);
	if (s != OK) {
	    if(aimDebug) {
	        errlogPrintf("(icbAdcSub): Error reading ICB_ADC %s\n", 
	        	psub->desc);
            }
	    return(ERROR);
	}

	psub->a = adcrange;
	psub->b = adcoffset;
	if(strcmp(adcacqmode, "PHA")) psub->c = 1;
	else psub->c = 0;
	psub->d = cnvgain;
	psub->e = lld;
	psub->f = uld;
	psub->g = zero;
	psub->h = adcflags;
	psub->i = antiCflag;
	psub->j = lateCflag;
	psub->k = delayedFlag;
	if(attnFlag) 
	    psub->l = 2;
	else if(olFlag)
	    psub->l = 1;
        else
	    psub->l = 0;

	return(OK);
	
}
