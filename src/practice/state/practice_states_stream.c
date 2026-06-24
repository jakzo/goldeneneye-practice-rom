#include "practice_states_stream.h"
#include "../practice_sram.h"
#include <ultra64.h>

extern void *memcpy(void *dst, const void *src, size_t count);

SramStream *g_ActiveSramStream = NULL;

void sram_stream_init_write(SramStream *stream, u32 sram_base) {
  stream->sram_base = sram_base;
  stream->sram_offset = sram_base;
  stream->page_offset = 0;
  stream->total_processed = 0;
  bzero(stream->page, SRAM_PAGE_SIZE);
}

void sram_stream_init_read(SramStream *stream, u32 sram_base) {
  stream->sram_base = sram_base;
  stream->sram_offset = sram_base;
  stream->page_offset = 0;
  stream->total_processed = 0;
  // Eagerly read first page
  sram_read(stream->sram_offset, stream->page, SRAM_PAGE_SIZE);
  stream->sram_offset += SRAM_PAGE_SIZE;
}

void sram_stream_flush(SramStream *stream) {
  if (stream->page_offset > 0) {
    // Write full page to SRAM (partially filled, padded with zeros)
    sram_write(stream->sram_offset, stream->page, SRAM_PAGE_SIZE);
    stream->sram_offset += SRAM_PAGE_SIZE;
    stream->page_offset = 0;
  }
}

void sram_stream_seek(SramStream *stream, u32 absolute_sram_offset) {
  u32 page_start = (absolute_sram_offset / SRAM_PAGE_SIZE) * SRAM_PAGE_SIZE;
  u32 intra_page = absolute_sram_offset % SRAM_PAGE_SIZE;

  if (stream->sram_offset >= SRAM_PAGE_SIZE && (stream->sram_offset - SRAM_PAGE_SIZE) == page_start) {
    stream->page_offset = intra_page;
  } else {
    stream->sram_offset = page_start;
    sram_read(stream->sram_offset, stream->page, SRAM_PAGE_SIZE);
    stream->sram_offset += SRAM_PAGE_SIZE;
    stream->page_offset = intra_page;
  }
}

void sram_stream_write_bytes(SramStream *stream, const void *src, u32 size) {
  const u8 *src_bytes = (const u8 *)src;
  u32 bytes_written = 0;
  while (bytes_written < size) {
    u32 space_left = SRAM_PAGE_SIZE - stream->page_offset;
    u32 chunk_size = size - bytes_written;
    if (chunk_size > space_left) {
      chunk_size = space_left;
    }

    memcpy(stream->page + stream->page_offset, src_bytes + bytes_written, chunk_size);
    stream->page_offset += chunk_size;
    bytes_written += chunk_size;
    stream->total_processed += chunk_size;

    if (stream->page_offset == SRAM_PAGE_SIZE) {
      sram_write(stream->sram_offset, stream->page, SRAM_PAGE_SIZE);
      stream->sram_offset += SRAM_PAGE_SIZE;
      stream->page_offset = 0;
      bzero(stream->page, SRAM_PAGE_SIZE);
    }
  }
}

void sram_stream_read_bytes(SramStream *stream, void *dst, u32 size) {
  u8 *dst_bytes = (u8 *)dst;
  u32 bytes_read = 0;
  while (bytes_read < size) {
    u32 available = SRAM_PAGE_SIZE - stream->page_offset;
    u32 chunk_size = size - bytes_read;
    if (chunk_size > available) {
      chunk_size = available;
    }

    memcpy(dst_bytes + bytes_read, stream->page + stream->page_offset, chunk_size);
    stream->page_offset += chunk_size;
    bytes_read += chunk_size;
    stream->total_processed += chunk_size;

    if (stream->page_offset == SRAM_PAGE_SIZE) {
      sram_read(stream->sram_offset, stream->page, SRAM_PAGE_SIZE);
      stream->sram_offset += SRAM_PAGE_SIZE;
      stream->page_offset = 0;
    }
  }
}

void sram_stream_write_u32(SramStream *stream, u32 val) {
  sram_stream_write_bytes(stream, &val, sizeof(u32));
}

void sram_stream_write_u16(SramStream *stream, u16 val) {
  sram_stream_write_bytes(stream, &val, sizeof(u16));
}

void sram_stream_write_u8(SramStream *stream, u8 val) {
  sram_stream_write_bytes(stream, &val, sizeof(u8));
}

u32 sram_stream_read_u32(SramStream *stream) {
  u32 val;
  sram_stream_read_bytes(stream, &val, sizeof(u32));
  return val;
}

u16 sram_stream_read_u16(SramStream *stream) {
  u16 val;
  sram_stream_read_bytes(stream, &val, sizeof(u16));
  return val;
}

u8 sram_stream_read_u8(SramStream *stream) {
  u8 val;
  sram_stream_read_bytes(stream, &val, sizeof(u8));
  return val;
}

// Global active wrappers
void write32(u32 val) {
  sram_stream_write_u32(g_ActiveSramStream, val);
}

void write16(u16 val) {
  sram_stream_write_u16(g_ActiveSramStream, val);
}

void write8(u8 val) {
  sram_stream_write_u8(g_ActiveSramStream, val);
}

void write_bytes(const void *src, u32 size) {
  sram_stream_write_bytes(g_ActiveSramStream, src, size);
}

u32 read32(void) {
  return sram_stream_read_u32(g_ActiveSramStream);
}

u16 read16(void) {
  return sram_stream_read_u16(g_ActiveSramStream);
}

u8 read8(void) {
  return sram_stream_read_u8(g_ActiveSramStream);
}

void read_bytes(void *dst, u32 size) {
  sram_stream_read_bytes(g_ActiveSramStream, dst, size);
}
