#include "practice_ui.h"
#include "game/bondview.h"
#include "game/textrelated.h"
#include "player.h"
#include "practice_config.h"
#include <PR/os.h>
#include <bondconstants.h>
#include "emu_log.h"
#include <bondtypes.h>
#include <fr.h>
#include <stdarg.h>
#include <ultra64.h>

typedef struct {
  u32 start;
  u32 end;
  const char *name;
} UnknownRange;

static UnknownRange g_UnknownRanges[] = {
    {0x94, 0x9C, "field_94_to_98"},
    {0xFC, 0x104, "field_FC_to_100"},
    {0x108, 0x10C, "field_108"},
    {0x1A0, 0x1C0, "field_1A0_to_1BC"},
    {0x42C, 0x430, "field_42C"},
    {0x1068, 0x106C, "field_1068"},
    {0x112C, 0x1130, "field_112C"},
    {0x1280, 0x128C, "field_1280_to_1288"},
    {0x1B20, 0x2128, "field_1B20_to_2124"},
    {0x2250, 0x2858, "field_2250_to_2854"},
    {0x28E0, 0x2998, "field_28E0_to_2994"},
    {0x29B0, 0x29B8, "field_29B0_to_29B4"},
    {0x2A74, 0x2A80, "field_2A74_to_2A7C"},
};

static u8 g_PrevPlayerState[sizeof(struct player)];
static u8 g_PlayerStateInitialized = 0;

static void practice_monitor_unknown_fields(void) {
  u8 *curr;
  s32 num_ranges;
  s32 i;
  UnknownRange range;
  u32 offset;
  u32 curr_val;
  u32 prev_val;

  if (g_CurrentPlayer == NULL) {
    g_PlayerStateInitialized = 0;
    return;
  }

  curr = (u8 *)g_CurrentPlayer;
  num_ranges = sizeof(g_UnknownRanges) / sizeof(UnknownRange);

  if (!g_PlayerStateInitialized) {
    for (i = 0; i < num_ranges; i++) {
      range = g_UnknownRanges[i];
      for (offset = range.start; offset < range.end; offset += 4) {
        *(u32 *)(g_PrevPlayerState + offset) = *(u32 *)(curr + offset);
      }
    }
    g_PlayerStateInitialized = 1;
    return;
  }

  for (i = 0; i < num_ranges; i++) {
    range = g_UnknownRanges[i];
    for (offset = range.start; offset < range.end; offset += 4) {
      curr_val = *(u32 *)(curr + offset);
      prev_val = *(u32 *)(g_PrevPlayerState + offset);

      if (curr_val != prev_val) {
        *(u32 *)(g_PrevPlayerState + offset) = curr_val;
        // Log warning to screen with previous and current values
        practiceLogWarn("Field 0x%X: 0x%08X -> 0x%08X", offset, prev_val,
                        curr_val);
      }
    }
  }
}

extern int sprintf(char *dst, const char *fmt, ...);

// Bank Gothic: compact in-game HUD font
#define LOGGER_FONT_TABLE ptrFontBankGothic
#define LOGGER_FONT_CHARS ptrFontBankGothicChars

// Zurich Bold: large, bold menu font
// #define LOGGER_FONT_TABLE ptrFontZurichBold
// #define LOGGER_FONT_CHARS ptrFontZurichBoldChars

extern Gfx *draw_blackbox_to_screen(Gfx *glist, s32 *ulx, s32 *uly, s32 *lrx,
                                    s32 *lry);

// Externs for sprintf helper
extern s32 _Printf(char *(*pfn)(char *, const char *, size_t), char *,
                   const char *, va_list);
extern void *memcpy(void *dst, const void *src, size_t count);

#define MAX_LOG_MESSAGES 20
#define LOG_MESSAGE_MAX_LEN 256
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
  g_LogLifetimeCycles = (OSTime)(s32)((f32)(s32)osClockRate * practice.log_message_duration);
  g_LogSlideCycles = (OSTime)(s32)((f32)(s32)osClockRate * SLIDE_DURATION);
  g_LogSlideRate = SLIDE_RATE_PER_S / (f32)(s32)osClockRate;
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

  // Also forward to emulator log (ISViewer protocol)
  emu_log_write(msg->text);
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

// Note that rendering text with outline causes the text to be rendered 9x
#define RENDER_TEXT_WITH_OUTLINE TRUE

Gfx *renderText(Gfx *gdl, s32 *x, s32 *y, char *text, s32 fontChars, s32 font,
                u32 color, s16 viewX, s16 viewY) {
#if RENDER_TEXT_WITH_OUTLINE
  return textRenderGlow(gdl, x, y, text, fontChars, font, (s32)color,
                        0x000000FF, viewX, viewY, 0, 0);
#else
  return textRender(gdl, x, y, text, fontChars, font, (s32)color, viewX, viewY,
                    0, 0);
#endif
}

Gfx *practice_ui_render(Gfx *gdl) {
  s32 i;
  s32 current_y;
  s32 right_edge;
  OSTime current_time;

  // practice_monitor_unknown_fields();

  gdl = microcode_constructor(gdl);

  // Reset the scissor box to cover the entire screen so we are not clipped by
  // viewports
  gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, viGetX(), viGetY());

  // Render green "P" indicator at bottom-left of the visible area
  {
    struct fontchar *fontCharsZurich = (struct fontchar *)ptrFontZurichBoldChars;
    struct fontchar *charP = &fontCharsZurich['P'];
    s32 p_x = MARGIN_RIGHT;
    s32 p_y = viGetY() - charP->baseline - charP->height - MARGIN_BOTTOM;
    gdl = renderText(gdl, &p_x, &p_y, "P", (s32)fontCharsZurich, ptrFontZurichBold,
                     0x00CC00FF, viGetX(), viGetY());

    // Render mission timer next to the "P" indicator
    if (practice.show_mission_timer) {
      char timer_buf[16];
      // Position timer to the right of the "P" with a small gap
      s32 timer_x = p_x + 8;
      s32 timer_y = p_y;
      s32 missionTime = getMissiontimer();
      s32 minutes = missionTime / 60 / 60;
      s32 seconds = missionTime / 60 % 60;
      s32 hundredths = (missionTime % 60) * 100 / 60;
      s32 color = is_timer_active ? 0xFFFFFFFF : 0xA0A0A0FF;

      sprintf(timer_buf, "%d:%02d.%02d", minutes, seconds, hundredths);
      gdl =
          renderText(gdl, &timer_x, &timer_y, timer_buf, ptrFontBankGothicChars,
                     ptrFontBankGothic, color, viGetX(), viGetY());
    }
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

      if (y_bottom < 0) {
        cull_queue(i + 1);
        break;
      }

      if (msg->width < 0) {
        textMeasure(&msg->height, &msg->width, msg->text,
                    (struct fontchar *)LOGGER_FONT_CHARS,
                    (struct font *)LOGGER_FONT_TABLE, 0);
      }

      y_top = y_bottom - msg->height;

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

      // Render text
      gdl =
          renderText(gdl, &view_left, &view_vert, msg->text, LOGGER_FONT_CHARS,
                     LOGGER_FONT_TABLE, color_fg, viGetX(), viGetY());
      // textRender(gdl, &view_left, &view_vert, msg->text, LOGGER_FONT_CHARS,
      //            LOGGER_FONT_TABLE, color_fg, viGetX(), viGetY(), 0, 0);

      current_y = y_top - LINE_SPACING; // Move up for the next message
    }
  }

  gdl = combiner_bayer_lod_perspective(gdl);

  return gdl;
}
