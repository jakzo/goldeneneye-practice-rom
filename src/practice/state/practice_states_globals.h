#ifndef PRACTICE_STATES_GLOBALS_H
#define PRACTICE_STATES_GLOBALS_H

#include "practice_states_stream.h"
#include <ultra64.h>

#define BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH 0x65

typedef struct {
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
} SavedTankState;

typedef struct {
  s32 g_CameraMode;
  s32 camera_mode;
  s32 g_CameraAfterCinema;
  f32 camera_transition_timer;
  s32 camera_fade_active;
  s32 intro_camera_index;
  s32 is_timer_active;
  s32 g_PlayerInvincible;
} SavedCameraState;

typedef struct {
  s32 status_bar_text_buffer_index;
  s32 display_statusbar;
  char stringbuffer_lowerleft[5][BONDVIEW_HUD_MSG_BOTTOM_BUFFER_LENGTH];
} SavedHudState;

typedef struct {
#if defined(VERSION_JP) || defined(VERSION_EU)
  s32 dword_CODE_bss_jp80079CEC[5];
  s32 dword_CODE_bss_jp80079Cd8[5];
#else
  s32 copy_1stfonttable;
  s32 copy_2ndfonttable;
#endif
} SavedFontState;

typedef struct {
  s32 g_GlobalTimer;
  s32 mission_timer;
  u64 g_randomSeed;
  u64 g_chrObjRandomSeed;
} SavedValuesState;

typedef struct {
  SavedHudState hud;
  SavedFontState font;
  SavedTankState tank;
  SavedCameraState camera;
  SavedValuesState values;
} SavedGlobals;

void save_global_state(SramStream *stream);
void load_global_state(SramStream *stream);

#endif /* PRACTICE_STATES_GLOBALS_H */
