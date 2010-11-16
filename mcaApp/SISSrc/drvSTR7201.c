/* drvSTR7201.c */

/* drvSTR7201.c - Driver Support Routines for Struck STR7201 multiscaler */
/*
 *      Author:  Mark Rivers
 *      Date:    11/9/97
 *
 * Modification Log:
 * -----------------
 * .01  11/09/97  mlr  Developed from AIM device support
 * .02  2/06/99   mlr  Implemented pre-scaler in new SIS3801.  Removed some
 *                     optimization using AlmostEmpty flag, since it was
 *                     getting too complicated to support different versions
 *                     with different meanings of this flag.
 * .03  3/22/99   mlr  Added some debugging
 * .04  3/29/99   mlr  Fixed logic to add readFIFO to drvSTR7201AcqOff and
 *                     drvSTR7201GetAcqStatus.  Acquisition was not terminating
 *                     when it should, and data were not being read when
 *                     acquisition stopped.
 * .05  3/30/99   mlr  Fixed logic with preset counts
 * .06  09/12/99  mlr  Fixed #include to use "" instead of <> so 
 *                     "gnumake depends" works
 * .07  12/05/99  mlr  Added call to drvSTR7201Erase to STR7201Config, needs
 *                     this initialization.
 *                     Fixed bug in the elapsed time logic.  Start time was
 *                     being set in AcqOn, even if an erase had not been done.
 *                     Minor changes to avoid compiler warnings.
 * .08  12/12/99  mlr  Added in Tim Mooney's hardware check in functions called
 *                     from devSup.
 * .09  02/04/00  mlr  Added support for "simple" scaler mode in addition to
 *                     multichannel scaler mode.  Added "mode" and "signal" 
 *                     parameters to drvST7201SetPresets.
 * .10  08/12/00  mlr  Zeroed strCard structure via calloc rater than malloc
 *                     when creating it.  Things like presetEndChan were not
 *                     initialized to zero, and this could cause problems.
 *                     Added workaround for problem with prescaler not
 *                     clearing its internal counter when the prescale value is
 *                     changed.
 *                     Added code to save/restore floating point registers
 *                     in interrupt service routine.  This was a serious bug.
 *                     Fixed some debugging formatting, minor changes to avoid
 *                     compiler warnings.
 * .10  09/09/00  mlr  Fixed problem with prescaler not clearing its internal 
 *                     counter when the prescale value is changed.  SIS told me
 *                     the correct way to program the prescaler, which is to
 *                     disable it, then write the prescale factor, then enable it.
 * .11  10/03/00  mlr  Fixed serious logic problem.  Multiple threads, including
 *                     interrupt routine could call readFIFO, causing bus errors.
 *                     Add semaphore to interlock access to hardware and to strCard
 *                     structure, and disable interrupts when normal threads are
 *                     accessing hardware.
 *                     Improved logic in simple scaler mode so that acquisition is
 *                     stopped within one channel advance pulse of when it should stop
 *                     by calling checkAcquireStatus inside readFIFO.
 * .12  10/04/00  mlr  Added ability to enable channel 1 25MHz reference pulses.
 *                     Additional argument to STR7201Config.
 * .13  10/24/00  mlr  Added drvSTR7201GetConfig() to return configuration.
 * .14  10/16/02  mlr  Fixed bug with 25MHz reference pulses, can't use with old
 *                     boards.
 */


#include	<vxWorks.h>
#include        <vxLib.h>
#include	<types.h>
#include	<stdioLib.h>
#include        <stdlib.h>
#include	<wdLib.h>
#include	<memLib.h>
#include	<vme.h>
#include        <tickLib.h>
#include	<string.h>
#include	<taskLib.h>
#include	<fppLib.h>
#include        <sysLib.h>

#include	<alarm.h>
#include	<callback.h>
#include	<dbDefs.h>
#include	<dbAccess.h>
#include	<errlog.h>
#include	<recSup.h>
#include        <epicsExport.h>
#include	<devSup.h>
#include	<devLib.h>
#include	<link.h>

#include	"drvSTR7201.h"


typedef volatile struct {
    uint32_t csr_reg;               /* Offset = 0x00 */
    uint32_t irq_reg;               /* Offset = 0x04 */
    uint32_t unused08;
    uint32_t copy_disable_reg;      /* Offset = 0x0c */
    uint32_t fifo_write_reg;        /* Offset = 0x10 */
    uint32_t unused14;
    uint32_t unused18;
    uint32_t unused1c;
    uint32_t clear_fifo_reg;        /* Offset = 0x20 */
    uint32_t soft_next_reg;         /* Offset = 0x24 */
    uint32_t enable_next_reg;       /* Offset = 0x28 */
    uint32_t disable_next_reg;      /* Offset = 0x2c */
    uint32_t brd_clear_fifo_reg;    /* Offset = 0x30 */
    uint32_t brd_soft_next_reg;     /* Offset = 0x34 */
    uint32_t brd_enable_next_reg;   /* Offset = 0x38 */
    uint32_t brd_disable_next_reg;  /* Offset = 0x3c */
    uint32_t unused40_4c[4];
    uint32_t enable_ch1_pulser;     /* Offset - 0x50 */
    uint32_t disable_ch1_pulser;    /* Offset - 0x54 */
    uint32_t unused58_5c[2];
    uint32_t key_reset_reg;         /* Offset = 0x60 */
    uint32_t unused64;     
    uint32_t test_pulse_reg;        /* Offset = 0x68 */
    uint32_t unused6c;     
    uint32_t unused70_7c[4];
    uint32_t prescale_factor_reg;   /* Offset = 0x80 */
    uint32_t unused84_fc[31];
    uint32_t fifo_reg;              /* Offset = 0x100 */
    uint32_t unused104_7fc[447];    /* Unused space to 2KB per board */
} STR7201_REGS;


/* Hardware register definitions */
#define CSR_M_USER_LED                  0x00000001
#define CSR_M_FIFO_TEST_MODE            0x00000002
#define CSR_M_INPUT_MODE_BIT_0          0x00000004
#define CSR_M_INPUT_MODE_BIT_1          0x00000008
#define CSR_M_TEST_PULSES               0x00000010
#define CSR_M_INPUT_TEST_MODE           0x00000020
#define CSR_M_BROADCAST_MODE            0x00000040
#define CSR_M_10MHZ_LNE_MODE            0x00000040
#define CSR_M_BROADCAST_HAND            0x00000080
#define CSR_M_ENABLE_PRESCALER          0x00000080
#define CSR_M_FIFO_FLAG_EMPTY           0x00000100
#define CSR_M_FIFO_FLAG_ALMOST_EMPTY    0x00000200
#define CSR_M_FIFO_FLAG_HALF_FULL       0x00000400
#define CSR_M_FIFO_FLAG_ALMOST_FULL     0x00000800
#define CSR_M_FIFO_FLAG_FULL            0x00001000
#define CSR_M_ENABLE_NEXT_LOGIC         0x00008000
#define CSR_M_ENABLE_EXT_NEXT           0x00010000
#define CSR_M_ENABLE_EXT_CLEAR          0x00020000
#define CSR_M_ENABLE_EXT_DIS            0x00040000
#define CSR_M_DISABLE_COUNTING          0x00080000
#define CSR_M_ENABLE_IRQ_0              0x00100000
#define CSR_M_ENABLE_IRQ_1              0x00200000
#define CSR_M_ENABLE_IRQ_2              0x00400000
#define CSR_M_ENABLE_IRQ_3              0x00800000
#define CSR_M_INTERNAL_VME_IRQ          0x04000000
#define CSR_M_VME_IRQ                   0x08000000
#define CSR_M_VME_IRQS_0                0x10000000
#define CSR_M_VME_IRQS_1                0x20000000
#define CSR_M_VME_IRQS_2                0x40000000
#define CSR_M_VME_IRQS_3                0x80000000

#define IRQ_M_VME_IRQ_ENABLE            0x00000800

#define SET_CSR_BIT(csr, bit)  (csr) |= (bit); (csr) &= ~((bit) << 8)
#define CLEAR_CSR_BIT(csr, bit)  (csr) &= ~(bit); (csr) |= ((bit) << 8)

/* Define macros to interlock access to card, including interrupts */
# define INTERLOCK_ON \
         {semTake(p->lock, WAIT_FOREVER); \
          CLEAR_CSR_BIT(p->csr_write, CSR_M_ENABLE_IRQ_3); \
          p->address->csr_reg = p->csr_write;}

# define INTERLOCK_OFF \
         {SET_CSR_BIT(p->csr_write, CSR_M_ENABLE_IRQ_3); \
          p->address->csr_reg = p->csr_write; \
          semGive(p->lock);}

/* Default maximum number of cards.  User must call STR7201Setup if there is
   more than 1 card.  This is a global variable because some device support
   routines need to use it. */
int STR7201NumCards=1;


/* Debug support */
#define Debug(l,FMT,V) {if (l <= drvSTR7201Debug) \
			  { printf("%s(%d):",__FILE__,__LINE__); \
                            printf(FMT,V);}}

volatile int drvSTR7201Debug = 0;
epicsExportAddress(int, drvSTR7201Debug);

/* Default address of first card.  note, this is not factory default base
  address, since our vxWorks BSP does not provide access to those addresses */
static STR7201_REGS *STR7201BaseAddress = (STR7201_REGS *) 0xA0000000;

/* Default interrupt vector */
static STR7201IntVector = 220;

/* Default interrupt level */
static STR7201IntLevel = 6;


static struct strCard {
    int exists;
    int moduleID;
    int firmwareVersion;
    STR7201_REGS *address;
    uint32_t csr_write;
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
    int presetStartChan[STR7201_MAX_SIGNALS];
    int presetEndChan[STR7201_MAX_SIGNALS];
    long presetCounts[STR7201_MAX_SIGNALS];
    long elapsedCounts[STR7201_MAX_SIGNALS];
    int lneSource;
    int lnePrescale;
    int ch1RefEnable;
    int softAdvance;
    long *buffer;  /* maxSignals * maxChans */
    long *buffPtr;
    int acquiring;
    FP_CONTEXT *pFpContext;
    SEM_ID lock;
} *strCard=0;

/* Function prototypes */
static int csr_defaults(int firmWareVersion);
static void interruptServiceRoutine(void *arg);
static int readFIFO(int card);
static int checkAcquireStatus(int card);
static int updateElapsed(int card);
static int acqOff(int card);


int STR7201Setup(int numCards, int baseAddress, int intVector, int intLevel)
{
    STR7201NumCards = numCards;
    STR7201BaseAddress = (STR7201_REGS *) baseAddress;
    STR7201IntVector = intVector;
    STR7201IntLevel = intLevel;
    return (0);
}

int STR7201Config(int card, int maxSignals, int maxChans, int ch1RefEnable, int softAdvance)
{
    struct strCard *p;
    int i;
    int status;
    int csr;

    /* Allocate and initialize strCard if not done yet */
    if (strCard == 0) {
        Debug(1, "STR7201Config: numCards=%d\n", STR7201NumCards);
        strCard = (struct strCard*)
                        calloc(1, sizeof(struct strCard)*STR7201NumCards);
        for (i=0; i<STR7201NumCards; i++) {
            strCard[i].exists = 0;
            status = devRegisterAddress("drvSTR7201", atVMEA32,
                     (size_t)(STR7201BaseAddress + i), 2048,
                     (volatile void **)&strCard[i].address);
            if (!RTN_SUCCESS(status)) {
                errPrintf(status, __FILE__, __LINE__, 
                    "Can't register address for STR7201 card %d\n", i);
                return (ERROR);
            }
            Debug(1, "STR7201Config: configuring card=%d\n", i);
            Debug(1, "STR7201Config: local address=%p\n", strCard[i].address);
            /* Probe VME bus to see if card is there */
            if (vxMemProbe((char *) &strCard[i].address->csr_reg, 
                   VX_READ, 4, (char *) &csr) == OK) strCard[i].exists = 1;
        }
    }
    p=&strCard[card];
    if (p->exists == 0) return (ERROR);
    /* WORKED NEEDED - WILL THIS WORK WITH OLD STRUCK BOARDS? */
    p->moduleID = (p->address->irq_reg & 0xFFFF0000) >> 16;
    Debug(2, "STR7201Config: module ID=%x\n", p->moduleID);
    p->firmwareVersion = (p->address->irq_reg & 0x0000F000) >> 12;
    Debug(2, "STR7201Config: firmwareVersion=%d\n", p->firmwareVersion);
    p->maxSignals = maxSignals;
    p->maxChans = maxChans;
    p->acquiring = 0;
    p->startTime = 0;
    p->nChans = maxChans;
    p->nextChan = 0;
    p->nextSignal = 0;
    p->lneSource = LNE_EXTERNAL;
    p->lnePrescale = 0;
    p->ch1RefEnable = ch1RefEnable;
    p->softAdvance = softAdvance;
    p->buffer = (long *)malloc(maxSignals*maxChans*sizeof(long));

    /* Create semaphore to interlock access to card */
    p->lock = semBCreate(SEM_Q_FIFO, SEM_FULL);

    /* Reset card */
    Debug(2, "STR7201Config: resetting card %d\n", card);
    p->address->key_reset_reg = 1;

    /* Clear FIFO */
    Debug(2, "%s", "STR7201Config: clearing FIFO\n");
    p->address->clear_fifo_reg = 1;

    /* Initialize board */
    p->address->key_reset_reg = 1;
    p->csr_write = csr_defaults(p->firmwareVersion);
    p->address->csr_reg = p->csr_write;

    /* 
     * Set number of readout channels to maxSignals
     * Create a mask with zeros in the rightmost maxSignals bits, 
     * 1 in all higher order bits.
     */
    Debug(2, "STR7201Config: setting readout channels=%d\n",maxSignals);
    p->address->copy_disable_reg = 0xffffffff<<maxSignals;

    /* Set the prescale factor to 0.  Only on new boards */
    if (p->firmwareVersion >= 5) p->address->prescale_factor_reg = 0;

    /* Enable or disable 25 MHz channel 1 reference pulses. Only on new boards */
    if (p->firmwareVersion >= 5) {
       if (p->ch1RefEnable)
            p->address->enable_ch1_pulser = 1;
       else
            p->address->disable_ch1_pulser = 1;
    }

    /* The interrupt service routine can do floating point, need to save
     * fpContext */
    if (fppProbe()==OK)
       p->pFpContext = (FP_CONTEXT *)calloc(1, sizeof(FP_CONTEXT));
    else
       p->pFpContext = NULL;

    status = devConnectInterrupt(intVME, STR7201IntVector + card,
                 interruptServiceRoutine, (void *) card);
    if (!RTN_SUCCESS(status)) {
        errPrintf(status, __FILE__, __LINE__, "Can't connect to vector % d\n", 
                    STR7201IntVector + card);
        return (ERROR);
    }

    /* Enable interrupt level in EPICS */
    status = devEnableInterruptLevel(intVME, STR7201IntLevel);
    if (!RTN_SUCCESS(status)) {
        errPrintf(status, __FILE__, __LINE__,
            "Can't enable enterrupt level %d\n", STR7201IntLevel);
        return (ERROR);
    }

    /* Write interrupt level to hardware */
    Debug(2, "STR7201Config: irq before setting IntLevel= %lx\n", 
                p->address->irq_reg);
    p->address->irq_reg |= (STR7201IntLevel << 8);
    Debug(2, "STR7201Config: IntLevel mask= %x\n", (STR7201IntLevel << 8));
    Debug(2, "STR7201Config: irq after setting IntLevel= %lx\n", 
                p->address->irq_reg);
    
    /* Write interrupt vector to hardware */
    p->address->irq_reg |= (STR7201IntVector + card);
    Debug(2, "STR7201Config: irq = %lx\n", p->address->irq_reg);

    drvSTR7201Erase(card);
    return (OK);
}

int drvSTR7201GetConfig(int card, int *maxSignals, int *maxChans, int *ch1RefEnable,
                        int *softAdvance)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);
    *maxSignals = p->maxSignals;
    *maxChans = p->maxChans;
    *ch1RefEnable = p->ch1RefEnable;
    *softAdvance = p->softAdvance;
    return(OK);
}
    
int drvSTR7201Init(int card)
{
    /* This routine is called by device support. It returns status indicating
     * if the specfied card exists.
     */
    if ((strCard == 0) || (strCard[card].exists == 0))
        return (ERROR);
    else
        return (OK);
}


static int csr_defaults(int firmwareVersion)
{
    long csr=0;

    CLEAR_CSR_BIT(csr, CSR_M_USER_LED);         /* Turn off user LED */
    CLEAR_CSR_BIT(csr, CSR_M_FIFO_TEST_MODE);   /* Disable FIFO test mode */
    CLEAR_CSR_BIT(csr, CSR_M_INPUT_MODE_BIT_0); /* Input mode 2 */
    SET_CSR_BIT  (csr, CSR_M_INPUT_MODE_BIT_1);
    CLEAR_CSR_BIT(csr, CSR_M_TEST_PULSES);      /* No 25MHz test pulses */
    CLEAR_CSR_BIT(csr, CSR_M_INPUT_TEST_MODE);  /* No test input */
    if (firmwareVersion < 5) {
        CLEAR_CSR_BIT(csr, CSR_M_BROADCAST_MODE);   /* No broadcast mode */
        CLEAR_CSR_BIT(csr, CSR_M_BROADCAST_HAND);   /* No broadcast handshake */
    } else {
        CLEAR_CSR_BIT(csr, CSR_M_10MHZ_LNE_MODE); /* Disable int. 10 MHz LNE */
        SET_CSR_BIT(csr, CSR_M_ENABLE_PRESCALER); /* Enable LNE prescaler */
    }
    SET_CSR_BIT  (csr, CSR_M_ENABLE_EXT_NEXT);  /* Enable external NEXT */
    SET_CSR_BIT  (csr, CSR_M_ENABLE_EXT_CLEAR); /* Enable external CLEAR */
    SET_CSR_BIT  (csr, CSR_M_ENABLE_EXT_DIS);   /* Enable external DISABLE */
    SET_CSR_BIT  (csr, CSR_M_DISABLE_COUNTING); /* Disable counting */
    CLEAR_CSR_BIT(csr, CSR_M_ENABLE_IRQ_0);     /* Disable start of CIP IRQ */
    CLEAR_CSR_BIT(csr, CSR_M_ENABLE_IRQ_1);     /* Disable FIFO almost empty (full?) */
    CLEAR_CSR_BIT(csr, CSR_M_ENABLE_IRQ_2);     /* Disable FIFO half full */
    SET_CSR_BIT  (csr, CSR_M_ENABLE_IRQ_3);     /* Enable FIFO almost full */

    return (csr);
}


static void interruptServiceRoutine(void *arg)
{
    int card = (int)arg;
    struct strCard *p = &strCard[card];
    int debugLevel;

    /* Disable interrupt */
    p->address->irq_reg &= ~IRQ_M_VME_IRQ_ENABLE;

    /* Save floating point context if floating point hardware is present */
    if (p->pFpContext != NULL) fppSave (p->pFpContext);
    
    /* Read FIFO */
    /* Turn off debugging, can't print at interrupt level */
    debugLevel = drvSTR7201Debug;
    drvSTR7201Debug = 0;
    readFIFO(card);
    checkAcquireStatus(card);
    drvSTR7201Debug = debugLevel;
    if (p->pFpContext != NULL) fppRestore(p->pFpContext);

    /* Enable interrupt */
    p->address->irq_reg |= IRQ_M_VME_IRQ_ENABLE;
}


static int readFIFO(int card)
{
    struct strCard *p = &strCard[card];
    long *last = p->buffer + (p->maxSignals * p->nChans);

    Debug(10, "readFIFO: csr=%lx\n", p->address->csr_reg);
    Debug(10, "readFIFO: irq=%lx\n", p->address->irq_reg);

    /* First check if FIFO is empty and return immediately if it is */
    if (p->address->csr_reg & CSR_M_FIFO_FLAG_EMPTY) return (OK);

    /* Now read out FIFO. It would be more efficient not to check the empty
     * flag on each transfer, using the almost empty flag.  But this has gotten
     * too complex, and is unlikely to save time on new boards with lots of
     * memory.
     */
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
                /* We check acquire status frequently so we stop within one channel
                 * advance pulse of when we should stop */
                checkAcquireStatus(card);
            }
        }
        break;
    }
    return (OK);
}

int drvSTR7201Erase(int card)
{
    struct strCard *p = &strCard[card];
    int i;

    if (p->exists == 0) return (ERROR);
    INTERLOCK_ON;

    /* Erase buffer in driver */
    Debug(1, "drvSTR7201Erase: card=%d\n", card);
    p->buffPtr = p->buffer;
    for (i=0; i < (p->maxSignals * p->nChans); i++) *p->buffPtr++ = 0;

    /* Reset buffer pointer to start of buffer */
    p->buffPtr = p->buffer;
    p->nextChan = 0;
    p->nextSignal = 0;

    /* Erase FIFO and counters on board */
    p->address->clear_fifo_reg = 1;

    /* Reset the elapsed time */
    p->elapsedTime = 0.0;
    /* Reset the elapsed counts */
    for (i=0; i<p->maxSignals; i++) p->elapsedCounts[i] = 0;
    /* Reset the start time.  This is necessary here because we may be
     * acquiring, and AcqOn will not be called.  Normally this is set in AcqOn.
     */
    p->startTime = tickGet();
    p->elapsedPrevious = 0.;
    INTERLOCK_OFF;
    return (OK);
}

int drvSTR7201Read(int card, int signal, int maxChans, int *numChans, long *buff)
{
    struct strCard *p = &strCard[card];
    long *in=p->buffer+signal, *out=buff;
    int i;

    if (p->exists == 0) return (ERROR);

    *numChans = p->nextChan;
    if (*numChans > maxChans) *numChans = maxChans;
    INTERLOCK_ON;
    if (drvSTR7201Debug >= 1) {
        printf("%s(%d):",__FILE__,__LINE__);
        printf("drvSTR7201Read: card=%d, signal=%d, numChans=%d, maxSignals=%d\n",
               card, signal, *numChans, p->maxSignals);
    }
    readFIFO(card);
    Debug(1, "%s", "drvSTR7201Read: readFIFO OK\n");
    checkAcquireStatus(card);
    Debug(1, "%s", "drvSTR7201Read: checkAcquireStatus OK\n");
    for (i=0; i<*numChans; i++) {
        *out++ = *in;
        in += p->maxSignals;
    }
    Debug(1, "%s", "drvSTR7201Read: copied data OK\n");
    INTERLOCK_OFF;
    return (0);
}

int drvSTR7201SetNChans(int card, int nchans)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    p->nChans = nchans;
    Debug(1, "drvSTR7201SetNChans: p->nChans=%d\n", p->nChans);
    INTERLOCK_OFF;
    return (0);
}

int drvSTR7201GetAcqStatus(int card, int signal, float *time, long *ecounts,
                           int *acq_status)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    readFIFO(card);  
    checkAcquireStatus(card);
    *acq_status = p->acquiring;
    *time = p->elapsedTime;
    *ecounts = p->elapsedCounts[signal];
    Debug(1, "drvSTR7201GetAcqStatus: p->acquiring=%d\n", p->acquiring);
    Debug(1, "drvSTR7201GetAcqStatus: time=%f\n", *time);
    INTERLOCK_OFF;
    return (0);
}

static int updateElapsed(int card)
{
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
    return (OK);
}


static int checkAcquireStatus(int card)
{
    /* This function checks whether any of the presets has been reached.
       It returns TRUE if at least one preset has been reached, and false
       otherwise.  If a preset has been reached and the module is currently
       acquiring, it calls qcqOff(). */

    struct strCard *p = &strCard[card];
    int presetReached = FALSE;
    int signal;

    /* Update elapsed time and elapsed counts only if currently acquiring */
    if (p->acquiring) updateElapsed(card);

    if (p->nextChan >= p->nChans) {
        /* WORK NEEDED: Logic for number of sweeps goes here */
        presetReached = TRUE;
        Debug(1, "%s", "drvSTR7201CheckAcquire status: preset channels reached\n");
    }

    if (p->presetTime > 0.0) {
        if (p->elapsedTime >= p->presetTime) {
            presetReached = TRUE;
            Debug(1, "%s", "drvSTR7201CheckAcquire status: preset time reached\n");
        }
    }

    for (signal=0; signal<p->maxSignals; signal++) {
        if (p->presetCounts[signal] > 0) {
            if (p->elapsedCounts[signal] >= p->presetCounts[signal]) {
                presetReached = TRUE;
                Debug(1, "%s", "drvSTR7201CheckAcquire status: preset counts reached\n");
            }
        }
    }

    if (p->acquiring && presetReached) acqOff(card);
    return (presetReached);
}



int drvSTR7201AcqOn(int card)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    /* If already acuiring nothing to do */
    if (p->acquiring) goto done;

    /* If any preset already reached then don't start acquiring */
    if (checkAcquireStatus(card)) goto done;

    /* The following should only be done on 38xx boards with Rev. 5 firmware */
    if (p->firmwareVersion >= 5) {
        /* Select internal or external LNE */
        if (p->lneSource == LNE_INTERNAL) {
            SET_CSR_BIT(p->csr_write, CSR_M_10MHZ_LNE_MODE);
        } else {
            CLEAR_CSR_BIT(p->csr_write, CSR_M_10MHZ_LNE_MODE);
        }
        p->address->csr_reg = p->csr_write;
        /* Set the prescale factor. Note, it is necessary to do the following in
         * for the prescale counter to be properly cleared:
         *  - Disable prescaler
         *  - Write prescale factor
         *  - Enable prescaler
         */
        CLEAR_CSR_BIT(p->csr_write, CSR_M_ENABLE_PRESCALER);
        p->address->csr_reg = p->csr_write;
        p->address->prescale_factor_reg = p->lnePrescale;
        SET_CSR_BIT(p->csr_write, CSR_M_ENABLE_PRESCALER);
        p->address->csr_reg = p->csr_write;
    }

    /* Enable next logic */
    p->address->enable_next_reg = 1;

    /* Enable counting bit in CSR */
    CLEAR_CSR_BIT(p->csr_write, CSR_M_DISABLE_COUNTING);
    p->address->csr_reg = p->csr_write;


    /* Do one software next_clock. This starts the module counting without 
     * waiting for the first external next clock. 
     * Only do this if:
     *    Old firmware (not new SIS modules) OR
     *    (Using MCS (not scaler) MODE) AND
     *    ((Using internal channel advance) OR
     *    ((Using external channel advance) AND (softAdvance flag=1))
     */
    if ((p->firmwareVersion < 5) ||
       ((p->mode == MULTICHANNEL_SCALER_MODE) && 
       ((p->lneSource == LNE_INTERNAL) ||
       ((p->lneSource == LNE_EXTERNAL) && (p->softAdvance == 1)))))
       p->address->soft_next_reg = 1; 

    /* Set software acquiring flag */
    p->acquiring = 1;

    p->startTime = tickGet();
    Debug(1, "drvSTR7201AcqOn: p->acquiring=%d\n", p->acquiring);
    Debug(1, "drvSTR7201AcqOn: p->startTime=%d\n", p->startTime);

    /* Enable interrupt */
    p->address->irq_reg |= IRQ_M_VME_IRQ_ENABLE;
done:
    INTERLOCK_OFF;
    return (OK);
}

int drvSTR7201AcqOff(int card)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    /* Read FIFO to empty any valid data */
    readFIFO(card);

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
    p->address->irq_reg &= ~IRQ_M_VME_IRQ_ENABLE;

    /* Disable counting bit in CSR */
    SET_CSR_BIT(p->csr_write, CSR_M_DISABLE_COUNTING);
    p->address->csr_reg = p->csr_write;

    /* Disable next logic */
    p->address->disable_next_reg = 1;

    /* Erase FIFO and counters on board */
    p->address->clear_fifo_reg = 1;

    /* Clear software acquiring flag */
    p->acquiring = 0;

    /* Update elasped time, counts, etc. */
    updateElapsed(card);
    Debug(2, "drvSTR7201Acqoff: p->elapsedTime=%f\n", p->elapsedTime);

    /* Save the time acquired so far in case acquisition is started again */
    p->elapsedPrevious = p->elapsedTime;
    return (OK);
}

int drvSTR7201SetPresets(int card, int signal, float ptime, 
                            int startChan, int endChan, long pcounts, int mode)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    p->presetTime = ptime;
    p->presetStartChan[signal] = startChan;
    p->presetEndChan[signal] = endChan;
    p->presetCounts[signal] = pcounts;
    p->mode = mode;
    Debug(1, "drvSTR7201SetPreset: p->presetTime=%f\n", p->presetTime);
    INTERLOCK_OFF;
    return (OK);
}

int drvSTR7201SetChannelAdvance(int card, int source, int prescale)
{
    struct strCard *p = &strCard[card];

    if (p->exists == 0) return (ERROR);

    INTERLOCK_ON;
    p->lneSource = source;
    p->lnePrescale = prescale;
    Debug(1, "drvSTR7201SetChannelAdvance: source=%d\n", source);
    Debug(1, "                             prescale=%d\n", prescale);
    INTERLOCK_OFF;
    return (OK);
}
