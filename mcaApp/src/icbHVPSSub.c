/*****************************************************************************
* This routine provides an interface between the EPICS subroutine record and the
* icb routines for the specified module.                 
*****************************************************************************/

/* CI9641 2kV Power Supply  */
/*
 *
 * Subroutine fields ...
 *
 * DESC - contains the address string of the ICB module, e.g. NI674:1
 * A - Voltage Limit
 * B - Voltage
 * C - Overload Latch Mode (0=No, 1=Yes)
 * D - Inihibit Latch Mode (0=No, 1=Yes)
 * E - Inhibit voltage     (0=5V, 1=12V)
 * F - Status              (0=Off, 1=On)
 * G - Reset               (1=Reset overload and inhibit)
 * H - Fast ramp           (0=Slow, 1=Fast)
 * I - Polarity            (Read-only)
 * J - Inhibit             (Read-only)
 * K - Overload            (Read-only)
 * L - 0=HVPS Off-line, 1=HVPS On-line, 2= On-line and requires attention
 *
 *
 *
 * Modification History ....
 *
 * .00  02/03/97    mlr    created - borrowed from ned Arnold's ADC code
 * .01  07/25/00    mlr    Changed call to icb_initialize, removed ICB search
 *                         which is now done in icb_initialize
 * .02  09/06/00    mlr    Replaced printf with errlogPrintf
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
int icbDebug;


long icbHVPSSubI(psub)
    struct subRecord    *psub;
{
        return(OK);
}
		

long icbHVPSSub(psub)
    struct subRecord    *psub;
{
	int hvps_index;
	int s;

        double          *pnew;
        double          *pprev;
        int             i;
	unsigned short  changed;

	static float voltage_limit;
	static float voltage;
	static long  overload_latch;
	static long  inhibit_latch;
	static long  inhibit_voltage;
	static long  status;
	static long  reset;
	static long  fast_ramp;
	static long  polarity;
	static long  inhibit;
	static long  overload;
	static long  online;
	static long  busy;
	static long  olFlag;
	static long  attnFlag;
	static unsigned long  hvpsflags;


	ICB_PARAM_LIST hvps_write_list[] = {
	{CAM_F_HVPSVOLTLIM, 0,  &voltage_limit},   /* Voltage Limit	     */
	{CAM_F_VOLTAGE,     0,  &voltage},	   /* Voltage		     */
	{CAM_L_HVPSFOVLE,   0,  &overload_latch},  /* Overload latch enable  */
	{CAM_L_HVPSFINHLE,  0,  &inhibit_latch},   /* Inhibit latch enable   */
	{CAM_L_HVPSFLVINH,  0,  &inhibit_voltage}, /* 5v/12v inhibit	     */
	{CAM_L_HVPSFSTAT,   0,  &status},          /* Status (on/off)	     */
	{CAM_L_HVPSFOVINRES,0,  &reset},           /* Overload/inhibit reset */
	{CAM_L_HVPSFASTRAMP,0,	&fast_ramp},       /* Use fast ramp method   */
	{0,		    0,	0}};	           /* End of list	     */

	ICB_PARAM_LIST hvps_read_list[] = {
	{CAM_F_HVPSVOLTLIM, 0,  &voltage_limit},   /* Voltage Limit          */
	{CAM_F_VOLTAGE,     0,  &voltage},	   /* Voltage		     */
	{CAM_L_HVPSFOVLE,   0,  &overload_latch},  /* Overload latch enable  */
	{CAM_L_HVPSFINHLE,  0,  &inhibit_latch},   /* Inhibit latch enable   */
	{CAM_L_HVPSFLVINH,  0,  &inhibit_voltage}, /* 5v/12v inhibit	     */
	{CAM_L_HVPSFPOL,    0,  &polarity},        /* Output polarity        */
	{CAM_L_HVPSFINH,    0,  &inhibit},         /* Inhibit		     */
	{CAM_L_HVPSFOV,	    0,  &overload},        /* Overload		     */
	{CAM_L_HVPSFSTAT,   0,  &status},          /* Status (on/off)	     */
	{CAM_L_HVPSFONLINE, 0,  &online},          /* HVPS on line	     */
	{CAM_L_HVPSFOVINRES,0,  &reset},           /* Overload/inhibit reset */
	{CAM_L_HVPSFASTRAMP,0,	&fast_ramp},       /* Use fast ramp method   */
	{CAM_L_HVPSFBUSY,   0,	&busy},            /* HVPS is busy ramping   */
	{CAM_L_HVPSFLAGS,   0,	&hvpsflags},       /* HVPS mode flags	     */
	{CAM_L_HVPSFONLINE, 0,	&olFlag},	   /* HVPS on line           */
	{CAM_L_HVPSFATTEN,  0,	&attnFlag},	  /* Module requires attention*/
	{0,		    0,	0}};	           /* End of list	     */


        changed = 0;

	if(psub->udf) {
	    /* Initialize ICB data structures */
	    if(icbDebug) {
	        errlogPrintf("(icbHVPSSub): icb_initialize in icbHVPSSub\n");
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

	s = icb_findmod_by_address(psub->desc, &hvps_index);
	if (s != OK) {
	    if(icbDebug) {
	        errlogPrintf("(icbHVPSSub): Error looking up ICB_HVPS %s\n", 
	        	psub->desc);
            }
	    psub->udf = 1;
	    return(ERROR);
	}
        if(changed) {
	    /* Send new HVPS settings */
	    voltage_limit   = psub->a;
	    voltage         = psub->b;
	    overload_latch  = psub->c;
	    inhibit_latch   = psub->d;
	    inhibit_voltage = psub->e;
	    status          = psub->f;
	    reset           = psub->g;
	    fast_ramp       = psub->h;
	    s = icb_hvps_hdlr(hvps_index, hvps_write_list, ICB_M_HDLR_WRITE);
	    if (s != OK) {
	        if(icbDebug) {
	            errlogPrintf("(icbHVPSSub): Error writing to ICB_HVPS %s\n", 
	            	psub->desc);
                }
		return(ERROR) ;
            }
        }

        /* Read HVPS parameters */
	s = icb_hvps_hdlr(hvps_index, hvps_read_list, ICB_M_HDLR_READ);
	if (s != OK) {
	    if(icbDebug) {
	        errlogPrintf("(icbHVPSSub): Error reading ICB_HVPS %s\n", 
	        	psub->desc);
            }
	    return(ERROR);
	}

	psub->a = voltage_limit;
	psub->b = voltage;
	psub->c = overload_latch;
	psub->d = inhibit_latch;
	psub->e = inhibit_voltage;
	psub->f = status;
	psub->g = reset;
	psub->h = fast_ramp;
	psub->i = polarity;
	psub->j = inhibit;
	psub->k = overload;
	if(attnFlag) 
	    psub->l = 2;
	else if(olFlag)
	    psub->l = 1;
        else
	    psub->l = 0;

	return(OK);
	
}
