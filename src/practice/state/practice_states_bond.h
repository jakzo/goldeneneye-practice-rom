#ifndef PRACTICE_STATES_BOND_H
#define PRACTICE_STATES_BOND_H

#include "practice_storage.h"
#include <bondtypes.h>
#include <ultra64.h>

typedef struct {
  u8 block1[0x0808]; // 0x0000 to 0x0808 (pos, bobs, speeds, collision, model
                     // pointer info)
  u8 block2[0x0854]; // 0x0870 to 0x10C4 (hands, sway, zoom, crosshairs, trigger
                     // timer)
  u8 block3[0x00B8]; // 0x10F0 to 0x11A8 (coordinate limits, LOD, ammoheldarr)
  u8 block4[0x06C0]; // 0x11B8 to 0x1878 (zoom timers, fovy, aspect, inventory
                     // counts, gunheldarr, cheats, damage overlays)
  u8 block5[0x00B8]; // 0x29B8 to 0x2A70 (MP menus, control types, stats)
} SavedPlayerState;

typedef struct {
  s32 type;
  s32 weapon_right;
  s32 weapon_left;
  s32 prop_index;
} SavedInvItem;

typedef struct {
  // The core player state
  SavedPlayerState saved_player;

  // Helper offsets/indices for pointer resolution
  s32 room_pointer_offset;
  s32 field_488_current_tile_ptr_offset;
  s32 field_488_current_tile_ptr_for_portals_offset;
  s32 previous_collision_info_current_tile_ptr_offset;
  s32 previous_collision_info_current_tile_ptr_for_portals_offset;
  s32 field_2A70_offset;

  s32 autoaim_target_y_index;
  s32 autoaim_target_x_index;

  // Custom state (Prop state)
  bool has_prop;
  coord3d prop_pos;
  s32 prop_stan_offset;
  u8 prop_rooms[4];

  // Custom state (Inventory)
  s32 num_inv_items;
  SavedInvItem inv_items[50];

} SavedBondState;

#endif /* PRACTICE_STATES_BOND_H */
