/* drvIcbAsyn.c

    Author: Mark Rivers
    Date: 23-June-2004 Modified from icbMpfServer.cc
  
    This module is an asyn driver which is called from from asyn generic
    device support and commmunicates with the following Canberra ICB modules:
      9633/9635 ADC
      9615      Amplifier
      9641/9621 HVPS
                TCA
      9660      DSP
  
*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <errlog.h>

#include <iocsh.h>
#include <epicsExport.h>
#include <asynDriver.h>
#include <asynUInt32Digital.h>
#include <asynFloat64.h>
#include <asynDrvUser.h>
#include <cantProceed.h>
#include <epicsString.h>

#include "drvIcbAsyn.h"
#include "icbDsp.h"

#include "ndtypes.h"
#include "nmc_sys_defs.h"
#include "icb_user_subs.h"
#include "icb_sys_defs.h"
#include "icb_bus_defs.h"
#include "campardef.h"

typedef struct {
    int command;
    char *commandString;
} icbCommandStruct;

static icbCommandStruct icbAdcCommands[MAX_ICB_ADC_COMMANDS] = {
    {icbAdcGain,     "GAIN"},
    {icbAdcRange,    "RANGE"},
    {icbAdcOffset,   "OFFSET"},
    {icbAdcLld,      "LLD"},
    {icbAdcUld,      "ULD"},
    {icbAdcZero,     "ZERO"},
    {icbAdcGmod,     "GATE_MODE"},
    {icbAdcCmod,     "COINCIDENCE_MODE"},
    {icbAdcPmod,     "PEAK_MODE"},
    {icbAdcAmod,     "ACQUISITION_MODE"},
    {icbAdcTmod,     "TRANSFER_MODE"},
    {icbAdcZeroRbv,  "ZERO_READBACK"}
};
 
static icbCommandStruct icbAmpCommands[MAX_ICB_AMP_COMMANDS] = {
    {icbAmpCgain,    "COARSE_GAIN"},
    {icbAmpFgain,    "FINE_GAIN"},
    {icbAmpSfgain,   "SUPERFINE_GAIN"},
    {icbAmpInpp,     "INPUT_POLARITY"},
    {icbAmpInhp,     "INHIBIT_POLARITY"},
    {icbAmpDmod,     "DIFFERENTIAL_MODE"},
    {icbAmpSmod,     "SHAPING_MODE"},
    {icbAmpPtyp,     "PREAMP_TYPE"},
    {icbAmpPurmod,   "PUR_MODE"},
    {icbAmpBlmod,    "BASELINE_MODE"},
    {icbAmpDtmod,    "DEADTIME_MODE"},
    {icbAmpAutoPz,   "START_PZ"},
    {icbAmpPz,       "REQUESTED_PZ"},
    {icbAmpShaping,  "SHAPING"},
    {icbAmpPzRbv,    "PZ_READBACK"}
};

static icbCommandStruct icbHvpsCommands[MAX_ICB_HVPS_COMMANDS] = {
    {icbHvpsVolt,    "VOLT"},
    {icbHvpsVlim,    "LIMIT"},
    {icbHvpsInhl,    "INHIBIT_LEVEL"},
    {icbHvpsLati,    "LATCH_INHIBIT"},
    {icbHvpsLato,    "LATCH_OVERLOAD"},
    {icbHvpsReset,   "RESET"},
    {icbHvpsStatus,  "STATUS"},
    {icbHvpsFramp,   "FAST_RAMP"},
    {icbHvpsVpol,    "POLARITY"},
    {icbHvpsInh,     "INHIBIT"},
    {icbHvpsOvl,     "OVERLOAD"},
    {icbHvpsStatRbv, "STATUS_READBACK"},
    {icbHvpsBusy,    "BUSY"},
    {icbHvpsVoltRbv, "VOLT_READBACK"}
};

static icbCommandStruct icbTcaCommands[MAX_ICB_TCA_COMMANDS] = {
    {icbTcaPolarity, "POLARITY"},
    {icbTcaThresh,   "THRESHOLD"},
    {icbTcaScaEn,    "SCA_ENABLE"},
    {icbTcaGate1,    "GATE1"},
    {icbTcaGate2,    "GATE2"},
    {icbTcaGate3,    "GATE3"},
    {icbTcaSelect,   "TCA_SELECT"},
    {icbTcaPurEn,    "PUR_ENABLE"},
    {icbTcaPur1,     "PUR1"},
    {icbTcaPur2,     "PUR2"},
    {icbTcaPur3,     "PUR3"},
    {icbTcaPurAmp,   "PUR_AMP"},
    {icbTcaLow1,     "LOW1"},
    {icbTcaHi1,      "HI1"},
    {icbTcaLow2,     "LOW2"},
    {icbTcaHi2,      "HI2"},
    {icbTcaLow3,     "LOW3"},
    {icbTcaHi3,      "HIGH3"},
    {icbTcaStatus,   "STATUS"}
};

#define MAX_ICB_DSP_COMMANDS 75
static icbCommandStruct icbDspCommands[MAX_ICB_DSP_COMMANDS] = {
    {ID_AMP_CGAIN,    "AMP_CGAIN"},
    {ID_AMP_FGAIN,    "AMP_FGAIN"},
    {ID_AMP_SFGAIN,   "AMP_SFGAIN"},
    {ID_ADC_CGAIN,    "ADC_CGAIN"},
    {ID_ADC_CRANGE,   "ADC_CRANGE"},
    {ID_ADC_OFFS,     "ADC_OFFS"},
    {ID_ADC_TYPE,     "ADC_TYPE"},
    {ID_ADC_LLD,      "ADC_LLD"},
    {ID_ADC_ZERO,     "ADC_ZERO"},
    {ID_FILTER_RT,    "FILTER_RT"},
    {ID_FILTER_FT,    "FILTER_FT"},
    {ID_FILTER_MDEX,  "FILTER_MDEX"},
    {ID_FILTER_MFACT, "FILTER_MFACT"},
    {ID_FILTER_BLRM,  "FILTER_BLRM"},
    {ID_FILTER_PZM,   "FILTER_PZM"},
    {ID_FILTER_PZ,    "FILTER_PZ"},
    {ID_FILTER_THR,   "FILTER_THR"},
    {ID_FILTER_FRQ,   "FILTER_FRQ"},
    {ID_STABLZ_GCOR,  "STABLZ_GCOR"},
    {ID_STABLZ_ZCOR,  "STABLZ_ZCOR"},
    {ID_STABLZ_GMOD,  "STABLZ_GMOD"},
    {ID_STABLZ_ZMOD,  "STABLZ_ZMOD"},
    {ID_STABLZ_GDIV,  "STABLZ_GDIV"},
    {ID_STABLZ_ZDIV,  "STABLZ_ZDIV"},
    {ID_STABLZ_GSPAC, "STABLZ_GSPAC"},
    {ID_STABLZ_ZSPAC, "STABLZ_ZSPAC"},
    {ID_STABLZ_GWIN,  "STABLZ_GWIN"},
    {ID_STABLZ_ZWIN,  "STABLZ_ZWIN"},
    {ID_STABLZ_GCENT, "STABLZ_GCENT"},
    {ID_STABLZ_ZCENT, "STABLZ_ZCENT"},
    {ID_STABLZ_GRAT,  "STABLZ_GRAT"},
    {ID_STABLZ_ZRAT,  "STABLZ_ZRAT"},
    {ID_STABLZ_RESET, "STABLZ_RESET"},
    {ID_STABLZ_GAIN,  "STABLZ_GAIN"},
    {ID_STABLZ_ZERO,  "STABLZ_ZERO"},
    {ID_MISC_FDM,     "MISC_FDM"},
    {ID_MISC_FD,      "MISC_FD"},
    {ID_MISC_INPP,    "MISC_INPP"},
    {ID_MISC_INHP,    "MISC_INHP"},
    {ID_MISC_PURM,    "MISC_PURM"},
    {ID_MISC_GATM,    "MISC_GATM"},
    {ID_MISC_OUTM,    "MISC_OUTM"},
    {ID_MISC_GD,      "MISC_GD"},
    {ID_MISC_TINH,    "MISC_TINH"},
    {ID_MISC_LTRIM,   "MISC_LTRIM"},
    {ID_MISC_BBRN,    "MISC_BBRN"},
    {ID_INFO_TDAC,    "INFO_TDAC"},
    {ID_INFO_THRP,    "INFO_THRP"},
    {ID_INFO_THRI,    "INFO_THRI"},
    {ID_INFO_PZ,      "INFO_PZ"},
    {ID_INFO_BDC,     "INFO_BDC"},
    {ID_STATUS_FLGS,  "STATUS_FLGS"},
    {ID_SYSOP_CMD,    "SYSOP_CMD"},
    {ID_SYSOP_RDATL,  "SYSOP_RDATL"},
    {ID_SYSOP_RDATH,  "SYSOP_RDATH"},
    {ID_SYSOP_LCD,    "SYSOP_LCD"},
    {ID_SYSOP_DAC,    "SYSOP_DAC"},
    {ID_START_PZ,     "START_PZ"},
    {ID_START_BDC,    "START_BDC"},
    {ID_CLEAR_ERRORS, "CLEAR_ERRORS"},
    {ID_STABLZ_GOVR,  "STABLZ_GOVR"},
    {ID_STABLZ_GOVF,  "STABLZ_GOVF"},
    {ID_STABLZ_ZOVR,  "STABLZ_ZOVR"},
    {ID_STABLZ_ZOVF,  "STABLZ_ZOVF"},
    {ID_INFO_PZAER,   "INFO_PZAER"},
    {ID_INFO_BDCAER,  "INFO_BDCAER"},
    {ID_STATUS_PZBSY, "STATUS_PZBSY"},
    {ID_STATUS_BDBSY,  "STATUS_BDBSY"},
    {ID_STATUS_MINT,   "STATUS_MINT"},
    {ID_STATUS_DGBSY,  "STATUS_DGBSY"},
    {ID_STATUS_MERR,   "STATUS_MERR"},
    {ID_INFO_DT,       "INFO_DT"},
    {ID_INFO_ICR,      "INFO_ICR"},
    {ID_INFO_DSPRATE,  "INFO_DSPRATE"},
    {ID_INFO_MCARATE,  "INFO_MCARATE"}
};

#define LLD     0
#define ULD     0x20
#define SCA_1   0
#define SCA_2   0x40
#define SCA_3   0x80

typedef struct {
   int  module;
   int  icbAddress;
   int  polarity;
   int  threshold;
   int  sca_enable;
   int  gate1;
   int  gate2;
   int  gate3;
   int  select;
   int  pur_enable;
   int  pur1;
   int  pur2;
   int  pur3;
   int  pur_amp;
   float lld[3];
   float uld[3];
} icbTcaModule;

typedef struct {
   int  module;
   int  icbAddress;
   int  defined;
   int  stablz_gmod;
   int  stablz_zmod;
   int  info_pz;
   int  info_bdc;
   int  info_thri;
   int  status_flgs;
} icbDspModule;


typedef enum {
    icbUndefined,
    icbNotFound,
    icbFound
} icbModuleStatus;

typedef struct {
    char address[80];
    icbModuleStatus defined;
    int  index;
    /* Note, we put these structures in for all modules, regardless of type. 
     * Small extra memory, but simpler than using pointers and allocation */
    icbTcaModule tca;
    icbDspModule dsp; 
} icbModule;

typedef struct {
    char           *portName;
    icbModuleType  icbModuleType;
    icbModule      icbModule;
    asynInterface  common;
    asynInterface  uint32Digital;
    asynInterface  float64;
    asynInterface  drvUser;
} drvIcbAsynPvt;


/* These functions are in the public interfaces */    
static asynStatus uint32DigitalWrite(void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 value, epicsUInt32 mask); 
static asynStatus uint32DigitalRead( void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 *value, epicsUInt32 mask); 
static asynStatus float64Write(      void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 value); 
static asynStatus float64Read(       void *drvPvt, asynUser *pasynUser,
                                     epicsFloat64 *value);
static asynStatus drvUserCreate(     void *drvPvt, asynUser *pasynUser,
                                     const char *drvInfo,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserGetType(    void *drvPvt, asynUser *pasynUser,
                                     const char **pptypeName, size_t *psize);
static asynStatus drvUserDestroy(    void *drvPvt, asynUser *pasynUser);
static void icbReport(               void *drvPvt, FILE *fp, int details);
static asynStatus icbConnect(        void *drvPvt, asynUser *pasynUser);
static asynStatus icbDisconnect(     void *drvPvt, asynUser *pasynUser);

/* These functions are private, not in any interface */ 
static asynStatus icbRead(void *drvPvt, asynUser *pasynUser, int icbCommand, 
                          int *ivalue, double *dvalue);
static asynStatus icbReadAdc(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue);
static asynStatus icbReadAmp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue);
static asynStatus icbReadHvps(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue);
static asynStatus icbReadTca(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue);
static asynStatus icbReadDsp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue);
static asynStatus icbWrite(void *drvPvt, asynUser *pasynUser, int icbCommand, 
                           int ivalue, double dvalue);
static asynStatus icbWriteAdc(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue);
static asynStatus icbWriteAmp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue);
static asynStatus icbWriteHvps(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue);
static asynStatus icbWriteTca(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue);
static asynStatus icbWriteDsp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue);
static int writeAdc(icbModule *m, long command, char c, void *addr);
static int writeAmp(icbModule *m, long command, char c, void *addr);
static int writeHvps(icbModule *m, long command, char c, void *addr);
static int readAdc(icbModule *m, long command, char c, void *addr);
static int readAmp(icbModule *m, long command, char c, void *addr);
static int readHvps(icbModule *m, long command, char c, void *addr);
static int tcaWriteReg2(icbTcaModule *m);
static int tcaWriteReg6(icbTcaModule *m);
static int tcaWriteDiscrim(icbTcaModule *m, double percent, int discrim_spec);
static int sendDsp(int module, int icbAddress, int command, 
                   unsigned short data);
static int readDsp(int module, int icbAddress, int command, 
                   unsigned short *data);
static unsigned short dspDoubleToShort(double dval, double dmin, double dmax,
                                       unsigned short imin, 
                                       unsigned short imax);
static double dspShortToDouble(unsigned short ival, double dmin, double dmax,
                               unsigned short imin, unsigned short imax);
static double dspUnpackThroughput(unsigned short ival);

static asynStatus verifyModule(drvIcbAsynPvt *pPvt, asynUser *pasynUser, 
                               icbModule **m);

/* asynCommon interface */
static const struct asynCommon icbCommon = {
    icbReport,
    icbConnect,
    icbDisconnect
};

/* asynUInt32Digital interface */
static const asynUInt32Digital icbUInt32Digital = {
    uint32DigitalWrite,
    uint32DigitalRead
};

/* asynFloat64 interface */
static const asynFloat64 icbFloat64 = {
    float64Write,
    float64Read
};

/* asynDrvUser interface */
static const asynDrvUser icbDrvUser = {
    drvUserCreate,
    drvUserGetType,
    drvUserDestroy
};


int icbConfig(const char *portName, int enetAddress, int icbAddress, 
              icbModuleType icbModuleType)
{
    int status;
    drvIcbAsynPvt *pPvt;
    icbModule *m;
    unsigned char csr;

    pPvt = callocMustSucceed(1, sizeof(drvIcbAsynPvt), "icbSetup"); 
    pPvt->portName = epicsStrDup(portName);
    pPvt->icbModuleType = icbModuleType;
    m = &pPvt->icbModule;
    pPvt->common.interfaceType = asynCommonType;
    pPvt->common.pinterface  = (void *)&icbCommon;
    pPvt->common.drvPvt = pPvt;
    pPvt->uint32Digital.interfaceType = asynUInt32DigitalType;
    pPvt->uint32Digital.pinterface  = (void *)&icbUInt32Digital;
    pPvt->uint32Digital.drvPvt = pPvt;
    pPvt->float64.interfaceType = asynFloat64Type;
    pPvt->float64.pinterface  = (void *)&icbFloat64;
    pPvt->float64.drvPvt = pPvt;
    pPvt->drvUser.interfaceType = asynDrvUserType;
    pPvt->drvUser.pinterface  = (void *)&icbDrvUser;
    pPvt->drvUser.drvPvt = pPvt;
    status = pasynManager->registerPort(pPvt->portName,
                                        ASYN_CANBLOCK,  /* Not MULTIDEVICE */
                                        1,  /* autoconnect */
                                        0,  /* medium priority */
                                        0); /* default stack size */
    if (status != asynSuccess) {
        errlogPrintf("icbConfig ERROR: Can't register common\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->common);
    if (status != asynSuccess) {
        errlogPrintf("icbConfig ERROR: Can't register common.\n");
        return -1;
    }
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->uint32Digital);
    if (status != asynSuccess) {
        errlogPrintf("icbConfig ERROR: Can't register uint32Digital\n");
        return -1;
    }
 
    status = pasynManager->registerInterface(pPvt->portName,&pPvt->float64);
    if (status != asynSuccess) {
        errlogPrintf("icbConfig ERROR: Can't register float64\n");
        return -1;
    }

    status = pasynManager->registerInterface(pPvt->portName,&pPvt->drvUser);
    if (status != asynSuccess) {
        errlogPrintf("icbConfig ERROR: Can't register drvUser\n");
        return -1;
    }
 
    /* Initialize the ICB software in case this has not been done */
    status=icb_initialize();
    if (status != 0) return(-1);
    
    m->defined = icbNotFound;
    sprintf(m->address, "NI%X:%X", enetAddress, icbAddress);

    /* This part is different for different module types */
    switch (pPvt->icbModuleType) {
    case icbAdcType:
    case icbAmpType:
    case icbHvpsType:
        status = icb_findmod_by_address(m->address, &m->index);
        if (status != 0) {
            errlogPrintf("icbConfig: Error looking up ICB module %s\n", 
                         m->address);
            return(-1);
        }
        break;
    case icbTcaType:
        if (parse_ICB_address(m->address, &m->tca.module, 
                              &m->tca.icbAddress) != OK) {
            errlogPrintf("icbConfig: failed to initialize this address: %s\n",
                         m->address);
            return(-1);
        }
        /* Initialize the module. Can't turn on green light on this module, 
         * that is controlled by TCA select bit. Clear the reset bit. */
        csr = ICB_M_CTRL_CLEAR_RESET;
        write_icb(m->tca.module, m->tca.icbAddress, 0, 1, &csr);
        break;
    case icbDspType:
        if (parse_ICB_address(m->address, &m->dsp.module, 
                              &m->dsp.icbAddress) != OK) {
            errlogPrintf("icbConfig: failed to initialize this address: %s\n",
                         m->address);
            return(-1);
        }
        /* Initialize the module (turn on green light) */
        csr = ICB_M_CTRL_LED_GREEN;
        write_icb(m->dsp.module, m->dsp.icbAddress, 0, 1, &csr);
        break;
    default:
        errlogPrintf("icbConfig: unknown module type %d\n", pPvt->icbModuleType);
        return(-1);
    }
    m->defined = icbFound;
    return(0);
}



static asynStatus uint32DigitalWrite(void *drvPvt, asynUser *pasynUser,
                                     epicsUInt32 value, epicsUInt32 mask)
{
    int icbCommand=*(int *)pasynUser->drvUser;

    return(icbWrite(drvPvt, pasynUser, icbCommand, value, 0.));
}

static asynStatus float64Write(void *drvPvt, asynUser *pasynUser,
                               epicsFloat64 value)
{
    int icbCommand=*(int *)pasynUser->drvUser;

    return(icbWrite(drvPvt, pasynUser, icbCommand, 0, value));
}

static asynStatus icbWrite(void *drvPvt, asynUser *pasynUser, int icbCommand, 
                           int ivalue, double dvalue)
{
    drvIcbAsynPvt *pPvt = (drvIcbAsynPvt *)drvPvt;
    icbModule *module;
    int status;

    status = verifyModule(pPvt, pasynUser, &module);
    if (status != asynSuccess) return(status);

    switch (pPvt->icbModuleType) {
    case icbAdcType:
        status=icbWriteAdc(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbAmpType:
        status=icbWriteAmp(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbHvpsType:
        status=icbWriteHvps(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbTcaType:
        status=icbWriteTca(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbDspType:
        status=icbWriteDsp(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    }
    if (status != 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "drvIcbAsyn::icbWrite ERROR [%s]): command=%d\n",
                  pPvt->portName, icbCommand);
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "      dvalue=%f, ivalue=%d, status=%d\n",
                  dvalue, ivalue, status);
        return(asynError);
    }
    return(asynSuccess);
}


static asynStatus icbWriteAdc(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue)
{
    int status;
    char str[80];
    float fvalue=dvalue;

    switch (icbCommand) {
        case icbAdcGain:   /* We set both the ADC gain if either changes */
        case icbAdcRange:
            status = writeAdc(module, CAM_L_CNVGAIN, 0, &ivalue);
            status = writeAdc(module, CAM_L_ADCRANGE, 0, &ivalue);
            break;
        case icbAdcOffset:
            ivalue = (int) dvalue;
            status = writeAdc(module, CAM_L_ADCOFFSET, 0, &ivalue);
            break;
        case icbAdcLld:
            status = writeAdc(module, CAM_F_LLD, 0, &fvalue);
            break;
        case icbAdcUld:
            status = writeAdc(module, CAM_F_ULD, 0, &fvalue);
            break;
        case icbAdcZero:
            status = writeAdc(module, CAM_F_ZERO, 0, &fvalue);
            break;
        case icbAdcGmod:
            status = writeAdc(module, CAM_L_ADCFANTIC, 0, &ivalue);
            break;
        case icbAdcCmod:
            status = writeAdc(module, CAM_L_ADCFLATEC, 0, &ivalue);
            break;
        case icbAdcPmod:
            status = writeAdc(module, CAM_L_ADCFDELPK, 0, &ivalue);
            break;
        case icbAdcAmod:
            if (ivalue) strcpy(str, "SVA");
            else strcpy(str, "PHA");
            status = writeAdc(module, CAM_T_ADCACQMODE, 'S', &str);
            break;
        case icbAdcTmod:
            status = writeAdc(module, CAM_L_ADCFNONOV, 0, &ivalue);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbWrite unknown ADC command=%d\n",
                      icbCommand);
            status = asynError;
    }
    return(status);
}


static asynStatus icbWriteAmp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue)
{
    int status;
    char str[80];
    float fvalue=dvalue;

    switch (icbCommand) {
        case icbAmpCgain:
            fvalue = (float)ivalue;
            /* The value 2.5 will be sent as 2, since MBBO only does integers */
            if (fvalue == 2.0) fvalue = 2.5;
            status = writeAmp(module, CAM_F_AMPHWGAIN1, 0, &fvalue);
            break;
        case icbAmpFgain:
            status = writeAmp(module, CAM_F_AMPHWGAIN2, 0, &fvalue);
            break;
        case icbAmpSfgain:
            status = writeAmp(module, CAM_F_AMPHWGAIN3, 0, &fvalue);
            break;
        case icbAmpInpp:
            status = writeAmp(module, CAM_L_AMPFNEGPOL, 0, &ivalue);
            break;
        case icbAmpInhp:
            status = writeAmp(module, CAM_L_AMPFCOMPINH, 0, &ivalue);
            break;
        case icbAmpDmod:
            status = writeAmp(module, CAM_L_AMPFDIFF, 0, &ivalue);
            break;
        case icbAmpSmod:
            if (ivalue) strcpy(str, "TRIANGLE");
            else strcpy(str, "GAUSSIAN");
            status = writeAmp(module, CAM_T_AMPSHAPEMODE, 'S', &str);
            break;
        case icbAmpPtyp:
            if (ivalue) strcpy(str, "TRP");
            else strcpy(str, "RC");
            status = writeAmp(module, CAM_T_PRAMPTYPE, 'S', &str);
            break;
        case icbAmpPurmod:
            status = writeAmp(module, CAM_L_AMPFPUREJ, 0, &ivalue);
            break;
        case icbAmpBlmod:
            if (ivalue) strcpy(str, "ASYM");
            else strcpy(str, "SYM");
            status = writeAmp(module, CAM_T_AMPBLRTYPE, 'S', &str);
            break;
        case icbAmpDtmod:
            if (ivalue) strcpy(str, "LFC");
            else strcpy(str, "Norm");
            status = writeAmp(module, CAM_T_AMPDTCTYPE, 'S', &str);
            break;
        case icbAmpPz:
            ivalue = (int) fvalue;
            status = writeAmp(module, CAM_L_AMPPZ, 0, &ivalue);
            break;
        case icbAmpAutoPz:
            status = writeAmp(module, CAM_L_AMPFPZSTART, 0, &ivalue);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbWrite unknown AMP command=%d\n",
                      icbCommand);
            status = asynError;
    }
    return(status);
}


static asynStatus icbWriteHvps(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                               int icbCommand, icbModule *module, 
                               int ivalue, double dvalue)
{
    int status;
    float fvalue=dvalue;

    switch (icbCommand) {
        case icbHvpsVolt:
            status = writeHvps(module, CAM_F_VOLTAGE, 0, &fvalue);
            break;
        case icbHvpsVlim:
            status = writeHvps(module, CAM_F_HVPSVOLTLIM, 0, &fvalue);
            break;
        case icbHvpsInhl:
            status = writeHvps(module, CAM_L_HVPSFLVINH, 0, &ivalue);
            break;
        case icbHvpsLati:
            status = writeHvps(module, CAM_L_HVPSFINHLE, 0, &ivalue);
            break;
        case icbHvpsLato:
            status = writeHvps(module, CAM_L_HVPSFOVLE, 0, &ivalue);
            break;
        case icbHvpsStatus:
            status = writeHvps(module, CAM_L_HVPSFSTAT, 0, &ivalue);
            break;
        case icbHvpsReset:
            status = writeHvps(module, CAM_L_HVPSFOVINRES, 0, &ivalue);
            break;
        case icbHvpsFramp:
            status = writeHvps(module, CAM_L_HVPSFASTRAMP, 0, &ivalue);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbWrite unknown HVPS command=%d\n",
                      icbCommand);
            status = asynError;
            break;
    }
    return(status);
}


static asynStatus icbWriteTca(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue)
{
    int status=asynSuccess;
    float fvalue=dvalue;
    icbTcaModule *tca = &module->tca;

    switch (icbCommand) {
        case icbTcaPolarity:
            tca->polarity = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaThresh:
            tca->threshold = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaScaEn:
            tca->sca_enable = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaGate1:
            tca->gate1 = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaGate2:
            tca->gate2 = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaGate3:
            tca->gate3 = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaSelect:
            tca->select = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaPurEn:
            tca->pur_enable = ivalue;
            tcaWriteReg2(tca);
            break;
        case icbTcaPur1:
            tca->pur1 = ivalue;
            tcaWriteReg6(tca);
            break;
        case icbTcaPur2:
            tca->pur2 = ivalue;
            tcaWriteReg6(tca);
            break;
        case icbTcaPur3:
            tca->pur3 = ivalue;
            tcaWriteReg6(tca);
            break;
        case icbTcaPurAmp:
            tca->pur_amp = ivalue;
            tcaWriteReg6(tca);
            break;
        case icbTcaLow1:
            /* It appears that it is necessary to write the ULD whenever the 
             * LLD changes.  We write both when either changes. */
            tca->lld[0] = fvalue;
            tcaWriteDiscrim(tca, tca->lld[0], SCA_1 | LLD);
            tcaWriteDiscrim(tca, tca->uld[0], SCA_1 | ULD);
            break;
        case icbTcaHi1:
            tca->uld[0] = fvalue;
            tcaWriteDiscrim(tca,tca->lld[0], SCA_1 | LLD);
            tcaWriteDiscrim(tca, tca->uld[0], SCA_1 | ULD);
            break;
        case icbTcaLow2:
            tca->lld[1] = fvalue;
            tcaWriteDiscrim(tca, tca->lld[1], SCA_2 | LLD);
            tcaWriteDiscrim(tca, tca->uld[1], SCA_2 | ULD);
            break;
        case icbTcaHi2:
            tca->uld[1] = fvalue;
            tcaWriteDiscrim(tca, tca->lld[1], SCA_2 | LLD);
            tcaWriteDiscrim(tca, tca->uld[1], SCA_2 | ULD);
            break;
        case icbTcaLow3:
            tca->lld[2] = fvalue;
            tcaWriteDiscrim(tca, tca->lld[2], SCA_3 | LLD);
            tcaWriteDiscrim(tca, tca->uld[2], SCA_3 | ULD);
            break;
        case icbTcaHi3:
            tca->uld[2] = fvalue;
            tcaWriteDiscrim(tca, tca->lld[2], SCA_3 | LLD);
            tcaWriteDiscrim(tca, tca->uld[2], SCA_3 | ULD);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbWrite unknown TCA command=%d\n",
                      icbCommand);
            status = asynError;
            break;
    }
    return(status);
}


static asynStatus icbWriteDsp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int ivalue, double dvalue)
{
    int status=asynSuccess;
    icbDspModule *dsp = &module->dsp;
    unsigned short svalue=ivalue;
    int sendCommand=1;

    switch (icbCommand) {
        /* These are the ao records, double values */
        /* Convert from double value to output value (unsigned short) */
        case ID_AMP_FGAIN:
            svalue = dspDoubleToShort(dvalue, 0.4, 1.6, 1024, 4095);
            break;
        case ID_AMP_SFGAIN:
            svalue = dspDoubleToShort(dvalue, 0.0, 0.03, 0, 4095);
            break;
        case ID_ADC_OFFS:
            svalue = dspDoubleToShort(dvalue, 0.0, 16128., 0, 126);
            break;
        case ID_ADC_LLD:
            svalue = dspDoubleToShort(dvalue, 0.0, 100., 0, 32767);
            break;
        case ID_ADC_ZERO:
            svalue = dspDoubleToShort(dvalue, -3.125, 3.125, 0, 6250);
            break;
        case ID_FILTER_RT:
            svalue = dspDoubleToShort(dvalue, 0.4, 28.0, 4, 280);
            break;
        case ID_FILTER_FT:
            svalue = dspDoubleToShort(dvalue, 0.0, 3.0, 0, 30);
            break;
        case ID_FILTER_MFACT:
            svalue = dspDoubleToShort(dvalue, 0.0, 32767, 0, 32767);
            break;
        case ID_FILTER_PZ:
            svalue = dspDoubleToShort(dvalue, 0.0, 4095., 0, 4095);
            break;
        case ID_FILTER_THR:
            svalue = dspDoubleToShort(dvalue, 0.0, 32767., 0, 32767);
            break;
        case ID_STABLZ_GSPAC:
            svalue = dspDoubleToShort(dvalue, 2.0, 512., 2, 512);
            break;
        case ID_STABLZ_ZSPAC:
            svalue = dspDoubleToShort(dvalue, 2.0, 512., 2, 512);
            break;
        case ID_STABLZ_GWIN:
            svalue = dspDoubleToShort(dvalue, 1.0, 128., 1, 128);
            break;
        case ID_STABLZ_ZWIN:
            svalue = dspDoubleToShort(dvalue, 1.0, 128., 1, 128);
            break;
        case ID_STABLZ_GCENT:
            svalue = dspDoubleToShort(dvalue, 10.0, 16376., 10, 16376);
            break;
        case ID_STABLZ_ZCENT:
            svalue = dspDoubleToShort(dvalue, 10.0, 16376., 10, 16376);
            break;
        case ID_STABLZ_GRAT:
            svalue = dspDoubleToShort(dvalue, .01, 100., 1, 10000);
            break;
        case ID_STABLZ_ZRAT:
            svalue = dspDoubleToShort(dvalue, .01, 100., 1, 10000);
            break;
        case ID_MISC_FD:
            svalue = dspDoubleToShort(dvalue, 0.0, 100., 0, 1000);
            break;
        case ID_MISC_LTRIM:
            svalue = dspDoubleToShort(dvalue, 0.0, 1000., 0, 1000);
            break;

        /* These are the mbbo records, integer values */
        /* First we deal with the special cases */
        case ID_ADC_CGAIN:   /* We set both the ADC gain and range */
            status = sendDsp(dsp->module, dsp->icbAddress, ID_ADC_CRANGE, svalue);
            break;
        case ID_START_PZ:   /* Start auto pole-zero if value is non-zero */
            if (svalue != 0)
                status = sendDsp(dsp->module, dsp->icbAddress, ID_SYSOP_CMD, 8);
            sendCommand = 0;
            break;
        case ID_START_BDC:  /* Start auto BDC if value is non-zero */
            if (svalue != 0)
                status = sendDsp(dsp->module, dsp->icbAddress, ID_SYSOP_CMD, 9);
            sendCommand = 0;
            break;
        case ID_INFO_THRI:   /* Remember throughput index */
            dsp->info_thri = svalue;
            break;
        case ID_CLEAR_ERRORS:  /* Clear errors, abort auto-PZ/BDC if value */
                               /* is non-zero */
            if (svalue != 0)
                status = sendDsp(dsp->module, dsp->icbAddress, ID_SYSOP_CMD, 0);
            sendCommand = 0;
            break;
        /* For all of the following commands just send svalue */
        case ID_AMP_CGAIN:
        case ID_FILTER_BLRM:
        case ID_FILTER_PZM:
        case ID_MISC_INPP:
        case ID_MISC_INHP:
        case ID_MISC_PURM:
        case ID_MISC_GATM:
        case ID_MISC_OUTM:
        case ID_MISC_FDM:
        case ID_MISC_BBRN:
        case ID_MISC_GD:
        case ID_ADC_TYPE:
        case ID_STABLZ_GMOD:
        case ID_STABLZ_ZMOD:
        case ID_STABLZ_GCOR:
        case ID_STABLZ_RESET:
        case ID_STABLZ_ZCOR:
        case ID_ADC_CRANGE:
        case ID_STABLZ_GDIV:
        case ID_STABLZ_ZDIV:
        case ID_FILTER_FRQ:
        case ID_INFO_TDAC:
        case ID_FILTER_MDEX:
        case ID_MISC_TINH:
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbWrite unknown DSP command=%d\n",
                      icbCommand);
            status = asynError;
            sendCommand = 0;
            break;
    }
    if (sendCommand) {
        status = sendDsp(dsp->module, dsp->icbAddress, icbCommand, svalue);
    }
    return(status);
}



static asynStatus uint32DigitalRead(void *drvPvt, asynUser *pasynUser,
                                    epicsUInt32 *value, epicsUInt32 mask)
{
    int icbCommand=*(int *)pasynUser->drvUser;

    return(icbRead(drvPvt, pasynUser, icbCommand, value, NULL));
}

static asynStatus float64Read(void *drvPvt, asynUser *pasynUser,
                              epicsFloat64 *value)
{
    int icbCommand=*(int *)pasynUser->drvUser;

    return(icbRead(drvPvt, pasynUser, icbCommand, NULL, value));
}

static asynStatus icbRead(void *drvPvt, asynUser *pasynUser, int icbCommand, 
                          int *ivalue, double *dvalue)
{
    drvIcbAsynPvt *pPvt = (drvIcbAsynPvt *)drvPvt;
    icbModule *module;
    int status;
 
    status = verifyModule(pPvt, pasynUser, &module);
    if (status != asynSuccess) return(status);

    switch (pPvt->icbModuleType) {
    case icbAdcType:
        status=icbReadAdc(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbAmpType:
        status=icbReadAmp(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbHvpsType:
        status=icbReadHvps(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbTcaType:
        status=icbReadTca(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    case icbDspType:
        status=icbReadDsp(pPvt, pasynUser, icbCommand, module, ivalue, dvalue);
        break;
    }
    if (status != 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "drvIcbAsyn::icbRead ERROR [%s]): command=%d\n",
                  pPvt->portName, icbCommand);
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "      dvalue=%f, ivalue=%d, status=%d\n", 
                  *dvalue, *ivalue, status);
        return(asynError);
    }
    return(asynSuccess);
}

static asynStatus icbReadAdc(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue)
{
    float fvalue;
    int status;

    switch (icbCommand) {
        case icbAdcZeroRbv:
            status = readAdc(module, CAM_F_ZERO, 0, &fvalue);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbRead, unknown ADC command=%d\n",
                      icbCommand);
            status = -1;
            break;
    }
    *dvalue = fvalue;
    return(status);
}

static asynStatus icbReadAmp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue)
{
    float fvalue=0.;
    int status;

    switch (icbCommand) {
        case icbAmpShaping:
            status = readAmp(module, CAM_F_AMPTC, 0, &fvalue);
            break;
        case icbAmpPzRbv:
            status = readAmp(module, CAM_L_AMPPZ, 0, ivalue);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbRead, unknown AMP command=%d\n",
                      icbCommand);
            status = -1;
            break;
    }
    *dvalue = fvalue;
    return(status);
}


static asynStatus icbReadHvps(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                              int icbCommand, icbModule *module, 
                              int *ivalue, double *dvalue)
{
    float fvalue=0.;
    int status;

    switch (icbCommand) {
        case icbHvpsVpol:
            status = readHvps(module, CAM_L_HVPSFPOL, 0, ivalue);
            break;
        case icbHvpsInh:
            status = readHvps(module, CAM_L_HVPSFINH, 0, ivalue);
            break;
        case icbHvpsOvl:
            status = readHvps(module, CAM_L_HVPSFOV, 0, ivalue);
            break;
        case icbHvpsStatRbv:
            status = readHvps(module, CAM_L_HVPSFSTAT, 0, ivalue);
            break;
        case icbHvpsBusy:
            status = readHvps(module, CAM_L_HVPSFBUSY, 0, ivalue);
            break;
        case icbHvpsVoltRbv:
            status = readHvps(module, CAM_F_VOLTAGE, 0, &fvalue);
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbRead, unknown HVPS command=%d\n",
                      icbCommand);
            status = -1;
            break;
    }
    *dvalue = fvalue;
    return(status);
}


static asynStatus icbReadTca(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue)
{
    icbTcaModule *tca = &module->tca;
    unsigned char reg;
    int status;

    switch (icbCommand) {
        case icbTcaStatus:
            /* Read status of module */
            status = read_icb(tca->module, tca->icbAddress, 0, 1, &reg);
            /* It seems like "status" should tell us if the module can 
             * communicate but it does not, it returns OK even for non-existant 
             * modules. Look for data = 0xff instead */
            if (reg == 0xff) {       /* Can't communicate */
                *ivalue = 3;
                status = ERROR;
            } else if (reg & 0x8)     /* module has been reset */
                *ivalue = 2;
            else if (reg & 0x10)    /* module failed self test */
                *ivalue = 1;
            else                    /* module must be ok */
                *ivalue = 0;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbRead, unknown TCA command=%d\n",
                      icbCommand);
            status = -1;
            break;
    }
    return(status);
}


static asynStatus icbReadDsp(drvIcbAsynPvt *pPvt, asynUser *pasynUser,
                             int icbCommand, icbModule *module, 
                             int *ivalue, double *dvalue)
{
    icbDspModule *dsp = &module->dsp;
    unsigned short svalue;
    int status=asynSuccess;

    switch (icbCommand) {
        /* These are the mbbi records, return value in *ivalue */
        case ID_STABLZ_GMOD:
           status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
           /* Store value for use by other commands */
           dsp->stablz_gmod = svalue;
           *ivalue = svalue & 0x3;
           break;
        case ID_STABLZ_GOVR:
           /* Use cached value of GMOD */
           if (dsp->stablz_gmod & 0x80) *ivalue=1; else *ivalue=0;
           break;
        case ID_STABLZ_GOVF:
           /* Use cached value of GMOD */
           if (dsp->stablz_gmod & 0x40) *ivalue=1; else *ivalue=0;
           break;
        case ID_STABLZ_ZMOD:
           status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
           /* Store value for use by other commands */
           dsp->stablz_gmod = svalue;
           *ivalue = svalue & 0x3;
           break;
        case ID_STABLZ_ZOVR:
           /* Use cached value of GMOD */
           if (dsp->stablz_zmod & 0x80) *ivalue=1; else *ivalue=0;
           break;
        case ID_STABLZ_ZOVF:
           /* Use cached value of GMOD */
           if (dsp->stablz_zmod & 0x40) *ivalue=1; else *ivalue=0;
           break;
        case ID_INFO_PZAER:
           *ivalue = ((dsp->info_pz & 0x7000) >> 12);
           break;
        case ID_INFO_BDCAER:
           *ivalue = ((dsp->info_bdc & 0x7000) >> 12);
           break;
        case ID_STATUS_PZBSY:
           *ivalue = (dsp->status_flgs & 0x1) != 0;
           break;
        case ID_STATUS_BDBSY:
           *ivalue = (dsp->status_flgs & 0x2) != 0;
           break;
        case ID_STATUS_MINT:
           *ivalue = (dsp->status_flgs & 0x4) != 0;
           break;
        case ID_STATUS_DGBSY:
           *ivalue = (dsp->status_flgs & 0x8) != 0;
           break;
        case ID_STATUS_MERR:
           *ivalue = (dsp->status_flgs & 0x10) != 0;
           break;

        /* These are the ao records, return value in *dvalue*/
        case ID_STATUS_FLGS:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            /* Store value for use by other commands */
            dsp->status_flgs = svalue;
            *dvalue = svalue;
            break;
        case ID_INFO_PZ:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            /* Store value for use by other commands */
            dsp->info_pz = svalue;
            /* Return actual pole zero */
            svalue = svalue & 0xfff;
            *dvalue = dspShortToDouble(svalue, 0.0, 4095., 0, 4095);
            break;
        case ID_FILTER_PZ:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            *dvalue = dspShortToDouble(svalue, 0.0, 4095., 0, 4095);
            break;
        case ID_INFO_BDC:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            /* Store value for use by other commands */
            dsp->info_bdc = svalue;
            /* Return actual flat top */
            svalue = svalue & 0x3f;
            *dvalue = dspShortToDouble(svalue, 0.0, 3.0, 0, 30);
            break;
        case ID_FILTER_FT:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            *dvalue = dspShortToDouble(svalue, 0.0, 3.0, 0, 30);
            break;
        case ID_STABLZ_GAIN:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            *dvalue = svalue;
            break;
        case ID_STABLZ_ZERO:
            status = readDsp(dsp->module, dsp->icbAddress, icbCommand, &svalue);
            *dvalue = svalue;
            break;
        case ID_INFO_THRP:
            status = readDsp(dsp->module, dsp->icbAddress, ID_INFO_THRP, &svalue);
            *dvalue = dspUnpackThroughput(svalue);
            /* Need to do special case of DT divide by 10. */
            if (dsp->info_thri == 0) *dvalue = *dvalue / 10.;
            break;
        default:
            asynPrint(pasynUser, ASYN_TRACE_ERROR,
                      "drvIcbAsyn::icbRead, unknown DSP command=%d\n",
                      icbCommand);
            status = -1;
            break;
    }
    return(status);
}


/* asynDrvUser routines */
static asynStatus drvUserCreate(void *drvPvt, asynUser *pasynUser,
                                const char *drvInfo,
                                const char **pptypeName, size_t *psize)
{
    drvIcbAsynPvt *pPvt = (drvIcbAsynPvt *)drvPvt;
    int i;
    int maxCommands=0;
    icbCommandStruct *pcommands=NULL;
    char *pstring;

    switch (pPvt->icbModuleType) {
    case icbAdcType:
        maxCommands = MAX_ICB_ADC_COMMANDS;
        pcommands = icbAdcCommands;
        break;
    case icbAmpType:
        maxCommands = MAX_ICB_AMP_COMMANDS;
        pcommands = icbAmpCommands;
        break;
    case icbHvpsType:
        maxCommands = MAX_ICB_HVPS_COMMANDS;
        pcommands = icbHvpsCommands;
        break;
    case icbTcaType:
        maxCommands = MAX_ICB_TCA_COMMANDS;
        pcommands = icbTcaCommands;
        break;
    case icbDspType:
        maxCommands = MAX_ICB_DSP_COMMANDS;
        pcommands = icbDspCommands;
        break;
    }
 
    for (i=0; i<maxCommands; i++) {
        pstring = pcommands[i].commandString;
        if (epicsStrCaseCmp(drvInfo, pstring) == 0) {
            pasynUser->drvUser = &pcommands[i].command;
            if (pptypeName) *pptypeName = epicsStrDup(pstring);
            if (psize) *psize = sizeof(pcommands[i].command);
            asynPrint(pasynUser, ASYN_TRACE_FLOW,
                      "drvIcbAsyn:drvUserCreate::drvUserCreate, command=%s\n", pstring);
            return(asynSuccess);
        }
    }
    asynPrint(pasynUser, ASYN_TRACE_ERROR,
              "drvIcbAsyn::drvUserCreate, unknown command=%s\n", drvInfo);
    return(asynError);
}

static asynStatus drvUserGetType(void *drvPvt, asynUser *pasynUser,
                                 const char **pptypeName, size_t *psize)
{
    int *pcommand = pasynUser->drvUser;

    *pptypeName = NULL;
    *psize = 0;
    if (pcommand) {
        if (pptypeName)
            *pptypeName = epicsStrDup("icbCommands");
        if (psize) *psize = sizeof(*pcommand);
    }
    return(asynSuccess);
}

static asynStatus drvUserDestroy(void *drvPvt, asynUser *pasynUser)
{
    return(asynSuccess);
}


/* asynCommon routines */

/* Report  parameters */
static void icbReport(void *drvPvt, FILE *fp, int details)
{
    drvIcbAsynPvt *pPvt = (drvIcbAsynPvt *)drvPvt;
    icbModule *module;

    assert(pPvt);
    fprintf(fp, "ICB port %s:\n", pPvt->portName);
    if (details >= 1) {
        module = &pPvt->icbModule;
        if (module->defined == icbFound) 
            fprintf(fp, "  module address: %s OK\n", module->address);
        else if (module->defined == icbNotFound) 
            fprintf(fp, "  module address: %s NOT FOUND\n", module->address);
    }
}

/* Connect */
static asynStatus icbConnect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionConnect(pasynUser);
    return(asynSuccess);
}

/* Disconnect */
static asynStatus icbDisconnect(void *drvPvt, asynUser *pasynUser)
{
    pasynManager->exceptionDisconnect(pasynUser);
    return(asynSuccess);
}


static asynStatus verifyModule(drvIcbAsynPvt *pPvt, asynUser *pasynUser, 
                               icbModule **m)
{
    *m = &pPvt->icbModule;
    if ((*m)->defined != icbFound) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "drvIcbAsyn::verifyModule module not defined or not found\n");
        return(asynError);
    }
    return(asynSuccess);
}


static int writeAdc(icbModule *m, long command, char c, void *addr)
{
   ICB_PARAM_LIST icb_write_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   return(icb_adc_hdlr(m->index, icb_write_list, ICB_M_HDLR_WRITE));
}

static int writeAmp(icbModule *m, long command, char c, void *addr)
{
   ICB_PARAM_LIST icb_write_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   return(icb_amp_hdlr(m->index, icb_write_list, ICB_M_HDLR_WRITE));
}

static int writeHvps(icbModule *m, long command, char c, void *addr)
{
   ICB_PARAM_LIST icb_write_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   return(icb_hvps_hdlr(m->index, icb_write_list, ICB_M_HDLR_WRITE));
}


/* We need a way to determine if the module is communicating.  icb_xxx_hdlr does
 * not always return an error condition if the module is not available.  Read the
 * CSR and test it for the value 0xff, which is what it has if it is not on the ICB. */
static int readAdc(icbModule *m, long command, char c, void *addr)
{
   int s;
   unsigned char csr;
   ICB_PARAM_LIST icb_read_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   s = icb_adc_hdlr(m->index, icb_read_list, ICB_M_HDLR_READ);
   if (s != OK) return(s);
   icb_input(m->index, 0, 1, &csr);
   if (csr == 0xff) return(ERROR);
   else return(OK);
}

static int readAmp(icbModule *m, long command, char c, void *addr)
{
   int s;
   unsigned char csr;
   ICB_PARAM_LIST icb_read_list[] = {
      {command, c, addr},    /* Parameter              */
      {0,       0, 0}};      /* End of list               */
   s = icb_amp_hdlr(m->index, icb_read_list, ICB_M_HDLR_READ);
   if (s != OK) return(s);
   icb_input(m->index, 0, 1, &csr);
   if (csr == 0xff) return(ERROR);
   else return(OK);
}

static int readHvps(icbModule *m, long command, char c, void *addr)
{
   int s;
   unsigned char csr;
   ICB_PARAM_LIST icb_read_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   s = icb_hvps_hdlr(m->index, icb_read_list, ICB_M_HDLR_READ);
   if (s != OK) return(s);
   icb_input(m->index, 0, 1, &csr);
   if (csr == 0xff) return(ERROR);
   else return(OK);
}


static int tcaWriteReg2(icbTcaModule *m)
{
   unsigned char reg;

   /* build up register 2 */
   reg = 0;
   if (m->select)       reg |= 0x80;
   if (m->gate3)        reg |= 0x40;
   if (m->gate2)        reg |= 0x20;
   if (m->gate1)        reg |= 0x10;
   if (m->sca_enable)   reg |= 0x08;
   if (m->threshold)    reg |= 0x04;
   if (m->pur_enable)   reg |= 0x02;
   if (m->polarity)     reg |= 0x01;
   return(write_icb(m->module, m->icbAddress, 2, 1, &reg));
}

static int tcaWriteReg6(icbTcaModule *m)
{
   unsigned char reg;

   /* build up register 6 */
   reg = 0;
   if (m->pur_amp)   reg |= 0x08;
   if (m->pur3)      reg |= 0x04;
   if (m->pur2)      reg |= 0x02;
   if (m->pur1)      reg |= 0x01;
   return(write_icb(m->module, m->icbAddress, 6, 1, &reg));
}

static int tcaWriteDiscrim(icbTcaModule *m, double percent, int discrim_spec)
{
   ULONG dac;
   unsigned char registers[2];

   if (percent < 0.0) percent = 0.0;
   if (percent > 100.) percent = 100.;

   /* Convert to 12 bit DAC value & load into regs for write */
   dac = (int) (percent * 40.95);
   registers[0] = dac & 0xff;
   registers[1] = ((dac & 0xff00) >> 8) | discrim_spec;

   /* Send new value to the ICB module */
   return(write_icb(m->module, m->icbAddress, 3, 2, &registers[0]));
}


static int sendDsp(int module, int icbAddress, int command, unsigned short data)
{
   unsigned char registers[4];
   unsigned char r6;
   int i;

   registers[0] = 0;  /* Not a read command, use only table 0 for now */
   registers[1] = command;                  /* Parameter opcode */
   registers[2] = (data & 0x0000ff00) >> 8; /* MSB of data */
   registers[3] = data & 0x000000ff;        /* LSB of data */
   write_icb(module, icbAddress, 2, 4, registers);
   /* The following delay seems to be necessary or writes will sometimes
    * timeout below */
   epicsThreadSleep(DELAY_9660);

   /* Read register 6 back.  Return error flag on error.  Wait for WDONE to be
    * set */
   for (i=0; i<MAX_9660_POLLS; i++) {
      read_icb(module, icbAddress, 6, 1, &r6);
      if ((r6 & R6_FAIL)  ||  (r6 & R6_MERR)) {
         return(ERROR);
      }
      if ((r6 & R6_WDONE) && ((r6 & R6_MBUSY) == 0)) {
         return(OK);
      }
      epicsThreadSleep(DELAY_9660);
   }
   return(ERROR);
}


static int readDsp(int module, int icbAddress, int command,
                   unsigned short *data)
{
   unsigned char registers[4];
   unsigned char r6;
   int i;
   int s;

   registers[0] = 8;  /* Read command, use only table 0 for now */
   registers[1] = command;                  /* Parameter opcode */
   registers[2] = 0;
   registers[3] = 0;
   *data = 0;
   s = write_icb(module, icbAddress, 2, 4, registers);
   if (s != OK) {
      return(ERROR);
   }
   /* The following delay seems to be necessary or reads will sometimes
    * timeout below */
   epicsThreadSleep(DELAY_9660);

   /* Read register 6 back.  Return error flag on error.  Wait for RDAV to be
    * set */
   for (i=0; i<MAX_9660_POLLS; i++) {
      s = read_icb(module, icbAddress, 6, 1, &r6);
      if (s != OK) {
         return(ERROR);
      }
      if ((r6 & R6_FAIL)  ||  (r6 & R6_MERR)) {
         return(ERROR);
      }
      if ((r6 & R6_RDAV) && ((r6 & R6_MBUSY) == 0)) {
         goto read_data;
      }
      epicsThreadSleep(DELAY_9660);
   }
   return(ERROR);

read_data:
   read_icb(module, icbAddress, 2, 4, registers);
   *data = registers[3] | (registers[2] << 8) |
           (registers[1] << 16) | ((registers[0] & 0x1f) << 24);
   return(OK);
}


static unsigned short dspDoubleToShort(double dval, double dmin, double dmax,
                                       unsigned short imin, unsigned short imax)
{
   double d=dval;
   unsigned short ival;

   if (d < dmin) d = dmin;
   if (d > dmax) d = dmax;
   ival = (unsigned short) (imin + (imax-imin) * (d - dmin) / 
                                                   (dmax - dmin) + 0.5);
   return(ival);
}

static double dspShortToDouble(unsigned short ival, double dmin, double dmax,
                               unsigned short imin, unsigned short imax)
{
   double dval;
   unsigned short i=ival;

   if (i < imin) i = imin;
   if (i > imax) i = imax;
   dval = dmin + (dmax-dmin) * (i - imin) / (imax - imin);
   return(dval);
}

static double dspUnpackThroughput(unsigned short ival)
{
   int exponent;
   double mantissa;

   exponent = (ival & 0xE000) >> 13;
   mantissa = (ival & 0x1FFF);
   return(mantissa * (2 << exponent));
}


/* iocsh functions */

static const iocshArg icbConfigArg0 = { "Port name",iocshArgString};
static const iocshArg icbConfigArg1 = { "Ethernet address",iocshArgInt};
static const iocshArg icbConfigArg2 = { "ICB address",iocshArgInt};
static const iocshArg icbConfigArg3 = { "ICB module type",iocshArgInt};
static const iocshArg * const icbConfigArgs[4] = {&icbConfigArg0,
                                                  &icbConfigArg1,
                                                  &icbConfigArg2,
                                                  &icbConfigArg3};
static const iocshFuncDef icbConfigFuncDef = {"icbConfig",4,icbConfigArgs};
static void icbConfigCallFunc(const iocshArgBuf *args)
{
    icbConfig(args[0].sval, args[2].ival, 
              args[3].ival, args[4].ival);
}

void icbAsynRegister(void)
{
    iocshRegister(&icbConfigFuncDef,icbConfigCallFunc);
}

epicsExportRegistrar(icbAsynRegister);
