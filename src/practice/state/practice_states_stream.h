#ifndef PRACTICE_STATES_STREAM_H
#define PRACTICE_STATES_STREAM_H

#include <ultra64.h>

#define SRAM_PAGE_SIZE 512

typedef struct {
  u8 page[SRAM_PAGE_SIZE] __attribute__((aligned(16)));
  u32 page_offset;
  u32 sram_base;
  u32 sram_offset;
  u32 total_processed;
} SramStream;

extern SramStream *g_ActiveSramStream;

void sram_stream_init_write(SramStream *stream, u32 sram_base);
void sram_stream_init_read(SramStream *stream, u32 sram_base);
void sram_stream_flush(SramStream *stream);
void sram_stream_seek(SramStream *stream, u32 absolute_sram_offset);

void sram_stream_write_bytes(SramStream *stream, const void *src, u32 size);
void sram_stream_read_bytes(SramStream *stream, void *dst, u32 size);

void sram_stream_write_u32(SramStream *stream, u32 val);
void sram_stream_write_u16(SramStream *stream, u16 val);
void sram_stream_write_u8(SramStream *stream, u8 val);

u32 sram_stream_read_u32(SramStream *stream);
u16 sram_stream_read_u16(SramStream *stream);
u8 sram_stream_read_u8(SramStream *stream);

// Global active stream helper functions to make calling code look like
// write32/read32
void write32(u32 val);
void write16(u16 val);
void write8(u8 val);
void write_bytes(const void *src, u32 size);

u32 read32(void);
u16 read16(void);
u8 read8(void);
void read_bytes(void *dst, u32 size);

#endif // PRACTICE_STATES_STREAM_H
