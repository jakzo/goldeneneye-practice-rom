#include <os_extension.h>
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <joy.h>
#include <music.h>
#include <snd.h>
#include "bondinv.h"
#include "bondview.h"
#include "chr.h"
#include "debugmenu_handler.h"
#include "file2.h"
#include "front.h"
#include "gun.h"
#include "lvl_text.h"
#include "objecthandler.h"
#include "player.h"
#include "player_2.h"
#include "chrobjhandler.h"
#include "assets/obseg/text/LmiscE.h"
#include "practice_states.h"

extern void *memcpy(void *dst, const void *src, size_t count);
extern s32 get_numguards(void);
extern PropRecord pos_data_entry[];
extern void place_item_in_hand_swap_and_make_visible(s32 hand, s32 item);
extern void doorDeactivatePortal(DoorRecord *door);

static bool g_HasSavedState = FALSE;

// ==========================================
// 1. BOND STATE SAVING AND LOADING
// ==========================================

static struct player g_SavedPlayerState;
static s32 g_SavedMissionTimer = 0;
static s32 g_SavedInTankFlag = 0;
static struct PropRecord *g_SavedPlayerTankProp = NULL;
static s32 g_SavedCameraMode = 0;

#define MAX_SAVED_INVENTORY_ITEMS 128
static InvItem g_SavedInventoryItems[MAX_SAVED_INVENTORY_ITEMS];
static s32 g_SavedInventoryItemsCount = 0;
static InvItem *g_SavedPtrInventoryFirstInCycle = NULL;
static InvItem *g_SavedPItemCur = NULL;

static InvItem* relocate_inv_pointer(InvItem *p, InvItem *saved_base, InvItem *active_base, s32 max_count) {
    s32 index;
    if (p == NULL || saved_base == NULL || active_base == NULL) return NULL;
    index = p - saved_base;
    if (index >= 0 && index < max_count) {
        return active_base + index;
    }
    return NULL;
}

static void save_bond_state(void)
{
    /* Save Player Structure */
    memcpy(&g_SavedPlayerState, g_CurrentPlayer, sizeof(struct player));
    g_SavedMissionTimer = mission_timer;
    g_SavedInTankFlag = in_tank_flag;
    g_SavedPlayerTankProp = g_PlayerTankProp;
    g_SavedCameraMode = g_CameraMode;

    /* Save Inventory items */
    g_SavedInventoryItemsCount = g_CurrentPlayer->equipmaxitems;
    if (g_SavedInventoryItemsCount > MAX_SAVED_INVENTORY_ITEMS) {
        g_SavedInventoryItemsCount = MAX_SAVED_INVENTORY_ITEMS;
    }
    if (g_CurrentPlayer->p_itemcur != NULL) {
        memcpy(g_SavedInventoryItems, g_CurrentPlayer->p_itemcur, g_SavedInventoryItemsCount * sizeof(InvItem));
    }
    g_SavedPtrInventoryFirstInCycle = g_CurrentPlayer->ptr_inventory_first_in_cycle;
    g_SavedPItemCur = g_CurrentPlayer->p_itemcur;
}

static void load_bond_state(void)
{
    s32 i;

    // Restore health & armor
    g_CurrentPlayer->bondhealth = g_SavedPlayerState.bondhealth;
    g_CurrentPlayer->bondarmour = g_SavedPlayerState.bondarmour;
    g_CurrentPlayer->oldhealth = g_SavedPlayerState.oldhealth;
    g_CurrentPlayer->oldarmour = g_SavedPlayerState.oldarmour;
    g_CurrentPlayer->apparenthealth = g_SavedPlayerState.apparenthealth;
    g_CurrentPlayer->apparentarmour = g_SavedPlayerState.apparentarmour;

    // Restore health/armor show timers
    g_CurrentPlayer->damageshowtime = g_SavedPlayerState.damageshowtime;
    g_CurrentPlayer->healthshowtime = g_SavedPlayerState.healthshowtime;
    g_CurrentPlayer->healthshowmode = g_SavedPlayerState.healthshowmode;

    if (g_SavedPlayerState.damageshowtime < 0) {
        gunSetGunAmmoVisible(GUNAMMOREASON_DAMAGE, TRUE);
        gunSetSightVisible(GUNSIGHTREASON_DAMAGE, TRUE);
        hudmsgsSetOn(4);
        bondviewClearUpperTextDisplayFlag(PLAYERFLAG_NOTIMER);
        countdownTimerSetVisible(8, 1);
    }

    // Restore dead state and death animation markers (important for reviving)
    g_CurrentPlayer->bonddead = g_SavedPlayerState.bonddead;
    g_CurrentPlayer->redbloodfinished = g_SavedPlayerState.redbloodfinished;
    g_CurrentPlayer->deathanimfinished = g_SavedPlayerState.deathanimfinished;

    // Restore cheat invincibility status
    g_CurrentPlayer->bondinvincible = g_SavedPlayerState.bondinvincible;

    // Restore ammo counts
    memcpy(g_CurrentPlayer->ammoheldarr, g_SavedPlayerState.ammoheldarr, sizeof(g_CurrentPlayer->ammoheldarr));

    /* Restore Inventory List */
    if (g_CurrentPlayer->p_itemcur != NULL && g_SavedPItemCur != NULL) {
        s32 count = g_CurrentPlayer->equipmaxitems;
        if (count > g_SavedInventoryItemsCount) {
            count = g_SavedInventoryItemsCount;
        }
        memcpy(g_CurrentPlayer->p_itemcur, g_SavedInventoryItems, count * sizeof(InvItem));

        // Relocate next and prev pointers inside the new p_itemcur bank
        for (i = 0; i < count; i++) {
            g_CurrentPlayer->p_itemcur[i].next = relocate_inv_pointer(g_CurrentPlayer->p_itemcur[i].next, g_SavedPItemCur, g_CurrentPlayer->p_itemcur, count);
            g_CurrentPlayer->p_itemcur[i].prev = relocate_inv_pointer(g_CurrentPlayer->p_itemcur[i].prev, g_SavedPItemCur, g_CurrentPlayer->p_itemcur, count);
        }

        g_CurrentPlayer->ptr_inventory_first_in_cycle = relocate_inv_pointer(g_SavedPtrInventoryFirstInCycle, g_SavedPItemCur, g_CurrentPlayer->p_itemcur, count);
    }
    g_CurrentPlayer->equipallguns = g_SavedPlayerState.equipallguns;
    g_CurrentPlayer->equipcuritem = g_SavedPlayerState.equipcuritem;

    // Restore weapon selection and magazine ammo counts (keeping pointers safe)
    memcpy(&g_CurrentPlayer->hands[0], &g_SavedPlayerState.hands[0], sizeof(struct hand));
    memcpy(&g_CurrentPlayer->hands[1], &g_SavedPlayerState.hands[1], sizeof(struct hand));
    g_CurrentPlayer->hands[0].audioHandle = NULL;
    g_CurrentPlayer->hands[0].rocket = NULL;
    g_CurrentPlayer->hands[1].audioHandle = NULL;
    g_CurrentPlayer->hands[1].rocket = NULL;

    // Request the engine to load the 1st person weapon models on demand
    place_item_in_hand_swap_and_make_visible(GUNRIGHT, g_SavedPlayerState.hand_item[GUNRIGHT]);
    place_item_in_hand_swap_and_make_visible(GUNLEFT, g_SavedPlayerState.hand_item[GUNLEFT]);
    g_CurrentPlayer->cur_item_weapon_getname = g_SavedPlayerState.cur_item_weapon_getname;

    // Restore look angles and trigonometric caches
    g_CurrentPlayer->vv_theta = g_SavedPlayerState.vv_theta;
    g_CurrentPlayer->vv_costheta = g_SavedPlayerState.vv_costheta;
    g_CurrentPlayer->vv_sintheta = g_SavedPlayerState.vv_sintheta;
    g_CurrentPlayer->vv_verta = g_SavedPlayerState.vv_verta;
    g_CurrentPlayer->vv_verta360 = g_SavedPlayerState.vv_verta360;

    // Safely sync top-level coordinates
    g_CurrentPlayer->pos = g_SavedPlayerState.pos;
    g_CurrentPlayer->pos2 = g_SavedPlayerState.pos2;
    g_CurrentPlayer->offset = g_SavedPlayerState.offset;
    g_CurrentPlayer->pos3 = g_SavedPlayerState.pos3;
    g_CurrentPlayer->current_model_pos = g_SavedPlayerState.current_model_pos;
    g_CurrentPlayer->previous_model_pos = g_SavedPlayerState.previous_model_pos;
    g_CurrentPlayer->current_room_pos = g_SavedPlayerState.current_room_pos;
    g_CurrentPlayer->bondprevpos = g_SavedPlayerState.bondprevpos;
    if (g_CurrentPlayer->prop != NULL) {
        g_CurrentPlayer->prop->pos = g_SavedPlayerState.field_488.collision_position;
        g_CurrentPlayer->prop->stan = g_SavedPlayerState.field_488.current_tile_ptr;
    }
    g_CurrentPlayer->stanHeight = g_SavedPlayerState.stanHeight;
    g_CurrentPlayer->vertical_bounce_adjust = g_SavedPlayerState.vertical_bounce_adjust;
    g_CurrentPlayer->crouchpos = g_SavedPlayerState.crouchpos;
    g_CurrentPlayer->ducking_height_offset = g_SavedPlayerState.ducking_height_offset;

    // Direct restoration of full collision, room/portal pointers, and camera smoothing
    g_CurrentPlayer->field_488 = g_SavedPlayerState.field_488;
    g_CurrentPlayer->previous_collision_info = g_SavedPlayerState.previous_collision_info;
    g_CurrentPlayer->room_pointer = g_SavedPlayerState.room_pointer;

    g_CurrentPlayer->field_3B8 = g_SavedPlayerState.field_3B8;
    g_CurrentPlayer->field_3C4 = g_SavedPlayerState.field_3C4;
    g_CurrentPlayer->field_3C8 = g_SavedPlayerState.field_3C8;
    g_CurrentPlayer->field_3CC = g_SavedPlayerState.field_3CC;

    in_tank_flag = g_SavedInTankFlag;
    g_PlayerTankProp = g_SavedPlayerTankProp;
    g_CameraMode = g_SavedCameraMode;

    mission_timer = g_SavedMissionTimer;
}

// ==========================================
// 2. GUARDS STATE SAVING AND LOADING
// ==========================================

#define MAX_SAVED_GUARDS 64

typedef struct {
    s16 chrnum;
    coord3d pos;
    StandTile *stan;
    f32 damage;
    ACT_TYPE actiontype;
    CHRFLAG chrflags;
    f32 theta;

    // Physics / movement
    coord3d prevpos;
    coord3d fallspeed;
    f32 ground;
    f32 sumground;
    f32 manground;

    // AI
    u8 alertness;
    u8 morale;
    s8 sleep;
    s32 timer60;
    u16 aioffset;
    s16 aireturnlist;
    AIRecord *ailist;
    u8 action_union[120];

    bool active;
} SavedGuardState;

static SavedGuardState g_SavedGuards[MAX_SAVED_GUARDS];
static s32 g_SavedGuardsCount = 0;

static void save_guards_state(void)
{
    s32 i;
    ChrRecord *guard;
    s32 numslots;

    g_SavedGuardsCount = 0;
    numslots = get_numguards();
    if (numslots > MAX_SAVED_GUARDS) {
        numslots = MAX_SAVED_GUARDS;
    }

    for (i = 0; i < numslots; i++) {
        guard = &g_ChrSlots[i];
        if (guard->model != NULL && guard->prop != NULL) {
            g_SavedGuards[g_SavedGuardsCount].chrnum = guard->chrnum;
            g_SavedGuards[g_SavedGuardsCount].pos = guard->prop->pos;
            g_SavedGuards[g_SavedGuardsCount].stan = guard->prop->stan;
            g_SavedGuards[g_SavedGuardsCount].damage = guard->damage;
            g_SavedGuards[g_SavedGuardsCount].actiontype = guard->actiontype;
            g_SavedGuards[g_SavedGuardsCount].chrflags = guard->chrflags;
            g_SavedGuards[g_SavedGuardsCount].theta = getsubroty(guard->model);

            g_SavedGuards[g_SavedGuardsCount].prevpos = guard->prevpos;
            g_SavedGuards[g_SavedGuardsCount].fallspeed = guard->fallspeed;
            g_SavedGuards[g_SavedGuardsCount].ground = guard->ground;
            g_SavedGuards[g_SavedGuardsCount].sumground = guard->sumground;
            g_SavedGuards[g_SavedGuardsCount].manground = guard->manground;

            g_SavedGuards[g_SavedGuardsCount].alertness = guard->alertness;
            g_SavedGuards[g_SavedGuardsCount].morale = guard->morale;
            g_SavedGuards[g_SavedGuardsCount].sleep = guard->sleep;
            g_SavedGuards[g_SavedGuardsCount].timer60 = guard->timer60;
            g_SavedGuards[g_SavedGuardsCount].aioffset = guard->aioffset;
            g_SavedGuards[g_SavedGuardsCount].aireturnlist = guard->aireturnlist;
            g_SavedGuards[g_SavedGuardsCount].ailist = guard->ailist;
            memcpy(g_SavedGuards[g_SavedGuardsCount].action_union, &guard->act_init, 120);

            g_SavedGuards[g_SavedGuardsCount].active = TRUE;
            g_SavedGuardsCount++;
        }
    }
}

static void load_guards_state(void)
{
    s32 i, j;
    ChrRecord *guard;
    s32 numslots;

    numslots = get_numguards();
    for (i = 0; i < numslots; i++) {
        guard = &g_ChrSlots[i];
        if (guard->model != NULL && guard->prop != NULL) {
            for (j = 0; j < g_SavedGuardsCount; j++) {
                if (g_SavedGuards[j].active && g_SavedGuards[j].chrnum == guard->chrnum) {
                    guard->prop->pos = g_SavedGuards[j].pos;
                    guard->prop->stan = g_SavedGuards[j].stan;
                    guard->damage = g_SavedGuards[j].damage;
                    guard->actiontype = g_SavedGuards[j].actiontype;
                    guard->chrflags = g_SavedGuards[j].chrflags;

                    guard->prevpos = g_SavedGuards[j].prevpos;
                    guard->fallspeed = g_SavedGuards[j].fallspeed;
                    guard->ground = g_SavedGuards[j].ground;
                    guard->sumground = g_SavedGuards[j].sumground;
                    guard->manground = g_SavedGuards[j].manground;

                    guard->alertness = g_SavedGuards[j].alertness;
                    guard->morale = g_SavedGuards[j].morale;
                    guard->sleep = g_SavedGuards[j].sleep;
                    guard->timer60 = g_SavedGuards[j].timer60;
                    guard->aioffset = g_SavedGuards[j].aioffset;
                    guard->aireturnlist = g_SavedGuards[j].aireturnlist;
                    guard->ailist = g_SavedGuards[j].ailist;
                    memcpy(&guard->act_init, g_SavedGuards[j].action_union, 120);

                    // Sync visual model position and Y-rotation
                    setsuboffset(guard->model, &g_SavedGuards[j].pos);
                    setsubroty(guard->model, g_SavedGuards[j].theta);
                    break;
                }
            }
        }
    }
}

// ==========================================
// 3. DOORS STATE SAVING AND LOADING
// ==========================================

#define MAX_SAVED_DOORS 64

typedef struct {
    struct DoorRecord *door;
    f32 openPosition;
    f32 speed;
    s8 openstate;
    bool active;
} SavedDoorState;

static SavedDoorState g_SavedDoors[MAX_SAVED_DOORS];
static s32 g_SavedDoorsCount = 0;

static void save_doors_state(void)
{
    s32 i;

    g_SavedDoorsCount = 0;
    for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
        PropRecord *prop = &pos_data_entry[i];
        if (prop->type == PROP_TYPE_DOOR && prop->door != NULL) {
            g_SavedDoors[g_SavedDoorsCount].door = prop->door;
            g_SavedDoors[g_SavedDoorsCount].openPosition = prop->door->openPosition;
            g_SavedDoors[g_SavedDoorsCount].speed = prop->door->speed;
            g_SavedDoors[g_SavedDoorsCount].openstate = prop->door->openstate;
            g_SavedDoors[g_SavedDoorsCount].active = TRUE;
            g_SavedDoorsCount++;
            if (g_SavedDoorsCount >= MAX_SAVED_DOORS) {
                break;
            }
        }
    }
}

static void load_doors_state(void)
{
    s32 i;
    
    for (i = 0; i < g_SavedDoorsCount; i++) {
        if (g_SavedDoors[i].active && g_SavedDoors[i].door != NULL) {
            struct DoorRecord *door = g_SavedDoors[i].door;
            door->openPosition = g_SavedDoors[i].openPosition;
            door->speed = g_SavedDoors[i].speed;
            door->openstate = g_SavedDoors[i].openstate;
            
            // Instantly sync visual state and portals
            door7F052B00(door);
            if (door->openPosition > 0.0f) {
                doorActivatePortal(door);
            } else {
                doorDeactivatePortal(door);
            }
            door7F053B10(door);
        }
    }
}

// ==========================================
// 4. MAIN PUBLIC ENTRY POINTS
// ==========================================

void save_game_state(void)
{
    if (g_CurrentPlayer == NULL) {
        return;
    }

    save_bond_state();
    save_guards_state();
    save_doors_state();

    g_HasSavedState = TRUE;
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    HUDMESSAGEBOTTOM("STATE SAVED");
}

void load_game_state(void)
{
    if (g_CurrentPlayer == NULL || !g_HasSavedState) {
        if (!g_HasSavedState) {
            HUDMESSAGEBOTTOM("NO SAVED STATE");
        }
        return;
    }

    load_bond_state();
    load_guards_state();
    load_doors_state();

    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    HUDMESSAGEBOTTOM("STATE LOADED");
}
