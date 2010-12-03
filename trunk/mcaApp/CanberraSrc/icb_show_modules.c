/*******************************************************************************
* This routine polls all of the known AIM modules for arrached ICB modules
* and prints out information about each module it finds.
*******************************************************************************/

/* Modifications
 * 23-Mar-1999  MLR  Minor changes to avoid compiler errors
 * 06-Dec-1999  MLR  Minor changes to avoid compiler warnings
 * 25-Jul-2000  MLR  Changed call to icb_initialize(), removed scanning for
 *                   ICB modules which is now done in icb_initialize()
 */

#define ICB_DEFINE_NAMES 1
#include <stdio.h>
#include "ndtypes.h"
#include "icb_sys_defs.h"
#include "icb_bus_defs.h"
#include "nmc_sys_defs.h"
#include "mcamsgdef.h"
#include "nmcmsgdef.h"
#include "campardef.h"

extern struct icb_module_info_struct *icb_module_info;	/* pointer to ICB module info structure */

extern struct nmc_module_info_struct *nmc_module_info;	/* pointer to networked module info */

int icb_show_modules()
{
	struct icb_module_info_struct *entry;
	int i;
	int s;

	static char pramptype[10];
	static float amphwgain1;
	static float amphwgain2;
	static float amphwgain3;
	static char ampshapemode[10];
	static long amppz;
	static char ampblrtype[10];
	static char ampdtctype[10];
	static float amptc;
	static long ampflags;

	ICB_PARAM_LIST amp_param_list[] = {
	{CAM_T_PRAMPTYPE, 'S', &pramptype},	/* Preamplifier type (RC/TRP) */
	{CAM_F_AMPHWGAIN1, 0,  &amphwgain1},	/* Course GAIN		      */
	{CAM_F_AMPHWGAIN2, 0,  &amphwgain2},	/* Fine Gain		      */
	{CAM_F_AMPHWGAIN3, 0,  &amphwgain3},	/* Super Fine Gain	      */
	{CAM_T_AMPSHAPEMODE, 'S', &ampshapemode},	/* Amplifier shaping mode     */
	{CAM_L_AMPPZ,		0,  &amppz},	/* Amplifier pole zero	      */
	{CAM_T_AMPBLRTYPE, 'S', &ampblrtype},	/* Base-line restore (SYM,ASYM) */
	{CAM_T_AMPDTCTYPE, 'S', &ampdtctype},	/* Dead-time control (Norm, LFC) */
	{CAM_F_AMPTC,		0,	&amptc},	/* Amplifier time constant    */
	{CAM_L_AMPFLAGS,	0,	&ampflags},	/* Amplifier mode flags	      */
	{0,			0,	0}};	/* End of list		      */

	static long adcrange;
	static long adcoffset;
	static char adcacqmode[10];
	static long cnvgain;
	static float lld;
	static float uld;
	static float zero;
	static long adcflags;
	ICB_PARAM_LIST adc_param_list[] = {
	{CAM_L_ADCRANGE,	0,	&adcrange},	/* Range		      */
	{CAM_L_ADCOFFSET,	0,	&adcoffset},	/* Offset		      */
	{CAM_T_ADCACQMODE, 'S',	&adcacqmode},	/* Acq Mode		      */
	{CAM_L_CNVGAIN,	0,	&cnvgain},	/* Conversion Gain	      */
	{CAM_F_LLD,		0,	&lld},	 /* Lower level discriminator  */
	{CAM_F_ULD,		0,	&uld},	 /* Upper level discriminator  */
	{CAM_F_ZERO,	0,	&zero},	 /* Zero			      */
	{CAM_L_ADCFLAGS,0,	&adcflags}, /* ADC mode flags	      */
	{0,			0,	0}};	/* End of list		      */

	/* Initialize ICB data structures */
	icb_initialize();
	
	/* Step through all ICB modules, print out information about each one. */
	printf("\n");
	for (i=0; i < ICB_K_MAX_MODULES; i++)
	{
		entry = &icb_module_info[i];
		if (!(*entry).valid) break;
		printf("%s   %d  %s   %d\n", 
					(*entry).address, 
					(*entry).module_type,
					icb_module_names[(*entry).module_type-1],
					(*entry).module_sn);
		switch ((*entry).module_type) {
		case ICB_K_MTYPE_CI9615:  /* CI's Standard CC Amplifier	*/
			/* Read amplifier parameters */
			s = icb_amp_hdlr(i, amp_param_list, ICB_M_HDLR_READ);
			if (s != OK) {printf("Error reading amplifier\n"); return 1;}
			printf("   Preamp type:       %s\n", pramptype);
			printf("   Coarse gain:       %f\n", amphwgain1);
			printf("   Fine gain:         %f\n", amphwgain2);
			printf("   Super fine gain:   %f\n", amphwgain3);
			printf("   Shaping mode:      %s\n", ampshapemode);
			printf("   Pole zero:         %ld\n", amppz);
			printf("   Baseline restore:  %s\n", ampblrtype);
			printf("   Dead-time control: %s\n", ampdtctype);
			printf("   Time constant:     %f\n", amptc);
			printf("   Flags:             %lx\n", ampflags);
			break;
		case ICB_K_MTYPE_CI9633:  /* CI's 16K channel ADC */
		case ICB_K_MTYPE_CI9635:  /* CI's 8K channel ADC */
			/* Read ADC parameters */
			s = icb_adc_hdlr(i, adc_param_list, ICB_M_HDLR_READ);
			if (s != OK) {printf("Error reading ADC\n"); return 1;}
			printf("   Range:             %ld\n", adcrange);
			printf("   Offset:            %ld\n", adcoffset);
			printf("   Acquisition mode:  %s\n", adcacqmode);
			printf("   Conversion gain:   %ld\n", cnvgain);
			printf("   Lower level disc:  %f\n", lld);
			printf("   Upper level disc:  %f\n", uld);
			printf("   Zero:              %f\n", zero);
			printf("   Flags:             %lx\n", adcflags);
			break;
		case ICB_K_MTYPE_CI9641:  /* CI's 2000 volt HVPS */
		case ICB_K_MTYPE_CI9645:  /* CI's 6000 volt HVPS */
			/* Read HVPS parameters */
			s = icb_hvps_hdlr(i, adc_param_list, ICB_M_HDLR_READ);
			if (s != OK) {printf("Error reading HVPS\n"); return 1;}
			break;
		}
	}
	return OK;
	
}

