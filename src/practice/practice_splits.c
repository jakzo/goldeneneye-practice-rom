#include "practice_splits.h"
#include "bondinv.h"
#include "bondview.h"
#include "joy.h"
#include "player.h"
#include "practice_config.h"
#include "practice_ui.h"
#include <bondconstants.h>
#include <math.h>
#include <os_extension.h>
#include <ultra64.h>

extern s32 g_CurrentStageToLoad;
extern DIFFICULTY selected_difficulty;
extern s32 mission_timer;
extern s32 g_GlobalTimer;
extern s32 is_timer_active;

#define MAX_TIMER_TICKS (60 * 60 * 100 - 1)
#define MAX_SPLITS 32

// ---------------------------------------------------------------------------
// Split data definitions
// ---------------------------------------------------------------------------
//
// Each level+difficulty combo gets its own array of Split entries.
// Arrays are terminated by a { NULL, {{0,0},{0,0},{0,0},{0,0}} } sentinel.
//
// Coordinates are in GoldenEye world space (x,z for bird's eye view).
// Vertices should be ordered (CW or CCW) for the quad.
//
// To add splits for a new level+difficulty:
//   1. Define the Split array
//   2. Add an entry to g_SplitLists[]
//
// Use the trigger+C-down hotkey during gameplay to log Bond's position
// and capture coordinates for defining new splits.
// ---------------------------------------------------------------------------

// Sentinel entry to terminate split arrays
#define SPLIT_END {NULL, SPLIT_TYPE_NONE}

static const Split g_Runway_Agent_Splits[] = {
    {
        "Slide",
        SPLIT_TYPE_AREA,
        NULL,
        {
            {6635.0f, -5880.0f},
            {6350.0f, -5880.0f},
            {6350.0f, -6100.0f},
            {6635.0f, -6100.0f},
        },
    },
    {
        "Hut entry",
        SPLIT_TYPE_AREA,
        NULL,
        {
            {5630.0f, -7100.0f},
            {5630.0f, -6800.0f},
            {5500.0f, -6800.0f},
            {5500.0f, -7100.0f},
        },
    },
    {
        "Key pickup",
        SPLIT_TYPE_ITEM,
        "ignition key",
    },
    {
        "Hut exit",
        SPLIT_TYPE_AREA,
        NULL,
        {
            {5720.0f, -7100.0f},
            {5880.0f, -7100.0f},
            {5880.0f, -6800.0f},
            {5720.0f, -6800.0f},
        },
    },
    {
        "Before first boost",
        SPLIT_TYPE_AREA,
        NULL,
        {
            {7220.0f, -8470.0f},
            {6530.0f, -8760.0f},
            {6430.0f, -9320.0f},
            {7660.0f, -8920.0f},
        },
    },
    {
        "Turret",
        SPLIT_TYPE_AREA,
        NULL,
        {
            {6970.0f, -17920.0f},
            {11280.0f, -16670.0f},
            {10960.0f, -15600.0f},
            {7830.0f, -16500.0f},
        },
    },
    {
        "Plane",
        SPLIT_TYPE_AREA,
        NULL,
        {
            {10550.0f, -20360.0f},
            {9760.0f, -20780.0f},
            {10080.0f, -21520.0f},
            {11070.0f, -21340.0f},
        },
    },
    SPLIT_END,
};

// ---------------------------------------------------------------------------
// Master split list indexed by level+difficulty
// ---------------------------------------------------------------------------
static const SplitList g_SplitLists[] = {
    {LEVELID_RUNWAY, DIFFICULTY_AGENT, g_Runway_Agent_Splits},
    {-1, -1, NULL}, // sentinel
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
static const SplitList *s_CurrentSplitList = NULL;
static s32 s_CurrentSplitIndex = 0;
static s32 s_TotalSplits = 0;
static s32
    s_SplitCumulativeTimes[MAX_SPLITS]; // cumulative tick times per split
static s32 s_LevelSummaryLogged = FALSE;
static bool s_TriggerSplitNow = FALSE;

// ---------------------------------------------------------------------------
// Point-in-triangle check (same-side / cross-product test)
// Vertices v0, v1, v2 must be in consistent CW or CCW order.
// Uses only muls/subs — no divisions.
// ---------------------------------------------------------------------------
static s32 point_in_tri(f32 px, f32 pz, const SplitVertex *v0,
                        const SplitVertex *v1, const SplitVertex *v2) {
  f32 d0 = (v1->x - v0->x) * (v0->z - pz) - (v1->z - v0->z) * (v0->x - px);
  f32 d1 = (v2->x - v1->x) * (v1->z - pz) - (v2->z - v1->z) * (v1->x - px);
  f32 d2 = (v0->x - v2->x) * (v2->z - pz) - (v0->z - v2->z) * (v2->x - px);

  if (((d0 > 0.0f) && (d1 > 0.0f) && (d2 > 0.0f)) ||
      ((d0 < 0.0f) && (d1 < 0.0f) && (d2 < 0.0f))) {
    return TRUE;
  }
  return FALSE;
}

// ---------------------------------------------------------------------------
// Point-in-quad check (2-triangle same-side test)
// Faster than ray casting for convex quads: splits into (v0,v1,v2) and
// (v0,v2,v3)
// ---------------------------------------------------------------------------
static s32 point_in_quad(f32 px, f32 pz, const SplitVertex *verts) {
  if (point_in_tri(px, pz, &verts[0], &verts[1], &verts[2])) {
    return TRUE;
  }
  return point_in_tri(px, pz, &verts[0], &verts[2], &verts[3]);
}

// ---------------------------------------------------------------------------
// Point-in-polygon dispatch
//  n==3  -> triangle test (fast, no divs)
//  n==4  -> quad test (2 triangles, no divs)
//  n>4   -> ray casting (works for any simple polygon)
// ---------------------------------------------------------------------------
static s32 point_in_polygon(f32 px, f32 pz, const SplitVertex *verts, s32 n) {
  s32 i;
  s32 j;
  s32 inside;
  s32 vi_outside;
  f32 x_intersect;

  if (n == 3) {
    return point_in_tri(px, pz, &verts[0], &verts[1], &verts[2]);
  }
  if (n == 4) {
    return point_in_quad(px, pz, verts);
  }

  // Ray casting / even-odd rule for arbitrary polygons
  inside = 0;
  j = n - 1;
  for (i = 0; i < n; j = i, i++) {
    vi_outside = (verts[i].z > pz) != (pz > verts[j].z);
    if (vi_outside) {
      x_intersect = verts[i].x + (pz - verts[i].z) / (verts[j].z - verts[i].z) *
                                     (verts[j].x - verts[i].x);
      if (px < x_intersect) {
        inside = !inside;
      }
    }
  }
  return inside;
}

// ---------------------------------------------------------------------------
// Format mission timer (in 60Hz ticks) to "M:SS.cc" string
// ---------------------------------------------------------------------------
static void format_time(s32 ticks, char *buf, bool always_show_minutes) {
  s32 minutes;
  s32 seconds;
  s32 hundredths;
  s32 pos = 0;

  if (ticks > MAX_TIMER_TICKS) {
    ticks = MAX_TIMER_TICKS;
  }

  minutes = ticks / 60 / 60;
  seconds = ticks / 60 % 60;
  hundredths = (ticks % 60) * 100 / 60;

  if (always_show_minutes || minutes > 0) {
    // Minutes (no leading zero)
    if (minutes >= 10) {
      buf[pos++] = '0' + minutes / 10;
      minutes %= 10;
    }
    buf[pos++] = '0' + minutes;

    buf[pos++] = ':';
  }

  // Seconds (always 2 digits)
  buf[pos++] = '0' + seconds / 10;
  buf[pos++] = '0' + seconds % 10;

  buf[pos++] = '.';

  // Hundredths (always 2 digits)
  buf[pos++] = '0' + hundredths / 10;
  buf[pos++] = '0' + hundredths % 10;

  buf[pos] = '\0';
}

// ---------------------------------------------------------------------------
// Find the split list for the current level+difficulty
// ---------------------------------------------------------------------------
static void splits_find_current_list(void) {
  s32 i;
  s32 level;
  s32 diff;
  const Split *sp;

  level = g_CurrentStageToLoad;
  diff = selected_difficulty;

  s_CurrentSplitList = NULL;
  s_CurrentSplitIndex = 0;
  s_TotalSplits = 0;
  s_LevelSummaryLogged = FALSE;

  for (i = 0; g_SplitLists[i].splits != NULL; i++) {
    if (g_SplitLists[i].level_id == level &&
        g_SplitLists[i].difficulty == diff) {
      s_CurrentSplitList = &g_SplitLists[i];
      // Count total splits
      sp = g_SplitLists[i].splits;
      while (sp->name != NULL) {
        s_TotalSplits++;
        sp++;
      }
      return;
    }
  }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void splits_init(void) { splits_find_current_list(); }

void trigger_split() {
  char time_buf[16];
  char delta_buf[16];
  s32 last_split_time = s_CurrentSplitIndex > 0
                            ? s_SplitCumulativeTimes[s_CurrentSplitIndex - 1]
                            : 0;
  s32 delta = mission_timer - last_split_time;

  if (s_CurrentSplitIndex < MAX_SPLITS) {
    s_SplitCumulativeTimes[s_CurrentSplitIndex] = mission_timer;
  }

  // Log the split
  {
    const Split *split = &s_CurrentSplitList->splits[s_CurrentSplitIndex];
    format_time(mission_timer, time_buf, TRUE);
    format_time(delta, delta_buf, FALSE);
    practiceLogInfo("SPLIT %d/%d: %s - %s (delta: %s)", s_CurrentSplitIndex + 1,
                    s_TotalSplits, split->name, time_buf, delta_buf);
  }

  s_CurrentSplitIndex++;
}

void splits_tick(void) {
  s32 i;
  s32 current_time;
  s32 delta;
  s32 prev_time;
  const Split *next_split;
  const Split *split;
  PropRecord *bond;

  if (!practice.splits_enabled)
    return;

  // If no split list is loaded for current level+difficulty, try to find one
  if (s_CurrentSplitList == NULL) {
    splits_find_current_list();
    if (s_CurrentSplitList == NULL)
      return;
  }

  // Check if the ending cutscene is playing and log a level summary
  if (!s_LevelSummaryLogged && s_CurrentSplitIndex > 0 &&
      g_CameraMode == CAMERAMODE_POSEND) {
    s_LevelSummaryLogged = TRUE;
    return;
  }

  // If all splits have been triggered, nothing more to do
  if (s_CurrentSplitIndex >= s_TotalSplits) {
    return;
  }

  bond = get_curplayer_positiondata();
  if (bond == NULL)
    return;

  // Check if the split is triggered
  next_split = &s_CurrentSplitList->splits[s_CurrentSplitIndex];

  if (next_split->type == SPLIT_TYPE_AREA &&
      point_in_quad(bond->pos.x, bond->pos.z, next_split->vertices)) {
    trigger_split();
  }
}

void splits_log_position(void) {
  PropRecord *prop = get_curplayer_positiondata();
  if (prop != NULL) {
    practiceLogDebug("Bond pos: x=%.1f y=%.1f z=%.1f", prop->pos.x, prop->pos.y,
                     prop->pos.z);
  } else {
    practiceLogDebug("Bond pos: (no player data)");
  }
}

// Extern declarations for menu rendering
extern Gfx *frontPrintText(Gfx *gdl, s32 *x, s32 *y, s8 *text,
                           s32 second_font_table, s32 first_font_table,
                           s32 arg6, s32 view_x, s32 view_y, s32 arg9,
                           s32 arga);
extern s16 viGetX(void);
extern s16 viGetY(void);
extern s32 ptrFontZurichBold;
extern s32 ptrFontZurichBoldChars;
extern int sprintf(char *dst, const char *fmt, ...);

s32 splits_have_final(void) {
  return practice.splits_enabled && s_CurrentSplitList != NULL;
}

Gfx *splits_render_final(Gfx *DL) {
  s32 i;
  s32 x;
  s32 y;
  s32 prev_time = 0;
  s32 y_spacing;

  if (s_TotalSplits == 0) {
    x = 55;
    y = 220;
    DL = frontPrintText(DL, &x, &y, (s8 *)"No splits configured",
                        ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF,
                        viGetX(), viGetY(), 0, 0);
    return DL;
  }

  y_spacing = 13;
  y = 204;

  for (i = 0; i < s_TotalSplits; i++) {
    const Split *split = &s_CurrentSplitList->splits[i];
    char name_buf[32];
    char time_buf[16];
    char delta_buf[24];
    s32 x_pos;

    // Print split number and name
    sprintf(name_buf, "%d. %s", i + 1, split->name);
    x_pos = 55;
    DL = frontPrintText(DL, &x_pos, &y, (s8 *)name_buf, ptrFontZurichBoldChars,
                        ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);

    if (i < s_CurrentSplitIndex) {
      s32 cumulative = s_SplitCumulativeTimes[i];
      s32 delta = cumulative - prev_time;
      format_time(cumulative, time_buf, TRUE);
      format_time(delta, delta_buf, FALSE);
      prev_time = cumulative;

      // Print cumulative time
      x_pos = 240;
      DL =
          frontPrintText(DL, &x_pos, &y, (s8 *)time_buf, ptrFontZurichBoldChars,
                         ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);

      // Print delta
      sprintf(name_buf, "(+%s)", delta_buf);
      x_pos = 310;
      DL =
          frontPrintText(DL, &x_pos, &y, (s8 *)name_buf, ptrFontZurichBoldChars,
                         ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);
    } else {
      x_pos = 240;
      DL = frontPrintText(DL, &x_pos, &y, (s8 *)"-:--.--",
                          ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF,
                          viGetX(), viGetY(), 0, 0);
    }
    y += y_spacing;
  }

  return DL;
}

extern char *langGet(s32 id);

static bool item_names_match(const char *inventoryName, const char *splitItemName) {
  s32 i;
  for (i = 0; i < 64; i++) {
    char a = inventoryName[i];
    char b = splitItemName[i];
    // Case-insensitive compare, string must only contain A-Z and space
    if ((a | 0x20) != (b | 0x20)) {
      // Item names end with \n for some reason
      return a == '\n' && b == '\0';
    }
    if (a == '\0') {
      return TRUE;
    }
  }
  return FALSE;
}

void split_if_necessary_on_item_pickup(PropRecord *prop) {
  const Split *split;
  textoverride *override;

  if (!s_CurrentSplitList || s_CurrentSplitIndex >= s_TotalSplits)
    return;

  split = &s_CurrentSplitList->splits[s_CurrentSplitIndex];
  if (split->type != SPLIT_TYPE_ITEM)
    return;

  override = bondinvGetTextbyObj(prop->obj);
  if (!override || !override->shorttext)
    return;

  if (item_names_match(langGet(override->shorttext), split->item))
    trigger_split();
}
