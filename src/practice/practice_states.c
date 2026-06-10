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
