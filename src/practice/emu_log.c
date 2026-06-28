/**
 * emu_log.c
 *
 * Implements logging via USB (real hardware) or ISViewer (emulators).
 *
 * Detection order at init:
 *   1. USB: probes for SummerCart64, 64Drive, or EverDrive via PI-bus
 *      register checks. If found, logs are sent over USB using the UNFLoader
 *      protocol (compatible with UNFLoader, g64drive, etc.).
 *   2. ISViewer: if no USB cart is detected, probes for an emulated ISViewer
 *      at 0x13FF0000 (cen64 -is-viewer, ares, simple64, dgb-n64).
 *
 * This mirrors libdragon's approach where both channels can be active
 * simultaneously.
 */

#include "emu_log.h"
#include "../usb.h" /* usb_initialize, usb_write, DATATYPE_TEXT */
#include <PR/rcp.h> /* IO_WRITE, IO_READ, PHYS_TO_K1 */
#include <stdarg.h>
#include <string.h>
#include <ultra64.h>

/* ------------------------------------------------------------------ */
/* ISViewer physical addresses (as seen by the RCP / PI bus)          */
/* ------------------------------------------------------------------ */
#define ISVIEWER_PHYS_MAGIC 0x13FF0000
#define ISVIEWER_PHYS_WRITE_PTR 0x13FF0014
#define ISVIEWER_PHYS_BUFFER 0x13FF0020
#define ISVIEWER_BUFFER_LEN 512 /* typical emulated buffer size */

/* Magic value written to ISVIEWER_PHYS_MAGIC to detect presence.
 * Using a value different from the real ISViewer's default to avoid
 * false positives on real hardware that happens to have that value
 * sitting in memory. */
#define ISVIEWER_MAGIC_VALUE 0x12345678

/* ------------------------------------------------------------------ */
/* State                                                              */
/* ------------------------------------------------------------------ */
static int g_emu_log_initialized = 0;
static int g_usb_available = 0; /* non-zero if USB cart detected */
static int g_isv_available = 0; /* non-zero if ISViewer detected */

/* ------------------------------------------------------------------ */
/* ISViewer probe                                                     */
/* ------------------------------------------------------------------ */
static int isviewer_probe(void) {
  u32 test;

  /*
   * iQue (iQue Player / 64DD emulation) doesn't have the PI
   * address space mapped this way.  We could check sys_bbplayer()
   * if it were available, but this project doesn't have that
   * function.  We just let the probe fail gracefully.
   */

  /* Write inverted magic and verify it sticks */
  IO_WRITE(ISVIEWER_PHYS_MAGIC, ~ISVIEWER_MAGIC_VALUE);
  test = IO_READ(ISVIEWER_PHYS_MAGIC);
  if (test != ~ISVIEWER_MAGIC_VALUE)
    return 0;

  /* Write real magic and verify */
  IO_WRITE(ISVIEWER_PHYS_MAGIC, ISVIEWER_MAGIC_VALUE);
  test = IO_READ(ISVIEWER_PHYS_MAGIC);
  if (test != ISVIEWER_MAGIC_VALUE)
    return 0;

  return 1;
}

/* ------------------------------------------------------------------ */
/* ISViewer writer                                                    */
/* ------------------------------------------------------------------ */
static void isviewer_write(const char *text) {
  int len;
  int offset;

  if (!text || text[0] == '\0')
    return;

  len = 0;
  while (text[len] != '\0')
    len++;

  /*
   * Write data to the ISViewer buffer in 32-bit big-endian words.
   * This matches what emulators expect (N64 is big-endian).
   */
  offset = 0;
  while (len > 0) {
    int chunk = (len < ISVIEWER_BUFFER_LEN) ? len : ISVIEWER_BUFFER_LEN;
    int i;

    /* Write 32-bit big-endian words (N64 native endianness) */
    for (i = 0; i < chunk; i += 4) {
      u32 word = 0;
      int b;

      for (b = 0; b < 4 && (i + b) < chunk; b++)
        word |= ((u32)(unsigned char)text[offset + i + b]) << (24 - b * 8);

      IO_WRITE(ISVIEWER_PHYS_BUFFER + i, word);
    }

    /* Flush this chunk: write the length to the write-pointer register.
     * Emulators interpret this as a "data ready" signal. */
    IO_WRITE(ISVIEWER_PHYS_WRITE_PTR, chunk);

    offset += chunk;
    len -= chunk;
  }
}

/* ------------------------------------------------------------------ */
/* USB writer — delegates to the existing src/usb.c stack             */
/* ------------------------------------------------------------------ */
static void usb_log_write(const char *text) {
  int len;

  if (!text || text[0] == '\0')
    return;

  len = 0;
  while (text[len] != '\0')
    len++;

  /* Use the existing UNFLoader-compatible USB write with DATATYPE_TEXT.
   * The underlying usb_sc64_write / usb_64drive_write / usb_everdrive_write
   * handles DMA'ing the data to SDRAM and issuing the cart's USB command. */
  usb_write(DATATYPE_TEXT, text, len);
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */
void emu_log_init(void) {
  if (g_emu_log_initialized)
    return;
  g_emu_log_initialized = 1;

  /* 1. Try USB cart detection first (real hardware) */
  if (usb_initialize()) {
    g_usb_available = 1;
    /* Don't return yet — also probe ISViewer so both channels can
     * work (though on real hardware ISViewer won't be present). */
  }

  /* 2. Try ISViewer probe (emulators) */
  if (isviewer_probe()) {
    g_isv_available = 1;
  }
}

void emu_log_write(const char *text) {
  if (!text || text[0] == '\0')
    return;

  /* Auto-init on first use */
  if (!g_emu_log_initialized)
    emu_log_init();

  /* Send via USB if a flashcart is present */
  if (g_usb_available)
    usb_log_write(text);

  /* Send via ISViewer if available (emulators) */
  if (g_isv_available)
    isviewer_write(text);
}

/*
 * The IDO compiler used for this project does not support variadic macros
 * or the C99 standard printf family.  We use _Printf (the IDO internal
 * printf engine) through a custom writer callback, which is already done
 * elsewhere in the codebase.
 */
extern s32 _Printf(char *(*pfn)(char *, const char *, size_t), char *,
                   const char *, va_list);
extern void *memcpy(void *dst, const void *src, size_t count);

typedef struct {
  char *dest;
  size_t remaining;
} BufWriter;

static char *proutBufWriter(char *dst_ptr, const char *src, size_t count) {
  BufWriter *buf = (BufWriter *)dst_ptr;
  if (buf->remaining > 0) {
    size_t copy_count = count;
    if (copy_count > buf->remaining) {
      copy_count = buf->remaining;
    }
    memcpy(buf->dest, src, copy_count);
    buf->dest += copy_count;
    buf->remaining -= copy_count;
  }
  return dst_ptr;
}

void emu_log(const char *fmt, ...) {
  char buf[512];
  va_list args;
  BufWriter bw;

  if (!g_emu_log_initialized)
    emu_log_init();

  if (!g_usb_available && !g_isv_available)
    return;

  bw.dest = buf;
  bw.remaining = sizeof(buf) - 2;

  va_start(args, fmt);
  _Printf(proutBufWriter, (char *)&bw, fmt, args);
  va_end(args);

  bw.dest--;
  *bw.dest = '\n';
  bw.dest++;
  *bw.dest = '\0';
  emu_log_write(buf);
}
