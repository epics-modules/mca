// icbServer.cc
//
//  Author: Mark Rivers
//  Date: 4-Oct-2000  Modified from icbDspServer.cc
//
//  This module is an MPF server which receives messages from devIcbMpf.cc and
//  commmunicates with the following Canberra ICB modules:
//    9633/9635 ADC
//    9615      Amplifier
//    9641/9621 HVPS
//  The 9660 DSP module and TCA have their own separate server files because they
//  are quite complex.
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
#include "campardef.h"
#include "icbServerDefs.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=icbServerDebug) errlogPrintf(f,## v); }
#endif
volatile int icbServerDebug = 0;
}
static void* icbHash=NULL;

typedef struct {
   char address[80];
   int  defined;
   int  index;
} ICB_MODULE;


class icbServer {
public:
    void processMessages();
    MessageServer *pMessageServer;
    static void icbServerTask(icbServer *);
    static icbServer *findModule(const char *serverName);
    int writeAdc(ICB_MODULE *m, long command, char c, void *addr);
    int writeAmp(ICB_MODULE *m, long command, char c, void *addr);
    int writeHvps(ICB_MODULE *m, long command, char c, void *addr);
    int readAdc(ICB_MODULE *m, long command, char c, void *addr);
    int readAmp(ICB_MODULE *m, long command, char c, void *addr);
    int readHvps(ICB_MODULE *m, long command, char c, void *addr);
    int maxModules;
    ICB_MODULE *icbModule;
};

extern "C" int icbSetup(const char *serverName, int maxModules, int queueSize)
{
    int s;

    icbServer *p = new icbServer;
    if (icbHash == NULL) gphInitPvt(&icbHash, 256);
    char *temp = (char *)malloc(strlen(serverName)+1);
    strcpy(temp, serverName);
    GPHENTRY *hashEntry = gphAdd(icbHash, temp, NULL);
    hashEntry->userPvt = p;
    p->pMessageServer = new MessageServer(serverName, queueSize);
    p->maxModules = maxModules;
    p->icbModule = (ICB_MODULE *)
                           calloc(maxModules, sizeof(ICB_MODULE));
    // Initialize the ICB software in case this has not been done
    s=icb_initialize();
    DEBUG(5, "(icbConfig): icb_initialize=%d\n", s);

    epicsThreadId threadId = epicsThreadCreate(serverName, epicsThreadPriorityMedium, 10000, 
                      (EPICSTHREADFUNC)icbServer::icbServerTask, (void*) p);
    if(threadId == NULL) {
       errlogPrintf("%s icbServer ThreadCreate Failure\n", serverName);
       return(-1);
    }
    return(0);
}

extern "C" int icbConfig(const char *serverName, int module, int enetAddress, int icbAddress)
{
   ICB_MODULE *m;
   int s;

   icbServer *p = icbServer::findModule(serverName);
   if (p == NULL) {
      printf("icbConfig: can't find icbServer %s\n", serverName);
      return(-1);
   }
   if ((module < 0) || (module >= p->maxModules)) {
      errlogPrintf("icbAddModule: invalid module\n");
      return(-1);
   }
   m = &p->icbModule[module];
   if (m->defined) {
      printf("icbAddModule: module %d already defined!\n", module);
      return(-1);
   }
   m->defined = 1;
   sprintf(m->address, "NI%X:%X", enetAddress, icbAddress);
   s = icb_findmod_by_address(m->address, &m->index);
   if (s != OK) {
      errlogPrintf("(icbConfig): Error looking up ICB module %s\n", m->address);
      return(ERROR);
   }
   return(OK);
}

icbServer* icbServer::findModule(const char *name)
{
    GPHENTRY *hashEntry = gphFind(icbHash, name, NULL);
    if (hashEntry == NULL) return (NULL);
    return((icbServer *)hashEntry->userPvt);
}

void icbServer::icbServerTask(icbServer *picbServer)
{
   picbServer->processMessages();
}

void icbServer::processMessages()
{
   while(true) {
      pMessageServer->waitForMessage();
      Message *inmsg;
      ICB_MODULE *m;
      int index;
      int icbCommand;
      int status=OK;
      int ival=0;
      float fval=0.;
      char str[80];

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
         m = &icbModule[index];
         if (!m->defined) {
            DEBUG(1, "(processMessages): module %d is not defined\n", index);
            psm->status = ERROR;
            pMessageServer->reply(psm);
            continue;
         }
         if (psm->extra == ICB_WRITE) {  /* This is a write (output) command */
            if (psm->getType() == messageTypeInt32) {
               // This is an MBBO record, write an integer value
               Int32Message *pim = (Int32Message *)inmsg;
               ival = pim->value;
               DEBUG(3,
                  "(icbServer [%s]): Got Int32Message (mbbo), cmd=%d, value=%d\n",
                  pMessageServer->getName(), icbCommand, ival)
            } else if (psm->getType() == messageTypeFloat64) {
               // This is an AO record
               Float64Message *pim = (Float64Message *)inmsg;
               fval = pim->value;
               DEBUG(3,
                  "(icbServer [%s]): Got Float64Message (ao), cmd=%d, value=%f\n",
                  pMessageServer->getName(), icbCommand, fval)
            }
            if (icbCommand & ID_ADC) {
               // This is an ADC command
               switch (icbCommand) {
               case ID_ADC_GAIN:   // We set both the ADC gain if either changes
               case ID_ADC_RANGE:
                  status = writeAdc(m, CAM_L_CNVGAIN, 0, &ival);
                  status = writeAdc(m, CAM_L_ADCRANGE, 0, &ival);
                  break;
               case ID_ADC_OFFSET:
                  ival = (int) fval;
                  status = writeAdc(m, CAM_L_ADCOFFSET, 0, &ival);
                  break;
               case ID_ADC_LLD:
                  status = writeAdc(m, CAM_F_LLD, 0, &fval);
                  break;
               case ID_ADC_ULD:
                  status = writeAdc(m, CAM_F_ULD, 0, &fval);
                  break;
               case ID_ADC_ZERO:
                  status = writeAdc(m, CAM_F_ZERO, 0, &fval);
                  break;
               case ID_ADC_GMOD:
                  status = writeAdc(m, CAM_L_ADCFANTIC, 0, &ival);
                  break;
               case ID_ADC_CMOD:
                  status = writeAdc(m, CAM_L_ADCFLATEC, 0, &ival);
                  break;
               case ID_ADC_PMOD:
                  status = writeAdc(m, CAM_L_ADCFDELPK, 0, &ival);
                  break;
               case ID_ADC_AMOD:
                  if (ival) strcpy(str, "SVA");
                  else strcpy(str, "PHA");
                  status = writeAdc(m, CAM_T_ADCACQMODE, 'S', &str);
                  break;
               case ID_ADC_TMOD:
                  status = writeAdc(m, CAM_L_ADCFNONOV, 0, &ival);
                  break;
               default:
                  DEBUG(1, "icbServer (ai record), unknown function=%d\n",
                           icbCommand);
                  status = ERROR;
               }
            }
            else if (icbCommand & ID_AMP) {
               // This is an AMP command
               switch (icbCommand) {
               case ID_AMP_CGAIN:
                  fval = (float)ival;
                  // The value 2.5 will be sent as 2, since MBBO only does integers
                  if (fval == 2.0) fval = 2.5;
                  status = writeAmp(m, CAM_F_AMPHWGAIN1, 0, &fval);
                  break;
               case ID_AMP_FGAIN:
                  status = writeAmp(m, CAM_F_AMPHWGAIN2, 0, &fval);
                  break;
               case ID_AMP_SFGAIN:
                  status = writeAmp(m, CAM_F_AMPHWGAIN3, 0, &fval);
                  break;
               case ID_AMP_INPP:
                  status = writeAmp(m, CAM_L_AMPFNEGPOL, 0, &ival);
                  break;
               case ID_AMP_INHP:
                  status = writeAmp(m, CAM_L_AMPFCOMPINH, 0, &ival);
                  break;
               case ID_AMP_DMOD:
                  status = writeAmp(m, CAM_L_AMPFDIFF, 0, &ival);
                  break;
               case ID_AMP_SMOD:
                  if (ival) strcpy(str, "TRIANGLE");
                  else strcpy(str, "GAUSSIAN");
                  status = writeAmp(m, CAM_T_AMPSHAPEMODE, 'S', &str);
                  break;
               case ID_AMP_PTYP:
                  if (ival) strcpy(str, "TRP");
                  else strcpy(str, "RC");
                  status = writeAmp(m, CAM_T_PRAMPTYPE, 'S', &str);
                  break;
               case ID_AMP_PURMOD:
                  status = writeAmp(m, CAM_L_AMPFPUREJ, 0, &ival);
                  break;
               case ID_AMP_BLMOD:
                  if (ival) strcpy(str, "ASYM");
                  else strcpy(str, "SYM");
                  status = writeAmp(m, CAM_T_AMPBLRTYPE, 'S', &str);
                  break;
               case ID_AMP_DTMOD:
                  if (ival) strcpy(str, "LFC");
                  else strcpy(str, "Norm");
                  status = writeAmp(m, CAM_T_AMPDTCTYPE, 'S', &str);
                  DEBUG(1, "writing DTCTYPE, status=%d\n", status);
                  break;
               case ID_AMP_PZ:
                  ival = (int) fval;
                  status = writeAmp(m, CAM_L_AMPPZ, 0, &ival);
                  break;
               case ID_AMP_AUTO_PZ:
                  status = writeAmp(m, CAM_L_AMPFPZSTART, 0, &ival);
                  break;
               default:
                  DEBUG(1, "icbServer (ai record), unknown function=%d\n",
                           icbCommand);
                  status = ERROR;
               }
            }
            else if (icbCommand & ID_HVPS) {
               // This is a HVPS command
               switch (icbCommand) {
               case ID_HVPS_VOLT:
                  status = writeHvps(m, CAM_F_VOLTAGE, 0, &fval);
                  break;
               case ID_HVPS_VLIM:
                  status = writeHvps(m, CAM_F_HVPSVOLTLIM, 0, &fval);
                  break;
               case ID_HVPS_INHL:
                  status = writeHvps(m, CAM_L_HVPSFLVINH, 0, &ival);
                  break;
               case ID_HVPS_LATI:
                  status = writeHvps(m, CAM_L_HVPSFINHLE, 0, &ival);
                  break;
               case ID_HVPS_LATO:
                  status = writeHvps(m, CAM_L_HVPSFOVLE, 0, &ival);
                  break;
               case ID_HVPS_STATUS:
                  status = writeHvps(m, CAM_L_HVPSFSTAT, 0, &ival);
                  break;
               case ID_HVPS_RESET:
                  status = writeHvps(m, CAM_L_HVPSFOVINRES, 0, &ival);
                  break;
               case ID_HVPS_FRAMP:
                  status = writeHvps(m, CAM_L_HVPSFASTRAMP, 0, &ival);
                  break;
               default:
                  DEBUG(1, "icbServer (ai record), unknown function=%d\n",
                           icbCommand);
                  status = ERROR;
               }
            } else {
              DEBUG(1, "(icbServer [%s]): invalid ICB command %d\n",
                  pMessageServer->getName(), icbCommand)
            }
         } else if (psm->extra == ICB_READ) {  /* This is a read (input) command */
            Float64Message *pfm = (Float64Message *)inmsg;
            Int32Message *pim = (Int32Message *)inmsg;
            switch (icbCommand) {
            case ID_ADC_ZERORBV:
               status = readAdc(m, CAM_F_ZERO, 0, &fval);
               pfm->value = fval;
               break;
            case ID_AMP_SHAPING:
               status = readAmp(m, CAM_F_AMPTC, 0, &fval);
               pfm->value = fval;
               break;
            case ID_AMP_PZRBV:
               status = readAmp(m, CAM_L_AMPPZ, 0, &ival);
               pfm->value = (float)ival;
               break;
            case ID_HVPS_VPOL:
               status = readHvps(m, CAM_L_HVPSFPOL, 0, &ival);
               pim->value = ival;
               break;
            case ID_HVPS_INH:
               status = readHvps(m, CAM_L_HVPSFINH, 0, &ival);
               pim->value = ival;
               break;
            case ID_HVPS_OVL:
               status = readHvps(m, CAM_L_HVPSFOV, 0, &ival);
               pim->value = ival;
               break;
            case ID_HVPS_STATRBV:
               status = readHvps(m, CAM_L_HVPSFSTAT, 0, &ival);
               pim->value = ival;
               break;
            case ID_HVPS_BUSY:
               status = readHvps(m, CAM_L_HVPSFBUSY, 0, &ival);
               pim->value = ival;
               break;
            case ID_HVPS_VOLTRBV:
               status = readHvps(m, CAM_F_VOLTAGE, 0, &fval);
               pfm->value = fval;
               break;
            default:
               DEBUG(1, "icbServer (ai record), unknown function=%d\n",
                  icbCommand);
               status = ERROR;
            }
            if (status != OK) DEBUG(1,
               "(icbServer [%s]): Got Float64Message (read), cmd=%d\n",
               pMessageServer->getName(), icbCommand)
            if (status != OK) DEBUG(1,
               "      fval=%f, ival=%d, status=%d\n", fval, ival, status)
         } else {
              DEBUG(1, "(icbServer [%s]): invalid message type\n",
                  pMessageServer->getName())
         }
         psm->status = status;
         pMessageServer->reply(psm);
      } // End while(pMessageServer->receive())
   } // End while(true)
}

int icbServer::writeAdc(ICB_MODULE *m, long command, char c, void *addr)
{
   ICB_PARAM_LIST icb_write_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   return(icb_adc_hdlr(m->index, icb_write_list, ICB_M_HDLR_WRITE));
}

int icbServer::writeAmp(ICB_MODULE *m, long command, char c, void *addr)
{
   ICB_PARAM_LIST icb_write_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   return(icb_amp_hdlr(m->index, icb_write_list, ICB_M_HDLR_WRITE));
}

int icbServer::writeHvps(ICB_MODULE *m, long command, char c, void *addr)
{
   ICB_PARAM_LIST icb_write_list[] = {
      {command, c,  addr},   /* Parameter              */
      {0,       0,  0}};     /* End of list               */
   return(icb_hvps_hdlr(m->index, icb_write_list, ICB_M_HDLR_WRITE));
}

// We need a way to determine if the module is communicating.  icb_xxx_hdlr does
// not always return an error condition if the module is not available.  Read the
// CSR and test it for the value 0xff, which is what it has if it is not on the ICB.
int icbServer::readAdc(ICB_MODULE *m, long command, char c, void *addr)
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

int icbServer::readAmp(ICB_MODULE *m, long command, char c, void *addr)
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

int icbServer::readHvps(ICB_MODULE *m, long command, char c, void *addr)
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

static const iocshArg icbSetupArg0 = { "Server name",iocshArgString};
static const iocshArg icbSetupArg1 = { "MaxModules",iocshArgInt};
static const iocshArg icbSetupArg2 = { "QueueSize",iocshArgInt};
static const iocshArg * const icbSetupArgs[3] = {&icbSetupArg0,
                                                 &icbSetupArg1,
                                                 &icbSetupArg2};
static const iocshFuncDef icbSetupFuncDef = {"icbSetup",3,icbSetupArgs};
static void icbSetupCallFunc(const iocshArgBuf *args)
{
    icbSetup(args[0].sval, args[1].ival, args[2].ival);
}


static const iocshArg icbConfigArg0 = { "Server name",iocshArgString};
static const iocshArg icbConfigArg1 = { "Module",iocshArgInt};
static const iocshArg icbConfigArg2 = { "Ethernet address",iocshArgInt};
static const iocshArg icbConfigArg3 = { "ICB address",iocshArgInt};
static const iocshArg * const icbConfigArgs[4] = {&icbConfigArg0,
                                                  &icbConfigArg1,
                                                  &icbConfigArg2,
                                                  &icbConfigArg3};
static const iocshFuncDef icbConfigFuncDef = {"icbConfig",4,icbConfigArgs};
static void icbConfigCallFunc(const iocshArgBuf *args)
{
    icbConfig(args[0].sval, args[1].ival, args[2].ival, args[3].ival);
}

void icbServerRegister(void)
{
    iocshRegister(&icbSetupFuncDef,icbSetupCallFunc);
    iocshRegister(&icbConfigFuncDef,icbConfigCallFunc);
}

epicsExportRegistrar(icbServerRegister);
