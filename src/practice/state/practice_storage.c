#include "practice_storage.h"
#include "../practice_sram.h"
#include "emu_log.h"

extern void *memcpy(void *dst, const void *src, size_t count);

static u8 debug_data[20000];
static u8 debug_written[20000];

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

  // Save data in memory to compare when reading
  if (cur->offset <= sizeof(debug_data)) {
    memcpy(&debug_data[cur->offset - size], data, size);

    // Mark bytes as written in this session
    u32 idx;
    u32 off = cur->offset - size;
    for (idx = 0; idx < size; idx++) {
      debug_written[off + idx] = 1;
    }
  }
}

void storage_read(StorageCursor *cur, void *data, u32 size) {
  sram_read(cur->offset, data, size);
  cur->offset += size;

  // Check consistency of storage against data saved in memory
  if (cur->offset <= sizeof(debug_data)) {
    u32 i;
    u32 m = cur->offset - size;
    for (i = 0; i < size; i++) {
      if (debug_written[m + i]) {
        u8 a = debug_data[m + i];
        u8 b = ((u8 *)data)[i];
        if (a != b) {
          u32 j = (i >= 3) ? (i - 3) : 0;
          u32 e = (i + 3 < size) ? (i + 3) : (size - 1);
          for (; j <= e; j++) {
            u8 x = debug_data[m + j];
            u8 y = ((u8 *)data)[j];
            emu_log("%04x [%04d]: %02x %02x%s", m + j, j, x, y,
                    x == y ? "" : " <--- BAD");
          }
        }
      }
    }
  }
}
