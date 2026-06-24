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
  sram_read(SAVE_STATE_SRAM_OFFSET, &g_SavedHeader, sizeof(g_SavedHeader));

  g_HasSavedState = g_SavedHeader.magic == SAVE_STATE_MAGIC &&
                    g_SavedHeader.version == SAVE_STATE_VERSION;
}

void save_game_state(void) {
  SramStream stream;

  if (g_CurrentPlayer == NULL)
    return;

  sram_stream_init_write(&stream, SAVE_STATE_SRAM_OFFSET);
  g_ActiveSramStream = &stream;

  /* 1. Write placeholder header (magic, version, level_id, size=0). */
  {
    SaveStateHeader header;
    header.magic = SAVE_STATE_MAGIC;
    header.version = SAVE_STATE_VERSION;
    header.level_id = g_CurrentStageToLoad;
    header.size = 0; /* patched below */
    sram_stream_write_bytes(&stream, &header, sizeof(header));
  }

  /* 2. Write globals. */
  save_global_state(&stream);

  /* 3. Write bond state. */
  save_bond_state(&stream);

  /* 4. Write props state. */
  if (!save_props_state(&stream)) {
    practiceLogWarn("Failed to save state");
    g_ActiveSramStream = NULL;
    return;
  }

  /* Flush the remaining bytes in the buffer to SRAM. */
  sram_stream_flush(&stream);

  /* 5. Patch the header size field in g_SavedHeader. */
  g_SavedHeader.magic = SAVE_STATE_MAGIC;
  g_SavedHeader.version = SAVE_STATE_VERSION;
  g_SavedHeader.level_id = g_CurrentStageToLoad;
  g_SavedHeader.size = stream.total_processed;

  sram_write(SAVE_STATE_SRAM_OFFSET, &g_SavedHeader, sizeof(g_SavedHeader));

  g_ActiveSramStream = NULL;
  g_HasSavedState = TRUE;

  sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State saved");
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
  g_ActiveSramStream = &stream;

  /* 1. Skip header (already validated from g_SavedHeader). */
  sram_stream_seek(&stream, SAVE_STATE_SRAM_OFFSET + sizeof(SaveStateHeader));

  /* 2. Load globals. */
  load_global_state(&stream);

  /* 3. Load bond state. */
  load_bond_state(&stream);

  /* 4. Load props state. */
  if (!load_props_state(&stream)) {
    practiceLogWarn("Failed to load state");
    g_ActiveSramStream = NULL;
    return;
  }

  g_ActiveSramStream = NULL;

  sndPlaySfx((struct ALBankAlt_s *)g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State loaded");
}
