#ifndef PRACTICE_STATES_H
#define PRACTICE_STATES_H

#include "../practice_sram.h"
#include "practice_states_stream.h"
#include <bondtypes.h>
#include <ultra64.h>

#define SAVE_STATE_MAGIC 0x47455353 // "GESS"

/**
 * Increment this version number before releasing a new version of the ROM with
 * a breaking change to the save format.
 *
 * Safe changes:
 * - Adding extra fields/structs to the very end of the save state.
 * - Changing the data being stored inside an existing field (as long as
 * type/size is unchanged).
 *
 * Breaking changes:
 * - Modifying the size or layout of fields in the middle of the save state
 * (which alters offsets of all subsequent fields).
 */
#define SAVE_STATE_VERSION 5
typedef struct {
  u32 magic;
  u16 version;
  u16 size;
  s32 level_id;
} SaveStateHeader;

void save_global_state(StateStream *stream);
void load_global_state_pre_props(StateStream *stream);
bool load_global_state_post_props(void);
bool save_viewer_players_state(StateStream *stream);
bool load_viewer_players_state(StateStream *stream);
bool save_props_state(StateStream *stream);
bool load_props_state(StateStream *stream);

extern bool g_HasSavedState;

void init_save_state_system(void);
void save_game_state(void);
void load_game_state(void);

#endif /* PRACTICE_STATES_H */
