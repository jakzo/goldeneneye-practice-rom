#include "practice_states.h"
#include "bondinv.h"
#include "bondview.h"
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

extern void bondviewApplyVertaTheta(void);
extern void chrpropDelist(PropRecord *prop);
extern PropRecord pos_data_entry[];
extern int bondinvAddPropToInv(PropRecord *prop);

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

typedef struct {
    s32 type;
    s32 weapon_right;
    s32 weapon_left;
    s32 prop_index;
} SavedInvItem;

typedef struct {
    // Control / Camera State
    s32 unknown;

    // Position
    coord3d pos;
    coord3d pos2;
    coord3d pos3;
    coord3d offset;
    StandTile *room_pointer;
    coord3d current_model_pos;
    coord3d previous_model_pos;
    coord3d current_room_pos;
    s32 curRoomIndex;
    coord3d bondprevpos;

    // Collision
    struct collision434 field_488;
    struct collision434 previous_collision_info;

    // Angles
    f32 vv_theta;
    f32 vv_verta;

    // Movement speeds / physics
    f32 speedsideways;
    f32 speedstrafe;
    f32 speedforwards;
    s32 speedmaxtime60;
    f32 speedboost;
    f32 vertical_bounce_adjust;
    f32 ducking_height_offset;
    s32 crouchpos;

    // 3D Velocity
    f32 field_78;
    f32 field_7C;
    f32 field_80;
    f32 speedgo;
    coord3d bondshotspeed;
    f32 speedtheta;
    f32 speedverta;

    // Bounce Dampeners
    f32 field_84;
    f32 field_88;

    // Height Tracking
    f32 field_6C;
    f32 field_70;
    f32 stanHeight;

    // Smoothed / Render Coordinates
    coord3d field_3B8;
    f32 field_3C4;
    f32 field_3C8;
    f32 field_3CC;

    // Walk / Head Bob Animation State
    s32 resetheadpos;
    s32 resetheadrot;
    s32 resetheadtick;
    s32 headanim;
    f32 headdamp;
    s32 headwalkingtime60;
    f32 headamplitude;
    f32 sideamplitude;
    coord3d headpos;
    coord3d headlook;
    coord3d headup;
    coord3d headpossum;
    coord3d headlooksum;
    coord3d headupsum;
    coord3d headbodyoffset;
    f32 standheight;
    coord3d standbodyoffset;
    f32 standfrac;
    coord3d standlook[2];
    coord3d standup[2];
    s32 standcnt;

    // Health / Armor
    f32 bondhealth;
    f32 bondarmour;
    f32 oldhealth;
    f32 oldarmour;
    f32 apparenthealth;
    f32 apparentarmour;

    // Prop state
    bool has_prop;
    coord3d prop_pos;
    StandTile *prop_stan;
    u8 prop_rooms[PROPRECORD_STAN_ROOM_LEN];

    // Inventory & Ammo state
    s32 ammoheldarr[30];
    s32 equipcuritem;
    s32 equipallguns;
    s32 hand_item[2];
    s32 num_inv_items;
    SavedInvItem inv_items[50];

    // Hand Weapon logical state
    s32 hands_weaponnum[2];
    s32 hands_weaponnum_watchmenu[2];
    s32 hands_previous_weapon[2];
    s32 hands_weapon_firing_status[2];
    s32 hands_weapon_hold_time[2];
    s32 hands_when_detonating_mines_is_0[2];
    s32 hands_weapon_current_animation[2];
    s32 hands_weapon_ammo_in_magazine[2];
    s32 hands_weapon_next_weapon[2];
    s32 hands_weapon_animation_trigger[2];
    s32 hands_field_8B8[2];
    s32 lock_hand_model[2];

    // Cheat status
    u8 bondinvincible;

    // HUD Display states
#if defined(VERSION_JP) || defined(VERSION_EU)
    f32 damageshowtime;
    f32 healthshowtime;
#else
    s32 damageshowtime;
    s32 healthshowtime;
#endif
    s32 healthshowmode;

    // Fading & Breathing
    f32 bondfadetime60;
    f32 bondfadetimemax60;
    f32 bondfadefracold;
    f32 bondfadefracnew;
    f32 bondbreathing;

    // Weapon Sway cycle
    f32 gunposamplitude;
    f32 gunsync;
    f32 syncchange;
    f32 synccount;
    s32 syncoffset;
    f32 field_107C;
    f32 field_1080;
} SavedBondState;

static SavedBondState g_SavedBondState;
static bool g_HasSavedState = FALSE;

static void save_bond_state(void) {
    if (g_CurrentPlayer == NULL) {
        return;
    }

    // Control / Camera State
    g_SavedBondState.unknown = g_CurrentPlayer->unknown;

    // Position
    g_SavedBondState.pos = g_CurrentPlayer->pos;
    g_SavedBondState.pos2 = g_CurrentPlayer->pos2;
    g_SavedBondState.pos3 = g_CurrentPlayer->pos3;
    g_SavedBondState.offset = g_CurrentPlayer->offset;
    g_SavedBondState.room_pointer = g_CurrentPlayer->room_pointer;
    g_SavedBondState.current_model_pos = g_CurrentPlayer->current_model_pos;
    g_SavedBondState.previous_model_pos = g_CurrentPlayer->previous_model_pos;
    g_SavedBondState.current_room_pos = g_CurrentPlayer->current_room_pos;
    g_SavedBondState.curRoomIndex = g_CurrentPlayer->curRoomIndex;
    g_SavedBondState.bondprevpos = g_CurrentPlayer->bondprevpos;

    // Collision
    g_SavedBondState.field_488 = g_CurrentPlayer->field_488;
    g_SavedBondState.previous_collision_info = g_CurrentPlayer->previous_collision_info;

    // Angles
    g_SavedBondState.vv_theta = g_CurrentPlayer->vv_theta;
    g_SavedBondState.vv_verta = g_CurrentPlayer->vv_verta;

    // Movement speeds / physics
    g_SavedBondState.speedsideways = g_CurrentPlayer->speedsideways;
    g_SavedBondState.speedstrafe = g_CurrentPlayer->speedstrafe;
    g_SavedBondState.speedforwards = g_CurrentPlayer->speedforwards;
    g_SavedBondState.speedmaxtime60 = g_CurrentPlayer->speedmaxtime60;
    g_SavedBondState.speedboost = g_CurrentPlayer->speedboost;
    g_SavedBondState.vertical_bounce_adjust = g_CurrentPlayer->vertical_bounce_adjust;
    g_SavedBondState.ducking_height_offset = g_CurrentPlayer->ducking_height_offset;
    g_SavedBondState.crouchpos = g_CurrentPlayer->crouchpos;

    // 3D Velocity
    g_SavedBondState.field_78 = g_CurrentPlayer->field_78;
    g_SavedBondState.field_7C = g_CurrentPlayer->field_7C;
    g_SavedBondState.field_80 = g_CurrentPlayer->field_80;
    g_SavedBondState.speedgo = g_CurrentPlayer->speedgo;
    g_SavedBondState.bondshotspeed = g_CurrentPlayer->bondshotspeed;
    g_SavedBondState.speedtheta = g_CurrentPlayer->speedtheta;
    g_SavedBondState.speedverta = g_CurrentPlayer->speedverta;

    // Bounce Dampeners
    g_SavedBondState.field_84 = g_CurrentPlayer->field_84;
    g_SavedBondState.field_88 = g_CurrentPlayer->field_88;

    // Height Tracking
    g_SavedBondState.field_6C = g_CurrentPlayer->field_6C;
    g_SavedBondState.field_70 = g_CurrentPlayer->field_70;
    g_SavedBondState.stanHeight = g_CurrentPlayer->stanHeight;

    // Smoothed / Render Coordinates
    g_SavedBondState.field_3B8 = g_CurrentPlayer->field_3B8;
    g_SavedBondState.field_3C4 = g_CurrentPlayer->field_3C4;
    g_SavedBondState.field_3C8 = g_CurrentPlayer->field_3C8;
    g_SavedBondState.field_3CC = g_CurrentPlayer->field_3CC;

    // Walk / Head Bob Animation State
    g_SavedBondState.resetheadpos = g_CurrentPlayer->resetheadpos;
    g_SavedBondState.resetheadrot = g_CurrentPlayer->resetheadrot;
    g_SavedBondState.resetheadtick = g_CurrentPlayer->resetheadtick;
    g_SavedBondState.headanim = g_CurrentPlayer->headanim;
    g_SavedBondState.headdamp = g_CurrentPlayer->headdamp;
    g_SavedBondState.headwalkingtime60 = g_CurrentPlayer->headwalkingtime60;
    g_SavedBondState.headamplitude = g_CurrentPlayer->headamplitude;
    g_SavedBondState.sideamplitude = g_CurrentPlayer->sideamplitude;
    g_SavedBondState.headpos = g_CurrentPlayer->headpos;
    g_SavedBondState.headlook = g_CurrentPlayer->headlook;
    g_SavedBondState.headup = g_CurrentPlayer->headup;
    g_SavedBondState.headpossum = g_CurrentPlayer->headpossum;
    g_SavedBondState.headlooksum = g_CurrentPlayer->headlooksum;
    g_SavedBondState.headupsum = g_CurrentPlayer->headupsum;
    g_SavedBondState.headbodyoffset = g_CurrentPlayer->headbodyoffset;
    g_SavedBondState.standheight = g_CurrentPlayer->standheight;
    g_SavedBondState.standbodyoffset = g_CurrentPlayer->standbodyoffset;
    g_SavedBondState.standfrac = g_CurrentPlayer->standfrac;
    g_SavedBondState.standlook[0] = g_CurrentPlayer->standlook[0];
    g_SavedBondState.standlook[1] = g_CurrentPlayer->standlook[1];
    g_SavedBondState.standup[0] = g_CurrentPlayer->standup[0];
    g_SavedBondState.standup[1] = g_CurrentPlayer->standup[1];
    g_SavedBondState.standcnt = g_CurrentPlayer->standcnt;

    // Health / Armor
    g_SavedBondState.bondhealth = g_CurrentPlayer->bondhealth;
    g_SavedBondState.bondarmour = g_CurrentPlayer->bondarmour;
    g_SavedBondState.oldhealth = g_CurrentPlayer->oldhealth;
    g_SavedBondState.oldarmour = g_CurrentPlayer->oldarmour;
    g_SavedBondState.apparenthealth = g_CurrentPlayer->apparenthealth;
    g_SavedBondState.apparentarmour = g_CurrentPlayer->apparentarmour;

    // Prop state
    if (g_CurrentPlayer->prop != NULL) {
        g_SavedBondState.has_prop = TRUE;
        g_SavedBondState.prop_pos = g_CurrentPlayer->prop->pos;
        g_SavedBondState.prop_stan = g_CurrentPlayer->prop->stan;
        g_SavedBondState.prop_rooms[0] = g_CurrentPlayer->prop->rooms[0];
        g_SavedBondState.prop_rooms[1] = g_CurrentPlayer->prop->rooms[1];
        g_SavedBondState.prop_rooms[2] = g_CurrentPlayer->prop->rooms[2];
        g_SavedBondState.prop_rooms[3] = g_CurrentPlayer->prop->rooms[3];
    } else {
        g_SavedBondState.has_prop = FALSE;
    }

    // Inventory & Ammo state
    {
        s32 i;
        for (i = 0; i < 30; i++) {
            g_SavedBondState.ammoheldarr[i] = g_CurrentPlayer->ammoheldarr[i];
        }
    }
    g_SavedBondState.equipcuritem = g_CurrentPlayer->equipcuritem;
    g_SavedBondState.equipallguns = g_CurrentPlayer->equipallguns;
    g_SavedBondState.hand_item[0] = g_CurrentPlayer->hand_item[0];
    g_SavedBondState.hand_item[1] = g_CurrentPlayer->hand_item[1];

    // Clear saved inventory items
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

    // Hand Weapon logical state
    {
        s32 i;
        for (i = 0; i < 2; i++) {
            g_SavedBondState.hands_weaponnum[i] = g_CurrentPlayer->hands[i].weaponnum;
            g_SavedBondState.hands_weaponnum_watchmenu[i] = g_CurrentPlayer->hands[i].weaponnum_watchmenu;
            g_SavedBondState.hands_previous_weapon[i] = g_CurrentPlayer->hands[i].previous_weapon;
            g_SavedBondState.hands_weapon_firing_status[i] = g_CurrentPlayer->hands[i].weapon_firing_status;
            g_SavedBondState.hands_weapon_hold_time[i] = g_CurrentPlayer->hands[i].weapon_hold_time;
            g_SavedBondState.hands_when_detonating_mines_is_0[i] = g_CurrentPlayer->hands[i].when_detonating_mines_is_0;
            g_SavedBondState.hands_weapon_current_animation[i] = g_CurrentPlayer->hands[i].weapon_current_animation;
            g_SavedBondState.hands_weapon_ammo_in_magazine[i] = g_CurrentPlayer->hands[i].weapon_ammo_in_magazine;
            g_SavedBondState.hands_weapon_next_weapon[i] = g_CurrentPlayer->hands[i].weapon_next_weapon;
            g_SavedBondState.hands_weapon_animation_trigger[i] = g_CurrentPlayer->hands[i].weapon_animation_trigger;
            g_SavedBondState.hands_field_8B8[i] = g_CurrentPlayer->hands[i].field_8B8;
            g_SavedBondState.lock_hand_model[i] = g_CurrentPlayer->lock_hand_model[i];
        }
    }

    // Cheat status
    g_SavedBondState.bondinvincible = g_CurrentPlayer->bondinvincible;

    // HUD Display states
    g_SavedBondState.damageshowtime = g_CurrentPlayer->damageshowtime;
    g_SavedBondState.healthshowtime = g_CurrentPlayer->healthshowtime;
    g_SavedBondState.healthshowmode = g_CurrentPlayer->healthshowmode;

    // Fading & Breathing
    g_SavedBondState.bondfadetime60 = g_CurrentPlayer->bondfadetime60;
    g_SavedBondState.bondfadetimemax60 = g_CurrentPlayer->bondfadetimemax60;
    g_SavedBondState.bondfadefracold = g_CurrentPlayer->bondfadefracold;
    g_SavedBondState.bondfadefracnew = g_CurrentPlayer->bondfadefracnew;
    g_SavedBondState.bondbreathing = g_CurrentPlayer->bondbreathing;

    // Weapon Sway cycle
    g_SavedBondState.gunposamplitude = g_CurrentPlayer->gunposamplitude;
    g_SavedBondState.gunsync = g_CurrentPlayer->gunsync;
    g_SavedBondState.syncchange = g_CurrentPlayer->syncchange;
    g_SavedBondState.synccount = g_CurrentPlayer->synccount;
    g_SavedBondState.syncoffset = g_CurrentPlayer->syncoffset;
    g_SavedBondState.field_107C = g_CurrentPlayer->field_107C;
    g_SavedBondState.field_1080 = g_CurrentPlayer->field_1080;
}

static void load_bond_state(void) {
    if (g_CurrentPlayer == NULL) {
        return;
    }

    // Control / Camera State
    g_CurrentPlayer->unknown = g_SavedBondState.unknown;

    // Position
    g_CurrentPlayer->pos = g_SavedBondState.pos;
    g_CurrentPlayer->pos2 = g_SavedBondState.pos2;
    g_CurrentPlayer->pos3 = g_SavedBondState.pos3;
    g_CurrentPlayer->offset = g_SavedBondState.offset;
    g_CurrentPlayer->room_pointer = g_SavedBondState.room_pointer;
    g_CurrentPlayer->current_model_pos = g_SavedBondState.current_model_pos;
    g_CurrentPlayer->previous_model_pos = g_SavedBondState.previous_model_pos;
    g_CurrentPlayer->current_room_pos = g_SavedBondState.current_room_pos;
    g_CurrentPlayer->curRoomIndex = g_SavedBondState.curRoomIndex;
    g_CurrentPlayer->bondprevpos = g_SavedBondState.bondprevpos;

    // Collision
    g_CurrentPlayer->field_488 = g_SavedBondState.field_488;
    g_CurrentPlayer->previous_collision_info = g_SavedBondState.previous_collision_info;

    // Angles
    g_CurrentPlayer->vv_theta = g_SavedBondState.vv_theta;
    g_CurrentPlayer->vv_verta = g_SavedBondState.vv_verta;

    // Recalculate derived look parameters from new vv_theta / vv_verta
    bondviewApplyVertaTheta();

    // Movement speeds / physics
    g_CurrentPlayer->speedsideways = g_SavedBondState.speedsideways;
    g_CurrentPlayer->speedstrafe = g_SavedBondState.speedstrafe;
    g_CurrentPlayer->speedforwards = g_SavedBondState.speedforwards;
    g_CurrentPlayer->speedmaxtime60 = g_SavedBondState.speedmaxtime60;
    g_CurrentPlayer->speedboost = g_SavedBondState.speedboost;
    g_CurrentPlayer->vertical_bounce_adjust = g_SavedBondState.vertical_bounce_adjust;
    g_CurrentPlayer->ducking_height_offset = g_SavedBondState.ducking_height_offset;
    g_CurrentPlayer->crouchpos = g_SavedBondState.crouchpos;

    // 3D Velocity
    g_CurrentPlayer->field_78 = g_SavedBondState.field_78;
    g_CurrentPlayer->field_7C = g_SavedBondState.field_7C;
    g_CurrentPlayer->field_80 = g_SavedBondState.field_80;
    g_CurrentPlayer->speedgo = g_SavedBondState.speedgo;
    g_CurrentPlayer->bondshotspeed = g_SavedBondState.bondshotspeed;
    g_CurrentPlayer->speedtheta = g_SavedBondState.speedtheta;
    g_CurrentPlayer->speedverta = g_SavedBondState.speedverta;

    // Bounce Dampeners
    g_CurrentPlayer->field_84 = g_SavedBondState.field_84;
    g_CurrentPlayer->field_88 = g_SavedBondState.field_88;

    // Height Tracking
    g_CurrentPlayer->field_6C = g_SavedBondState.field_6C;
    g_CurrentPlayer->field_70 = g_SavedBondState.field_70;
    g_CurrentPlayer->stanHeight = g_SavedBondState.stanHeight;

    // Smoothed / Render Coordinates
    g_CurrentPlayer->field_3B8 = g_SavedBondState.field_3B8;
    g_CurrentPlayer->field_3C4 = g_SavedBondState.field_3C4;
    g_CurrentPlayer->field_3C8 = g_SavedBondState.field_3C8;
    g_CurrentPlayer->field_3CC = g_SavedBondState.field_3CC;

    // Walk / Head Bob Animation State
    g_CurrentPlayer->resetheadpos = g_SavedBondState.resetheadpos;
    g_CurrentPlayer->resetheadrot = g_SavedBondState.resetheadrot;
    g_CurrentPlayer->resetheadtick = g_SavedBondState.resetheadtick;
    g_CurrentPlayer->headanim = g_SavedBondState.headanim;
    g_CurrentPlayer->headdamp = g_SavedBondState.headdamp;
    g_CurrentPlayer->headwalkingtime60 = g_SavedBondState.headwalkingtime60;
    g_CurrentPlayer->headamplitude = g_SavedBondState.headamplitude;
    g_CurrentPlayer->sideamplitude = g_SavedBondState.sideamplitude;
    g_CurrentPlayer->headpos = g_SavedBondState.headpos;
    g_CurrentPlayer->headlook = g_SavedBondState.headlook;
    g_CurrentPlayer->headup = g_SavedBondState.headup;
    g_CurrentPlayer->headpossum = g_SavedBondState.headpossum;
    g_CurrentPlayer->headlooksum = g_SavedBondState.headlooksum;
    g_CurrentPlayer->headupsum = g_SavedBondState.headupsum;
    g_CurrentPlayer->headbodyoffset = g_SavedBondState.headbodyoffset;
    g_CurrentPlayer->standheight = g_SavedBondState.standheight;
    g_CurrentPlayer->standbodyoffset = g_SavedBondState.standbodyoffset;
    g_CurrentPlayer->standfrac = g_SavedBondState.standfrac;
    g_CurrentPlayer->standlook[0] = g_SavedBondState.standlook[0];
    g_CurrentPlayer->standlook[1] = g_SavedBondState.standlook[1];
    g_CurrentPlayer->standup[0] = g_SavedBondState.standup[0];
    g_CurrentPlayer->standup[1] = g_SavedBondState.standup[1];
    g_CurrentPlayer->standcnt = g_SavedBondState.standcnt;

    // Health / Armor
    g_CurrentPlayer->bondhealth = g_SavedBondState.bondhealth;
    g_CurrentPlayer->bondarmour = g_SavedBondState.bondarmour;
    g_CurrentPlayer->oldhealth = g_SavedBondState.oldhealth;
    g_CurrentPlayer->oldarmour = g_SavedBondState.oldarmour;
    g_CurrentPlayer->apparenthealth = g_SavedBondState.apparenthealth;
    g_CurrentPlayer->apparentarmour = g_SavedBondState.apparentarmour;

    // Sync player's world PropRecord
    // If g_CurrentPlayer->prop is NULL, it means the player has no physical presence or representation in the 3D world (e.g. they are dead or the level has not fully loaded/spawned yet).
    // Checking for NULL and tracking via has_prop allows us to safely handle transitions between gameplay and intro/cutscenes.
    if (g_SavedBondState.has_prop) {
        if (g_CurrentPlayer->prop != NULL) {
            // Case A: Saved state has prop and current player has prop.
            // 1. Deregister from current rooms to avoid room/portal desync
            chrpropDeregisterRooms(g_CurrentPlayer->prop);

            // 2. Restore values
            g_CurrentPlayer->prop->pos = g_SavedBondState.prop_pos;
            g_CurrentPlayer->prop->stan = g_SavedBondState.prop_stan;
            g_CurrentPlayer->prop->rooms[0] = g_SavedBondState.prop_rooms[0];
            g_CurrentPlayer->prop->rooms[1] = g_SavedBondState.prop_rooms[1];
            g_CurrentPlayer->prop->rooms[2] = g_SavedBondState.prop_rooms[2];
            g_CurrentPlayer->prop->rooms[3] = g_SavedBondState.prop_rooms[3];

            // 3. Register to new rooms
            chrpropRegisterRooms(g_CurrentPlayer->prop);
        } else {
            // Case B: Saved state has prop but current player does NOT have prop (e.g. loading gameplay state while in intro/cutscene).
            // We must allocate a new player prop to represent the gameplay state.
            g_CurrentPlayer->prop = chrpropAllocate();
            if (g_CurrentPlayer->prop != NULL) {
                g_CurrentPlayer->prop->obj = NULL;
                g_CurrentPlayer->prop->type = PROP_TYPE_VIEWER;
                g_CurrentPlayer->prop->pos = g_SavedBondState.prop_pos;
                g_CurrentPlayer->prop->stan = g_SavedBondState.prop_stan;
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
            // Case C: Saved state has no prop, but current player has a prop (e.g. loading cutscene/death state during active gameplay).
            // We must deregister, delist, disable, and free the current player prop to match the saved state.
            chrpropDeregisterRooms(g_CurrentPlayer->prop);
            chrpropDelist(g_CurrentPlayer->prop);
            chrpropDisable(g_CurrentPlayer->prop);
            chrpropFree(g_CurrentPlayer->prop);
            g_CurrentPlayer->prop = NULL;
        }
        // Case D: Saved state has no prop and current player has no prop. Nothing to do.
    }

    // Sync player's 3D visual Model instance
    if (g_CurrentPlayer->ptr_char_objectinstance != NULL) {
        setsuboffset(g_CurrentPlayer->ptr_char_objectinstance, &g_CurrentPlayer->pos);
        setsubroty(g_CurrentPlayer->ptr_char_objectinstance, get_curplay_horizontal_rotation_in_degrees());
    }

    // Re-initialize player inventory, preserving text overrides
    {
        textoverride *saved_overrides = g_CurrentPlayer->textoverrides;
        bondinvReinitInv();
        g_CurrentPlayer->textoverrides = saved_overrides;
    }
    g_CurrentPlayer->equipallguns = g_SavedBondState.equipallguns;

    // Restore saved inventory items
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

    // Restore ammo counts
    {
        s32 i;
        for (i = 0; i < 30; i++) {
            g_CurrentPlayer->ammoheldarr[i] = g_SavedBondState.ammoheldarr[i];
        }
    }

    // Restore equipcuritem
    g_CurrentPlayer->equipcuritem = g_SavedBondState.equipcuritem;

    // Restore hand weapon logical state and trigger model loading directly
    {
        s32 hand;
        for (hand = 0; hand < 2; hand++) {
            ITEM_IDS saved_weapon = g_SavedBondState.hands_weaponnum[hand];

            // If the hand weapon model currently loaded is different from the saved one,
            // trigger loading the new model on the next frame.
            if (g_CurrentPlayer->hand_item[hand] != saved_weapon) {
                g_CurrentPlayer->hand_invisible[hand] = -3;
                g_CurrentPlayer->field_2A44[hand] = saved_weapon;
                g_CurrentPlayer->hand_item[hand] = ITEM_UNARMED;
                g_CurrentPlayer->hands[hand].weapon_current_animation = 0; // force idle/aiming
            } else {
                g_CurrentPlayer->hands[hand].weapon_current_animation = g_SavedBondState.hands_weapon_current_animation[hand];
            }

            g_CurrentPlayer->hands[hand].weaponnum = saved_weapon;
            g_CurrentPlayer->hands[hand].weaponnum_watchmenu = g_SavedBondState.hands_weaponnum_watchmenu[hand];
            g_CurrentPlayer->hands[hand].previous_weapon = g_SavedBondState.hands_previous_weapon[hand];
            g_CurrentPlayer->hands[hand].weapon_firing_status = g_SavedBondState.hands_weapon_firing_status[hand];
            g_CurrentPlayer->hands[hand].weapon_hold_time = g_SavedBondState.hands_weapon_hold_time[hand];
            g_CurrentPlayer->hands[hand].when_detonating_mines_is_0 = g_SavedBondState.hands_when_detonating_mines_is_0[hand];
            g_CurrentPlayer->hands[hand].weapon_ammo_in_magazine = g_SavedBondState.hands_weapon_ammo_in_magazine[hand];
            g_CurrentPlayer->hands[hand].weapon_next_weapon = g_SavedBondState.hands_weapon_next_weapon[hand];
            g_CurrentPlayer->hands[hand].weapon_animation_trigger = g_SavedBondState.hands_weapon_animation_trigger[hand];
            g_CurrentPlayer->hands[hand].field_8B8 = g_SavedBondState.hands_field_8B8[hand];
            g_CurrentPlayer->lock_hand_model[hand] = g_SavedBondState.lock_hand_model[hand];
        }
    }

    // Cheat status
    g_CurrentPlayer->bondinvincible = g_SavedBondState.bondinvincible;

    // HUD Display states
    g_CurrentPlayer->damageshowtime = g_SavedBondState.damageshowtime;
    g_CurrentPlayer->healthshowtime = g_SavedBondState.healthshowtime;
    g_CurrentPlayer->healthshowmode = g_SavedBondState.healthshowmode;

    // Fading & Breathing
    g_CurrentPlayer->bondfadetime60 = g_SavedBondState.bondfadetime60;
    g_CurrentPlayer->bondfadetimemax60 = g_SavedBondState.bondfadetimemax60;
    g_CurrentPlayer->bondfadefracold = g_SavedBondState.bondfadefracold;
    g_CurrentPlayer->bondfadefracnew = g_SavedBondState.bondfadefracnew;
    g_CurrentPlayer->bondbreathing = g_SavedBondState.bondbreathing;

    // Weapon Sway cycle
    g_CurrentPlayer->gunposamplitude = g_SavedBondState.gunposamplitude;
    g_CurrentPlayer->gunsync = g_SavedBondState.gunsync;
    g_CurrentPlayer->syncchange = g_SavedBondState.syncchange;
    g_CurrentPlayer->synccount = g_SavedBondState.synccount;
    g_CurrentPlayer->syncoffset = g_SavedBondState.syncoffset;
    g_CurrentPlayer->field_107C = g_SavedBondState.field_107C;
    g_CurrentPlayer->field_1080 = g_SavedBondState.field_1080;
}

void save_game_state(void) {
  if (g_CurrentPlayer == NULL) {
    return;
  }

  save_bond_state();

  g_HasSavedState = TRUE;
  sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State saved");
}

void load_game_state(void) {
  if (g_CurrentPlayer == NULL || !g_HasSavedState) {
    if (!g_HasSavedState) {
      practiceLogWarn("No saved state");
    }
    return;
  }

  load_bond_state();

  sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
  practiceLogInfo("State loaded");
}
