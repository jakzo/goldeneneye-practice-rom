#ifndef PRACTICE_STORAGE_H
#define PRACTICE_STORAGE_H

#include <ultra64.h>

/**
 * Storage abstraction layer for save state persistence.
 *
 * Currently backed by SRAM, but designed so that alternative backends
 * (e.g. Summercart SD, internet) can be dropped in by replacing the
 * implementation in practice_storage.c.
 *
 * Data is read/written sequentially via a cursor that auto-advances.
 * The cursor tracks an absolute offset within the storage medium.
 */

typedef struct {
  u32 offset;
} StorageCursor;

/**
 * Initialize a storage cursor at the given base offset.
 */
void storage_cursor_init(StorageCursor *cur, u32 base_offset);

/**
 * Write `size` bytes from `data` to storage at the cursor's current
 * position, then advance the cursor.
 *
 * The source memory must be valid RDRAM (it is passed to the PI DMA
 * engine).  This means live game-state structs can be written directly
 * without an intermediate buffer — the DMA reads from RDRAM and writes
 * to the storage medium.
 */
void storage_write(StorageCursor *cur, const void *data, u32 size);

/**
 * Read `size` bytes from storage at the cursor's current position into
 * `data`, then advance the cursor.
 *
 * The destination memory must be valid RDRAM.  Live game-state structs
 * can be read into directly.
 */
void storage_read(StorageCursor *cur, void *data, u32 size);

#endif /* PRACTICE_STORAGE_H */
