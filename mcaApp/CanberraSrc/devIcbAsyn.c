/*  devIcbAsyn.c

    Author: Mark Rivers
    23-June-2004
  
    This file provides device support for Canberra ICB modules using asyn.
    ao, ai, mbbo and mbbi records are supported.
    This layer knows nothing about the specific ICB module.  It simply passes
    information from the record to an asyn driver.
    There is typically one asyn port in a system, although multiple ports 
    can be used to improve performance.
  
    The INP or OUT field of the record must be type VMEIO with the following
    syntax #Cc Ss @serverName module
  
    c is the ICB module type
      0=ADC
      1=Amplifier
      2=HVPS
      3=TCA
      4=DSP
    s is the ICB function code to be performed
    portName is the name of the asyn port (created with icbSetup)
    module is the module index (assigned with icbConfig)
  
    Modifications:
    .01  23-Jun-2004 MLR   Created from previous file devIcbMpf.cc
*/


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <recGbl.h>
#include <link.h>

#include "dbAccess.h"
#include "dbDefs.h"
#include "link.h"
#include "errlog.h"
#include "alarm.h"
#include "dbCommon.h"
#include "aoRecord.h"
#include "aiRecord.h"
#include "mbboRecord.h"
#include "mbbiRecord.h"
#include "recSup.h"
#include "devSup.h"

#include <epicsExport.h>
#include <epicsString.h>
#include <cantProceed.h>
#include <asynDriver.h>
#include <asynEpicsUtils.h>

#include "devIcbAsyn.h"

typedef enum {recTypeAi, recTypeAo, recTypeMbbi, recTypeMbbo} recType;

typedef struct devIcbPvt {
    asynUser   *pasynUser;
    asynCommon *pasynCommon;
    void       *commonPvt;
    asynIcb    *pasynIcb;
    void       *icbPvt;
    recType    recType;
    icbModuleType icbModuleType;
    int        icbCommand;
    int        address;
    int        ivalue;
    double     dvalue;
    asynStatus status;
} devIcbPvt;

typedef struct dsetIcb {
    long       number;
    DEVSUPFUN  report;
    DEVSUPFUN  init;
    DEVSUPFUN  init_record;
    DEVSUPFUN  get_ioint_info;
    DEVSUPFUN  io;
    DEVSUPFUN  convert;
} dsetIcb;

static long initCommon(dbCommon *pr, DBLINK *plink, recType rt);
static long startIOCommon(dbCommon *pr);
static long completeIOCommon(dbCommon *pr);
static void devIcbCallback(asynUser *pasynUser);
static long icbConvert(dbCommon *pr, int pass);

static long initAi(aiRecord *pr);
static long readAi(aiRecord *pr);
static long initAo(aoRecord *pr);
static long writeAo(aoRecord *pr);
static long initMbbi(mbbiRecord *pr);
static long readMbbi(mbbiRecord *pr);
static long initMbbo(mbboRecord *pr);
static long writeMbbo(mbboRecord *pr);

dsetIcb devAiIcbAsyn = {6, 0, 0, initAi, 0, readAi, icbConvert};
dsetIcb devAoIcbAsyn = {6, 0, 0, initAo, 0, writeAo, icbConvert};
dsetIcb devMbbiIcbAsyn = {6, 0, 0, initMbbi, 0, readMbbi, 0};
dsetIcb devMbboIcbAsyn = {6, 0, 0, initMbbo, 0, writeMbbo, 0};

epicsExportAddress(dset, devAiIcbAsyn);
epicsExportAddress(dset, devAoIcbAsyn);
epicsExportAddress(dset, devMbbiIcbAsyn);
epicsExportAddress(dset, devMbboIcbAsyn);


static long initCommon(dbCommon *pr, DBLINK *plink, recType rt)
{
    int card;
    char *port, *userParam;
    devIcbPvt *pPvt;
    asynUser *pasynUser=NULL;
    asynInterface *pasynInterface;
    asynStatus status;
    struct vmeio *pvmeio;
    char temp[100], *p;
    int i;

    pPvt = callocMustSucceed(1, sizeof(devIcbPvt), "devIcbAsyn::init_common");
    pr->dpvt = pPvt;
    pPvt->recType = rt;
    
    pasynUser = pasynManager->createAsynUser(devIcbCallback, 0);
    pPvt->pasynUser = pasynUser;
    pasynUser->userPvt = pr;

/* We can't use this yet, because the database still uses the CARD field of
 * VMEIO
    status = pasynEpicsUtils->parseLink(pasynUser, plink, 
                                        &port, &pPvt->icbCommand,
                                        &userParam);
    if (status != asynSuccess) {
        errlogPrintf("devIcbAsyn::initCommon %s bad link %s\n", 
                     pr->name, pasynUser->errorMessage);
        goto bad;
    }
*/

/* Parse VME link here for now */
    pvmeio=(struct vmeio*)&(plink->value);
    pPvt->icbCommand = pvmeio->signal;
    card = pvmeio->card;
    p = pvmeio->parm;
    /* first field of parm is always the asyn port name */
    for(i=0; *p && *p!=',' && *p!=' '; i++, p++) temp[i]=*p;
    temp[i]='\0';
    port = epicsStrDup(temp);
    userParam = epicsStrDup(++p);

    pPvt->icbModuleType = card;
    pPvt->address = atoi(userParam);
    status = pasynManager->connectDevice(pasynUser, port, pPvt->address);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devIcbAsyn::icbConfig, error in connectDevice %s\n",
                  pasynUser->errorMessage);
        goto bad;
    }
    pasynInterface = pasynManager->findInterface(pasynUser, asynCommonType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devIcbAsyn::icbConfig, cannot find common interface %s\n",
                  pasynUser->errorMessage);
        goto bad;
    }
    pPvt->pasynCommon = (asynCommon *)pasynInterface->pinterface;
    pPvt->commonPvt = pasynInterface->drvPvt;
    pasynInterface = pasynManager->findInterface(pasynUser, asynIcbType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devIcbAsyn::icbConfig, cannot find icb interface %s\n",
                  pasynUser->errorMessage);
        goto bad;
    }
    pPvt->pasynIcb = (asynIcb *)pasynInterface->pinterface;
    pPvt->icbPvt = pasynInterface->drvPvt;
    if ((rt == recTypeAi) || (rt == recTypeAo))
       return(2); /* Do not convert */
    else
       return(0);
bad:
    pr->pact = 1;
    return(-1);
}


static long initAi(aiRecord *pr)
{
    return(initCommon((dbCommon *)pr, &pr->inp, recTypeAi));
}
static long initAo(aoRecord *pr)
{
    return(initCommon((dbCommon *)pr, &pr->out, recTypeAo));
}
static long initMbbi(mbbiRecord *pr)
{
    return(initCommon((dbCommon *)pr, &pr->inp, recTypeMbbi));
}
static long initMbbo(mbboRecord *pr)
{
    return(initCommon((dbCommon *)pr, &pr->out, recTypeMbbo));
}
static long readAi(aiRecord *pr)
{
    if (!pr->pact)
        return(startIOCommon((dbCommon *)pr));
    else
        return(completeIOCommon((dbCommon *)pr));
}
static long readMbbi(mbbiRecord *pr)
{
    if (!pr->pact)
        return(startIOCommon((dbCommon *)pr));
    else
        return(completeIOCommon((dbCommon *)pr));
}
static long writeAo(aoRecord *pr)
{
    devIcbPvt *pPvt = (devIcbPvt *)pr->dpvt;

    if (!pr->pact) {
        pPvt->dvalue = pr->val;
        return(startIOCommon((dbCommon *)pr));
    } else
        return(completeIOCommon((dbCommon *)pr));
}
static long writeMbbo(mbboRecord *pr)
{
    devIcbPvt *pPvt = (devIcbPvt *)pr->dpvt;

    if (!pr->pact) {
        pPvt->ivalue = pr->rval;
        return(startIOCommon((dbCommon *)pr));
    } else
        return(completeIOCommon((dbCommon *)pr));
}


static long startIOCommon(dbCommon *pr)
{
    devIcbPvt *pPvt = (devIcbPvt *)pr->dpvt;
    int status;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
              "devIcbAsyn::startIOCommon, record=%s\n", pr->name);
    pr->pact = 1;
    status = pasynManager->queueRequest(pPvt->pasynUser, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pPvt->pasynUser, ASYN_TRACE_ERROR,
                  "devIcbAsyn::startIOCommon, error calling queueRequest %s\n",
                  pPvt->pasynUser->errorMessage);
        status = -1;
    }
    return(status);
}


static void devIcbCallback(asynUser *pasynUser)
{
    dbCommon *pr = (dbCommon *)pasynUser->userPvt;
    devIcbPvt *pPvt = (devIcbPvt *)pr->dpvt;
    rset *prset = pr->rset;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
              "devIcbAsyn::devIcbCallback, record=%s, module type=%d, "
              "command=%d, ivalue=%d, dvalue=%f\n", 
              pr->name, pPvt->icbModuleType, pPvt->icbCommand, 
              pPvt->ivalue, pPvt->dvalue);
    switch(pPvt->recType) {
        case recTypeAi:
        case recTypeMbbi:
            pPvt->status = pPvt->pasynIcb->read(pPvt->icbPvt, pasynUser, 
                                                pPvt->icbModuleType, 
                                                pPvt->icbCommand, 
                                                &pPvt->ivalue, &pPvt->dvalue);
            break;
        case recTypeAo:
        case recTypeMbbo:
            pPvt->status = pPvt->pasynIcb->write(pPvt->icbPvt, pasynUser, 
                                                 pPvt->icbModuleType, 
                                                 pPvt->icbCommand,
                                                 pPvt->ivalue, pPvt->dvalue);
            break;
    }
    /* Process the record.  This will result in the readXi or writeXi routine
     * being called again, but with pact=1 */
    dbScanLock(pr);
    (*prset->process)(pr);
    dbScanUnlock(pr);
}


static long completeIOCommon(dbCommon *pr)
{
    devIcbPvt *pPvt = (devIcbPvt *)pr->dpvt;
    int rc=0;

    asynPrint(pPvt->pasynUser, ASYN_TRACE_FLOW, 
              "devIcbAsyn::completeIOCommon, record=%s\n", pr->name);
    switch(pPvt->recType) {
        case recTypeAi: {
            aiRecord *pai = (aiRecord *)pr;
            if (pPvt->status == asynSuccess) {
                pai->val = pPvt->dvalue;
                rc = 2;
            } 
            }
            break;
        case recTypeMbbi: {
            mbbiRecord *pmbbi = (mbbiRecord *)pr;
            if (pPvt->status == asynSuccess) {
                pmbbi->val = pPvt->ivalue;
                rc = 2;
            }
            }
            break;
        case recTypeAo: {
            aoRecord *pao = (aoRecord *)pr;
            if (pPvt->status == asynSuccess) {
                pao->rbv = (int)pPvt->dvalue;
                rc = 0;
            }
            }
            break;
        case recTypeMbbo: {
            mbboRecord *pmbbo = (mbboRecord *)pr;
            if (pPvt->status == asynSuccess) {
                pmbbo->rbv = pPvt->ivalue;
                rc = 0;
            } 
            }
            break;
    }

    if (pPvt->status == asynSuccess)
        pr->udf = 0;
    else
        recGblSetSevr(pr, COMM_ALARM, INVALID_ALARM);
    return(rc);
}
     
static long icbConvert(dbCommon* pr, int pass)
{
   return 0;
}
