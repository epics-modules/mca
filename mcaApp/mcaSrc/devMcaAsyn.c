/* devMcaAsyn.c

    Author: Mark Rivers
    16-May-2004

    This is device support for the MCA record with asyn drivers.
  
    Modifications:
      16-May-2004  MLR  Created from previous devMcaMpf.cc
*/


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <recGbl.h>
#include <dbAccess.h>
#include <dbDefs.h>
#include <link.h>
#include <errlog.h>
#include <dbCommon.h>
#include <cantProceed.h>
#include <recSup.h>
#include <devSup.h>
#include <alarm.h>
#include <epicsExport.h>
#include <epicsMessageQueue.h>

#include "asynDriver.h"

#include "mcaRecord.h"
#include "mca.h"
#include "asynMca.h"

/* For now we use a fixed-size message queue.  This will be fixed later */

#define MSG_QUEUE_SIZE 100
typedef struct {
    mcaCommand command;
    int ivalue;
    double dvalue;
} mcaAsynMessage;

typedef struct {
    mcaRecord *pmca;
    asynUser *pasynUser;
    epicsMessageQueueId msgQueueId;
    asynMca *pasynMcaInterface;
    void *asynMcaInterfacePvt;
    mcaAsynAcquireStatus acquireStatus;
    int nread;
    int *data;
} mcaAsynPvt;

static long init_record(mcaRecord *pmca);
static long send_msg(mcaRecord *pmca, mcaCommand command, void *parg);
static long read_array(mcaRecord *pmca);
static void asynCallback(asynUser *pasynUser);

typedef struct {
    long            number;
    DEVSUPFUN       report;
    DEVSUPFUN       init;
    DEVSUPFUN       init_record;
    DEVSUPFUN       get_ioint_info;
    long            (*send_msg)(mcaRecord *pmca, mcaCommand command, void *parg);
    long            (*read_array)(mcaRecord *pmca);
} mcaAsynDset;

mcaAsynDset devMcaAsyn = {
    6,
    NULL,
    NULL,
    init_record,
    NULL,
    send_msg,
    read_array
};
epicsExportAddress(dset, devMcaAsyn);


static long init_record(mcaRecord *pmca)
{
    struct vmeio *pvmeio;
    asynUser *pasynUser;
    char *port;
    int signal;
    asynStatus status;
    asynInterface *pasynInterface;
    mcaAsynPvt *pPvt;

    /* Allocate asynMcaPvt private structure */
    pPvt = callocMustSucceed(1, sizeof(mcaAsynPvt), "devMcaAsyn init_record()");
    pPvt->data = callocMustSucceed(pmca->nmax, sizeof(long), 
                                   "devMcaAsyn init_record()");
    /* Create asynUser */
    pasynUser = pasynManager->createAsynUser(asynCallback, 0);
    pasynUser->userPvt = pPvt;
    pPvt->pasynUser = pasynUser;
    pPvt->pmca = pmca;
    pmca->dpvt = pPvt;

    /* Get the VME link field */
    /* Get the signal from the VME signal */
    pvmeio = (struct vmeio*)&(pmca->inp.value);
    signal = pvmeio->signal;
    /* Get the port name from the parm field */
    port = pvmeio->parm;             

    /* Connect to device */
    status = pasynManager->connectDevice(pasynUser, port, signal);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s connectDevice failed to %s\n",
                  pmca->name, port);
        goto bad;
    }

    /* Get the asynMca interface */
    pasynInterface = pasynManager->findInterface(pasynUser, asynMcaType, 1);
    if (!pasynInterface) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s find mca interface failed\n",
                  pmca->name);
        goto bad;
    }
    pPvt->pasynMcaInterface = (asynMca *)pasynInterface->pinterface;
    pPvt->asynMcaInterfacePvt = pasynInterface->drvPvt;

    /* Create EPICS message queue */
    pPvt->msgQueueId = epicsMessageQueueCreate(MSG_QUEUE_SIZE, 
                                               sizeof(mcaAsynMessage));
    if (pPvt->msgQueueId == 0) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR,
                  "devMcaAsyn::init_record, %s message queue create failed\n",
                  pmca->name);
        goto bad;
    }
    return(0);
bad:
    pmca->pact=1;
    return(0);
}


static long send_msg(mcaRecord *pmca, mcaCommand command, void *parg)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pmca->dpvt;
    asynUser *pasynUser = pPvt->pasynUser;
    asynUser *pasynUserCopy;
    mcaAsynMessage msg;
    int status;

    msg.command = command;
    msg.ivalue=0;
    msg.dvalue=0.;

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "devMcaAsyn::send_msg: command=%d, pact=%d, rdns=%d, rdng=%d\n", 
              command, pmca->pact, pmca->rdns, pmca->rdng);

    /* If we are already in COMM_ALARM then this server is not reachable, 
     * return */
    if ((pmca->nsta == COMM_ALARM) || (pmca->stat == COMM_ALARM)) return(-1);

    /* If rdns is true and command=MSG_GET_ACQ_STATUS then this is a second 
     * call from the record to complete */
    if (pmca->rdns && (command == MSG_GET_ACQ_STATUS)) {
        /* This is a second call from record after I/O is complete. 
         * Copy information from private to record */
        pmca->ertm = pPvt->acquireStatus.realTime;
        pmca->eltm = pPvt->acquireStatus.liveTime;
        pmca->dwel = pPvt->acquireStatus.dwellTime;
        pmca->act =  pPvt->acquireStatus.totalCounts;
        pmca->acqg = pPvt->acquireStatus.acquiring;
        asynPrint(pasynUser, ASYN_TRACE_FLOW, 
                  "devMcaAsyn::send_msg, record=%s, elapsed time=%f,"
                  " dwell time=%f, acqg=%d\n", 
                  pmca->name, pmca->ertm, pmca->dwel, pmca->acqg);
        return(0);
    }

    /* Make a copy of asynUser.  This is needed because we can have multiple
     * requests queued.  It will be freed in the callback */
    pasynUserCopy = pasynManager->duplicateAsynUser(pasynUser, asynCallback, 0);

    switch (command) {
    case MSG_ACQUIRE:
        /* start acquisition */
        break;
    case MSG_READ:
        /* start read operation */
        /* Set the flag which tells the record that the read is not complete */
        pmca->rdng = 1;
        pmca->pact = 1;
        break;
    case MSG_SET_CHAS_INT:
        /* set channel advance source to internal (timed) */
        break;
    case MSG_SET_CHAS_EXT:
        /* set channel advance source to external */
        break;
    case MSG_SET_NCHAN:
        /* set number of channels */
        msg.ivalue = pmca->nuse;
        break;
    case MSG_SET_DWELL:
        /* set dwell time per channel. */
        msg.dvalue = pmca->dwel;
        break;
    case MSG_SET_REAL_TIME:
        /* set preset real time. */
        msg.dvalue = pmca->prtm;
        break;
    case MSG_SET_LIVE_TIME:
        /* set preset live time */
        msg.dvalue = pmca->pltm;
        break;
    case MSG_SET_COUNTS:
        /* set preset counts */
        msg.dvalue = pmca->pct;
        break;
    case MSG_SET_LO_CHAN:
        /* set preset count low channel */
        msg.ivalue = pmca->pct;
        break;
    case MSG_SET_HI_CHAN:
        /* set preset count high channel */
        msg.ivalue = pmca->pcth;
        break;
    case MSG_SET_NSWEEPS:
        /* set number of sweeps (for MCS mode) */
        msg.ivalue = pmca->pswp;
        break;
    case MSG_SET_MODE_PHA:
        /* set mode to pulse height analysis */
        break;
    case MSG_SET_MODE_MCS:
        /* set mode to MultiChannel Scaler */
        break;
    case MSG_SET_MODE_LIST:
        /* set mode to LIST (record each incoming event) */
        break;
    case MSG_GET_ACQ_STATUS:
        /* Read the current status of the device */
        /* Set the flag which tells the record that the read status is not
           complete */
        pmca->rdns = 1;
        pmca->pact = 1;
        break;
    case MSG_STOP_ACQUISITION:
        /* stop data acquisition */
        break;
    case MSG_ERASE:
        /* erase */
        break;
    case MSG_SET_SEQ:
        /* set sequence number */
        msg.ivalue = pmca->seq;
        break;
    case MSG_SET_PSCL:
        /* set channel advance prescaler. */
        msg.ivalue = pmca->pscl;
        break;
    }
    /* Put the message on the message queue */
    epicsMessageQueueSend(pPvt->msgQueueId, (void *)&msg, sizeof(msg));
    /* Queue asyn request, so we get a callback when driver is ready */
    status = pasynManager->queueRequest(pasynUserCopy, 0, 0);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devMcaAsyn::send_msg: error calling queueRequest, %s\n", 
                  pasynUser->errorMessage);
        return(-1);
    }
    return(0);
}


void asynCallback(asynUser *pasynUser)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pasynUser->userPvt;
    mcaRecord *pmca = pPvt->pmca;
    mcaAsynMessage msg;
    rset *prset = (rset *)pmca->rset;
    int status;

    /* Read message from queue */
    status = epicsMessageQueueReceive(pPvt->msgQueueId, &msg, sizeof(msg));

    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "devMcaAsyn::asynCallback: command=%d, ivalue=%d, dvalue=%f\n",
              msg.command, msg.ivalue, msg.dvalue);

    /* If we are already in COMM_ALARM then this server is not reachable, 
     * return */
    if ((pmca->nsta == COMM_ALARM) || (pmca->stat == COMM_ALARM)) return;

    switch (msg.command) {
    case MSG_READ:
        /* Read data */
       pPvt->pasynMcaInterface->readData(pPvt->asynMcaInterfacePvt, 
                                         pPvt->pasynUser, 
                                         pmca->nuse, &pPvt->nread, pPvt->data);
       dbScanLock((dbCommon *)pmca);
       (*prset->process)(pmca);
       dbScanUnlock((dbCommon *)pmca);
       break;

    case MSG_GET_ACQ_STATUS:
        /* Read the current status of the device */
       pPvt->pasynMcaInterface->readStatus(pPvt->asynMcaInterfacePvt, 
                                           pPvt->pasynUser, 
                                           &pPvt->acquireStatus);
       dbScanLock((dbCommon *)pmca);
       (*prset->process)(pmca);
       dbScanUnlock((dbCommon *)pmca);     
       break;

    default:
        /* All other commands just call drivers command() function */
        pPvt->pasynMcaInterface->command(pPvt->asynMcaInterfacePvt, 
                                         pPvt->pasynUser,
                                         msg.command, msg.ivalue, msg.dvalue);
        break;
    }
    status = pasynManager->freeAsynUser(pasynUser);
    if (status != asynSuccess) {
        asynPrint(pasynUser, ASYN_TRACE_ERROR, 
                  "devMcaAsyn::asynCallback: error in freeAsynUser\n");
    }
}


static long read_array(mcaRecord *pmca)
{
    mcaAsynPvt *pPvt = (mcaAsynPvt *)pmca->dpvt;
    asynUser *pasynUser = pPvt->pasynUser;

    /* Copy data from private buffer to record */
    memcpy(pmca->bptr, pPvt->data, pPvt->nread*sizeof(long));
    pmca->udf=0;
    pmca->nord = pPvt->nread;
    asynPrint(pasynUser, ASYN_TRACE_FLOW, 
              "devMcaAsyn::read_value, record=%s, nord=%d\n",
              pmca->name, pmca->nord);
    return(0);
}
