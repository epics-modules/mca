/* DMA Routines */

// If OS supports DMA - overridden from compiler line

#define USE_DMA 1

#include <epicsTypes.h>

#define VME_AM_EXT_USR_D64BLT           0x08
#define VME_AM_EXT_SUP_D64BLT           0x0c
#define VME_AM_STD_USR_D64BLT           0x38
#define VME_AM_STD_SUP_D64BLT           0x3c

#ifdef USE_DMA

#include "apsLib.h"

#else

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*DMA_CALLBACK)(void *context);
typedef int* DMA_ID;

DMA_ID sysDmaCreate(DMA_CALLBACKS callback, void *context) 
{ 
  return -1;
}
void * sysDmaContext(DMA_ID dmaId)
{ 
  return -1;
}
int sysDmaStatus(DMA_ID dmaId)
{ 
  return -1;
}
int sysDmaToVme(DMA_ID dmaId, epicsUInt32 vmeAddr, int adrsSpace,
                   void *pLocal, int length, int dataWidth)
{ 
  return -1;
}
int sysDmaFromVme(DMA_ID dmaId, void *pLocal, epicsUInt32 vmeAddr,
                     int adrsSpace, int length, int dataWidth)
{ 
  return -1;
}

#ifdef __cplusplus
}
#endif
#endif /* USE_DMA */
