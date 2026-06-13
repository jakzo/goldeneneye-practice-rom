#ifndef PRACTICE_SRAM_H
#define PRACTICE_SRAM_H

#include <ultra64.h>

void osSramInit(void);
s32 sram_read(u32 offset, void *dramAddr, u32 size);
s32 sram_write(u32 offset, void *dramAddr, u32 size);

#endif /* PRACTICE_SRAM_H */
