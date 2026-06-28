#ifndef PRACTICE_SRAM_H
#define PRACTICE_SRAM_H

#include <ultra64.h>

#define SRAM_SIZE_BYTES (128 * 1024)

/*
 * SRAM layout:
 *   0x000-0x1ff: Original game save (emulated 4 Kbit EEPROM)
 *   0x200-0x5ff: Practice config (1 KiB reserved)
 *   0x600-end:   Practice save state
 */
#define GAME_SAVE_SRAM_SIZE 0x200
#define CONFIG_SRAM_OFFSET GAME_SAVE_SRAM_SIZE
#define CONFIG_SRAM_SIZE 0x400
#define SAVE_STATE_SRAM_OFFSET (CONFIG_SRAM_OFFSET + CONFIG_SRAM_SIZE)

void osSramInit(void);
s32 sram_read(u32 offset, void *dramAddr, u32 size);
s32 sram_write(u32 offset, void *dramAddr, u32 size);

#endif /* PRACTICE_SRAM_H */
