/* File:    drvSIS3820.c
 * Author:  Wayne Lewis
 * Date:    15-feb-2006
 *
 * Description:
 * This is the driver support routine for the SIS3820 multiscaler.
 * It is based heavily on the driver support for the STR7201 written by
 * Mark Rivers.
 */

/*
 * Modification Log:
 * -----------------
 * .01  16-feb-06 wkl   Initial migration to SIS3820
 * .02  17-mar-06 wkl   Add options to set input and output modes
 * .03  17-mar-06 wkl   Fix up preset counting support.
 */


/************/
/* Includes */
/************/

/* System includes */
#include	<stdio.h>
#include	<stdlib.h>

/*#include	<sys/types.h>*/	/*we can use epicsTypes.h instead for more OS independence */
#ifdef vxWorks
#include	<fppLib.h>
#endif

/* EPICS includes */
#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<recSup.h>
#include  <epicsExport.h>
#include	<devSup.h>
#include	<devLib.h>
#include	<link.h>
#include	<epicsTypes.h>

/* Custom includes */
#include	"sis3820.h"
#include	"drvSIS3820.h"
#include	"devScaler.h"
#include	"devScalerSIS3820.h"

/***************/
/* Definitions */
/***************/

/* Define macros to interlock access to card, including interrupts */
# define INTERLOCK_ON \
         {epicsMutexLock(p->lock); \
          p->address->irq_control_status_reg = SIS3820_IRQ_SOURCE2_DISABLE;}

# define INTERLOCK_OFF \
         {p->address->irq_control_status_reg = SIS3820_IRQ_SOURCE2_ENABLE; \
          epicsMutexUnlock(p->lock);}

/* Debug support */
#define Debug(l,FMT,V...) {if (l <= drvSIS3820Debug) \
			  { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,## V);}}

volatile int drvSIS3820Debug = 0;
epicsExportAddress(int, drvSIS3820Debug);

/**************/
/* Structures */
/**************/

/* This structure duplicates the control and status register structure of the
 * SIS3820. Note that it does not extend to the FIFO/SDRAM area, as this would
 * chew a lot of memory. Access to the FIFO/SDRAM needs to be via an explicit
 * offset from the base address. FIFO address space is from 0x800000 to
 * 0xfffffc. 
 */
typedef volatile struct {
  epicsUInt32 control_status_reg;         /* Offset = 0x00 */
  epicsUInt32 moduleID_reg;               /* Offset = 0x04 */
  epicsUInt32 irq_config_reg;             /* Offset = 0x08 */
  epicsUInt32 irq_control_status_reg;     /* Offset = 0x0c */

  epicsUInt32 acq_preset_reg;             /* Offset = 0x10 */
  epicsUInt32 acq_count_reg;              /* Offset = 0x14 */
  epicsUInt32 lne_prescale_factor_reg;    /* Offset = 0x18 */
  epicsUInt32 unused1c;
  
  epicsUInt32 preset_group1_reg;          /* Offset = 0x20 */
  epicsUInt32 preset_group2_reg;          /* Offset = 0x24 */
  epicsUInt32 preset_enable_reg;          /* Offset = 0x28 */
  epicsUInt32 unused2c;

  epicsUInt32 cblt_setup_reg;             /* Offset = 0x30 */
  epicsUInt32 sdram_page_reg;             /* Offset = 0x34 */
  epicsUInt32 fifo_word_count_reg;        /* Offset = 0x38 */
  epicsUInt32 fifo_word_threshold_reg;    /* Offset = 0x3c */

  epicsUInt32 hiscal_start_preset_reg;    /* Offset = 0x40 */
  epicsUInt32 hiscal_start_counter_reg;   /* Offset = 0x44 */
  epicsUInt32 hiscal_last_acq_count_reg;  /* Offset = 0x48 */
  epicsUInt32 unused4c;

  epicsUInt32 unused50_fc[44];
  
  epicsUInt32 op_mode_reg;                /* Offset = 0x100 */
  epicsUInt32 copy_disable_reg;           /* Offset = 0x104 */
  epicsUInt32 lne_channel_select_reg;     /* Offset = 0x108 */
  epicsUInt32 preset_channel_select_reg;  /* Offset = 0x10c */

  epicsUInt32 unused110_1fc[60];
  
  epicsUInt32 count_disable_reg;          /* Offset = 0x200 */
  epicsUInt32 count_clear_reg;            /* Offset = 0x204 */
  epicsUInt32 counter_overflow_reg;       /* Offset = 0x208 */
  epicsUInt32 unused20c;

  epicsUInt32 ch1_17_high_bits_reg;       /* Offset = 0x210 */

  epicsUInt32 unused214_2fc[59];

  epicsUInt32 sdram_prom_reg;             /* Offset = 0x300 */
  epicsUInt32 unused304;
  epicsUInt32 unused308;
  epicsUInt32 unused30c;

  epicsUInt32 xilinx_test_data_reg;       /* Offset = 0x310 */
  epicsUInt32 xilinx_control;             /* Offset = 0x314 */

  epicsUInt32 unused318_3fc[58];

  epicsUInt32 key_reset_reg;              /* Offset = 0x400 */
  epicsUInt32 key_fifo_reset_reg;         /* Offset = 0x404 */
  epicsUInt32 key_test_pulse_reg;         /* Offset = 0x408 */
  epicsUInt32 key_counter_clear;          /* Offset = 0x40c */

  epicsUInt32 key_lne_clock_reg;          /* Offset = 0x410 */
  epicsUInt32 key_op_arm_reg;             /* Offset = 0x414 */
  epicsUInt32 key_op_enable_reg;          /* Offset = 0x418 */
  epicsUInt32 key_op_disable_reg;         /* Offset = 0x41c */

  epicsUInt32 unused420_4fc[56];
  
  epicsUInt32 unused500_7fc[192];

  epicsUInt32 shadow_regs[32];            /* Offset = 0x800 to 0x87c */
  epicsUInt32 unused880_8fc[32];

  epicsUInt32 unused900_9fc[64];

  epicsUInt32 counter_regs[32];           /* Offset = 0xa00 to 0xa7c */
  epicsUInt32 unuseda80_afc[32];

  epicsUInt32 unusedb00_ffc[320];         /* Unused space to 4KB per board */
} SIS3820_REGS;


static struct strCard {
  int exists;
  int moduleID;
  int firmwareVersion;
  SIS3820_REGS *address;
  int mode;
  int maxSignals;
  int maxChans;
  int nChans;
  int nextChan;
  int nextSignal;
  double presetTime;
  int startTime;
  double elapsedTime;
  double elapsedPrevious;
  int presetStartChan[SIS3820_MAX_SIGNALS];
  int presetEndChan[SIS3820_MAX_SIGNALS];
  long presetCounts[SIS3820_MAX_SIGNALS];
  long elapsedCounts[SIS3820_MAX_SIGNALS];
  int lneSource;
  int lnePrescale;
  int ch1RefEnable;
  int softAdvance;
  long *buffer;  /* maxSignals * maxChans */
  long *buffPtr;
  int acquiring;
#ifdef vxWorks
  FP_CONTEXT *pFpContext;
#endif
  epicsMutexId lock;
} *strCard=0;

/***********************/
/* Function prototypes */
/***********************/

static void interruptServiceRoutine(int card);
static int readFIFO(int card);
static int checkAcquireStatus(int card);
static int updateElapsed(int card);
static int acqOff(int card);

/* internal functions */
static epicsUInt32 setControlStatusReg();
static epicsUInt32 setOpModeReg(int inputMode, int outputMode);
static epicsUInt32 setIrqControlStatusReg();

/*******************************/
/* Global variable declaration */
/*******************************/

int SIS3820NumCards;
epicsUInt32 *SIS3820BaseAddress;
int SIS3820IntVector;
int SIS3820IntLevel;

/*************************/
/* External declarations */
/*************************/

extern struct scaler_state **scaler_state; /* See devScalerSIS3820.c */

/********/
/* Code */
/********/

/* iocsh commands */

/* This command sets up all the SIS3820s in the crate */
int SIS3820Setup(int numCards, int baseAddress, int intVector, int intLevel)
{
  SIS3820NumCards = numCards;
  SIS3820BaseAddress = (epicsUInt32 *) baseAddress;
  SIS3820IntVector = intVector;
  SIS3820IntLevel = intLevel;
  return (0);
}

/* This command needs to be called once for each SIS3820 in the crate */
int SIS3820Config(int card, 
                  int maxSignals, 
                  int maxChans, 
                  int ch1RefEnable, 
                  int softAdvance, 
                  int inputMode, 
                  int outputMode,
                  int lnePrescale)
{
  struct strCard *p;
  int i;
  int status;
  epicsUInt32 controlStatusReg;
  epicsUInt32 irqControlStatusReg;
  epicsUInt32 opModeReg;
  epicsUInt32 channelDisableRegister = 0xffffffff;


  /* Allocate and initialize strCard if not done yet */
  if (strCard == 0) 
  {
    Debug(1, "SIS3820Config: numCards=%d\n", SIS3820NumCards);

    strCard = (struct strCard*)
      calloc(1, sizeof(struct strCard)*SIS3820NumCards);

    for (i=0; i<SIS3820NumCards; i++) 
    {
      strCard[i].exists = 0;

      /* Call devLib to get the system address that corresponds to the VME base
       * address of the board.
       */
      status = devRegisterAddress("drvSIS3820", SIS3820_ADDRESS_TYPE,
          (size_t)(SIS3820BaseAddress + i), SIS3820_BOARD_SIZE,
          (volatile void **)&strCard[i].address);

      if (!RTN_SUCCESS(status)) 
      {
        errPrintf(status, __FILE__, __LINE__, 
            "Can't register address for SIS3820 card %d\n", i);
        return (ERROR);
      }
      
      Debug(1, "SIS3820Config: configuring card=%d\n", i);
      Debug(1, "SIS3820Config: local address=%p\n", strCard[i].address);

      /* Probe VME bus to see if card is there */
      if (devReadProbe(4, (char *) &strCard[i].address->control_status_reg, 
            (char *) &controlStatusReg) == OK) strCard[i].exists = 1;
    }
  }

  p=&strCard[card];
  if (p->exists == 0) 
    return (ERROR);
  /* WORK NEEDED - WILL THIS WORK WITH OLD STRUCK BOARDS? */
  /* Got to here...need to update these with new structure element names */

  p->moduleID = (p->address->moduleID_reg & 0xFFFF0000) >> 16;
  Debug(2, "SIS3820Config: module ID=%x\n", p->moduleID);
  p->firmwareVersion = (p->address->moduleID_reg & 0x0000FFFF);
  Debug(2, "SIS3820Config: firmwareVersion=%d\n", p->firmwareVersion);
  p->maxSignals = maxSignals;
  p->maxChans = maxChans;
  p->acquiring = 0;
  p->startTime = 0;
  p->nChans = maxChans;
  p->nextChan = 0;
  p->nextSignal = 0;
  p->lneSource = SIS3820_LNE_EXTERNAL;
  p->lnePrescale = lnePrescale;
  p->ch1RefEnable = ch1RefEnable;
  p->softAdvance = softAdvance;
  /*
  p->buffer = (long *)malloc(maxSignals*maxChans*sizeof(long));
  */

  /* Create semaphore to interlock access to card */
  p->lock = epicsMutexCreate();

  /* Reset card */
  Debug(2, "SIS3820Config: resetting card %d\n", card);
  p->address->key_reset_reg = 1;

  /* Clear FIFO */
  Debug(2, "SIS3820Config: clearing FIFO\n");
  p->address->key_fifo_reset_reg= 1;

  /* Initialize board and set the control status register */
  Debug(2, "SIS3820Config: initialising control status register\n");
  p->address->key_reset_reg = 1;

  controlStatusReg = setControlStatusReg();
  p->address->control_status_reg = controlStatusReg;
  
  Debug(1, "SIS3820Config: control status register = %d\n",
      p->address->control_status_reg);

  /* Set the interrupt control register */
  Debug(1, "SIS3820Config: setting interrupt control register\n");

  irqControlStatusReg = setIrqControlStatusReg();
  p->address->irq_control_status_reg = irqControlStatusReg;

  Debug(1, "SIS3820Config: interrupt control register = %d\n",
      p->address->irq_control_status_reg);

  /* Set the operation mode of the scaler */
  Debug(1, "SIS3820Config: setting operation mode\n");

  opModeReg = setOpModeReg(inputMode, outputMode);
  p->address->op_mode_reg = opModeReg;

  Debug(1, "SIS3820Config: operation mode register = %d\n",
      p->address->op_mode_reg);

  /* Set the LNE channel */
  Debug(1, "SIS3820Config: setting LNE signal generation to channel %d\n",
      SIS3820_LNE_CHANNEL);

  p->address->lne_channel_select_reg = SIS3820_LNE_CHANNEL;

  Debug(1, "SIS3820Config: set LNE signal generation to channel %d\n",
      p->address->lne_channel_select_reg);


  /* 
   * Set number of readout channels to maxSignals
   * Create a mask with zeros in the rightmost maxSignals bits, 
   * 1 in all higher order bits.
   */
  Debug(2, "SIS3820Config: setting readout channels=%d\n",maxSignals);

  for (i = 0; i < maxSignals; i++)
  {
    channelDisableRegister <<= 1;
  }

  Debug(2, "SIS3820Config: setting readout disable register=0x%08x\n",
      channelDisableRegister);

  /* Disable channel in MCS mode. */
  p->address->copy_disable_reg = channelDisableRegister;
  /* Disable channel in scaler mode. */
  p->address->count_disable_reg = channelDisableRegister;

  /* Set the prescale factor to the desired value. */
  p->address->lne_prescale_factor_reg = lnePrescale;

  /* Enable or disable 50 MHz channel 1 reference pulses. */
  if (p->ch1RefEnable)
    p->address->control_status_reg |= CTRL_REFERENCE_CH1_ENABLE;
  else
    p->address->control_status_reg |= CTRL_REFERENCE_CH1_DISABLE;

  /* The interrupt service routine can do floating point, need to save
   * fpContext */
#ifdef vxWorks
  if (fppProbe()==OK)
    p->pFpContext = (FP_CONTEXT *)calloc(1, sizeof(FP_CONTEXT));
  else
    p->pFpContext = NULL;
#endif
  Debug(2, "SIS3820Config: interruptServiceRoutine pointer %p\n",
      interruptServiceRoutine);

  status = devConnectInterruptVME(SIS3820IntVector + card,
      (void *)interruptServiceRoutine, (void *) card);
  if (!RTN_SUCCESS(status)) 
  {
    errPrintf(status, __FILE__, __LINE__, "Can't connect to vector % d\n", 
        SIS3820IntVector + card);
    return (ERROR);
  }

  /* Enable interrupt level in EPICS */
  status = devEnableInterruptLevel(intVME, SIS3820IntLevel);
  if (!RTN_SUCCESS(status)) {
    errPrintf(status, __FILE__, __LINE__,
        "Can't enable enterrupt level %d\n", SIS3820IntLevel);
    return (ERROR);
  }

  /* Write interrupt level to hardware */
  Debug(2, "SIS3820Config: irq before setting IntLevel= 0x%x\n", 
      p->address->irq_config_reg);

  p->address->irq_config_reg &= ~SIS3820_IRQ_LEVEL_MASK;
  p->address->irq_config_reg |= (SIS3820IntLevel << 8);

  Debug(2, "SIS3820Config: IntLevel mask= 0x%x\n", (SIS3820IntLevel << 8));
  Debug(2, "SIS3820Config: irq after setting IntLevel= 0x%x\n", 
      p->address->irq_config_reg);

  /* Write interrupt vector to hardware */
  Debug(2, "SIS3820Config: irq before setting IntLevel= 0x%x\n", 
      p->address->irq_config_reg);

  p->address->irq_config_reg &= ~SIS3820_IRQ_VECTOR_MASK;
  p->address->irq_config_reg |= (SIS3820IntVector + card);

  Debug(2, "SIS3820Config: irq = 0x%x\n", p->address->irq_config_reg);

  /* Enable interrupts in hardware */
  Debug(2, "SIS3820Config: irq before enabling interrupts= 0x%x\n", 
      p->address->irq_config_reg);

  p->address->irq_config_reg |= SIS3820_IRQ_ENABLE;

  Debug(2, "SIS3820Config: irq after enabling interrupts= 0x%x\n", 
      p->address->irq_config_reg);

  drvSIS3820Erase(card);
  return (OK);
}

int drvSIS3820GetConfig(int card, int *maxSignals, int *maxChans, 
    int *ch1RefEnable, int *softAdvance)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);
    *maxSignals = p->maxSignals;
    *maxChans = p->maxChans;
    *ch1RefEnable = p->ch1RefEnable;
    *softAdvance = p->softAdvance;
    return(OK);
}
    
int drvSIS3820Init(int card)
{
    /* This routine is called by device support. It returns status indicating
     * if the specfied card exists.
     */
    if ((strCard == 0) || (strCard[card].exists == 0))
        return (ERROR);
    else
        return (OK);
}

static void interruptServiceRoutine(int card)
{
    struct strCard *p = &strCard[card];
    epicsUInt32 irqStatusReg;

    Debug(5, "interruptServiceRoutine: entering\n");

    /* Disable interrupt */
    p->address->irq_config_reg &= ~SIS3820_IRQ_ENABLE;

    /* Save floating point context if floating point hardware is present */
#ifdef vxWorks
    if (p->pFpContext != NULL) fppSave (p->pFpContext);
#endif
    
    /* Test which interrupt source has triggered this interrupt. */
    irqStatusReg = p->address->irq_control_status_reg;

    /* Check for the preset count reached interrupt */
    if (irqStatusReg & SIS3820_IRQ_SOURCE2_FLAG)
    {
      /* Acquisition has completed */
      
      /* Reset the interrupt source */
      p->address->irq_control_status_reg = SIS3820_IRQ_SOURCE2_CLEAR;

      /* Clear software acquiring flag */
      p->acquiring = 0;

      /* Tell record support that the hardware has finished counting. */
      scaler_state[card]->done = 1;

      /* Get the record processed */
      callbackRequest(scaler_state[card]->pcallback);
    }

#ifdef vxWorks
    if (p->pFpContext != NULL) fppRestore(p->pFpContext);
#endif

    /* Enable interrupt */
    p->address->irq_config_reg |= SIS3820_IRQ_ENABLE;
}

static int readFIFO(int card)
{
  /*
    struct strCard *p = &strCard[card];
    long *last = p->buffer + (p->maxSignals * p->nChans);

    Debug(10, "readFIFO: csr=%lx\n", p->address->csr_reg);
    Debug(10, "readFIFO: irq=%lx\n", p->address->irq_reg);

    */
    /* First check if FIFO is empty and return immediately if it is */
  /*
    if (p->address->csr_reg & CSR_M_FIFO_FLAG_EMPTY) return (OK);
    */

    /* Now read out FIFO. It would be more efficient not to check the empty
     * flag on each transfer, using the almost empty flag.  But this has gotten
     * too complex, and is unlikely to save time on new boards with lots of
     * memory.
     */
  /*
    switch (p->mode) {
    case MULTICHANNEL_SCALER_MODE:
        while ((p->buffPtr <= last) && 
                !(p->address->csr_reg & CSR_M_FIFO_FLAG_EMPTY)) {
            *p->buffPtr++ = p->address->fifo_reg;
        }
        break;
    case SIMPLE_SCALER_MODE:
        while (!(p->address->csr_reg & CSR_M_FIFO_FLAG_EMPTY)) {
            Debug(10, "readFIFO: nextSignal=%d\n", p->nextSignal);
            *(p->buffer + p->nextSignal) += p->address->fifo_reg;
            p->nextSignal++;
            if (p->nextSignal == p->maxSignals) {
                p->nextSignal = 0;
                */
                /* We check acquire status frequently so we stop within one channel
                 * advance pulse of when we should stop */
  /*
                checkAcquireStatus(card);
            }
        }
        break;
    }
    */
    return (OK);
}

int drvSIS3820Erase(int card)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);
    INTERLOCK_ON;

    /* Erase buffer in driver */
    Debug(1, "drvSIS3820Erase: card=%d\n", card);
    /*
    p->buffPtr = p->buffer;
    for (i=0; i < (p->maxSignals * p->nChans); i++) *p->buffPtr++ = 0;
    */

    /* Reset buffer pointer to start of buffer */
    /*
    p->buffPtr = p->buffer;
    p->nextChan = 0;
    p->nextSignal = 0;
    */

    /* Erase FIFO and counters on board */
    p->address->key_fifo_reset_reg= 1;
    p->address->key_counter_clear= 1;

    /* Reset the elapsed time */
    /*
    p->elapsedTime = 0.0;
    */
    /* Reset the elapsed counts */
    /*
    for (i=0; i<p->maxSignals; i++) p->elapsedCounts[i] = 0;
    */
    /* Reset the start time.  This is necessary here because we may be
     * acquiring, and AcqOn will not be called.  Normally this is set in AcqOn.
     */
    /*
    p->startTime = tickGet();
    p->elapsedPrevious = 0.;
    */
    INTERLOCK_OFF;
    return (OK);
}

int drvSIS3820Read(int card, long *buff)
{
  struct strCard *p = &strCard[card];
  long *out=buff;
  epicsUInt32 *in = p->address->counter_regs;
  int i;

  if (p->exists == 0) return (ERROR);

  INTERLOCK_ON;
  Debug(1, "drvSIS3820Read: card=%d\n",card);

  for (i=0; i < SIS3820_MAX_SIGNALS; i++) 
  {
    *out++ = *in++;
  }

  Debug(4, "drvSIS3820Read: interrupt status register = 0x%08x\n", 
      p->address->irq_control_status_reg);

  Debug(1, "drvSIS3820Read: copied data OK\n");
  INTERLOCK_OFF;
  return (0);
}

int drvSIS3820SetNChans(int card, int nchans)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    p->nChans = nchans;
    Debug(1, "drvSIS3820SetNChans: p->nChans=%d\n", p->nChans);
    INTERLOCK_OFF;
    return (0);
}

int drvSIS3820GetAcqStatus(int card, int signal, int *acq_status)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    *acq_status = p->acquiring;
    Debug(1, "drvSIS3820GetAcqStatus: p->acquiring=%d\n", p->acquiring);
    INTERLOCK_OFF;
    return (0);
}

static int updateElapsed(int card)
{
  /*
    struct strCard *p = &strCard[card];
    int i, j, signal;

    Debug(2, "updateElapsed: p->buffPtr=%p\n", p->buffPtr);
    Debug(2, "updateElapsed: p->nextChan=%d\n", p->nextChan);
    Debug(2, "updateElapsed: p->nChans=%d\n", p->nChans);

    p->nextChan = (p->buffPtr - p->buffer) / p->maxSignals;
    p->elapsedTime = p->elapsedPrevious + 
                     (double) (tickGet() - p->startTime) / sysClkRateGet();
    for (signal=0; signal<p->maxSignals; signal++) {
        if (p->presetCounts[signal] > 0) {
            p->elapsedCounts[signal] = 0;
            for (i = p->presetStartChan[signal], 
                 j = p->presetStartChan[signal] * p->maxSignals + signal; 
                 i <= p->presetEndChan[signal]; 
                 i++, j += p->maxSignals) {
                    p->elapsedCounts[signal] += p->buffer[j];
            }
            Debug(5, "updateElapsed: p->presetCounts=%ld\n",
                            p->presetCounts[signal]);
            Debug(5, "updateElapsed: p->elapsedCounts=%ld\n",
                            p->elapsedCounts[signal]);
        }
    }
    */
    return (OK);
}


static int checkAcquireStatus(int card)
{
    /* This function checks whether any of the presets has been reached.
       It returns TRUE if at least one preset has been reached, and false
       otherwise.  If a preset has been reached and the module is currently
       acquiring, it calls qcqOff(). */

  /*
    struct strCard *p = &strCard[card];
    int presetReached = FALSE;
    int signal;

    */
    /* Update elapsed time and elapsed counts only if currently acquiring */
  /*
    if (p->acquiring) updateElapsed(card);

    if (p->nextChan >= p->nChans) {
    */
        /* WORK NEEDED: Logic for number of sweeps goes here */
  /*
        presetReached = TRUE;
        Debug(1, "drvSIS3820CheckAcquire status: preset channels reached\n");
    }

    if (p->presetTime > 0.0) {
        if (p->elapsedTime >= p->presetTime) {
            presetReached = TRUE;
            Debug(1, "drvSIS3820CheckAcquire status: preset time reached\n");
        }
    }

    for (signal=0; signal<p->maxSignals; signal++) {
        if (p->presetCounts[signal] > 0) {
            if (p->elapsedCounts[signal] >= p->presetCounts[signal]) {
                presetReached = TRUE;
                Debug(1, 
                    "drvSIS3820CheckAcquire status: preset counts reached\n");
            }
        }
    }

    if (p->acquiring && presetReached) acqOff(card);
    return (presetReached);
    */
  return (0);
}



int drvSIS3820AcqOn(int card)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    /* If already acquiring nothing to do */
     Debug(5,"drvSIS3820AcqOn: acquiring status = %d\n",
               p->acquiring);
     
    if (p->acquiring)
    {
      Debug(5,"drvSIS3820AcqOn: already acquiring\n");
      INTERLOCK_OFF;
      return (OK);
    }

    /* If any preset already reached then don't start acquiring */
    if (checkAcquireStatus(card))
    {
      Debug(5,"drvSIS3820AcqOn: preset reached\n");
      INTERLOCK_OFF;
      return (OK);
    }

    /* Enable counting */

    /*
    p->address->count_disable_reg = 0x0;

    */
    Debug(5, "drvSIS3820AcqOn: count enable register (0=enable) = 0x%08x\n",
        p->address->count_disable_reg);

    p->address->key_op_enable_reg = 1;

    /* Set software acquiring flag */
    p->acquiring = 1;

    Debug(1, "drvSIS3820AcqOn: p->acquiring=%d\n", p->acquiring);
    Debug(1, "drvSIS3820AcqOn: p->startTime=%d\n", p->startTime);

    /* Enable interrupt */
    p->address->irq_config_reg |= SIS3820_IRQ_ENABLE;
    
    INTERLOCK_OFF;
    return (OK);
}

int drvSIS3820AcqOff(int card)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;

    /* Call low-level routine */
    acqOff(card);

    INTERLOCK_OFF;
    return (OK);
}

static int acqOff(int card)
{
  struct strCard *p = &strCard[card];

  if (p->exists == 0) return (ERROR);

  /* If not acquiring then nothing to do */
  if (!p->acquiring) return (OK);

  /* Disable interrupt */
  p->address->irq_control_status_reg &= ~(SIS3820_IRQ_ENABLE);

  /* Set disable counting bits in register */
  /*
  p->address->count_disable_reg = 0xffffffff;
  */

  /* Disable operation of the board */
  p->address->key_op_disable_reg = 1;

  /* Erase FIFO and counters on board */
  p->address->key_fifo_reset_reg = 1;
  p->address->key_counter_clear = 1;

  /* Clear software acquiring flag */
  p->acquiring = 0;

  return (OK);
}

int drvSIS3820ClearPresets(int card)
{
  /* Clear all preset prior to setting new ones. */
  struct strCard *p = &strCard[card];

  Debug(2, "drvSIS3820ClearPresets: entering\n");

  if (p->exists == 0) return (ERROR);

  INTERLOCK_ON;

  /* Disable both banks of preset counters. */

  /*
  p->address->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP1;
  p->address->preset_enable_reg &= ~SIS3820_PRESET_STATUS_ENABLE_GROUP2;
  */
  
  Debug(2, "drvSIS3820ClearPresets: preset enable register = 0x%08x\n",
      p->address->preset_enable_reg);

  /* Reset the preset count values */

  p->address->preset_group1_reg = 111;
  p->address->preset_group2_reg = 222;

  Debug(2, "drvSIS3820ClearPresets: preset count bank 1 = 0x%08x\n",
      p->address->preset_group1_reg);
  Debug(2, "drvSIS3820ClearPresets: preset count bank 2 = 0x%08x\n",
      p->address->preset_group2_reg);

  INTERLOCK_OFF;
  return (OK);
}

  
int drvSIS3820SetPresets(int card, int signal, long pcounts)
{
  struct strCard *p = &strCard[card];
  epicsUInt32 presetChannelSelectRegister;

  if (p->exists == 0) return (ERROR);

  INTERLOCK_ON;
  p->presetCounts[signal] = pcounts;

  /* Set the preset counter value. The SIS3820 has one preset value for
   * signals 1-16, and another for signals 17-32.
   */
  if (signal < 16)
  {
    /* Set the preset counter value. */
    p->address->preset_group1_reg = pcounts;

    Debug(2, "drvSIS3820SetPresets: preset count bank 1 = %d\n",
        p->address->preset_group1_reg);

    /* Enable this bank of counters for preset checking */
    /*
    p->address->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP1;
    */

    Debug(2, "drvSIS3820SetPresets: preset enable register = 0x%08x\n",
        p->address->preset_enable_reg);

    /* Set the correct channel for checking against the preset value */
    presetChannelSelectRegister = p->address->preset_channel_select_reg;
    
    Debug(2, "drvSIS3820SetPresets: preset channel select register = 0x%08x\n",
        p->address->preset_channel_select_reg);

    presetChannelSelectRegister &= ~SIS3820_FOUR_BIT_MASK;
    presetChannelSelectRegister |= signal;

    Debug(2, "drvSIS3820SetPresets: preset channel select = 0x%08x\n",
        presetChannelSelectRegister);

    p->address->preset_channel_select_reg = presetChannelSelectRegister;

    Debug(2, "drvSIS3820SetPresets: preset channel select = 0x%08x\n",
        p->address->preset_channel_select_reg);
  }
  else
  {
    /* Set the preset counter value. */
    p->address->preset_group2_reg = pcounts;

    Debug(2, "drvSIS3820SetPresets: preset count bank 2 = %d\n",
        p->address->preset_group2_reg);

    /* Enable this bank of counters for preset checking */
    /*
    p->address->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP2;
    */

    Debug(2, "drvSIS3820SetPresets: preset enable register = 0x%08x\n",
        p->address->preset_enable_reg);

    /* Set the correct channel for checking against
     * the preset value */
    presetChannelSelectRegister = p->address->preset_channel_select_reg;

    presetChannelSelectRegister &= ~(SIS3820_FOUR_BIT_MASK << 16);
    presetChannelSelectRegister |= (signal << 16);

    Debug(2, "drvSIS3820SetPresets: preset channel select = 0x%08x\n",
        presetChannelSelectRegister);

    p->address->preset_channel_select_reg = presetChannelSelectRegister;

    Debug(2, "drvSIS3820SetPresets: preset channel select = 0x%08x\n",
        p->address->preset_channel_select_reg);
  }

  INTERLOCK_OFF;
  return (OK);
}

int drvSIS3820EnablePresetBank(int card, int bank)
{
  /* This function enables the preset function for the requested bank of preset
   * counter.
   */

  struct strCard *p = &strCard[card];

  Debug(5, "drvSIS3820EnablePresetBank: entering. bank = %d\n", bank);

  if (bank == 0)
  {
    p->address->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP1;
    return (0);
  }
  else if (bank == 1)
  {
    p->address->preset_enable_reg |= SIS3820_PRESET_STATUS_ENABLE_GROUP2;
    return (0);
  }

  /* Incorrect scaler bank number sent */

  return (-1);
}

int drvSIS3820DisablePresetBank(int card, int bank)
{
  /* This function disables the preset function for the requested bank of preset
   * counter.
   */

  struct strCard *p = &strCard[card];

  Debug(5, "drvSIS3820DisablePresetBank: entering. bank = %d\n", bank);

  if (bank == 0)
  {
    p->address->preset_enable_reg &= (~SIS3820_PRESET_STATUS_ENABLE_GROUP1);
    return (0);
  }
  else if (bank == 1)
  {
    p->address->preset_enable_reg &= (~SIS3820_PRESET_STATUS_ENABLE_GROUP2);
    return (0);
  }

  /* Incorrect scaler bank number sent */

  return (-1);
}



  
/*
int drvSIS3820SetChannelAdvance(int card, int source, int prescale)
{
  struct strCard *p = &strCard[card];

  if (p->exists == 0) return (ERROR);

  INTERLOCK_ON;
  p->lneSource = source;
  p->lnePrescale = prescale;
  Debug(1, "drvSIS3820SetChannelAdvance: source=%d\n", source);
  Debug(1, "                             prescale=%d\n", prescale);
  INTERLOCK_OFF;
  return (OK);
}
*/

/**********************/
/* Internal functions */
/**********************/

static epicsUInt32 setIrqControlStatusReg()
{
  /* Set the desired interrupts. The SIS3820 can generate interrupts on
   * thefollowing conditions:
   * 0 = LNE/clock shadow
   * 1 = FIFO threshold
   * 2 = Acquisition completed
   * 3 = Overflow
   * 4 = FIFO almost full
   Initially, we are interested in the acquisition completed condition
   */
  epicsUInt32 interruptRegister = 0;

  interruptRegister += SIS3820_IRQ_SOURCE2_ENABLE;

  interruptRegister += SIS3820_IRQ_SOURCE0_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE1_DISABLE;
  /*interruptRegister += SIS3820_IRQ_SOURCE2_DISABLE;*/
  interruptRegister += SIS3820_IRQ_SOURCE3_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE4_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE5_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE6_DISABLE;
  interruptRegister += SIS3820_IRQ_SOURCE7_DISABLE;

  interruptRegister += SIS3820_IRQ_SOURCE0_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE1_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE2_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE3_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE4_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE5_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE6_CLEAR;
  interruptRegister += SIS3820_IRQ_SOURCE7_CLEAR;

  return interruptRegister;
}

static epicsUInt32 setOpModeReg(int inputMode, int outputMode)
{
  /* Set up the operation mode of the SIS3820.
     Initial mode will be:
   * Preset scaler
   * Front panel output mode from parameter 
   * Front panel input mode from parameter
   * Internal start for histogramming scaler
   * FIFO emulation mode
   * Arm/enable mode 0
   * LNE sourced from VME bus
   * 32 bit data format
   * Non-clearing mode - scaler will be cleared by the software
   */

  epicsUInt32 operationRegister = 0;

  operationRegister += SIS3820_NON_CLEARING_MODE;
  operationRegister += SIS3820_MCS_DATA_FORMAT_32BIT;
  operationRegister += SIS3820_SCALER_DATA_FORMAT_32BIT;
  operationRegister += SIS3820_LNE_SOURCE_VME;
  operationRegister += SIS3820_ARM_ENABLE_CONTROL_SIGNAL;
  operationRegister += SIS3820_FIFO_MODE;
  operationRegister += SIS3820_HISCAL_START_SOURCE_VME;

  /* Check that input mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */

  if (inputMode < 0 || inputMode > 5)
    inputMode = 0;

  operationRegister += inputMode << SIS3820_INPUT_MODE_SHIFT;

  /* Check that output mode is in the allowable range. If so, shift the mode
   * requested by the correct number of bits, and add to the register.
   */

  if (outputMode < 0 || outputMode > 2)
    outputMode = 0;

  operationRegister += outputMode << SIS3820_OUTPUT_MODE_SHIFT;
  
  /* Set the scaler mode to simple scaler */

  operationRegister += SIS3820_OP_MODE_SCALER;

  Debug(3, "setOpModeReg: operationRegister = %d\n", operationRegister);

  return (operationRegister);
}

static epicsUInt32 setControlStatusReg()
{
  /* Set up the default behaviour of the card */
  /* Initially, this will have the following:
   * User LED off
   * Counter test modes disabled
   * Reference pulser enabled to Channel 1
   * LNE prescaler active
   */

  epicsUInt32 controlRegister = 0;

  controlRegister += CTRL_USER_LED_OFF;
  controlRegister += CTRL_COUNTER_TEST_25MHZ_DISABLE;
  controlRegister += CTRL_COUNTER_TEST_MODE_DISABLE;
  controlRegister += CTRL_REFERENCE_CH1_ENABLE;

  return (controlRegister);
}

