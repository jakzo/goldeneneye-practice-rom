#include <ultra64.h>
#include <PR/os_internal.h>
#include <PR/rcp.h>
#include "practice_sram.h"

#define SRAM_START_ADDR  0x08000000
#define SRAM_latency     0x5
#define SRAM_pulse       0x0c
#define SRAM_pageSize    0xd
#define SRAM_relDuration 0x2

OSPiHandle gSramHandle;
static s32 gSramInitialized = 0;

// Declarations of internal libultra APIs we need
extern OSDevMgr __osPiDevMgr;
extern OSMesgQueue *osPiGetCmdQueue(void);

void osSramInit(void)
{
    OSIntMask saveMask;

    // Configure PI registers directly for Domain 2 (SRAM)
    IO_WRITE(PI_BSD_DOM2_LAT_REG, SRAM_latency);
    IO_WRITE(PI_BSD_DOM2_PWD_REG, SRAM_pulse);
    IO_WRITE(PI_BSD_DOM2_PGS_REG, SRAM_pageSize);
    IO_WRITE(PI_BSD_DOM2_RLS_REG, SRAM_relDuration);

    // Initialize the OSPiHandle for SRAM
    gSramHandle.type = DEVICE_TYPE_SRAM;
    gSramHandle.baseAddress = PHYS_TO_K1(SRAM_START_ADDR);
    gSramHandle.latency = (u8)SRAM_latency;
    gSramHandle.pulse = (u8)SRAM_pulse;
    gSramHandle.pageSize = (u8)SRAM_pageSize;
    gSramHandle.relDuration = (u8)SRAM_relDuration;
    gSramHandle.domain = PI_DOMAIN2;
    gSramHandle.speed = 0;

    bzero(&(gSramHandle.transferInfo), sizeof(gSramHandle.transferInfo));

    // Safely add it to the PI Table
    saveMask = osSetIntMask(OS_IM_NONE);
    gSramHandle.next = __osPiTable;
    __osPiTable = &gSramHandle;
    osSetIntMask(saveMask);

    gSramInitialized = 1;
}

static s32 sram_epi_dma(s32 direction, u32 devAddr, void *dramAddr, u32 size)
{
    OSIoMesg mb;
    OSMesgQueue mq;
    OSMesg m;

    if (!__osPiDevMgr.active) {
        return -1;
    }

    osCreateMesgQueue(&mq, &m, 1);

    mb.hdr.type = (direction == OS_READ) ? OS_MESG_TYPE_EDMAREAD : OS_MESG_TYPE_EDMAWRITE;
    mb.hdr.pri = OS_MESG_PRI_NORMAL;
    mb.hdr.retQueue = &mq;
    mb.dramAddr = dramAddr;
    mb.devAddr = devAddr;
    mb.size = size;
    mb.piHandle = &gSramHandle;

    if (osSendMesg(osPiGetCmdQueue(), (OSMesg)&mb, OS_MESG_NOBLOCK) == -1) {
        return -1;
    }

    osRecvMesg(&mq, NULL, OS_MESG_BLOCK);
    return 0;
}

s32 sram_read(u32 offset, void *dramAddr, u32 size)
{
    if (!gSramInitialized) {
        osSramInit();
    }

    osInvalDCache(dramAddr, size);
    return sram_epi_dma(OS_READ, offset, dramAddr, size);
}

s32 sram_write(u32 offset, void *dramAddr, u32 size)
{
    if (!gSramInitialized) {
        osSramInit();
    }

    osWritebackDCache(dramAddr, size);
    return sram_epi_dma(OS_WRITE, offset, dramAddr, size);
}
