//icbTcaServer.cc

//
//  Author: Mark Rivers
//  Date: 12-Oct-2000  Modified from icbDspServer.cc
//
//  This module is an MPF server which receives messages from devIcbMpf.cc and
//  commmunicates with a Canberra 2016 AMP/TCA module.
//
//  It can receive Float64 messages (ao or ai records) or Int32 messages (mbbo,
//  mbbi records).  pm->extra is ICB_READ for ai or mbbi, and ICB_WRITE for ao
//  or mbbo.

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <errlog.h>
#include <iocsh.h>
#include <epicsExport.h>
#include <gpHash.h>
#include <epicsTypes.h>
#include <symTable.h>

#include "Message.h"
#include "Int32Message.h"
#include "Float64Message.h"
#include "mpfType.h"
#include "DevIcbMpf.h"

#include "ndtypes.h"
#include "nmc_sys_defs.h"
#include "icb_user_subs.h"
#include "icb_sys_defs.h"
#include "icb_bus_defs.h"
#include "icbTca.h"

#define LLD     0
#define ULD     0x20
#define SCA_1   0
#define SCA_2   0x40
#define SCA_3   0x80

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=icbTcaDebug) errlogPrintf(f,## v); }
#endif
volatile int icbTcaDebug = 0;
}

static void *icbTcaHash;

typedef struct {
   int  module;
   int  icbAddress;
   int  defined;
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
} ICB_TCA_MODULE;

class icbTcaServer;
extern "C" int icbTcaAddModule(icbTcaServer *picbTcaServer,
                                int module, char *address);

class icbTcaServer {
public:
    void processMessages();
    static icbTcaServer *findModule(const char *serverName);
    MessageServer *pMessageServer;
    static void icbTcaServerTask(icbTcaServer *);
    int writeReg2(ICB_TCA_MODULE *m);
    int writeReg6(ICB_TCA_MODULE *m);
    int writeDiscrim(ICB_TCA_MODULE *m, double percent, int discrim_spec);
    int maxModules;
    ICB_TCA_MODULE *icbTcaModule;
};

extern "C" int icbTcaSetup(const char *serverName, int maxModules, int queueSize)
{
    int s;

    icbTcaServer *p = new icbTcaServer;
    if (icbTcaHash == NULL) gphInitPvt(&icbTcaHash, 256);
    char *temp = (char *)malloc(strlen(serverName)+1);
    strcpy(temp, serverName);
    GPHENTRY *hashEntry = gphAdd(icbTcaHash, temp, NULL);
    hashEntry->userPvt = p;
    p->pMessageServer = new MessageServer(serverName, queueSize);
    p->maxModules = maxModules;
    p->icbTcaModule = (ICB_TCA_MODULE *)
                           calloc(maxModules, sizeof(ICB_TCA_MODULE));

    // Initialize the ICB software in case this has not been done
    s=icb_initialize();
    DEBUG(5, "(icbTcaConfig): icb_initialize=%d\n", s);

    epicsThreadId threadId = epicsThreadCreate(serverName, epicsThreadPriorityMedium, 10000, 
                      (EPICSTHREADFUNC)icbTcaServer::icbTcaServerTask, (void*) p);
    if(threadId == NULL) {
       errlogPrintf("%s icbTcaServer ThreadCreate Failure\n", serverName);
       return(-1);
    }
    return(0);
}

extern "C" int icbTcaConfig(const char *serverName, int module, int enetAddress, int icbAddress)
{
   ICB_TCA_MODULE *m;
   unsigned char csr;
   char address[20];

   icbTcaServer *p = icbTcaServer::findModule(serverName);
   if (p == NULL) {
      printf("icbTcaConfig: can't find icbTcaServer %s\n", serverName);
      return(-1);
   }

   if ((module < 0) || (module >= p->maxModules)) {
      errlogPrintf("icbTcaAddModule: invalid module\n");
      return(ERROR);
   }
   m = &p->icbTcaModule[module];
   if (m->defined) {
      printf("icbTcaAddModule: module %d already defined!\n", module);
      return(ERROR);
   }
   m->defined = 1;
   sprintf(address, "NI%X:%X", enetAddress, icbAddress);
   if (parse_ICB_address(address, &m->module, &m->icbAddress) != OK) {
       errlogPrintf("icbTcaModule: failed to initialize this address: %s\n",
                           address);
       return(ERROR);
   }
   // Initialize the module. Can't turn on green light on this module, that is
   // controlled by TCA select bit. Clear the reset bit.
   csr = ICB_M_CTRL_CLEAR_RESET;
   write_icb(m->module, m->icbAddress, 0, 1, &csr);
   return(OK);
}

icbTcaServer* icbTcaServer::findModule(const char *name)
{
    GPHENTRY *hashEntry = gphFind(icbTcaHash, name, NULL);
    if (hashEntry == NULL) return (NULL);
    return((icbTcaServer *)hashEntry->userPvt);
}

void icbTcaServer::icbTcaServerTask(icbTcaServer *picbTcaServer)
{
   picbTcaServer->processMessages();
}

void icbTcaServer::processMessages()
{
   while(true) {
      pMessageServer->waitForMessage();
      Message *inmsg;
      ICB_TCA_MODULE *m;
      int icbCommand;
      int module, icbAddress;
      int index;
      int ival=0;
      float fval=0.;
      int status=OK;
      unsigned char reg;
      Int32Message *pim = NULL;
      Float64Message *pfm =NULL;

      while((inmsg = pMessageServer->receive())) {
         StandardFieldsMessage *psm = (StandardFieldsMessage *)inmsg;
         icbCommand = psm->cmd;
         index = psm->address;
         if ((index < 0) || (index >= maxModules)) {
            DEBUG(1, "(processMessages): invalid module %d\n", index);
            psm->status = ERROR;
            pMessageServer->reply(psm);
            continue;
         }
         m = &icbTcaModule[index];
         if (!m->defined) {
            DEBUG(1, "(processMessages): module %d is not defined\n", index);
            psm->status = ERROR;
            pMessageServer->reply(psm);
            continue;
         }
         module = m->module;
         icbAddress = m->icbAddress;
         if (psm->extra == ICB_WRITE) {  /* This is a write (output) command */
            if (psm->getType() == messageTypeInt32) {
               // This is an MBBO record, write an integer value
               pim = (Int32Message *)inmsg;
               ival = pim->value;
               DEBUG(3,
                  "(icbTcaServer [%s]): Got Int32Message (mbbo), cmd=%d, value=%d\n",
                  pMessageServer->getName(), icbCommand, ival)
            } else if (psm->getType() == messageTypeFloat64) {
               // This is an AO record
               pfm = (Float64Message *)inmsg;
               fval = pfm->value;
               DEBUG(3,
                  "(icbTcaServer [%s]): Got Float64Message (ao), cmd=%d, value=%f\n",
                  pMessageServer->getName(), icbCommand, fval)
            }
            switch (icbCommand) {
            case ICB_TCA_POLARITY:
               m->polarity = ival;
               writeReg2(m);
               break;
            case ICB_TCA_THRESH:
               m->threshold = ival;
               writeReg2(m);
               break;
            case ICB_TCA_SCAEN:
               m->sca_enable = ival;
               writeReg2(m);
               break;
            case ICB_TCA_GATE1:
               m->gate1 = ival;
               writeReg2(m);
               break;
            case ICB_TCA_GATE2:
               m->gate2 = ival;
               writeReg2(m);
               break;
            case ICB_TCA_GATE3:
               m->gate3 = ival;
               writeReg2(m);
               break;
            case ICB_TCA_SELECT:
               m->select = ival;
               writeReg2(m);
               break;
            case ICB_TCA_PUREN:
               m->pur_enable = ival;
               writeReg2(m);
               break;
            case ICB_TCA_PUR1:
               m->pur1 = ival;
               writeReg6(m);
               break;
            case ICB_TCA_PUR2:
               m->pur2 = ival;
               writeReg6(m);
               break;
            case ICB_TCA_PUR3:
               m->pur3 = ival;
               writeReg6(m);
               break;
            case ICB_TCA_PURAMP:
               m->pur_amp = ival;
               writeReg6(m);
               break;
            case ICB_TCA_LOW1:
               // It appears that it is necessary to write the ULD whenever the LLD
               // changes.  We write both when either changes.
               m->lld[0] = fval;
               writeDiscrim(m, m->lld[0], SCA_1 | LLD);
               writeDiscrim(m, m->uld[0], SCA_1 | ULD);
               break;
            case ICB_TCA_HI1:
               m->uld[0] = fval;
               writeDiscrim(m, m->lld[0], SCA_1 | LLD);
               writeDiscrim(m, m->uld[0], SCA_1 | ULD);
               break;
            case ICB_TCA_LOW2:
               m->lld[1] = fval;
               writeDiscrim(m, m->lld[1], SCA_2 | LLD);
               writeDiscrim(m, m->uld[1], SCA_2 | ULD);
               break;
            case ICB_TCA_HI2:
               m->uld[1] = fval;
               writeDiscrim(m, m->lld[1], SCA_2 | LLD);
               writeDiscrim(m, m->uld[1], SCA_2 | ULD);
               break;
            case ICB_TCA_LOW3:
               m->lld[2] = fval;
               writeDiscrim(m, m->lld[2], SCA_3 | LLD);
               writeDiscrim(m, m->uld[2], SCA_3 | ULD);
               break;
            case ICB_TCA_HI3:
               m->uld[2] = fval;
               writeDiscrim(m, m->lld[2], SCA_3 | LLD);
               writeDiscrim(m, m->uld[2], SCA_3 | ULD);
               break;
            default:
               DEBUG(1, "(icbTcaServer [%s]): invalid command=%d\n",
                  pMessageServer->getName(), icbCommand)
               break;
            } // End switch (icbCommand)
         } // End ICB_WRITE
         else if (psm->extra == ICB_READ) {  /* This is a read (input) command */
            if (psm->getType() == messageTypeInt32) {
               // This is an MBBI record, read an integer value
               pim = (Int32Message *)inmsg;
            } else if (psm->getType() == messageTypeFloat64) {
               // This is an AI record
               pfm = (Float64Message *)inmsg;
            }
            switch (icbCommand) {
            case ICB_TCA_STATUS:
               // Read status of module
               status = read_icb(m->module, m->icbAddress, 0, 1, &reg);
               // It seems like "status" should tell us if the module can communicate
               // but it does not, it returns OK even for non-existant modules
               // Look for data = 0xff instead
               if (reg == 0xff) {       /* Can't communicate */
                  pim->value = 3;
                  status = ERROR;
               } else if (reg & 0x8)     /* module has been reset */
                  pim->value = 2;
               else if (reg & 0x10)    /* module failed self test */
                  pim->value = 1;
               else                    /* module must be ok */
                  pim->value = 0;
               break;
            default:
               DEBUG(1, "(icbTcaServer [%s]): invalid command=%d\n",
                  pMessageServer->getName(), icbCommand)
               break;
            }
         }
         else {
            DEBUG(1,"(icbTcaServer [%s]): invalid command=%d\n",
                  pMessageServer->getName(), icbCommand)
         }
         psm->status = status;
         pMessageServer->reply(psm);
      } // End while(pMessageServer->receive())
   } // End while(true)
}


int icbTcaServer::writeReg2(ICB_TCA_MODULE *m)
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

int icbTcaServer::writeReg6(ICB_TCA_MODULE *m)
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

int icbTcaServer::writeDiscrim(ICB_TCA_MODULE *m, double percent, int discrim_spec)
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
   DEBUG(5, "(icbTcaServer::writeDiscrim): reg[3]=%x, reg[4]=%x\n",
      registers[0], registers[1]);
   return(write_icb(m->module, m->icbAddress, 3, 2, &registers[0]));
}

static const iocshArg icbTcaSetupArg0 = { "Server name",iocshArgString};
static const iocshArg icbTcaSetupArg1 = { "MaxModules",iocshArgInt};
static const iocshArg icbTcaSetupArg2 = { "QueueSize",iocshArgInt};
static const iocshArg * const icbTcaSetupArgs[3] = {&icbTcaSetupArg0,
                                                 &icbTcaSetupArg1,
                                                 &icbTcaSetupArg2};
static const iocshFuncDef icbTcaSetupFuncDef = {"icbTcaSetup",3,icbTcaSetupArgs};
static void icbTcaSetupCallFunc(const iocshArgBuf *args)
{
    icbTcaSetup(args[0].sval, args[1].ival, args[2].ival);
}


static const iocshArg icbTcaConfigArg0 = { "Server name",iocshArgString};
static const iocshArg icbTcaConfigArg1 = { "Module",iocshArgInt};
static const iocshArg icbTcaConfigArg2 = { "Ethernet address",iocshArgInt};
static const iocshArg icbTcaConfigArg3 = { "ICB address",iocshArgInt};
static const iocshArg * const icbTcaConfigArgs[4] = {&icbTcaConfigArg0,
                                                  &icbTcaConfigArg1,
                                                  &icbTcaConfigArg2,
                                                  &icbTcaConfigArg3};
static const iocshFuncDef icbTcaConfigFuncDef = {"icbTcaConfig",4,icbTcaConfigArgs};
static void icbTcaConfigCallFunc(const iocshArgBuf *args)
{
    icbTcaConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

void icbTcaServerRegister(void)
{
    addSymbol("icbTcaDebug", (void *)&icbTcaDebug, epicsInt32T);
    iocshRegister(&icbTcaSetupFuncDef,icbTcaSetupCallFunc);
    iocshRegister(&icbTcaConfigFuncDef,icbTcaConfigCallFunc);
}

epicsExportRegistrar(icbTcaServerRegister);
