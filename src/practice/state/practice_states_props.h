#ifndef PRACTICE_STATES_PROPS_H
#define PRACTICE_STATES_PROPS_H

#include "chrai.h"
#include <bondtypes.h>
#include <ultra64.h>

// TODO: This is safer but makes the memory usage huge
// #define PROPS_STATE_BUFFER_MAX (sizeof(SavedRecordsOfProp) *
// POS_DATA_ENTRY_LEN)
#define PROPS_STATE_BUFFER_MAX 12000

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
} SavedObjectRecord;

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
  SavedPropRecord prop;
  union {
    SavedDoorRecord door;
    // TODO: Add future structs here
  } type;
} SavedRecordsOfProp;

typedef struct {
  u32 size;
  u16 recordCount;
  s16 indexOfFirstEntry;
  s16 indexOfCurrentEntry;
  s16 indexOfFinalEntry;
  u8 data[PROPS_STATE_BUFFER_MAX];
} SavedPropsState;

bool save_props_state(SavedPropsState *dst);
bool load_props_state(SavedPropsState *src);

#endif /* PRACTICE_STATES_PROPS_H */
