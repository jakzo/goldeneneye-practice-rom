#include "practice_storage.h"
#include "../practice_sram.h"

/**
 * SRAM-backed storage implementation.
 *
 * All reads/writes go through the SRAM PI DMA helpers in practice_sram.c.
 * To add a new backend (Summercart SD, internet, etc.), replace the
 * body of storage_write() and storage_read() — or introduce a function-
 * pointer table if multiple backends must coexist at runtime.
 */

void storage_cursor_init(StorageCursor *cur, u32 base_offset) {
  cur->offset = base_offset;
}

void storage_write(StorageCursor *cur, const void *data, u32 size) {
  /* sram_write's dramAddr is non-const because the PI DMA helper is
     bidirectional; the buffer is never modified for writes. */
  sram_write(cur->offset, (void *)data, size);
  cur->offset += size;
}

void storage_read(StorageCursor *cur, void *data, u32 size) {
  sram_read(cur->offset, data, size);
  cur->offset += size;
}
