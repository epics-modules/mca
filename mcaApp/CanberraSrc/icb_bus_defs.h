/*******************************************************************************
*
* This module contains definitions relating to the Instrument Control Bus (ICB).
* These definitions should be system-independent; i.e., relate directly to the
* bus. Any system-dependent definitions should go in ICB_SYS_DEFS.H.
*
********************************************************************************
*
* Revision History:
*
*	23-Sep-1988	RJH	Original
*	12_Feb-1992	TLG	Added new CCNIM modules.
*
*******************************************************************************/

/*
* Define ICB module types.  Fetched from the ICB module's register 0.
*/

#define ICB_K_MTYPE_MASK	3	/* Mask for Register 0 type specifier*/
#define ICB_K_MTYPE_RPI		1	/* Remote Parallel Interface (ND554) */
#define ICB_K_MTYPE_ICB2	2	/* 2nd Generation ICB		     */

/*
* 2nd generation ICB types.  Fetched from ICB module's non-volatile RAM
* locations 0 (LSN) and 1 (MSN).
*/

#define ICB_K_MTYPE_CI9641	3	/* CI's 2000 volt HVPS		     */
#define ICB_K_MTYPE_CI9645	4	/* CI's 6000 volt HVPS		     */
#define ICB_K_MTYPE_CI9633	5	/* CI's 16K channel ADC		     */
#define ICB_K_MTYPE_CI9635	6	/* CI's 8K channel ADC		     */
#define ICB_K_MTYPE_CI9615	7	/* CI's Standard CC Amplifier	     */
#define ICB_K_MAX_MTYPE 	7	/* Highest module type number	     */

/*
* Define ASCII equivalents for ICB module types. The names must be space padded
* if necessary to be 16 characters long. The last entry must be for unknown
* module types.
*/

#ifdef ICB_DEFINE_NAMES
static char icb_module_names[ICB_K_MAX_MTYPE+1][17] = {"ND554 RPI       ",
						       "Unknown         ",
						       "CI9641 2K HVPS  ",
						       "CI9645 6K HVPS  ",
						       "CI9633 16K ADC  ",
						       "CI9635 8K ADC   ",
						       "CI9615 AMP      ",
						       "Unknown         "};
#endif

/*
* Define 1st generation ICB module status bits (register 1)
* The RPI is the only 1st generation ICB module.
*/

#define ICB_M_STAT_REQUEST	0x01	/* Service Request asserted */
#define ICB_M_STAT_INIT		0x02	/* module has been initialized */
#define ICB_M_STAT_ERROR	0x04	/* module has failed */

/*
* Define 2nd generation ICB module status bits (register 0)
* Read only.
*/

#define ICB_M_STAT_SRVREQ	0x04	/* Service request bit		*/
#define ICB_M_STAT_RESET	0x08	/* Module Reset bit		*/
#define ICB_M_STAT_FAIL		0x10	/* Module Failed bit		*/

/*
* Define 2nd generation ICB module control bits (register 0)
* Write only.
*/

#define ICB_M_CTRL_LED_RED	0x01	/* Turns on/off the red LED	      */
					/*  Bit value: 0=OFF, 1=ON	      */
#define ICB_M_CTRL_STORE_NVRAM	0x02	/* Stores data (in Reg 1) into NVRAM  */
#define ICB_M_CTRL_CLEAR_RESET	0x04	/* Clears module reset status bit     */
#define ICB_M_CTRL_RESET	0x08	/* Resets the module		      */
#define ICB_M_CTRL_LED_GREEN	0x10	/* Turns on/off the green LED	      */
					/*  Bit value: 0=OFF, 1=ON	      */
#define ICB_M_CTRL_LED_YELLOW	0x11	/* Turns on/off both the red and green*/
					/*  LEDs to make yellow		      */


/*
* Define the HVPS specific register bits.
*/

#define HVPS_M_R2_INHRANGE_12V		0x10
#define HVPS_M_R2_INHLATCH_ENA		0x08
#define HVPS_M_R2_OVLATCH_ENA		0x04
#define HVPS_M_R2_RESET			0x02
#define HVPS_M_R2_STATUS_ON		0x01

#define HVPS_M_R3_OVERLOAD		0x04
#define HVPS_M_R3_INHIBIT		0x02
#define HVPS_M_R3_POLARITY_NEG		0x01


/*
* Define the ADC specific register bits.
*/

#define ADC_M_R2_ACQMODE_SVA		0x80
#define ADC_M_R2_ANTICOINC_ENA		0x40
#define ADC_M_R2_EARLYCOINC_ENA		0x20
#define ADC_M_R2_DELAYPEAK_ENA		0x10
#define ADC_M_R2_NONOVERLAP_ENA		0x08
#define ADC_M_R2_LTCPUR_EOC		0x04


/*
* Define the AMP specific register bits.
*/

#define AMP_M_R2_BUSY			0x80
#define AMP_M_R2_LOW_INDEX		0x40
#define AMP_M_R2_HIGH_INDEX		0x20
#define AMP_M_R2_EQUAL			0x10

#define AMP_M_R2_START			0x80
#define AMP_M_R2_INDEX			0x40
#define AMP_M_R2_DRIVE_ENABLE		0x20
#define AMP_M_R2_LOAD_FG11_8		0x10

#define AMP_M_R6_INPPOLARITY_NEG	0x80
#define AMP_M_R6_INPUT_DIFF		0x40
#define AMP_M_R6_BLRMODE_ASYM		0x20
#define AMP_M_R6_PILEUPREJ_ENA		0x10
#define AMP_M_R6_INHPOLARITY_NEG	0x08
#define AMP_M_R6_LTCMODE_LFC		0x04
#define AMP_M_R6_PREAMP_TRP		0x02
#define AMP_M_R6_SHAPEMODE_TRIANGLE	0x01

#define AMP_M_R8_BUSY			0x80
#define AMP_M_R8_NVRAM_PAGE		0x40
#define AMP_M_R8_ERROR			0x10

#define AMP_M_R8_START			0x80
#define AMP_M_R8_LOAD_PZ11_8		0x10
