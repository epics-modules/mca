//fastSweep.cc

/*
    Author: Mark Rivers
    10/27/99

    04/20/00  MLR  Changed timing logic.  No longer reprogram IP-330 clock,
                   just change whether the callback is executed when called.
    09/04/00  MLR  Changed presetTime to liveTime and realTime for
                   compatibility with devMcaMpf.
    01/16/01 MLR   Added check for valid pIp330 in Ip330Sweep::init
    04/09/03 MLR   Created abstract base class, fastSweep, from Ip330Sweep
                   fastSweep is device-independent.  In order to use it a derived 
                   class must be created that implements the 
                   writeOutput(), readOutput(), setMicroSecondsPerScan and 
                   getMicroSecondsPerScan functions.
    06/10/03 MLR   Converted to EPICS R3.14.2
*/

#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#include <epicsTime.h>
#include <epicsExport.h>

#include "Message.h"
#include "Float64Message.h"
#include "Int32ArrayMessage.h"
#include "Float64ArrayMessage.h"
#include "mpfType.h"

#include "mca.h"
#include "fastSweep.h"
#include "DevMcaMpf.h"

#include "fastSweep.h"

extern "C"
{
#ifdef NODEBUG
#define DEBUG(l,f,v) ;
#else
#define DEBUG(l,f,v...) { if(l<fastSweepDebug) printf(f,## v); }
#endif
volatile int fastSweepDebug = 0;
}
epicsExportAddress(int, fastSweepDebug);


fastSweep:: fastSweep(int firstChan, int lastChan, int maxPoints) : 
                      firstChan(firstChan), lastChan(lastChan), maxPoints(maxPoints),
                      acquiring(0), numAcquired(0), 
                      elapsedTime(0.), startTime(epicsTime::getCurrent()),
                      numChans(lastChan - firstChan + 1)
{
    DEBUG(1, "fastSweep:fastSweep, maxPoints=%d, numChans=%d, this=%p\n", 
              maxPoints, numChans, this);
    pData = (int *)calloc(maxPoints*numChans,sizeof(int));
    if(!pData) {
        printf("fastSweep calloc failed\n");
    }
}

void fastSweep::getData(int channel, int *data)
{
    memcpy(data, &pData[maxPoints*(channel-firstChan)], numPoints*sizeof(int));
}

void fastSweep::nextPoint(int *newData)
{
    int i;

    if (numAcquired >= numPoints) {
       acquiring = 0;
    }
    if (!acquiring) return;
    
    int offset = numAcquired;
    for (i = 0; i < (numChans); i++) {
        pData[offset] = newData[firstChan + i];
        offset += maxPoints;
    }
    numAcquired++;
    elapsedTime = epicsTime::getCurrent() - startTime;
    if ((realTime > 0) && (elapsedTime >= realTime))
            acquiring = 0;
    if ((liveTime > 0) && (elapsedTime >= liveTime))
            acquiring = 0;
}

void fastSweep::erase()
{
    memset(pData, 0, maxPoints*numChans*sizeof(int));
    numAcquired = 0;
    /* Reset the elapsed time */
    elapsedTime = 0;
    startTime = epicsTime::getCurrent();
}   

void fastSweep::startAcquire()
{
    if (!acquiring) {
       acquiring = 1;
       startTime = epicsTime::getCurrent();
    }
}   

void fastSweep::stopAcquire()
{
    acquiring = 0;
}   

int fastSweep::setNumPoints(int n)
{
    if ((n < 1) || (n > maxPoints)) return(-1);
    numPoints = n;
    return(0);
}   

int fastSweep::getNumPoints()
{
    return(numPoints);
}   

int fastSweep::setRealTime(double time)
{
    // "time" is in seconds
    realTime = time;
    return(0);
}   

int fastSweep::setLiveTime(double time)
{
    // "time" is in seconds
    liveTime = time;
    return(0);
}   

double fastSweep::getElapsedTime()
{
    // Return elapsed time in seconds
    return(elapsedTime);
}

int fastSweep::getStatus()
{
    return(acquiring);
}


fastSweepServer::fastSweepServer(const char *serverName, fastSweep *pFastSweep, 
                                 int queueSize) :  pFastSweep(pFastSweep)
{
    pMessageServer = new MessageServer(serverName, queueSize);
}

void fastSweepServer::fastServer(fastSweepServer *pFastSweepServer)
{
    while(true) {
        MessageServer *pMessageServer = pFastSweepServer->pMessageServer;
        fastSweep *pFastSweep = pFastSweepServer->pFastSweep;
        pMessageServer->waitForMessage();
        Message *inmsg;
        int cmd;
        int nchans=0;
        int len;
        struct devMcaMpfStatus *pstat;

        while((inmsg = pMessageServer->receive())) {
            Float64Message *pim = (Float64Message *)inmsg;
            Int32ArrayMessage *pi32m = NULL;
            Float64ArrayMessage *pf64m = NULL;
            pim->status = 0;
            cmd = pim->cmd;
            DEBUG(2, "(fastSweepServer): Got Message, cmd=%d\n", cmd)
            DEBUG(2, "(fastSweepServer): pFastSweep=%p\n", pFastSweep)
            switch (cmd) {
               case MSG_ACQUIRE:
                  pFastSweep->startAcquire();
                  break;
               case MSG_READ:
                  pi32m = (Int32ArrayMessage *)pMessageServer->
                           allocReplyMessage(pim, messageTypeInt32Array);
                  nchans = pFastSweep->getNumPoints();
                  DEBUG(2, "cmdRead, allocating nchans=%d\n", nchans)
                  pi32m->allocValue(nchans);
                  DEBUG(2, "cmdRead, setLength\n")
                  pi32m->setLength(nchans);
                  DEBUG(2, "cmdRead, getData, address=%d\n", pim->address)
                  pFastSweep->getData(pim->address, pi32m->value);
                  delete pim;
                  pMessageServer->reply(pi32m);
                  break;
               case MSG_SET_CHAS_INT:
                  // No-op for fastSweep
                  break;
               case MSG_SET_CHAS_EXT:
                  // No-op for fastSweep
                  break;
               case MSG_SET_NCHAN:
                  if(pFastSweep->setNumPoints((int)pim->value))
                        pim->status= -1;
                  break;
               case MSG_SET_DWELL:
                  if(pFastSweep->setMicroSecondsPerPoint(pim->value*1.e6))
                        pim->status= -1;
                  break;
               case MSG_SET_REAL_TIME:
                  if(pFastSweep->setRealTime(pim->value))
                        pim->status= -1;
                  break;
               case MSG_SET_LIVE_TIME:
                  if(pFastSweep->setLiveTime(pim->value))
                        pim->status= -1;
                  break;
               case MSG_SET_COUNTS:
                  // No-op for fastSweep
                  break;
               case MSG_SET_LO_CHAN:
                  // No-op for fastSweep
                  break;
               case MSG_SET_HI_CHAN:
                  // No-op for fastSweep
                  break;
               case MSG_SET_NSWEEPS:
                  // No-op for fastSweep
                  break;
               case MSG_SET_MODE_PHA:
                  // No-op for fastSweep
                  break;
               case MSG_SET_MODE_MCS:
                  // No-op for fastSweep
                  break;
               case MSG_SET_MODE_LIST:
                  // No-op for fastSweep
                  break;
               case MSG_GET_ACQ_STATUS:
                  pf64m = (Float64ArrayMessage *)pMessageServer->
                                allocReplyMessage(pim, messageTypeFloat64Array);
                  len = sizeof(struct devMcaMpfStatus) / sizeof(double);
                  DEBUG(2, "(fastSweepServer): len=%d\n", len)
                  pf64m->allocValue(len);
                  pf64m->setLength(len);
                  pstat = (struct devMcaMpfStatus *)pf64m->value;
                  pstat->realTime = pFastSweep->getElapsedTime();
                  pstat->liveTime = pFastSweep->getElapsedTime();
                  pstat->dwellTime = 
                     pFastSweep->getMicroSecondsPerPoint()/1.e6;
                  pstat->acquiring = pFastSweep->getStatus();
                  pstat->totalCounts = 0;
                  delete pim;
                  pMessageServer->reply(pf64m);
                  break;
               case MSG_STOP_ACQUISITION:
                  pFastSweep->stopAcquire();
                  break;
               case MSG_ERASE:
                  pFastSweep->erase();
                  break;
               case MSG_SET_SEQ:
                  // No-op for fastSweep
                  break;
               case MSG_SET_PSCL:
                  // No-op for fastSweep
                  break;
               default:
                  printf("%s fastSweepServer got illegal command %d\n",
                     pMessageServer->getName(), pim->cmd);
                  break;
            }
            if (cmd != MSG_READ && cmd != MSG_GET_ACQ_STATUS)
               pMessageServer->reply(pim);
        }
    }
}
