#include "practice_states.h"
#include "../practice_sram.h"
#include "player.h"
#include "practice_ui.h"
#include <bondconstants.h>
#include <music.h>
#include <snd.h>
#include <ultra64.h>

extern s32 g_CurrentStageToLoad;
extern void store_osgetcount(void);

/* Small header cache so we can validate without re-reading SRAM. */
static SaveStateHeader g_SavedHeader;
bool g_HasSavedState = FALSE;

void init_save_state_system(void) {
  sram_read(SAVE_STATE_SRAM_OFFSET, &g_SavedHeader, sizeof(g_SavedHeader));

  g_HasSavedState = g_SavedHeader.magic == SAVE_STATE_MAGIC &&
                    g_SavedHeader.version == SAVE_STATE_VERSION;
}

void save_game_state(void) {
  SramStream stream;

  if (g_CurrentPlayer == NULL)
    return;

  sram_stream_init_write(&stream, SAVE_STATE_SRAM_OFFSET);

  /* 1. Write placeholder header (magic, version, level_id, size=0). */
  {
    SaveStateHeader header;
    header.magic = SAVE_STATE_MAGIC;
    header.version = SAVE_STATE_VERSION;
    header.level_id = g_CurrentStageToLoad;
    header.size = 0; /* patched below */
    write_bytes(&stream.base, &header, sizeof(header));
  }

  /* 2. Write globals. */
  save_global_state(&stream.base);

  /* 3. Write props and their associated player state. */
  if (!save_props_state(&stream.base)) {
    practiceLogWarn("Failed to save state");
    return;
  }

  /* Flush the remaining bytes in the buffer to SRAM. */
  stream_flush(&stream.base);

  /* 4. Patch the header size field in g_SavedHeader. */
  g_SavedHeader.magic = SAVE_STATE_MAGIC;
  g_SavedHeader.version = SAVE_STATE_VERSION;
  g_SavedHeader.level_id = g_CurrentStageToLoad;
  g_SavedHeader.size = stream.base.total_processed;

  sram_write(SAVE_STATE_SRAM_OFFSET, &g_SavedHeader, sizeof(g_SavedHeader));

  g_HasSavedState = TRUE;

  sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State saved (%dKB)", (g_SavedHeader.size + 1023) / 1024);
}

void load_game_state(void) {
  SramStream stream;

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

  sram_stream_init_read(&stream, SAVE_STATE_SRAM_OFFSET);

  /* 1. Skip header (already validated from g_SavedHeader). */
  stream_seek(&stream.base, SAVE_STATE_SRAM_OFFSET + sizeof(SaveStateHeader));

  /* 2. Load scalar globals and cache prop-dependent global references. */
  load_global_state_pre_props(&stream.base);

  /* 3. Load props, followed by their associated player state. */
  if (!load_props_state(&stream.base)) {
    practiceLogWarn("Failed to load state");
    return;
  }

  /* 4. Resolve prop-dependent globals and restore the current player. */
  if (!load_global_state_post_props()) {
    practiceLogWarn("Failed to restore post-prop globals");
    return;
  }

  // Re-baseline frame timer so time isn't dumped into the next deltaFrames
  store_osgetcount();

  sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State loaded");
}
