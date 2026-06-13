#include "practice_states_bond.h"
#include "practice_states_globals.h"
#include "bondinv.h"
#include "bondview.h"
#include "watch.h"
#include "chr.h"
#include "gun.h"
#include "player.h"
#include "player_2.h"
#include "practice_ui.h"
#include "objecthandler.h"
#include "chrai.h"
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
extern PropRecord pos_data_entry[];
extern int bondinvAddPropToInv(PropRecord *prop);
extern s32 g_EnterTankAudioState;
extern void *memcpy(void *dst, const void *src, size_t count);

static s32 get_prop_index(PropRecord *prop) {
    if (prop == NULL) {
        return -1;
    }
    return prop - pos_data_entry;
}

static PropRecord *get_prop_by_index(s32 index) {
    if (index < 0 || index >= POS_DATA_ENTRY_LEN) {
        return NULL;
    }
    return &pos_data_entry[index];
}

static PropRecord *get_safe_prop_by_index(s32 index) {
    PropRecord *prop = get_prop_by_index(index);
    if (prop != NULL && (prop->flags & PROPFLAG_ENABLED)) {
        return prop;
    }
    return NULL;
}

extern struct StandTile *standTileStart;

static s32 get_tile_offset(StandTile *tile) {
    if (tile == NULL || standTileStart == NULL) {
        return -1;
    }
    return (s32)((u8 *)tile - (u8 *)standTileStart);
}

static StandTile *get_tile_by_offset(s32 offset) {
    if (offset < 0 || standTileStart == NULL) {
        return NULL;
    }
    return (StandTile *)((u8 *)standTileStart + offset);
}

typedef struct {
    s32 type;
    s32 weapon_right;
    s32 weapon_left;
    s32 prop_index;
} SavedInvItem;

typedef struct {
    // The core player state
    struct player saved_player;

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
    u8 prop_rooms[PROPRECORD_STAN_ROOM_LEN];

    // Custom state (Inventory)
    s32 num_inv_items;
    SavedInvItem inv_items[50];

    // Tank Physics and Turret State Tracking Fields (non-player globals)
    s32 in_tank_flag;
    s32 g_PlayerTankProp_index;
    s32 g_WorldTankProp_index;
    f32 g_PlayerTankYOffset;
    f32 g_TankTurnSpeed;
    f32 g_TankOrientationAngle;
    f32 tank_turret_unused_angle;
    f32 g_TankTurretVerticalAngle;
    f32 g_TankTurretVerticalAngleRelated;
    f32 g_TankTurretOrientationAngleRad;
    f32 g_TankTurretOrientationAngleDeg;
    f32 tank_turret_turn_speed;
    s32 g_BondCanEnterTank;
    f32 g_TankTurretAngle;
    f32 g_TankTurretTurn;
    s32 g_ExplodeTankOnDeathFlag;
    s32 g_TankDamagePenaltyTicks;
    s32 g_EnterTankAudioState;

    // Camera Mode and Cutscene Transition States (non-player globals)
    s32 g_CameraMode;
    s32 camera_mode;
    s32 g_CameraAfterCinema;
    f32 camera_transition_timer;
    s32 camera_fade_active;
    s32 intro_camera_index;
    s32 is_timer_active;
    s32 g_PlayerInvincible;
} SavedBondState;

static SavedBondState g_SavedBondState;
bool g_HasSavedState = FALSE;

void save_bond_state(void) {
    if (g_CurrentPlayer == NULL) {
        return;
    }

    // 1. Perform structural memcpy of the entire player struct
    memcpy(&g_SavedBondState.saved_player, g_CurrentPlayer, sizeof(struct player));

    // 2. Perform pointer-to-offset/index conversions for tile and prop pointers
    g_SavedBondState.room_pointer_offset = get_tile_offset(g_CurrentPlayer->room_pointer);
    g_SavedBondState.field_488_current_tile_ptr_offset = get_tile_offset(g_CurrentPlayer->field_488.current_tile_ptr);
    g_SavedBondState.field_488_current_tile_ptr_for_portals_offset = get_tile_offset(g_CurrentPlayer->field_488.current_tile_ptr_for_portals);
    g_SavedBondState.previous_collision_info_current_tile_ptr_offset = get_tile_offset(g_CurrentPlayer->previous_collision_info.current_tile_ptr);
    g_SavedBondState.previous_collision_info_current_tile_ptr_for_portals_offset = get_tile_offset(g_CurrentPlayer->previous_collision_info.current_tile_ptr_for_portals);
    g_SavedBondState.field_2A70_offset = get_tile_offset(g_CurrentPlayer->field_2A70);

    g_SavedBondState.autoaim_target_y_index = get_prop_index(g_CurrentPlayer->autoaim_target_y);
    g_SavedBondState.autoaim_target_x_index = get_prop_index(g_CurrentPlayer->autoaim_target_x);

    // 3. Save player's physical prop state
    if (g_CurrentPlayer->prop != NULL) {
        g_SavedBondState.has_prop = TRUE;
        g_SavedBondState.prop_pos = g_CurrentPlayer->prop->pos;
        g_SavedBondState.prop_stan_offset = get_tile_offset(g_CurrentPlayer->prop->stan);
        g_SavedBondState.prop_rooms[0] = g_CurrentPlayer->prop->rooms[0];
        g_SavedBondState.prop_rooms[1] = g_CurrentPlayer->prop->rooms[1];
        g_SavedBondState.prop_rooms[2] = g_CurrentPlayer->prop->rooms[2];
        g_SavedBondState.prop_rooms[3] = g_CurrentPlayer->prop->rooms[3];
    } else {
        g_SavedBondState.has_prop = FALSE;
    }

    // 4. Save player inventory linked list to flat array
    {
        s32 i;
        for (i = 0; i < 50; i++) {
            g_SavedBondState.inv_items[i].type = -1;
            g_SavedBondState.inv_items[i].weapon_right = -1;
            g_SavedBondState.inv_items[i].weapon_left = -1;
            g_SavedBondState.inv_items[i].prop_index = -1;
        }
    }
    g_SavedBondState.num_inv_items = 0;

    {
        InvItem *first = g_CurrentPlayer->ptr_inventory_first_in_cycle;
        InvItem *item = first;
        s32 count = 0;
        if (item != NULL) {
            do {
                if (count >= 50) break;
                g_SavedBondState.inv_items[count].type = item->type;
                if (item->type == INV_ITEM_WEAPON) {
                    g_SavedBondState.inv_items[count].weapon_right = item->type_inv_item.type_weap.weapon;
                } else if (item->type == INV_ITEM_DUAL) {
                    g_SavedBondState.inv_items[count].weapon_right = item->type_inv_item.type_dual.weapon_right;
                    g_SavedBondState.inv_items[count].weapon_left = item->type_inv_item.type_dual.weapon_left;
                } else if (item->type == INV_ITEM_PROP) {
                    g_SavedBondState.inv_items[count].prop_index = get_prop_index(item->type_inv_item.type_prop.prop);
                }
                count++;
                item = item->next;
            } while (item != first && item != NULL);
        }
        g_SavedBondState.num_inv_items = count;
    }

    // 5. Save non-player global tracking fields (tank physics state)
    g_SavedBondState.in_tank_flag = in_tank_flag;
    g_SavedBondState.g_PlayerTankProp_index = get_prop_index(g_PlayerTankProp);
    g_SavedBondState.g_WorldTankProp_index = get_prop_index(g_WorldTankProp);
    g_SavedBondState.g_PlayerTankYOffset = g_PlayerTankYOffset;
    g_SavedBondState.g_TankTurnSpeed = g_TankTurnSpeed;
    g_SavedBondState.g_TankOrientationAngle = g_TankOrientationAngle;
    g_SavedBondState.tank_turret_unused_angle = tank_turret_unused_angle;
    g_SavedBondState.g_TankTurretVerticalAngle = g_TankTurretVerticalAngle;
    g_SavedBondState.g_TankTurretVerticalAngleRelated = g_TankTurretVerticalAngleRelated;
    g_SavedBondState.g_TankTurretOrientationAngleRad = g_TankTurretOrientationAngleRad;
    g_SavedBondState.g_TankTurretOrientationAngleDeg = g_TankTurretOrientationAngleDeg;
    g_SavedBondState.tank_turret_turn_speed = tank_turret_turn_speed;
    g_SavedBondState.g_BondCanEnterTank = g_BondCanEnterTank;
    g_SavedBondState.g_TankTurretAngle = g_TankTurretAngle;
    g_SavedBondState.g_TankTurretTurn = g_TankTurretTurn;
    g_SavedBondState.g_ExplodeTankOnDeathFlag = g_ExplodeTankOnDeathFlag;
    g_SavedBondState.g_TankDamagePenaltyTicks = g_TankDamagePenaltyTicks;
    g_SavedBondState.g_EnterTankAudioState = g_EnterTankAudioState;

    // 6. Save non-player global tracking fields (camera mode & cutscene timers)
    g_SavedBondState.g_CameraMode = g_CameraMode;
    g_SavedBondState.camera_mode = camera_mode;
    g_SavedBondState.g_CameraAfterCinema = g_CameraAfterCinema;
    g_SavedBondState.camera_transition_timer = camera_transition_timer;
    g_SavedBondState.camera_fade_active = camera_fade_active;
    g_SavedBondState.intro_camera_index = intro_camera_index;
    g_SavedBondState.is_timer_active = is_timer_active;
    g_SavedBondState.g_PlayerInvincible = g_PlayerInvincible;

    g_HasSavedState = TRUE;
}

void load_bond_state(void) {
    PropRecord *curr_prop;
    struct Model *curr_char_obj;
    Model *curr_model;
    textoverride *curr_textoverrides;

    Mtx *curr_10C4;
    Mtx *curr_10C8;
    Mtxf *curr_10CC;
    Mtxf *curr_10D4;
    Mtx *curr_proj;
    Mtxf *curr_projf;
    Mtxf *curr_10E8;
    Mtxf *curr_10EC;

    ObjectRecord *curr_rocket0;
    ObjectRecord *curr_rocket1;

    u8 *curr_blood_cur;
    u8 *curr_blood_nxt;

    s32 preload_hand_item[2];

    if (g_CurrentPlayer == NULL) {
        return;
    }

    // 1. Backup live active pointers that must not be overwritten
    curr_prop = g_CurrentPlayer->prop;
    curr_char_obj = g_CurrentPlayer->ptr_char_objectinstance;
    curr_model = g_CurrentPlayer->model;
    curr_textoverrides = g_CurrentPlayer->textoverrides;

    curr_10C4 = g_CurrentPlayer->field_10C4;
    curr_10C8 = g_CurrentPlayer->field_10C8;
    curr_10CC = g_CurrentPlayer->field_10CC;
    curr_10D4 = g_CurrentPlayer->field_10D4;
    curr_proj = g_CurrentPlayer->projmatrix;
    curr_projf = g_CurrentPlayer->projmatrixf;
    curr_10E8 = g_CurrentPlayer->field_10E8;
    curr_10EC = g_CurrentPlayer->field_10EC;

    curr_rocket0 = g_CurrentPlayer->hands[0].rocket;
    curr_rocket1 = g_CurrentPlayer->hands[1].rocket;

    curr_blood_cur = g_CurrentPlayer->bloodImgCur;
    curr_blood_nxt = g_CurrentPlayer->bloodImgNxt;

    preload_hand_item[0] = g_CurrentPlayer->hand_item[0];
    preload_hand_item[1] = g_CurrentPlayer->hand_item[1];

    // 2. Perform the full memcpy from saved state
    memcpy(g_CurrentPlayer, &g_SavedBondState.saved_player, sizeof(struct player));

    // 3. Restore the live pointers to their current active memory locations
    g_CurrentPlayer->prop = curr_prop;
    g_CurrentPlayer->ptr_char_objectinstance = curr_char_obj;
    g_CurrentPlayer->model = curr_model;
    g_CurrentPlayer->textoverrides = curr_textoverrides;

    g_CurrentPlayer->field_10C4 = curr_10C4;
    g_CurrentPlayer->field_10C8 = curr_10C8;
    g_CurrentPlayer->field_10CC = curr_10CC;
    g_CurrentPlayer->field_10D4 = curr_10D4;
    g_CurrentPlayer->projmatrix = curr_proj;
    g_CurrentPlayer->projmatrixf = curr_projf;
    g_CurrentPlayer->field_10E8 = curr_10E8;
    g_CurrentPlayer->field_10EC = curr_10EC;

    g_CurrentPlayer->hands[0].rocket = curr_rocket0;
    g_CurrentPlayer->hands[1].rocket = curr_rocket1;

    g_CurrentPlayer->bloodImgCur = curr_blood_cur;
    g_CurrentPlayer->bloodImgNxt = curr_blood_nxt;

    // 4. Resolve tile pointers and target prop pointers from saved offsets/indices
    g_CurrentPlayer->room_pointer = get_tile_by_offset(g_SavedBondState.room_pointer_offset);
    g_CurrentPlayer->field_488.current_tile_ptr = get_tile_by_offset(g_SavedBondState.field_488_current_tile_ptr_offset);
    g_CurrentPlayer->field_488.current_tile_ptr_for_portals = get_tile_by_offset(g_SavedBondState.field_488_current_tile_ptr_for_portals_offset);
    g_CurrentPlayer->previous_collision_info.current_tile_ptr = get_tile_by_offset(g_SavedBondState.previous_collision_info_current_tile_ptr_offset);
    g_CurrentPlayer->previous_collision_info.current_tile_ptr_for_portals = get_tile_by_offset(g_SavedBondState.previous_collision_info_current_tile_ptr_for_portals_offset);
    g_CurrentPlayer->field_2A70 = get_tile_by_offset(g_SavedBondState.field_2A70_offset);

    g_CurrentPlayer->autoaim_target_y = get_safe_prop_by_index(g_SavedBondState.autoaim_target_y_index);
    g_CurrentPlayer->autoaim_target_x = get_safe_prop_by_index(g_SavedBondState.autoaim_target_x_index);

    // 5. Recalculate derived look parameters from restored vv_theta / vv_verta
    bondviewApplyVertaTheta();

    // 6. Sync player's world PropRecord properties (rather than overwriting pointer)
    if (g_SavedBondState.has_prop) {
        if (g_CurrentPlayer->prop != NULL) {
            chrpropDeregisterRooms(g_CurrentPlayer->prop);

            g_CurrentPlayer->prop->pos = g_SavedBondState.prop_pos;
            g_CurrentPlayer->prop->stan = get_tile_by_offset(g_SavedBondState.prop_stan_offset);
            g_CurrentPlayer->prop->rooms[0] = g_SavedBondState.prop_rooms[0];
            g_CurrentPlayer->prop->rooms[1] = g_SavedBondState.prop_rooms[1];
            g_CurrentPlayer->prop->rooms[2] = g_SavedBondState.prop_rooms[2];
            g_CurrentPlayer->prop->rooms[3] = g_SavedBondState.prop_rooms[3];

            chrpropRegisterRooms(g_CurrentPlayer->prop);
        } else {
            g_CurrentPlayer->prop = chrpropAllocate();
            if (g_CurrentPlayer->prop != NULL) {
                g_CurrentPlayer->prop->obj = NULL;
                g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;
                g_CurrentPlayer->prop->pos = g_SavedBondState.prop_pos;
                g_CurrentPlayer->prop->stan = get_tile_by_offset(g_SavedBondState.prop_stan_offset);
                g_CurrentPlayer->prop->rooms[0] = g_SavedBondState.prop_rooms[0];
                g_CurrentPlayer->prop->rooms[1] = g_SavedBondState.prop_rooms[1];
                g_CurrentPlayer->prop->rooms[2] = g_SavedBondState.prop_rooms[2];
                g_CurrentPlayer->prop->rooms[3] = g_SavedBondState.prop_rooms[3];

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

    // 7. Sync player's 3D visual Model instance
    if (g_CurrentPlayer->ptr_char_objectinstance != NULL) {
        setsuboffset(g_CurrentPlayer->ptr_char_objectinstance, &g_CurrentPlayer->pos);
        setsubroty(g_CurrentPlayer->ptr_char_objectinstance, get_curplay_horizontal_rotation_in_degrees());
    }

    // 8. Re-initialize player inventory, preserving text overrides
    {
        textoverride *saved_overrides = g_CurrentPlayer->textoverrides;
        bondinvReinitInv();
        g_CurrentPlayer->textoverrides = saved_overrides;
    }

    // 9. Restore saved inventory items
    {
        s32 i;
        for (i = 0; i < g_SavedBondState.num_inv_items; i++) {
            s32 type = g_SavedBondState.inv_items[i].type;
            if (type == INV_ITEM_WEAPON) {
                bondinvAddInvItem(g_SavedBondState.inv_items[i].weapon_right);
            } else if (type == INV_ITEM_DUAL) {
                bondinvAddDoublesInvItem(g_SavedBondState.inv_items[i].weapon_right, g_SavedBondState.inv_items[i].weapon_left);
            } else if (type == INV_ITEM_PROP) {
                PropRecord *prop = get_prop_by_index(g_SavedBondState.inv_items[i].prop_index);
                if (prop != NULL) {
                    bondinvAddPropToInv(prop);
                }
            }
        }
    }

    // 10. Restore hand weapon model loading and logical animation triggers
    {
        s32 hand;
        for (hand = 0; hand < 2; hand++) {
            ITEM_IDS saved_weapon = g_SavedBondState.saved_player.hands[hand].weaponnum;

            if (preload_hand_item[hand] != saved_weapon) {
                g_CurrentPlayer->hand_invisible[hand] = -3;
                g_CurrentPlayer->field_2A44[hand] = saved_weapon;
                g_CurrentPlayer->hand_item[hand] = ITEM_UNARMED;
                g_CurrentPlayer->hands[hand].weapon_current_animation = 0;
                g_CurrentPlayer->lock_hand_model[hand] = 0;
                used_to_load_1st_person_model_on_demand(hand);
            } else {
                g_CurrentPlayer->hands[hand].weapon_current_animation = g_SavedBondState.saved_player.hands[hand].weapon_current_animation;
            }

            g_CurrentPlayer->hands[hand].audioHandle = NULL;
        }
    }

    // 11. Restore non-player global tracking fields (tank physics state)
    in_tank_flag = g_SavedBondState.in_tank_flag;
    g_PlayerTankProp = get_safe_prop_by_index(g_SavedBondState.g_PlayerTankProp_index);
    g_WorldTankProp = get_safe_prop_by_index(g_SavedBondState.g_WorldTankProp_index);
    g_PlayerTankYOffset = g_SavedBondState.g_PlayerTankYOffset;
    g_TankTurnSpeed = g_SavedBondState.g_TankTurnSpeed;
    g_TankOrientationAngle = g_SavedBondState.g_TankOrientationAngle;
    tank_turret_unused_angle = g_SavedBondState.tank_turret_unused_angle;
    g_TankTurretVerticalAngle = g_SavedBondState.g_TankTurretVerticalAngle;
    g_TankTurretVerticalAngleRelated = g_SavedBondState.g_TankTurretVerticalAngleRelated;
    g_TankTurretOrientationAngleRad = g_SavedBondState.g_TankTurretOrientationAngleRad;
    g_TankTurretOrientationAngleDeg = g_SavedBondState.g_TankTurretOrientationAngleDeg;
    tank_turret_turn_speed = g_SavedBondState.tank_turret_turn_speed;
    g_BondCanEnterTank = g_SavedBondState.g_BondCanEnterTank;
    g_TankTurretAngle = g_SavedBondState.g_TankTurretAngle;
    g_TankTurretTurn = g_SavedBondState.g_TankTurretTurn;
    g_ExplodeTankOnDeathFlag = g_SavedBondState.g_ExplodeTankOnDeathFlag;
    g_TankDamagePenaltyTicks = g_SavedBondState.g_TankDamagePenaltyTicks;
    g_EnterTankAudioState = g_SavedBondState.g_EnterTankAudioState;

    // 12. Restore camera mode & cutscene transition states (globals)
    g_CameraMode = g_SavedBondState.g_CameraMode;
    camera_mode = g_SavedBondState.camera_mode;
    g_CameraAfterCinema = g_SavedBondState.g_CameraAfterCinema;
    camera_transition_timer = g_SavedBondState.camera_transition_timer;
    camera_fade_active = g_SavedBondState.camera_fade_active;
    intro_camera_index = g_SavedBondState.intro_camera_index;
    is_timer_active = g_SavedBondState.is_timer_active;
    g_PlayerInvincible = g_SavedBondState.g_PlayerInvincible;

    // 13. Re-generate watch menu GDLs and backdrop segment pointers to update absolute physical addresses
    {
        extern Gfx *sub_GAME_7F0A3330(Gfx *arg0, void *arg1, s32 arg2);
        extern void sub_GAME_7F0A69A8(void);
        extern struct WatchVertex *setup_watch_rectangles(struct WatchVertex *vtx, s32 startx, s32 startz, s32 width, s32 height, s32 horizontal_offset, s32 vertical_offset);
        extern Gfx *sub_GAME_7F0A3B40(Gfx *gdl, s32 *arg1);

        sub_GAME_7F0A3330(&g_CurrentPlayer->watch_body_armor_bar_gdl, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->armor_display_values), 0x2E);
        sub_GAME_7F0A3330(&g_CurrentPlayer->watch_health_bar_gdl, OS_K0_TO_PHYSICAL(&g_CurrentPlayer->health_display_values), 0x2E);

        sub_GAME_7F0A69A8();

        {
            Gfx *ptr_b = g_CurrentPlayer->buffer_for_watch_greenbackdrop_DL;
            struct WatchVertex *ptr_a = &g_CurrentPlayer->buffer_for_watch_greenbackdrop_vertices->vtx[0];
            s32 i;
            for (i = 0; i < (WATCH_NUMBER_SCREENS * WATCH_SCREEN_SELECT_RECTANGLE_HSTEP); i += WATCH_SCREEN_SELECT_RECTANGLE_HSTEP) {
                struct WatchVertex *ptr_copy = ptr_a;
                ptr_a = setup_watch_rectangles(ptr_a, i, 0, 0x64, 0x14, -0x12B, 0x136);
                ptr_b = sub_GAME_7F0A3B40(ptr_b, OS_K0_TO_PHYSICAL(ptr_copy));
            }
            gSPEndDisplayList(ptr_b);

            {
                struct WatchVertex *ptr_copy = (struct WatchVertex *)g_CurrentPlayer->buffer_for_watch_static_vertices;
                ptr_b = g_CurrentPlayer->buffer_for_watch_static_DL;
                setup_watch_rectangles(&g_CurrentPlayer->buffer_for_watch_static_vertices->vtx[0], 0, 0, 0x398, 0x14, -0x1CC, 0);
                ptr_b = sub_GAME_7F0A3B40(ptr_b, OS_K0_TO_PHYSICAL(ptr_copy));
                gSPEndDisplayList(ptr_b);
            }
        }
    }
}
