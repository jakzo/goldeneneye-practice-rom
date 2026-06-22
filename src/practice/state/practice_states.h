#ifndef PRACTICE_STATES_H
#define PRACTICE_STATES_H

#include "practice_states_bond.h"
#include "practice_states_globals.h"
#include "practice_states_props.h"
#include "practice_storage.h"
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
#define SAVE_STATE_VERSION 3
#define SAVE_STATE_SRAM_OFFSET 0x200

typedef struct {
  u32 magic;
  u16 version;
  u16 size;
  s32 level_id;
} SaveStateHeader;

/* ------------------------------------------------------------------ */
/* Section types — each is a unit of data written/read atomically.     */
/* The working memory is a union of all of these so that only enough   */
/* stack space for the largest single section is needed.               */
/* ------------------------------------------------------------------ */

/* Sparse bond helper fields (pointer offsets, prop state). */
typedef struct {
  s32 room_pointer_offset;
  s32 field_488_current_tile_ptr_offset;
  s32 field_488_current_tile_ptr_for_portals_offset;
  s32 previous_collision_info_current_tile_ptr_offset;
  s32 previous_collision_info_current_tile_ptr_for_portals_offset;
  s32 field_2A70_offset;
  s32 autoaim_target_y_index;
  s32 autoaim_target_x_index;
  bool has_prop;
  coord3d prop_pos;
  s32 prop_stan_offset;
  u8 prop_rooms[4];
} BondHelperSection;

/* Flattened inventory linked list. */
typedef struct {
  s32 num_inv_items;
  SavedInvItem inv_items[50];
} BondInventorySection;

/* A single prop record (prop header + type-specific union). */
typedef SavedRecordsOfProp PropRecordSection;

/* Working memory — a union of every section type so that stack usage
   is bounded by the largest single section (~800 bytes). */
typedef union SaveWorkMem {
  SaveStateHeader header;
  SavedGlobals globals;
  BondHelperSection bondHelper;
  BondInventorySection bondInventory;
  PropsHeaderSection propsHeader;
  PropRecordSection propRecord;
  SavedProjectileEntry projectile;
  SavedEmbedmentEntry embedment;
} SaveWorkMem;

/* Section save/load functions (declared here because they all use
   SaveWorkMem and StorageCursor).  Implemented in the respective
   section files. */
void save_global_state(SavedGlobals *dst);
void load_global_state(SavedGlobals *src);
void save_bond_state(StorageCursor *cur, SaveWorkMem *work);
void load_bond_state(StorageCursor *cur, SaveWorkMem *work);
bool save_props_state(StorageCursor *cur, SaveWorkMem *work);
bool load_props_state(StorageCursor *cur, SaveWorkMem *work);

extern bool g_HasSavedState;

void init_save_state_system(void);
void save_game_state(void);
void load_game_state(void);

#endif /* PRACTICE_STATES_H */
