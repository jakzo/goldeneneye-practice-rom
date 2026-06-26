#include "practice_debug.h"
#include "bondview.h"
#include "emu_log.h"
#include "practice_states.h"
#include "practice_ui.h"
#include <bondgame.h>
#include <memp.h>
#include <tlb_manage.h>
#include <ultra64.h>
#include "joy.h"

/* g_mempPools[] is defined in memp.c; we need an extern here as it's not
 * in memp.h. */
extern MemoryPool g_mempPools[MEMPOOL_COUNT];

void practice_log_memory_usage() {
  s32 stage_start, stage_pos, stage_end, stage_size, stage_used, stage_free;
  s32 perm_start, perm_pos, perm_end, perm_size, perm_used, perm_free;
  s32 total_start, total_end, total_size;
  u8(*tlb_block)[TLB_BLOCK_SIZE];
  s32 pool_end, bss_addr, code_data_size, above_pool, free_rdram;
  s32 stage_free_kb, perm_free_kb, total_free_kb;

  /* ---- Pool boundaries ---------------------------------------------- */
  total_start = g_mempPools[MEMPOOL_TOTAL].start;
  total_end = g_mempPools[MEMPOOL_TOTAL].end;
  total_size = total_end - total_start;

  stage_start = g_mempPools[MEMPOOL_STAGE].start;
  stage_pos = g_mempPools[MEMPOOL_STAGE].pos;
  stage_end = g_mempPools[MEMPOOL_STAGE].end;

  perm_start = g_mempPools[MEMPOOL_PERMANENT].start;
  perm_pos = g_mempPools[MEMPOOL_PERMANENT].pos;
  perm_end = g_mempPools[MEMPOOL_PERMANENT].end;

  /* Handle uninitialized pools (pos == 0 means bank hasn't been set up) */
  if (stage_pos == 0) {
    stage_used = 0;
    stage_free = stage_end - stage_start;
  } else {
    stage_used = stage_pos - stage_start;
    stage_free = stage_end - stage_pos;
  }

  if (perm_pos == 0) {
    perm_used = 0;
    perm_free = perm_end - perm_start;
  } else {
    perm_used = perm_pos - perm_start;
    perm_free = perm_end - perm_pos;
  }

  stage_size = stage_end - stage_start;
  perm_size = perm_end - perm_start;

  /* ---- Full 4 MB RDRAM view ---------------------------------------- */
  bss_addr = (s32)(u32)&_bssSegmentEnd;
  tlb_block = tlbmanageGetTlbAllocatedBlock();
  pool_end = (s32)(u32)tlb_block;

  /* Everything between 0x80000000 and pool start: .text, .data, .bss */
  code_data_size = pool_end - bss_addr; /* actually the pool area */
  /* Wait -- the pool area IS bssSegmentEnd → tlbAllocatedBlock.
   * Code/data/BSS lives below bssSegmentEnd. */
  code_data_size = total_start - 0x80000000; /* rough code+data+BSS */

  /* Above the pool (TLB block + stacks + unused) */
  above_pool = 0x80400000 - pool_end;
  if (above_pool < 0)
    above_pool = 0; /* in case we somehow exceed 4MB */

  /* ---- Free RDRAM (unused pool space)  ------------------------------ */
  total_free_kb = (stage_free + perm_free) / 1024;

  /* ---- Log everything ----------------------------------------------- */
  practiceLogInfo("--- Memory ---");

  practiceLogInfo("RDRAM: 4MB total");
  practiceLogInfo("Code+data+BSS: ~%d KB", code_data_size / 1024);
  practiceLogInfo("Pool area:    %d KB  (0x%X - 0x%X)", total_size / 1024,
                  total_start, pool_end);
  practiceLogInfo("Stack/other:  ~%d KB", above_pool / 1024);

  practiceLogInfo("STAGE pool:   %d/%d KB used, %d KB free", stage_used / 1024,
                  stage_size / 1024, stage_free / 1024);
  practiceLogInfo("PERM  pool:   %d/%d KB used, %d KB free", perm_used / 1024,
                  perm_size / 1024, perm_free / 1024);

  practiceLogInfo("Pool free:    %d KB / %d KB total pool", total_free_kb,
                  total_size / 1024);
}

static s32 g_save_test_timer = -1;

void practice_debug_tick() {
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
      emu_log_write("SAVE_TEST_START_HOLD_C_LEFT\n");
      g_SimulatedButtons |= L_CBUTTONS;
    }
    if (g_save_test_timer == 90) {
      emu_log_write("SAVE_TEST_STOP_HOLD_C_LEFT\n");
      g_SimulatedButtons &= ~L_CBUTTONS;
    }
    if (g_save_test_timer == 120) {
      emu_log_write("SAVE_TEST_TRIGGER_LOAD\n");
      load_game_state();
      emu_log_write("SAVE_TEST_LOAD_DONE\n");
    }
  }
#endif
}
