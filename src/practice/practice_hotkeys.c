#include "game/lvl.h"
#include "os.h"
#include "player_2.h"
#include "practice_config.h"
#include "practice_debug.h"
#include "practice_splits.h"
#include "practice_timescale.h"
#include "practice_ui.h"
#include "state/practice_states.h"
#include <bondconstants.h>
#include <bondgame.h>
#include <joy.h>
#include <os_extension.h>
#include <ultra64.h>

extern s32 g_CurrentStageToLoad;
extern s32 g_BgCurrentRoom; // player's current room (defined in bg.c)

extern int sprintf(char *dst, const char *fmt, ...);

bool practice_check_hotkeys(void) {
  u16 jgbptf;

  u16 jgb = joyGetButtons(get_cur_playernum(), ANY_BUTTON);
  u16 trigger = practice.left_trigger_hotkeys ? L_TRIG : R_TRIG;

  if (!(jgb & trigger)) {
    if (g_IsTimePaused)
      unpause();
    return FALSE;
  }

  jgbptf = joyGetButtonsPressedThisFrame(get_cur_playernum(), ANY_BUTTON);

  if ((jgbptf & trigger) && !g_IsTimePaused)
    pause();

  if (jgbptf & D_JPAD) {
    save_game_state();
    return TRUE;
  }
  if (jgbptf & U_JPAD) {
    load_game_state();
    return TRUE;
  }

  if (jgbptf & L_JPAD) {
    f32 new_time_scale = ((s32)(g_TimeScale * 10.0f + 0.5f) - 1) * 0.1f;
    set_time_scale(new_time_scale);
    practiceLogInfo("Time scale: %.1f", g_TimeScale);
    return TRUE;
  }
  if (jgbptf & R_JPAD) {
    f32 new_time_scale = ((s32)(g_TimeScale * 10.0f + 0.5f) + 1) * 0.1f;
    set_time_scale(new_time_scale);
    practiceLogInfo("Time scale: %.1f", g_TimeScale);
    return TRUE;
  }

  if (jgbptf & D_CBUTTONS) {
    splits_log_position();
    return TRUE;
  }

  if (jgbptf & START_BUTTON) {
    unpause();
    lvlStageLoad(g_CurrentStageToLoad);
    practiceLogInfo("Level restarted");
    return TRUE;
  }

#if DEV
  // TODO: Display memory usage while paused?
  if (jgbptf & R_CBUTTONS) {
    practice_log_memory_usage();
    return TRUE;
  }

  // Log the room the player is currently in (useful for finding room IDs).
  if (jgbptf & U_CBUTTONS) {
    practiceLogInfo("Room: %d", g_BgCurrentRoom);
    return TRUE;
  }
#endif

  return TRUE;
}
