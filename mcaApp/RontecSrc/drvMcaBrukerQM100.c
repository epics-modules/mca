/* drvMcaBrukerQM100.c
	
    Author: Jens Eden
    based on drvMcaRontec from Mark River

	compiles and runs on Windows only

*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <errlog.h>
#include <iocsh.h>
#include <cantProceed.h>
#include <epicsString.h>
#include <epicsTime.h>
#include <asynDriver.h>
#include <asynInt32.h>
#include <asynFloat64.h>
#include <asynInt32Array.h>
#include <asynDrvUser.h>
#include <asynOctetSyncIO.h>

#include "mca.h"
#include "drvMca.h"

#include <WTypes.h>
#include <WinBase.h>
#include <epicsExport.h>


typedef struct {
    mcaCommand command;
    char *commandString;
} mcaCommandStruct;

static mcaCommandStruct mcaCommands[MAX_MCA_COMMANDS] = {
    {mcaStartAcquire,           mcaStartAcquireString},           /* int32, write */
    {mcaStopAcquire,            mcaStopAcquireString},            /* int32, write */
    {mcaErase,                  mcaEraseString},                  /* int32, write */
    {mcaData,                   mcaDataString},                   /* int32Array, read/write */
    {mcaReadStatus,             mcaReadStatusString},             /* int32, write */
    {mcaChannelAdvanceInternal, mcaChannelAdvanceInternalString}, /* int32, write */
    {mcaChannelAdvanceExternal, mcaChannelAdvanceExternalString}, /* int32, write */
    {mcaNumChannels,            mcaNumChannelsString},            /* int32, write */
    {mcaDwellTime,              mcaDwellTimeString},              /* float64, write */
    {mcaPresetLiveTime,         mcaPresetLiveTimeString},         /* float64, write */
    {mcaPresetRealTime,         mcaPresetRealTimeString},         /* float64, write */
    {mcaPresetCounts,           mcaPresetCountsString},           /* float64, write */
    {mcaPresetLowChannel,       mcaPresetLowChannelString},       /* int32, write */
    {mcaPresetHighChannel,      mcaPresetHighChannelString},      /* int32, write */
    {mcaPresetSweeps,           mcaPresetSweepsString},           /* int32, write */
    {mcaModePHA,                mcaModePHAString},                /* int32, write */
    {mcaModeMCS,                mcaModeMCSString},                /* int32, write */
    {mcaModeList,               mcaModeListString},               /* int32, write */
    {mcaSequence,               mcaSequenceString},               /* int32, write */
    {mcaPrescale,               mcaPrescaleString},               /* int32, write */
    {mcaAcquiring,              mcaAcquiringString},              /* int32, read */
    {mcaElapsedLiveTime,        mcaElapsedLiveTimeString},        /* float64, read */
    {mcaElapsedRealTime,        mcaElapsedRealTimeString},        /* float64, read */
    {mcaElapsedCounts,          mcaElapsedCountsString}           /* float64, read */
};

struct TRTSpectrumHeader {
	char 	Ident[26];
	int		version;
	int 	size;
	double	datetime;
	int 	ChannelCount;
	int		ChannelOffset; 
	double	calAbs;
	double	calLin;
	double	SigmaAbs;
	double	SigmaLin;
	int		RealTime;
	int 	LifeTime;
};
/* functions from RTIfcClient.dll */

typedef int (CALLBACK* LPSendComFun)(int, int, char*, char*, int);
typedef int (CALLBACK* LPOpenClient)(char*, char*,char*, BOOL, BOOL, int*);
typedef int (CALLBACK* LPStRFun)(int, int);
typedef int (CALLBACK* LPGetSpecFun)(int, int, struct TRTSpectrumHeader *, int);
typedef int (CALLBACK* LPStartFun)(int, int, int);
typedef int (CALLBACK* LPStartCntFun)(int, int, double, double, int);

static LPOpenClient		pOpenClient;
static LPSendComFun		pSendRCLCommand;
static LPStRFun			pReadSpectrum;
static LPGetSpecFun		pGetSpectrum;
static LPStartFun		pStartSpectrumMeasurement;
static LPStartFun		pStartSpectrumLifeTimeMeasurement;
static LPStartCntFun	pStartSpectrumCounterMeasurement;
static LPStRFun			pStopSpectrumMeasurement;

#define BRUKERQM100_MAXCHANS 4096
#define BRUKERQM100_MESSAGE_SIZE 100
#define BRUKERQM100_TIMEOUT 2.0
#define BRUKERQM100_SPECBUFFERSIZE 4*BRUKERQM100_MAXCHANS + 100

typedef struct {
    int module;
    char *portName;
    char *serialPort;
    int maxChans;
    int nchans;
    int binning;
    int plive;
    int preal;
    int ptotal;
    int ptschan;
    int ptechan;
    int elive;
    int ereal;
    int etotals;
    int exists;
    int acquiring;
	int cid;
	int clientOpen;
	int spuno;
	char *server;
	char *user;
	char *passwd;
	char *specBuffer;
    asynUser *pasynUser;
    asynInterface common;
    asynInterface int32;
    asynInterface float64;
    asynInterface int32Array;
    asynInterface drvUser;
} mcaBrukerQM100Pvt;

/* These functions are in public interfaces */
static asynStatus int32Write        (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 value);
static asynStatus int32Read         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *value);
static asynStatus getBounds         (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *low, epicsInt32 *high);
static asynStatus float64Write      (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 value);
static asynStatus float64Read       (void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 *value);
static asynStatus int32ArrayRead    (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans,
                                     size_t *nactual);
static asynStatus int32ArrayWrite   (void *drvPvt, asynUser *pasynUser,
                                     epicsInt32 *data, size_t maxChans);
static asynStatus drvUserCreate     (void *drvPvt, asynUser *pasynUser,
                                     const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType    (void *drvPvt, asynUser *pasynUser,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy    (void *drvPvt, asynUser *pasynUser);

static void BrukerQM100Report               (void *drvPvt, FILE *fp, int details);
static asynStatus BrukerQM100Connect        (void *drvPvt, asynUser *pasynUser);
static asynStatus BrukerQM100Disconnect     (void *drvPvt, asynUser *pasynUser);

/* Private methods */
static asynStatus BrukerQM100Write(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue);
static asynStatus BrukerQM100Read(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue);
static int sendMessage(mcaBrukerQM100Pvt *pPvt, char *output, char *input);
static int openBQM100Client(void *);

/*
 * asynCommon methods
 */
static struct asynCommon mcaBrukerQM100Common = {
    BrukerQM100Report,
    BrukerQM100Connect,
    BrukerQM100Disconnect
};

/* asynInt32 methods */
static asynInt32 mcaBrukerQM100Int32 = {
    int32Write,
    int32Read,
    getBounds,
    NULL,
    NULL
};

/* asynFloat64 methods */
static asynFloat64 mcaBrukerQM100Float64 = {
    float64Write,
    float64Read,
    NULL,
    NULL
};

/* asynInt32Array methods */
static asynInt32Array mcaBrukerQM100Int32Array = {
    int32ArrayWrite,
    int32ArrayRead,
    NULL,
    NULL
};

static asynDrvUser mcaBrukerQM100DrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


/*
 * portName:	Asyn Port
 * user:		BQM100-User
 * passwd:		Password of BQM100-User
 * server:		Server to connect with (usually "Local server")
 * spuno:		Spectrometer Unit Number (usually 1)
 */
int BrukerQM100Config(const char *portName, char *user, char *passwd, char *server, int spuno)
{
    int status;
    mcaBrukerQM100Pvt *pPvt;

    pPvt = callocMustSucceed(1, sizeof(mcaBrukerQM100Pvt), "BrukerQM100Config");

    /* Copy parameters to object private */
    pPvt->exists = 1;
    pPvt->cid=0;
	pPvt->clientOpen=0;    
    pPvt->maxChans = BRUKERQM100_MAXCHANS;
    pPvt->nchans = pPvt->maxChans;
    pPvt->binning  = 1;
    /* Set reasonable defaults for other parameters */
    pPvt->plive   = 0;
    pPvt->preal   = 0;
    pPvt->ptotal  = 0;
    pPvt->ptschan = 0;
    pPvt->ptechan = 0;
    pPvt->acquiring = 0;
    
    pPvt->spuno=spuno;
    pPvt->portName = epicsStrDup(portName);
    pPvt->user = epicsStrDup(user);
    pPvt->passwd = epicsStrDup(passwd);
    pPvt->server = epicsStrDup(server);

    /*
     *  Link with higher level routines
     */
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&mcaBrukerQM100Common;
    pPvt->common.drvPvt = pPvt;
    pPvt->int32.interfaceType = asynInt32Type;
    pPvt->int32.pinterface  = (void *)&mcaBrukerQM100Int32;
    pPvt->int32.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&mcaBrukerQM100Float64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->int32Array.interfaceType = asynInt32ArrayType;
    pPvt->int32Array.pinterface  = (void *)&mcaBrukerQM100Int32Array;
    pPvt->int32Array.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&mcaBrukerQM100DrvUser;
    pPvt->drvUser.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                        ASYN_MULTIDEVICE | ASYN_CANBLOCK,
                                        1,  /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stacksize */
    if (status != asynSuccess) {
        errlogPrintf("BrukerQM100Config ERROR: Can't register myself.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName, &pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("BrukerQM100Config: Can't register common.\n");
        return -1;
    }
    status = pasynInt32Base->initialize(pPvt->portName, &pPvt->int32);
    if (status != asynSuccess) {
        errlogPrintf("BrukerQM100Config: Can't register int32.\n");
        return -1;
    }

    status = pasynFloat64Base->initialize(pPvt->portName, &pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("BrukerQM100Config: Can't register float64.\n");
        return -1;
    }

    status = pasynInt32ArrayBase->initialize(pPvt->portName, &pPvt->int32Array);
    if (status != asynSuccess) {
        errlogPrintf("BrukerQM100Config: Can't register int32Array.\n");
        return -1;
    }

    status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("BrukerQM100Config ERROR: Can't register drvUser\n");
        return -1;
    }
    
	if (!(pPvt->specBuffer=(char *)malloc(BRUKERQM100_SPECBUFFERSIZE))){ 
		errlogPrintf("(drvMcaBrukerQM100) Unable to allocate Spectrum buffer\n");
        return -1;
	}
    
    return(0);
}


static asynStatus int32Write(void *drvPvt, asynUser *pasynUser,
                             epicsInt32 value)
{
    return(BrukerQM100Write(drvPvt, pasynUser, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    return(BrukerQM100Write(drvPvt, pasynUser, 0, value));
}

static asynStatus BrukerQM100Write(void *drvPvt, asynUser *pasynUser,
                           epicsInt32 ivalue, epicsFloat64 dvalue)
{
    mcaBrukerQM100Pvt *pPvt = (mcaBrukerQM100Pvt *)drvPvt;
    mcaCommand command=pasynUser->reason;
    char response[BRUKERQM100_MESSAGE_SIZE];
    int signal, status;
	
    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
             "drvMcaBrukerQM100::BrukerQM100Write entry, command=%d, signal=%d, "
             "ivalue=%d, dvalue=%f\n", command, signal, ivalue, dvalue);
    switch (command) {
        case mcaStartAcquire:
            /* Start acquisition. */
            pPvt->elive = 0;
            pPvt->ereal = 0;
            if (pPvt->plive > 0.) {
				status=pStartSpectrumLifeTimeMeasurement(pPvt->cid, pPvt->spuno, pPvt->plive);
			}
	        else if (pPvt->preal > 0.) {
			  	status=pStartSpectrumMeasurement(pPvt->cid, pPvt->spuno, pPvt->preal);
			}
	        else if (pPvt->ptotal > 0) {
				status=pStartSpectrumCounterMeasurement(pPvt->cid, pPvt->spuno, ((double)pPvt->ptschan)/1000., ((double)pPvt->ptechan)/1000., pPvt->ptotal);
			}
            else {
			  	status=pStartSpectrumMeasurement(pPvt->cid, pPvt->spuno, 0);
			}
            break;
        case mcaStopAcquire:
            /* stop data acquisition */
            sendMessage(pPvt, "$MP ON", NULL);
            break;
        case mcaErase:
            sendMessage(pPvt, "$CC", NULL);
            break;
        case mcaReadStatus:
            /* Read acquisition status */
            sendMessage(pPvt, "$FP", response);
            if (response[4] == '-') pPvt->acquiring = 1; else pPvt->acquiring = 0;
			/* we need to transfer the spectrum to buffer and read the header */
			if ((status=pReadSpectrum(pPvt->cid, pPvt->spuno)))
				errlogPrintf("(drvMcaBrukerQM100) error(%d) copying spectrum to buffer\n",status);
			if ((status=pGetSpectrum(pPvt->cid, pPvt->spuno,(struct TRTSpectrumHeader *)pPvt->specBuffer,BRUKERQM100_SPECBUFFERSIZE)))
				errlogPrintf("(drvMcaBrukerQM100) error(%d) reading spectrum\n",status);
			pPvt->elive = *((int*)(pPvt->specBuffer + 86));
			pPvt->ereal = *((int*)(pPvt->specBuffer + 82));
            
        case mcaChannelAdvanceInternal:
            /* set channel advance source to internal (timed) */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaChannelAdvanceExternal:
            /* set channel advance source to external */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaNumChannels:
            /* set number of channels */
            pPvt->nchans = ivalue;
            if (pPvt->nchans > pPvt->maxChans) {
                asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                          "mcaBrukerQM100Server Illegal nuse field");
                pPvt->nchans = pPvt->maxChans;
            }
            pPvt->binning = pPvt->maxChans / pPvt->nchans;
            break;
        case mcaModePHA:
            /* set mode to Pulse Height Analysis */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaModeMCS:
            /* set mode to MultiChannel Scaler */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaModeList:
            /* set mode to LIST (record each incoming event) */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaSequence:
            /* set sequence number */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaPrescale:
            /* No-op for BrukerQM100 */
            break;
        case mcaPresetSweeps:
            /* set number of sweeps (for MCS mode) */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaPresetLowChannel:
            /* set lower side of region integrated for preset counts */
		    pPvt->ptschan = ivalue;;
            break;
        case mcaPresetHighChannel:
            /* set high side of region integrated for preset counts */
		    pPvt->ptechan = ivalue;;
            break;
        case mcaDwellTime:
            /* set dwell time */
            /* This is a NOOP for BrukerQM100 */
            break;
        case mcaPresetRealTime:
            /* set preset real time. Convert to msecs */
            pPvt->preal = (int) (1000. * dvalue);
            break;
        case mcaPresetLiveTime:
            /* set preset live time. Convert to msecs */
            pPvt->plive = (int) (1000. * dvalue);
            break;
        case mcaPresetCounts:
            /* set preset counts */
            pPvt->ptotal = ivalue;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                      "drvMcaBrukerQM100::BrukerQM100Write port %s got illegal command %d\n",
                      pPvt->portName, command);
            break;
    }
    return(asynSuccess);
}


static asynStatus int32Read(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *value)
{
    return(BrukerQM100Read(drvPvt, pasynUser, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    return(BrukerQM100Read(drvPvt, pasynUser, NULL, value));
}

static asynStatus BrukerQM100Read(void *drvPvt, asynUser *pasynUser,
                          epicsInt32 *pivalue, epicsFloat64 *pfvalue)
{
    mcaBrukerQM100Pvt *pPvt = (mcaBrukerQM100Pvt *)drvPvt;
    mcaCommand command = pasynUser->reason;
    asynStatus status=asynSuccess;

    switch (command) {
        case mcaAcquiring:
            *pivalue = pPvt->acquiring;
            break;
        case mcaDwellTime:
            *pfvalue = 0.;
            break;
        case mcaElapsedLiveTime:
            *pfvalue = pPvt->elive/1000.;
            break;
        case mcaElapsedRealTime:
            *pfvalue = pPvt->ereal/1000.;
            break;
        case mcaElapsedCounts:
            *pfvalue = 0;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvMcaBrukerQM100::BrukerQM100Read got illegal command %d\n",
                      command);
            status = asynError;
            break;
    }
    return(status);
}

static asynStatus getBounds(void *drvPvt, asynUser *pasynUser,
                            epicsInt32 *low, epicsInt32 *high)
{
    *low = 0;
    *high = 0;
    return(asynSuccess);
}


static asynStatus int32ArrayRead(void *drvPvt, asynUser *pasynUser,
                                 epicsInt32 *data, size_t maxChans, 
                                 size_t *nactual)
{
    mcaBrukerQM100Pvt *pPvt = (mcaBrukerQM100Pvt *)drvPvt;
    int signal;
	unsigned int *puibuffer;
	int i, status, headersize;	

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
            "drvMcaBrukerQM100::int32ArrayRead entry, signal=%d, maxChans=%d\n", 
            signal, maxChans);

	if (pPvt->clientOpen == 0) openBQM100Client(drvPvt);

	/* while acquiring we use the spectrum already fetched in mcaReadStatus */ 
	if (!pPvt->acquiring){
		if ((status=pReadSpectrum(pPvt->cid, pPvt->spuno)))
			errlogPrintf("(drvMcaBrukerQM100) error(%d) copying spectrum to buffer\n",status);
		if ((status=pGetSpectrum(pPvt->cid, pPvt->spuno,(struct TRTSpectrumHeader *)pPvt->specBuffer,BRUKERQM100_SPECBUFFERSIZE)))
			errlogPrintf("(drvMcaBrukerQM100) error(%d) reading spectrum\n",status);
	}
	headersize = *((int*)(pPvt->specBuffer + 30));	
	pPvt->nchans = *((int*)(pPvt->specBuffer + 42));
	puibuffer = (unsigned int*)(pPvt->specBuffer+headersize);
	if (pPvt->nchans > maxChans ) pPvt->nchans = maxChans;
	*nactual = pPvt->nchans;
	/* tbd: use memcopy */
 	for (i=0; i<pPvt->nchans; ++i) data[i]=puibuffer[i];
 
    return(asynSuccess);
}

static asynStatus int32ArrayWrite(void *drvPvt, asynUser *pasynUser,
                                  epicsInt32 *data, size_t maxChans)
{
    mcaBrukerQM100Pvt *pPvt = (mcaBrukerQM100Pvt *)drvPvt;
    int signal;

    if (maxChans > pPvt->maxChans) maxChans = pPvt->maxChans;

    pasynManager->getAddr(pasynUser, &signal);

    asynPrint(pasynUser, ASYN_TRACE_FLOW,
             "drvMcaBrukerQM100::int32ArrayWrite entry, signal=%d, maxChans=%d\n",
             signal, maxChans);

    /* There seems to be a way to write the BrukerQM100 memory, with SETHOSTMEM,
     * but there is no callable function to do it yet */
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvMcaBrukerQM100::int32ArrayWrite, write to BrukerQM100 not implemented\n");
    return(asynSuccess);
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    int i;
    char *pstring;

    for (i=0; i<MAX_MCA_COMMANDS; i++) {
        pstring = mcaCommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->reason = mcaCommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(mcaCommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
              "drvSweep::drvUserCreate, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvMcaBrukerQM100::drvUserCreate, unknown command=%s\n", drvInfo);
    return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    mcaCommand command = pasynUser->reason;

    *pptypeName = NULL;
    *psize = 0;
    if (pptypeName)
        *pptypeName = epicsStrDup(mcaCommands[command].commandString);
    if (psize) *psize = sizeof(command);
    return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return(asynSuccess);
}


/* asynCommon routines */

/* Report  parameters */
static void BrukerQM100Report(void *drvPvt, FILE *fp, int details)
{
    mcaBrukerQM100Pvt *pPvt = (mcaBrukerQM100Pvt *)drvPvt;

    fprintf(fp, "BrukerQM100 %s: connected on serial port %s\n",
            pPvt->portName, pPvt->serialPort);
    if (details >= 1) {
        fprintf(fp, "              maxChans: %d\n", pPvt->maxChans);
        fprintf(fp, "              nchans:   %d\n", pPvt->nchans);
        fprintf(fp, "              binning:  %d\n", pPvt->binning);
    }
}

/* Connect */
static asynStatus BrukerQM100Connect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Connect */
static asynStatus BrukerQM100Disconnect(void *drvPvt, asynUser *pasynUser)
{
    /* Does nothing for now.  
     * May be used if connection management is implemented */
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


/* Support routines */
static int sendMessage(mcaBrukerQM100Pvt *pPvt, char *output, char *input)
{
	int status;
	char buffer[BRUKERQM100_MESSAGE_SIZE];
	char outbuffer[BRUKERQM100_MESSAGE_SIZE];

	if (input == NULL) input=buffer;

	if (pPvt->clientOpen == 0) openBQM100Client((void *)pPvt);
	strncpy(outbuffer, output, BRUKERQM100_MESSAGE_SIZE);
	outbuffer[BRUKERQM100_MESSAGE_SIZE-2] = '\0' ;
	strcat(outbuffer, "\r");
	if ((status=pSendRCLCommand(pPvt->cid, pPvt->spuno, outbuffer, input, BRUKERQM100_MESSAGE_SIZE)))
				errlogPrintf("drvMcaBrukerQM100::sendMessage: SendRCL %s\n  Error: %d\n", outbuffer, status); 
	input[BRUKERQM100_MESSAGE_SIZE-1] = '\0' ;

	return(status);
}

/* We cannot load the DLL in BrukerQM100Config.
 * Loading the DLL and calling the Bruker API-Functions must
 * be done in the same thread.  
 */
static int openBQM100Client(void *drvPvt)
{
    mcaBrukerQM100Pvt *pPvt = (mcaBrukerQM100Pvt *)drvPvt;
	char *server="Local server";
	char *user="edx";
	char *passwd="edx";
	int status=0;
	char *libname ="RTIfcClient.dll";
	HINSTANCE hDLL;                 /* Handle to DLL */

	/* run this just once */
	if (pPvt->clientOpen) return 1;
	
	hDLL = LoadLibrary(libname);
	if (hDLL == NULL)
	{
		errlogPrintf("(BrukerQM100Config) Unable to load library %s\n",libname); 
		return 1;
	}
	pOpenClient = (LPOpenClient)GetProcAddress(hDLL,"OpenClient");
	if (!pOpenClient)printf("(BrukerQM100Config) Unable to find function OpenClient\n"); 
	pSendRCLCommand = (LPSendComFun)GetProcAddress(hDLL,"SendRCLCommand");
	if (!pSendRCLCommand)printf("(BrukerQM100Config) Unable to find function SendRCLCommand\n"); 
	pReadSpectrum = (LPStRFun)GetProcAddress(hDLL,"ReadSpectrum");
	if (!pReadSpectrum)printf("(BrukerQM100Config) Unable to find function ReadSpectrum\n"); 
	pGetSpectrum = (LPGetSpecFun)GetProcAddress(hDLL,"GetSpectrum");
	if (!pGetSpectrum)printf("(BrukerQM100Config) Unable to find function GetSpectrum\n"); 
	pStartSpectrumMeasurement = (LPStartFun)GetProcAddress(hDLL,"StartSpectrumMeasurement");
	if (!pStartSpectrumMeasurement)errlogPrintf("(roentecServer) Unable to find function StartSpectrumMeasurement\n"); 
	pStartSpectrumLifeTimeMeasurement = (LPStartFun)GetProcAddress(hDLL,"StartSpectrumLifeTimeMeasurement");
	if (!pStartSpectrumLifeTimeMeasurement)errlogPrintf("(roentecServer) Unable to find function StartSpectrumLifeTimeMeasurement\n"); 
	pStartSpectrumCounterMeasurement = (LPStartCntFun)GetProcAddress(hDLL,"StartSpectrumCounterMeasurement");
	if (!pStartSpectrumCounterMeasurement)errlogPrintf("(roentecServer) Unable to find function StartSpectrumCounterMeasurement\n"); 
	pStopSpectrumMeasurement = (LPStRFun)GetProcAddress(hDLL,"StopSpectrumMeasurement");
	if (!pStopSpectrumMeasurement)errlogPrintf("(roentecServer) Unable to find function StopSpectrumMeasurement\n"); 
	
	status = pOpenClient(server,user,passwd,0,1,&pPvt->cid);
	if (status == 0) {
		pPvt->clientOpen = 1;
		errlogPrintf("drvMcaBrukerQM100:openBQM100Client: Successfully opened client \n");
	}
	else
		errlogPrintf("drvMcaBrukerQM100:openBQM100Client: OpenClient: ERROR: %d \n", status);

	return status;
}


/* iocsh functions */

static const iocshArg BrukerQM100ConfigArg0 = { "Asyn port name",iocshArgString};
static const iocshArg BrukerQM100ConfigArg1 = { "username",iocshArgString};
static const iocshArg BrukerQM100ConfigArg2 = { "password",iocshArgString};
static const iocshArg BrukerQM100ConfigArg3 = { "Servername",iocshArgString};
static const iocshArg BrukerQM100ConfigArg4 = { "Spectrometer Unit No",iocshArgInt};
static const iocshArg * const BrukerQM100ConfigArgs[5] = {&BrukerQM100ConfigArg0,
                                                     &BrukerQM100ConfigArg1,
                                                     &BrukerQM100ConfigArg2,
                                                     &BrukerQM100ConfigArg3,
                                                     &BrukerQM100ConfigArg4};
static const iocshFuncDef BrukerQM100ConfigFuncDef = {"BrukerQM100Config",5,BrukerQM100ConfigArgs};
static void BrukerQM100ConfigCallFunc(const iocshArgBuf *args)
{
    BrukerQM100Config(args[0].sval, args[1].sval, args[2].sval, args[3].sval, args[4].ival);
}

void mcaBrukerQM100Register(void)
{
    iocshRegister(&BrukerQM100ConfigFuncDef,BrukerQM100ConfigCallFunc);
}

epicsExportRegistrar(mcaBrukerQM100Register);
 
