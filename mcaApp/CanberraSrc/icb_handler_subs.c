/*******************************************************************************
*
* This module contains handlers for each type of 2nd generation ICB module.
*
* Notes:
*
*    1)	The key structure used in this module is ICB_MODULE_INFO, sometimes
*	referred to as the module database. It is defined in ICB_SYS_DEFS.H, 
*	and created by ICB_CRMPSC. The database is a "module hotel": once an
*	entry is entered for a module, it is never removed. The entries are
*	assigned in strictly ascending order.
*
*    2) Module addresses are stored in the database in "normalized" format.
*	This just means that the addresses are left justified, uppercase,
*	with no leading zeroes in any numeric value.
*
*    3) Once assumption made here (as well in other ND9900 modules) is that
*	device names are unique in the first character (i.e., 'N' always means
*	NI devices).
*
********************************************************************************
*
* Revision History:
*
*	20-Feb-1992	TLG	Original (started)
*	11-Aug-1992	TLG	Fixed non-terminated parameter lists in
*				ICB_SETUP_CCNIM_MODULES and
*				ICB_ARCHIVE_CCNIM_MODULES subroutines.
*	29-Jan-1993	RJH	Make sure all string compares are case-blind
*	12-Mar-1993	TLG	Add logic to slowly ramp up the HVPS Module.
*				Non-Canberra detectors appear to be more
*				sensitive to how quickly the voltage level
*				increases.
*	18-Apr-1993	RJH	Return a blank string for unknown module types
*				in the _READ routines.
*	 7-May-1993	TLG	In ICB_SETUP_CCNIM_MODULES() the HVPS Module's
*				current ON/OFF state is read in from the
*				module itself in order to preserve it's value.
*	24-May-1993	TLG	Converted some string compare calls from
*				strcmp() or stricmp() to strnicmp().  This
*				will ensure that CAM_T_xxxx parameters are
*				evaluated correctly. (i.e. CAM_T_ADCACQMODE).
*
*	26-Oct-1993	TLG	Modified the routine icb_hvps_ramp_voltage()
*				so that it returns a TRUE status if the
*				current and new voltage levels are the same.
*				Also load the off status into the HVPS cache
*				when the HVPS is reset.
*
*	 4-Nov-1993	TLG	Now fetch the computed amplifier pole zero
*				even if the auto PZ fails to converge.
*
*	 6-Dec-1993	TLG	If amplifier is reset and the fine gain
*				motor position in the non-volatile memory
*				is reasonable, store it in the amplifier
*				registers.  If the position is the same
*				as what is stored in the AMP cache, do
*				nothing else.  Otherwise, home the motor
*				and reposition it.
*				Also reconciled how the NVRAM was being
*				written vs how it was being read for the
*				motor position.
*	27-Jan-1994	RJH	Don't call LIB$WAIT with a floating 
*				constant (doesn't work on AXP).
*       1994            MLR     Converted to vxWorks
*       1996?           MLR     Modifications to work with amplifier
*       18-Nov-1997     MLR     Minor mods to avoid compiler warnings
*       23-Mar-1998     MLR     Minor mods to avoid compiler errors
*       06-Dec-1999     MLR     Minor mods to avoid compiler warnings
*       10-Aug-2000     TMM     Fixed bugs which caused memory corruption with
*                               unknown ICB modules (e.g. TCA and DSP)
*
*******************************************************************************/

#define ICB_DEFINE_NAMES 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "icb_bus_defs.h"
#include "nmc_sys_defs.h"
#include "nmcmsgdef.h"
#include "mcamsgdef.h"
#include "campardef.h"

extern ICB_MODULE_INFO *icb_module_info;	/* pointer to ICB module info structure */

extern struct nmc_module_info_struct *nmc_module_info;	/* pointer to networked module info */

/*-------------------------------------------------------*/
/* Declare the verify error bits that should precipitate */
/* the setting the ASTF_CCERR flag.			 */
/*-------------------------------------------------------*/

LONG hvps_ccerr_vbits = CAM_M_HVPSVF_VOLT  | CAM_M_HVPSVF_OVLE  |
			CAM_M_HVPSVF_INHLE | CAM_M_HVPSVF_LVINH |
			CAM_M_HVPSVF_STAT;

LONG adc_ccerr_vbits  = CAM_M_ADCVF_RANGE   | CAM_M_ADCVF_OFFSET  |
			CAM_M_ADCVF_ACQMODE | CAM_M_ADCVF_CNVGAIN |
			CAM_M_ADCVF_LLD     | CAM_M_ADCVF_ULD     |
			CAM_M_ADCVF_ZERO    | CAM_M_ADCVF_ANTIC   |
			CAM_M_ADCVF_LATEC   | CAM_M_ADCVF_DELPK   |
			CAM_M_ADCVF_NONOV   | CAM_M_ADCVF_LTCPUR;

LONG amp_ccerr_vbits  = CAM_M_AMPVF_PRAMPT  | CAM_M_AMPVF_HWGAIN1 |
			CAM_M_AMPVF_HWGAIN3 | CAM_M_AMPVF_SHAPEM  |
			CAM_M_AMPVF_BLRTYPE | CAM_M_AMPVF_DTCTYPE |
			CAM_M_AMPVF_DIFF    | CAM_M_AMPVF_NEGPOL  |
			CAM_M_AMPVF_COMPINH | CAM_M_AMPVF_PUREJ;


/*--------------------------------------*/
/* Define a structure offset macro	*/
/*--------------------------------------*/

#define SOFFSET(p_type,field) \
        (((char *) (&(((p_type)NULL)->field))) - (char *)NULL)

#define SOFFSET_OF(s_type,field) SOFFSET(s_type*,field)



/*******************************************************************************
*
*	Supported HVPS parameter arrays
*
*******************************************************************************/


static ICB_PARAM_LIST hvps_param_list[] = {
	{CAM_T_HVPSADDRESS,	'S',	0},	/* HVPS Address		*/
	{CAM_T_HVPSTYPE,	'S',	0},	/* HVPS Type		*/
	{CAM_T_HVPSID,		'S',	0},	/* HVPS ID		*/
	{CAM_F_HVPSVOLTLIM,	0,	0},	/* Voltage Limit	*/
	{CAM_F_VOLTAGE,		0,	0},	/* Voltage		*/
	{CAM_L_HVPSFLAGS,	0,	0},	/* HVPS mode flags	*/
	{0,			0,	0}};	/* End of list		*/

static ICB_PARAM_LIST hvps_flag_list[] = {
	{CAM_L_HVPSFOVLE,	0,	0},	/* Overload latch enable      */
	{CAM_L_HVPSFINHLE,	0,	0},	/* Inhibit latch enable	      */
	{CAM_L_HVPSFLVINH,	0,	0},	/* 5v/12v inhibit	      */
	{CAM_L_HVPSFPOL,	0,	0},	/* Output polarity            */
	{CAM_L_HVPSFINH,	0,	0},	/* Inhibit		      */
	{CAM_L_HVPSFOV,		0,	0},	/* Overload		      */
	{CAM_L_HVPSFSTAT,	0,	0},	/* Status (on/off)	      */
	{CAM_L_HVPSFONLINE,	0,	0},	/* HVPS on line		      */
	{CAM_L_HVPSFOVINRES,	0,	0},	/* Overload/inhibit reset     */
	{CAM_L_HVPSFATTEN,	0,	0},	/* Module requires attention  */
	{CAM_L_HVPSFASTRAMP,	0,	0},	/* Use fast ramp method	      */
	{CAM_L_HVPSFBUSY,	0,	0},	/* HVPS is busy ramping       */
	{0,			0,	0}};	/* End of list		      */

/*----------------------------------------------*/
/* These flag bit masks MUST be loaded in the	*/
/* same order as the above parameter list.	*/
/*----------------------------------------------*/

static LONG hvps_flag_bits[] = {
	CAM_M_HVPSF_OVLE,
	CAM_M_HVPSF_INHLE,
	CAM_M_HVPSF_LVINH,
	CAM_M_HVPSF_POL,
	CAM_M_HVPSF_INH,
	CAM_M_HVPSF_OV,
	CAM_M_HVPSF_STAT,
	CAM_M_HVPSF_ONLINE,
	CAM_M_HVPSF_OVINRES,
	CAM_M_HVPSF_ATTEN,
	CAM_M_HVPSF_FASTRAMP,
	CAM_M_HVPSF_BUSY,
	0};

/*--------------------------------------*/
/* Define supported HVPS module types	*/
/*--------------------------------------*/

static CHAR *hvps_types_str[] = {
			"ICB 9641",
			"ICB 9645",
			0};

static LONG hvps_types_int[] = {
			ICB_K_MTYPE_CI9641,	/* 2000 Volt supply	*/
			ICB_K_MTYPE_CI9645,	/* 6000 Volt supply	*/
			0};			/* End of list		*/


/*******************************************************************************
*
*	Supported ADC parameter arrays
*
*******************************************************************************/


static ICB_PARAM_LIST adc_param_list[] = {
	{CAM_T_ADCADDRESS,	'S',	0},	/* ADC Address		      */
	{CAM_T_ADCTYPE,		'S',	0},	/* ADC Type		      */
	{CAM_T_ADCID,		'S',	0},	/* ADC ID		      */
	{CAM_L_ADCRANGE,	0,	0},	/* Range		      */
	{CAM_L_ADCOFFSET,	0,	0},	/* Offset		      */
	{CAM_T_ADCACQMODE,	'S',	0},	/* Acq Mode		      */
	{CAM_L_CNVGAIN,		0,	0},	/* Conversion Gain	      */
	{CAM_F_LLD,		0,	0},	/* Lower level discriminator  */
	{CAM_F_ULD,		0,	0},	/* Upper level discriminator  */
	{CAM_F_ZERO,		0,	0},	/* Zero			      */
	{CAM_L_ADCFLAGS,	0,	0},	/* ADC mode flags	      */
	{0,			0,	0}};	/* End of list		      */

static ICB_PARAM_LIST adc_flag_list[] = {
	{CAM_L_ADCFANTIC,	0,	0},	/* Anti-coincidence mode      */
	{CAM_L_ADCFLATEC,	0,	0},	/* Late (vs early) coinc mode */
	{CAM_L_ADCFDELPK,	0,	0},	/* Delayed (vs auto) peak detect */
	{CAM_L_ADCFCIMCAIF,	0,	0},	/* CI (vs ND) ADC interface   */
	{CAM_L_ADCFNONOV,	0,	0},	/* Non-overlap transfer mode  */
	{CAM_L_ADCFONLINE,	0,	0},	/* ADC on line		      */
	{CAM_L_ADCFATTEN,	0,	0},	/* Module requires attention  */
	{0,			0,	0}};	/* End of list		      */

/*----------------------------------------------*/
/* These flag bit masks MUST be loaded in the	*/
/* same order as the above parameter list.	*/
/*----------------------------------------------*/

static LONG adc_flag_bits[] = {
	CAM_M_ADCF_ANTIC,
	CAM_M_ADCF_LATEC,
	CAM_M_ADCF_DELPK,
	CAM_M_ADCF_CIMCAIF,
	CAM_M_ADCF_NONOV,
	CAM_M_ADCF_ONLINE,
	CAM_M_ADCF_ATTEN,
	0};

/*--------------------------------------*/
/* Define supported ADC module types	*/
/*--------------------------------------*/

static CHAR *adc_types_str[] = {
			"ICB 9633",
			"ICB 9635",
			0};

static LONG adc_types_int[] = {
			ICB_K_MTYPE_CI9633,	/* 16K chan ADC		*/
			ICB_K_MTYPE_CI9635,	/* 8K chan ADC		*/
			0};			/* End of list		*/

/*******************************************************************************
*
*	Supported Amplifier parameter arrays
*
*******************************************************************************/


static ICB_PARAM_LIST amp_param_list[] = {
	{CAM_T_AMPADDRESS,	'S',	0},	/* Amplifier Address	      */
	{CAM_T_AMPTYPE,		'S',	0},	/* Amplifier Type	      */
	{CAM_T_AMPID,		'S',	0},	/* Amplifier ID		      */
	{CAM_T_PRAMPTYPE,	'S',	0},	/* Preamplifier type (RC/TRP) */
	{CAM_F_AMPHWGAIN1,	0,	0},	/* Course GAIN		      */
	{CAM_F_AMPHWGAIN2,	0,	0},	/* Fine Gain		      */
	{CAM_F_AMPHWGAIN3,	0,	0},	/* Super Fine Gain	      */
	{CAM_T_AMPSHAPEMODE,	'S',	0},	/* Amplifier shaping mode     */
	{CAM_L_AMPPZ,		0,	0},	/* Amplifier pole zero	      */
	{CAM_T_AMPBLRTYPE,	'S',	0},	/* Base-line restore (SYM,ASYM) */
	{CAM_T_AMPDTCTYPE,	'S',	0},	/* Dead-time control (Norm, LFC) */
	{CAM_F_AMPTC,		0,	0},	/* Amplifier time constant    */
	{CAM_L_AMPFLAGS,	0,	0},	/* Amplifier mode flags	      */
	{0,			0,	0}};	/* End of list		      */

static ICB_PARAM_LIST amp_flag_list[] = {
	{CAM_L_AMPFDIFF,	0,	0},	/* Differential (vs normal) input */
	{CAM_L_AMPFNEGPOL,	0,	0},	/* Negative (vs positive) polarity */
	{CAM_L_AMPFCOMPINH,	0,	0},	/* Complement inhibit polarity*/
	{CAM_L_AMPFPUREJ,	0,	0},	/* Pileup reject (ena/dis)    */
	{CAM_L_AMPFONLINE,	0,	0},	/* Amplifier on line	      */
	{CAM_L_AMPFMOTRBUSY,	0,	0},	/* Amplifier motor busy	      */
	{CAM_L_AMPFPZBUSY,	0,	0},	/* Amplifier pole zero busy   */
	{CAM_L_AMPFATTEN,	0,	0},	/* Module requires attention  */
	{CAM_L_AMPFPZSTART,	0,	0},	/* Start AMP auto pole zero   */
	{CAM_L_AMPFPZFAIL,	0,	0},	/* AMP auto pole zero failed  */
	{0,			0,	0}};	/* End of list		      */

/*----------------------------------------------*/
/* These flag bit masks MUST be loaded in the	*/
/* same order as the above parameter list.	*/
/*----------------------------------------------*/

static LONG amp_flag_bits[] = {
	CAM_M_AMPF_DIFF,
	CAM_M_AMPF_NEGPOL,
	CAM_M_AMPF_COMPINH,
	CAM_M_AMPF_PUREJ,
	CAM_M_AMPF_ONLINE,
	CAM_M_AMPF_MOTRBUSY,
	CAM_M_AMPF_PZBUSY,
	CAM_M_AMPF_ATTEN,
	CAM_M_AMPF_PZSTART,
	CAM_M_AMPF_PZFAIL,
	0};

/*--------------------------------------*/
/* Define supported AMP module types	*/
/*--------------------------------------*/

static CHAR *amp_types_str[] = {
			"ICB 9615",
			0};

static LONG amp_types_int[] = {
			ICB_K_MTYPE_CI9615,	/* Standard Amp		*/
			0};			/* End of list		*/


/*------------------------------------------------*/
/* Define table of supported course gain settings */
/* and corrisponding DAC values.		  */
/*------------------------------------------------*/

static struct {
	REAL real_val;
	LONG dac_val;
} amp_hwgain1_tbl[] =  {{500.0, 0x0F},
			{250.0, 0x0D},
			{100.0, 0x0B},
			{ 50.0, 0x07},
			{ 25.0, 0x05},
			{ 10.0, 0x03},
			{  5.0, 0x01},
			{  2.5, 0x00}};

static REAL amp_shape_time[]  = {0.5, 1.0, 2.0,   4.0, 6.0,  12.0, 0.0};


/*******************************************************************************
*
*	Summarize parameter lists and handler functions.
*
*******************************************************************************/


static ICB_PARAM_LIST *icb_plist_summary[]  = {adc_param_list,
						amp_param_list,
						hvps_param_list, NULL};

static ICB_PARAM_LIST *icb_flist_summary[]  = {adc_flag_list,
						amp_flag_list,
						hvps_flag_list, NULL};

static LONG (*icb_hdlr_summary[])()  = {icb_adc_hdlr,
					icb_amp_hdlr,
					icb_hvps_hdlr, NULL};


/*
* List the parameters that must be read from the ICB module itself
* (rather than the cache) during a read operation.
*/

static LONG icb_req_read_plist[] = {CAM_L_HVPSFPOL,
				    CAM_L_HVPSFOV,
				    CAM_L_HVPSFINH,
				    CAM_L_HVPSFSTAT,
				    CAM_F_AMPTC,
				    0};


/*******************************************************************************
*
* ICB_HVPS_HDLR handles the HVPS models 9641 (2K volt max) and 9645 (6K volt
* max).
*
* Its calling format is:
*
*	status = ICB_HVPS_HDLR (index, param, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword) is the index of the module in icb_module_info[].
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the HVPS module.
*
*  "flags" (longword, by reference) is used as follows:
*	ICB_M_HDLR_READ		! Reads the specified parameters from the
*				! ICB module.
*	ICB_M_HDLR_WRITE	! Writes the specified parameters to the
*				! ICB module.
*	ICB_M_HDLR_WRITE_CACHE	! Writes the specified parameters to the
*				! ICB module's local cache only.
*	ICB_M_HDLR_INITIALIZE	! Initialize the module.
*	ICB_M_HDLR_FLUSH_CACHE	! Sends the cached parameter values to the
*				! ICB module
*
*******************************************************************************/

LONG icb_hvps_hdlr (LONG index,
		    ICB_PARAM_LIST *params,
		    LONG flags)

{
  LONG s;
  ICB_CCNIM_HVPS *hvps;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *plist;
  LONG reset;
  LONG present;

/*
* First make sure that the module exists and is the correct type.
*/
	s = icb_validate_module (index, params, &present, &reset, flags);
	if (s != OK) return s;
	entry = &icb_module_info[index];
	hvps  = (ICB_CCNIM_HVPS *) entry->pcache;
	hvps->flags.bit.online = present;

/*
* If the initialize flag is set, update the module & clear the module reset.
* If the HVPS was reset, make sure it's considered off.
*/
	if (present && (reset || ((flags & ICB_M_HDLR_INITIALIZE) != 0))) {

	    if (reset) hvps->flags.bit.stat = 0;	/* HVPS must be off */

	    s = icb_hvps_write (hvps, hvps_param_list, ICB_M_HDLR_INITIALIZE);
	    if (s != OK) return s;

	    s = icb_write_csr ((ICB_CCNIM_ANY *) hvps, ICB_M_CTRL_LED_GREEN,
				     ICB_M_CTRL_CLEAR_RESET, -1);
	    if ((flags & ICB_M_HDLR_INITIALIZE) != 0) return s;
	    if (s != OK) return s;
	}

/*
* Handle flush cache operations. Don't actually write the parameters if
* the module was reset.  If that's the case the parameters have already
* been written (see above).
*/
	if ((flags & ICB_M_HDLR_FLUSH_CACHE) != 0) {
	    if ((plist = icb_build_cached_plist ((ICB_CCNIM_ANY *) hvps)) == NULL)
	    	return OK;
	    if (!reset) s = icb_hvps_write (hvps, plist, ICB_M_HDLR_INITIALIZE);
	    else s = OK;
	    free (plist);
	    if (s != OK) return s;
	}

/*
* Handle the read or write operation.
*/
	s = OK;
	if ((flags & ICB_M_HDLR_WRITE) != 0)
	    s = icb_hvps_write (hvps, params, flags);
	else if ((flags & ICB_M_HDLR_READ) != 0)
	    s = icb_hvps_read (hvps, params, flags);
	if (s != OK) return s;


/*
* Make sure the green LED is on.
*/
	if ((flags & ICB_M_HDLR_WRITE_CACHE) != ICB_M_HDLR_WRITE_CACHE)
	    s = icb_write_csr ((ICB_CCNIM_ANY *)hvps, ICB_M_CTRL_LED_GREEN, 0, -1);

	return s;
}


/*******************************************************************************
*
* ICB_ADC_HDLR handles the ADC models 9633 (8K chan max) and 9635 (16K chan
* max).
*
* Its calling format is:
*
*	status = ICB_ADC_HDLR (index, param, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword) is the index of the module in icb_module_info[].
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the ADC module.
*
*  "flags" (longword, by reference) is used as follows:
*	ICB_M_HDLR_READ		! Reads the specified parameters from the
*				! ICB module.
*	ICB_M_HDLR_WRITE	! Writes the specified parameters to the
*				! ICB module.
*	ICB_M_HDLR_WRITE_CACHE	! Writes the specified parameters to the
*				! ICB module's local cache only.
*	ICB_M_HDLR_INITIALIZE	! Initialize the module.
*	ICB_M_HDLR_FLUSH_CACHE	! Sends the cached parameter values to the
*				! ICB module
*
*******************************************************************************/

LONG icb_adc_hdlr (LONG index,
		   ICB_PARAM_LIST *params,
		   LONG flags)

{
  LONG s;
  ICB_CCNIM_ADC *adc;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *plist;
  LONG reset;
  LONG present;

/*
* First make sure that the module exists and is the correct type.
*/
	s = icb_validate_module (index, params,	&present, &reset, flags);
	if (s != OK) return s;
	entry = &icb_module_info[index];
	adc   = (ICB_CCNIM_ADC *) entry->pcache;
	adc->flags.bit.online = present;


/*
* If the initialize flag is set, update the module & clear the module reset.
*/
	if (present && (reset || ((flags & ICB_M_HDLR_INITIALIZE) != 0))) {

	    s = icb_adc_write (adc, adc_param_list, ICB_M_HDLR_INITIALIZE);
	    if (s != OK) return s;

	    s = icb_write_csr ((ICB_CCNIM_ANY *)adc, ICB_M_CTRL_LED_GREEN,
				    ICB_M_CTRL_CLEAR_RESET, -1);
	    if ((flags & ICB_M_HDLR_INITIALIZE) != 0) return s;
	    if (s != OK) return s;
	}

/*
* Handle flush cache operations. Don't actually write the parameters if
* the module was reset.  If that's the case the parameters have already
* been written (see above).
*/
	if ((flags & ICB_M_HDLR_FLUSH_CACHE) != 0) {
	    if ((plist = icb_build_cached_plist ((ICB_CCNIM_ANY *) adc)) == NULL) 
	    	return OK;
	    if (!reset) s = icb_adc_write (adc, plist, ICB_M_HDLR_INITIALIZE);
	    else s = OK;
	    free (plist);
	    if (s != OK) return s;
	}

/*
* Handle the read or write operation.
*/
	s = OK;
	if ((flags & ICB_M_HDLR_WRITE) != 0)
	    s = icb_adc_write (adc, params, flags);
	else if ((flags & ICB_M_HDLR_READ) != 0)
	    s = icb_adc_read (adc, params, flags);
	if (s != OK) return s;

/*
* Make sure the green LED is on.
*/
	if ((flags & ICB_M_HDLR_WRITE_CACHE) != ICB_M_HDLR_WRITE_CACHE)
	    s = icb_write_csr ((ICB_CCNIM_ANY *)adc, ICB_M_CTRL_LED_GREEN, 0, -1);

	return s;
}


/*******************************************************************************
*
* ICB_AMP_HDLR handles the amplifier model 9615.
*
* Its calling format is:
*
*	status = ICB_AMP_HDLR (index, param, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword) is the index of the module in icb_module_info[].
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the amplifier module.
*
*  "flags" (longword, by reference) is used as follows:
*	ICB_M_HDLR_READ		! Reads the specified parameters from the
*				! ICB module.
*	ICB_M_HDLR_WRITE	! Writes the specified parameters to the
*				! ICB module.
*	ICB_M_HDLR_WRITE_CACHE	! Writes the specified parameters to the
*				! ICB module's local cache only.
*	ICB_M_HDLR_INITIALIZE	! Initialize the module.
*	ICB_M_HDLR_FLUSH_CACHE	! Sends the cached parameter values to the
*				! ICB module
*
*******************************************************************************/

LONG icb_amp_hdlr (LONG index,
		   ICB_PARAM_LIST *params,
		   LONG flags)

{
  LONG s;
  ICB_CCNIM_AMP *amp;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *plist;
  LONG reset;
  LONG present;

/*
* First make sure that the module exists and is the correct type.
*/
	s = icb_validate_module (index, params,	&present, &reset, flags);
	if (s != OK) return s;
	entry = &icb_module_info[index];
	amp   = (ICB_CCNIM_AMP *) entry->pcache;
	amp->flags.bit.online = present;


/*
* If the initialize flag is set, update the module & clear the module reset.
*/
	if (present && (reset || ((flags & ICB_M_HDLR_INITIALIZE) != 0))) {

	    s = icb_amp_write (amp, amp_param_list, ICB_M_HDLR_INITIALIZE);
	    if (s != OK) return s;

	    s = icb_write_csr ((ICB_CCNIM_ANY *)amp, ICB_M_CTRL_LED_GREEN,
				    ICB_M_CTRL_CLEAR_RESET, -1);
	    if ((flags & ICB_M_HDLR_INITIALIZE) != 0) return s;
	    if (s != OK) return s;
	}

/*
* Handle flush cache operations. Don't actually write the parameters if
* the module was reset.  If that's the case the parameters have already
* been written (see above).
*/
	if ((flags & ICB_M_HDLR_FLUSH_CACHE) != 0) {
	    if ((plist = icb_build_cached_plist ((ICB_CCNIM_ANY *) amp)) == NULL) 
	    	return OK;
	    if (!reset) s = icb_amp_write (amp, plist, ICB_M_HDLR_INITIALIZE);
	    else s = OK;
	    free (plist);
	    if (s != OK) return s;
	}

/*
* Handle the read or write operation.
*/
	s = OK;
	if ((flags & ICB_M_HDLR_WRITE) != 0)
	    s = icb_amp_write (amp, params, flags);
	else if ((flags & ICB_M_HDLR_READ) != 0)
	    s = icb_amp_read (amp, params, flags);
	if (s != OK) return s;

/*
* Make sure the green LED is on.
*/
	if ((flags & ICB_M_HDLR_WRITE_CACHE) != ICB_M_HDLR_WRITE_CACHE)
	    s = icb_write_csr ((ICB_CCNIM_ANY *)amp, ICB_M_CTRL_LED_GREEN, 0, -1);

	return s;
}


/*******************************************************************************
*
* ICB_VALIDATE_MODULE does some work that's common for each kind of CCNIM
* module.  It makes sure that the module exists, that it is the correct type.
* Finally it attempts to read the module's 16 registers.  
* It notes if the module is actually on line and if it's been reset.
*
* Its calling format is:
*
*	status = ICB_VALIDATE_MODULE (index, params, present, reset, flags);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword) is the module index in icb_module_info[].
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the HVPS module.
*
*  "present" (longword, by reference) returned boolian indicating if the
*  module is on-line
*
*  "reset" ((longword, by reference) returned boolian indicating if the
*   module has been reset.
*
*  "flags" (longword) is the reserved flags field.
*
*******************************************************************************/

int icb_validate_module (LONG index,
			 ICB_PARAM_LIST *params,
			 LONG *present,
			 LONG *reset,
			 LONG flags)

{
  LONG i, j;
  LONG s;
  ICB_MODULE_INFO *entry;
  UCHAR register_list[16];
  LONG do_read;
  ICB_CCNIM_ANY *ccany;

/*
* Assume the module is not present and not reset.
*/
	*present = FALSE;
	*reset   = FALSE;

/* Make sure the module index is in the valid range */
	if ((index < 0) || (index >= ICB_K_MAX_MODULES)) 
		return (MCA__NOSUCHICBMODULE);

/* Make sure that this entry in icb_module_info is for a valid module */
	entry = &icb_module_info[index];
	if (entry->valid != 1)  return (MCA__NOSUCHICBMODULE);
	
/*
* If this is an interaction with module cache only, exit now.
*/
	if ((flags & ICB_M_HDLR_CACHE_ONLY) != 0) return OK;

	ccany = (ICB_CCNIM_ANY *) entry->pcache;
	if (ccany == NULL) return(-1); /* tmm: no cache. */
	
/* Make sure the icb_index value for the cache entry for this module points to
   this module structure */
	if (ccany->icb_index != index) return (MCA__INVGROUP);

/*
* If this is a read operation, it may not be necessary to access the
* ICB module.  Most parameters maybe read from the CACHE.  Some
* exceptions (for example) are the HVPS's overload & inhibit flags.
* See the definition of the array "icb_req_read_plist" for all
* the required read parameters.
*/
	if ((flags & ICB_M_HDLR_READ) != 0) {
	    do_read = FALSE;
	    for (i = 0; params[i].pcode != 0; i++) {
		for (j = 0; icb_req_read_plist[j] != 0; j++)
		    if (params[i].pcode == icb_req_read_plist[j]) {
			do_read = TRUE;
			break;
		    }
		if (do_read) break;
	    }
	    if (do_read == FALSE) {
		*present = entry->present;
		return OK;
	    }
	}

/*
* Read the module's 16 registers.  R0 is the Module's CSR.  R1 is 
* NVRAM data and need not be stored.
*/
	s = icb_input (ccany->icb_index, 0, 16, register_list);
	if (s != OK) return s;
	entry->r0_read = register_list[0];
	memcpy (entry->registers, &register_list[2], 14);

/*
* If the module had been removed, a -1 will be read from each register.
* If that's the case, flag that it's no longer present.
*/
	if (entry->r0_read == -1) {
	    entry->present = FALSE;
	    return OK;
	}
	entry->present = *present = TRUE;

/*
* Check for a module reset.  Only load the "was_reset" flag if the
* module reset flag is set.  It's upto the outside world to clear
* the flag.
*/
	*reset = (entry->r0_read & ICB_M_STAT_RESET) != 0;
	if (*reset) entry->was_reset = TRUE;

/*
* Signal if the module has failed.
*/
	if ((entry->r0_read & ICB_M_STAT_FAIL) != 0) {
	    entry->failed = TRUE;
	    return(MCA__INVRESP);
	} else
	    entry->failed = FALSE;

	return OK;
}


/*******************************************************************************
*
* ICB_INIT_CCNIM_CACHE clears the cache and loads the parameter codes
*
* Its calling format is:
*
*	status = ICB_INIT_CCNIM_CACHE (index, flags);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword) is the index of the module in icb_module_info[].
*
*  "flags" (longword) is the reserved flags field.
*
*******************************************************************************/

int icb_init_ccnim_cache (LONG index,
		      LONG flags)

{
  LONG i, j;
  ICB_MODULE_INFO *entry;
  ICB_CCNIM_ANY *ccany;
  ICB_PARAM_LIST *plist=NULL, *flist=NULL;

/*
* Get a pointer to the module cache
*/
  	entry = &icb_module_info[index];

/*
* Allocate and clear the module cache
*/
  	switch((*entry).module_type) {
  	
  	case ICB_K_MTYPE_RPI: /* Remote Parallel Interface (ND554) */
 		break;

  	case ICB_K_MTYPE_CI9641: 	/* CI's 2000 volt HVPS		     */
  	case ICB_K_MTYPE_CI9645:  	/* CI's 6000 volt HVPS		     */
		(*entry).pcache = (ICB_CCNIM_ANY *) calloc(1, sizeof(ICB_CCNIM_HVPS));
                if ((*entry).pcache == NULL) return(-1); /* tmm */
  		plist = hvps_param_list;
  		flist = hvps_flag_list;
  		break;

  	case ICB_K_MTYPE_CI9633:	/* CI's 16K channel ADC		     */
  	case ICB_K_MTYPE_CI9635:	/* CI's 8K channel ADC		     */
		(*entry).pcache = (ICB_CCNIM_ANY *) calloc(1, sizeof(ICB_CCNIM_ADC));
                if ((*entry).pcache == NULL) return(-1); /* tmm */
  		plist = adc_param_list;
  		flist = adc_flag_list;
  		break;

  	case ICB_K_MTYPE_CI9615:	/* CI's Standard CC Amplifier	     */
		(*entry).pcache = (ICB_CCNIM_ANY *) calloc(1, sizeof(ICB_CCNIM_AMP));
                if ((*entry).pcache == NULL) return(-1); /* tmm */
  		plist = amp_param_list;
  		flist = amp_flag_list;
  		break;
    }
	
/* 
*   Make pointer to generic cache for any module
*/
    ccany = (ICB_CCNIM_ANY *) (*entry).pcache;
    if (ccany == NULL) return(OK); /* tmm: if we got this far without a cache, we don't need one */
/* 
*  Set the icb_index pointer in the cache structure to this entry in
*  icb_module_info[]
*/
    ccany->icb_index = index;

/*
* Load the non-flag parameters into the cache parameter list.
*/
	for (i = 0, j = 0; plist[i].pcode != 0; i++, j++)
	    ccany->param_list[j].pcode = plist[i].pcode;

/*
* Load the flag parameters into the cache parameter list.
*/
	j--;			/* Overwrite CAM_L_xxxFLAGS parameter */
	for (i = 0; flist[i].pcode != 0; i++, j++)
	    ccany->param_list[j].pcode = flist[i].pcode;

	return OK;
}


/*******************************************************************************
*
* ICB_BUILD_CACHED_PLIST counts the number of cached parameters in the specifed
* structure.  Allocates/loads an array parameter structures with the codes
* of the parameters that have been cached.  Finally it clears the cached
* flag for each parameter.
*
* Its calling format is:
*
*	plist = ICB_BUILD_CACHED_PLIST (ccany);
* 
* where
*
*  "plist" (ICB_PARAM_LIST *) is returned list of parameters based on 
*
*  "ccany" (ICB_CCNIM_ANY *) is the pointer to the generic ccnim structure.
*
*******************************************************************************/

ICB_PARAM_LIST *icb_build_cached_plist (ICB_CCNIM_ANY *ccany)

{
  LONG i, j;
  LONG pcount = 0;
  ICB_PARAM_LIST *plist;

/*
* Count the number of cached parameters.  If zero, return a NULL value
* for the parameter list.
*/
	for (i = 0; i < ICB_K_MAX_CACHED_PARAMS; i++)
	    if (ccany->param_list[i].cached_flag == TRUE) pcount++;
	if (pcount == 0) return NULL;
	pcount++;			/* Add one extra for terminator */

/*
* Allocate an array of parameter structures.
*/
	plist = (ICB_PARAM_LIST *) malloc (pcount * sizeof(*plist));

/*
* Load the parameter codes & clear the cached flags.
*/
	for (i = 0, j = 0; i < ICB_K_MAX_CACHED_PARAMS; i++)
	    if (ccany->param_list[i].cached_flag == TRUE) {
		plist[j].pcode = ccany->param_list[i].pcode;
		ccany->param_list[i].cached_flag = FALSE;
		j++;
	    }
	plist[j].pcode = 0;		/* Terminate the parameter list */

/*
* Return the parameter list pointer
*/
	return plist;
}


/*******************************************************************************
*
* ICB_SET_CACHED_FLAGS sets the cached flag for each parameter in the list.
*
* Its calling format is:
*
*	plist = ICB_SET_CACHED_FLAGS (ccany, params);
* 
* where
*
*  "plist" (ICB_PARAM_LIST *) is returned list of parameters based on 
*
*  "ccany" (ICB_CCNIM_ANY *) is the pointer to the generic ccnim structure.
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the amplifier module.
*
*******************************************************************************/

int icb_set_cached_flags (ICB_CCNIM_ANY *ccany,
		      ICB_PARAM_LIST *params)

{
  LONG i, j;

/*
* For each parameter, find it's entry (if any) in the cached parameter
* list and set it's cached flag to TRUE.
*/
	for (i = 0; params[i].pcode != 0; i++)
	    for (j = 0; j < ICB_K_MAX_CACHED_PARAMS; j++) {
		if (params[i].pcode == ccany->param_list[j].pcode) {
		    ccany->param_list[j].cached_flag = TRUE;
		    break;
		}
	    }

	return OK;
}


/*******************************************************************************
*
* ICB_HVPS_WRITE writes the specified parameters to the HVPS ICB module.
*
* Its calling format is:
*
*	status = ICB_HVPS_WRITE (hvps, params, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*  "params" (ICB_PARAM_LIST array) is a zero terminated list of CAM parameters that
*  are to be applied to the HVPS module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_hvps_write (ICB_CCNIM_HVPS *hvps,
		     ICB_PARAM_LIST *params,
		     LONG flags)

{
  LONG i;
  LONG s=OK;
  LONG index;
  ULONG fbits;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *flag_list;
  LONG *flag_vals;
  UCHAR reg_val;
  LONG local_flags;
  REAL level;
  ICB_PARAM_LIST *params_start;

/*
* Get a pointer to the icb module database entry.
*/
	index = hvps->icb_index;
	if ((index != -1) && ((flags & ICB_M_HDLR_CACHE_ONLY) == 0)) {
	    entry = &icb_module_info[hvps->icb_index];
	    if (entry->present == FALSE) entry = NULL;
	} else
	    entry = NULL;

/*
* Loop until the parameter list terminator is encountered.
*/
	params_start = params;
	for (; params->pcode != 0; params++) {

	/*-------------------------------*/
	/* Write the value to the module */
	/*-------------------------------*/

	    s = OK;
	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_F_VOLTAGE:	/* Voltage			*/
					/*------------------------------*/

		/* Make sure the new value is valid */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->voltage = *((REAL *) params->value);

		     if (hvps->voltage > hvps->voltlim)
			hvps->voltage = hvps->voltlim;
		     if (hvps->voltage < 0.0)
			hvps->voltage = 0.0;
		     if (!entry) break;

		/* If fast ramp is enabled, go ahead and send the new   */
		/* voltage level					*/

		     if (hvps->flags.bit.fastramp) {
			s = icb_hvps_send_voltage (hvps, hvps->voltage, 0);
			hvps->flags.bit.busy = FALSE;
			break;
		     }

		/* If the module is off or a ramp is in progress, exit */

		     if (!hvps->flags.bit.stat || hvps->flags.bit.busy)
			break;

		/* Begin ramp process... Set busy flag and call the ramp rtn */

		     hvps->flags.bit.busy = TRUE;
		     s = icb_hvps_ramp_voltage (hvps);
		     break;

					/*------------------------------*/
		case CAM_F_HVPSVOLTLIM:	/* Voltage Limit		*/
					/*------------------------------*/

		/* Load absolute limit based on the module type */

		     if (entry->module_type == ICB_K_MTYPE_CI9641)
			hvps->abs_voltlim = 2000;
		     else if (entry->module_type == ICB_K_MTYPE_CI9645)
			hvps->abs_voltlim = 6000;
		     else
			break;

		/* Make sure the new value is valid */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->voltlim = *((REAL *) params->value);

		     if (hvps->voltlim > hvps->abs_voltlim)
			hvps->voltlim = hvps->abs_voltlim;
			
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFLAGS:	/* Flags field			*/
					/*------------------------------*/

		/* Build a flag parameter list based on the flag bit settings */

		     flag_list = (ICB_PARAM_LIST *) malloc (sizeof (hvps_flag_list));
		     flag_vals = (LONG *) malloc (sizeof (hvps_flag_bits));
		     memcpy (flag_list, hvps_flag_list, sizeof (hvps_flag_list));

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0) {
			fbits = *((LONG *) params->value);
			for (i = 0; flag_list[i].pcode != 0; i++) {
			    flag_vals[i] = ((hvps_flag_bits[i] & fbits) != 0);
			    flag_list[i].value = &flag_vals[i];
			}
		     }

		/* Send the individual flag values */

		     local_flags = flags | ICB_M_HDLR_NOVERIFY;
		     s = icb_hvps_write (hvps, flag_list, local_flags);
		     free (flag_list);
		     free (flag_vals);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFOVLE:	/* Overload latch enable	*/
					/*------------------------------*/

		/* Set or clear the overload latch enable bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.ovle = *((LONG *) params->value);
		     if (!entry) break;

		     if (hvps->flags.bit.ovle)
			entry->registers[0] |= HVPS_M_R2_OVLATCH_ENA;
		     else
			entry->registers[0] &= ~HVPS_M_R2_OVLATCH_ENA;

		/* Send new flags byte to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFINHLE:	/* Inhibit latch enable		*/
					/*------------------------------*/

		/* Set or clear the inhibit latch enable bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.inhle = *((LONG *) params->value);
		     if (!entry) break;

		     if (hvps->flags.bit.inhle)
			entry->registers[0] |= HVPS_M_R2_INHLATCH_ENA;
		     else
			entry->registers[0] &= ~HVPS_M_R2_INHLATCH_ENA;

		/* Send new flags byte to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFLVINH:	/* 5v/12v inhibit		*/
					/*------------------------------*/

		/* Set or clear the 12V level inhibit enable bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.lvinh = *((LONG *) params->value);
		     if (!entry) break;

		     if (hvps->flags.bit.lvinh)
			entry->registers[0] |= HVPS_M_R2_INHRANGE_12V;
		     else
			entry->registers[0] &= ~HVPS_M_R2_INHRANGE_12V;

		/* Send new flags byte to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFPOL:    /* Output polarity              */
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.pol = *((LONG *) params->value);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFSTAT:	/* Status (on/off)		*/
					/*------------------------------*/

		/* Set or clear the On/Off bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.stat = *((LONG *) params->value);
		     if (!entry) break;

		/* If module is being turned off, terminate the ramp process */
		/* if its currently active.  Then just turn off the module.  */

		     if (!hvps->flags.bit.stat) {
			if (hvps->ramp_ast_id != 0) {
/* 
   For now nothing is needed here, but if the ramp runs as a separate task then
   we will want to kill that task
*/
/*			    sys$cantim (hvps->ramp_ast_id, 0); */
			    hvps->ramp_ast_id = 0;
			}
			s = icb_hvps_set_state (hvps, FALSE, 0);
			hvps->flags.bit.busy = FALSE;
			break;
		     }

		/* If the HVPS is already on, then don't bother */

		     if ((entry->registers[0] & HVPS_M_R2_STATUS_ON) != 0)
			break;

		/* If fast ramp is enabled, load the cached HVPS value	*/
		/* into the module and turn it on.			*/

		     if (hvps->flags.bit.fastramp) {
			s = icb_hvps_send_voltage (hvps, hvps->voltage, 0);
			if (s != OK) break;
			s = icb_hvps_set_state (hvps, TRUE, 0);
			hvps->flags.bit.busy = FALSE;
			break;
		     }

		/* Otherwise, load a value of zero volts into the module */
		/* and turn it on.					 */

		     level = 0.0;
		     s = icb_hvps_send_voltage (hvps, level, 0);
		     if (s != OK) break;
		     s = icb_hvps_set_state (hvps, TRUE, 0);
		     if (s != OK) break;

		/* Finally ramp up the voltage to the desired level */

		     hvps->flags.bit.busy = TRUE;
		     s = icb_hvps_ramp_voltage (hvps);
		     break;

					 /*-----------------------------*/
		case CAM_L_HVPSFOVINRES: /* Overload/Inhibit Reset	*/
					 /*-----------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			if (*((LONG *) params->value) == 0) break;

		/* Reset the HVPS */

		     if (!entry) break;
		     reg_val = entry->registers[0] | HVPS_M_R2_RESET;
		     s = icb_output (index, 2, 1, &reg_val);
		     break;

					 /*-----------------------------*/
		case CAM_L_HVPSFASTRAMP: /* Fast Ramp Flag		*/
					 /*-----------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.fastramp = *((LONG *) params->value);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFATTEN:	/* Module requires attention	*/
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			hvps->flags.bit.atten = *((LONG *) params->value);
		     if (!entry) break;

		     if (hvps->flags.bit.atten)
			entry->r0_write |=  ICB_M_CTRL_LED_YELLOW;
		     else
			entry->r0_write &= ~ICB_M_CTRL_LED_YELLOW;
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */

	    if (s != OK) return s;

	} /* End of for() */

/*
* Set the cached flags for cache only operations
*/
	if ((flags & ICB_M_HDLR_WRITE_CACHE) == ICB_M_HDLR_WRITE_CACHE)
	    icb_set_cached_flags ((ICB_CCNIM_ANY *) hvps, params_start);

/*
* Verify the new module settings.
*/
	if ((flags & ICB_M_HDLR_NOVERIFY) == 0)
	    if (entry) s = icb_hvps_verify (hvps, params_start, 0, NULL);

	return s;
}


/*******************************************************************************
*
* ICB_HVPS_READ reads the specified parameters from the HVPS ICB module.
* This function assumes that the registers in the module were just read into
* the ICB_MODULE_INFO structure!
*
* Its calling format is:
*
*	status = ICB_HVPS_READ (hvps, params, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be read from the HVPS module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_hvps_read (ICB_CCNIM_HVPS *hvps,
		    ICB_PARAM_LIST *params,
		    LONG flags)

{
  LONG i;
  ULONG reg;
  LONG index;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *params_start;


/*
* Get a pointer to the icb module database entry.
*/
	index = hvps->icb_index;
	if (index != -1) {
	    entry = &icb_module_info[hvps->icb_index];
	    if (entry->present == FALSE) entry = NULL;
	} else
	    entry = NULL;

/*
* Loop until the parameter list terminator is encountered.
*/
	params_start = params;
	for (; params->pcode != 0; params++) {

	/*--------------------------------*/
	/* Read the value from the module */
	/*--------------------------------*/

	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_F_VOLTAGE:	/* Voltage			*/
					/*------------------------------*/

		     *((REAL *) params->value) = hvps->voltage;
		     break;

					/*------------------------------*/
		case CAM_F_HVPSVOLTLIM:	/* Voltage Limit		*/
					/*------------------------------*/


		     *((REAL *) params->value) = hvps->voltlim;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFLAGS:	/* Flags field			*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->flags.lword;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFOVLE:	/* Overload latch enable	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.ovle;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFINHLE:	/* Inhibit latch enable		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.inhle;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFLVINH:	/* 5v/12v inhibit		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.lvinh;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFPOL:    /* Output polarity              */
					/*------------------------------*/

		     if (!entry) {
			*((ULONG *) params->value) = hvps->flags.bit.pol;
			break;
		     }

		     reg = ((ULONG) entry->registers[1]);
		     *((ULONG *) params->value) = i = ((reg & HVPS_M_R3_POLARITY_NEG) != 0);

		     if (i != hvps->flags.bit.pol) {
			hvps->flags.bit.pol = i;
			hvps->vflags.bit.id = TRUE;
		     }
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFINH:	/* Inhibit			*/
					/*------------------------------*/

		     if (entry) {
			reg = ((ULONG) entry->registers[1]);
			hvps->flags.bit.inh = ((reg & HVPS_M_R3_INHIBIT) == 0);
		     }
		     *((ULONG *) params->value) = hvps->flags.bit.inh;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFOV:	/* Overload			*/
					/*------------------------------*/

		     if (entry) {
			reg = ((ULONG) entry->registers[1]);
			hvps->flags.bit.ov = ((reg & HVPS_M_R3_OVERLOAD) == 0);
		     }
		     *((ULONG *) params->value) = hvps->flags.bit.ov;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFSTAT:	/* Status (on/off)		*/
					/*------------------------------*/

		     if (entry) {
			reg = entry->registers[0];
			hvps->flags.bit.stat = ((reg & HVPS_M_R2_STATUS_ON) != 0);
		     }
		     *((ULONG *) params->value) = hvps->flags.bit.stat;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFONLINE:	/* Module on-line		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.online;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFATTEN:	/* Module requires attention	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.atten;
		     break;

					 /*-----------------------------*/
		case CAM_L_HVPSFASTRAMP: /* Fast Ramp Flag		*/
					 /*-----------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.fastramp;
		     break;

					 /*-----------------------------*/
		case CAM_L_HVPSFBUSY:	 /* Ramp in progress flag	*/
					 /*-----------------------------*/

		     *((ULONG *) params->value) = hvps->flags.bit.busy;
		     break;

		/*======================================================*/
		/* Verify Parameter Flags Section...			*/
		/*======================================================*/

					/*------------------------------*/
		case CAM_L_HVPSVFLAGS:	/* Verify Flags field		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.lword;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFID:	/* VF: Module ID		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.id;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFVOLT:	/* VF: Voltage			*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.volt;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFOVLE:	/* VF: Overload latch		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.ovle;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFINHLE:	/* VF: Inhibit latch 		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.inhle;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFLVINH:	/* VF: 5v/12v inhibit		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.lvinh;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFPOL:	/* VF: Output polarity 		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.id;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSVFSTAT:	/* VF: Status (on/off)		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = hvps->vflags.bit.stat;
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */


	} /* End of for() */


	return OK;
}


/*******************************************************************************
*
* ICB_HVPS_VERIFY is called from ICB_HVPS_WRITE() after one or more parameters
* values have been written to the HVPS module.  It reads the registers of the
* HVPS and compares them with what is stored in the ICB database.
*
* Its calling format is:
*
*	status = ICB_HVPS_VERIFY (hvps, params, flags, reg_list)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*  "params" (ICB_PARAM_LIST array) is a zero terminated list of CAM parameters
*  that have been applied to the HVPS module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*  "reg_list" (UBYTE *) The list of ICB registers.  This should be NULL
*  when ICB_HVPS_VERIFY() is called from the outside.
*
*******************************************************************************/

LONG icb_hvps_verify (ICB_CCNIM_HVPS *hvps,
		      ICB_PARAM_LIST *params,
		      LONG flags,
		      UBYTE *reg_list)

{
  LONG s;
  UBYTE registers[14];
  UBYTE lreg;
  UBYTE mreg;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.  If an entry does not
* exist or the module is not present, get out.
*/
	if (hvps->icb_index == -1) return OK;
	entry = &icb_module_info[hvps->icb_index];
	if (entry->present == FALSE) return OK;

/*
* Read in the module registers 2 though 16.
*/
	if (reg_list == NULL) {
	    s = icb_input (hvps->icb_index, 2, 14, registers);
	    if (s != OK) return s;
	} else
	    memcpy (registers, reg_list, 14);

/*
* Loop until all the parameters have been verified.
*/
	for (; params->pcode != 0; params++) {

	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_F_VOLTAGE:	/* Voltage			*/
					/*------------------------------*/

		     registers[2] &= 0xC0;
		     hvps->vflags.bit.volt =
			((entry->registers[2] != registers[2]) ||
			 (entry->registers[3] != registers[3]));
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFLAGS:	/* Flags field			*/
					/*------------------------------*/

		     icb_hvps_verify (hvps, hvps_flag_list, flags, registers);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFOVLE:	/* Overload latch enable	*/
					/*------------------------------*/

		     lreg = entry->registers[0] & HVPS_M_R2_OVLATCH_ENA;
		     mreg =        registers[0] & HVPS_M_R2_OVLATCH_ENA;
		     hvps->vflags.bit.ovle = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFINHLE:	/* Inhibit latch enable		*/
					/*------------------------------*/

		     lreg = entry->registers[0] & HVPS_M_R2_INHLATCH_ENA;
		     mreg =        registers[0] & HVPS_M_R2_INHLATCH_ENA;
		     hvps->vflags.bit.inhle = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFLVINH:	/* 5v/12v inhibit		*/
					/*------------------------------*/

		     lreg = entry->registers[0] & HVPS_M_R2_INHRANGE_12V;
		     mreg =        registers[0] & HVPS_M_R2_INHRANGE_12V;
		     hvps->vflags.bit.lvinh = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFPOL:    /* Output polarity              */
					/*------------------------------*/

		     mreg = ((registers[1] & HVPS_M_R3_POLARITY_NEG) != 0);
		     hvps->vflags.bit.pol = (hvps->flags.bit.pol != mreg);
		     if (hvps->vflags.bit.pol == 1)
			hvps->flags.bit.pol = mreg;
		     break;

					/*------------------------------*/
		case CAM_L_HVPSFSTAT:	/* Status (on/off)		*/
					/*------------------------------*/

		     lreg = entry->registers[0] & HVPS_M_R2_STATUS_ON;
		     mreg =        registers[0] & HVPS_M_R2_STATUS_ON;
		     hvps->vflags.bit.stat = (lreg != mreg);
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */


	} /* End of for() */

	return OK;
}


/*******************************************************************************
*
* ICB_HVPS_RAMP_VOLTAGE gradually ramps up/down the voltage level the
* specified ICB module.
*
* Its calling format is:
*
*	status = ICB_HVPS_RAMP_VOLTAGE (hvps);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*******************************************************************************/

LONG icb_hvps_ramp_voltage (ICB_CCNIM_HVPS *hvps)

/* 
   This routine runs as an AST on VMS systems.  This means it runs as
   a separate "thread".  Under EPICS this routine presently runs inline
   and thus causes a significant delay when ramping.  In the future we
   could simply make this routine run as a separate vxWorks task.
*/
{
  LONG s = OK;					/* Assume true status	*/
  REAL hvps_diff;
  REAL level;
  double delay = 5.0;	/* Delta time of 5 seconds */

/*
* Clear the AST Id
*/
	hvps->ramp_ast_id = 0;

while (1) {
   /*
   * Get the difference between the level we are moving toward and the
   * current level of the module itself.
   */
	hvps_diff = hvps->voltage - hvps->current_level;
	if (hvps_diff == 0.0) break;

   /*
   * If difference > 400, use 400 as the increment.
   */
	if (hvps_diff > 400.0)
	    level = hvps->current_level + 400.0;
	else
	    level = hvps->current_level + hvps_diff;

   /*
   * Make sure new level is not negative.
   */
	if (level < 0.0) level = 0.0;

   /*
   * Send the new level to the module.
   */
	s = icb_hvps_send_voltage (hvps, level, 0);
	if (s != OK) break;

   /*
   * If there is more to be done, wait 5 seconds.
   */
	epicsThreadSleep(delay);
}

/*
* If we get this far, the ramp is complete.
*/
	hvps->flags.bit.busy = FALSE;
	return s;
}


/*******************************************************************************
*
* ICB_HVPS_SEND_VOLTAGE writes the specified voltage to the HVPS ICB module.
*
* Its calling format is:
*
*	status = ICB_HVPS_SEND_VOLTAGE (hvps, voltage, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*  "voltage" (float, by reference) is the voltage value to send to the ICB
*  module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_hvps_send_voltage (ICB_CCNIM_HVPS *hvps,
			    REAL voltage,
			    LONG flags)

{
  LONG s;
  LONG index;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.
*/
	index = hvps->icb_index;
	if (index == -1) return OK;
	entry = &icb_module_info[hvps->icb_index];
	if (entry->present == FALSE) return OK;

/*
* Archive new voltage level
*/
	hvps->current_level = voltage;

/*
* Convert voltage to 10 bit DAC value & load into local regs
*/
	icb_hvps_convert_voltage (hvps, &voltage, 0);

/*
* Send new voltage level to the ICB module
*/
	s = icb_output (index, 5, 1, &entry->registers[3]);
	if (s != OK) return s;
	s = icb_output (index, 4, 1, &entry->registers[2]);
	if (s != OK) return s;
	return OK;
}


/*******************************************************************************
*
* ICB_HVPS_CONVERT_VOLTAGE converts a voltage level to a DAC value or a DAC
* value to a voltage level.  By default the voltage is converted to a
* DAC value.
*
* Its calling format is:
*
*	status = ICB_HVPS_CONVERT_VOLTAGE (hvps, voltage, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*  "voltage" (float, by reference) is the voltage to be converted or returned.
*
*  "flags" (longword, by reference) is the reserved flags field
*	   ICB_M_HVPS_DAC_TO_VOLTAGE	: Indicates the DAC value should be
*					  converted into a voltage level.
*
*******************************************************************************/

LONG icb_hvps_convert_voltage (ICB_CCNIM_HVPS *hvps,
			       REAL *voltage,
			       LONG flags)
{
  ULONG dac;
  REAL hvps_step;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.
*/
	if (hvps->icb_index == -1) return OK;
	entry = &icb_module_info[hvps->icb_index];

/*
* Get the appropriate step based on the module type
*/
	if (entry->module_type == ICB_K_MTYPE_CI9641)
	    hvps_step = 1.955;		/* 2000 / 1023 */
	else if (entry->module_type == ICB_K_MTYPE_CI9645)
	    hvps_step = 5.865;		/* 6000 / 1023 */
	else
	     return OK;

/*
* Depending on the flags value, convert the voltage into a DAC value
* or convert the DAC value into a voltage.
*/
	if ((flags & ICB_M_HVPS_DAC_TO_VOLTAGE) == 0) {
	    dac = *voltage / hvps_step;
	    entry->registers[2] = dac << 6;
	    entry->registers[3] = dac >> 2;
	} else {
	    dac  = ((entry->registers[2] >> 6) & 0x0003);
	    dac |= ((entry->registers[3] << 2) & 0x03FC);
	    *voltage = (REAL) dac * hvps_step;
	}

	return OK;
}


/*******************************************************************************
*
* ICB_HVPS_SET_STATE turns the HVPS ICB module ON or OFF.
*
* Its calling format is:
*
*	status = ICB_HVPS_SET_STATE (hvps, state, flags);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "hvps" (ICB_CCNIM_HVPS *) is the pointer to the structure that caches info
*  about the HVPS.
*
*  "state" (longword, by reference) is the ON/OFF state to be applied.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_hvps_set_state (ICB_CCNIM_HVPS *hvps,
			 LONG state,
			 LONG flags)

{
  LONG s;
  LONG index;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.
*/
	index = hvps->icb_index;
	if (index == -1) return OK;
	entry = &icb_module_info[hvps->icb_index];
	if (entry->present == FALSE) return OK;

/*
* Load the register bit appropriately.  If turning HVPS off, zero the
* current level.
*/
	if (state)
	    entry->registers[0] |= HVPS_M_R2_STATUS_ON;
	else {
	    entry->registers[0] &= ~HVPS_M_R2_STATUS_ON;
	    hvps->current_level = 0.0;
	}

/*
* Send new flags byte to the ICB module
*/
	s = icb_output (index, 2, 1, &entry->registers[0]);
	return s;
}


/*******************************************************************************
*
* ICB_ADC_WRITE writes the specified parameters to the ADC ICB module.
*
* Its calling format is:
*
*	status = ICB_ADC_WRITE (adc, params, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "adc" (ICB_CCNIM_ADC *) is the pointer to the structure that caches info
*  about the ADC.
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the ADC module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_adc_write (ICB_CCNIM_ADC *adc,
		    ICB_PARAM_LIST *params,
		    LONG flags)

{
  LONG i;
  LONG s=OK;
  ULONG dac;
  ULONG fbits;
  LONG load_dac = FALSE;
  LONG index;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *flag_list;
  LONG *flag_vals;
  UCHAR reg_val;
  LONG local_flags;
  ICB_PARAM_LIST *params_start;

/*
* Get a pointer to the icb module database entry.
*/
	index = adc->icb_index;
	if ((index != -1) && ((flags & ICB_M_HDLR_CACHE_ONLY) == 0)) {
	    entry = &icb_module_info[adc->icb_index];
	    if (entry->present == FALSE) entry = NULL;
	} else
	    entry = NULL;

/*
* Loop until the parameter list is complete
*/
	params_start = params;
	for (; params->pcode != 0; params++) {

	/*-------------------------------*/
	/* Write the value to the module */
	/*-------------------------------*/

	    s = OK;
	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_L_ADCRANGE:	/* Range			*/
					/*------------------------------*/

		/* Convert range in channels to the format the module expects */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->range = *((LONG *) params->value);
		     if (!entry) break;

		     dac = icb_adc_encode_chns (adc->range) << 4;
		     entry->registers[1] &= 0x0F;
		     entry->registers[1] |= dac;

		/* Send new range value to the ICB module */

		     s = icb_output (index, 3, 1, &entry->registers[1]);
		     break;

					/*------------------------------*/
		case CAM_L_ADCOFFSET:	/* Offset			*/
					/*------------------------------*/

		/* Convert offset in channels to the format the module expects */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->offset = *((LONG *) params->value);
		     if (!entry) break;

		     dac = ~(adc->offset >> 7);
		     entry->registers[10] = dac;

		/* Send new offset value to the ICB module */

		     s = icb_output (index, 12, 1, &entry->registers[10]);
		     break;

					/*------------------------------*/
		case CAM_T_ADCACQMODE:	/* Acquisition Mode		*/
					/*------------------------------*/

		/* Set the acquisition mode */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			icb_dsc2str (adc->acqmode, params->value, 8);
		     if (!entry) break;

		     if (strnicmp (adc->acqmode, "SVA", 3) == 0)
			entry->registers[0] &= ~ADC_M_R2_ACQMODE_SVA;
		     else
			entry->registers[0] |= ADC_M_R2_ACQMODE_SVA;

		/* Send new mode selection value to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_CNVGAIN:	/* Conversion Gain		*/
					/*------------------------------*/

		/* Convert cnvgain channels to the format the module expects */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->cnvgain = *((LONG *) params->value);
		     if (!entry) break;

		     dac = icb_adc_encode_chns (adc->cnvgain);
		     entry->registers[1] &= 0xF0;
		     entry->registers[1] |= dac;

		/* Send new cnvgain value to the ICB module */

		     s = icb_output (index, 3, 1, &entry->registers[1]);
		     break;

					/*------------------------------*/
		case CAM_F_LLD:		/* Lower level discriminator	*/
					/*------------------------------*/

		/* Make sure LLD value is reasonable */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->lld = *((REAL *) params->value);
		     if (adc->lld <   0.0) adc->lld  = 0.0;
		     if (adc->lld > 110.0) adc->lld  = 110.0;

		/* Convert to 12 bit DAC value & load into local regs */

		     if (!entry) break;
		     dac = adc->lld * 37.2273;
		     entry->registers[2] = dac;
		     entry->registers[3] = dac >> 8;

		/* Send new LLD value to the ICB module */

		     s = icb_output (index, 4, 2, &entry->registers[2]);
		     load_dac = TRUE;
		     break;

					/*------------------------------*/
		case CAM_F_ULD:		/* Upper level discriminator	*/
					/*------------------------------*/

		/* Make sure ULD value is reasonable */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->uld = *((REAL *) params->value);
		     if (adc->uld <   0.0) adc->uld = 0.0;
		     if (adc->uld > 110.0) adc->uld = 110.0;

		/* Convert to 12 bit DAC value & load into local regs */

		     if (!entry) break;
		     dac = adc->uld * 37.2273;
		     entry->registers[4] = dac;
		     entry->registers[5] = dac >> 8;

		/* Send new LLD value to the ICB module */

		     s = icb_output (index, 6, 2, &entry->registers[4]);
		     load_dac = TRUE;
		     break;

					/*------------------------------*/
		case CAM_F_ZERO:	/* Zero				*/
					/*------------------------------*/

		/* Make sure Zero value is reasonable */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->zero = *((REAL *) params->value);
		     if (adc->zero < -5.0) adc->zero = -5.0;
		     if (adc->zero >  5.0) adc->zero =  5.0;

		/* Convert to 12 bit DAC value & load into local regs */

		     if (!entry) break;
		     dac = (-(adc->zero - 5.0)) * 409.50;
		     entry->registers[6] = dac;
		     entry->registers[7] = dac >> 8;

		/* Send new LLD value to the ICB module */

		     s = icb_output (index, 8, 2, &entry->registers[6]);
		     load_dac = TRUE;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLAGS:	/* ADC Mode flags		*/
					/*------------------------------*/

		/* Build a flag parameter list based on the flag bit settings */

		     flag_list = (ICB_PARAM_LIST *) malloc (sizeof (adc_flag_list));
		     flag_vals = (LONG *) malloc (sizeof (adc_flag_bits));
		     memcpy (flag_list, adc_flag_list, sizeof (adc_flag_list));

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0) {
			fbits = *((LONG *) params->value);
			for (i = 0; flag_list[i].pcode != 0; i++) {
			    flag_vals[i] = ((adc_flag_bits[i] & fbits) != 0);
			    flag_list[i].value = &flag_vals[i];
			}
		     }

		/* Send the individual flag values */

		     local_flags = flags | ICB_M_HDLR_NOVERIFY;
		     s = icb_adc_write (adc, flag_list, local_flags);
		     free (flag_list);
		     free (flag_vals);
		     continue;

					/*------------------------------*/
		case CAM_L_ADCFANTIC:	/* Anti-coincidence mode	*/
					/*------------------------------*/

		/* Set / Clear the anticoincidence mode bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->flags.bit.antic = *((LONG *) params->value);
		     if (!entry) break;

		     if (adc->flags.bit.antic)
			entry->registers[0] &= ~ADC_M_R2_ANTICOINC_ENA;
		     else
			entry->registers[0] |= ADC_M_R2_ANTICOINC_ENA;

		/* Send new mode selection value to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLATEC:	/* Late (vs early) coinc mode	*/
					/*------------------------------*/

		/* Set / Clear the late coincidence mode bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->flags.bit.latec = *((LONG *) params->value);
		     if (!entry) break;

		     if (adc->flags.bit.latec)
			entry->registers[0] |= ADC_M_R2_EARLYCOINC_ENA;
		     else
			entry->registers[0] &= ~ADC_M_R2_EARLYCOINC_ENA;

		/* Send new mode selection value to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFDELPK:	/* Delayed (vs auto) peak detect*/
					/*------------------------------*/

		/* Set / Clear the delayed peak detect bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->flags.bit.delpk = *((LONG *) params->value);
		     if (!entry) break;

		     if (adc->flags.bit.delpk)
			entry->registers[0] &= ~ADC_M_R2_DELAYPEAK_ENA;
		     else
			entry->registers[0] |= ADC_M_R2_DELAYPEAK_ENA;

		/* Send new mode selection value to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFNONOV:	/* Non-overlap transfer mode	*/
					/*------------------------------*/

		/* Set / Clear the non-overlap transfer bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->flags.bit.nonov = *((LONG *) params->value);
		     if (!entry) break;

		     if (adc->flags.bit.nonov)
			entry->registers[0] &= ~ADC_M_R2_NONOVERLAP_ENA;
		     else
			entry->registers[0] |= ADC_M_R2_NONOVERLAP_ENA;

		/* Send new mode selection value to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLTCPUR:	/* LTC/PUR Output signal	*/
					/*------------------------------*/

		/* Set / Clear the LTC/PUR EOC bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->flags.bit.ltcpur = *((LONG *) params->value);
		     if (!entry) break;

		     if (adc->flags.bit.ltcpur)
			entry->registers[0] &= ~ADC_M_R2_LTCPUR_EOC;
		     else
			entry->registers[0] |= ADC_M_R2_LTCPUR_EOC;

		/* Send new mode selection value to the ICB module */

		     s = icb_output (index, 2, 1, &entry->registers[0]);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFATTEN:	/* Module requires attention	*/
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			adc->flags.bit.atten = *((LONG *) params->value);
		     if (!entry) break;

		     if (adc->flags.bit.atten)
			entry->r0_write |=  ICB_M_CTRL_LED_YELLOW;
		     else
			entry->r0_write &= ~ICB_M_CTRL_LED_YELLOW;
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */

	    if (s != OK) return s;

	} /* End of for() */

/*
* If the load dac flag is set to true, write something to the ADC's load
* DAC register
*/
	if (load_dac) {
		reg_val = 0;
	    s = icb_output (index, 13, 1, &reg_val);
	    if (s != OK) return s;
	}

/*
* Set the cached flags for cache only operations
*/
	if ((flags & ICB_M_HDLR_WRITE_CACHE) == ICB_M_HDLR_WRITE_CACHE)
	    icb_set_cached_flags ((ICB_CCNIM_ANY *) adc, params_start);

/*
* Verify the new module settings.
*/
	if ((flags & ICB_M_HDLR_NOVERIFY) == 0)
	    if (entry) s = icb_adc_verify (adc, params_start, 0, NULL);

	return s;
}


/*******************************************************************************
*
* ICB_ADC_READ reads the specified parameters from the ADC ICB module.
* This function assumes that the registers in the module were just read into
* the ICB_MODULE_INFO structure!
*
* Its calling format is:
*
*	status = ICB_ADC_READ (adc, params, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "adc" (ICB_CCNIM_ADC *) is the pointer to the structure that caches info
*  about the ADC.
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be read from the ADC module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_adc_read (ICB_CCNIM_ADC *adc,
		   ICB_PARAM_LIST *params,
		   LONG flags)

{
  LONG index;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *params_start;

/*
* Get a pointer to the icb module database entry.
*/
	index = adc->icb_index;
	if (index != -1) {
	    entry = &icb_module_info[adc->icb_index];
	    if (entry->present == FALSE) entry = NULL;
	} else
	    entry = NULL;

/*
* Loop until the parameter list is complete
*/
	params_start = params;
	for (; params->pcode != 0; params++) {

	/*--------------------------------*/
	/* Read the value from the module */
	/*--------------------------------*/

	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_L_ADCRANGE:	/* Range			*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->range;
		     break;

					/*------------------------------*/
		case CAM_L_ADCOFFSET:	/* Offset			*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->offset;
		     break;

					/*------------------------------*/
		case CAM_T_ADCACQMODE:	/* Acquisition Mode		*/
					/*------------------------------*/

		     icb_str2dsc (params->value, adc->acqmode);
		     break;

					/*------------------------------*/
		case CAM_L_CNVGAIN:	/* Conversion Gain		*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->cnvgain;
		     break;

					/*------------------------------*/
		case CAM_F_LLD:		/* Lower level discriminator	*/
					/*------------------------------*/

		     *((REAL *) params->value) = adc->lld;
		     break;

					/*------------------------------*/
		case CAM_F_ULD:		/* Upper level discriminator	*/
					/*------------------------------*/

		     *((REAL *) params->value) = adc->uld;
		     break;

					/*------------------------------*/
		case CAM_F_ZERO:	/* Zero				*/
					/*------------------------------*/

		     *((REAL *) params->value) = adc->zero;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLAGS:	/* ADC Mode flags		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = adc->flags.lword;
		     continue;

					/*------------------------------*/
		case CAM_L_ADCFANTIC:	/* Anti-coincidence mode	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->flags.bit.antic;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLATEC:	/* Late (vs early) coinc mode	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->flags.bit.latec;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFDELPK:	/* Delayed (vs auto) peak detect*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->flags.bit.delpk;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFNONOV:	/* Non-overlap transfer mode	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->flags.bit.nonov;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLTCPUR:	/* LTC/PUR Output signal	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->flags.bit.ltcpur;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFONLINE:	/* Module on-line		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = adc->flags.bit.online;
		     break;

					/*------------------------------*/
		case CAM_L_ADCFATTEN:	/* Module requires attention	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = adc->flags.bit.atten;
		     break;

		/*======================================================*/
		/* Verify Parameter Flags Section...			*/
		/*======================================================*/

					/*------------------------------*/
		case CAM_L_ADCVFLAGS:	/* Verify Flags field		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = adc->vflags.lword;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFID:	/* VF: Module ID 		*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.id;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFRANGE:	/* VF: Range			*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.range;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFOFFSET:	/* VF: Offset			*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.offset;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFACQMODE:/* VF: Acquisition Mode		*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.acqmode;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFCNVGAIN:/* VF: Conversion Gain		*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.cnvgain;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFLLD:	/* VF: Lower level discriminator*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.lld;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFULD:	/* VF: Upper level discriminator*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.uld;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFZERO:	/* VF: Zero			*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.zero;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFANTIC:	/* VF: Anti-coinc. mode		*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.antic;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFLATEC:	/* VF: Late coinc mode		*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.latec;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFDELPK:	/* VF: Delayed peak detect	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.delpk;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFNONOV:	/* VF: Non-overlap transfer	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.nonov;
		     break;

					/*------------------------------*/
		case CAM_L_ADCVFLTCPUR:	/* VF: LTC/PUR Output	 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = adc->vflags.bit.ltcpur;
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */


	} /* End of for() */


	return OK;
}



/*******************************************************************************
*
* ICB_ADC_VERIFY is called from ICB_ADC_WRITE() after one or more parameters
* values have been written to the ADC module.  It reads the registers of the
* ADC and compares them with what is stored in the ICB database.
*
* Its calling format is:
*
*	status = ICB_ADC_VERIFY (adc, params, flags, reg_list)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "adc" (ICB_CCNIM_ADC *) is the pointer to the structure that caches info
*  about the ADC.
*
*  "params" (ICB_PARAM_LIST array) is a zero terminated list of CAM parameters
*  that have been applied to the HVPS module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*  "reg_list" (UBYTE *) The list of ICB registers.  This should be NULL
*  when ICB_ADC_VERIFY() is called from the outside.
*
*******************************************************************************/

LONG icb_adc_verify (ICB_CCNIM_ADC *adc,
		     ICB_PARAM_LIST *params,
		     LONG flags,
		     UBYTE *reg_list)

{
  LONG s;
  UBYTE registers[14];
  UBYTE lreg;
  UBYTE mreg;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.  If an entry does not
* exist or the module is not present, get out.
*/
	if (adc->icb_index == -1) return OK;
	entry = &icb_module_info[adc->icb_index];
	if (entry->present == FALSE) return OK;

/*
* Read in the module registers 2 though 16.
*/
	if (reg_list == NULL) {
	    s = icb_input (adc->icb_index, 2, 14, registers);
	    if (s != OK) return s;
	} else
	    memcpy (registers, reg_list, 14);

/*
* Loop until the parameter list is complete
*/
	for (; params->pcode != 0; params++) {

	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_L_ADCRANGE:	/* Range			*/
					/*------------------------------*/

		     lreg = entry->registers[1] & 0xF0;
		     mreg =        registers[1] & 0xF0;
		     adc->vflags.bit.range = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_ADCOFFSET:	/* Offset			*/
					/*------------------------------*/

		     lreg = entry->registers[10] & 0x7F;
		     mreg =        registers[10] & 0x7F;
		     adc->vflags.bit.offset = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_T_ADCACQMODE:	/* Acquisition Mode		*/
					/*------------------------------*/

		     lreg = entry->registers[0] & ADC_M_R2_ACQMODE_SVA;
		     mreg =        registers[0] & ADC_M_R2_ACQMODE_SVA;
		     adc->vflags.bit.acqmode = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_CNVGAIN:	/* Conversion Gain		*/
					/*------------------------------*/

		     lreg = entry->registers[1] & 0x0F;
		     mreg =        registers[1] & 0x0F;
		     adc->vflags.bit.cnvgain = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_F_LLD:		/* Lower level discriminator	*/
					/*------------------------------*/

		     registers[3] &= 0x0F;
		     adc->vflags.bit.lld =
			((entry->registers[2] != registers[2]) ||
			 (entry->registers[3] != registers[3]));
		     break;

					/*------------------------------*/
		case CAM_F_ULD:		/* Upper level discriminator	*/
					/*------------------------------*/

		     registers[5] &= 0x0F;
		     adc->vflags.bit.uld = 
			((entry->registers[4] != registers[4]) ||
			 (entry->registers[5] != registers[5]));
		     break;

					/*------------------------------*/
		case CAM_F_ZERO:	/* Zero				*/
					/*------------------------------*/

		     registers[7] &= 0x0F;
		     adc->vflags.bit.zero = 
			((entry->registers[6] != registers[6]) ||
			 (entry->registers[7] != registers[7]));
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLAGS:	/* ADC Mode flags		*/
					/*------------------------------*/

		     icb_adc_verify (adc, adc_flag_list, flags, registers);
		     continue;

					/*------------------------------*/
		case CAM_L_ADCFANTIC:	/* Anti-coincidence mode	*/
					/*------------------------------*/

		     lreg = entry->registers[0] & ADC_M_R2_ANTICOINC_ENA;
		     mreg =        registers[0] & ADC_M_R2_ANTICOINC_ENA;
		     adc->vflags.bit.antic = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLATEC:	/* Late (vs early) coinc mode	*/
					/*------------------------------*/

		     lreg = entry->registers[0] & ADC_M_R2_EARLYCOINC_ENA;
		     mreg =        registers[0] & ADC_M_R2_EARLYCOINC_ENA;
		     adc->vflags.bit.latec = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFDELPK:	/* Delayed (vs auto) peak detect*/
					/*------------------------------*/

		     lreg = entry->registers[0] & ADC_M_R2_DELAYPEAK_ENA;
		     mreg =        registers[0] & ADC_M_R2_DELAYPEAK_ENA;
		     adc->vflags.bit.delpk = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFNONOV:	/* Non-overlap transfer mode	*/
					/*------------------------------*/

		     lreg = entry->registers[0] & ADC_M_R2_NONOVERLAP_ENA;
		     mreg =        registers[0] & ADC_M_R2_NONOVERLAP_ENA;
		     adc->vflags.bit.nonov = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_ADCFLTCPUR:	/* LTC/PUR Output signal	*/
					/*------------------------------*/

		     lreg = entry->registers[0] & ADC_M_R2_LTCPUR_EOC;
		     mreg =        registers[0] & ADC_M_R2_LTCPUR_EOC;
		     adc->vflags.bit.ltcpur = (lreg != mreg);
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */

	} /* End of for() */

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_WRITE writes the specified parameters to the amplifier ICB module.
*
* Its calling format is:
*
*	status = ICB_AMP_WRITE (amp, params, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be applied to the amplifier module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_amp_write (ICB_CCNIM_AMP *amp,
		    ICB_PARAM_LIST *params,
		    LONG flags)

{
  LONG i;
  LONG s=OK;
  ULONG dac;
  ULONG fbits;
  LONG index;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *flag_list;
  LONG *flag_vals;
  LONG local_flags;
  ICB_PARAM_LIST *params_start;

/*
* Get a pointer to the icb module database entry.
*/
	index = amp->icb_index;
	if ((index != -1) && ((flags & ICB_M_HDLR_CACHE_ONLY) == 0)) {
	    entry = &icb_module_info[amp->icb_index];
	    if (entry->present == FALSE) entry = NULL;
	} else
	    entry = NULL;

/*
* Loop until the parameter list is complete
*/
	params_start = params;
	for (; params->pcode != 0; params++) {

	/*-------------------------------*/
	/* Write the value to the module */
	/*-------------------------------*/

	    s = OK;
	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_T_PRAMPTYPE:	/* Preamplifier type (RC/TRP)	*/
					/*------------------------------*/

		/* Set or clear the preamp type bit (1=Trans. Reset Preamp) */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			icb_dsc2str (amp->pramptype, params->value, 8);
		     if (!entry) break;

		     if (strnicmp (amp->pramptype, "TRP", 3) == 0)
			entry->registers[4] |= AMP_M_R6_PREAMP_TRP;
		     else if (strnicmp (amp->pramptype, "RC", 2) == 0)
			entry->registers[4] &= ~AMP_M_R6_PREAMP_TRP;
		     else {
			strcpy (amp->pramptype, "RC");
			entry->registers[4] &= ~AMP_M_R6_PREAMP_TRP;
		     }

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*------------------------------*/
		case CAM_F_AMPGAIN:	/* Virtual Amplifier Gain	*/
					/*------------------------------*/
		     
		     if ((flags & ICB_M_HDLR_INITIALIZE) != 0) break;

		     amp->gain = *((REAL *) params->value);
		     s = icb_amp_vgain_to_hwgain (amp, flags);
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN1:	/* Course GAIN		 	*/
					/*------------------------------*/

		/* Convert floating course gain into a DAC value */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->hwgain1 = *((REAL *) params->value);

		     for (i = 0; amp_hwgain1_tbl[i].real_val != 0.0; i++)
			if (amp->hwgain1 >= amp_hwgain1_tbl[i].real_val) break;
		     amp->hwgain1 = amp_hwgain1_tbl[i].real_val;
		     amp->gain    = amp->hwgain1 * amp->hwgain2 * amp->hwgain3;
		     if (!entry) break;

		/* Send new course gain byte to the ICB module */

		     entry->registers[8] = amp_hwgain1_tbl[i].dac_val;
		     s = icb_output (index, 10, 1, &entry->registers[8]);
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN2:	/* Fine Gain		 	*/
					/*------------------------------*/

		/* Setting the stepper motor in the AMP is too complicated */
		/* to be done here in a switch statement.		   */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->hwgain2 = *((REAL *) params->value);
		     if (amp->hwgain2 < 1.0) amp->hwgain2 = 1.0;
		     if (amp->hwgain2 > 3.0) amp->hwgain2 = 3.0;
		     amp->gain = amp->hwgain1 * amp->hwgain2 * amp->hwgain3;

		     if (!entry) break;
		     s = icb_amp_write_gain2 (amp);
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN3:	/* Super Fine Gain	 	*/
					/*------------------------------*/

		/* Convert floating super find gain into a DAC value */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->hwgain3 = *((REAL *) params->value);
		     if (amp->hwgain3 < 0.998) amp->hwgain3 = 0.998;
		     if (amp->hwgain3 > 1.002) amp->hwgain3 = 1.002;
		     amp->gain = amp->hwgain1 * amp->hwgain2 * amp->hwgain3;
		     if (!entry) break;


		/* Send new super fine gain byte to the ICB module */

		     dac = (amp->hwgain3 - 0.998) * 63750.0;
		     entry->registers[3] = dac;
		     s = icb_output (index, 5, 1, &entry->registers[3]);
		     break;

					/*------------------------------*/
		case CAM_T_AMPSHAPEMODE:/* Amplifier shaping mode	*/
					/*------------------------------*/

		/* Set or clear the shape mode bit (1=Triangular shape Mode */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			icb_dsc2str (amp->shapemode, params->value, 8);
		     if (!entry) break;

		     if (strnicmp (amp->shapemode, "TRIANGLE", 8) == 0)
			entry->registers[4] |= AMP_M_R6_SHAPEMODE_TRIANGLE;
		     else
			entry->registers[4] &= ~AMP_M_R6_SHAPEMODE_TRIANGLE;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*------------------------------*/
		case CAM_L_AMPPZ:	/* Amplifier pole zero	 	*/
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->pz = *((LONG *) params->value);
		     if (!entry) break;

		/* Store the pole zero in the module */

		     s = icb_amp_write_pz (amp);
		     break;

					/*------------------------------*/
		case CAM_T_AMPBLRTYPE:	/* Base-line restore (SYM,ASYM)	*/
					/*------------------------------*/

		/* Set or clear the baseline restore bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			icb_dsc2str (amp->blrtype, params->value, 8);
		     if (!entry) break;

		     if (strnicmp (amp->blrtype, "ASYM", 4) == 0)
			entry->registers[4] |= AMP_M_R6_BLRMODE_ASYM;
		     else
			entry->registers[4] &= ~AMP_M_R6_BLRMODE_ASYM;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*-------------------------------*/
		case CAM_T_AMPDTCTYPE:	/* Dead-time control (Norm, LFC) */
					/*-------------------------------*/

		/* Set or clear the dead time control bit (1=LFC) */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			icb_dsc2str (amp->dtctype, params->value, 8);
		     if (!entry) break;

		     if (strnicmp (amp->dtctype, "LFC", 3) == 0)
			entry->registers[4] |= AMP_M_R6_LTCMODE_LFC;
		     else
			entry->registers[4] &= ~AMP_M_R6_LTCMODE_LFC;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*------------------------------*/
		case CAM_F_AMPTC:	/* Amplifier time constant	*/
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->tc = *((REAL *) params->value);
		     break;

					/*------------------------------*/
		case CAM_L_AMPFLAGS:	/* Amplifier Mode flags	  	*/
					/*------------------------------*/

		/* Build a flag parameter list based on the flag bit settings */
			
		     flag_list = (ICB_PARAM_LIST *) malloc (sizeof (amp_flag_list));
		     flag_vals = (LONG *) malloc (sizeof (amp_flag_bits));
		     memcpy (flag_list, amp_flag_list, sizeof (amp_flag_list));

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0) {
			fbits = *((LONG *) params->value);
			for (i = 0; flag_list[i].pcode != 0; i++) {
			    flag_vals[i] = ((amp_flag_bits[i] & fbits) != 0);
			    flag_list[i].value = &flag_vals[i];
			}
		     }

		/* Send the individual flag values */

		     local_flags = flags | ICB_M_HDLR_NOVERIFY;
		     s = icb_amp_write (amp, flag_list, local_flags);
		     free (flag_list);
		     free (flag_vals);
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFDIFF:	/* Differential (vs normal) input */
					/*--------------------------------*/

		/* Set or clear the differential input bit */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->flags.bit.diff = *((LONG *) params->value);
		     if (!entry) break;

		     if (amp->flags.bit.diff)
			entry->registers[4] |= AMP_M_R6_INPUT_DIFF;
		     else
			entry->registers[4] &= ~AMP_M_R6_INPUT_DIFF;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*---------------------------------*/
		case CAM_L_AMPFNEGPOL:	/* Negative (vs positive) polarity */
					/*---------------------------------*/

		/* Set or clear the input polarity bit (1=negative) */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->flags.bit.negpol = *((LONG *) params->value);
		     if (!entry) break;

		     if (amp->flags.bit.negpol)
			entry->registers[4] |= AMP_M_R6_INPPOLARITY_NEG;
		     else
			entry->registers[4] &= ~AMP_M_R6_INPPOLARITY_NEG;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*------------------------------*/
		case CAM_L_AMPFCOMPINH:	/* Complement inhibit polarity	*/
					/*------------------------------*/

		/* Set or clear the inhibit polarity bit  */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->flags.bit.compinh = *((LONG *) params->value);
		     if (!entry) break;

		     if (amp->flags.bit.compinh)
			entry->registers[4] |= AMP_M_R6_INHPOLARITY_NEG;
		     else
			entry->registers[4] &= ~AMP_M_R6_INHPOLARITY_NEG;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFPUREJ:	/* Pileup reject (enable/disable) */
					/*--------------------------------*/

		/* Set or clear the pileup reject bit (1=negative) */

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->flags.bit.purej = *((LONG *) params->value);
		     if (!entry) break;

		     if (amp->flags.bit.purej)
			entry->registers[4] |= AMP_M_R6_PILEUPREJ_ENA;
		     else
			entry->registers[4] &= ~AMP_M_R6_PILEUPREJ_ENA;

		/* Send setup byte to the ICB module */

		     s = icb_output (index, 6, 1, &entry->registers[4]);
		     break;

					/*------------------------------*/
		case CAM_L_AMPFPZSTART:	/* Begin amp pole zero adjust	*/
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) != 0) break;
		     if (*((LONG *) params->value) == 0) break;

		/* Performing an automatic pole zero in the AMP is too */
		/* complicated to be done here in a switch statement.  */

		     if (!entry) break;
		     s = icb_amp_start_pz (amp);
		     break;

					/*------------------------------*/
		case CAM_L_AMPFATTEN:	/* Module requires attention	*/
					/*------------------------------*/

		     if ((flags & ICB_M_HDLR_INITIALIZE) == 0)
			amp->flags.bit.atten = *((LONG *) params->value);
		     if (!entry) break;

		     if (amp->flags.bit.atten)
			entry->r0_write |=  ICB_M_CTRL_LED_YELLOW;
		     else
			entry->r0_write &= ~ICB_M_CTRL_LED_YELLOW;
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */

	    if (s != OK) return s;

	} /* End of for() */


/*
* Set the cached flags for cache only operations
*/
	if ((flags & ICB_M_HDLR_WRITE_CACHE) == ICB_M_HDLR_WRITE_CACHE)
	    icb_set_cached_flags ((ICB_CCNIM_ANY *) amp, params_start);

/*
* Verify the new module settings.
*/
	if ((flags & ICB_M_HDLR_NOVERIFY) == 0)
	    if (entry) s = icb_amp_verify (amp, params_start, 0, NULL);

	return s;
}


/*******************************************************************************
*
* ICB_AMP_READ reads the specified parameters from the amplifier ICB module.
* This function assumes that the registers in the module were just read into
* the ICB_MODULE_INFO structure!
*
* Its calling format is:
*
*	status = ICB_AMP_READ (amp, params, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*  "params" (ICB_PARAM_LIST *) is a zero terminated list of CAM parameters that
*  are to be read from the AMP module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_amp_read (ICB_CCNIM_AMP *amp,
		   ICB_PARAM_LIST *params,
		   LONG flags)

{
  LONG i;
  ULONG reg;
  LONG  index;
  ICB_MODULE_INFO *entry;
  ICB_PARAM_LIST *params_start;

/*
* Get a pointer to the icb module database entry.
*/
	index = amp->icb_index;
	if (index != -1) {
	    entry = &icb_module_info[amp->icb_index];
	    if (entry->present == FALSE) entry = NULL;
	} else
	    entry = NULL;

/*
* Loop until the parameter list is complete
*/
	params_start = params;
	for (; params->pcode != 0; params++) {

	/*--------------------------------*/
	/* Read the value from the module */
	/*--------------------------------*/

	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_T_PRAMPTYPE:	/* Preamplifier type (RC/TRP)	*/
					/*------------------------------*/

		     icb_str2dsc (params->value, amp->pramptype);
		     break;

					/*------------------------------*/
		case CAM_F_AMPGAIN:	/* Virtual Amplifier Gain	*/
					/*------------------------------*/

		     *((REAL *) params->value) = amp->gain;
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN1:	/* Course GAIN		 	*/
					/*------------------------------*/

		     *((REAL *) params->value) = amp->hwgain1;
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN2:	/* Fine Gain		 	*/
					/*------------------------------*/

		     *((REAL *) params->value) = amp->hwgain2;
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN3:	/* Super Fine Gain	 	*/
					/*------------------------------*/

		     *((REAL *) params->value) = amp->hwgain3;
		     break;

					/*------------------------------*/
		case CAM_T_AMPSHAPEMODE:/* Amplifier shaping mode	*/
					/*------------------------------*/

		     icb_str2dsc (params->value, amp->shapemode);
		     break;

					/*------------------------------*/
		case CAM_L_AMPPZ:	/* Amplifier pole zero	 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->pz;
		     break;

					/*------------------------------*/
		case CAM_T_AMPBLRTYPE:	/* Base-line restore (SYM,ASYM)	*/
					/*------------------------------*/

		     icb_str2dsc (params->value, amp->blrtype);
		     break;

					/*-------------------------------*/
		case CAM_T_AMPDTCTYPE:	/* Dead-time control (Norm, LFC) */
					/*-------------------------------*/

		     icb_str2dsc (params->value, amp->dtctype);
		     break;

					/*------------------------------*/
		case CAM_F_AMPTC:	/* Amplifier time constant	*/
					/*------------------------------*/

		     if (!entry) {
			*((REAL *) params->value) = amp->tc;
			break;
		     }

		/* Bits 0-5 of Register 7 contain the current TC */
		/* Logic 0 indicates the setting.  Bit 0 ==> 0.5 */
		/* Bit 5 ==> 12 us				 */

		     reg = ((ULONG) entry->registers[5]);
		     for (i = 0; i < 6; i++, reg >>= 1)
			if ((reg & 1) == 0) break;
		     *((REAL *) params->value) = amp_shape_time[i];

		     if (amp->tc != amp_shape_time[i]) {
			amp->vflags.bit.tc = TRUE;
			amp->tc            = amp_shape_time[i];
		     }
		     break;

					/*------------------------------*/
		case CAM_L_AMPFLAGS:	/* Amplifier Mode flags	  	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->flags.lword;
		     continue;

					/*--------------------------------*/
		case CAM_L_AMPFDIFF:	/* Differential (vs normal) input */
					/*--------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.diff;
		     break;

					/*---------------------------------*/
		case CAM_L_AMPFNEGPOL:	/* Negative (vs positive) polarity */
					/*---------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.negpol;
		     break;

					/*------------------------------*/
		case CAM_L_AMPFCOMPINH:	/* Complement inhibit polarity	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.compinh;
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFPUREJ:	/* Pileup reject (enable/disable) */
					/*--------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.purej;
		     break;

					/*------------------------------*/
		case CAM_L_AMPFONLINE:	/* Module on-line		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.online;
		     break;

					 /*-------------------------------*/
		case CAM_L_AMPFMOTRBUSY: /* Motor Busy 			  */
					 /*-------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.motrbusy;
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFPZBUSY:	/* Pole Zero Busy		  */
					/*--------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.pzbusy;
		     break;

					/*------------------------------*/
		case CAM_L_AMPFATTEN:	/* Module requires attention	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.atten;
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFPZFAIL:	/* Pole Zero Failed		  */
					/*--------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.pzfail;
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFMOTRFAIL:/* Fine Gain Motor Failed	  */
					/*--------------------------------*/

		     *((ULONG *) params->value) = amp->flags.bit.motrfail;
		     break;

		/*======================================================*/
		/* Verify Parameter Flags Section...			*/
		/*======================================================*/

					/*------------------------------*/
		case CAM_L_AMPVFLAGS:	/* Verify Flags field		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->vflags.lword;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFID:	/* VF: Module ID 		*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.id;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFPRAMPT:	/* VF: Preamplifier type 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.prampt;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFHWGAIN1:/* VF: Course GAIN	 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.hwgain1;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFHWGAIN2:/* VF: Fine Gain	 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.hwgain2;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFHWGAIN3:/* VF: Super Fine Gain	 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.hwgain3;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFSHAPEM:	/* VF: Amplifier shaping mode	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.shapem;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFPZ:	/* VF: Amplifier pole zero 	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.pz;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFBLRTYPE:/* VF: Base-line restore	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.blrtype;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFDTCTYPE:/* VF: Dead-time control	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.dtctype;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFTC:	/* VF: Amplifier time constant	*/
					/*------------------------------*/

		     *((LONG *) params->value) = amp->vflags.bit.tc;
		     break;

					/*-------------------------------*/
		case CAM_L_AMPVFDIFF:	/* VF: Differential input	 */
					/*-------------------------------*/

		     *((ULONG *) params->value) = amp->vflags.bit.diff;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFNEGPOL:	/* VF: Negative polarity	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->vflags.bit.negpol;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFCOMPINH:/* VF: Comp. inhibit polarity	*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->vflags.bit.compinh;
		     break;

					/*------------------------------*/
		case CAM_L_AMPVFPUREJ:	/* VF: Pileup reject 		*/
					/*------------------------------*/

		     *((ULONG *) params->value) = amp->vflags.bit.purej;
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */


	} /* End of for() */

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_VERIFY is called from ICB_AMP_WRITE() after one or more parameters
* values have been written to the AMP module.  It reads the registers of the
* AMP and compares them with what is stored in the ICB database.
*
* Its calling format is:
*
*	status = ICB_AMP_VERIFY (amp, params, flags, reg_list)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the Amplifier.
*
*  "params" (ICB_PARAM_LIST array) is a zero terminated list of CAM parameters
*  that have been applied to the HVPS module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*  "reg_list" (UBYTE *) The list of ICB registers.  This should be NULL
*  when ICB_AMP_VERIFY() is called from the outside.
*
*******************************************************************************/

LONG icb_amp_verify (ICB_CCNIM_AMP *amp,
		     ICB_PARAM_LIST *params,
		     LONG flags,
		     UBYTE *reg_list)

{
  LONG i;
  LONG s;
  UBYTE registers[14];
  UBYTE lreg;
  UBYTE mreg;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.  If an entry does not
* exist or the module is not present, get out.
*/
	if (amp->icb_index == -1) return OK;
	entry = &icb_module_info[amp->icb_index];
	if (entry->present == FALSE) return OK;

/*
* Read in the module registers 2 though 16.
*/
	if (reg_list == NULL) {
	    s = icb_input (amp->icb_index, 2, 14, registers);
	    if (s != OK) return s;
	} else
	    memcpy (registers, reg_list, 14);

/*
* Loop until the parameter list is complete
*/
	for (; params->pcode != 0; params++) {

	    switch (params->pcode) {

					/*------------------------------*/
		case CAM_T_PRAMPTYPE:	/* Preamplifier type (RC/TRP)	*/
					/*------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_PREAMP_TRP;
		     mreg =        registers[4] & AMP_M_R6_PREAMP_TRP;
		     amp->vflags.bit.prampt = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN1:	/* Course GAIN		 	*/
					/*------------------------------*/

		     lreg = entry->registers[8] & 0x0F;
		     mreg =        registers[8] & 0x0F;
		     amp->vflags.bit.hwgain1 = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN2:	/* Fine Gain		 	*/
					/*------------------------------*/

		     /* Verify is done in ICB_AMP_TEST_GAIN2_COMPLETE() */

		     if (amp->flags.bit.motrbusy)
			amp->vflags.bit.hwgain2 = FALSE;
		     break;

					/*------------------------------*/
		case CAM_F_AMPHWGAIN3:	/* Super Fine Gain	 	*/
					/*------------------------------*/

		     amp->vflags.bit.hwgain3 = 
			(entry->registers[3] != registers[3]);
		     break;

					/*------------------------------*/
		case CAM_T_AMPSHAPEMODE:/* Amplifier shaping mode	*/
					/*------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_SHAPEMODE_TRIANGLE;
		     mreg =        registers[4] & AMP_M_R6_SHAPEMODE_TRIANGLE;
		     amp->vflags.bit.shapem = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_AMPPZ:	/* Amplifier pole zero	 	*/
					/*------------------------------*/

		     if (amp->flags.bit.pzbusy == TRUE) break;
		     mreg = registers[6]        & 0x0F;
		     lreg = entry->registers[6] & 0x0F;
		     amp->vflags.bit.pz =
			((lreg != mreg) || (entry->registers[7] != registers[7]));
		     break;

					/*------------------------------*/
		case CAM_T_AMPBLRTYPE:	/* Base-line restore (SYM,ASYM)	*/
					/*------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_BLRMODE_ASYM;
		     mreg =        registers[4] & AMP_M_R6_BLRMODE_ASYM;
		     amp->vflags.bit.blrtype = (lreg != mreg);
		     break;

					/*-------------------------------*/
		case CAM_T_AMPDTCTYPE:	/* Dead-time control (Norm, LFC) */
					/*-------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_LTCMODE_LFC;
		     mreg =        registers[4] & AMP_M_R6_LTCMODE_LFC;
		     amp->vflags.bit.dtctype = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_F_AMPTC:	/* Amplifier time constant	*/
					/*------------------------------*/

		     lreg = 1;
		     for (i = 0; amp_shape_time[i] != 0.0; i++, lreg <<= 1)
			if (amp->tc == amp_shape_time[i]) break;
		     lreg = (~lreg) & 0x3F;
		     registers[5] &= 0x3F;
		     amp->vflags.bit.tc = (lreg != registers[5]);

		     /* If no match, load the actual value */

		     if (amp->vflags.bit.tc == 1) {
			lreg = registers[5];
			for (i = 0; i < 6; i++, lreg >>= 1)
			    if ((lreg & 1) == 0) break;
		        amp->tc = amp_shape_time[i];
		     }
		     break;

					/*------------------------------*/
		case CAM_L_AMPFLAGS:	/* Amplifier Mode flags	  	*/
					/*------------------------------*/

		     icb_amp_verify (amp, amp_flag_list, flags, registers);
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFDIFF:	/* Differential (vs normal) input */
					/*--------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_INPUT_DIFF;
		     mreg =        registers[4] & AMP_M_R6_INPUT_DIFF;
		     amp->vflags.bit.diff = (lreg != mreg);
		     break;

					/*---------------------------------*/
		case CAM_L_AMPFNEGPOL:	/* Negative (vs positive) polarity */
					/*---------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_INPPOLARITY_NEG;
		     mreg =        registers[4] & AMP_M_R6_INPPOLARITY_NEG;
		     amp->vflags.bit.negpol = (lreg != mreg);
		     break;

					/*------------------------------*/
		case CAM_L_AMPFCOMPINH:	/* Complement inhibit polarity	*/
					/*------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_INHPOLARITY_NEG;
		     mreg =        registers[4] & AMP_M_R6_INHPOLARITY_NEG;
		     amp->vflags.bit.compinh = (lreg != mreg);
		     break;

					/*--------------------------------*/
		case CAM_L_AMPFPUREJ:	/* Pileup reject (enable/disable) */
					/*--------------------------------*/

		     lreg = entry->registers[4] & AMP_M_R6_PILEUPREJ_ENA;
		     mreg =        registers[4] & AMP_M_R6_PILEUPREJ_ENA;
		     amp->vflags.bit.purej = (lreg != mreg);
		     break;

					/*------------------------------*/
		default:		/* Unrecognized parameter	*/
					/*------------------------------*/
		     continue;

	    } /* End of switch() */

	} /* End of for() */

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_VGAIN_TO_HWGAIN converts the cached virtual gain value into 3
* hardware gain values.
*
* Its calling format is:
*
*	status = ICB_AMP_VGAIN_TO_HWGAIN (amp, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

LONG icb_amp_vgain_to_hwgain (ICB_CCNIM_AMP *amp,
			      LONG flags)

{
  LONG i;		/* Loop counter			*/
  REAL vg;		/* Intermediate virtual gain	*/
  static REAL cg;	/* Computed course gain		*/
  static REAL fg;	/* Computed fine gain		*/
  static REAL sfg;	/* Computed super fine gain	*/

  ICB_PARAM_LIST plist[] = {{CAM_F_AMPHWGAIN1, 0, &cg},
			    {CAM_F_AMPHWGAIN2, 0, &fg},
			    {CAM_F_AMPHWGAIN3, 0, &sfg},
			    {0,                0, 0}};

/*
* Make sure the current value is reasonable
*/
	if (amp->gain <    5.0) amp->gain =    5.0;
	if (amp->gain > 1503.0) amp->gain = 1503.0;

/*
* Divide virtual gain by possible course gains until a course gain
* is found that results in a fine gain value between 1.0 and 3.0.
*/
	for (i = 0; amp_hwgain1_tbl[i].real_val != 0.0; i++) {
	    fg = amp->gain / amp_hwgain1_tbl[i].real_val;
	    if ((fg >= 1.0) && (fg <= 3.0)) break;
	}
	cg = amp_hwgain1_tbl[i].real_val;

/*
* Adjust the fine gain so that it has only 3 digits right of the decimal.
*/
	i = (fg *= 1000.0);
	if ((fg - ((REAL) i)) >= 0.5) i++;	/* Round up if necessary */
	fg = ((REAL) i) / 1000.0;

/*
* Use this adjusted fine gain to compute the super fine gain.  Make
* sure the super fine gain is within reason.
*/
	vg  = cg * fg;
	sfg = amp->gain / vg;
	if (sfg < 0.998) sfg = 0.998;
	else if (sfg > 1.002) sfg = 1.002;

/*
* Adjust the super gain so that it has only 4 digits right of the decimal.
*/
	i = (sfg *= 10000.0);
	if ((sfg - ((REAL) i)) >= 0.5) i++;	/* Round up if necessary */
	sfg = ((REAL) i) / 10000.0;

/*
* Set the hardware gains
*/
	return icb_amp_write (amp, plist, flags);
}


/*******************************************************************************
*
* ICB_AMP_WRITE_GAIN2 writes the new fine gain value to the amplifier.
*
* Its calling format is:
*
*	status = ICB_AMP_WRITE_GAIN2 (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_write_gain2 (ICB_CCNIM_AMP *amp)
/* 
   This routine runs as an AST on VMS systems.  This means it runs as
   a separate "thread".  Under EPICS this routine presently runs inline
   and thus causes a significant delay when waiting for the stepper motor.  
   In the future we could simply make this routine run as a separate vxWorks 
   task.
*/

{
  LONG s;
  LONG nvpos;
  ULONG pos;
  ICB_MODULE_INFO *entry;
  double dt_qsec = 0.25;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];


while (1) {
   /*
   * Set our busy flag and see if the the module itself is busy?  If the
   * module is busy wait 1/4 second.  HOW DOES ENTRY->REGISTERS GET UPDATED
   * DURING THIS LOOP?
   */
	amp->flags.bit.motrbusy = 1;
	if ((entry->registers[0] & AMP_M_R2_BUSY) == 0) break;
	epicsThreadSleep(dt_qsec);
}

/*
* Has the module been reset?  If so get the last recorded position from
* the NVRAM  If that's garbage, home the motor.
*/
	if ((entry->r0_read & ICB_M_STAT_RESET) != 0) {

	    s = icb_amp_get_nvram_motor_pos (amp->icb_index, &nvpos, 0);
	    if (s != OK) return s;

	    if (nvpos <= 0x0FA0) {
		s = icb_amp_put_motor_pos (amp->icb_index, nvpos, 0);
		pos = icb_amp_compute_motor_pos (amp);
		if (nvpos == pos) {	/* Is current position correct?    */
		    amp->flags.bit.motrbusy = 0;	/* Motor not busy! */
		    return s;				/* We're done      */
		}
	    }
	    s = icb_amp_home_motor (amp);
	    return s;
	}

/*
* Otherwise, are we where we're supposed to be?  If not, home the motor.
*/
	else if ((entry->registers[0] & AMP_M_R2_EQUAL) == 0) {
	    s = icb_amp_home_motor (amp);
	    return s;
	}

/*
* Everything looks good, so begin the motor position.
*/
	s = icb_amp_complete_position (amp);
	return s;
}



/*******************************************************************************
*
* ICB_AMP_COMPLETE_POSITION completes the new motor position request.  It
* assumes that the module is not busy and the current position of the motor
* is correct.
*
* Its calling format is:
*
*	status = ICB_AMP_COMPLETE_POSITION (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_complete_position (ICB_CCNIM_AMP *amp)

{
  LONG s;
  UBYTE temp[4];
  ULONG pos;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

/*
* In case of failure, load 0xFFF into the NVRAM motor position
*/
	s = icb_amp_put_nvram_motor_pos (amp->icb_index, 0x0FFF, 0);
	if (s != OK) return s;

/*
* Compute the fine gain motor position
*/
	pos = icb_amp_compute_motor_pos (amp);

/*
* Send new fine gain bytes to the ICB module
*/
	entry->registers[0] = (pos >> 8) | AMP_M_R2_LOAD_FG11_8 |
					   AMP_M_R2_DRIVE_ENABLE;
	entry->registers[1] = pos;
	s = icb_output (amp->icb_index, 2, 2, &entry->registers[0]);
	if (s != OK) return s;

/*
* Set the Start bit
*/
	temp[0] = AMP_M_R2_START | AMP_M_R2_DRIVE_ENABLE;
	s = icb_output (amp->icb_index, 2, 1, temp);
	if (s != OK) return s;

/*
* Wait for busy to go false
*/
	s = icb_amp_test_gain2_complete (amp);
	return s;
}


/*******************************************************************************
*
* ICB_AMP_TEST_GAIN2_COMPLETE reads the module's busy flag.  If clear, it 
*
* Its calling format is:
*
*	status = ICB_AMP_TEST_GAIN2_COMPLETE (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_test_gain2_complete (ICB_CCNIM_AMP *amp)
/* 
   This routine runs as an AST on VMS systems.  This means it runs as
   a separate "thread".  Under EPICS this routine presently runs inline
   and thus causes a significant delay when waiting for the stepper motor.  
   In the future we could simply make this routine run as a separate vxWorks 
   task.
*/

{
  LONG s;
  ULONG pos;
  UBYTE temp[4];
  ICB_MODULE_INFO *entry;
  double dt_qsec = 0.25;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

while (1) {
   /*
   * Check to see if busy flag is clear.  If not, reschedule this function.
   */
	s = icb_input (amp->icb_index, 2, 1, temp);
	if (s != OK) return s;

	if ((temp[0] & AMP_M_R2_BUSY) == 0) break;
	epicsThreadSleep(dt_qsec);
}
	entry->registers[0] = temp[0];

/*
* Clear our local busy flag.
*/
	amp->flags.bit.motrbusy = 0;

/*
* Check the equal flag.  If not set, get out.
*/
	if ((temp[0] & AMP_M_R2_EQUAL) == 0) {
	    amp->flags.bit.motrfail = TRUE;
	    amp->vflags.bit.hwgain2 = TRUE;
	    return OK;
	}

/*
* If successful, load the current motor position into the NVRAM.
*/
	amp->flags.bit.motrfail = FALSE;
	amp->vflags.bit.hwgain2 = FALSE;

	pos = icb_amp_compute_motor_pos (amp);
	s = icb_amp_put_nvram_motor_pos (amp->icb_index, pos, 0);
	if (s != OK) return s;

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_HOME_MOTOR places the amplifier's fine gain motor in the home
* position.
*
* Its calling format is:
*
*	status = ICB_AMP_HOME_MOTOR (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

int icb_amp_home_motor (ICB_CCNIM_AMP *amp)

{
  LONG s;
  UBYTE temp[4];
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

/*
* Load fine gain register and motor position registers with 0
* These are registers 2-4.
*/
	temp[0] = AMP_M_R2_LOAD_FG11_8;
	temp[1] = 0;
	temp[2] = 0;
	s = icb_output (amp->icb_index, 2, 3, temp);
	if (s != OK) return s;

/*
* Set the Start, index and drive enable bits.
*/
	temp[0] = AMP_M_R2_START | AMP_M_R2_INDEX | AMP_M_R2_DRIVE_ENABLE;
	s = icb_output (amp->icb_index, 2, 1, temp);
	if (s != OK) return s;

/*
* Wait for busy to go false
*/
	s = icb_amp_test_home (amp);
	return s;
}


/*******************************************************************************
*
* ICB_AMP_TEST_HOME waits for the motor to return to the home positon.
* When done, it completes the position request.
*
* Its calling format is:
*
*	status = ICB_AMP_TEST_HOME (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_test_home (ICB_CCNIM_AMP *amp)
/* 
   This routine runs as an AST on VMS systems.  This means it runs as
   a separate "thread".  Under EPICS this routine presently runs inline
   and thus causes a significant delay when waiting for the stepper motor.  
   In the future we could simply make this routine run as a separate vxWorks 
   task.
*/

{
  LONG s;
  UBYTE temp[4];
  ICB_MODULE_INFO *entry;
  double dt_qsec= .25;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

/*
* Check to see if busy flag is clear.  If not, wait till it is.
*/
while (1) {
	s = icb_input (amp->icb_index, 2, 1, temp);
	if (s != OK) return s;

	if ((temp[0] & AMP_M_R2_BUSY) == 0) break;
	epicsThreadSleep(dt_qsec);
}
	entry->registers[0] = temp[0];

/*
* Load fine gain register and motor position registers with 0
* These are registers 2-4.  Leave drive enable bit set!
*/
	temp[0] = AMP_M_R2_DRIVE_ENABLE | AMP_M_R2_LOAD_FG11_8;
	temp[1] = 0;
	temp[2] = 0;
	s = icb_output (amp->icb_index, 2, 3, temp);
	if (s != OK) return s;

/*
* Complete the motor position request.
*/
	s = icb_amp_complete_position (amp);
	return s;
}


/*******************************************************************************
*
* ICB_AMP_COMPUTE_MOTOR_POS computes the motor position based on the
* CAM HWGAIN2 parameter value.
*
* Its calling format is:
*
*	pos = ICB_AMP_COMPUTE_MOTOR_POS (amp)
* 
* where
*
*  "pos" is the motor position value (0 - FA0 hex).
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

ULONG icb_amp_compute_motor_pos (ICB_CCNIM_AMP *amp)

{

  LONG pos;

/*
* Make sure the fine gain value is reasonable before computing the
* position.
*/
	if (amp->hwgain2 < 1.0) amp->hwgain2 = 1.0;
	if (amp->hwgain2 > 3.0) amp->hwgain2 = 3.0;
	pos = (amp->hwgain2 - 1.0) * 2000.0;

	return pos;
}


/*******************************************************************************
*
* ICB_AMP_GET_NVRAM_MOTOR_POS returns the last recorded motor position
* from the 2nd page of the non-volatile memory.
*
* Its calling format is:
*
*	status = ICB_AMP_GET_NVRAM_MOTOR_POS (index, nvpos, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword, by reference) is the index into the icb module database.
*
*  "nvpos" (longword, by reference) Position read in from the NVRAM.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

int icb_amp_get_nvram_motor_pos (index, nvpos, flags)

LONG index;
LONG *nvpos;
LONG flags;

{
  LONG s;
  LONG i;
  ULONG nibble;
  UCHAR reg_val;

/*
* Map the second page of NVRAM.
*/
  	reg_val = AMP_M_R8_NVRAM_PAGE;
	s = icb_output (index, 8, 1, &reg_val);
	if (s != OK) return s;

/*
* Read the 1st three nibbles.  They are retrieved starting from the LSN to
* the MSN.
*/
	*nvpos  = 0;
	nibble = 0;
	for (i = 2; i >= 0; i--) {
	    s = icb_read_nvram (index, i, (LONG *) &nibble);
	    if (s != OK) return s;
	    *nvpos = (*nvpos * 16)  | nibble;
	}

/*
* Unmap the 2nd page of NVRAM.
*/
	reg_val = 0;
	s = icb_output (index, 8, 1, &reg_val);
	if (s != OK) return s;

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_PUT_NVRAM_MOTOR_POS writes the specified motor position to the 2nd
* page of the non-volatile memory.
*
* Its calling format is:
*
*	status = ICB_AMP_PUT_NVRAM_MOTOR_POS (index, nvpos, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword, by reference) is the index into the icb module database.
*
*  "nvpos" (longword, by reference) Position sent to the NVRAM.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

int icb_amp_put_nvram_motor_pos (index, nvpos, flags)

LONG index;
LONG nvpos;
LONG flags;

{
  LONG s;
  LONG i;
  UCHAR reg_val;
  CHAR nibble;
  ULONG temp;
  struct icb_module_info_struct *entry;  

/*
* Map the second page of NVRAM.
*/
  	reg_val = AMP_M_R8_NVRAM_PAGE;
	s = icb_output (index, 8, 1, &reg_val);
	if (s != OK) return s;

/*
* Write the 1st three nibbles.  The least significant nibble is stored first,
* then the middle, then the most significant nibble of the fine gain motor
* position.
*/
	temp = nvpos;
	for (i = 0; i <= 2; i++) {
	    nibble = temp & 0x0F;
	    s = icb_write_nvram (index, i, nibble);
	    if (s != OK) return s;
	    temp = temp / 16;
	}

/*
* Store the NVRAM
*/
	entry = &icb_module_info[index];
	reg_val  = ICB_M_CTRL_STORE_NVRAM | entry->r0_write;
	s = icb_output (index, 0, 1, &reg_val);
	if (s != OK) return s;

/*
* Unmap the 2nd page of NVRAM.
*/
	reg_val = 0;
	s = icb_output (index, 8, 1, &reg_val);
	if (s != OK) return s;

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_PUT_MOTOR_POS writes the specified motor position to the module.
* The motor itself is not moved.
*
* Its calling format is:
*
*	status = ICB_AMP_PUT_MOTOR_POS (index, pos, flags)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "index" (longword, by reference) is the index into the icb module database.
*
*  "pos" (longword, by reference) Position sent to the module.
*
*  "flags" (longword, by reference) is the reserved flags field
*
*******************************************************************************/

int icb_amp_put_motor_pos (index, pos, flags)

LONG index;
LONG pos;
LONG flags;

{
  LONG s;
  UBYTE temp[4];
  ICB_MODULE_INFO *entry;
  double wait_time = 0.1;	/* for taskDelay */

/*
* Get a pointer to the icb module database entry.
*/
	entry = &icb_module_info[index];

/*
* Load fine gain register and motor position registers with 0
* These are registers 2-4.
*/
	temp[0] = AMP_M_R2_LOAD_FG11_8;
	temp[1] = 0;
	temp[2] = 0;
	s = icb_output (index, 2, 3, temp);
	if (s != OK) return s;

/*
* Load fine gain register with the specified position
*/
	temp[0]  = (pos >> 8) & 0x0F;		/* Bits 11 - 8 */
	temp[0] |= AMP_M_R2_LOAD_FG11_8;	/* Load flag   */
	temp[1]  = pos & 0x00FF;		/* Bits  7 - 0 */
	s = icb_output (index, 2, 2, temp);
	if (s != OK) return s;

/*
* Set the Start bit
*/
	temp[0] = AMP_M_R2_START;
	s = icb_output (index, 2, 1, temp);
	if (s != OK) return s;

/*
* Wait for busy to go false
*/
	s = icb_input (index, 2, 1, temp);
	if (s != OK) return s;
	while ((temp[0] & AMP_M_R2_BUSY) != 0) {
	    epicsThreadSleep(wait_time);
	    s = icb_input (index, 2, 1, temp);
	    if (s != OK) return s;
	}
	entry->registers[0] = temp[0];

/*
* Set drive enable bit
*/
	temp[0] = AMP_M_R2_DRIVE_ENABLE;
	s = icb_output (index, 2, 1, temp);
	if (s != OK) return s;

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_START_PZ writes the initial pole zero value and begins the
* amplifier auto pole zero.
*
* Its calling format is:
*
*	status = ICB_AMP_START_PZ (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_start_pz (ICB_CCNIM_AMP *amp)

{
  LONG s=OK;
  UCHAR reg_val;
  ICB_MODULE_INFO *entry;
  double dt_qsec = 0.25;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->flags.bit.online == 0) goto abort;
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

/*
* Set our busy flag and see if the the module itself is busy?  If the
* module is busy, reschedule to try again in 1/4 second.
*/
	amp->flags.bit.pzbusy = 1;
	amp->flags.bit.pzfail = 0;
while(1) {
	if ((entry->registers[6] & AMP_M_R8_BUSY) == 0) break;
	epicsThreadSleep(dt_qsec);
}

/*
* Send initial pole zero value to the ICB module
*/
	s = icb_amp_write_pz (amp);
	if (s != OK) goto abort;

/*
* Start the auto pole zero.
*/
	reg_val = AMP_M_R8_START;
	s = icb_output (amp->icb_index, 8, 1, &reg_val);
	if (s != OK) goto abort;

/*
* Wait for busy to go false
*/
	s = icb_amp_test_pz_complete (amp);
	return s;

/*
* If I/O to module failed, clear busy flag and indicate failure
*/

abort:
	amp->flags.bit.pzbusy = 0;
	amp->flags.bit.pzfail = 1;
	return s;
}


/*******************************************************************************
*
* ICB_AMP_TEST_PZ_COMPLETE reads the module's busy flag.  If clear, it 
* checks the error flag and records (in no error) the calculated pole zero.
*
* Its calling format is:
*
*	status = ICB_AMP_TEST_PZ_COMPLETE (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_test_pz_complete (ICB_CCNIM_AMP *amp)

{
  LONG s;
  UBYTE temp[4];
  ICB_MODULE_INFO *entry;
  double dt_qsec = 0.25;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

/*
* Check to see if busy flag is clear.  If not, wait till it is.
*/
while (1) {
	s = icb_input (amp->icb_index, 8, 2, temp);
	if (s != OK) {
	    amp->flags.bit.pzbusy = 0;
	    amp->flags.bit.pzfail = 1;
	    return s;
	}
	if ((temp[0] & AMP_M_R8_BUSY) == 0) break;
	epicsThreadSleep(dt_qsec);
}
	entry->registers[6] = temp[0];

/*
* Clear our local busy flag.
*/
	amp->flags.bit.pzbusy = 0;

/*
* Get the error flag.
*/
	amp->flags.bit.pzfail = (temp[0] & AMP_M_R8_ERROR) != 0;

/*
* Load the new pole zero value (as read from the module).
*/
	amp->pz = ((temp[0] & 0x0F) << 8) + temp[1];

	return OK;
}


/*******************************************************************************
*
* ICB_AMP_WRITE_PZ writes pole zero value to the amplifier module.
*
* Its calling format is:
*
*	status = ICB_AMP_WRITE_PZ (amp)
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "amp" (ICB_CCNIM_AMP *) is the pointer to the structure that caches info
*  about the amplifier.
*
*******************************************************************************/

LONG icb_amp_write_pz (ICB_CCNIM_AMP *amp)

{
  LONG s;
  ICB_MODULE_INFO *entry;

/*
* Get a pointer to the icb module database entry.
*/
	if (amp->flags.bit.online == 0) return OK;
	if (amp->icb_index < 0) return OK;
	entry = &icb_module_info[amp->icb_index];

/*
* Write the pole zero value to the module
*/
	amp->pz &= 0x0FFF;
	if (amp->pz == 0) amp->pz = 0x0B00;
	entry->registers[6] &= 0x40;	/* Preserve TRP bit	*/
	entry->registers[6] |= (amp->pz >> 8) | AMP_M_R8_LOAD_PZ11_8;
	entry->registers[7]  = amp->pz;
	s = icb_output (amp->icb_index, 8, 2, &entry->registers[6]);

	return s;
}


/*******************************************************************************
*
* ICB_ADC_ENCODE_CHNS convert a channels value into the form expected by the
* ADC module.  The returned value of the function is determined in the
* following way.
*
*	Returned Value		Channels
*	--------------		--------
*	       0		  32768
*	       1		  16384
*	       2		   8192
*	       3		   4096
*	       4		   2048
*	       5		   1024
*	       6		    512
*	       7		    256
*
*
* Its calling format is:
*
*	code = ICB_ADC_ENCODE_CHNS (channels)
* 
* where
*
*  "code" (longword) is the encoded channels value
*
*  "channels" (longword, by value) is the conv. gain or range (in channels).
*
*******************************************************************************/

int icb_adc_encode_chns (chns)

ULONG chns;

{
  LONG i;
  ULONG mask;

/*
* Find the first set bit.
*/
	mask = 32768;
	for (i = 0; i <= 7; i++, mask >>= 1)
	    if ((mask & chns) == mask) return i;

/*
* Otherwise return 0
*/
	return 0;
}


/*******************************************************************************
*
* ICB_WRITE_CSR writes the specified bits to the CSR of the specified
* computer controlled NIM module.  The caller may specify permanent
* and/or temporary bits to set as well as mask for existing bits to be
* preserved.
*
* Calling format:
*
*	status = ICB_WRITE_CSR (ccnim, perm_bits, temp_bits, mask);
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*  "ccnim" (ICB_CCNIM, by reference) is a pointer to the computer
*   controlled NIM module to be written too.
*
*  "perm_bits" (longword, by reference) are the permanent bits to be set
*
*  "temp_bits" (longword, by reference) are the temporary bits to be set
*
*  "mask" (longword, by reference) is the mask of bits to be preserved.
*
*******************************************************************************/

int icb_write_csr (ICB_CCNIM_ANY *ccnim,
	       LONG perm_bits,
	       LONG temp_bits,
	       LONG mask)

{
  LONG s;
  UCHAR csr;
  UCHAR register_list[16];
  ICB_MODULE_INFO *entry;

/*
* Make sure an index to the module database is specified.
*/
	if (ccnim->icb_index == -1) return OK;

/*
* Get a pointer to the entry, apply the permanent bits and mask off
* the unwanted bits.
*/
	entry = &icb_module_info[ccnim->icb_index];
	entry->r0_write |= perm_bits;
	entry->r0_write &= mask;

/*
* Set the temporary bits and send it to the CSR.
*/
	csr = entry->r0_write | temp_bits;
	s = icb_output (ccnim->icb_index, 0, 1, &csr);
	if (s != OK) return s;

/*
* If the clear reset or reset bit was set, read the module's 16 registers.
*/
	if ((csr & (ICB_M_CTRL_CLEAR_RESET | ICB_M_CTRL_RESET)) != 0) {
	    s = icb_input (ccnim->icb_index, 0, 16, register_list);
	    if (s != OK) return s;
	    entry->r0_read = register_list[0];
	    memcpy (entry->registers, &register_list[2], 14);
	}

	return s;
}



/*******************************************************************************
*
* ICB_MONITOR_MODULES monitors all valid, present, in use modules that have
* the monitor flag set.
*
* Its calling format is:
*
*	status = ICB_MONITOR_MODULES ();
* 
* where
*
*  "status" is the status of the operation. Any errors have been signaled.
*
*******************************************************************************/

int icb_monitor_modules ()

{
  LONG i;
  ICB_MODULE_INFO *entry;
  static REAL tc;
  static LONG tc_verify;
  static LONG inhibit;
  static LONG overload;
  static ICB_PARAM_LIST amp_plist[]  = {{CAM_F_AMPTC,    0, &tc},
					{CAM_L_AMPVFTC,  0, &tc_verify},
					{0,              0, 0}};
  static ICB_PARAM_LIST hvps_plist[] = {{CAM_L_HVPSFINH, 0, &inhibit},
					{CAM_L_HVPSFOV,  0, &overload},
					{0,              0, 0}};

/*
* Loop through the list of icb modules.  Monitor the ones that are
* valid, present, and have the monitor state flag set.
*/
	entry = &icb_module_info[0];
	for (i = 0; i < ICB_K_MAX_MODULES; i++, entry++) {
	    if (!entry->valid || !entry->present) continue;
	    if (!entry->monitor_state) continue;

	/* Handle HVPS modules */

	    if ((entry->module_type == ICB_K_MTYPE_CI9641) ||
		(entry->module_type == ICB_K_MTYPE_CI9645)) {
		inhibit = overload = FALSE;
		icb_hvps_hdlr (i, hvps_plist, ICB_M_HDLR_READ);
		if (inhibit || overload) entry->state_change = TRUE;
	    }

	/* Handle AMP module.  */

	    else if (entry->module_type == ICB_K_MTYPE_CI9615) {
		tc_verify = FALSE;
		icb_amp_hdlr (i, amp_plist,  ICB_M_HDLR_READ);
		if (tc_verify) {
		    entry->state_change = TRUE;
		}
	    }

	/* No need to monitor the ADC module */


	/* We done did it... clear the monitor flag */

	    entry->monitor_state = FALSE;
	}

	return OK;
}


/* End of ICB_HANDLER_SUBS.C */
