/* apsLib.h */

/* Routines added for APS BSPs
 *
 * Author: Andrew Johnson <anj@aps.anl.gov>
 */

#ifndef INC_apsLib_H
#define INC_apsLib_H

#include <vxWorks.h>
#include <moduleLib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* A24 Routines */

extern STATUS sysA24MapRam(size_t size);
extern void *sysA24Malloc(size_t size);
extern STATUS sysA24Free(void *pBlock);
extern void sysA24MemShow(void);


/* DMA Routines */

typedef void (*DMA_CALLBACK)(void *context);
typedef struct dmaRequest *DMA_ID;

extern DMA_ID sysDmaCreate(DMA_CALLBACK callback, void *context);
extern void * sysDmaContext(DMA_ID dmaId);
extern STATUS sysDmaStatus(DMA_ID dmaId);
extern STATUS sysDmaToVme(DMA_ID dmaId, UINT32 vmeAddr, int adrsSpace,
			  void *pLocal, int length, int dataWidth);
extern STATUS sysDmaFromVme(DMA_ID dmaId, void *pLocal, UINT32 vmeAddr,
			    int adrsSpace, int length, int dataWidth);


/* Microsecond Timer Routines */

extern UINT32 sysUsSince(UINT32 start);
extern void sysUsDelay(int delay);


/* Other Routines */

extern MODULE_ID load(char *file);
extern STATUS bootShow(void);
extern STATUS sysVmeMapShow(void);
extern STATUS sysIntShow (void);
extern STATUS sysAtReboot(VOIDFUNCPTR closedown);

#ifdef __cplusplus
}
#endif

#endif /* INC_apsLib_H */
