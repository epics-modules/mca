// devIcbMpf.cc

/*
    Author: Mark Rivers
    11-Sept-2000
  
    This file provides device support for Canberra ICB modules using MPF.
    ao, ai, mbbo and mbbi records are supported.
    This layer knows nothing about the specific ICB module.  It simply passes
    messages from the record to an MPF server.
    There are presently 2 MPF servers for ICB devices.  icbServer.cc handles the less
    complex ICB devices, including the 9641 HVPS, 9635 ADC, 9615 amplifier, and
    TCA.  icbDspServer.cc handles the 9660 DSP, since this is a complex device.
    There is typically one MPF server running in a system for each category of 
    ICB device being used, although multiple servers could be used to improve 
    performance.
  
    The init_record functions for the ao and mbbo records pass the values to 
    the server only if the PINI field in the record is TRUE.
  
    The INP or OUT field of the record must be type VMEIO with the following
    syntax #Cc Ss @serverName module
  
    c is the MPF server location
    s is the ICB function code to be performed
    serverName is the name of the MPF server
    module is the module index (assigned with icbXXXConfig or icbXXXAddModule)
  
    Modifications:
    .01  24-Oct-2000 MLR   Set severity for error return on ai and mbbi
    .02  12-Feb-2002 MLR   Eliminate connectIO functions, put logic in 
                           ::init_record to download parameters initially.
*/


#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <recGbl.h>

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

#include "Message.h"
#include "Float64Message.h"
#include "Int32Message.h"
#include "DevMpf.h"
#include "DevIcbMpf.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=devIcbMpfDebug) errlogPrintf(f,## v); }
#endif
volatile int devIcbMpfDebug = 0;
}


// Analog output record

class DevAoIcbMpf : public DevMpf
{
public:
   DevAoIcbMpf(dbCommon*,DBLINK*);
   long startIO(dbCommon* pr);
   long completeIO(dbCommon* pr,Message* m);
   virtual void receiveReply(dbCommon* pr, Message* m);
   static long init_record(void*);
private:
   int icbCommand;
   int address;
   int processed;
};

// Create DSET
MAKE_LINCONV_DSET(devAoIcbMpf,DevAoIcbMpf::init_record)
DevAoIcbMpf::DevAoIcbMpf(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    icbCommand=io->signal;
    if (getUserParm()) address = atoi(getUserParm());
    else address = 0;
    processed = 0;
    return;
}

long DevAoIcbMpf::startIO(dbCommon* pr)
{
    Float64Message *pm = new Float64Message;
    aoRecord* pao = (aoRecord*)pr;

    DEBUG(5,
      "DevAoIcbMpf::startIO record=%s, command=%d, address=%d\n", 
                  pr->name, icbCommand, address);
    pm->address = address;
    pm->cmd = icbCommand;
    pm->value = pao->val;
    pm->extra = ICB_WRITE;
    processed = 1;
    return(sendReply(pm));
}

long DevAoIcbMpf::completeIO(dbCommon* pr,Message* m)
{
    Float64Message *pm = (Float64Message *)m;
    aoRecord* pao = (aoRecord*)pr;
    if (pm->status != 0) {
       DEBUG(2, "DevAoIcbMpf::completeIO, status=%d\n", pm->status);
       recGblSetSevr(pao,WRITE_ALARM,INVALID_ALARM);
   } else {
       pao->rbv = (int)pm->value;
    }
    pao->udf=0;
    delete m;
    return (pm->status);
}

void DevAoIcbMpf::receiveReply(dbCommon* pr, Message* m)
{
    pr->udf = 0;
    delete m;
}


long DevAoIcbMpf::init_record(void* v)
{
    aoRecord* pao = (aoRecord*)v;
    DevAoIcbMpf *pDevAoIcbMpf = new DevAoIcbMpf((dbCommon*)pao,&(pao->out));
    pDevAoIcbMpf->bind();
    DevAoIcbMpf* pobj = (DevAoIcbMpf *)pao->dpvt;
    Float64Message *pm = new Float64Message;
    if (pao->pini) {
       pm->address = pobj->address;
       pm->cmd = pobj->icbCommand;
       pm->value = pao->val;
       pm->extra = ICB_WRITE;
       pobj->send(pm, replyTypeReceiveReply);
    }
    return(0);
}


// Analog input record

class DevAiIcbMpf : public DevMpf
{
public:
   DevAiIcbMpf(dbCommon*,DBLINK*);
   long startIO(dbCommon* pr);
   long completeIO(dbCommon* pr,Message* m);
   static long init_record(void*);
private:
   int icbCommand;
   int address;
};

// Create DSET
MAKE_LINCONV_DSET(devAiIcbMpf,DevAiIcbMpf::init_record)
DevAiIcbMpf::DevAiIcbMpf(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    icbCommand=io->signal;
    if (getUserParm()) address = atoi(getUserParm());
    else address = 0;
    return;
}

long DevAiIcbMpf::startIO(dbCommon* pr)
{
    Float64Message *pm = new Float64Message;

    DEBUG(5,
      "DevAiIcbMpf::startIO record=%s, command=%d, address=%d\n", 
                  pr->name, icbCommand, address);
    pm->address = address;
    pm->cmd = icbCommand;
    pm->extra = ICB_READ;
    return(sendReply(pm));
}

long DevAiIcbMpf::completeIO(dbCommon* pr,Message* m)
{
    aiRecord* pai = (aiRecord*)pr;

    Float64Message *pm = (Float64Message *)m;
    if (pm->status != 0) {
       DEBUG(2, "DevAiIcbMpf::completeIO, status=%d\n", pm->status);
       recGblSetSevr(pai,READ_ALARM,INVALID_ALARM);
    }
    pai->val = pm->value;
    pai->udf=0;
    delete m;
    return (MPF_NoConvert);
}

long DevAiIcbMpf::init_record(void* v)
{
    aiRecord* pai = (aiRecord*)v;
    DevAiIcbMpf *pDevAiIcbMpf = new DevAiIcbMpf((dbCommon*)pai,&(pai->inp));
    pDevAiIcbMpf->bind();
    return(0);
}


// Multi-bit binary output record

class DevMbboIcbMpf : public DevMpf
{
public:
   DevMbboIcbMpf(dbCommon*,DBLINK*);
   long startIO(dbCommon* pr);
   long completeIO(dbCommon* pr,Message* m);
   virtual void receiveReply(dbCommon* pr, Message* m);
   static long init_record(void*);
private:
   int icbCommand;
   int address;
   int processed;
};

// Create DSET
MAKE_LINCONV_DSET(devMbboIcbMpf,DevMbboIcbMpf::init_record)
DevMbboIcbMpf::DevMbboIcbMpf(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    icbCommand=io->signal;
    if (getUserParm()) address = atoi(getUserParm());
    else address = 0;
    processed = 0;
    return;
}

long DevMbboIcbMpf::startIO(dbCommon* pr)
{
    Int32Message *pm = new Int32Message;
    mbboRecord* pmbbo = (mbboRecord*)pr;

    DEBUG(5,
      "DevMbboIcbMpf::startIO record=%s, command=%d, address=%d\n", 
                  pr->name, icbCommand, address);
    pm->address = address;
    pm->cmd = icbCommand;
    pm->value = pmbbo->rval;
    pm->extra = ICB_WRITE;
    processed = 1;
    return(sendReply(pm));
}

long DevMbboIcbMpf::completeIO(dbCommon* pr,Message* m)
{
    Int32Message *pm = (Int32Message *)m;
    mbboRecord* pmbbo = (mbboRecord*)pr;
    if (pm->status != 0) {
       DEBUG(2, "DevMbboIcbMpf::completeIO, status=%d\n", pm->status);
       recGblSetSevr(pmbbo,WRITE_ALARM,INVALID_ALARM);
    } else {
       pmbbo->rbv = pm->value;
    }
    pmbbo->udf=0;
    delete m;
    return (pm->status);
}

void DevMbboIcbMpf::receiveReply(dbCommon* pr, Message* m)
{
    pr->udf = 0;
    delete m;
}

long DevMbboIcbMpf::init_record(void* v)
{
    mbboRecord* pmbbo = (mbboRecord*)v;
    DevMbboIcbMpf *pDevMbboIcbMpf = new DevMbboIcbMpf((dbCommon*)pmbbo,&(pmbbo->out));
    pDevMbboIcbMpf->bind();
    DevMbboIcbMpf* pobj = (DevMbboIcbMpf *)pmbbo->dpvt;
    Int32Message *pm = new Int32Message;
    if (pmbbo->pini) {
       pm->address = pobj->address;
       pm->cmd = pobj->icbCommand;
       pm->value = pmbbo->rval;
       pm->extra = ICB_WRITE;
       pobj->send(pm, replyTypeReceiveReply);
    }
    return(0);
}



// Multi-bit binary input record

class DevMbbiIcbMpf : public DevMpf
{
public:
   DevMbbiIcbMpf(dbCommon*,DBLINK*);
   long startIO(dbCommon* pr);
   long completeIO(dbCommon* pr,Message* m);
   static long init_record(void*);
private:
   int icbCommand;
   int address;
};

// Create DSET
MAKE_LINCONV_DSET(devMbbiIcbMpf,DevMbbiIcbMpf::init_record)
DevMbbiIcbMpf::DevMbbiIcbMpf(dbCommon* pr,DBLINK* l) : DevMpf(pr,l,false)
{
    vmeio* io = (vmeio*)&(l->value);
    icbCommand=io->signal;
    if (getUserParm()) address = atoi(getUserParm());
    else address = 0;
    return;
}

long DevMbbiIcbMpf::startIO(dbCommon* pr)
{
    Int32Message *pm = new Int32Message;

    DEBUG(5,
      "DevMbbiIcbMpf::startIO record=%s, command=%d, address=%d\n", 
                  pr->name, icbCommand, address);
    pm->address = address;
    pm->cmd = icbCommand;
    pm->extra = ICB_READ;
    return(sendReply(pm));
}

long DevMbbiIcbMpf::completeIO(dbCommon* pr,Message* m)
{
    mbbiRecord* pmbbi = (mbbiRecord*)pr;

    Int32Message *pm = (Int32Message *)m;
    if (pm->status != 0) {
       DEBUG(2, "DevMbbiIcbMpf::completeIO, status=%d\n", pm->status);
       recGblSetSevr(pmbbi,READ_ALARM,INVALID_ALARM);
    }
    pmbbi->val = pm->value;
    pmbbi->udf=0;
    delete m;
    return (MPF_NoConvert);
}

long DevMbbiIcbMpf::init_record(void* v)
{
    mbbiRecord* pmbbi = (mbbiRecord*)v;
    DevMbbiIcbMpf *pDevMbbiIcbMpf = new DevMbbiIcbMpf((dbCommon*)pmbbi,&(pmbbi->inp));
    pDevMbbiIcbMpf->bind();
    return(0);
}
