/*****************************************************************************
* This routine provides an interface between the EPICS subroutine record and the
* icb routines for the amplifier module.                 
*****************************************************************************/

/* CI9635 AMP  */
/*
 *
 * Subroutine fields ...
 *
 * DESC - contains the address string of the ICB module, e.g. NI674:1
 * A - Preamp type (0=RC, 1=TRP)
 * B - Coarse gain
 * C - Fine gain
 * D - Super fine gain
 * E - Shaping mode
 * F - Pole zero
 * G - Base-line restore (0=SYM, 1=ASYM)
 * H - Deadtime control (0=normal, 1=LFC)
 * I - Time constant
 * J - Polarity
 * K - Pileup reject
 * L - 0=Amp Off-line, 1=Amp On-line, 2= On-line and requires attention
 *
 *
 *
 * Modification History ....
 *
 * .00  11/28/96    mlr    created - based on icbAdcSub.c
 * .01  07/25/00    mlr    Changed call to icb_initialize, removed ICB search
 *                         which is now done in icb_initialize
 *  .02 09/06/00    mlr    Replaced printf with errlogPrintf
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


long icbAmpSubI(psub)
    struct subRecord    *psub;
{
        return(OK);
}
		

long icbAmpSub(psub)
    struct subRecord    *psub;
{
	int amp_index;
	int s;

        double          *pnew;
        double          *pprev;
        int             i;
	unsigned short  changed;

	static char pramptype[10];
	static float amphwgain1;
	static float amphwgain2;
	static float amphwgain3;
	static char ampshapemode[10];
	static long amppz;
	static char ampblrtype[10];
	static char ampdtctype[10];
	static float amptc;

	static long polFlag;
	static long purFlag;
	static long ampFlags;
	static long olFlag;
	static long attnFlag;


	ICB_PARAM_LIST amp_write_list[] = {
	{CAM_T_PRAMPTYPE, 'S', &pramptype},	/* Preamplifier type (RC/TRP) */
	{CAM_F_AMPHWGAIN1, 0,  &amphwgain1},	/* Course GAIN		      */
	{CAM_F_AMPHWGAIN2, 0,  &amphwgain2},	/* Fine Gain		      */
	{CAM_F_AMPHWGAIN3, 0,  &amphwgain3},	/* Super Fine Gain	      */
	{CAM_T_AMPSHAPEMODE, 'S', &ampshapemode}, /* Amplifier shaping mode     */
	{CAM_L_AMPPZ,		0,  &amppz},	/* Amplifier pole zero	      */
	{CAM_T_AMPBLRTYPE, 'S', &ampblrtype},	/* Base-line restore (SYM,ASYM) */
	{CAM_T_AMPDTCTYPE, 'S', &ampdtctype},	/* Dead-time control (Norm, LFC) */
	{CAM_L_AMPFNEGPOL,  0,	&polFlag},	/* Negative (vs positive) polarity */
	{CAM_L_AMPFPUREJ,   0,	&purFlag},	/* Pileup reject (ena/dis)    */
	{0,			0,	0}};	/* End of list		      */

	ICB_PARAM_LIST amp_read_list[] = {
	{CAM_T_PRAMPTYPE, 'S', &pramptype},	/* Preamplifier type (RC/TRP) */
	{CAM_F_AMPHWGAIN1, 0,  &amphwgain1},	/* Course GAIN		      */
	{CAM_F_AMPHWGAIN2, 0,  &amphwgain2},	/* Fine Gain		      */
	{CAM_F_AMPHWGAIN3, 0,  &amphwgain3},	/* Super Fine Gain	      */
	{CAM_T_AMPSHAPEMODE, 'S', &ampshapemode}, /* Amplifier shaping mode     */
	{CAM_L_AMPPZ,		0,  &amppz},	/* Amplifier pole zero	      */
	{CAM_T_AMPBLRTYPE, 'S', &ampblrtype},	/* Base-line restore (SYM,ASYM) */
	{CAM_T_AMPDTCTYPE, 'S', &ampdtctype},	/* Dead-time control (Norm, LFC) */
	{CAM_F_AMPTC,	    0,	&amptc},	/* Amplifier time constant    */
	{CAM_L_AMPFNEGPOL,  0,	&polFlag},	/* Negative (vs positive) polarity */
	{CAM_L_AMPFPUREJ,   0,	&purFlag},	/* Pileup reject (ena/dis)    */
	{CAM_L_AMPFLAGS,    0,	&ampFlags},     /* AMP mode flags	      */
	{CAM_L_AMPFONLINE,  0,	&olFlag},	/* AMP on line		      */
	{CAM_L_AMPFATTEN,   0,	&attnFlag},	/* Module requires attention  */
	{0,			0,	0}};	/* End of list		      */

        changed = 0;

	if(psub->udf) {
	    /* Initialize ICB data structures */
	    if(aimDebug) {
	        errlogPrintf("(icbAmpSub): icb_initialize\n");
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

	s = icb_findmod_by_address(psub->desc, &amp_index);
	if (s != OK) {
	    if(aimDebug) {
	        errlogPrintf("(icbAmpSub): Error looking up ICB_AMP %s\n", 
	        	psub->desc);
            }
	    psub->udf = 1;
	    return(ERROR);
	}
        if(changed) {
	    /* Send new AMP settings */
	    if(psub->a) strcpy(pramptype, "TRP");
            else strcpy(pramptype, "RC");
	    amphwgain1 = psub->b;
	    amphwgain2 = psub->c;
	    amphwgain3 = psub->d;
	    if(psub->e) strcpy(ampshapemode, "TRIANGLE");
            else strcpy(ampshapemode, "GAUSSIAN");
	    amppz = psub->f;     
	    if(psub->g) strcpy(ampblrtype, "ASYM");
            else strcpy(ampblrtype, "SYM");
	    if(psub->h) strcpy(ampdtctype, "LFC");
            else strcpy(ampdtctype, "Norm");
	    polFlag = psub->j;
            purFlag = psub->k;

	    s = icb_amp_hdlr(amp_index, amp_write_list, ICB_M_HDLR_WRITE);
	    if (s != OK) {
	        if(aimDebug) {
	            errlogPrintf("(icbAmpSub): Error writing to ICB_AMP %s\n", 
	            	psub->desc);
                }
		return(ERROR) ;
            }
        }

        /* Read AMP parameters */
	s = icb_amp_hdlr(amp_index, amp_read_list, ICB_M_HDLR_READ);
	if (s != OK) {
	    if(aimDebug) {
	        errlogPrintf("(icbAmpSub): Error reading ICB_AMP %s\n", 
	        	psub->desc);
            }
	    return(ERROR);
	}

	if (strcmp(pramptype, "TRP")==0) psub->a = 1;
	else psub->a = 0;
	psub->b = amphwgain1;
	psub->c = amphwgain2;
	psub->d = amphwgain3;
	if (strcmp(ampshapemode, "TRIANGLE")==0) psub->e = 1;
	else psub->e = 0;
	psub->f = amppz;
	if (strcmp(ampblrtype, "ASYM")==0) psub->g = 1;
	else psub->g = 0;
	if (strcmp(ampdtctype, "LFC")==0) psub->h = 1;
	else psub->h = 0;
	psub->i = amptc;
	psub->j = polFlag;
        psub->k = purFlag;
	if(attnFlag) 
	    psub->l = 2;
	else if(olFlag)
	    psub->l = 1;
        else
	    psub->l = 0;

	return(OK);
	
}
