#ifndef PRACTICE_STATES_H
#define PRACTICE_STATES_H

#include "practice_states_bond.h"
#include "practice_states_globals.h"
#include <ultra64.h>

#define SAVE_STATE_MAGIC 0x47455353 // "GESS"

/**
 * Increment this version number before releasing a new version of the ROM with
 * a breaking change to the save format.
 *
 * Safe changes:
 * - Adding extra fields/structs to the very end of SaveState.
 * - Changing the data being stored inside an existing field (as long as
 * type/size is unchanged).
 *
 * Breaking changes:
 * - Modifying the size or layout of structs in the middle of SaveState (which
 * alters offsets of all subsequent fields).
 */
#define SAVE_STATE_VERSION 1
#define SAVE_STATE_SRAM_OFFSET 0x200

typedef struct {
  u32 magic;    // 4 bytes magic value
  u16 version;  // 2 bytes version
  u16 size;     // 2 bytes total size
  s32 level_id; // 4 bytes level ID
  SavedGlobals global_state;
  SavedBondState bond_state;
} SaveState;

extern bool g_HasSavedState;

void init_save_state_system(void);
void save_game_state(void);
void load_game_state(void);

#endif /* PRACTICE_STATES_H */
