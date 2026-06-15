#include "practice_grenade_cam.h"
#include "bg.h"
#include "bondconstants.h"
#include "bondgame.h"
#include "bondtypes.h"
#include "bondview.h"
#include "chrai.h"
#include "chrlv.h"
#include "chrobjhandler.h"
#include "explosions.h"
#include "lvl.h"
#include "player.h"
#include "player_2.h"
#include "practice_config.h"
#include "stan.h"
#include <fr.h>
#include <math.h>
#include <os_extension.h>
#include <ultra64.h>

// TODO: Cannot render more than 2 without game crashing
#define MAX_PIP_SLOTS 2
#define PIP_SIZE 75
#define PIP_SPACING 4
#define FREEZE_DURATION_SEC 5.0f
#define MAX_CAMERA_DIST 80.0f
#define MAX_CAMERA_DIST_SQR (MAX_CAMERA_DIST * MAX_CAMERA_DIST)

struct pip_slot {
  struct Projectile *grenade;
  struct ObjectRecord *grenade_obj;
  coord3d grenade_last_pos;
  struct StandTile *grenade_last_stan;
  coord3d camera_pos;
  s32 freeze_ticks;
  bool active;
};

static struct pip_slot g_PipSlots[MAX_PIP_SLOTS];

extern s32 z_buffer;
extern s32 z_buffer_width;
extern s32 g_ClockTimer;
extern u8 g_ViBackIndex;
extern VideoSettings *g_ViBackData;
extern s32 g_viColorOutputMode;

#if defined(VERSION_EU)
extern void sub_GAME_7F0A46A0(Gfx **gdl, s32 arg1);
#else
extern void sub_GAME_7F0A4824(Gfx **gdl, s32 arg1);
#endif
extern Gfx *sub_GAME_7F0A2C44(Gfx *gdl);
extern void sub_GAME_7F0876C4(coord3d *cam_pos, coord3d *cam_look,
                              coord3d *cam_up);

extern coord3d flt_CODE_bss_80079940;
extern f32 flt_CODE_bss_8007994C;
extern coord3d flt_CODE_bss_80079950;
extern f32 flt_CODE_bss_8007995C;
extern coord3d flt_CODE_bss_80079960;
extern f32 flt_CODE_bss_8007996C;
extern coord3d flt_CODE_bss_80079970;
extern f32 flt_CODE_bss_8007997C;
extern f32 flt_CODE_bss_80079980;

void practice_grenade_cam_tick(void) {
  s32 i, s;
  struct PropRecord *player_prop = get_curplayer_positiondata();
  struct Projectile *p;
  struct pip_slot *slot;
  bool still_active, is_player_grenade;
  coord3d g_pos;
  f32 dx, dz, dist_sqr, dist, min_y, max_y;

  if (!practice.grenade_cam || g_CurrentPlayer == NULL || player_prop == NULL)
    return;

  // 1. Tick existing slots
  for (s = 0; s < MAX_PIP_SLOTS; s++) {
    slot = &g_PipSlots[s];
    if (!slot->active)
      continue;

    if (slot->freeze_ticks > 0) {
      // Freeze phase
      slot->freeze_ticks -= g_ClockTimer;
      if (slot->freeze_ticks <= 0) {
        slot->active = FALSE;
        slot->grenade = NULL;
        slot->grenade_obj = NULL;
      }
    } else {
      // Tracking phase
      p = slot->grenade;
      still_active = p != NULL && !(p->flags & PROJECTILEFLAG_FREE) &&
                     p->obj == slot->grenade_obj && p->obj->prop != NULL;

      if (still_active) {
        g_pos = p->obj->prop->pos;
        slot->grenade_last_pos = g_pos;
        if (p->obj->prop->stan != NULL) {
          slot->grenade_last_stan = p->obj->prop->stan;
        }

        // Camera follow logic
        dx = slot->camera_pos.x - g_pos.x;
        dz = slot->camera_pos.z - g_pos.z;
        dist_sqr = dx * dx + dz * dz;
        if (dist_sqr > MAX_CAMERA_DIST_SQR) {
          dist = sqrtf(dist_sqr);
          slot->camera_pos.x = g_pos.x + (dx / dist) * MAX_CAMERA_DIST;
          slot->camera_pos.z = g_pos.z + (dz / dist) * MAX_CAMERA_DIST;
        }

        max_y = g_pos.y + 50.0f;
        min_y = g_pos.y + 30.0f;
        if (slot->camera_pos.y < min_y) {
          slot->camera_pos.y = min_y;
        } else if (slot->camera_pos.y > max_y) {
          slot->camera_pos.y = max_y;
        }
      } else {
        // Start freeze phase
        slot->freeze_ticks = (s32)(FREEZE_DURATION_SEC * CHRLV_FRAMERATE_F);
      }
    }
  }

  // 2. Scan for new player-thrown grenades
  for (i = 0; i < PROJECTILES_ARR_MAX; i++) {
    p = &g_Projectiles[i];
    is_player_grenade = !(p->flags & PROJECTILEFLAG_FREE) && p->obj != NULL &&
                        p->obj->prop != NULL &&
                        p->obj->obj == PROP_CHRGRENADE &&
                        p->ownerprop == player_prop;
    if (!is_player_grenade)
      continue;

    // Check if already tracked
    for (s = 0; s < MAX_PIP_SLOTS; s++) {
      if (g_PipSlots[s].active && g_PipSlots[s].grenade_obj == p->obj)
        goto next_grenade;
    }

    // Find first available slot
    for (s = 0; s < MAX_PIP_SLOTS; s++) {
      slot = &g_PipSlots[s];
      if (slot->active)
        continue;

      slot->active = TRUE;
      slot->grenade = p;
      slot->grenade_obj = p->obj;
      slot->grenade_last_pos = p->obj->prop->pos;
      slot->grenade_last_stan = p->obj->prop->stan;
      slot->freeze_ticks = 0;

      slot->camera_pos.x = p->obj->prop->pos.x;
      slot->camera_pos.z = p->obj->prop->pos.z;
      slot->camera_pos.y = p->obj->prop->pos.y + 30.0f;
      break;
    }
  next_grenade:;
  }
}

Gfx *practice_grenade_cam_render(Gfx *gdl) {
  s32 s;
  s32 active_count = 0;
  struct pip_slot *slot;

  // Viewport layout variables
  s16 screen_w;
  s16 spacing;
  s16 top;
  s16 left;

  // Saved player view/matrix state
  s16 saved_viewx;
  s16 saved_viewy;
  s16 saved_viewleft;
  s16 saved_viewtop;

  f32 saved_screenwidth;
  f32 saved_screenheight;
  f32 saved_halfwidth;
  f32 saved_halfheight;
  f32 saved_screenleft;
  f32 saved_screentop;
  f32 saved_scalex;
  f32 saved_scaley;

  f32 saved_fovy;
  f32 saved_aspect;
  f32 saved_perspnear;
  f32 saved_perspfovy;
  f32 saved_perspaspect;

  s32 saved_field_5C;
  s32 saved_field_60;
  s32 saved_field_64;
  s32 saved_field_68;

  coord3d saved_pos;
  coord3d saved_pos3;
  struct collision434 saved_field_488;
  StandTile *saved_room_pointer;

  coord3d saved_current_model_pos;
  coord3d saved_previous_model_pos;
  coord3d saved_current_room_pos;
  s32 saved_curRoomIndex;

  Mtx *saved_field_10C4;
  Mtx *saved_field_10C8;
  Mtxf *saved_field_10CC;
  Mtxf *saved_field_10D4;
  s32 saved_field_10E0;
  s32 saved_field_10E4;
  Mtxf *saved_field_10E8;
  Mtxf *saved_field_10EC;

  coord3d saved_frustum_940;
  f32 saved_frustum_94C;
  coord3d saved_frustum_950;
  f32 saved_frustum_95C;
  coord3d saved_frustum_960;
  f32 saved_frustum_96C;
  coord3d saved_frustum_970;
  f32 saved_frustum_97C;
  f32 saved_frustum_980;

  Vp saved_vp;
  VideoSettings saved_vi;

  // Camera parameters
  coord3d cam_pos;
  coord3d target;
  coord3d cam_look;
  coord3d cam_up;
  StandTile *stan;

  for (s = 0; s < MAX_PIP_SLOTS; s++) {
    if (g_PipSlots[s].active) {
      active_count++;
    }
  }

  if (active_count == 0) {
    return gdl;
  }

  // Save states
  saved_viewx = g_CurrentPlayer->viewx;
  saved_viewy = g_CurrentPlayer->viewy;
  saved_viewleft = g_CurrentPlayer->viewleft;
  saved_viewtop = g_CurrentPlayer->viewtop;

  saved_screenwidth = g_CurrentPlayer->c_screenwidth;
  saved_screenheight = g_CurrentPlayer->c_screenheight;
  saved_halfwidth = g_CurrentPlayer->c_halfwidth;
  saved_halfheight = g_CurrentPlayer->c_halfheight;
  saved_screenleft = g_CurrentPlayer->c_screenleft;
  saved_screentop = g_CurrentPlayer->c_screentop;
  saved_scalex = g_CurrentPlayer->c_scalex;
  saved_scaley = g_CurrentPlayer->c_scaley;

  saved_fovy = g_CurrentPlayer->fovy;
  saved_aspect = g_CurrentPlayer->aspect;
  saved_perspnear = g_CurrentPlayer->c_perspnear;
  saved_perspfovy = g_CurrentPlayer->c_perspfovy;
  saved_perspaspect = g_CurrentPlayer->c_perspaspect;

  saved_field_5C = g_CurrentPlayer->field_5C;
  saved_field_60 = g_CurrentPlayer->field_60;
  saved_field_64 = g_CurrentPlayer->field_64;
  saved_field_68 = g_CurrentPlayer->field_68;

  saved_pos = g_CurrentPlayer->pos;
  saved_pos3 = g_CurrentPlayer->pos3;
  saved_field_488 = g_CurrentPlayer->field_488;
  saved_room_pointer = g_CurrentPlayer->room_pointer;

  saved_current_model_pos = g_CurrentPlayer->current_model_pos;
  saved_previous_model_pos = g_CurrentPlayer->previous_model_pos;
  saved_current_room_pos = g_CurrentPlayer->current_room_pos;
  saved_curRoomIndex = g_CurrentPlayer->curRoomIndex;

  saved_field_10C4 = g_CurrentPlayer->field_10C4;
  saved_field_10C8 = g_CurrentPlayer->field_10C8;
  saved_field_10CC = g_CurrentPlayer->field_10CC;
  saved_field_10D4 = g_CurrentPlayer->field_10D4;
  saved_field_10E0 = g_CurrentPlayer->field_10E0;
  saved_field_10E4 = g_CurrentPlayer->field_10E4;
  saved_field_10E8 = g_CurrentPlayer->field_10E8;
  saved_field_10EC = g_CurrentPlayer->field_10EC;

  saved_frustum_940 = flt_CODE_bss_80079940;
  saved_frustum_94C = flt_CODE_bss_8007994C;
  saved_frustum_950 = flt_CODE_bss_80079950;
  saved_frustum_95C = flt_CODE_bss_8007995C;
  saved_frustum_960 = flt_CODE_bss_80079960;
  saved_frustum_96C = flt_CODE_bss_8007996C;
  saved_frustum_970 = flt_CODE_bss_80079970;
  saved_frustum_97C = flt_CODE_bss_8007997C;
  saved_frustum_980 = flt_CODE_bss_80079980;

  saved_vp = g_CurrentPlayer->viewports[g_ViBackIndex];
  saved_vi = *g_ViBackData;

  screen_w = viGetX();
  spacing = (screen_w - PIP_SIZE * MAX_PIP_SLOTS) / (MAX_PIP_SLOTS + 1);
  top = PIP_SPACING;

  for (s = 0; s < MAX_PIP_SLOTS; s++) {
    slot = &g_PipSlots[s];
    if (!slot->active)
      continue;

    left = spacing + s * (PIP_SIZE + spacing);

    // 1. Clear Z-buffer inside scissor bounds
    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, z_buffer_width,
                     OS_K0_TO_PHYSICAL(z_buffer));
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetFillColor(gdl++,
                    (GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)));
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, left, top, left + PIP_SIZE,
                  top + PIP_SIZE);
    gDPFillRectangle(gdl++, left, top, left + PIP_SIZE - 1, top + PIP_SIZE - 1);
    gDPPipeSync(gdl++);

    // Restore color image
    if (g_viColorOutputMode != COLORMODE_32BIT) {
      gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_16b, g_ViBackData->bufx,
                       OS_K0_TO_PHYSICAL(g_ViBackData->framebuf));
    } else {
      gDPSetColorImage(gdl++, G_IM_FMT_RGBA, G_IM_SIZ_32b, g_ViBackData->bufx,
                       OS_K0_TO_PHYSICAL(cfb_16[0]));
    }

    // 2. Set viewport & projection
    g_CurrentPlayer->viewx = PIP_SIZE;
    g_CurrentPlayer->viewy = PIP_SIZE;
    g_CurrentPlayer->viewleft = left;
    g_CurrentPlayer->viewtop = top;
    g_CurrentPlayer->fovy = 60.0f;
    g_CurrentPlayer->aspect = 1.0f;

    currentPlayerSetScreenSize(PIP_SIZE, PIP_SIZE);
    currentPlayerSetScreenPosition(left, top);
    currentPlayerSetPerspective(g_ViBackData->znear, 60.0f, 1.0f);
    currentPlayerSetCameraScale();

    viSetViewSize(PIP_SIZE, PIP_SIZE);
    viSetViewPosition(left, top);
    viSetFovY(60.0f);
    viSetAspect(1.0f);

    gdl = video_related_F(gdl);

    // Load a dynamically allocated viewport in g_VtxBuffers (flushed to
    // physical RAM) because the player struct is in stage BSS/heap and CPU
    // modifications to it are not coherent with the RSP's physical memory
    // reads.
    {
      Vp *vp = (Vp *)dynAllocate(sizeof(Vp));
      vp->vp.vscale[0] = (g_ViBackData->viewx * 2);
      vp->vp.vtrans[0] =
          (g_ViBackData->viewx * 2) + (g_ViBackData->viewleft * 4);
      vp->vp.vscale[1] = (g_ViBackData->viewy * 2);
      vp->vp.vtrans[1] =
          (g_ViBackData->viewy * 2) + (g_ViBackData->viewtop * 4);
      vp->vp.vscale[2] = 0x1FF;
      vp->vp.vtrans[2] = 0x1FF;
      vp->vp.vscale[3] = 0;
      vp->vp.vtrans[3] = 0;
      osWritebackDCache(vp, sizeof(Vp));
      gSPViewport(gdl++, OS_K0_TO_PHYSICAL(vp));
    }

    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, left, top, left + PIP_SIZE,
                  top + PIP_SIZE);

    // 3. Set camera transformation
    cam_pos = slot->camera_pos;
    target = slot->grenade_last_pos;
    cam_look.x = target.x - cam_pos.x;
    cam_look.y = target.y - cam_pos.y;
    cam_look.z = target.z - cam_pos.z;

    if (cam_look.x == 0.0f && cam_look.z == 0.0f) {
      // Crashes with divide by 0 due to gimbal lock if we don't do this
      cam_look.x = 0.001f;
    }

    cam_up.x = 0.0f;
    cam_up.y = 1.0f;
    cam_up.z = 0.0f;

    stan = slot->grenade_last_stan;
    if (stan == NULL)
      stan = saved_room_pointer;
    g_CurrentPlayer->room_pointer = stan;
    g_CurrentPlayer->field_488.current_tile_ptr_for_portals = stan;
    g_CurrentPlayer->pos = cam_pos;
    g_CurrentPlayer->pos3 = cam_pos;
    g_CurrentPlayer->field_488.pos = cam_pos;
    g_CurrentPlayer->field_488.pos3 = cam_pos;

    sub_GAME_7F0876C4(&cam_pos, &cam_look, &cam_up);

    // 4. Render visual scene
    gdl = skyRender(gdl);
    bgRoomVisibilityRelated();
    gdl = bgLevelRender_practice(gdl);
    gdl = sub_GAME_7F049B58(gdl);
#if defined(VERSION_EU)
    sub_GAME_7F0A46A0(&gdl, 1);
#else
    sub_GAME_7F0A4824(&gdl, 1);
#endif
    gdl = sub_GAME_7F0A2C44(gdl);
    gdl = explosionRenderFlyingParticles(gdl);

    // 5. Draw white border around slot
    gDPPipeSync(gdl++);
    gDPSetRenderMode(gdl++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCycleType(gdl++, G_CYC_FILL);
    gDPSetFillColor(gdl++, 0xFFFFFFFF);
    gDPSetScissor(gdl++, G_SC_NON_INTERLACE, 0, 0, screen_w, viGetY());

    gDPFillRectangle(gdl++, left, top, left + PIP_SIZE - 1, top); // Top
    gDPFillRectangle(gdl++, left, top + PIP_SIZE - 1, left + PIP_SIZE - 1,
                     top + PIP_SIZE - 1);                         // Bottom
    gDPFillRectangle(gdl++, left, top, left, top + PIP_SIZE - 1); // Left
    gDPFillRectangle(gdl++, left + PIP_SIZE - 1, top, left + PIP_SIZE - 1,
                     top + PIP_SIZE - 1); // Right
    gDPPipeSync(gdl++);
  }

  // Restore player state
  g_CurrentPlayer->viewx = saved_viewx;
  g_CurrentPlayer->viewy = saved_viewy;
  g_CurrentPlayer->viewleft = saved_viewleft;
  g_CurrentPlayer->viewtop = saved_viewtop;

  g_CurrentPlayer->c_screenwidth = saved_screenwidth;
  g_CurrentPlayer->c_screenheight = saved_screenheight;
  g_CurrentPlayer->c_halfwidth = saved_halfwidth;
  g_CurrentPlayer->c_halfheight = saved_halfheight;
  g_CurrentPlayer->c_screenleft = saved_screenleft;
  g_CurrentPlayer->c_screentop = saved_screentop;
  g_CurrentPlayer->c_scalex = saved_scalex;
  g_CurrentPlayer->c_scaley = saved_scaley;

  g_CurrentPlayer->fovy = saved_fovy;
  g_CurrentPlayer->aspect = saved_aspect;
  g_CurrentPlayer->c_perspnear = saved_perspnear;
  g_CurrentPlayer->c_perspfovy = saved_perspfovy;
  g_CurrentPlayer->c_perspaspect = saved_perspaspect;

  g_CurrentPlayer->field_5C = saved_field_5C;
  g_CurrentPlayer->field_60 = saved_field_60;
  g_CurrentPlayer->field_64 = saved_field_64;
  g_CurrentPlayer->field_68 = saved_field_68;

  g_CurrentPlayer->pos = saved_pos;
  g_CurrentPlayer->pos3 = saved_pos3;
  g_CurrentPlayer->field_488 = saved_field_488;
  g_CurrentPlayer->room_pointer = saved_room_pointer;

  g_CurrentPlayer->current_model_pos = saved_current_model_pos;
  g_CurrentPlayer->previous_model_pos = saved_previous_model_pos;
  g_CurrentPlayer->current_room_pos = saved_current_room_pos;
  g_CurrentPlayer->curRoomIndex = saved_curRoomIndex;

  g_CurrentPlayer->field_10C4 = saved_field_10C4;
  g_CurrentPlayer->field_10C8 = saved_field_10C8;
  g_CurrentPlayer->field_10CC = saved_field_10CC;
  g_CurrentPlayer->field_10D4 = saved_field_10D4;
  g_CurrentPlayer->field_10E0 = saved_field_10E0;
  g_CurrentPlayer->field_10E4 = saved_field_10E4;
  g_CurrentPlayer->field_10E8 = saved_field_10E8;
  g_CurrentPlayer->field_10EC = saved_field_10EC;

  flt_CODE_bss_80079940 = saved_frustum_940;
  flt_CODE_bss_8007994C = saved_frustum_94C;
  flt_CODE_bss_80079950 = saved_frustum_950;
  flt_CODE_bss_8007995C = saved_frustum_95C;
  flt_CODE_bss_80079960 = saved_frustum_960;
  flt_CODE_bss_8007996C = saved_frustum_96C;
  flt_CODE_bss_80079970 = saved_frustum_970;
  flt_CODE_bss_8007997C = saved_frustum_97C;
  flt_CODE_bss_80079980 = saved_frustum_980;

  g_CurrentPlayer->viewports[g_ViBackIndex] = saved_vp;
  osWritebackDCache(&g_CurrentPlayer->viewports[g_ViBackIndex], sizeof(Vp));
  *g_ViBackData = saved_vi;

  currentPlayerSetScreenSize(saved_screenwidth, saved_screenheight);
  currentPlayerSetScreenPosition(saved_screenleft, saved_screentop);
  currentPlayerSetPerspective(saved_perspnear, saved_perspfovy,
                              saved_perspaspect);
  currentPlayerSetCameraScale();

  gdl = video_related_F(gdl);

  return gdl;
}
