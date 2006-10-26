/* File:    drvSIS3820.h
 * Author:  Wayne Lewis
 * Date:    15-feb-2006
 *
 * Description:
 * This is the driver support header for the SIS3820 multiscaler.
 * It is based heavily on the driver support for the STR7201 written by
 * Mark Rivers.
 */

/*
 * Modification Log:
 * -----------------
 * .01  16-feb-06 wkl   Initial migration to SIS3820
 * .02  17-mar-06 wkl   Added bit shifts for input and output modes
 */

/***************/
/* Definitions */
/***************/

#define SIS3820_MAX_SIGNALS 32
#define SIS3820_MAX_CARDS 4

#define MULTICHANNEL_SCALER_MODE 0
#define SIMPLE_SCALER_MODE       1
#define SIS3820_LNE_INTERNAL 0
#define SIS3820_LNE_EXTERNAL 1
#define SIS3820_LNE_10MHZ 2
#define SIS3820_LNE_CHANNEL 32
#define SIS3820_INTERNAL_CLOCK 50000000  /* The internal clock on the SIS38xx */

#define SIS3820_ADDRESS_TYPE atVMEA32
#define SIS3820_BOARD_SIZE 0x4000000

/* Additional SIS3820 flags */
#define SIS3820_IRQ_ENABLE 0x800
#define SIS3820_IRQ_ROAK 0x1000

/* Bit masks */
#define SIS3820_IRQ_LEVEL_MASK 0x00000700
#define SIS3820_IRQ_VECTOR_MASK 0x000000ff
#define SIS3820_FOUR_BIT_MASK 0x0000000f

/* Bit shifts */
#define SIS3820_INPUT_MODE_SHIFT 16
#define SIS3820_OUTPUT_MODE_SHIFT 20

#ifndef ERROR
#define ERROR -1
#endif
#ifndef OK
#define OK 0
#endif

#define NINT(f) (long)((f)>0 ? (f)+0.5 : (f)-0.5) 

/**************/
/* Structures */
/**************/

/***********************/
/* Function prototypes */
/***********************/

/* External functions */
/* iocsh functions */
int SIS3820Setup(int maxCards, int baseAddress, int intVector, int intLevel);
int SIS3820Config(int card, 
                  int maxSignals, 
                  int maxChans, 
                  int ch1RefEnable, 
                  int softAdvance, 
                  int inputMode, 
                  int outputMode, 
                  int lnePrescale);

/* device support functions */
int drvSIS3820GetConfig(int card, 
                        int *maxSignals, 
                        int *maxChans, 
                        int *ch1RefEnable, 
                        int *softAdvance);
int drvSIS3820Init(int card);
int drvSIS3820AcqOn(int card);
int drvSIS3820AcqOff(int card);
int drvSIS3820ClearPresets(int card);
int drvSIS3820SetPresets(int card, int signal, long pcounts);
int drvSIS3820EnablePresetBank(int card, int bank);
int drvSIS3820DisablePresetBank(int card, int bank);
int drvSIS3820SetChannelAdvance(int card, int source, int prescale);
int drvSIS3820GetAcqStatus(int card, int signal, int *acq_status);
int drvSIS3820Erase(int card);
int drvSIS3820Read(int card, long *buff);
int drvSIS3820SetNChans(int card, int nchans);

