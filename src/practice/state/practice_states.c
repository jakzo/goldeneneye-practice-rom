#include "practice_states.h"
#include "player.h"
#include "practice_sram.h"
#include "practice_states_bond.h"
#include "practice_states_globals.h"
#include "practice_storage.h"
#include "practice_ui.h"
#include <bondconstants.h>
#include <music.h>
#include <snd.h>
#include <ultra64.h>

extern s32 g_CurrentStageToLoad;

/* Small header cache so we can validate without re-reading SRAM. */
static SaveStateHeader g_SavedHeader;
bool g_HasSavedState = FALSE;

void init_save_state_system(void) {
  StorageCursor cur;

  storage_cursor_init(&cur, SAVE_STATE_SRAM_OFFSET);
  storage_read(&cur, &g_SavedHeader, sizeof(g_SavedHeader));

  g_HasSavedState = g_SavedHeader.magic == SAVE_STATE_MAGIC &&
                    g_SavedHeader.version == SAVE_STATE_VERSION;
}

void save_game_state(void) {
  StorageCursor cur;
  SaveWorkMem work;
  u32 totalStart;
  u32 totalEnd;

  if (g_CurrentPlayer == NULL)
    return;

  storage_cursor_init(&cur, SAVE_STATE_SRAM_OFFSET);
  totalStart = cur.offset;

  /* 1. Write header (size patched at the end). */
  work.header.magic = SAVE_STATE_MAGIC;
  work.header.version = SAVE_STATE_VERSION;
  work.header.level_id = g_CurrentStageToLoad;
  work.header.size = 0; /* patched below */
  /* Copy to g_SavedHeader now — work.header will be overwritten by
     subsequent section fills (work is a union). */
  g_SavedHeader = work.header;
  storage_write(&cur, &work.header, sizeof(work.header));

  /* 2. Write globals (small sparse section via working memory). */
  save_global_state(&work.globals);
  storage_write(&cur, &work.globals, sizeof(work.globals));

  /* 3. Write bond state (player blocks direct + helper + inventory). */
  save_bond_state(&cur, &work);

  /* 4. Write props state (per-record streaming). */
  if (!save_props_state(&cur, &work)) {
    practiceLogWarn("Failed to save state");
    return;
  }

  /* 5. Patch the header size field in g_SavedHeader (already has
     correct magic/version/level_id from step 1). */
  totalEnd = cur.offset;
  g_SavedHeader.size = totalEnd - totalStart;
  {
    StorageCursor patchCur;
    storage_cursor_init(&patchCur, SAVE_STATE_SRAM_OFFSET);
    storage_write(&patchCur, &g_SavedHeader, sizeof(g_SavedHeader));
  }

  g_HasSavedState = TRUE;

  sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State saved");
}

void load_game_state(void) {
  StorageCursor cur;

  if (g_CurrentPlayer == NULL || !g_HasSavedState) {
    if (!g_HasSavedState) {
      practiceLogWarn("No saved state");
    }
    return;
  }

  if (g_SavedHeader.magic != SAVE_STATE_MAGIC) {
    practiceLogWarn("Invalid save");
    return;
  }

  if (g_SavedHeader.version < SAVE_STATE_VERSION) {
    practiceLogWarn("Save was made with an older ROM version");
    return;
  }

  if (g_SavedHeader.version > SAVE_STATE_VERSION) {
    practiceLogWarn("Save was made with a newer ROM version");
    return;
  }

  if (g_SavedHeader.level_id != g_CurrentStageToLoad) {
    practiceLogWarn("Save does not match current level");
    return;
  }

  /* Stop all active sound effects before loading state. */
  sndDeactivateAllSfxByFlag_1();

  storage_cursor_init(&cur, SAVE_STATE_SRAM_OFFSET);

  {
    SaveWorkMem work;

    /* 1. Skip header (already validated from g_SavedHeader). */
    cur.offset += sizeof(work.header);

    /* 2. Load globals. */
    storage_read(&cur, &work.globals, sizeof(work.globals));
    load_global_state(&work.globals);

    /* 3. Load bond state. */
    load_bond_state(&cur, &work);

    /* 4. Load props state. */
    if (!load_props_state(&cur, &work)) {
      practiceLogWarn("Failed to load state");
      return;
    }
  }

  sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State loaded");
}
