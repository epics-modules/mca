//icbDspServer.cc

//
//  Author: Mark Rivers
//  Date: 1-Sept-2000  Modified from mcaAIMServer.cc
//
//  This module is an MPF server which receives messages from devIcbMpf.cc and
//  commmunicates with a Canberra 9660 DSP module.
//
//  It can receive Float64 messages (ao or ai records) or Int32 messages (mbbo,
//  mbbi records).  pm->extra is ICB_READ for ai or mbbi, and ICB_WRITE for ao
//  or mbbo.

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <errlog.h>

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
#include "icbDsp.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<=icbDspServerDebug) errlogPrintf(f,## v); }
#endif
volatile int icbDspServerDebug = 0;
}

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
} ICB_DSP_MODULE;

class icbDspServer;
extern "C" int icbDspAddModule(icbDspServer *picbDspServer, 
                                int module, char *address);

class icbDspServer {
public:
    void processMessages();
    MessageServer *pMessageServer;
    static void icbDspServerTask(icbDspServer *);
    int sendDsp(int module, int icbAddress, int command, unsigned short data);
    int readDsp(int module, int icbAddress, int command, unsigned short *data);
    unsigned short doubleToShort(double dval, double dmin, double dmax,
                                 unsigned short imin, unsigned short imax);
    double shortToDouble(unsigned short ival, double dmin, double dmax,
                                 unsigned short imin, unsigned short imax);
    double unpackThroughput(unsigned short ival);
    int maxModules;
    ICB_DSP_MODULE *icbDspModule;
};

extern "C" icbDspServer *icbDspConfig(const char *serverName, int maxModules, 
                             char *address, int queueSize)
{
    char taskname[80];
    int s;

    icbDspServer *p = new icbDspServer;
    p->pMessageServer = new MessageServer(serverName, queueSize);
    p->maxModules = maxModules;
    p->icbDspModule = (ICB_DSP_MODULE *)
                           calloc(maxModules, sizeof(ICB_DSP_MODULE));

    // Initialize the ICB software in case this has not been done
    s=icb_initialize();
    DEBUG(5, "(icbDspConfig): icb_initialize=%d\n", s);

    icbDspAddModule(p, 0, address);
    strcpy(taskname, "t");
    strcat(taskname, serverName);
    epicsThreadId threadId = epicsThreadCreate(taskname, epicsThreadPriorityMedium, 10000,
                      (EPICSTHREADFUNC)icbDspServer::icbDspServerTask, (void*) p);
    if(threadId == NULL)
       errlogPrintf("%s icbDspServer ThreadCreate Failure\n",
            p->pMessageServer->getName());
    return(p);
}

extern "C" int icbDspAddModule(icbDspServer *p, int module, 
                             char *address)
{
   ICB_DSP_MODULE *m;
   unsigned char csr;
   
   if ((module < 0) || (module >= p->maxModules)) {
      errlogPrintf("icbDspAddModule: invalid module\n");
      return(ERROR);
   }
   m = &p->icbDspModule[module];
   if (m->defined) {
      printf("icbDspAddModule: module %d already defined!\n", module);
      return(ERROR);
   }
   m->defined = 1;
   if (parse_ICB_address(address, &m->module, &m->icbAddress) != OK) {
       errlogPrintf("icbDspModule: failed to initialize this address: %s\n",
                           address);
       return(ERROR);
   }
   // Initialize the module (turn on green light)
   csr = ICB_M_CTRL_LED_GREEN;
   write_icb(m->module, m->icbAddress, 0, 1, &csr);
   return(OK);
}    

void icbDspServer::icbDspServerTask(icbDspServer *picbDspServer)
{
   picbDspServer->processMessages();
}

void icbDspServer::processMessages()
{
   while(true) {
      pMessageServer->waitForMessage();
      Message *inmsg;
      ICB_DSP_MODULE *m;
      int icbCommand;
      int module, icbAddress;
      int status=OK;
      int index;

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
         m = &icbDspModule[index];
         if (!m->defined) {
            DEBUG(1, "(processMessages): module %d is not defined\n", index);
            psm->status = ERROR;
            pMessageServer->reply(psm);
            continue;
         }
         module = m->module;
         icbAddress = m->icbAddress;
         if ((psm->getType() == messageTypeInt32) &&
             (psm->extra == ICB_WRITE)) {
            // This is an MBBO record, write an integer value
            Int32Message *pim = (Int32Message *)inmsg;
            unsigned short value = pim->value;
            DEBUG(3,
               "(icbDspServer [%s]): Got Int32Message (mbbo), cmd=%d, value=%d\n",
                  pMessageServer->getName(), icbCommand, value)
            switch (icbCommand) {
            // Check for special cases here.
            // Most values just write rval field.
            case ID_ADC_CGAIN:   // We set both the ADC gain and range
               status = sendDsp(module, icbAddress, ID_ADC_CGAIN, value);
               status = sendDsp(module, icbAddress, ID_ADC_CRANGE, value);
               break;
            case ID_START_PZ:   // Start auto pole-zero if value is non-zero
               if (value != 0)
                  status = sendDsp(module, icbAddress, ID_SYSOP_CMD, 8);
               break;
            case ID_START_BDC:  // Start auto BDC if value is non-zero
               if (value != 0)
                  status = sendDsp(module, icbAddress, ID_SYSOP_CMD, 9);
               break;
            case ID_INFO_THRI:   // Remember throughput index
               m->info_thri = value;
               status = sendDsp(module, icbAddress, icbCommand, value);
               break;
            case ID_CLEAR_ERRORS:  // Clear errors, abort auto-PZ/BDC if value
                                   // is non-zero
               if (value != 0)
                  status = sendDsp(module, icbAddress, ID_SYSOP_CMD, 0);
               break;
            default:  // For all other opcodes we just send the value
               status = sendDsp(module, icbAddress, icbCommand, value);
               break;
            } // End switch (icbCommand)
         } // End MBBO message type
         else if ((psm->getType() == messageTypeInt32) &&
             (psm->extra == ICB_READ)) {
            // This is an MBBI record, read an integer value
            Int32Message *pim = (Int32Message *)inmsg;
            unsigned short ival;
            switch (icbCommand) {
            case ID_STABLZ_GMOD:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               // Store value for use by other commands
               m->stablz_gmod = ival;
               ival = ival & 0x3;
               break;
            case ID_STABLZ_GOVR:
               // Use cached value of GMOD
               if (m->stablz_gmod & 0x80) ival=1; else ival=0;
               break;
            case ID_STABLZ_GOVF:
               // Use cached value of GMOD
               if (m->stablz_gmod & 0x40) ival=1; else ival=0;
               break;
            case ID_STABLZ_ZMOD:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               // Store value for use by other commands
               m->stablz_gmod = ival;
               ival = ival & 0x3;
               break;
            case ID_STABLZ_ZOVR:
               // Use cached value of GMOD
               if (m->stablz_zmod & 0x80) ival=1; else ival=0;
               break;
            case ID_STABLZ_ZOVF:
               // Use cached value of GMOD
               if (m->stablz_zmod & 0x40) ival=1; else ival=0;
               break;
            case ID_INFO_PZAER:
               ival = ((m->info_pz & 0x7000) >> 12);
               break;
            case ID_INFO_BDCAER:
               ival = ((m->info_bdc & 0x7000) >> 12);
               break;
            case ID_STATUS_PZBSY:
               ival = (m->status_flgs & 0x1) != 0;
               break;
            case ID_STATUS_BDBSY:
               ival = (m->status_flgs & 0x2) != 0;
               break;
            case ID_STATUS_MINT:
               ival = (m->status_flgs & 0x4) != 0;
               break;
            case ID_STATUS_DGBSY:
               ival = (m->status_flgs & 0x8) != 0;
               break;
            case ID_STATUS_MERR:
               ival = (m->status_flgs & 0x10) != 0;
               break;
            default:
               DEBUG(1, "icbDspServer (mbbi record), unknown function=%d\n",
                        icbCommand);
               status = ERROR;
               break;
            } // End switch (icbCommand)
            pim->value = ival;
            DEBUG(3,
               "(icbDspServer [%s]): Got Int32Message (mbbi), cmd=%d, value=%d\n",
                  pMessageServer->getName(), icbCommand, ival)
         } // End MBBI message type
         else if ((psm->getType() == messageTypeFloat64) &&
                  (psm->extra == ICB_WRITE)) {
            // This is an AO record
            Float64Message *pim = (Float64Message *)inmsg;
            double dval = pim->value;
            USHORT ival=0;
            DEBUG(3,
               "(icbDspServer [%s]): Got Float64Message (ao), cmd=%d, value=%f\n",
                  pMessageServer->getName(), icbCommand, dval)
            switch (icbCommand) {
               // Convert from double value (VAL field) to output value (USHORT)
            case ID_AMP_FGAIN:
               ival = doubleToShort(dval, 0.4, 1.6, 1024, 4095);
               break;
            case ID_AMP_SFGAIN:
               ival = doubleToShort(dval, 0.0, 0.03, 0, 4095);
               break;
            case ID_ADC_OFFS:
               ival = doubleToShort(dval, 0.0, 16128., 0, 126);
               break;
            case ID_ADC_LLD:
               ival = doubleToShort(dval, 0.0, 100., 0, 32767);
               break;
            case ID_ADC_ZERO:
               ival = doubleToShort(dval, -3.125, 3.125, 0, 6250);
               break;
            case ID_FILTER_RT:
               ival = doubleToShort(dval, 0.4, 28.0, 4, 280);
               break;
            case ID_FILTER_FT:
               ival = doubleToShort(dval, 0.0, 3.0, 0, 30);
               break;
            case ID_FILTER_MFACT:
               ival = doubleToShort(dval, 0.0, 32767, 0, 32767);
               break;
            case ID_FILTER_PZ:
               ival = doubleToShort(dval, 0.0, 4095., 0, 4095);
               break;
            case ID_FILTER_THR:
               ival = doubleToShort(dval, 0.0, 32767., 0, 32767);
               break;
            case ID_STABLZ_GSPAC:
               ival = doubleToShort(dval, 2.0, 512., 2, 512);
               break;
            case ID_STABLZ_ZSPAC:
               ival = doubleToShort(dval, 2.0, 512., 2, 512);
               break;
            case ID_STABLZ_GWIN:
               ival = doubleToShort(dval, 1.0, 128., 1, 128);
               break;
            case ID_STABLZ_ZWIN:
               ival = doubleToShort(dval, 1.0, 128., 1, 128);
               break;
            case ID_STABLZ_GCENT:
               ival = doubleToShort(dval, 10.0, 16376., 10, 16376);
               break;
            case ID_STABLZ_ZCENT:
               ival = doubleToShort(dval, 10.0, 16376., 10, 16376);
               break;
            case ID_STABLZ_GRAT:
               ival = doubleToShort(dval, .01, 100., 1, 10000);
               break;
            case ID_STABLZ_ZRAT:
               ival = doubleToShort(dval, .01, 100., 1, 10000);
               break;
            case ID_MISC_FD:
               ival = doubleToShort(dval, 0.0, 100., 0, 1000);
               break;
            case ID_MISC_LTRIM:
               ival = doubleToShort(dval, 0.0, 1000., 0, 1000);
               break;
            default:
               DEBUG(1, "icbDspServer (ao record), unknown function=%d\n",
                        icbCommand);
               status = ERROR;
            } // End switch (icbCommand)
            if (status == OK) 
               status = sendDsp(module, icbAddress, icbCommand, ival);
         } // End AO message type
         else if ((psm->getType() == messageTypeFloat64) &&
                  (psm->extra == ICB_READ)) {
            // This is an AI record
            Float64Message *pim = (Float64Message *)inmsg;
            unsigned short ival;
            double dval=0.;
            switch (icbCommand) {
            case ID_STATUS_FLGS:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               // Store value for use by other commands
               m->status_flgs = ival;
               dval = ival;
               break;
            case ID_INFO_PZ:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               // Store value for use by other commands
               m->info_pz = ival;
               // Return actual pole zero
               ival = ival & 0xfff;
               dval = shortToDouble(ival, 0.0, 4095., 0, 4095);
               break;
            case ID_FILTER_PZ:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               dval = shortToDouble(ival, 0.0, 4095., 0, 4095);
               break;
            case ID_INFO_BDC:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               // Store value for use by other commands
               m->info_bdc = ival;
               // Return actual flat top
               ival = ival & 0x3f;
               dval = shortToDouble(ival, 0.0, 3.0, 0, 30);
               break;
            case ID_FILTER_FT:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               dval = shortToDouble(ival, 0.0, 3.0, 0, 30);
               break;
            case ID_STABLZ_GAIN:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               dval = ival;
               break;
            case ID_STABLZ_ZERO:
               status = readDsp(module, icbAddress, icbCommand, &ival);
               dval = ival;
               break;
            case ID_INFO_THRP:
               status = readDsp(module, icbAddress, ID_INFO_THRP, &ival);
               dval = unpackThroughput(ival);
               // Need to do special case of DT divide by 10.
               if (m->info_thri == 0) dval = dval / 10.;
               break;
            default:
               DEBUG(1, "icbDspServer (ai record), unknown function=%d\n",
                        icbCommand);
               status = ERROR;
            } // End switch (icbCommand)
            pim->value = dval;
            DEBUG(3,
               "(icbDspServer [%s]): Got Float64Message (ai), cmd=%d, value=%f\n",
                  pMessageServer->getName(), icbCommand, dval)
         } // End AI message type (last valid message type)
         else { 
            DEBUG(1, "(icbDspServer [%s]): invalid message type\n",
                  pMessageServer->getName())
         }
         psm->status = status;
         pMessageServer->reply(psm);
      } // End while(pMessageServer->receive())
   } // End while(true)
}


int icbDspServer::sendDsp(int module, int icbAddress, int command,
                      unsigned short data)
{
   unsigned char registers[4];
   unsigned char r6;
   int i;

   DEBUG(5, "icbDsp::sendDsp: module=%d, icbAddress=%d, command=%d, data=%d\n",
                   module, icbAddress, command, data);
   registers[0] = 0;  // Not a read command, use only table 0 for now
   registers[1] = command;                  // Parameter opcode
   registers[2] = (data & 0x0000ff00) >> 8; // MSB of data
   registers[3] = data & 0x000000ff;        // LSB of data
   write_icb(module, icbAddress, 2, 4, registers);
   /* The following delay seems to be necessary or writes will sometimes 
    * timeout below */
   epicsThreadSleep(DELAY_9660);

   // Read register 6 back.  Return error flag on error.  Wait for WDONE to be
   // set 
   for (i=0; i<MAX_9660_POLLS; i++) {
      read_icb(module, icbAddress, 6, 1, &r6);
      if ((r6 & R6_FAIL)  ||  (r6 & R6_MERR)) {
         DEBUG(1, "icbDsp::sendDsp: module error, module=%d, icbAddress=%d, command=%d, data=%d\n",
                   module, icbAddress, command, data);
         return(ERROR);
      }
      if ((r6 & R6_WDONE) && ((r6 & R6_MBUSY) == 0)) {
         DEBUG(5, "icbDsp::sendDsp: success, module=%d, icbAddress=%d, command=%d, data=%d\n",
                   module, icbAddress, command, data);
         return(OK);
      }
      epicsThreadSleep(DELAY_9660);
   }
   DEBUG(1, "icbDsp::sendDsp: timeout, module=%d, icbAddress=%d, command=%d, data=%d\n",
                   module, icbAddress, command, data);
   return(ERROR);
}


int icbDspServer::readDsp(int module, int icbAddress, int command,
                      unsigned short *data)
{
   unsigned char registers[4];
   unsigned char r6;
   int i;
   int s;

   DEBUG(5, "icbDsp::readDsp: module=%d, icbAddress=%d, command=%d\n",
                   module, icbAddress, command);
   registers[0] = 8;  // Read command, use only table 0 for now
   registers[1] = command;                  // Parameter opcode
   registers[2] = 0;
   registers[3] = 0;
   *data = 0;
   s = write_icb(module, icbAddress, 2, 4, registers);
   if (s != OK) {
      DEBUG(1, "icbDsp::readDsp: error in write_icb, module=%d, icbAddress=%d, command=%d, status=%d\n",
                   module, icbAddress, command, s);
      return(ERROR);
   }
   /* The following delay seems to be necessary or reads will sometimes 
    * timeout below */
   epicsThreadSleep(DELAY_9660);

   // Read register 6 back.  Return error flag on error.  Wait for RDAV to be
   // set 
   for (i=0; i<MAX_9660_POLLS; i++) {
      s = read_icb(module, icbAddress, 6, 1, &r6);
      if (s != OK) {
         DEBUG(1, "icbDsp::readDsp: error in read_icb, module=%d, icbAddress=%d, command=%d, status=%d\n",
                   module, icbAddress, command, s);
         return(ERROR);
      }
      if ((r6 & R6_FAIL)  ||  (r6 & R6_MERR)) {
         DEBUG(1, "icbDsp::readDsp: module error, module=%d, icbAddress=%d, command=%d\n",
                   module, icbAddress, command);
         return(ERROR);
      }
      if ((r6 & R6_RDAV) && ((r6 & R6_MBUSY) == 0)) {
         goto read_data;
      }
      epicsThreadSleep(DELAY_9660);
   }
   DEBUG(1, "icbDsp::readDsp: timeout, module=%d, icbAddress=%d, command=%d\n",
                   module, icbAddress, command);
   return(ERROR);
   
read_data:
   read_icb(module, icbAddress, 2, 4, registers);
   *data = registers[3] | (registers[2] << 8) | 
           (registers[1] << 16) | ((registers[0] & 0x1f) << 24);
   DEBUG(5, "icbDsp::readDsp: module=%d, icbAddress=%d, command=%d, data=%d\n",
                   module, icbAddress, command, *data);
   return(OK);
}

unsigned short icbDspServer::doubleToShort(
                              double dval, double dmin, double dmax,
                              unsigned short imin, unsigned short imax)
{
   double d=dval;
   unsigned short ival;

   if (d < dmin) d = dmin;
   if (d > dmax) d = dmax;
   ival = (unsigned short) (imin + (imax-imin) * (d - dmin) / (dmax - dmin) + 0.5);
   return(ival);
}

double icbDspServer::shortToDouble(
                              unsigned short ival, double dmin, double dmax,
                              unsigned short imin, unsigned short imax)
{
   double dval;
   unsigned short i=ival;

   if (i < imin) i = imin;
   if (i > imax) i = imax;
   dval = dmin + (dmax-dmin) * (i - imin) / (imax - imin);
   return(dval);
}

double icbDspServer::unpackThroughput(unsigned short ival)
{
   int exponent;
   double mantissa;

   exponent = (ival & 0xE000) >> 13;
   mantissa = (ival & 0x1FFF);
   return(mantissa * (2 << exponent));
}
