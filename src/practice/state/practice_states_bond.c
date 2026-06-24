#include "practice_states_bond.h"
#include "bondinv.h"
#include "bondview.h"
#include "chr.h"
#include "chrai.h"
#include "gun.h"
#include "objecthandler.h"
#include "player.h"
#include "player_2.h"
#include "practice_states.h"
#include "practice_states_globals.h"
#include "practice_states_utils.h"
#include "practice_storage.h"
#include "practice_ui.h"
#include "watch.h"
#include <bondconstants.h>
#include <bondgame.h>
#include <joy.h>
#include <music.h>
#include <os_extension.h>
#include <snd.h>
#include <ultra64.h>

extern void used_to_load_1st_person_model_on_demand(enum GUNHAND hand);

extern void bondviewApplyVertaTheta(void);
extern void chrpropDelist(PropRecord *prop);
extern int bondinvAddPropToInv(PropRecord *prop);
extern s32 g_EnterTankAudioState;
extern void *memcpy(void *dst, const void *src, size_t count);

/* ------------------------------------------------------------------ */
/* Player state — the five contiguous blocks are written directly to   */
/* storage from the live player struct (no working-memory copy).       */
/* On load they are read directly back, with live pointers backed up   */
/* and restored around the raw copy.                                   */
/* ------------------------------------------------------------------ */

static const struct {
  u32 srcoff;
  u32 size;
} player_blocks[5] = {
    {0x0000, 0x0808}, {0x0870, 0x0854}, {0x10F0, 0x00B8},
    {0x11B8, 0x06C0}, {0x29B8, 0x00B8},
};

static void save_player_state_direct(SramStream *stream, struct player *src) {
  u8 *src_bytes = (u8 *)src;
  s32 i;
  for (i = 0; i < 5; i++) {
    sram_stream_write_bytes(stream, src_bytes + player_blocks[i].srcoff,
                            player_blocks[i].size);
  }
}

static void load_player_state_direct(SramStream *stream, struct player *dst) {
  u8 *dst_bytes = (u8 *)dst;
  s32 backup_field_5C = dst->field_5C;
  s32 backup_field_60 = dst->field_60;
  s32 backup_field_64 = dst->field_64;
  s32 backup_field_68 = dst->field_68;
  PropRecord *backup_prop = dst->prop;
  struct Model *backup_ptr_char_objectinstance = dst->ptr_char_objectinstance;
  Model *backup_model = dst->model;
  ObjectRecord *backup_hand_rocket[2];
  InvItem *backup_ptr_inventory_first_in_cycle =
      dst->ptr_inventory_first_in_cycle;
  InvItem *backup_p_itemcur = dst->p_itemcur;
  textoverride *backup_textoverrides = dst->textoverrides;
  s32 i;

  backup_hand_rocket[0] = dst->hands[0].rocket;
  backup_hand_rocket[1] = dst->hands[1].rocket;

  for (i = 0; i < 5; i++) {
    sram_stream_read_bytes(stream, dst_bytes + player_blocks[i].srcoff,
                           player_blocks[i].size);
  }

  dst->field_5C = backup_field_5C;
  dst->field_60 = backup_field_60;
  dst->field_64 = backup_field_64;
  dst->field_68 = backup_field_68;
  dst->prop = backup_prop;
  dst->ptr_char_objectinstance = backup_ptr_char_objectinstance;
  dst->model = backup_model;
  dst->hands[0].rocket = backup_hand_rocket[0];
  dst->hands[1].rocket = backup_hand_rocket[1];
  dst->ptr_inventory_first_in_cycle = backup_ptr_inventory_first_in_cycle;
  dst->p_itemcur = backup_p_itemcur;
  dst->textoverrides = backup_textoverrides;
}

/* ------------------------------------------------------------------ */
/* Save                                                                */
/* ------------------------------------------------------------------ */

void save_bond_state(SramStream *stream) {
  if (g_CurrentPlayer == NULL) {
    return;
  }

  /* 1. Player blocks — direct to storage. */
  save_player_state_direct(stream, g_CurrentPlayer);

  /* 2. Bond helper section (sparse fields). */
  {
    BondHelperSection helper;
    BondHelperSection *h = &helper;
    h->room_pointer_offset = get_tile_offset(g_CurrentPlayer->room_pointer);
    h->field_488_current_tile_ptr_offset =
        get_tile_offset(g_CurrentPlayer->field_488.current_tile_ptr);
    h->field_488_current_tile_ptr_for_portals_offset = get_tile_offset(
        g_CurrentPlayer->field_488.current_tile_ptr_for_portals);
    h->previous_collision_info_current_tile_ptr_offset = get_tile_offset(
        g_CurrentPlayer->previous_collision_info.current_tile_ptr);
    h->previous_collision_info_current_tile_ptr_for_portals_offset =
        get_tile_offset(g_CurrentPlayer->previous_collision_info
                            .current_tile_ptr_for_portals);
    h->field_2A70_offset = get_tile_offset(g_CurrentPlayer->field_2A70);
    h->autoaim_target_y_index =
        get_prop_index(g_CurrentPlayer->autoaim_target_y);
    h->autoaim_target_x_index =
        get_prop_index(g_CurrentPlayer->autoaim_target_x);

    if (g_CurrentPlayer->prop != NULL) {
      h->has_prop = TRUE;
      h->prop_pos = g_CurrentPlayer->prop->pos;
      h->prop_stan_offset = get_tile_offset(g_CurrentPlayer->prop->stan);
      h->prop_rooms[0] = g_CurrentPlayer->prop->rooms[0];
      h->prop_rooms[1] = g_CurrentPlayer->prop->rooms[1];
      h->prop_rooms[2] = g_CurrentPlayer->prop->rooms[2];
      h->prop_rooms[3] = g_CurrentPlayer->prop->rooms[3];
    } else {
      h->has_prop = FALSE;
    }
    sram_stream_write_bytes(stream, h, sizeof(*h));
  }

  /* 3. Inventory section. */
  {
    BondInventorySection inventory;
    BondInventorySection *inv = &inventory;
    InvItem *first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    InvItem *item = first;
    s32 count = 0;
    s32 i;

    for (i = 0; i < 50; i++) {
      inv->inv_items[i].type = -1;
      inv->inv_items[i].weapon_right = -1;
      inv->inv_items[i].weapon_left = -1;
      inv->inv_items[i].prop_index = -1;
    }

    if (item != NULL) {
      do {
        if (count >= 50)
          break;
        inv->inv_items[count].type = item->type;
        if (item->type == INV_ITEM_WEAPON) {
          inv->inv_items[count].weapon_right =
              item->type_inv_item.type_weap.weapon;
        } else if (item->type == INV_ITEM_DUAL) {
          inv->inv_items[count].weapon_right =
              item->type_inv_item.type_dual.weapon_right;
          inv->inv_items[count].weapon_left =
              item->type_inv_item.type_dual.weapon_left;
        } else if (item->type == INV_ITEM_PROP) {
          inv->inv_items[count].prop_index =
              get_prop_index(item->type_inv_item.type_prop.prop);
        }
        count++;
        item = item->next;
      } while (item != first && item != NULL);
    }
    inv->num_inv_items = count;
    sram_stream_write_bytes(stream, inv, sizeof(*inv));
  }
}

/* ------------------------------------------------------------------ */
/* Load                                                                */
/* ------------------------------------------------------------------ */

void load_bond_state(SramStream *stream) {
  s32 preload_hand_item[2];
  textoverride *live_textoverrides;

  if (g_CurrentPlayer == NULL) {
    return;
  }

  /* 1. Backup hand items and text overrides. */
  preload_hand_item[0] = g_CurrentPlayer->hand_item[0];
  preload_hand_item[1] = g_CurrentPlayer->hand_item[1];
  live_textoverrides = g_CurrentPlayer->textoverrides;

  /* 2. Read player blocks directly into live struct. */
  load_player_state_direct(stream, g_CurrentPlayer);

  /* 3. Read and apply bond helper section. */
  {
    BondHelperSection helper;
    BondHelperSection *h = &helper;
    sram_stream_read_bytes(stream, h, sizeof(*h));

    g_CurrentPlayer->room_pointer = get_tile_by_offset(h->room_pointer_offset);
    g_CurrentPlayer->field_488.current_tile_ptr =
        get_tile_by_offset(h->field_488_current_tile_ptr_offset);
    g_CurrentPlayer->field_488.current_tile_ptr_for_portals =
        get_tile_by_offset(h->field_488_current_tile_ptr_for_portals_offset);
    g_CurrentPlayer->previous_collision_info.current_tile_ptr =
        get_tile_by_offset(h->previous_collision_info_current_tile_ptr_offset);
    g_CurrentPlayer->previous_collision_info.current_tile_ptr_for_portals =
        get_tile_by_offset(
            h->previous_collision_info_current_tile_ptr_for_portals_offset);
    g_CurrentPlayer->field_2A70 = get_tile_by_offset(h->field_2A70_offset);

    g_CurrentPlayer->autoaim_target_y =
        get_enabled_prop_by_index(h->autoaim_target_y_index);
    g_CurrentPlayer->autoaim_target_x =
        get_enabled_prop_by_index(h->autoaim_target_x_index);

    /* Recalculate derived look parameters. */
    bondviewApplyVertaTheta();

    /* Sync player's world PropRecord. */
    if (h->has_prop) {
      if (g_CurrentPlayer->prop != NULL) {
        chrpropDeregisterRooms(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->pos = h->prop_pos;
        g_CurrentPlayer->prop->stan = get_tile_by_offset(h->prop_stan_offset);
        g_CurrentPlayer->prop->rooms[0] = h->prop_rooms[0];
        g_CurrentPlayer->prop->rooms[1] = h->prop_rooms[1];
        g_CurrentPlayer->prop->rooms[2] = h->prop_rooms[2];
        g_CurrentPlayer->prop->rooms[3] = h->prop_rooms[3];
        chrpropRegisterRooms(g_CurrentPlayer->prop);
      } else {
        g_CurrentPlayer->prop = chrpropAllocate();
        if (g_CurrentPlayer->prop != NULL) {
          g_CurrentPlayer->prop->obj = NULL;
          g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;
          g_CurrentPlayer->prop->pos = h->prop_pos;
          g_CurrentPlayer->prop->stan = get_tile_by_offset(h->prop_stan_offset);
          g_CurrentPlayer->prop->rooms[0] = h->prop_rooms[0];
          g_CurrentPlayer->prop->rooms[1] = h->prop_rooms[1];
          g_CurrentPlayer->prop->rooms[2] = h->prop_rooms[2];
          g_CurrentPlayer->prop->rooms[3] = h->prop_rooms[3];
          chrpropActivate(g_CurrentPlayer->prop);
          chrpropEnable(g_CurrentPlayer->prop);
          chrpropRegisterRooms(g_CurrentPlayer->prop);
        }
      }
    } else {
      if (g_CurrentPlayer->prop != NULL) {
        chrpropDeregisterRooms(g_CurrentPlayer->prop);
        chrpropDelist(g_CurrentPlayer->prop);
        chrpropDisable(g_CurrentPlayer->prop);
        chrpropFree(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop = NULL;
      }
    }

    /* Sync 3D visual Model instance. */
    if (g_CurrentPlayer->ptr_char_objectinstance != NULL) {
      setsuboffset(g_CurrentPlayer->ptr_char_objectinstance,
                   &g_CurrentPlayer->pos);
      setsubroty(g_CurrentPlayer->ptr_char_objectinstance,
                 get_curplay_horizontal_rotation_in_degrees());
    }
  }

  /* 4. Re-initialize inventory, then read and apply inventory section. */
  {
    BondInventorySection inventory;
    BondInventorySection *inv = &inventory;
    s32 i;

    bondinvReinitInv();
    g_CurrentPlayer->textoverrides = live_textoverrides;

    sram_stream_read_bytes(stream, inv, sizeof(*inv));

    for (i = 0; i < inv->num_inv_items; i++) {
      s32 type = inv->inv_items[i].type;
      if (type == INV_ITEM_WEAPON) {
        bondinvAddInvItem(inv->inv_items[i].weapon_right);
      } else if (type == INV_ITEM_DUAL) {
        bondinvAddDoublesInvItem(inv->inv_items[i].weapon_right,
                                 inv->inv_items[i].weapon_left);
      } else if (type == INV_ITEM_PROP) {
        PropRecord *prop = get_prop_by_index(inv->inv_items[i].prop_index);
        if (prop != NULL) {
          bondinvAddPropToInv(prop);
        }
      }
    }
  }

  /* 5. Restore hand weapon model loading and logical animation triggers. */
  {
    /* block2 lives at player+0x0870; the hands array is at the start. */
    struct hand *saved_hands = (struct hand *)((u8 *)g_CurrentPlayer + 0x0870);
    s32 hand;
    for (hand = 0; hand < 2; hand++) {
      ITEM_IDS saved_weapon = saved_hands[hand].weaponnum;

      if (preload_hand_item[hand] != saved_weapon) {
        g_CurrentPlayer->hand_invisible[hand] = -3;
        g_CurrentPlayer->field_2A44[hand] = saved_weapon;
        g_CurrentPlayer->hand_item[hand] = ITEM_UNARMED;
        g_CurrentPlayer->hands[hand].weapon_current_animation = 0;
        g_CurrentPlayer->lock_hand_model[hand] = 0;
        used_to_load_1st_person_model_on_demand(hand);
      } else {
        g_CurrentPlayer->hands[hand].weapon_current_animation =
            saved_hands[hand].weapon_current_animation;
      }

      g_CurrentPlayer->hands[hand].audioHandle = NULL;
    }
  }

  /* 6. Re-generate watch menu GDLs. */
  {
    extern Gfx *sub_GAME_7F0A3330(Gfx * arg0, void *arg1, s32 arg2);
    extern void sub_GAME_7F0A69A8(void);
    extern struct WatchVertex *setup_watch_rectangles(
        struct WatchVertex * vtx, s32 startx, s32 startz, s32 width, s32 height,
        s32 horizontal_offset, s32 vertical_offset);
    extern Gfx *sub_GAME_7F0A3B40(Gfx * gdl, s32 * arg1);

    sub_GAME_7F0A3330(
        (Gfx *)&g_CurrentPlayer->watch_body_armor_bar_gdl,
        (void *)OS_K0_TO_PHYSICAL(&g_CurrentPlayer->armor_display_values),
        0x2E);
    sub_GAME_7F0A3330(
        (Gfx *)&g_CurrentPlayer->watch_health_bar_gdl,
        (void *)OS_K0_TO_PHYSICAL(&g_CurrentPlayer->health_display_values),
        0x2E);

    sub_GAME_7F0A69A8();

    {
      Gfx *ptr_b = g_CurrentPlayer->buffer_for_watch_greenbackdrop_DL;
      struct WatchVertex *ptr_a =
          &g_CurrentPlayer->buffer_for_watch_greenbackdrop_vertices->vtx[0];
      s32 i;
      for (i = 0;
           i < (WATCH_NUMBER_SCREENS * WATCH_SCREEN_SELECT_RECTANGLE_HSTEP);
           i += WATCH_SCREEN_SELECT_RECTANGLE_HSTEP) {
        struct WatchVertex *ptr_copy = ptr_a;
        ptr_a = setup_watch_rectangles(ptr_a, i, 0, 0x64, 0x14, -0x12B, 0x136);
        ptr_b = sub_GAME_7F0A3B40(ptr_b, (s32 *)OS_K0_TO_PHYSICAL(ptr_copy));
      }
      gSPEndDisplayList(ptr_b);

      {
        struct WatchVertex *ptr_copy =
            (struct WatchVertex *)
                g_CurrentPlayer->buffer_for_watch_static_vertices;
        ptr_b = g_CurrentPlayer->buffer_for_watch_static_DL;
        setup_watch_rectangles(
            &g_CurrentPlayer->buffer_for_watch_static_vertices->vtx[0], 0, 0,
            0x398, 0x14, -0x1CC, 0);
        ptr_b = sub_GAME_7F0A3B40(ptr_b, (s32 *)OS_K0_TO_PHYSICAL(ptr_copy));
        gSPEndDisplayList(ptr_b);
      }
    }
  }
}
