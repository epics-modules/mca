/* mcaRegister.c */

#include "epicsExport.h"
#include "epicsTypes.h"
#include "symTable.h"

extern int mcaRecordDebug;
extern int devMcaMpfDebug;
extern int fastSweepDebug;

void mcaRegister(void)
{
    addSymbol("mcaRecordDebug", &mcaRecordDebug, epicsInt32T);
    addSymbol("devMcaMpfDebug", &devMcaMpfDebug, epicsInt32T);
    addSymbol("fastSweepDebug", &fastSweepDebug, epicsInt32T);
}

epicsExportRegistrar(mcaRegister);
