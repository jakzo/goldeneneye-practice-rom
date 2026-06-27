#include "practice_tests.h"
#include "bondview.h"
#include "emu_log.h"
#include "joy.h"
#include "state/practice_states.h"
#include <bondgame.h>
#include <ultra64.h>

static s32 g_save_test_timer = -1;

void practice_tests_tick(void) {
#if TEST_CASE == STATE_DOOR
  if (g_save_test_timer == -1 && g_CameraMode == CAMERAMODE_FP) {
    g_save_test_timer = 0;
    emu_log_write("STATE_DOOR test started\n");
  }
  if (g_save_test_timer >= 0) {
    PropRecord *door = NULL;
    PropRecord *p = NULL;
    u32 door_num = 4; // roller door in front of bond at start
    for (p = ptr_obj_pos_list_first_entry; p != NULL; p = p->next) {
      if (p->type == PROP_TYPE_DOOR && door_num-- <= 0) {
        door = p;
        break;
      }
    }
    g_save_test_timer++;
    if (g_save_test_timer == 1) {
      emu_log_write("SAVE_TEST_TIMER_ACTIVE\n");
    }
    if (g_save_test_timer == 30) {
      if (door == NULL) {
        emu_log_write("DOOR_NOT_FOUND\n");
      } else {
        emu_log_write("DOOR_TEST_BEFORE\n");
        // bond_interact_object();
        propdoorInteract(door);
        propExecuteTickOperation(door, FALSE);
        emu_log_write("DOOR_TEST_AFTER\n");
      }
    }
    if (g_save_test_timer == 60) {
      emu_log_write("SAVE_TEST_TRIGGER_SAVE\n");
      save_game_state();
      emu_log_write("SAVE_TEST_SAVE_DONE\n");
    }
    if (g_save_test_timer == 90) {
      emu_log_write("SAVE_TEST_TRIGGER_LOAD\n");
      load_game_state();
      emu_log_write("SAVE_TEST_LOAD_DONE\n");
    }
  }
#elif TEST_CASE == STATE_PICKUP
  if (g_save_test_timer == -1 && g_CameraMode == CAMERAMODE_FP) {
    g_save_test_timer = 0;
    emu_log_write("STATE_PICKUP test started\n");
  }
  if (g_save_test_timer >= 0) {
    g_save_test_timer++;
    if (g_save_test_timer == 1) {
      emu_log_write("SAVE_TEST_TIMER_ACTIVE\n");
    }
    if (g_save_test_timer == 30) {
      emu_log_write("SAVE_TEST_TRIGGER_SAVE\n");
      save_game_state();
      emu_log_write("SAVE_TEST_SAVE_DONE\n");
    }
    if (g_save_test_timer == 60) {
      emu_log_write("SAVE_TEST_START_MOVING_LEFT\n");
      g_SimulatedButtons |= L_CBUTTONS;
    }
    if (g_save_test_timer == 90) {
      emu_log_write("SAVE_TEST_AMMO_CRATE_PICKED_UP\n");
      emu_log_write("SAVE_TEST_STOP_MOVING_LEFT\n");
      g_SimulatedButtons &= ~L_CBUTTONS;
    }
    if (g_save_test_timer == 120) {
      emu_log_write("SAVE_TEST_TRIGGER_LOAD\n");
      load_game_state();
      emu_log_write("SAVE_TEST_LOAD_DONE\n");
    }
  }
#elif TEST_CASE == STATE_BUNKER
  if (g_save_test_timer == -1 && g_CameraMode == CAMERAMODE_FP) {
    g_save_test_timer = 0;
    emu_log_write("STATE_BUNKER test started\n");
  }
  if (g_save_test_timer >= 0) {
    g_save_test_timer++;
    if (g_save_test_timer == 1) {
      emu_log_write("SAVE_TEST_TIMER_ACTIVE\n");
    }
    if (g_save_test_timer == 30) {
      emu_log_write("SAVE_TEST_TRIGGER_SAVE\n");
      save_game_state();
      emu_log_write("SAVE_TEST_SAVE_DONE\n");
    }
    if (g_save_test_timer == 60) {
      emu_log_write("SAVE_TEST_TRIGGER_LOAD\n");
      load_game_state();
      emu_log_write("SAVE_TEST_LOAD_DONE\n");
    }
  }
#endif
}
