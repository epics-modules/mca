#ifndef RTEMS_DMA_H
#define RTEMS_DMA_H

#include <epicsTypes.h>
#include <bsp.h>
#include <bsp/vme_am_defs.h>
#include <bsp/VMEDMA.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif


/* need this prototype for RTEMS <= 4.9.3 */
int posix_memalign(void **memptr, size_t alignment, size_t size);

typedef void (*DMA_CALLBACK)(void *context);
typedef int* DMA_ID;

/**
 * NOTE: the convention here is to always use DMA channel 0.
 * This is the common denominator among Tundra Universe and Tsi148
 * chipsets.
 */


/**
 *  setup a DMA channel and DMA ISR
 */
static DMA_ID sysDmaCreate(DMA_CALLBACK callback, void *context) {
    /* dmaChannel must be 0 for mvme5500, 0 or 1 for mvme6100/mvme3100 */
    int dmaChannel = 0; 
    int status = 0;
    DMA_ID dmaId;
   
    /* unused by RTEMS, we allocate a DMA_ID anyway for compatibility */
    dmaId = (DMA_ID)calloc(1,sizeof(DMA_ID));
    if(dmaId == NULL) {
        printf("Couldn't allocate DMA_ID: %s\n", strerror(errno));
        return dmaId;
    }

#ifndef RTEMS_DMA_LOWLATENCY
    status = BSP_VMEDmaSetup(dmaChannel,BSP_VMEDMA_OPT_THROUGHPUT, 
                                VME_AM_EXT_SUP_MBLT, 0);
#else 
    status = BSP_VMEDmaSetup(dmaChannel,BSP_VMEDMA_OPT_LOWLATENCY, 
                                VME_AM_EXT_SUP_MBLT, 0);
#endif
    if(status != 0) {
        printf("Error: BSP_VMEDmaSetup returned code %d\n", status);
        return NULL;
    }

    status = BSP_VMEDmaInstallISR(dmaChannel, callback, context);
    if (status != 0) {
        printf("Error in BSP_VMEDmaInstallISR: code %d\n", status);
        return NULL;
    }

    return dmaId;
}


/**
 * Returns one of (for Tsi148, mvme6100/3100):
 *      BSP_VMEDMA_STATUS_OK (0)
 *      BSP_VMEDMA_STATUS_BUSY (3)
 *      BSP_VMEDMA_STATUS_BERR_PCI (2)
 *      BSP_VMEDMA_STATUS_BERR_VME (1)
 *      BSP_VMEDMA_STATUS_OERR (5)
 */
static int sysDmaStatus(DMA_ID dmaId) {
    int dmaChannel = 0;

    return (int)BSP_VMEDmaStatus(dmaChannel);
}


/**
 * Returns one of (for Tsi148, mvme6100/3100):
 *      BSP_VMEDMA_STATUS_OK (0)
 *      BSP_VMEDMA_STATUS_BUSY (3)
 *      BSP_VMEDMA_STATUS_UNSUP (-1)
 *
 * Args:
 *      adrsSpace is set in sysDmaCreate()
 *      length is in bytes
 *      dataWidth is unused
 *          
 */
static int sysDmaFromVme(DMA_ID dmaId, void *pLocal, epicsUInt32 vmeAddr,
                    int adrsSpace, int length, int dataWidth)       {
    int dmaChannel = 0;

    return BSP_VMEDmaStart(dmaChannel, (uint32_t)pLocal, vmeAddr, length);
}

/**
 * These are unused in the context of the SIS3820 driver
 */
#if 0
static int sysDmaToVme(DMA_ID dmaId, epicsUInt32 vmeAddr, int adrsSpace,
                    void *pLocal, int length, int dataWidth)            {
    return -1;
}

static void* sysDmaContext(DMA_ID dmaId) {
    return NULL;
}
#endif /* #if 0'd */


#ifdef __cplusplus
}
#endif
#endif /* RTEMS_DMA_H */
