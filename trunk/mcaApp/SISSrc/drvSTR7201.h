/* drvSTR7201.h - Driver Support Routines for Struck STR7201 multiscaler */
/*
 *      Author:  Mark Rivers
 *      Date:    11/9/97
 *
 * Modification Log:
 * -----------------
 * .01  11/09/97  mlr  Developed from AIM device support
 * .02  2/06/99   mlr  Implemented pre-scaler in new SIS3801.
 * .03  23/03/00  mlr  Added support for simple scaler mode
 * .04  2/04/00   mlr  Added "signal" parameter to drvSTR7201SetPresets()
 * .05  10/03/00  mlr  Added ch1RefEnable parameter to drvSTR7201Config()
 * .06  10/24/00  mlr  Added drvSTR7201GetConfig()
 */

#define STR7201_MAX_SIGNALS 32
#define MULTICHANNEL_SCALER_MODE 0
#define SIMPLE_SCALER_MODE       1
#define LNE_INTERNAL 0
#define LNE_EXTERNAL 1
#define INTERNAL_CLOCK 10000000  /* The internal clock on the SIS38xx */
#define NINT(f) (long)((f)>0 ? (f)+0.5 : (f)-0.5) 

/* Function prototypes */
int STR7201Setup(int maxCards, int baseAddress, int intVector, int intLevel);
int STR7201Config(int card, int maxSignals, int maxChans, int ch1RefEnable, int softAdvance);
int drvSTR7201GetConfig(int card, int *maxSignals, int *maxChans, int *ch1RefEnable, int *softAdvance);
int drvSTR7201Init(int card);
int drvSTR7201AcqOn(int card);
int drvSTR7201AcqOff(int card);
int drvSTR7201SetPresets(int card, int signal, float ptime, 
                            int startChan, int endChan, long pcounts, int mode);
int drvSTR7201SetChannelAdvance(int card, int source, int prescale);
int drvSTR7201GetAcqStatus(int card, int signal, float *elapsed_time, 
                            long *elapsed_counts, int *acq_status);
int drvSTR7201Erase(int card);
int drvSTR7201Read(int card, int signal, int maxChans, int *numChans, long *buff);
int drvSTR7201SetNChans(int card, int nchans);
