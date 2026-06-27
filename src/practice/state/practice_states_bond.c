#include "bondinv.h"
#include "bondview.h"
#include "chrai.h"
#include "objecthandler.h"
#include "player.h"
#include "player_2.h"
#include "practice_states.h"
#include "practice_states_utils.h"
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

static const struct {
  u32 srcoff;
  u32 size;
} player_blocks[5] = {
    {0x0000, 0x0808}, {0x0870, 0x0854}, {0x10F0, 0x00B8},
    {0x11B8, 0x06C0}, {0x29B8, 0x00B8},
};

static void save_player_state_direct(StateStream *stream, struct player *src) {
  u8 *src_bytes = (u8 *)src;
  s32 i;
  for (i = 0; i < 5; i++) {
    write_bytes(stream, src_bytes + player_blocks[i].srcoff,
                player_blocks[i].size);
  }
}

static void load_player_state_direct(StateStream *stream, struct player *dst) {
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
    read_bytes(stream, dst_bytes + player_blocks[i].srcoff,
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

static void save_current_player_state(StateStream *stream) {
  if (g_CurrentPlayer == NULL) {
    return;
  }

  /* 1. Player blocks — direct to storage. */
  save_player_state_direct(stream, g_CurrentPlayer);

  /* 2. Bond helper section (sparse fields). */
  write_u32(stream, get_tile_offset(g_CurrentPlayer->room_pointer));
  write_u32(stream,
            get_tile_offset(g_CurrentPlayer->field_488.current_tile_ptr));
  write_u32(
      stream,
      get_tile_offset(g_CurrentPlayer->field_488.current_tile_ptr_for_portals));
  write_u32(stream,
            get_tile_offset(
                g_CurrentPlayer->previous_collision_info.current_tile_ptr));
  write_u32(stream, get_tile_offset(g_CurrentPlayer->previous_collision_info
                                        .current_tile_ptr_for_portals));
  write_u32(stream, get_tile_offset(g_CurrentPlayer->field_2A70));
  write_u32(stream, get_prop_index(g_CurrentPlayer->autoaim_target_y));
  write_u32(stream, get_prop_index(g_CurrentPlayer->autoaim_target_x));

  if (g_CurrentPlayer->prop != NULL) {
    write_u8(stream, TRUE);
    write_bytes(stream, &g_CurrentPlayer->prop->pos, sizeof(coord3d));
    write_u32(stream, get_tile_offset(g_CurrentPlayer->prop->stan));
    write_u8(stream, g_CurrentPlayer->prop->rooms[0]);
    write_u8(stream, g_CurrentPlayer->prop->rooms[1]);
    write_u8(stream, g_CurrentPlayer->prop->rooms[2]);
    write_u8(stream, g_CurrentPlayer->prop->rooms[3]);
  } else {
    write_u8(stream, FALSE);
  }

  /* 3. Inventory section. */
  {
    InvItem *first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    InvItem *item = first;
    s32 count = 0;
    s32 i;

    if (item != NULL) {
      do {
        count++;
        item = item->next;
      } while (item != first && item != NULL);
    }

    write_u32(stream, count);

    item = first;
    for (i = 0; i < count; i++) {
      s32 weapon_right = -1;
      s32 weapon_left = -1;
      s32 prop_index = -1;

      write_u32(stream, item->type);
      if (item->type == INV_ITEM_WEAPON) {
        weapon_right = item->type_inv_item.type_weap.weapon;
      } else if (item->type == INV_ITEM_DUAL) {
        weapon_right = item->type_inv_item.type_dual.weapon_right;
        weapon_left = item->type_inv_item.type_dual.weapon_left;
      } else if (item->type == INV_ITEM_PROP) {
        prop_index = get_prop_index(item->type_inv_item.type_prop.prop);
      }
      write_u32(stream, weapon_right);
      write_u32(stream, weapon_left);
      write_u32(stream, prop_index);

      item = item->next;
    }
  }
}

static void load_current_player_state(StateStream *stream) {
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
    g_CurrentPlayer->room_pointer = get_tile_by_offset(read_u32(stream));
    g_CurrentPlayer->field_488.current_tile_ptr =
        get_tile_by_offset(read_u32(stream));
    g_CurrentPlayer->field_488.current_tile_ptr_for_portals =
        get_tile_by_offset(read_u32(stream));
    g_CurrentPlayer->previous_collision_info.current_tile_ptr =
        get_tile_by_offset(read_u32(stream));
    g_CurrentPlayer->previous_collision_info.current_tile_ptr_for_portals =
        get_tile_by_offset(read_u32(stream));
    g_CurrentPlayer->field_2A70 = get_tile_by_offset(read_u32(stream));

    g_CurrentPlayer->autoaim_target_y =
        get_enabled_prop_by_index(read_u32(stream));
    g_CurrentPlayer->autoaim_target_x =
        get_enabled_prop_by_index(read_u32(stream));

    /* Recalculate derived look parameters. */
    bondviewApplyVertaTheta();

    /* Sync player's world PropRecord. */
    if (read_u8(stream)) {
      coord3d prop_pos;
      s32 prop_stan_offset;
      u8 prop_rooms[4];

      read_bytes(stream, &prop_pos, sizeof(coord3d));
      prop_stan_offset = read_u32(stream);
      prop_rooms[0] = read_u8(stream);
      prop_rooms[1] = read_u8(stream);
      prop_rooms[2] = read_u8(stream);
      prop_rooms[3] = read_u8(stream);

      if (g_CurrentPlayer->prop != NULL) {
        chrpropDeregisterRooms(g_CurrentPlayer->prop);
        g_CurrentPlayer->prop->pos = prop_pos;
        g_CurrentPlayer->prop->stan = get_tile_by_offset(prop_stan_offset);
        g_CurrentPlayer->prop->rooms[0] = prop_rooms[0];
        g_CurrentPlayer->prop->rooms[1] = prop_rooms[1];
        g_CurrentPlayer->prop->rooms[2] = prop_rooms[2];
        g_CurrentPlayer->prop->rooms[3] = prop_rooms[3];
        chrpropRegisterRooms(g_CurrentPlayer->prop);
      } else {
        g_CurrentPlayer->prop = chrpropAllocate();
        if (g_CurrentPlayer->prop != NULL) {
          g_CurrentPlayer->prop->obj = NULL;
          g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;
          g_CurrentPlayer->prop->pos = prop_pos;
          g_CurrentPlayer->prop->stan = get_tile_by_offset(prop_stan_offset);
          g_CurrentPlayer->prop->rooms[0] = prop_rooms[0];
          g_CurrentPlayer->prop->rooms[1] = prop_rooms[1];
          g_CurrentPlayer->prop->rooms[2] = prop_rooms[2];
          g_CurrentPlayer->prop->rooms[3] = prop_rooms[3];
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
    s32 num_inv_items;
    s32 i;

    bondinvReinitInv();
    g_CurrentPlayer->textoverrides = live_textoverrides;

    num_inv_items = read_u32(stream);

    for (i = 0; i < num_inv_items; i++) {
      s32 type = read_u32(stream);
      s32 weapon_right = read_u32(stream);
      s32 weapon_left = read_u32(stream);
      s32 prop_index = read_u32(stream);

      if (type == INV_ITEM_WEAPON) {
        bondinvAddInvItem(weapon_right);
      } else if (type == INV_ITEM_DUAL) {
        bondinvAddDoublesInvItem(weapon_right, weapon_left);
      } else if (type == INV_ITEM_PROP) {
        PropRecord *prop = get_prop_by_index(prop_index);
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

bool save_viewer_players_state(StateStream *stream) {
  s32 original_player = get_cur_playernum();
  s32 player_count = getPlayerCount();
  s32 saved_count = 0;
  s32 i;

  for (i = 0; i < player_count; i++) {
    if (g_playerPointers[i] != NULL) {
      saved_count++;
    }
  }

  write_u8(stream, saved_count);

  for (i = 0; i < player_count; i++) {
    struct player *player = g_playerPointers[i];
    s16 viewer_prop_index;

    if (player == NULL) {
      continue;
    }

    viewer_prop_index = get_prop_index(player->prop);
    if (player->prop != NULL && player->prop->type != PROP_TYPE_VIEWER) {
      practiceLogWarn("Player %d has non-viewer prop type %d", i,
                      player->prop->type);
      set_cur_player(original_player);
      return FALSE;
    }

    write_u8(stream, i);
    write_u16(stream, viewer_prop_index);

    set_cur_player(i);
    save_current_player_state(stream);
  }

  set_cur_player(original_player);
  return TRUE;
}

bool load_viewer_players_state(StateStream *stream) {
  s32 live_player_count = getPlayerCount();
  s32 saved_count = read_u8(stream);
  s32 i;

  for (i = 0; i < saved_count; i++) {
    s32 player_index = read_u8(stream);
    s16 viewer_prop_index = (s16)read_u16(stream);
    PropRecord *viewer_prop = NULL;

    if (player_index < 0 || player_index >= live_player_count ||
        g_playerPointers[player_index] == NULL) {
      practiceLogWarn("Invalid saved player index %d", player_index);
      return FALSE;
    }

    if (viewer_prop_index >= 0) {
      viewer_prop = get_enabled_prop_by_index(viewer_prop_index);
      if (viewer_prop != NULL && viewer_prop->type != PROP_TYPE_VIEWER) {
        practiceLogWarn("Player %d prop %d has unexpected type %d",
                        player_index, viewer_prop_index, viewer_prop->type);
        return FALSE;
      }

      /*
       * While prop addition/removal is disabled, a saved viewer can be absent
       * when loading from a cutscene. Leave the live pointer in place so the
       * player loader can update it or allocate a replacement from its saved
       * position data.
       */
      if (viewer_prop != NULL) {
        g_playerPointers[player_index]->prop = viewer_prop;
      }
    }

    set_cur_player(player_index);
    load_current_player_state(stream);
  }

  return TRUE;
}
