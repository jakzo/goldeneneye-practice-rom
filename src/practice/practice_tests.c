#include "practice_tests.h"
#include "bondview.h"
#include "chr.h"
#include "chrai.h"
#include "chrobjhandler.h"
#include "emu_log.h"
#include "joy.h"
#include "player.h"
#include "practice_timescale.h"
#include "state/practice_states.h"
#include <bondgame.h>
#include <ultra64.h>

extern s32 g_ClockTimer;
extern s32 chraiGetAIListID(AIRecord *AIList, bool *isGlobalAIList);
extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;
extern u64 g_tlbRandomSeed;

static s32 g_save_test_timer = -1;
static u32 case_delta = 0;
static bool after_frames(u32 num_frames) {
  case_delta += num_frames;
  return g_save_test_timer == case_delta;
}

void practice_tests_tick() {
  if (g_save_test_timer == -1 && g_CameraMode == CAMERAMODE_FP) {
    g_save_test_timer = 0;
    emu_log("TEST_STARTED");
  }

  if (g_save_test_timer < 0)
    return;

  g_save_test_timer++;
  case_delta = 0;

  {
#if TEST_CASE == STATE_DOOR
    PropRecord *door = NULL;
    PropRecord *p = NULL;
    u32 door_num = 4; // roller door in front of bond at start of runway
    for (p = ptr_obj_pos_list_first_entry; p != NULL; p = p->next) {
      if (p->type == PROP_TYPE_DOOR && door_num-- <= 0) {
        door = p;
        break;
      }
    }

    if (after_frames(30)) {
      if (door == NULL) {
        emu_log("DOOR_NOT_FOUND");
      } else {
        emu_log("DOOR_TEST_BEFORE");
        // bond_interact_object();
        propdoorInteract(door);
        propExecuteTickOperation(door, FALSE);
        emu_log("DOOR_TEST_AFTER");
      }
    } else if (after_frames(30)) {
      emu_log("TRIGGER_SAVE");
      save_game_state();
      emu_log("SAVE_DONE");
    } else if (after_frames(30)) {
      emu_log("TRIGGER_LOAD");
      load_game_state();
      emu_log("LOAD_DONE");
    } else if (after_frames(2)) {
      emu_log("TEST_COMPLETE");
    }
#elif TEST_CASE == STATE_GRENADE
    if (after_frames(20)) {
      emu_log("TRIGGER_SAVE");
      save_game_state();
      emu_log("SAVE_DONE");
    } else if (after_frames(20)) {
      emu_log("START_MOVING_LEFT");
      g_SimulatedButtons |= L_CBUTTONS;
    } else if (after_frames(20)) {
      emu_log("GRENADES_PICKED_UP");
      emu_log("STOP_MOVING_LEFT");
      g_SimulatedButtons &= ~L_CBUTTONS;
    } else if (after_frames(20)) {
      emu_log("TRIGGER_LOAD");
      load_game_state();
      emu_log("LOAD_DONE");
    } else if (after_frames(20)) {
      emu_log("START_MOVING_LEFT");
      g_SimulatedButtons |= L_CBUTTONS;
    } else if (after_frames(20)) {
      emu_log("GRENADES_PICKED_UP");
      emu_log("STOP_MOVING_LEFT");
      g_SimulatedButtons &= ~L_CBUTTONS;
    } else if (after_frames(20)) {
      emu_log("SWITCHING_WEAPONS_TO_GRENADES");
      g_SimulatedButtons |= A_BUTTON;
    } else if (after_frames(2)) {
      g_SimulatedButtons &= ~A_BUTTON;
    } else if (after_frames(30)) {
      emu_log("THROWING_GRENADE");
      g_SimulatedButtons |= Z_TRIG;
    } else if (after_frames(2)) {
      g_SimulatedButtons &= ~Z_TRIG;
    } else if (after_frames(15)) {
      emu_log("TRIGGER_SAVE");
      save_game_state();
      emu_log("SAVE_DONE");
    } else if (after_frames(300)) {
      emu_log("TRIGGER_LOAD");
      load_game_state();
      emu_log("LOAD_DONE");
    } else if (after_frames(2)) {
      emu_log("TRIGGER_SECOND_SAVE");
      save_game_state();
      emu_log("SECOND_SAVE_DONE");
    } else if (after_frames(2)) {
      emu_log("TEST_COMPLETE");
    }
#elif TEST_CASE == STATE_BUNKER
    {
      static s32 previous_chr_count = -1;
      s32 i;
      s32 chr_count = 0;
      ChrRecord *controller = NULL;

      for (i = 0; i < g_NumChrSlots; i++) {
        if (g_ChrSlots[i].model != NULL) {
          chr_count++;
          if (g_ChrSlots[i].chrnum == 25) {
            controller = &g_ChrSlots[i];
          }
        }
      }

      if (g_save_test_timer % 300 == 0) {
        emu_log("BUNKER frame=%d alarm=%d flags=%08x chrs=%d ctrl=%p "
                "timer=%d hidden=%04x aioffset=%d action=%d",
                g_save_test_timer, alarm_timer, objectiveregisters1, chr_count,
                controller, controller != NULL ? controller->timer60 : -1,
                controller != NULL ? controller->hidden : 0,
                controller != NULL ? controller->aioffset : -1,
                controller != NULL ? controller->actiontype : -1);
      }

      if (chr_count != previous_chr_count) {
        emu_log("CHR_COUNT_CHANGED frame=%d old=%d new=%d", g_save_test_timer,
                previous_chr_count, chr_count);
        for (i = 0; i < g_NumChrSlots; i++) {
          ChrRecord *chr = &g_ChrSlots[i];
          if (chr->model != NULL) {
            bool is_global = FALSE;
            s32 ailist_id = chr->ailist != NULL
                                ? chraiGetAIListID(chr->ailist, &is_global)
                                : -1;
            emu_log("CHR slot=%d num=%d body=%d ai=%04x off=%d flags=%08x "
                    "hidden=%04x hear=%d action=%d",
                    i, chr->chrnum, chr->bodynum, ailist_id, chr->aioffset,
                    chr->chrflags, chr->hidden, chr->lastheartarget60,
                    chr->actiontype);
          }
        }
        previous_chr_count = chr_count;
      }
    }

    if (after_frames(30)) {
      emu_log("TRIGGER_SAVE");
      save_game_state();
      emu_log("SAVE_DONE");
    } else if (after_frames(30)) {
      emu_log("TRIGGER_ALARM");
      alarmActivate();
      objectiveregisters1 |= 0x100;
    } else if (after_frames(600)) {
      emu_log("PRE_LOAD alarm=%d flags=%08x", alarm_timer, objectiveregisters1);
      emu_log("TRIGGER_LOAD");
      load_game_state();
      emu_log("LOAD_DONE");
      emu_log("POST_LOAD alarm=%d flags=%08x", alarm_timer,
              objectiveregisters1);
    } else if (after_frames(1200)) {
      emu_log("TEST_COMPLETE");
    }
#elif TEST_CASE == STATE_DAM
    if (after_frames(30)) {
      emu_log("TRIGGER_SAVE");
      save_game_state();
      emu_log("SAVE_DONE");
    } else if (after_frames(30)) {
      emu_log("TRIGGER_LOAD");
      load_game_state();
      emu_log("LOAD_DONE");
    } else if (after_frames(2)) {
      emu_log("TEST_COMPLETE");
    }
#elif TEST_CASE == FIRE_SLOWMO
    // The first-person gun must fire only when simulation time advances. A shot
    // (ammo decrement) on a frame where no time passed (prev_clock == 0) means
    // the gun fired while frozen/slowed -- the bug.
    {
      static s32 prev_ammo = -1;
      static s32 prev_clock = 1;
      s32 ammo = g_CurrentPlayer->hands[GUNRIGHT].weapon_ammo_in_magazine;
      if (prev_ammo >= 0 && ammo < prev_ammo) {
        emu_log("SHOT clock=%d ammo=%d->%d", prev_clock, prev_ammo, ammo);
        if (prev_clock == 0) {
          emu_log("BUG_FIRE_WHILE_FROZEN");
        }
      }
      prev_ammo = ammo;
      prev_clock = g_ClockTimer;
    }

    if (after_frames(60)) {
      // Hold fire at normal speed first so the gun is raised and shooting.
      emu_log("PRESS_Z");
      g_SimulatedButtons |= Z_TRIG;
    } else if (after_frames(30)) {
      emu_log("FREEZE");
      set_time_scale(0.0f);
    } else if (after_frames(120)) {
      emu_log("SLOWMO_0.1");
      set_time_scale(0.1f);
    } else if (after_frames(240)) {
      emu_log("UNFREEZE_NORMAL");
      set_time_scale(1.0f);
    } else if (after_frames(60)) {
      emu_log("RELEASE_Z");
      g_SimulatedButtons &= ~Z_TRIG;
    } else if (after_frames(20)) {
      emu_log("TEST_COMPLETE");
    }
#endif
  }
}

#if TEST_CASE == RNG_LOAD
extern s32 speedgraphframes; // finalized per-frame delta

// Determinism check for loading a save state. Save once, then repeatedly load
// the state and log LOG_FRAMES consecutive frames of (deltaFrames, RNG seeds).
// Each iteration replays from the identical saved state, so if load + gameplay
// is deterministic every iteration must print the exact same per-frame
// sequence. Divergence in a given column pinpoints what isn't deterministic
// (e.g. deltaFrames jitter from wall-clock timing, or g_tlbRandomSeed which the
// save state does not restore).
//
// Runs from updateFrameCounters (via practice_tests_frame) once the frame delta
// and RNG seeds are finalized for the frame, before gameplay consumes any RNG.
void practice_tests_frame() {
  enum { PH_WAIT_FP, PH_SETTLE, PH_RUN, PH_DONE };
  static s32 phase = PH_WAIT_FP;
  static s32 iteration = 0;
  static s32 frame = 0;
  static s32 settle = 0;
  const s32 SETTLE_FRAMES = 30;
  const s32 LOG_FRAMES = 30;
  const s32 NUM_ITERATIONS = 10;

  switch (phase) {
  case PH_WAIT_FP:
    if (g_CameraMode == CAMERAMODE_FP) {
      emu_log("TEST_STARTED");
      phase = PH_SETTLE;
      settle = 0;
    }
    break;

  case PH_SETTLE:
    // Let the level settle before capturing the reference state.
    if (++settle >= SETTLE_FRAMES) {
      emu_log("TRIGGER_SAVE");
      save_game_state();
      emu_log("SAVE_DONE");
      load_game_state();
      iteration = 0;
      frame = 0;
      phase = PH_RUN;
    }
    break;

  case PH_RUN:
    emu_log("ITER %d f=%d delta=%d rng=%08x%08x chrobj=%08x%08x tlb=%08x%08x",
            iteration, frame, speedgraphframes, (u32)(g_randomSeed >> 32),
            (u32)g_randomSeed, (u32)(g_chrObjRandomSeed >> 32),
            (u32)g_chrObjRandomSeed, (u32)(g_tlbRandomSeed >> 32),
            (u32)g_tlbRandomSeed);
    frame++;
    if (frame >= LOG_FRAMES) {
      iteration++;
      if (iteration >= NUM_ITERATIONS) {
        emu_log("TEST_COMPLETE");
        phase = PH_DONE;
      } else {
        load_game_state();
        frame = 0;
      }
    }
    break;

  default:
    break;
  }
}
#else
void practice_tests_frame() {}
#endif
