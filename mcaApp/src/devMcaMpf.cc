// devMcaMpf.cc

/*
    Author: Mark Rivers
    04-Sept-2000

    This is device support for the MCA record with MPF servers.
    Servers currently supported are ip300SweepServer.cc (Acromag IP330-ADC used
    as a waveform recorder) and mcaAIMServer.cc (Canberra AIM Ethernet MCA).
  
    Modifications:
      24-Nov-2000  MLR  Changed DSET definition to eliminate compiler errors in new G++
      12-Feb-2002  MLR  Removed connectIO; added waitConnect to
                        sendFloat64Message, so that messages can be sent before
                        iocInit is complete.
      23-Feb-2002  MLR  Fixed bug, missing code for MSG_SET_SEQ.
*/


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

extern "C" {
#include "dbAccess.h"
#include "dbDefs.h"
#include "link.h"
#include "errlog.h"
#include "dbCommon.h"
#include "mcaRecord.h"
#include "mca.h"
#include "recSup.h"
}

#include "Message.h"
#include "Float64Message.h"
#include "Int32ArrayMessage.h"
#include "Float64ArrayMessage.h"
#include "DevMpf.h"
#include "DevMcaMpf.h"

#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=devMcaMpfDebug) errlogPrintf(f,## v); }
#endif
volatile int devMcaMpfDebug = 0;
}

class DevMcaMpf : public DevMpf
{
public:
        DevMcaMpf(dbCommon*,DBLINK*);
        long startIO(dbCommon* pr);
        long completeIO(dbCommon* pr,Message* m);
        virtual void receiveReply(dbCommon* pr, Message* m);
        static long init_record(void*);
        static long send_msg(mcaRecord *pmca, unsigned long msg, void *parg);
        static long read_array(mcaRecord *pmca);
private:
        int sendFloat64Message(int cmd, double value);
        int getAcquireStatus(mcaRecord *pmca);
        int readType;
        int channel;
};

// This device support module cannot use the standard MPF macros for creating
// DSETS because we want to define additional functions which those macros do
// not allow.

typedef struct {
        long            number;
        DEVSUPFUN       report;
        DEVSUPFUN       init;
        DEVSUPFUN       init_record;
        DEVSUPFUN       get_ioint_info;
        long            (*send_msg)(mcaRecord *pmca, unsigned long msg, void *parg);
        long            (*read_array)(mcaRecord *pmca);
} MCAMPF_DSET;

extern "C" {
    MCAMPF_DSET devMcaMpf = {
        6,
        NULL,
        NULL,
        DevMcaMpf::init_record,
        NULL,
        DevMcaMpf::send_msg,
        DevMcaMpf::read_array
    };
};


DevMcaMpf::DevMcaMpf(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    channel=io->signal;
    return;
}

long DevMcaMpf::startIO(dbCommon* pr)
{
    Float64Message *pm = new Float64Message;
    mcaRecord* pmca = (mcaRecord*)pr;

    DEBUG(5,"DevMcaMpf::startIO, enter, record=%s, readType=%d, channel=%d\n", 
                  pr->name, readType, channel);
    pm->address = channel;
    switch (readType) {
    case MSG_READ:
       pm->cmd = MSG_READ;
       pm->setClientExtra(MSG_READ);
       break;
    case MSG_GET_ACQ_STATUS:
       pm->cmd = MSG_GET_ACQ_STATUS;
       pm->setClientExtra(MSG_GET_ACQ_STATUS);
       break;
    default:
        errlogPrintf("%s ::startIO illegal read type %d\n",
                    pmca->name, readType);
        break;
    }
    
    return(sendReply(pm));
}

long DevMcaMpf::read_array(mcaRecord *pmca)
{
    // This static function is called by the MCA record to read the data.
    // It just sets a flag and then calls DevMpf::read_write.
    // The flag is needed by startIO and completeIO to know if this is a read
    // of data or of acquisition status
    DevMcaMpf* devMcaMpf = (DevMcaMpf *)pmca->dpvt;
    
    DEBUG(5,"DevMcaMpf::read_array, enter, record=%s\n", pmca->name);
    devMcaMpf->readType = MSG_READ;
    return(DevMpf::read_write(pmca));
}

int DevMcaMpf::getAcquireStatus(mcaRecord *pmca)
{
    // This function is to read the acquisition status.
    // It just sets a flag and then calls DevMpf::read_write.
    // The flag is needed by startIO and completeIO to know if this is a read
    // of data or of acquisition status
    
    DEBUG(5,"DevMcaMpf::getAcquireStatus, enter, record=%s\n", pmca->name);
    readType = MSG_GET_ACQ_STATUS;
    return(DevMpf::read_write(pmca));
}

long DevMcaMpf::completeIO(dbCommon* pr,Message* m)
{
    // This function is called when a message comes back with data
    // or acquisition status information
    mcaRecord* pmca = (mcaRecord*)pr;
    int size;
    Float64ArrayMessage *pfm = NULL;
    Int32ArrayMessage *pam = NULL;
    struct devMcaMpfStatus *pstat;

    switch (m->getType()) {
    case messageTypeInt32Array:
       pam = (Int32ArrayMessage *)m;
       DEBUG(5,"DevMcaMpf::completeIO, record=%s, status=%d, clientExtra=%d\n", 
                  pmca->name, pam->status, pam->getClientExtra());
       if(pam->status) {
           recGblSetSevr(pmca,READ_ALARM,INVALID_ALARM);
           delete m;
           return(MPF_NoConvert);
       }
       if(pam->getClientExtra() != MSG_READ) {
           errlogPrintf("%s ::completeIO command %d\n",
                    pmca->name, pam->getClientExtra());
           recGblSetSevr(pmca,READ_ALARM,INVALID_ALARM);
           delete m;
           return(MPF_NoConvert);
       }
       size = MIN(pmca->nuse, pam->getLength());
       memcpy(pmca->bptr, pam->value, size*sizeof(int32));
       pmca->udf=0;
       pmca->nord = size;
       break;

    case messageTypeFloat64Array:
       pfm = (Float64ArrayMessage *)m;
       if(pfm->status) {
           DEBUG(1,"DevMcaMpf::completeIO, record=%s, ERROR status=%d\n", 
              pmca->name, pfm->status);
           recGblSetSevr(pmca,READ_ALARM,INVALID_ALARM);
           delete m;
           return(MPF_NoConvert);
       }
       if(pfm->getClientExtra() != MSG_GET_ACQ_STATUS) {
           errlogPrintf("%s ::completeIO command %d\n",
                    pmca->name, pfm->getClientExtra());
           recGblSetSevr(pmca,READ_ALARM,INVALID_ALARM);
           delete m;
           return(MPF_NoConvert);
       }
       pstat = (struct devMcaMpfStatus *)pfm->value;
       pmca->ertm = pstat->realTime;
       pmca->eltm = pstat->liveTime;
       pmca->dwel = pstat->dwellTime;
       pmca->act =  (int)pstat->totalCounts;
       pmca->acqg = (int)pstat->acquiring;
       DEBUG(5,"DevMcaMpf::completeIO, record=%s, elapsed time=%f, dwell time=%f, acqg=%d\n", 
                  pmca->name, pmca->ertm, pmca->dwel, pmca->acqg);
       break;

    default:
        errlogPrintf("%s ::completeIO illegal message type %d\n",
                    pmca->name,m->getType());
        recGblSetSevr(pmca,READ_ALARM,INVALID_ALARM);
        delete m;
        return(MPF_NoConvert);
    }
    delete m;
    return(MPF_OK);
}

long DevMcaMpf::init_record(void* v)
{
    mcaRecord* pmca = (mcaRecord*)v;
    new DevMcaMpf((dbCommon*)pmca,&(pmca->inp));
    return(0);
}


long DevMcaMpf::send_msg(mcaRecord *pmca, unsigned long msg, void *parg)
{
    DevMcaMpf* devMcaMpf = (DevMcaMpf *)pmca->dpvt;

    switch (msg) {
    case MSG_ACQUIRE:
        /* start acquisition */
        devMcaMpf->sendFloat64Message(MSG_ACQUIRE, 0.);
        DEBUG(5, "DevMcaMpf(send_msg): start acquisition\n");
        break;
    case MSG_READ:
        /* start read operation */
        /* Set the flag which tells the record that the read is not complete */
        pmca->rdng = 1;
        read_array(pmca);
        break;
    case MSG_SET_CHAS_INT:
        /* set channel advance source to internal (timed) */
        devMcaMpf->sendFloat64Message(MSG_SET_CHAS_INT, 0.);
        break;
    case MSG_SET_CHAS_EXT:
        /* set channel advance source to external */
        devMcaMpf->sendFloat64Message(MSG_SET_CHAS_EXT, 0.);
        break;
    case MSG_SET_NCHAN:
        /* set number of channels */
        devMcaMpf->sendFloat64Message(MSG_SET_NCHAN, (double)pmca->nuse);
        DEBUG(5, "DevMcaMpf(send_msg): setting number of channels\n");
        break;
    case MSG_SET_DWELL:
        /* set dwell time per channel. */
        devMcaMpf->sendFloat64Message(MSG_SET_DWELL, (double)pmca->dwel);
        DEBUG(5, "DevMcaMpf(send_msg): setting dwell time\n");
        break;
    case MSG_SET_REAL_TIME:
        /* set preset real time. */
        devMcaMpf->sendFloat64Message(MSG_SET_REAL_TIME, (double)pmca->prtm);
        break;
    case MSG_SET_LIVE_TIME:
        /* set preset live time */
        devMcaMpf->sendFloat64Message(MSG_SET_LIVE_TIME, (double)pmca->pltm);
        break;
    case MSG_SET_COUNTS:
        /* set preset counts */
        devMcaMpf->sendFloat64Message(MSG_SET_COUNTS, (double)pmca->pct);
        break;
    case MSG_SET_LO_CHAN:
        /* set preset count low channel */
        devMcaMpf->sendFloat64Message(MSG_SET_LO_CHAN, (double)pmca->pctl);
        break;
    case MSG_SET_HI_CHAN:
        /* set preset count high channel */
        devMcaMpf->sendFloat64Message(MSG_SET_HI_CHAN, (double)pmca->pcth);
        break;
    case MSG_SET_NSWEEPS:
        /* set number of sweeps (for MCS mode) */
        devMcaMpf->sendFloat64Message(MSG_SET_NSWEEPS, (double)pmca->pswp);
        break;
    case MSG_SET_MODE_PHA:
        /* set mode to pulse height analysis */
        devMcaMpf->sendFloat64Message(MSG_SET_MODE_PHA, 0.);
        break;
    case MSG_SET_MODE_MCS:
        /* set mode to MultiChannel Scaler */
        devMcaMpf->sendFloat64Message(MSG_SET_MODE_MCS, 0.);
        break;
    case MSG_SET_MODE_LIST:
        /* set mode to LIST (record each incoming event) */
        devMcaMpf->sendFloat64Message(MSG_SET_MODE_LIST, 0.);
        break;
    case MSG_GET_ACQ_STATUS:
        /* Read the current status of the device */
        /* Set the flag which tells the record that the read status is not
           complete */
        pmca->rdns = 1;
        devMcaMpf->getAcquireStatus(pmca);
        break;
    case MSG_STOP_ACQUISITION:
        /* stop data acquisition */
        devMcaMpf->sendFloat64Message(MSG_STOP_ACQUISITION, 0.);
        DEBUG(5, "DevMcaMpf(send_msg): stop acquisition\n");
        break;
    case MSG_ERASE:
        /* erase */
        devMcaMpf->sendFloat64Message(MSG_ERASE, 0.);
        DEBUG(5, "DevMcaMpf(send_msg): erase\n");
        break;
    case MSG_SET_SEQ:
        /* set sequence number */
        devMcaMpf->sendFloat64Message(MSG_SET_SEQ, (double)pmca->seq);
        break;
    case MSG_SET_PSCL:
        /* set channel advance prescaler. */
        devMcaMpf->sendFloat64Message(MSG_SET_PSCL, (double)pmca->pscl);
        break;
    }
    return(0);
}


int DevMcaMpf::sendFloat64Message(int cmd, double value)
{
    Float64Message *pfm = new Float64Message;
    
    // Wait up to 30 seconds to connect, since server may not yet be running
    if (!connectWait(30.)) {
       DEBUG(1, "(sendFloat64Message): connectWait failed!\n");
       return(-1);
    }
    pfm->cmd = cmd;
    pfm->value = value;
    pfm->address = channel;
    return(send(pfm, replyTypeReceiveReply));
}

void DevMcaMpf::receiveReply(dbCommon* pr, Message* m)
{
    mcaRecord* pmca = (mcaRecord*)pr;
    DEBUG(5, "%s DevMcaMpf:receiveReply enter\n", pmca->name);
    delete m;
}

