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

#define MAX_SAVED_GUARDS 64
#define MAX_SAVED_DOORS 64

typedef struct {
    s16 chrnum;
    coord3d pos;
    StandTile *stan;
    f32 damage;
    ACT_TYPE actiontype;
    CHRFLAG chrflags;
    bool active;
} SavedGuardState;

typedef struct {
    struct DoorRecord *door;
    f32 openPosition;
    f32 speed;
    s8 openstate;
    bool active;
} SavedDoorState;

static struct player g_SavedPlayerState;
static s32 g_SavedMissionTimer = 0;
static bool g_HasSavedState = FALSE;

static SavedGuardState g_SavedGuards[MAX_SAVED_GUARDS];
static s32 g_SavedGuardsCount = 0;

static SavedDoorState g_SavedDoors[MAX_SAVED_DOORS];
static s32 g_SavedDoorsCount = 0;

extern void *memcpy(void *dst, const void *src, size_t count);
extern s32 get_numguards(void);
extern PropRecord pos_data_entry[];

void save_player_state(void)
{
    s32 i;
    ChrRecord *guard;
    s32 numslots;

    if (g_CurrentPlayer == NULL) {
        return;
    }

    /* 1. Save Player State */
    memcpy(&g_SavedPlayerState, g_CurrentPlayer, sizeof(struct player));
    g_SavedMissionTimer = mission_timer;

    /* 2. Save Active Guards State */
    g_SavedGuardsCount = 0;
    numslots = get_numguards();
    if (numslots > MAX_SAVED_GUARDS) {
        numslots = MAX_SAVED_GUARDS;
    }

    for (i = 0; i < numslots; i++) {
        guard = &g_ChrSlots[i];
        if (guard->model != NULL && guard->prop != NULL) {
            g_SavedGuards[g_SavedGuardsCount].chrnum = guard->chrnum;
            g_SavedGuards[g_SavedGuardsCount].pos.x = guard->prop->pos.x;
            g_SavedGuards[g_SavedGuardsCount].pos.y = guard->prop->pos.y;
            g_SavedGuards[g_SavedGuardsCount].pos.z = guard->prop->pos.z;
            g_SavedGuards[g_SavedGuardsCount].stan = guard->prop->stan;
            g_SavedGuards[g_SavedGuardsCount].damage = guard->damage;
            g_SavedGuards[g_SavedGuardsCount].actiontype = guard->actiontype;
            g_SavedGuards[g_SavedGuardsCount].chrflags = guard->chrflags;
            g_SavedGuards[g_SavedGuardsCount].active = TRUE;
            g_SavedGuardsCount++;
        }
    }

    /* 3. Save Door States */
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

    g_HasSavedState = TRUE;
    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    HUDMESSAGEBOTTOM("STATE SAVED");
}

void load_player_state(void)
{
    s32 i, j;
    ChrRecord *guard;
    s32 numslots;

    if (g_CurrentPlayer == NULL || !g_HasSavedState) {
        if (!g_HasSavedState) {
            HUDMESSAGEBOTTOM("NO SAVED STATE");
        }
        return;
    }

    /* 1. Restore Player State */
    memcpy(g_CurrentPlayer, &g_SavedPlayerState, sizeof(struct player));
    mission_timer = g_SavedMissionTimer;

    /* 2. Restore Guards State */
    numslots = get_numguards();
    for (i = 0; i < numslots; i++) {
        guard = &g_ChrSlots[i];
        if (guard->model != NULL && guard->prop != NULL) {
            for (j = 0; j < g_SavedGuardsCount; j++) {
                if (g_SavedGuards[j].active && g_SavedGuards[j].chrnum == guard->chrnum) {
                    guard->prop->pos.x = g_SavedGuards[j].pos.x;
                    guard->prop->pos.y = g_SavedGuards[j].pos.y;
                    guard->prop->pos.z = g_SavedGuards[j].pos.z;
                    guard->prop->stan = g_SavedGuards[j].stan;
                    guard->damage = g_SavedGuards[j].damage;
                    guard->actiontype = g_SavedGuards[j].actiontype;
                    guard->chrflags = g_SavedGuards[j].chrflags;

                    // Snapping visual model of the guard
                    setsuboffset(guard->model, &g_SavedGuards[j].pos);
                    break;
                }
            }
        }
    }

    /* 3. Restore Door States */
    for (i = 0; i < g_SavedDoorsCount; i++) {
        if (g_SavedDoors[i].active && g_SavedDoors[i].door != NULL) {
            struct DoorRecord *door = g_SavedDoors[i].door;
            door->openPosition = g_SavedDoors[i].openPosition;
            door->speed = g_SavedDoors[i].speed;
            door->openstate = g_SavedDoors[i].openstate;
            
            // Instantly sync visual state and portals
            door7F052B00(door);
            doorActivatePortal(door);
            door7F053B10(door);
        }
    }

    sndPlaySfx(g_musicSfxBufferPtr, CAMERA_BEEP1_SFX, 0);
    HUDMESSAGEBOTTOM("STATE LOADED");
}
