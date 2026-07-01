#include "practice_states_globals.h"
#include "bondview.h"
#include "chr.h"
#include "chrobjhandler.h"
#include "lvl.h"
#include "objective_status.h"
#include "player.h"
#include "player_2.h"
#include "practice_states_utils.h"
#include "practice_ui.h"
#include <ultra64.h>

#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65

extern char stringbuffer_lowerleft[0x5][BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH];
extern s32 status_bar_text_buffer_index;
extern s32 display_statusbar;
extern s32 g_EnterTankAudioState;
extern s32 g_GlobalTimer;
extern s32 mission_timer;
extern u64 g_randomSeed;
extern u64 g_chrObjRandomSeed;
extern void sub_GAME_7F0BD8FC(s32 arg0);

static s32 saved_player_tank_prop_index;
static s32 saved_world_tank_prop_index;
static s32 saved_current_player_index;
static u64 saved_random_seed;
static u64 saved_chr_obj_random_seed;

static s32 count_room_objective_criteria(void) {
  struct criteria_roomentered *criteria;
  s32 count = 0;

  for (criteria = ptr_last_enter_room_subobject_entry_type20; criteria != NULL;
       criteria = criteria->next) {
    count++;
  }

  return count;
}

static s32 count_deposit_objective_criteria(void) {
  struct criteria_deposit *criteria;
  s32 count = 0;

  for (criteria = ptr_last_deposit_in_room_subobject_entry_type21;
       criteria != NULL; criteria = criteria->next) {
    count++;
  }

  return count;
}

static s32 count_picture_objective_criteria(void) {
  struct criteria_picture *criteria;
  s32 count = 0;

  for (criteria = ptr_last_photo_obj_in_room_subobject_entry_type1E;
       criteria != NULL; criteria = criteria->next) {
    count++;
  }

  return count;
}

static void save_objective_state(StateStream *stream) {
  struct criteria_roomentered *room_criteria;
  struct criteria_deposit *deposit_criteria;
  struct criteria_picture *picture_criteria;
  s32 i;

  for (i = 0; i < OBJECTIVES_MAX; i++) {
    write_u32(stream, objectiveStatuses[i]);
  }

  write_u32(stream, count_room_objective_criteria());
  for (room_criteria = ptr_last_enter_room_subobject_entry_type20;
       room_criteria != NULL; room_criteria = room_criteria->next) {
    write_u32(stream, room_criteria->status);
  }

  write_u32(stream, count_deposit_objective_criteria());
  for (deposit_criteria = ptr_last_deposit_in_room_subobject_entry_type21;
       deposit_criteria != NULL; deposit_criteria = deposit_criteria->next) {
    write_u32(stream, deposit_criteria->flag);
  }

  write_u32(stream, count_picture_objective_criteria());
  for (picture_criteria = ptr_last_photo_obj_in_room_subobject_entry_type1E;
       picture_criteria != NULL; picture_criteria = picture_criteria->next) {
    write_u32(stream, picture_criteria->flag);
  }

  for (i = 0; i < MAX_PLAYER_COUNT; i++) {
    write_u32(stream, g_playerPlayerData[i].killed_civilians);
  }
}

static void load_objective_state(StateStream *stream) {
  struct criteria_roomentered *room_criteria;
  struct criteria_deposit *deposit_criteria;
  struct criteria_picture *picture_criteria;
  s32 saved_count;
  s32 current_count;
  s32 i;

  for (i = 0; i < OBJECTIVES_MAX; i++) {
    objectiveStatuses[i] = read_u32(stream);
  }

  saved_count = read_u32(stream);
  current_count = count_room_objective_criteria();
  room_criteria = ptr_last_enter_room_subobject_entry_type20;
  for (i = 0; i < saved_count; i++) {
    u32 status = read_u32(stream);
    if (room_criteria != NULL) {
      room_criteria->status = status;
      room_criteria = room_criteria->next;
    }
  }
  while (room_criteria != NULL) {
    room_criteria->status = 0;
    room_criteria = room_criteria->next;
  }
  if (saved_count != current_count) {
    practiceLogWarn("Room objective criteria count changed (%d saved, %d live)",
                    saved_count, current_count);
  }

  saved_count = read_u32(stream);
  current_count = count_deposit_objective_criteria();
  deposit_criteria = ptr_last_deposit_in_room_subobject_entry_type21;
  for (i = 0; i < saved_count; i++) {
    u32 flag = read_u32(stream);
    if (deposit_criteria != NULL) {
      deposit_criteria->flag = flag;
      deposit_criteria = deposit_criteria->next;
    }
  }
  while (deposit_criteria != NULL) {
    deposit_criteria->flag = 0;
    deposit_criteria = deposit_criteria->next;
  }
  if (saved_count != current_count) {
    practiceLogWarn(
        "Deposit objective criteria count changed (%d saved, %d live)",
        saved_count, current_count);
  }

  saved_count = read_u32(stream);
  current_count = count_picture_objective_criteria();
  picture_criteria = ptr_last_photo_obj_in_room_subobject_entry_type1E;
  for (i = 0; i < saved_count; i++) {
    u32 flag = read_u32(stream);
    if (picture_criteria != NULL) {
      picture_criteria->flag = flag;
      picture_criteria = picture_criteria->next;
    }
  }
  while (picture_criteria != NULL) {
    picture_criteria->flag = 0;
    picture_criteria = picture_criteria->next;
  }
  if (saved_count != current_count) {
    practiceLogWarn(
        "Picture objective criteria count changed (%d saved, %d live)",
        saved_count, current_count);
  }

  for (i = 0; i < MAX_PLAYER_COUNT; i++) {
    g_playerPlayerData[i].killed_civilians = read_u32(stream);
  }
}

#if defined(VERSION_JP) || defined(VERSION_EU)
extern s32 dword_CODE_bss_jp80079CEC[0x05];
extern s32 dword_CODE_bss_jp80079Cd8[0x05];
#else
extern s32 copy_1stfonttable;
extern s32 copy_2ndfonttable;
#endif

void save_global_state(StateStream *stream) {
  // HUD
  write_u32(stream, status_bar_text_buffer_index);
  write_u32(stream, display_statusbar);
  write_bytes(stream, stringbuffer_lowerleft, sizeof(stringbuffer_lowerleft));

  // Font
#if defined(VERSION_JP) || defined(VERSION_EU)
  write_bytes(stream, dword_CODE_bss_jp80079CEC,
              sizeof(dword_CODE_bss_jp80079CEC));
  write_bytes(stream, dword_CODE_bss_jp80079Cd8,
              sizeof(dword_CODE_bss_jp80079Cd8));
#else
  write_u32(stream, copy_1stfonttable);
  write_u32(stream, copy_2ndfonttable);
#endif

  // Tank
  write_u32(stream, in_tank_flag);
  write_u32(stream, get_prop_index(g_PlayerTankProp));
  write_u32(stream, get_prop_index(g_WorldTankProp));
  write_f32(stream, g_PlayerTankYOffset);
  write_f32(stream, g_TankTurnSpeed);
  write_f32(stream, g_TankOrientationAngle);
  write_f32(stream, tank_turret_unused_angle);
  write_f32(stream, g_TankTurretVerticalAngle);
  write_f32(stream, g_TankTurretVerticalAngleRelated);
  write_f32(stream, g_TankTurretOrientationAngleRad);
  write_f32(stream, g_TankTurretOrientationAngleDeg);
  write_f32(stream, tank_turret_turn_speed);
  write_u32(stream, g_BondCanEnterTank);
  write_f32(stream, g_TankTurretAngle);
  write_f32(stream, g_TankTurretTurn);
  write_u32(stream, g_ExplodeTankOnDeathFlag);
  write_u32(stream, g_TankDamagePenaltyTicks);
  write_u32(stream, g_EnterTankAudioState);

  // Camera
  write_u32(stream, g_CameraMode);
  write_u32(stream, camera_mode);
  write_u32(stream, g_CameraAfterCinema);
  write_f32(stream, camera_transition_timer);
  write_u32(stream, camera_fade_active);
  write_u32(stream, intro_camera_index);
  write_u32(stream, is_timer_active);
  write_u32(stream, g_PlayerInvincible);

  // Level activity
  write_u32(stream, sub_GAME_7F0BD8F0());
  write_u32(stream, lvlGetControlsLockedFlag());

  // Values
  write_u32(stream, g_GlobalTimer);
  write_u32(stream, mission_timer);
#if defined(VERSION_JP) || defined(VERSION_EU)
  write_f32(stream, watch_time_0);
#else
  write_u32(stream, watch_time_0);
#endif
  write_bytes(stream, &g_randomSeed, sizeof(g_randomSeed));
  write_bytes(stream, &g_chrObjRandomSeed, sizeof(g_chrObjRandomSeed));
  write_u32(stream, get_cur_playernum());

  // Alarm
  write_u32(stream, alarm_timer);
  write_u32(stream, objectiveregisters1);

  // Objectives
  save_objective_state(stream);
}

void load_global_state_pre_props(StateStream *stream) {
  // HUD
  status_bar_text_buffer_index = read_u32(stream);
  display_statusbar = read_u32(stream);
  read_bytes(stream, stringbuffer_lowerleft, sizeof(stringbuffer_lowerleft));

  // Font
#if defined(VERSION_JP) || defined(VERSION_EU)
  read_bytes(stream, dword_CODE_bss_jp80079CEC,
             sizeof(dword_CODE_bss_jp80079CEC));
  read_bytes(stream, dword_CODE_bss_jp80079Cd8,
             sizeof(dword_CODE_bss_jp80079Cd8));
#else
  copy_1stfonttable = read_u32(stream);
  copy_2ndfonttable = read_u32(stream);
#endif

  // Tank
  in_tank_flag = read_u32(stream);
  saved_player_tank_prop_index = read_u32(stream);
  saved_world_tank_prop_index = read_u32(stream);
  g_PlayerTankYOffset = read_f32(stream);
  g_TankTurnSpeed = read_f32(stream);
  g_TankOrientationAngle = read_f32(stream);
  tank_turret_unused_angle = read_f32(stream);
  g_TankTurretVerticalAngle = read_f32(stream);
  g_TankTurretVerticalAngleRelated = read_f32(stream);
  g_TankTurretOrientationAngleRad = read_f32(stream);
  g_TankTurretOrientationAngleDeg = read_f32(stream);
  tank_turret_turn_speed = read_f32(stream);
  g_BondCanEnterTank = read_u32(stream);
  g_TankTurretAngle = read_f32(stream);
  g_TankTurretTurn = read_f32(stream);
  g_ExplodeTankOnDeathFlag = read_u32(stream);
  g_TankDamagePenaltyTicks = read_u32(stream);
  g_EnterTankAudioState = read_u32(stream);

  // Camera
  g_CameraMode = read_u32(stream);
  camera_mode = read_u32(stream);
  g_CameraAfterCinema = read_u32(stream);
  camera_transition_timer = read_f32(stream);
  camera_fade_active = read_u32(stream);
  intro_camera_index = read_u32(stream);
  is_timer_active = read_u32(stream);
  g_PlayerInvincible = read_u32(stream);

  // Level activity
  sub_GAME_7F0BD8FC(read_u32(stream));
  lvlSetControlsLockedFlag(read_u32(stream));

  // Values
  g_GlobalTimer = read_u32(stream);
  mission_timer = read_u32(stream);
#if defined(VERSION_JP) || defined(VERSION_EU)
  watch_time_0 = read_f32(stream);
#else
  watch_time_0 = read_u32(stream);
#endif
  read_bytes(stream, &saved_random_seed, sizeof(saved_random_seed));
  read_bytes(stream, &saved_chr_obj_random_seed,
             sizeof(saved_chr_obj_random_seed));
  saved_current_player_index = read_u32(stream);

  // Alarm
  alarm_timer = read_u32(stream);
  objectiveregisters1 = read_u32(stream);
  // Sound states are dynamically allocated and all SFX are stopped before
  // loading. Let the alarm update create a fresh handle when it next runs.
  ptr_alarm_sfx = NULL;

  // Objectives
  load_objective_state(stream);

  // TODO: We should save the RNG state needed for restoring each prop
  // individually but for now just use the final RNG state when restoring props
  g_randomSeed = saved_random_seed;
  g_chrObjRandomSeed = saved_chr_obj_random_seed;
}

bool load_global_state_post_props(void) {
  if (saved_current_player_index < 0 ||
      saved_current_player_index >= getPlayerCount() ||
      g_playerPointers[saved_current_player_index] == NULL) {
    practiceLogWarn("Invalid saved current player index %d",
                    saved_current_player_index);
    return FALSE;
  }

  g_PlayerTankProp = get_enabled_prop_by_index(saved_player_tank_prop_index);
  g_WorldTankProp = get_enabled_prop_by_index(saved_world_tank_prop_index);

  set_cur_player(saved_current_player_index);

  // Prop and player restoration can consume randomness. Apply the saved RNG
  // state last so the next gameplay tick resumes the saved sequence.
  g_randomSeed = saved_random_seed;
  g_chrObjRandomSeed = saved_chr_obj_random_seed;

  return TRUE;
}
