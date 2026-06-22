#ifndef PRACTICE_STATES_PROPS_H
#define PRACTICE_STATES_PROPS_H

#include "chrai.h"
#include "practice_storage.h"
#include <bondtypes.h>
#include <ultra64.h>

/* The props block is streamed record-by-record to storage; no large
   in-memory buffer is needed.  These limits are used for bounds checks
   against the working-memory record. */

typedef struct {
  u16 byteSize;
  u16 index;
  u8 type;
  u8 flags;
  s16 timetoregen;
  coord3d pos;
  s32 stanOffset;
  f32 zDepth;
  u16 parentIndex;
  u16 childIndex;
  u16 prevIndex;
  u16 nextIndex;
  u8 rooms[PROPRECORD_STAN_ROOM_LEN];
  s32 unk30;
} SavedPropRecord;

typedef struct {
  u16 extrascale;
  u8 state;
  u8 type;
  s16 obj;
  s16 pad;
  u32 flags;
  u32 flags2;
  // PropRecord *prop; // Pointer back to parent PropRecord (recomputed)
  // Model *model; // Pointer to visual Model (loaded asset)
  Mtxf mtx;
  coord3d runtime_pos;
  u32 runtime_bitflags;
  // struct collision_data *ptr_allocated_collisiondata_block; // Pointer (not
  // saved) struct Projectile *projectile; // Pointer (not saved) struct
  // Embedment *embedment; // Pointer (not saved)
  f32 maxdamage;
  f32 damage;
  rgba_u8 shadecol;
  rgba_u8 nextcol;
  // Projectile/embedment state stored separately as global array snapshots.
  // These indices reference into the restored g_Projectiles[] / g_Embedments[].
  s16 projectileIdx;
  s16 embedmentIdx;
  u32 switchStates;
} SavedObjectRecord;

// Entries for the projectile/embedment global array snapshots saved after
// all prop records.
typedef struct {
  s32 index;
  Projectile projectile;
} SavedProjectileEntry;

typedef struct {
  s32 index;
  Embedment embedment;
} SavedEmbedmentEntry;

typedef struct {
  SavedObjectRecord obj;
  // s32 linkedDoorOffset; // Static and never changed
  f32 maxFrac;
  f32 perimFrac;
  f32 accel;
  f32 decel;
  f32 maxSpeed;
  u16 doorFlags;
  u16 doorType;
  u32 keyflags;
  u32 autoCloseFrames;
  u32 doorOpenSound;
  f32 frac;
  f32 unkac;
  f32 unkb0;
  f32 openPosition;
  f32 speed;
  s8 openstate;
  u8 unkbd;
  s16 calculatedopacity;
  s32 TintDist;
  s16 CullDist;
  s8 soundType;
  s8 fadeTime60;
  // struct DoorRecord *linkedDoor; // Static and never changed
  // Vertex* unkcc; // Pointer (not saved)
  struct ModelRoData_BoundingBoxRecord bbox;
  u32 openedTime;
  s32 portalNumber;
  // ALSoundState *openSoundState; // Pointer (cleared on load)
  // ALSoundState *closeSoundState; // Pointer (cleared on load)
  union {
    s32 lastcalc60i;
    f32 lastcalc60f;
  };
} SavedDoorRecord;

typedef struct {
  SavedObjectRecord obj;
  u32 keyflags;
} SavedKeyRecord;

typedef struct {
  SavedObjectRecord obj;
  s32 pad;
  Mtxf unk84;
  f32 unkC4;
  f32 unkC8;
  f32 unkCC;
  f32 unkD0;
  s32 unkD4;
  f32 unkD8;
  f32 unkDC;
  s32 timer;
  s32 convert_to_f32;
  f32 unkE8;
  s32 unkEC;
  s32 unkF0;
  s32 unkF4;
  s32 unkF8;
} SavedCCTVRecord;

typedef struct {
  SavedObjectRecord obj;
  s32 ammoType;
} SavedAmmoCrateRecord;

typedef struct {
  SavedObjectRecord obj;
  s8 weaponnum;
  s8 LinkedWeaponType;
  s16 timer;
  s16 dualweaponPropIndex;
} SavedWeaponObjRecord;

typedef struct {
  u32 cmdlist;
  u16 offset;
  s16 pause60;
  u32 tconfig;
  f32 rot;
  f32 xscale, xscalefrac, xscaleinc, xscaleold, xscalenew;
  f32 yscale, yscalefrac, yscaleinc, yscaleold, yscalenew;
  f32 xmid, xmidfrac, xmidinc, xmidold, xmidnew;
  f32 ymid, ymidfrac, ymidinc, ymidold, ymidnew;
  u8 red, redold, rednew;
  u8 green, greenold, greennew;
  u8 blue, blueold, bluenew;
  u8 alpha, alphaold, alphanew;
  f32 colfrac, colinc;
} SavedMonitorRecord;

typedef struct {
  SavedObjectRecord obj;
  SavedMonitorRecord Monitor;
  s32 OwnerOffset;
  s32 OwnerPart;
  s32 ImageNum;
} SavedMonitorObjRecord;

typedef struct {
  SavedObjectRecord obj;
  SavedMonitorRecord Monitor[4];
  u8 ImageNums[4];
} SavedMultiMonitorObjRecord;

typedef struct {
  SavedObjectRecord obj;
  s32 padID;
  f32 rot_related;
  f32 unk88;
  f32 unk8C;
  f32 unk90;
  f32 unk94;
  f32 unk98;
  f32 unk9C;
  f32 unkA0;
  f32 speed;
  f32 aimdist;
  s32 unkAC;
  f32 unkB0;
  f32 unkB4;
  s32 unkB8;
  s32 unkBC;
  s32 unkC0;
  s32 is_active;
  f32 unkD4;
  struct beam beam;
} SavedAutogunRecord;

typedef struct {
  SavedObjectRecord obj;
  u16 unk80;
  u16 quantities[AMMOTYPE_GLOBAL_MAX];
} SavedMultiAmmoCrateRecord;

typedef struct {
  SavedObjectRecord obj;
  f32 initialamount;
  f32 amount;
} SavedBodyArmourRecord;

typedef struct {
  SavedObjectRecord obj;
  u32 ailist;
  u16 aioffset;
  s16 aireturnlist;
  f32 speed;
  f32 wheelxrot;
  f32 wheelyrot;
  f32 speedaim;
  f32 speedtime60;
  f32 turnrot60;
  f32 roty;
  u32 path;
  s32 nextstep;
} SavedVehichleRecord;

typedef struct {
  SavedObjectRecord obj;
  u32 ailist;
  u16 aioffset;
  s16 aireturnlist;
  f32 rotoryrot;
  f32 rotaryspeed;
  f32 rotaryspeedaim;
  f32 rotaryspeedtime;
  f32 speed;
  f32 speedaim;
  f32 speedtime60;
  f32 yrot;
  s32 nextstep;
  u32 path;
} SavedAircraftRecord;

typedef struct {
  SavedObjectRecord obj;
  coord3d normal;
} SavedGlassRecord;

typedef SavedGlassRecord SavedSafeRecord;

typedef struct {
  SavedObjectRecord obj;
  s32 TintDist;
  s32 CullDist;
  s32 calculatedopacity;
  s32 portalnum;
  f32 unk90;
} SavedTintedGlassRecord;

typedef struct {
  SavedObjectRecord obj;
  struct rect4f rect;
  s32 unkA4;
  s32 unkA8;
  s32 unkAC;
  s32 unkB0;
  s32 unkB4;
  s32 unkB8;
  s32 unkBC;
  s32 unkC0;
  s32 is_firing_tank;
  f32 turret_vertical_angle;
  f32 turret_orientation_angle;
  f32 unkD0;
  f32 stan_y;
  s32 unkD8;
  f32 tank_orientation_angle;
} SavedTankRecord;

typedef struct {
  SavedPropRecord prop;
  union {
    SavedDoorRecord door;
    SavedKeyRecord key;
    SavedCCTVRecord cctv;
    SavedAmmoCrateRecord magazine;
    SavedWeaponObjRecord weaponObj;
    SavedMonitorObjRecord monitor;
    SavedMultiMonitorObjRecord multiMonitor;
    SavedAutogunRecord autogun;
    SavedMultiAmmoCrateRecord ammoCrate;
    SavedBodyArmourRecord armour;
    SavedVehichleRecord vehicle;
    SavedAircraftRecord aircraft;
    SavedGlassRecord glass;
    SavedSafeRecord safe;
    SavedTintedGlassRecord tintedGlass;
    SavedTankRecord tank;
  } type;
} SavedRecordsOfProp;

/* Props block header — written to storage before the variable-length
   prop records.  Also used as PropsHeaderSection in the working-memory
   union (see practice_states.h). */
typedef struct {
  u32 size;
  u16 recordCount;
  s16 indexOfFirstEntry;
  s16 indexOfCurrentEntry;
  s16 indexOfFinalEntry;
} PropsHeaderSection;

typedef PropsHeaderSection SavedPropsState;

/* Function declarations moved to practice_states.h (where SaveWorkMem
   is defined) to avoid circular include dependencies. */

#endif /* PRACTICE_STATES_PROPS_H */
