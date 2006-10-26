/* File:    devScalerSIS3820.h
 * Author:  Wayne Lewis
 * Date:    15-feb-2006
 *
 * Description:
 * This is the driver support header for the SIS3820 multiscaler.
 */

/*
 * Modification Log:
 * -----------------
 * .01  16-feb-06 wkl   Initial migration to SIS3820
 */

/**************/
/* Structures */
/**************/

#include <scalerRecord.h>

struct scaler_state {
	int num_channels;
	int prev_counting;
	int done;
	CALLBACK *pcallback;
	int card_exists;
	int card_in_use;
	int count_in_progress; /* count in progress? */
	unsigned short ident; /* identification info for this card */
	volatile char *localAddr; /* address of this card */
	/*
	   IOSCANPVT ioscanpvt;
	 */
	int preset[SIS3820_MAX_SIGNALS];
        scalerRecord *psr;
};

