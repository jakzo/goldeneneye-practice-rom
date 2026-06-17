#include "practice_splits.h"
#include "bondview.h"
#include "joy.h"
#include "player.h"
#include "practice_config.h"
#include "practice_ui.h"
#include <bondconstants.h>
#include <os_extension.h>
#include <ultra64.h>

#include <math.h>

// Forward declarations for game globals not exposed in headers
extern s32 g_CurrentStageToLoad;
extern int bondinvHasInvItem(ITEM_IDS item);
extern bool bondinvCheckHasKeyFlags(u32 wantkeyflags);

static s32 splits_check_item_held(s32 item_id) {
  InvItem *first;
  InvItem *item;

  if (bondinvHasInvItem((ITEM_IDS)item_id)) {
    return TRUE;
  }

  if (g_CurrentPlayer == NULL) {
    return FALSE;
  }

  first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
  item = first;
  while (item != NULL) {
    if (item->type == INV_ITEM_PROP) {
      PropRecord *prop = item->type_inv_item.type_prop.prop;
      if (prop != NULL) {
        if (prop->type == PROP_TYPE_WEAPON) {
          WeaponObjRecord *weapon = (WeaponObjRecord *)prop->obj;
          if (weapon != NULL && weapon->weaponnum == item_id) {
            return TRUE;
          }
        } else if (prop->type == PROP_TYPE_OBJ) {
          ObjectRecord *obj = prop->obj;
          if (obj != NULL && obj->type == PROPDEF_COLLECTABLE) {
            WeaponObjRecord *weapon = (WeaponObjRecord *)obj;
            if (weapon->weaponnum == item_id) {
              return TRUE;
            }
          }
        }
      }
    }
    item = item->next;
    if (item == first) {
      break;
    }
  }

  return FALSE;
}
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
// Each level+difficulty combo gets its own array of SplitArea entries.
// Arrays are terminated by a { NULL, {{0,0},{0,0},{0,0},{0,0}} } sentinel.
//
// Coordinates are in GoldenEye world space (x,z for bird's eye view).
// Vertices should be ordered (CW or CCW) for the quad.
//
// To add splits for a new level+difficulty:
//   1. Define the SplitArea array
//   2. Add an entry to g_SplitLists[]
//
// Use the trigger+C-down hotkey during gameplay to log Bond's position
// and capture coordinates for defining new splits.
// ---------------------------------------------------------------------------

// Sentinel entry to terminate split arrays
#define SPLIT_END                                                              \
  {                                                                            \
    NULL, { {0, 0}, {0, 0}, {0, 0}, {0, 0} }                                   \
  }

static const SplitArea g_Runway_Agent_Splits[] = {
    {
        "Slide",
        {
            {6635.0f, -5880.0f},
            {6350.0f, -5880.0f},
            {6350.0f, -6100.0f},
            {6635.0f, -6100.0f},
        },
    },
    {
        "Hut entry",
        {
            {5630.0f, -7100.0f},
            {5630.0f, -6800.0f},
            {5500.0f, -6800.0f},
            {5500.0f, -7100.0f},
        },
    },
    {
        "Key pickup",
        { {0,0}, {0,0}, {0,0}, {0,0} },
        SPLIT_TYPE_KEY,
        0,
        1
    },
    {
        "Hut exit",
        {
            {5720.0f, -7100.0f},
            {5880.0f, -7100.0f},
            {5880.0f, -6800.0f},
            {5720.0f, -6800.0f},
        },
    },
    {
        "Before first boost",
        {
            {7220.0f, -8470.0f},
            {6530.0f, -8760.0f},
            {6430.0f, -9320.0f},
            {7660.0f, -8920.0f},
        },
    },
    {
        "Turret",
        {
            {6970.0f, -17920.0f},
            {11280.0f, -16670.0f},
            {10960.0f, -15600.0f},
            {7830.0f, -16500.0f},
        },
    },
    {
        "Plane",
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
static s32 s_NextSplitIndex = 0;
static s32 s_TotalSplits = 0;
static s32 s_SplitsTriggered = 0;
static s32 s_LastMissionTimer = 0; // for computing delta times
static s32
    s_SplitCumulativeTimes[MAX_SPLITS]; // cumulative tick times per split
static s32 s_LevelSummaryLogged = FALSE;

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
  const SplitArea *sp;

  level = g_CurrentStageToLoad;
  diff = selected_difficulty;

  s_CurrentSplitList = NULL;
  s_NextSplitIndex = 0;
  s_TotalSplits = 0;
  s_SplitsTriggered = 0;
  s_LastMissionTimer = 0;
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

void splits_tick(void) {
  s32 i;
  s32 current_time;
  s32 delta;
  s32 prev_time;
  s32 triggered;
  char time_buf[16];
  char delta_buf[16];
  const SplitArea *next_split;
  const SplitArea *split;
  PropRecord *prop;
  f32 bond_x;
  f32 bond_z;

  if (!practice.splits_enabled) {
    return;
  }

  // If no split list is loaded for current level+difficulty, try to find one
  if (s_CurrentSplitList == NULL) {
    splits_find_current_list();
    if (s_CurrentSplitList == NULL) {
      return;
    }
  }

  // Check if the ending cutscene is playing and log a level summary
  if (!s_LevelSummaryLogged && s_SplitsTriggered > 0 &&
      g_CameraMode == CAMERAMODE_POSEND) {
    s_LevelSummaryLogged = TRUE;
    // practiceLogInfo("--- Final Splits ---");
    // prev_time = 0;
    // for (i = 0; i < s_SplitsTriggered; i++) {
    //   split = &s_CurrentSplitList->splits[i];
    //   delta = s_SplitCumulativeTimes[i] - prev_time;
    //   format_time(s_SplitCumulativeTimes[i], time_buf, TRUE);
    //   format_time(delta, delta_buf, FALSE);
    //   practiceLogInfo("[%d/%d] %s = (+ %s) %s", i + 1, s_TotalSplits,
    //                   split->name, delta_buf, time_buf);
    //   prev_time = s_SplitCumulativeTimes[i];
    // }
    // // Log overall time
    // if (s_SplitsTriggered > 0) {
    //   format_time(mission_timer, time_buf, TRUE);
    //   practiceLogInfo("Final time: %s", time_buf);
    // }
    return;
  }

  // If all splits have been triggered, nothing more to do
  if (s_NextSplitIndex >= s_TotalSplits) {
    return;
  }

  // Get Bond's current position
  prop = get_curplayer_positiondata();
  if (prop == NULL) {
    return;
  }

  bond_x = prop->pos.x;
  bond_z = prop->pos.z;

  // Check if the split is triggered
  next_split = &s_CurrentSplitList->splits[s_NextSplitIndex];
  triggered = FALSE;

  if (next_split->type == SPLIT_TYPE_AREA) {
    triggered = point_in_quad(bond_x, bond_z, next_split->vertices);
  } else if (next_split->type == SPLIT_TYPE_ITEM) {
    triggered = splits_check_item_held(next_split->item_id);
  } else if (next_split->type == SPLIT_TYPE_KEY) {
    triggered = bondinvCheckHasKeyFlags(next_split->key_flags);
  }

  if (triggered) {
    // Split triggered!
    current_time = mission_timer;
    delta = current_time - s_LastMissionTimer;

    // Store cumulative time for level summary
    if (s_SplitsTriggered < MAX_SPLITS) {
      s_SplitCumulativeTimes[s_SplitsTriggered] = current_time;
    }

    // Format times
    format_time(current_time, time_buf, TRUE);
    format_time(delta, delta_buf, FALSE);

    // Log the split
    // practiceLogInfo("SPLIT %d/%d: %s - %s (delta: %s)", s_SplitsTriggered +
    // 1,
    //                 s_TotalSplits, next_split->name, time_buf, delta_buf);

    s_LastMissionTimer = current_time;
    s_SplitsTriggered++;
    s_NextSplitIndex++;
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
extern Gfx *frontPrintText(Gfx *gdl, s32 *x, s32 *y, s8 *text, s32 second_font_table, s32 first_font_table, s32 arg6, s32 view_x, s32 view_y, s32 arg9, s32 arga);
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

  // Title "FINAL SPLITS"
  // x = 55;
  // y = 204;
  // DL = frontPrintText(DL, &x, &y, (s8 *)"FINAL SPLITS", ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);

  if (s_TotalSplits == 0) {
    x = 55;
    y = 220;
    DL = frontPrintText(DL, &x, &y, (s8 *)"No splits configured", ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);
    return DL;
  }

  // Single Column Layout
  y_spacing = 13;
  y = 204;

  for (i = 0; i < s_TotalSplits; i++) {
    const SplitArea *split = &s_CurrentSplitList->splits[i];
    char name_buf[32];
    char time_buf[16];
    char delta_buf[24];
    s32 x_pos;

    // Print split number and name
    sprintf(name_buf, "%d. %s", i + 1, split->name);
    x_pos = 55;
    DL = frontPrintText(DL, &x_pos, &y, (s8 *)name_buf, ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);

    if (i < s_SplitsTriggered) {
      s32 cumulative = s_SplitCumulativeTimes[i];
      s32 delta = cumulative - prev_time;
      format_time(cumulative, time_buf, TRUE);
      format_time(delta, delta_buf, FALSE);
      prev_time = cumulative;

      // Print cumulative time
      x_pos = 240;
      DL = frontPrintText(DL, &x_pos, &y, (s8 *)time_buf, ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);

      // Print delta
      sprintf(name_buf, "(+%s)", delta_buf);
      x_pos = 310;
      DL = frontPrintText(DL, &x_pos, &y, (s8 *)name_buf, ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);
    } else {
      x_pos = 240;
      DL = frontPrintText(DL, &x_pos, &y, (s8 *)"-:--.--", ptrFontZurichBoldChars, ptrFontZurichBold, 0xFF, viGetX(), viGetY(), 0, 0);
    }
    y += y_spacing;
  }

  return DL;
}
