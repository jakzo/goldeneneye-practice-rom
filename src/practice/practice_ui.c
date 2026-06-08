#include "practice_ui.h"
#include "game/textrelated.h"
#include "practice_config.h"
#include <PR/os.h>
#include <bondconstants.h>
#include <bondtypes.h>
#include <fr.h>
#include <stdarg.h>
#include <ultra64.h>

// Bank Gothic: compact in-game HUD font
extern s32 ptrFontBankGothic;
extern s32 ptrFontBankGothicChars;
#define LOGGER_FONT_TABLE ptrFontBankGothic
#define LOGGER_FONT_CHARS ptrFontBankGothicChars

// Zurich Bold: large, bold menu font
// extern s32 ptrFontZurichBold;
// extern s32 ptrFontZurichBoldChars;
// #define LOGGER_FONT_TABLE ptrFontZurichBold
// #define LOGGER_FONT_CHARS ptrFontZurichBoldChars

extern Gfx *draw_blackbox_to_screen(Gfx *glist, s32 *ulx, s32 *uly, s32 *lrx,
                                    s32 *lry);

// Externs for sprintf helper
extern s32 _Printf(char *(*pfn)(char *, const char *, size_t), char *,
                   const char *, va_list);
extern void *memcpy(void *dst, const void *src, size_t count);

#define MAX_LOG_MESSAGES 32
#define LOG_MESSAGE_MAX_LEN 256
#define LOG_MESSAGE_DISPLAY_TIME_S 2.0f
#define SLIDE_RATE_PER_S 640.0f
#define SLIDE_DURATION 1.0f
#define MARGIN_BOTTOM 9
#define MARGIN_RIGHT 5
#define LINE_SPACING 0

typedef enum {
  LOG_LEVEL_DEBUG,
  LOG_LEVEL_INFO,
  LOG_LEVEL_WARN,
  LOG_LEVEL_ERROR
} LogLevel;

typedef struct {
  char text[LOG_MESSAGE_MAX_LEN];
  LogLevel level;
  OSTime timestamp;
  s32 width;
  s32 height;
} LogMessage;

static LogMessage g_LogQueue[MAX_LOG_MESSAGES];
static s32 g_LogQueueStart = 0;
static s32 g_LogQueueCount = 0;

// Cached timing constants to prevent redundant tick calculations
static OSTime g_LogLifetimeCycles = 0;
static OSTime g_LogSlideCycles = 0;
static f32 g_LogSlideRate = 0.0f;

static void ensure_timing_initialized(void) {
  if (g_LogLifetimeCycles != 0)
    return;
  g_LogLifetimeCycles = (OSTime)(osClockRate * LOG_MESSAGE_DISPLAY_TIME_S);
  g_LogSlideCycles = (OSTime)(osClockRate * SLIDE_DURATION);
  g_LogSlideRate = SLIDE_RATE_PER_S / (f32)osClockRate;
}

// Culls the oldest N messages from the circular queue
static void cull_queue(s32 count) {
  g_LogQueueStart = (g_LogQueueStart + count) % MAX_LOG_MESSAGES;
  g_LogQueueCount -= count;
}

typedef struct {
  char *dest;
  size_t remaining;
} SnprintfBuffer;

static char *proutSnprintf(char *dst_ptr, const char *src, size_t count) {
  SnprintfBuffer *buf = (SnprintfBuffer *)dst_ptr;
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

static int practice_vsnprintf(char *dst, const char *fmt, va_list args) {
  SnprintfBuffer buf;

  buf.dest = dst;
  buf.remaining =
      LOG_MESSAGE_MAX_LEN - 2; // reserve 2 for newline and null terminator

  _Printf(proutSnprintf, (char *)&buf, fmt, args);

  *buf.dest = '\0';
  return (LOG_MESSAGE_MAX_LEN - 3) - buf.remaining;
}

static void add_to_log_queue(LogLevel level, const char *fmt, va_list args) {
  LogMessage *msg;
  s32 write_idx;
  s32 len;

  // If queue is full, discard the oldest message by moving start forward
  if (g_LogQueueCount >= MAX_LOG_MESSAGES) {
    cull_queue(1);
  }

  // Determine the next write slot based on start index and count
  write_idx = (g_LogQueueStart + g_LogQueueCount) % MAX_LOG_MESSAGES;
  msg = &g_LogQueue[write_idx];

  msg->level = level;
  msg->timestamp = osGetTime();

  // Format directly into the queue message text buffer safely
  len = practice_vsnprintf(msg->text, fmt, args);

  // Ensure string ends with a newline so textMeasure calculates height
  // correctly
  msg->text[len] = '\n';
  msg->text[len + 1] = '\0';

  msg->width = -1;
  msg->height = -1;

  g_LogQueueCount++;
}

void practiceLogDebug(const char *fmt, ...) {
  va_list args;
  if (!practice.show_debug_logs)
    return;
  va_start(args, fmt);
  add_to_log_queue(LOG_LEVEL_DEBUG, fmt, args);
  va_end(args);
}

void practiceLogInfo(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  add_to_log_queue(LOG_LEVEL_INFO, fmt, args);
  va_end(args);
}

void practiceLogWarn(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  add_to_log_queue(LOG_LEVEL_WARN, fmt, args);
  va_end(args);
}

void practiceLogError(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  add_to_log_queue(LOG_LEVEL_ERROR, fmt, args);
  va_end(args);
}

Gfx *practice_ui_render(Gfx *gdl) {
  s32 i;
  s32 current_y;
  s32 right_edge;
  OSTime current_time;

  gdl = microcode_constructor(gdl);

  // Reset the scissor box to cover the entire screen so we are not clipped by
  // viewports
  gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());

  // Render green "P" indicator at bottom-left of the visible area
  {
    struct fontchar *fontChars = ptrFontZurichBoldChars;
    struct fontchar *charP = &fontChars['P'];
    s32 p_x = MARGIN_RIGHT;
    s32 p_y = viGetY() - charP->baseline - charP->height - MARGIN_BOTTOM;
    gdl = textRenderGlow(gdl, &p_x, &p_y, "P", fontChars, ptrFontZurichBold,
                         0x00FF00FF, 0x000000FF, (s16)viGetX(), (s16)viGetY(),
                         0, 0);
  }

  if (g_LogQueueCount > 0) {
    ensure_timing_initialized();

    // Position it right-aligned at the bottom-right of the entire screen
    right_edge = viGetX() - MARGIN_RIGHT;
    current_y = viGetY() - MARGIN_BOTTOM;
    current_time = osGetTime();

    for (i = g_LogQueueCount - 1; i >= 0; i--) {
      s32 idx = (g_LogQueueStart + i) % MAX_LOG_MESSAGES;
      LogMessage *msg = &g_LogQueue[idx];
      s32 y_bottom = current_y;
      s32 y_top;
      OSTime age_cycles;
      f32 slide_offset = 0.0f;
      s32 view_left;
      s32 view_horiz;
      s32 view_vert;
      s32 view_top;
      u32 color_fg = 0xFFFFFFFF;
      u32 color_glow = 0x000000FF;

      if (msg->width < 0) {
        textMeasure(&msg->height, &msg->width, msg->text,
                    (struct fontchar *)LOGGER_FONT_CHARS,
                    (struct font *)LOGGER_FONT_TABLE, 0);
      }

      y_top = y_bottom - msg->height;

      if (y_bottom < 0) {
        cull_queue(i + 1);
        break;
      }

      age_cycles = current_time - msg->timestamp;
      if (age_cycles > g_LogLifetimeCycles) {
        OSTime slide_time_cycles = age_cycles - g_LogLifetimeCycles;
        if (slide_time_cycles >= g_LogSlideCycles) {
          cull_queue(i + 1);
          break;
        }
        slide_offset = (f32)(s32)slide_time_cycles * g_LogSlideRate;
      }

      view_left = right_edge - msg->width + (s32)slide_offset;
      view_horiz = right_edge + (s32)slide_offset;
      view_vert = y_bottom - msg->height;
      view_top = y_bottom;

      // Draw dark background box
      gdl = draw_blackbox_to_screen(gdl, &view_left, &view_vert, &view_horiz,
                                    &view_top);

      // Determine colors based on log level
      switch (msg->level) {
      case LOG_LEVEL_DEBUG:
        color_fg = 0xA0A0A0FF; // Grey
        break;
      case LOG_LEVEL_WARN:
        color_fg = 0xFFFF00FF; // Yellow
        break;
      case LOG_LEVEL_ERROR:
        color_fg = 0xFF0000FF; // Red
        break;
      case LOG_LEVEL_INFO:
      default:
        color_fg = 0xFFFFFFFF; // White
        break;
      }

      // Render text with glow
      gdl = textRenderGlow(gdl, &view_left, &view_vert, msg->text,
                           LOGGER_FONT_CHARS, LOGGER_FONT_TABLE, (s32)color_fg,
                           color_glow, (s16)(s32)viGetX(), (s16)viGetY(), 0, 0);

      current_y = y_top - LINE_SPACING; // Move up for the next message
    }
  }

  gdl = combiner_bayer_lod_perspective(gdl);

  return gdl;
}
