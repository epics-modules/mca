//mcaAIMServer.cc

/*
    Author: Mark Rivers
    Date: 04-Sept-2000  Modified from ip330Server.cc

    This module is an MPF server which receives messages from devMcaMpf.cc and
    commmunicates with a Canberra AIM module.
  
    21-Jan-2001  MLR  Fixed bug when erasing a single spectrum with signal != 0, 
                      it was only erasing the first 25% of the memory.
    01-Feb-2003  jee  Changes for Release 3.14.and Linux port
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsTime.h>

#include <errlog.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <epicsThread.h>

#include "Message.h"
#include "Int32Message.h"
#include "Float64Message.h"
#include "Int32ArrayMessage.h"
#include "Float64ArrayMessage.h"
#include "mpfType.h"
#include "mca.h"
#include "DevMcaMpf.h"
#include "nmc_sys_defs.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=mcaAIMServerDebug) errlogPrintf(f,## v); }
#endif
volatile int mcaAIMServerDebug = 0;
}


class mcaAIMServer {
public:
    int sendAIMSetup();
    void processMessages();
    MessageServer *pMessageServer;
    static void mcaAIMServerTask(mcaAIMServer *);
    int module;
    int adc;
    int base_address;
    int seq_address;
    int maxChans;
    int maxSignals;
    int maxSequences;
    int nchans;
    int plive;
    int preal;
    int ptotal;
    int ptschan;
    int ptechan;
    int elive;
    int ereal;
    int etotals;
    int acqmod;
    int exists;
    epicsTime *statusTime;
    double maxStatusTime;
    int acquiring;
};


extern "C" int AIMSetup(int maxPorts)
{
    errlogPrintf("AIMSetup is obsolete, no longer required.\n");
    return (-1);
}
extern "C" int AIMConfig(
    const char *serverName, int address, int port, int maxChans,
    int maxSignals, int maxSequences, char *ethernetDevice, int queueSize)
{
    unsigned char enet_address[6];
    int s;

    DEBUG(8,"(AIMConfig): ethernetDevice=%s\n", ethernetDevice);
    DEBUG(8,"(AIMConfig): serverName=%s\n", serverName);
    DEBUG(8,"(AIMConfig): address=%d, QueuSize=%d\n",address,queueSize);

    mcaAIMServer *p = new mcaAIMServer;
    p->pMessageServer = new MessageServer(serverName, queueSize);

    /* Initialize the AIM software */
    DEBUG(5, "(AIMConfig): ethernetDevice=%s\n", ethernetDevice);
    s=nmc_initialize(ethernetDevice);
    DEBUG(5, "(AIMConfig): nmc_initialize=%d\n", s);

    /* Copy parameters to object private */
    p->exists = 1;
    p->maxChans = maxChans;
    p->nchans = maxChans;
    p->maxSignals = maxSignals;
    p->maxSequences = maxSequences;
    /* Copy the module ADC port number. Subtract 1 to convert from */
    /* user units to device units  */
    p->adc = port - 1;
    /* Set reasonable defaults for other parameters */
    p->plive   = 0;
    p->preal   = 0;
    p->ptotal  = 0;
    p->ptschan = 0;
    p->ptechan = 0;
    p->acqmod  = 1;
    p->acquiring = 0;
    p->statusTime = new epicsTime;
    /* Maximum time to use old status info before a new query is sent. Only used 
     * if signal!=0, typically with multiplexors. 0.1 seconds  */
    p->maxStatusTime = 0.1;
    /* Compute the module Ethernet address */
    nmc_build_enet_addr(address, enet_address);

    /* Find the particular module on the net */
    s = nmc_findmod_by_addr((int *)&p->module, enet_address);
    if (s != OK) {
        errlogPrintf("AIMConfig: ERROR: cannot find module on the network!\n");
        return (ERROR);
    }
    DEBUG(5, "(AIMConfig): nmc_findmod_by_addr=%d\n", s);
    DEBUG(5, "(AIMConfig):   module=%d\n", p->module);

    /* Buy the module (make this IOC own it) */
    s = nmc_buymodule(p->module, 0);
    if (s != OK) {
        errlogPrintf("AIMConfig: ERROR: cannot buy module - someone else owns it!\n");
        return (ERROR);
    }
    DEBUG(5, "(AIMConfig): nmc_buymodule=%d\n", s);

    /* Allocate memory in the AIM, get base address */
    s = nmc_allocate_memory(p->module,
                    p->maxChans * p->maxSignals * p->maxSequences * 4,
                    (int *)&p->base_address);
    if (s != OK) {
        errlogPrintf("AIMConfig: ERROR: cannot allocate required memory in AIM\n");
        return (ERROR);
    }
    p->seq_address = p->base_address;
    DEBUG(5, "(AIMConfig): nmc_allocate_memory=%d\n", s);

    if (epicsThreadCreate(serverName, epicsThreadPriorityMedium, 10000, 
		 	 (EPICSTHREADFUNC)mcaAIMServer::mcaAIMServerTask, 
                         (void*) p) == NULL) {
       errlogPrintf("%s mcaAIMServer ThreadCreate Failure\n", serverName);
       return(-1);
    }
    return(0);
}

void mcaAIMServer::mcaAIMServerTask(mcaAIMServer *pmcaAIMServer)
{
   pmcaAIMServer->processMessages();
}

void mcaAIMServer::processMessages()
{
    DEBUG(3,"(mcaAIMServer): processMessages starting \n");
    while(true) {
        pMessageServer->waitForMessage();
        Message *inmsg;
        int cmd;
        int len;
        struct devMcaMpfStatus *pstat;
        int signal, address, seq;
        int s;
		epicsTime *pnow = new epicsTime;

        while((inmsg = pMessageServer->receive())) {
            Float64Message *pim = (Float64Message *)inmsg;
            Int32ArrayMessage *pi32m = NULL;
            Float64ArrayMessage *pf64m = NULL;
            pim->status = 0;
            cmd = pim->cmd;
            signal = pim->address;
            DEBUG(3, 
               "(mcaAIMServer [%s signal=%d]): Got Message, cmd=%d, value=%f\n", 
                           pMessageServer->getName(), signal, cmd, pim->value)
            switch (cmd) {
               case MSG_ACQUIRE:
                  /* Start acquisition. */
                  s = nmc_acqu_setstate(module, adc, 1);
                  DEBUG(2,
                     "(mcaAIMServer [%s signal=%d]): start acquisition status=%d\n", 
                     pMessageServer->getName(), signal, s);
                  break;
               case MSG_READ:
                  pi32m = (Int32ArrayMessage *)pMessageServer->
                           allocReplyMessage(pim, messageTypeInt32Array);
                  pi32m->allocValue(nchans);
                  pi32m->setLength(nchans);
                  address = seq_address + maxChans*signal*4;
                  DEBUG(8, "(mcaAIMServer): base_address=%x\n", base_address);
                  DEBUG(8, "(mcaAIMServer): seq_address=%x\n",  seq_address);
                  DEBUG(8, "(mcaAIMServer): signal=%x\n", signal);
                  DEBUG(8, "(mcaAIMServer): address=%x\n", address);
                  s = nmc_acqu_getmemory_cmp(module, adc, address, 1, 1, 1, 
                                             nchans, pi32m->value);
                  DEBUG(2, 
                     "(mcaAIMServer [%s signal=%d]): read %d chans, status=%d\n", 
                           pMessageServer->getName(), signal, nchans, s);
                  delete pim;
                  pMessageServer->reply(pi32m);
                  break;
               case MSG_SET_CHAS_INT:
                  /* set channel advance source to internal (timed) */
                  /* This is a NOOP for current MCS hardware - done manually */
                  break;
               case MSG_SET_CHAS_EXT:
                  /* set channel advance source to external */
                  /* This is a NOOP for current MCS hardware - done manually */
                  break;
               case MSG_SET_NCHAN:
                  /* set number of channels */
                  nchans = (int)pim->value;
                  if (nchans > maxChans) {
                     errlogPrintf("mcaAIMServer Illegal nuse field");
                     nchans = maxChans;
                  }
                  s = sendAIMSetup();
                  break;
               case MSG_SET_DWELL:
                  /* set dwell time */
                  /* This is a NOOP for current MCS hardware - done manually */
                  break;
               case MSG_SET_REAL_TIME:
                  /* set preset real time. Convert to centiseconds */
                  preal = (int) (100. * pim->value);
                  s = sendAIMSetup();
                  break;
               case MSG_SET_LIVE_TIME:
                  /* set preset live time. Convert to centiseconds */
                  plive = (int) (100. * pim->value);
                  s = sendAIMSetup();
                  break;
               case MSG_SET_COUNTS:
                  /* set preset counts */
                  ptotal = (int)pim->value;
                  s = sendAIMSetup();
                  break;
               case MSG_SET_LO_CHAN:
                  /* set lower side of region integrated for preset counts */
                  /* The preset total start channel is in bytes from start of
                     AIM memory */
                  ptschan = seq_address + maxChans*signal*4 + (int)pim->value*4;
                  s = sendAIMSetup();
                  break;
               case MSG_SET_HI_CHAN:
                  /* set high side of region integrated for preset counts */
                  /* The preset total end channel is in bytes from start of
                     AIM memory */
                  ptechan = seq_address + maxChans*signal*4 + (int)pim->value*4;
                  s = sendAIMSetup();
                  break;
               case MSG_SET_NSWEEPS:
                  /* set number of sweeps (for MCS mode) */
                  /* This is a NOOP on current version of MCS */
                  break;
               case MSG_SET_MODE_PHA:
                  /* set mode to Pulse Height Analysis */
                  acqmod = 1;
                  s = sendAIMSetup();
                  break;
               case MSG_SET_MODE_MCS:
                  /* set mode to MultiChannel Scaler */
                  acqmod = 1;
                  s = sendAIMSetup();
                  break;
               case MSG_SET_MODE_LIST:
                  /* set mode to LIST (record each incoming event) */
                  acqmod = 3;
                  s = sendAIMSetup();
                  break;
               case MSG_GET_ACQ_STATUS:
                  /* The status will be the same for each signal on a port.
                   * We optimize by not reading the status if this is not
                   * signal 0 and if the cached status is relatively recent */
                  pf64m = (Float64ArrayMessage *)pMessageServer->
                                allocReplyMessage(pim, messageTypeFloat64Array);
                  len = sizeof(struct devMcaMpfStatus) / sizeof(double);
                  pf64m->allocValue(len);
                  pf64m->setLength(len);
                  pstat = (struct devMcaMpfStatus *)pf64m->value;

                 /* Read the current status of the device if signal 0 or
                   * if the existing status info is too old */
                  if ((signal == 0) || 
                      ((pnow->getCurrent() - maxStatusTime) > *statusTime)) {
                     s = nmc_acqu_statusupdate(module, adc, 0, 0, 0,
                              &elive, &ereal, &etotals, &acquiring);
                     DEBUG(2, 
                        "(mcaAIMServer [%s signal=%d]): get_acq_status=%d\n", 
                              pMessageServer->getName(), signal, s);
                     statusTime->getCurrent();
                  }
                  pstat->realTime = ereal/100.0;
                  pstat->liveTime = elive/100.0;
                  pstat->dwellTime = 0.;
                  pstat->acquiring = acquiring;
                  pstat->totalCounts = etotals;
                  DEBUG(2, "(mcaAIMServer [%s signal=%d]): acquiring=%d\n", 
                              pMessageServer->getName(), signal, acquiring);
                  delete pim;
                  pMessageServer->reply(pf64m);
                  break;
               case MSG_STOP_ACQUISITION:
                  /* stop data acquisition */
                  s = nmc_acqu_setstate(module, adc, 0);
                  DEBUG(2, "(mcaAIMServer [%s signal=%d]): stop,  status=%d\n", 
                           pMessageServer->getName(), signal, s);
                  break;
               case MSG_ERASE:
                  /* Erase. If this is signal 0 then erase memory for all signals.
                   * Databases take advantage of this for performance with
                   * multielement detectors with multiplexors */
                  if (signal == 0) 
                     len = maxChans*maxSignals*4;
                  else
                     len = nchans*4;
                  /* If AIM is acquiring, turn if off */
                  if (acquiring)
                     s = nmc_acqu_setstate(module, adc, 0);
                  address = seq_address + maxChans*signal*4;
                  s = nmc_acqu_erase(module, address, len);
                  DEBUG(2, 
                     "(mcaAIMServer [%s signal=%d]): erased %d chans, status=%d\n", 
                     pMessageServer->getName(), signal, len, s);
                  /* Set the elapsed live and real time back to zero. */
                  s = nmc_acqu_setelapsed(module, adc, 0, 0);
                  /* If AIM was acquiring, turn it back on */
                  if (acquiring)
                     s = nmc_acqu_setstate(module, adc, 1);
                  break;
               case MSG_SET_SEQ:
                  /* set sequence number */
                  seq = (int)pim->value;
                  if (seq > maxSequences) {
                     errlogPrintf("mcaAIMServer: Illegal seq field\n");
                     seq = maxSequences;
                  }
                  seq_address = base_address + maxChans * maxSignals * seq * 4;
                  s = sendAIMSetup();
                  break;
               case MSG_SET_PSCL:
                  // No-op for AIM
                  break;
               default:
                  errlogPrintf("%s mcaAIMServer got illegal command %d\n",
                     pMessageServer->getName(), pim->cmd);
                  break;
            }
            if (cmd != MSG_READ && cmd != MSG_GET_ACQ_STATUS)
               pMessageServer->reply(pim);
        }
    }
}
/* Support routines */
int mcaAIMServer::sendAIMSetup()
{
   int s;
   s = nmc_acqu_setup(module, adc, seq_address, maxSignals*maxChans,
                      plive, preal, ptotal, ptschan, ptechan, acqmod);
   DEBUG(5, "(sendAIMSetup): status=%d\n", s);
   return(s);
}

static const iocshArg AIMConfigArg0 = { "Server name",iocshArgString};
static const iocshArg AIMConfigArg1 = { "Address",iocshArgInt};
static const iocshArg AIMConfigArg2 = { "Port",iocshArgInt};
static const iocshArg AIMConfigArg3 = { "MaxChan",iocshArgInt};
static const iocshArg AIMConfigArg4 = { "MaxSign",iocshArgInt};
static const iocshArg AIMConfigArg5 = { "MaxSeq",iocshArgInt};
static const iocshArg AIMConfigArg6 = { "Eth. dev",iocshArgString};
static const iocshArg AIMConfigArg7 = { "QueSiz",iocshArgInt};
static const iocshArg * const AIMConfigArgs[8] = {&AIMConfigArg0,
                                                  &AIMConfigArg1,
                                                  &AIMConfigArg2,
                                                  &AIMConfigArg3,
                                                  &AIMConfigArg4,
                                                  &AIMConfigArg5,
                                                  &AIMConfigArg6,
                                                  &AIMConfigArg7};
static const iocshFuncDef AIMConfigFuncDef = {"AIMConfig",8,AIMConfigArgs};
static void AIMConfigCallFunc(const iocshArgBuf *args)
{
    AIMConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival,
              args[4].ival, args[5].ival, args[6].sval, args[7].ival);
}

void mcaAIMRegister(void)
{
    iocshRegister(&AIMConfigFuncDef,AIMConfigCallFunc);
}

epicsExportRegistrar(mcaAIMRegister);
 

