#include "practice_states_chr.h"
#include "chr.h"
#include "chrai.h"
#include "initanitable.h"
#include "objecthandler.h"
#include "practice_states_utils.h"
#include <bondconstants.h>
#include <snd.h>

extern s32 chraiGetAIListID(AIRecord *AIList, bool *isGlobalAIList);
extern PathRecord *pathFindById(s32 ID);

#define CHR_COMBAT_HIDDEN_MASK                                                \
  (CHRHIDDEN_FIRE_WEAPON_LEFT | CHRHIDDEN_FIRE_WEAPON_RIGHT |                \
   CHRHIDDEN_FIRE_TRACER | CHRHIDDEN_MOVING)

#define CHR_FLINCH_HIDDEN_MASK CHRHIDDEN_RAND_FLINCH_MASK

#define CHR_DAMAGE_FLAGS_MASK CHRFLAG_INVINCIBLE

#define CHR_BEHAVIOR_FLAGS_MASK                                               \
  (CHRSTART_FORCENOBLOOD | CHRFLAG_CAN_SHOOT_CHRS | CHRFLAG_NO_AUTOAIM |     \
   CHRFLAG_LOCK_Y_POS | CHRFLAG_NO_SHADOW |                                  \
   CHRFLAG_IGNORE_ANIM_TRANSLATION | CHRFLAG_IMPACT_ALWAYS |                 \
   CHRFLAG_INCREASE_RUNNING_SPEED | CHRFLAG_COUNT_DEATH_AS_CIVILIAN |        \
   CHRFLAG_CULL_USING_HITBOX)

typedef struct FiringAnimationTableRef {
  struct weapon_firing_animation_table *table;
  u8 count;
} FiringAnimationTableRef;

static const FiringAnimationTableRef firing_animation_tables[] = {
    {rifle_firing_animation_group1, 1},
    {rifle_firing_animation_group2, 2},
    {rifle_firing_animation_group5, 2},
    {rifle_firing_animation_group3, 1},
    {rifle_firing_animation_group4, 1},
    {pistol_firing_animation_group1, 4},
    {pistol_firing_animation_group2, 2},
    {pistol_firing_animation_group3, 4},
    {pistol_firing_animation_group6, 4},
    {pistol_firing_animation_group4, 1},
    {pistol_firing_animation_group5, 1},
    {doubles_firing_animation_group1, 1},
    {doubles_firing_animation_group2, 2},
    {doubles_firing_animation_group3, 2},
    {crouched_rifle_firing_animation_group1, 1},
    {crouched_rifle_firing_animation_groupA, 1},
    {crouched_rifle_firing_animation_group2, 1},
    {crouched_rifle_firing_animation_group3, 1},
    {crouched_pistol_firing_animation_group1, 2},
    {crouched_pistol_firing_animation_group2, 3},
    {crouched_pistol_firing_animation_group3, 2},
    {crouched_doubles_firing_animation_group1, 2},
    {crouched_doubles_firing_animation_group2, 2},
    {crouched_doubles_firing_animation_group3, 2},
    {D_80030078, 20},
    {D_80030660, 10},
};

static s16 get_firing_animation_id(
    const struct weapon_firing_animation_table *animation) {
  s32 table;
  s32 row;

  if (animation == NULL) {
    return -1;
  }

  for (table = 0;
       table < (s32)(sizeof(firing_animation_tables) /
                     sizeof(firing_animation_tables[0]));
       table++) {
    for (row = 0; row < firing_animation_tables[table].count; row++) {
      if (animation == &firing_animation_tables[table].table[row]) {
        return (s16)((table << 8) | row);
      }
    }
  }

  return -1;
}

static struct weapon_firing_animation_table *
get_firing_animation_by_id(s16 id) {
  u8 table;
  u8 row;

  if (id < 0) {
    return NULL;
  }

  table = (u16)id >> 8;
  row = id & 0xff;

  if (table >=
          sizeof(firing_animation_tables) / sizeof(firing_animation_tables[0]) ||
      row >= firing_animation_tables[table].count) {
    return NULL;
  }

  return &firing_animation_tables[table].table[row];
}

static bool is_supported_action(ACT_TYPE actiontype) {
  switch (actiontype) {
  case ACT_INIT:
  case ACT_STAND:
  case ACT_KNEEL:
  case ACT_ANIM:
  case ACT_DIE:
  case ACT_DEAD:
  case ACT_ARGH:
  case ACT_PREARGH:
  case ACT_ATTACK:
  case ACT_ATTACKWALK:
  case ACT_ATTACKROLL:
  case ACT_SIDESTEP:
  case ACT_JUMPOUT:
  case ACT_RUNPOS:
  case ACT_PATROL:
  case ACT_GOPOS:
  case ACT_SURRENDER:
  case ACT_LOOKATTARGET:
  case ACT_SURPRISED:
  case ACT_STARTALARM:
  case ACT_THROWGRENADE:
  case ACT_TURNDIR:
  case ACT_TEST:
  case ACT_BONDINTRO:
  case ACT_BONDDIE:
  case ACT_BONDMULTI:
  case ACT_NULL:
    return TRUE;
  default:
    return FALSE;
  }
}

static bool is_supported_chr_action(const ChrRecord *chr) {
  if (!is_supported_action(chr->actiontype)) {
    return FALSE;
  }

  switch (chr->actiontype) {
  case ACT_ATTACK:
    return get_firing_animation_id(chr->act_attack.animfloats) >= 0;
  case ACT_ATTACKWALK:
    return get_firing_animation_id(chr->act_attackwalk.animfloats) >= 0;
  case ACT_ATTACKROLL:
    return get_firing_animation_id(chr->act_attackroll.animfloats) >= 0;
  case ACT_BONDMULTI:
    return chr->act_bondmulti.unk2c == NULL ||
           get_firing_animation_id(
               (struct weapon_firing_animation_table *)
                   chr->act_bondmulti.unk2c) >= 0;
  default:
    return TRUE;
  }
}

static s32 get_animation_offset(const ModelAnimation *animation) {
  if (animation == NULL) {
    return -1;
  }

  return (const u8 *)animation - &ptr_animation_table->data[0];
}

static ModelAnimation *get_animation_by_offset(s32 offset) {
  if (offset < 0) {
    return NULL;
  }

  return (ModelAnimation *)&ptr_animation_table->data[offset];
}

static s16 get_waypoint_index(const waypoint *point) {
  if (point == NULL || g_CurrentSetup.pathwaypoints == NULL) {
    return -1;
  }

  return point - g_CurrentSetup.pathwaypoints;
}

static waypoint *get_waypoint_by_index(s16 index) {
  if (index < 0 || g_CurrentSetup.pathwaypoints == NULL) {
    return NULL;
  }

  return &g_CurrentSetup.pathwaypoints[index];
}

static s16 get_patrol_path_id(const struct patrol_path *path) {
  const PathRecord *record = (const PathRecord *)path;
  return record != NULL ? record->ID : -1;
}

static struct patrol_path *get_patrol_path_by_id(s16 id) {
  return id >= 0 ? (struct patrol_path *)pathFindById(id) : NULL;
}

static void save_waydata(StateStream *stream, const struct waydata *waydata) {
  write_u8(stream, (u8)waydata->mode);
  write_u8(stream, (u8)waydata->unk01);
  write_u8(stream, (u8)waydata->unk02);
  write_u8(stream, (u8)waydata->unk03);
  write_bytes(stream, &waydata->pos, sizeof(coord3d));
  write_bytes(stream, &waydata->pos2, sizeof(coord3d));
  write_bytes(stream, &waydata->pos3, sizeof(coord3d));
  write_u32(stream, waydata->age);
  write_bytes(stream, &waydata->pos_copy, sizeof(coord3d));
  write_f32(stream, waydata->segdistdone);
  write_f32(stream, waydata->segdisttotal);
}

static void load_waydata(StateStream *stream, struct waydata *waydata) {
  waydata->mode = (s8)read_u8(stream);
  waydata->unk01 = (s8)read_u8(stream);
  waydata->unk02 = (s8)read_u8(stream);
  waydata->unk03 = (s8)read_u8(stream);
  read_bytes(stream, &waydata->pos, sizeof(coord3d));
  read_bytes(stream, &waydata->pos2, sizeof(coord3d));
  read_bytes(stream, &waydata->pos3, sizeof(coord3d));
  waydata->age = read_u32(stream);
  read_bytes(stream, &waydata->pos_copy, sizeof(coord3d));
  waydata->segdistdone = read_f32(stream);
  waydata->segdisttotal = read_f32(stream);
}

static void save_supported_action(StateStream *stream, const ChrRecord *chr) {
  s32 i;

  write_u8(stream, (u8)chr->actiontype);

  switch (chr->actiontype) {
  case ACT_STAND:
    write_u32(stream, chr->act_stand.prestand);
    write_u32(stream, chr->act_stand.face_entitytype);
    write_u32(stream, chr->act_stand.face_entityid);
    write_u32(stream, chr->act_stand.reaim);
    write_u32(stream, chr->act_stand.turning);
    write_u32(stream, chr->act_stand.checkfacingwall);
    write_u32(stream, chr->act_stand.wallcount);
    break;
  case ACT_ANIM:
    write_u8(stream, chr->act_anim.unk02c != 0);
    write_u8(stream, chr->act_anim.holdLastFrame != 0);
    write_u8(stream, chr->act_anim.playSfx != 0);
    write_u8(stream, chr->act_anim.idleOnEnd != 0);
    write_u8(stream, chr->act_anim.noTranslate != 0);
    write_u8(stream, (chr->chrflags & CHRFLAG_02000000) != 0);
    break;
  case ACT_DIE:
    // notifychrindex (offset 0) is reused by chrlvIterateGuardSeeShotDie as the
    // guard-notification scan cursor. The thud frames fire one-shot SFX and are
    // set to -1.0 once played, so restoring them prevents replay. timeextra,
    // elapseextra, extraspeed, and drcarollimagedelay are written at death entry
    // but never read, so they are omitted.
    write_u32(stream, chr->act_die.notifychrindex);
    write_f32(stream, chr->act_die.thudframe1);
    write_f32(stream, chr->act_die.thudframe2);
    break;
  case ACT_DEAD:
    // The corpse fade/reap timer lives in the offset-0 word, which the engine
    // reads as act_init.padding[0] (aliasing the bool act_dead.allowfade): -1
    // means "just entered", and >=0 is the elapsed fade timer that fadealpha is
    // derived from. The other named act_dead fields are never read.
    write_u32(stream, chr->act_init.padding[0]);
    break;
  case ACT_ARGH:
    // notifychrindex (offset 0) is reused by chrlvIterateGuardSeeShotDie as the
    // guard-notification scan cursor while ACT_ARGH/ACT_DIE are active.
    write_u32(stream, chr->act_argh.notifychrindex);
    write_u32(stream, chr->act_argh.unk30);
    break;
  case ACT_PREARGH:
    // unk044 has no reader or writer in GoldenEye and is omitted.
    write_bytes(stream, &chr->act_preargh.pos, sizeof(coord3d));
    write_f32(stream, chr->act_preargh.unk038);
    write_u32(stream, chr->act_preargh.unk03c);
    write_u32(stream, chr->act_preargh.unk040);
    break;
  case ACT_ATTACK:
    write_u16(stream, (u16)get_firing_animation_id(chr->act_attack.animfloats));
    write_u8(stream, (u8)chr->act_attack.unk30);
    write_u8(stream, (u8)chr->act_attack.unk31);
    write_u8(stream, (u8)chr->act_attack.unk32);
    write_u8(stream, (u8)chr->act_attack.unk33);
    write_u8(stream, (u8)chr->act_attack.unk34);
    write_u8(stream, (u8)chr->act_attack.unk36);
    write_u8(stream, (u8)chr->act_attack.unk37);
    write_bytes(stream, chr->act_attack.unk38, sizeof(chr->act_attack.unk38));
    write_bytes(stream, chr->act_attack.unk3a, sizeof(chr->act_attack.unk3a));
    write_bytes(stream, chr->act_attack.unk3c, sizeof(chr->act_attack.unk3c));
    write_u32(stream, chr->act_attack.unk40);
    write_u32(stream, chr->act_attack.unk44);
    write_u32(stream, chr->act_attack.attack_time);
    write_u32(stream, chr->act_attack.attacktype);
    write_u32(stream, chr->act_attack.entityid);
    write_u32(stream, chr->act_attack.unk54);
    write_u32(stream, chr->act_attack.type_of_motion);
    write_u8(stream, (u8)chr->act_attack.attack_item);
    break;
  case ACT_ATTACKWALK:
    write_u32(stream, chr->act_attackwalk.clock_timer30);
    write_u32(stream, chr->act_attackwalk.clock_timer34);
    write_u32(stream, chr->act_attackwalk.unk038);
    write_u16(stream,
              (u16)get_firing_animation_id(chr->act_attackwalk.animfloats));
    write_u32(stream, chr->act_attackwalk.timer40);
    write_u32(stream, chr->act_attackwalk.unk044);
    write_bytes(stream, chr->act_attackwalk.unk48,
                sizeof(chr->act_attackwalk.unk48));
    write_bytes(stream, chr->act_attackwalk.unk4a,
                sizeof(chr->act_attackwalk.unk4a));
    write_bytes(stream, chr->act_attackwalk.unk4C,
                sizeof(chr->act_attackwalk.unk4C));
    write_f32(stream, chr->act_attackwalk.speed);
    write_u8(stream, (u8)chr->act_attackwalk.attack_item);
    break;
  case ACT_ATTACKROLL:
    write_u16(stream,
              (u16)get_firing_animation_id(chr->act_attackroll.animfloats));
    write_u8(stream, (u8)chr->act_attackroll.unk30);
    write_u8(stream, (u8)chr->act_attackroll.unk31);
    write_u8(stream, (u8)chr->act_attackroll.unk32);
    write_u8(stream, (u8)chr->act_attackroll.unk33);
    write_u8(stream, (u8)chr->act_attackroll.unk34);
    write_u8(stream, (u8)chr->act_attackroll.unk35);
    write_u8(stream, (u8)chr->act_attackroll.unk36);
    write_u8(stream, (u8)chr->act_attackroll.unk37);
    write_bytes(stream, chr->act_attackroll.unk38,
                sizeof(chr->act_attackroll.unk38));
    write_bytes(stream, chr->act_attackroll.unk3a,
                sizeof(chr->act_attackroll.unk3a));
    write_bytes(stream, chr->act_attackroll.unk3c,
                sizeof(chr->act_attackroll.unk3c));
    write_u32(stream, chr->act_attackroll.unk40);
    write_u32(stream, chr->act_attackroll.unk44);
    write_u32(stream, chr->act_attackroll.attack_time);
    write_u32(stream, chr->act_attackroll.unk4c[0]);
    write_u32(stream, chr->act_attackroll.unk4c[1]);
    write_u32(stream, chr->act_attackroll.unk54[0]);
    write_u32(stream, chr->act_attackroll.unk54[1]);
    write_u8(stream, (u8)chr->act_attackroll.attack_item);
    break;
  case ACT_RUNPOS:
    write_bytes(stream, &chr->act_runpos.pos, sizeof(coord3d));
    write_f32(stream, chr->act_runpos.neardist);
    write_u32(stream, chr->act_runpos.eta60);
    write_f32(stream, chr->act_runpos.turnspeed);
    break;
  case ACT_PATROL:
    write_u16(stream, (u16)get_patrol_path_id(chr->act_patrol.path));
    write_u32(stream, chr->act_patrol.nextstep);
    write_u8(stream, chr->act_patrol.forward);
    save_waydata(stream, &chr->act_patrol.waydata);
    write_u32(stream, chr->act_patrol.lastvisible60);
    write_f32(stream, chr->act_patrol.speed);
    break;
  case ACT_GOPOS:
    write_bytes(stream, &chr->act_gopos.targetpos, sizeof(coord3d));
    write_u32(stream, get_tile_offset(chr->act_gopos.target));
    write_u16(stream, (u16)get_waypoint_index(chr->act_gopos.target_path));
    for (i = 0; i < MAX_CHRWAYPOINTS; i++) {
      write_u16(stream, (u16)get_waypoint_index(chr->act_gopos.waypoints[i]));
    }
    write_u8(stream, chr->act_gopos.curindex);
    write_u8(stream, chr->act_gopos.unk59);
    write_u16(stream, chr->act_gopos.unk5a);
    save_waydata(stream, &chr->act_gopos.waydata);
    write_u32(stream, chr->act_gopos.unk9c);
    write_f32(stream, chr->act_gopos.speed);
    break;
  case ACT_BONDMULTI:
    write_u16(
        stream,
        (u16)get_firing_animation_id(
            (struct weapon_firing_animation_table *)chr->act_bondmulti.unk2c));
    break;
  default:
    // Payload-free actions are driven entirely by their discriminator, Model
    // state, and (where applicable) restored equipment attachments.
    break;
  }
}

static void load_supported_action(StateStream *stream, ChrRecord *chr) {
  ACT_TYPE actiontype = (ACT_TYPE)read_u8(stream);
  s32 i;

  if (chr != NULL) {
    chr->actiontype = actiontype;
  }

  switch (actiontype) {
  case ACT_STAND: {
    s32 prestand = read_u32(stream);
    s32 face_entitytype = read_u32(stream);
    s32 face_entityid = read_u32(stream);
    s32 reaim = read_u32(stream);
    s32 turning = read_u32(stream);
    u32 checkfacingwall = read_u32(stream);
    s32 wallcount = read_u32(stream);

    if (chr != NULL) {
      chr->act_stand.prestand = prestand;
      chr->act_stand.face_entitytype = face_entitytype;
      chr->act_stand.face_entityid = face_entityid;
      chr->act_stand.reaim = reaim;
      chr->act_stand.turning = turning;
      chr->act_stand.checkfacingwall = checkfacingwall;
      chr->act_stand.wallcount = wallcount;
    }
    break;
  }
  case ACT_ANIM: {
    u32 unk02c = read_u8(stream);
    u32 holdLastFrame = read_u8(stream);
    u32 playSfx = read_u8(stream);
    u32 idleOnEnd = read_u8(stream);
    u32 noTranslate = read_u8(stream);
    bool sfxPlayed = read_u8(stream);

    if (chr != NULL) {
      chr->act_anim.unk02c = unk02c;
      chr->act_anim.holdLastFrame = holdLastFrame;
      chr->act_anim.playSfx = playSfx;
      chr->act_anim.idleOnEnd = idleOnEnd;
      chr->act_anim.noTranslate = noTranslate;
      if (sfxPlayed) {
        chr->chrflags |= CHRFLAG_02000000;
      } else {
        chr->chrflags &= ~CHRFLAG_02000000;
      }
    }
    break;
  }
  case ACT_DIE: {
    s32 notifychrindex = read_u32(stream);
    f32 thudframe1 = read_f32(stream);
    f32 thudframe2 = read_f32(stream);

    if (chr != NULL) {
      chr->act_die.notifychrindex = notifychrindex;
      chr->act_die.thudframe1 = thudframe1;
      chr->act_die.thudframe2 = thudframe2;
    }
    break;
  }
  case ACT_DEAD: {
    s32 fade_timer = read_u32(stream);

    if (chr != NULL) {
      chr->act_init.padding[0] = fade_timer;
    }
    break;
  }
  case ACT_ARGH: {
    s32 notifychrindex = read_u32(stream);
    s32 unk30 = read_u32(stream);

    if (chr != NULL) {
      chr->act_argh.notifychrindex = notifychrindex;
      chr->act_argh.unk30 = unk30;
    }
    break;
  }
  case ACT_PREARGH: {
    coord3d pos;
    f32 unk038;
    s32 unk03c;
    s32 unk040;

    read_bytes(stream, &pos, sizeof(coord3d));
    unk038 = read_f32(stream);
    unk03c = read_u32(stream);
    unk040 = read_u32(stream);

    if (chr != NULL) {
      chr->act_preargh.pos = pos;
      chr->act_preargh.unk038 = unk038;
      chr->act_preargh.unk03c = unk03c;
      chr->act_preargh.unk040 = unk040;
    }
    break;
  }
  case ACT_ATTACK: {
    s16 anim_id = (s16)read_u16(stream);
    s8 unk30 = (s8)read_u8(stream);
    s8 unk31 = (s8)read_u8(stream);
    s8 unk32 = (s8)read_u8(stream);
    s8 unk33 = (s8)read_u8(stream);
    s8 unk34 = (s8)read_u8(stream);
    s8 unk36 = (s8)read_u8(stream);
    s8 unk37 = (s8)read_u8(stream);
    s8 unk38[2];
    s8 unk3a[2];
    s8 unk3c[2];
    u32 unk40;
    s32 unk44;
    s32 attack_time;
    u32 attacktype;
    u32 entityid;
    u32 unk54;
    s32 type_of_motion;
    s8 attack_item;

    read_bytes(stream, unk38, sizeof(unk38));
    read_bytes(stream, unk3a, sizeof(unk3a));
    read_bytes(stream, unk3c, sizeof(unk3c));
    unk40 = read_u32(stream);
    unk44 = read_u32(stream);
    attack_time = read_u32(stream);
    attacktype = read_u32(stream);
    entityid = read_u32(stream);
    unk54 = read_u32(stream);
    type_of_motion = read_u32(stream);
    attack_item = (s8)read_u8(stream);

    if (chr != NULL) {
      chr->act_attack.animfloats = get_firing_animation_by_id(anim_id);
      chr->act_attack.unk30 = unk30;
      chr->act_attack.unk31 = unk31;
      chr->act_attack.unk32 = unk32;
      chr->act_attack.unk33 = unk33;
      chr->act_attack.unk34 = unk34;
      chr->act_attack.unk36 = unk36;
      chr->act_attack.unk37 = unk37;
      chr->act_attack.unk38[0] = unk38[0];
      chr->act_attack.unk38[1] = unk38[1];
      chr->act_attack.unk3a[0] = unk3a[0];
      chr->act_attack.unk3a[1] = unk3a[1];
      chr->act_attack.unk3c[0] = unk3c[0];
      chr->act_attack.unk3c[1] = unk3c[1];
      chr->act_attack.unk40 = unk40;
      chr->act_attack.unk44 = unk44;
      chr->act_attack.attack_time = attack_time;
      chr->act_attack.attacktype = attacktype;
      chr->act_attack.entityid = entityid;
      chr->act_attack.unk54 = unk54;
      chr->act_attack.type_of_motion = type_of_motion;
      chr->act_attack.attack_item = attack_item;
    }
    break;
  }
  case ACT_ATTACKWALK: {
    s32 clock_timer30 = read_u32(stream);
    s32 clock_timer34 = read_u32(stream);
    u32 unk038 = read_u32(stream);
    s16 anim_id = (s16)read_u16(stream);
    s32 timer40 = read_u32(stream);
    s32 unk044 = read_u32(stream);
    s8 unk48[2];
    s8 unk4a[2];
    s8 unk4C[2];
    f32 speed;
    s8 attack_item;

    read_bytes(stream, unk48, sizeof(unk48));
    read_bytes(stream, unk4a, sizeof(unk4a));
    read_bytes(stream, unk4C, sizeof(unk4C));
    speed = read_f32(stream);
    attack_item = (s8)read_u8(stream);

    if (chr != NULL) {
      chr->act_attackwalk.clock_timer30 = clock_timer30;
      chr->act_attackwalk.clock_timer34 = clock_timer34;
      chr->act_attackwalk.unk038 = unk038;
      chr->act_attackwalk.animfloats =
          get_firing_animation_by_id(anim_id);
      chr->act_attackwalk.timer40 = timer40;
      chr->act_attackwalk.unk044 = unk044;
      chr->act_attackwalk.unk48[0] = unk48[0];
      chr->act_attackwalk.unk48[1] = unk48[1];
      chr->act_attackwalk.unk4a[0] = unk4a[0];
      chr->act_attackwalk.unk4a[1] = unk4a[1];
      chr->act_attackwalk.unk4C[0] = unk4C[0];
      chr->act_attackwalk.unk4C[1] = unk4C[1];
      chr->act_attackwalk.speed = speed;
      chr->act_attackwalk.attack_item = attack_item;
    }
    break;
  }
  case ACT_ATTACKROLL: {
    s16 anim_id = (s16)read_u16(stream);
    s8 unk30 = (s8)read_u8(stream);
    s8 unk31 = (s8)read_u8(stream);
    s8 unk32 = (s8)read_u8(stream);
    s8 unk33 = (s8)read_u8(stream);
    s8 unk34 = (s8)read_u8(stream);
    s8 unk35 = (s8)read_u8(stream);
    s8 unk36 = (s8)read_u8(stream);
    s8 unk37 = (s8)read_u8(stream);
    s8 unk38[2];
    s8 unk3a[2];
    s8 unk3c[2];
    u32 unk40;
    s32 unk44;
    s32 attack_time;
    s32 unk4c[2];
    s32 unk54[2];
    s8 attack_item;

    read_bytes(stream, unk38, sizeof(unk38));
    read_bytes(stream, unk3a, sizeof(unk3a));
    read_bytes(stream, unk3c, sizeof(unk3c));
    unk40 = read_u32(stream);
    unk44 = read_u32(stream);
    attack_time = read_u32(stream);
    unk4c[0] = read_u32(stream);
    unk4c[1] = read_u32(stream);
    unk54[0] = read_u32(stream);
    unk54[1] = read_u32(stream);
    attack_item = (s8)read_u8(stream);

    if (chr != NULL) {
      chr->act_attackroll.animfloats =
          get_firing_animation_by_id(anim_id);
      chr->act_attackroll.unk30 = unk30;
      chr->act_attackroll.unk31 = unk31;
      chr->act_attackroll.unk32 = unk32;
      chr->act_attackroll.unk33 = unk33;
      chr->act_attackroll.unk34 = unk34;
      chr->act_attackroll.unk35 = unk35;
      chr->act_attackroll.unk36 = unk36;
      chr->act_attackroll.unk37 = unk37;
      chr->act_attackroll.unk38[0] = unk38[0];
      chr->act_attackroll.unk38[1] = unk38[1];
      chr->act_attackroll.unk3a[0] = unk3a[0];
      chr->act_attackroll.unk3a[1] = unk3a[1];
      chr->act_attackroll.unk3c[0] = unk3c[0];
      chr->act_attackroll.unk3c[1] = unk3c[1];
      chr->act_attackroll.unk40 = unk40;
      chr->act_attackroll.unk44 = unk44;
      chr->act_attackroll.attack_time = attack_time;
      chr->act_attackroll.unk4c[0] = unk4c[0];
      chr->act_attackroll.unk4c[1] = unk4c[1];
      chr->act_attackroll.unk54[0] = unk54[0];
      chr->act_attackroll.unk54[1] = unk54[1];
      chr->act_attackroll.attack_item = attack_item;
    }
    break;
  }
  case ACT_RUNPOS: {
    coord3d pos;
    f32 neardist;
    s32 eta60;
    f32 turnspeed;

    read_bytes(stream, &pos, sizeof(coord3d));
    neardist = read_f32(stream);
    eta60 = read_u32(stream);
    turnspeed = read_f32(stream);

    if (chr != NULL) {
      chr->act_runpos.pos = pos;
      chr->act_runpos.neardist = neardist;
      chr->act_runpos.eta60 = eta60;
      chr->act_runpos.turnspeed = turnspeed;
    }
    break;
  }
  case ACT_PATROL: {
    s16 path_id = (s16)read_u16(stream);
    s32 nextstep = read_u32(stream);
    bool forward = read_u8(stream);
    struct waydata waydata;
    s32 lastvisible60;
    f32 speed;

    load_waydata(stream, &waydata);
    lastvisible60 = read_u32(stream);
    speed = read_f32(stream);

    if (chr != NULL) {
      chr->act_patrol.path = get_patrol_path_by_id(path_id);
      chr->act_patrol.nextstep = nextstep;
      chr->act_patrol.forward = forward;
      chr->act_patrol.waydata = waydata;
      chr->act_patrol.lastvisible60 = lastvisible60;
      chr->act_patrol.speed = speed;
    }
    break;
  }
  case ACT_GOPOS: {
    coord3d targetpos;
    s32 target_offset;
    s16 target_path_index;
    s16 waypoint_indices[MAX_CHRWAYPOINTS];
    u8 curindex;
    u8 unk59;
    u16 unk5a;
    struct waydata waydata;
    s32 unk9c;
    f32 speed;

    read_bytes(stream, &targetpos, sizeof(coord3d));
    target_offset = read_u32(stream);
    target_path_index = (s16)read_u16(stream);
    for (i = 0; i < MAX_CHRWAYPOINTS; i++) {
      waypoint_indices[i] = (s16)read_u16(stream);
    }
    curindex = read_u8(stream);
    unk59 = read_u8(stream);
    unk5a = read_u16(stream);
    load_waydata(stream, &waydata);
    unk9c = read_u32(stream);
    speed = read_f32(stream);

    if (chr != NULL) {
      chr->act_gopos.targetpos = targetpos;
      chr->act_gopos.target = get_tile_by_offset(target_offset);
      chr->act_gopos.target_path = get_waypoint_by_index(target_path_index);
      for (i = 0; i < MAX_CHRWAYPOINTS; i++) {
        chr->act_gopos.waypoints[i] =
            get_waypoint_by_index(waypoint_indices[i]);
      }
      chr->act_gopos.curindex = curindex;
      chr->act_gopos.unk59 = unk59;
      chr->act_gopos.unk5a = unk5a;
      chr->act_gopos.waydata = waydata;
      chr->act_gopos.unk9c = unk9c;
      chr->act_gopos.speed = speed;
    }
    break;
  }
  case ACT_BONDMULTI: {
    s16 animation_id = (s16)read_u16(stream);

    if (chr != NULL) {
      chr->act_bondmulti.unk2c =
          (f32 *)get_firing_animation_by_id(animation_id);
    }
    break;
  }
  default:
    break;
  }
}

static void save_model_animation(StateStream *stream, const Model *model) {
  ModelRwData_HeaderRecord *root_data;
  bool has_root_data =
      model->obj != NULL && model->obj->RootNode != NULL &&
      (model->obj->RootNode->Opcode & 0xff) == MODELNODE_OPCODE_HEADER;

  write_u32(stream, get_animation_offset(model->anim));
  write_u32(stream, get_animation_offset(model->anim2));
  write_u8(stream, (u8)model->gunhand);
  write_u8(stream, (u8)model->unk25);
  write_u8(stream, (u8)model->animlooping);
  write_u8(stream, (u8)model->unk27);
  write_f32(stream, model->unk28);
  write_f32(stream, model->unk2c);
  write_u16(stream, (u16)model->framea);
  write_u16(stream, (u16)model->frameb);
  write_f32(stream, model->endframe);
  write_f32(stream, model->speed);
  write_f32(stream, model->newspeed);
  write_f32(stream, model->oldspeed);
  write_f32(stream, model->timespeed);
  write_f32(stream, model->elapsespeed);
  write_f32(stream, model->unk58);
  write_f32(stream, model->unk5c);
  write_u16(stream, (u16)model->frame2a);
  write_u16(stream, (u16)model->frame2b);
  write_f32(stream, model->unk6c);
  write_f32(stream, model->speed2);
  write_u32(stream, model->unk74);
  write_u32(stream, model->unk78);
  write_f32(stream, model->unk7c);
  write_u32(stream, model->unk80);
  write_f32(stream, model->unk84);
  write_f32(stream, model->unk88);
  write_u32(stream, model->unk8c);
  write_f32(stream, model->animloopframe);
  write_f32(stream, model->animloopmerge);
  write_u32(stream, model->unk9c);
  write_u32(stream, model->unka0);
  write_f32(stream, model->playspeed);
  write_f32(stream, model->animrate);
  write_f32(stream, model->unkac);
  write_f32(stream, model->unkb0);
  write_f32(stream, model->unkb4);
  write_f32(stream, model->unkb8);
  write_u32(stream, model->unkbc);

  write_u8(stream, has_root_data);
  if (has_root_data) {
    root_data = (ModelRwData_HeaderRecord *)modelGetNodeRwData(
        (Model *)model, model->obj->RootNode);
    write_bytes(stream, root_data, sizeof(ModelRwData_HeaderRecord));
  }
}

static void load_model_animation(StateStream *stream, Model *model) {
  s32 anim_offset = read_u32(stream);
  s32 anim2_offset = read_u32(stream);
  s8 gunhand = (s8)read_u8(stream);
  s8 unk25 = (s8)read_u8(stream);
  s8 animlooping = (s8)read_u8(stream);
  s8 unk27 = (s8)read_u8(stream);
  f32 unk28 = read_f32(stream);
  f32 unk2c = read_f32(stream);
  s16 framea = (s16)read_u16(stream);
  s16 frameb = (s16)read_u16(stream);
  f32 endframe = read_f32(stream);
  f32 speed = read_f32(stream);
  f32 newspeed = read_f32(stream);
  f32 oldspeed = read_f32(stream);
  f32 timespeed = read_f32(stream);
  f32 elapsespeed = read_f32(stream);
  f32 unk58 = read_f32(stream);
  f32 unk5c = read_f32(stream);
  s16 frame2a = (s16)read_u16(stream);
  s16 frame2b = (s16)read_u16(stream);
  f32 unk6c = read_f32(stream);
  f32 speed2 = read_f32(stream);
  s32 unk74 = read_u32(stream);
  s32 unk78 = read_u32(stream);
  f32 unk7c = read_f32(stream);
  s32 unk80 = read_u32(stream);
  f32 unk84 = read_f32(stream);
  f32 unk88 = read_f32(stream);
  s32 unk8c = read_u32(stream);
  f32 animloopframe = read_f32(stream);
  f32 animloopmerge = read_f32(stream);
  s32 unk9c = read_u32(stream);
  s32 unka0 = read_u32(stream);
  f32 playspeed = read_f32(stream);
  f32 animrate = read_f32(stream);
  f32 unkac = read_f32(stream);
  f32 unkb0 = read_f32(stream);
  f32 unkb4 = read_f32(stream);
  f32 unkb8 = read_f32(stream);
  s32 unkbc = read_u32(stream);
  bool has_root_data = read_u8(stream);
  ModelRwData_HeaderRecord root_data;

  if (has_root_data) {
    read_bytes(stream, &root_data, sizeof(ModelRwData_HeaderRecord));
  }

  if (model == NULL) {
    return;
  }

  model->anim = get_animation_by_offset(anim_offset);
  model->anim2 = get_animation_by_offset(anim2_offset);
  model->gunhand = gunhand;
  model->unk25 = unk25;
  model->animlooping = animlooping;
  model->unk27 = unk27;
  model->unk28 = unk28;
  model->unk2c = unk2c;
  model->framea = framea;
  model->frameb = frameb;
  model->endframe = endframe;
  model->speed = speed;
  model->newspeed = newspeed;
  model->oldspeed = oldspeed;
  model->timespeed = timespeed;
  model->elapsespeed = elapsespeed;
  model->unk58 = unk58;
  model->unk5c = unk5c;
  model->frame2a = frame2a;
  model->frame2b = frame2b;
  model->unk6c = unk6c;
  model->speed2 = speed2;
  model->unk74 = unk74;
  model->unk78 = unk78;
  model->unk7c = unk7c;
  model->unk80 = unk80;
  model->unk84 = unk84;
  model->unk88 = unk88;
  model->unk8c = unk8c;
  model->animloopframe = animloopframe;
  model->animloopmerge = animloopmerge;
  model->animflipfunc = 0;
  model->unk9c = unk9c;
  model->unka0 = unka0;
  model->playspeed = playspeed;
  model->animrate = animrate;
  model->unkac = unkac;
  model->unkb0 = unkb0;
  model->unkb4 = unkb4;
  model->unkb8 = unkb8;
  model->unkbc = unkbc;

  if (has_root_data && model->obj != NULL && model->obj->RootNode != NULL &&
      (model->obj->RootNode->Opcode & 0xff) == MODELNODE_OPCODE_HEADER) {
    ModelRwData_HeaderRecord *dst =
        (ModelRwData_HeaderRecord *)modelGetNodeRwData(model,
                                                       model->obj->RootNode);
    *dst = root_data;
  }
}

static void clear_chr_transient_sounds(ChrRecord *chr) {
  ALSoundState *sound3 = (ALSoundState *)chr->ptr_SEbuffer3;
  ALSoundState *sound4 = (ALSoundState *)chr->ptr_SEbuffer4;

  if (sound3 != NULL && sndGetPlayingState(sound3) != 0) {
    sndDeactivate(sound3);
  }
  if (sound4 != NULL && sndGetPlayingState(sound4) != 0) {
    sndDeactivate(sound4);
  }

  chr->ptr_SEbuffer3 = NULL;
  chr->ptr_SEbuffer4 = NULL;
}

static void clear_chr_transient_joint_list(ChrRecord *chr) {
  if (chr->field_20 != NULL) {
    sub_GAME_7F06B248(chr->field_20);
    chr->field_20 = NULL;
  }
}

void save_chr_record(StateStream *stream, const ChrRecord *chr) {
  s32 ailist_id = -1;
  bool supported_action = is_supported_chr_action(chr);
  bool has_model_transform =
      chr->model != NULL && chr->model->obj != NULL &&
      chr->model->obj->RootNode != NULL &&
      (chr->model->obj->RootNode->Opcode & 0xff) == MODELNODE_OPCODE_HEADER;

  // Allocation metadata is consumed before the destination ChrRecord exists so
  // a missing CHR can be recreated in its saved slot before the payload loads.
  write_u8(stream, (u8)chr->headnum);
  write_u8(stream, (u8)chr->bodynum);
  write_f32(stream, has_model_transform ? getsubroty(chr->model) : 0.0f);

  write_u8(stream, (u8)chr->accuracyrating);
  write_u8(stream, (u8)chr->speedrating);
  write_u8(stream, (u8)chr->arghrating);
  write_u8(stream, chr->grenadeprob);
  write_f32(stream, chr->visionrange);
  write_f32(stream, chr->hearingscale);
  write_u8(stream, chr->morale);
  write_u8(stream, chr->alertness);
  write_f32(stream, chr->chrwidth);
  write_f32(stream, chr->chrheight);
  write_u8(stream, (u8)chr->flinchcnt);
  write_u16(stream, chr->hidden & CHR_FLINCH_HIDDEN_MASK);
  write_u32(stream, chr->chrflags & CHR_BEHAVIOR_FLAGS_MASK);

  write_u8(stream, chr->fadealpha);
  write_f32(stream, chr->damage);
  write_f32(stream, chr->maxdamage);
  write_u32(stream, chr->chrflags & CHR_DAMAGE_FLAGS_MASK);

  write_u8(stream, (u8)chr->numarghs);
  write_u8(stream, (u8)chr->numclosearghs);
  write_u8(stream, chr->random);
  write_u16(stream, (u16)chr->padpreset1);
  write_u16(stream, (u16)chr->chrpreset1);
  write_u16(stream, (u16)chr->chrseeshot);
  write_u16(stream, (u16)chr->chrseedie);

  write_u32(stream, chr->lastseetarget60);
  write_bytes(stream, &chr->lastknowntargetpos, sizeof(coord3d));
  write_u32(stream, get_tile_offset((StandTile *)chr->targetTile));
  write_u32(stream, chr->seen_bond_time);
  write_u32(stream, chr->lastheartarget60);

  write_u16(stream, (u16)chr->chrnum);
  write_u8(stream, chr->flags2);
  write_u32(stream, chr->timer60);
  write_u8(stream, (chr->hidden & CHRHIDDEN_TIMER_ACTIVE) != 0);
  write_f32(stream, chr->shotbondsum);
  write_u8(stream, chr->firecount[0]);
  write_u8(stream, chr->firecount[1]);
  write_u8(stream, (u8)chr->aimendcount);
  write_f32(stream, chr->aimuplshoulder);
  write_f32(stream, chr->aimuprshoulder);
  write_f32(stream, chr->aimupback);
  write_f32(stream, chr->aimsideback);
  write_f32(stream, chr->aimendlshoulder);
  write_f32(stream, chr->aimendrshoulder);
  write_f32(stream, chr->aimendback);
  write_f32(stream, chr->aimendsideback);
  write_u32(stream, chr->fireslot_word);
  write_u32(stream, chr->field_178[0]);
  write_u32(stream, chr->field_178[1]);
  write_bytes(stream, chr->unk180, sizeof(chr->unk180));
  write_u16(stream, chr->hidden & CHR_COMBAT_HIDDEN_MASK);
  write_bytes(stream, &chr->shadecol, sizeof(rgba_u8));
  write_bytes(stream, &chr->nextcol, sizeof(rgba_u8));

  if (chr->ailist != NULL) {
    bool is_global_ailist;
    ailist_id = chraiGetAIListID(chr->ailist, &is_global_ailist);
  }
  write_u32(stream, ailist_id);
  write_u16(stream, chr->aioffset);
  write_u16(stream, (u16)chr->aireturnlist);
  write_u8(stream, (u8)chr->sleep);

  write_u8(stream, (u8)chr->invalidmove);
  write_f32(stream, chr->sumground);
  write_f32(stream, chr->manground);
  write_f32(stream, chr->ground);
  write_bytes(stream, &chr->fallspeed, sizeof(coord3d));
  write_bytes(stream, &chr->prevpos, sizeof(coord3d));
  write_u32(stream, chr->lastwalk60);
  write_u32(stream, chr->lastmoveok60);
  write_bytes(stream, &chr->collision_bounds, sizeof(rect4f));

  write_u8(stream, has_model_transform);
  if (has_model_transform) {
    coord3d model_offset;

    getsuboffset(chr->model, &model_offset);
    write_bytes(stream, &model_offset, sizeof(coord3d));
    write_f32(stream, getsubroty(chr->model));
  }

  write_u8(stream, supported_action);
  if (supported_action) {
    save_supported_action(stream, chr);
    write_u8(stream, chr->model != NULL);
    if (chr->model != NULL) {
      save_model_animation(stream, chr->model);
    }
  }

  write_u16(stream, get_prop_index(chr->weapons_held[0]));
  write_u16(stream, get_prop_index(chr->weapons_held[1]));
  write_u16(stream, get_prop_index(chr->weapons_held[2]));
  write_u16(stream, get_prop_index(chr->handle_positiondata_hat));

  {
    s32 hand;
    for (hand = 0; hand < 2; hand++) {
      PropRecord *prop = chr->weapons_held[hand];
      WeaponObjRecord *weapon = prop != NULL && prop->obj != NULL &&
                                        prop->obj->type == PROPDEF_COLLECTABLE
                                    ? prop->weapon
                                    : NULL;
      write_u16(stream, weapon != NULL ? (u16)weapon->obj : (u16)-1);
      write_u8(stream, weapon != NULL ? (u8)weapon->weaponnum : 0);
      write_u32(stream, weapon != NULL ? weapon->flags : 0);
    }
  }

  if (chr->handle_positiondata_hat != NULL &&
      chr->handle_positiondata_hat->obj != NULL &&
      chr->handle_positiondata_hat->obj->type == PROPDEF_HAT) {
    write_u16(stream, (u16)chr->handle_positiondata_hat->obj->obj);
    write_u32(stream, chr->handle_positiondata_hat->obj->flags);
  } else {
    write_u16(stream, (u16)-1);
    write_u32(stream, 0);
  }
}

void load_chr_allocation_state(StateStream *stream,
                               ChrAllocationState *allocation) {
  allocation->headnum = (s8)read_u8(stream);
  allocation->bodynum = (s8)read_u8(stream);
  allocation->heading = read_f32(stream);
}

void load_chr_record(StateStream *stream, ChrRecord *chr,
                     ChrAttachmentIndices *attachments) {
  s32 ailist_id;
  bool has_model_transform;
  coord3d model_offset;
  f32 model_heading;
  s16 weapon_indices[3];
  s16 hat_index;
  s32 hand;

  clear_chr_transient_joint_list(chr);

  chr->accuracyrating = (s8)read_u8(stream);
  chr->speedrating = (s8)read_u8(stream);
  chr->arghrating = (s8)read_u8(stream);
  chr->grenadeprob = read_u8(stream);
  chr->visionrange = read_f32(stream);
  chr->hearingscale = read_f32(stream);
  chr->morale = read_u8(stream);
  chr->alertness = read_u8(stream);
  chr->chrwidth = read_f32(stream);
  chr->chrheight = read_f32(stream);
  chr->flinchcnt = (s8)read_u8(stream);
  chr->hidden =
      (chr->hidden & ~CHR_FLINCH_HIDDEN_MASK) |
      ((u16)read_u16(stream) & CHR_FLINCH_HIDDEN_MASK);
  chr->chrflags =
      (chr->chrflags & ~CHR_BEHAVIOR_FLAGS_MASK) |
      (read_u32(stream) & CHR_BEHAVIOR_FLAGS_MASK);

  chr->fadealpha = read_u8(stream);
  chr->damage = read_f32(stream);
  chr->maxdamage = read_f32(stream);
  chr->chrflags =
      (chr->chrflags & ~CHR_DAMAGE_FLAGS_MASK) |
      (read_u32(stream) & CHR_DAMAGE_FLAGS_MASK);

  chr->numarghs = (s8)read_u8(stream);
  chr->numclosearghs = (s8)read_u8(stream);
  chr->random = read_u8(stream);
  chr->padpreset1 = (s16)read_u16(stream);
  chr->chrpreset1 = (s16)read_u16(stream);
  chr->chrseeshot = (s16)read_u16(stream);
  chr->chrseedie = (s16)read_u16(stream);

  chr->lastseetarget60 = read_u32(stream);
  read_bytes(stream, &chr->lastknowntargetpos, sizeof(coord3d));
  chr->targetTile = get_tile_by_offset((s32)read_u32(stream));
  chr->seen_bond_time = read_u32(stream);
  chr->lastheartarget60 = read_u32(stream);

  chr->chrnum = (s16)read_u16(stream);
  chr->flags2 = read_u8(stream);
  chr->timer60 = read_u32(stream);
  if (read_u8(stream)) {
    chr->hidden |= CHRHIDDEN_TIMER_ACTIVE;
  } else {
    chr->hidden &= ~CHRHIDDEN_TIMER_ACTIVE;
  }
  chr->shotbondsum = read_f32(stream);
  chr->firecount[0] = read_u8(stream);
  chr->firecount[1] = read_u8(stream);
  chr->aimendcount = (s8)read_u8(stream);
  chr->aimuplshoulder = read_f32(stream);
  chr->aimuprshoulder = read_f32(stream);
  chr->aimupback = read_f32(stream);
  chr->aimsideback = read_f32(stream);
  chr->aimendlshoulder = read_f32(stream);
  chr->aimendrshoulder = read_f32(stream);
  chr->aimendback = read_f32(stream);
  chr->aimendsideback = read_f32(stream);
  chr->fireslot_word = read_u32(stream);
  chr->field_178[0] = read_u32(stream);
  chr->field_178[1] = read_u32(stream);
  read_bytes(stream, chr->unk180, sizeof(chr->unk180));
  chr->hidden =
      (chr->hidden & ~CHR_COMBAT_HIDDEN_MASK) |
      ((u16)read_u16(stream) & CHR_COMBAT_HIDDEN_MASK);
  clear_chr_transient_sounds(chr);
  read_bytes(stream, &chr->shadecol, sizeof(rgba_u8));
  read_bytes(stream, &chr->nextcol, sizeof(rgba_u8));

  ailist_id = read_u32(stream);
  chr->ailist = ailist_id != -1 ? ailistFindById(ailist_id) : NULL;
  chr->aioffset = read_u16(stream);
  chr->aireturnlist = (s16)read_u16(stream);
  chr->sleep = (s8)read_u8(stream);

  if (chr->ailist == NULL) {
    chr->aioffset = 0;
    chr->aireturnlist = -1;
  }

  chr->invalidmove = (s8)read_u8(stream);
  chr->sumground = read_f32(stream);
  chr->manground = read_f32(stream);
  chr->ground = read_f32(stream);
  read_bytes(stream, &chr->fallspeed, sizeof(coord3d));
  read_bytes(stream, &chr->prevpos, sizeof(coord3d));
  chr->lastwalk60 = read_u32(stream);
  chr->lastmoveok60 = read_u32(stream);
  read_bytes(stream, &chr->collision_bounds, sizeof(rect4f));

  has_model_transform = read_u8(stream);
  if (has_model_transform) {
    read_bytes(stream, &model_offset, sizeof(coord3d));
    model_heading = read_f32(stream);

    if (chr->model != NULL && chr->model->obj != NULL &&
        chr->model->obj->RootNode != NULL) {
      setsuboffset(chr->model, &model_offset);
      setsubroty(chr->model, model_heading);
    }
  }

  if (read_u8(stream)) {
    load_supported_action(stream, chr);
    if (read_u8(stream)) {
      load_model_animation(stream, chr != NULL ? chr->model : NULL);
    }
  }

  weapon_indices[0] = (s16)read_u16(stream);
  weapon_indices[1] = (s16)read_u16(stream);
  weapon_indices[2] = (s16)read_u16(stream);
  hat_index = (s16)read_u16(stream);

  if (attachments != NULL) {
    attachments->weapons_held[0] = weapon_indices[0];
    attachments->weapons_held[1] = weapon_indices[1];
    attachments->weapons_held[2] = weapon_indices[2];
    attachments->hat = hat_index;
  }

  for (hand = 0; hand < 2; hand++) {
    s16 model = (s16)read_u16(stream);
    s8 weaponnum = (s8)read_u8(stream);
    u32 flags = read_u32(stream);

    if (attachments != NULL) {
      attachments->weapon_model[hand] = model;
      attachments->weaponnum[hand] = weaponnum;
      attachments->weapon_flags[hand] = flags;
    }
  }

  {
    s16 model = (s16)read_u16(stream);
    u32 flags = read_u32(stream);

    if (attachments != NULL) {
      attachments->hat_model = model;
      attachments->hat_flags = flags;
    }
  }
}

void load_chr_prop_spatial_state(PropRecord *prop, const coord3d *pos,
                                 s32 stan_offset, const u8 rooms[4],
                                 bool is_new_prop) {
  if (!is_new_prop) {
    chrpropDeregisterRooms(prop);
  }
  prop->pos = *pos;
  prop->stan = get_tile_by_offset(stan_offset);
  prop->rooms[0] = rooms[0];
  prop->rooms[1] = rooms[1];
  prop->rooms[2] = rooms[2];
  prop->rooms[3] = rooms[3];
  chrpropRegisterRooms(prop);
}
