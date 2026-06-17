#include "practice_states_props.h"
#include "chrai.h"
#include "chrobjhandler.h"
#include "memp.h"
#include "practice_states_utils.h"
#include "practice_ui.h"
#include <bondconstants.h>
#include <ultra64.h>

// TODO: Set to true and remove this once all prop types are supported and
// tested
#define ADD_AND_REMOVE_PROPS FALSE

extern void doorActivatePortal(DoorRecord *door);
extern void doorDeactivatePortal(DoorRecord *door);
extern void door7F052B00(DoorRecord *door);
extern void sub_GAME_7F052D8C(DoorRecord *door);
extern union ModelRwData *modelGetNodeRwData(Model *Objinst, ModelNode *root);

static void removePropAtIndex(s16 index) {
  PropRecord *toClear = get_prop_by_index(index);
  if (toClear == NULL || ADD_AND_REMOVE_PROPS)
    return;

  objFreePermanently(toClear->obj, TRUE);
  // TODO: Do I need to free the other records, models, etc. pointed to by
  // this entry? Or do some special cleanup for certain prop types? Like
  // removing them from global variable lists?
}

bool save_props_state(SavedPropsState *dst) {
  u8 *ptr = dst->data;
  u8 *limit = dst->data + PROPS_STATE_BUFFER_MAX - sizeof(SavedRecordsOfProp);
  s32 i;
  s16 idx;

  dst->recordCount = 0;

  for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
    PropRecord *prop = get_prop_by_index(i);
    SavedPropRecord *savedProp = (SavedPropRecord *)ptr;

    if (ptr >= limit) {
      practiceLogWarn("Not enough space to save all props");
      return FALSE;
    }

    if (prop == NULL || !(prop->flags & PROPFLAG_ENABLED)) {
      continue;
    }

    savedProp->index = i;
    savedProp->type = prop->type;
    savedProp->flags = prop->flags;
    savedProp->timetoregen = prop->timetoregen;
    savedProp->pos = prop->pos;
    savedProp->stanOffset = get_tile_offset(prop->stan);
    savedProp->zDepth = prop->zDepth;
    savedProp->parentIndex = get_prop_index(prop->parent);
    savedProp->childIndex = get_prop_index(prop->child);
    savedProp->prevIndex = get_prop_index(prop->prev);
    savedProp->nextIndex = get_prop_index(prop->next);
    savedProp->rooms[0] = prop->rooms[0];
    savedProp->rooms[1] = prop->rooms[1];
    savedProp->rooms[2] = prop->rooms[2];
    savedProp->rooms[3] = prop->rooms[3];
    savedProp->unk30 = prop->unk30;
    ptr += sizeof(SavedPropRecord);

    switch ((PROP_TYPE)prop->type) {
    case PROP_TYPE_DOOR: {
      DoorRecord *door = prop->door;
      SavedDoorRecord *savedDoor = ptr;
      SavedObjectRecord *savedObj = &savedDoor->obj;
      ObjectRecord *obj = prop->obj;
      PropDefHeaderRecord *pdhr = obj;

      if (obj == NULL || door == NULL) {
        break;
      }

      savedObj->extrascale = pdhr->extrascale;
      savedObj->state = pdhr->state;
      savedObj->type = pdhr->type;
      savedObj->obj = obj->obj;
      savedObj->pad = obj->pad;
      savedObj->flags = obj->flags;
      savedObj->flags2 = obj->flags2;
      savedObj->mtx = obj->mtx;
      savedObj->runtime_pos = obj->runtime_pos;
      savedObj->runtime_bitflags = obj->runtime_bitflags;
      savedObj->maxdamage = obj->maxdamage;
      savedObj->damage = obj->damage;
      savedObj->shadecol = obj->shadecol;
      savedObj->nextcol = obj->nextcol;

      savedDoor->maxFrac = door->maxFrac;
      savedDoor->perimFrac = door->perimFrac;
      savedDoor->accel = door->accel;
      savedDoor->decel = door->decel;
      savedDoor->maxSpeed = door->maxSpeed;
      savedDoor->doorFlags = door->doorFlags;
      savedDoor->doorType = door->doorType;
      savedDoor->keyflags = door->keyflags;
      savedDoor->autoCloseFrames = door->autoCloseFrames;
      savedDoor->doorOpenSound = door->doorOpenSound;
      savedDoor->frac = door->frac;
      savedDoor->unkac = door->unkac;
      savedDoor->unkb0 = door->unkb0;
      savedDoor->openPosition = door->openPosition;
      savedDoor->speed = door->speed;
      savedDoor->openstate = door->openstate;
      savedDoor->unkbd = door->unkbd;
      savedDoor->calculatedopacity = door->calculatedopacity;
      savedDoor->TintDist = door->TintDist;
      savedDoor->CullDist = door->CullDist;
      savedDoor->soundType = door->soundType;
      savedDoor->fadeTime60 = door->fadeTime60;
      savedDoor->bbox = door->bbox;
      savedDoor->openedTime = door->openedTime;
      savedDoor->portalNumber = door->portalNumber;
      savedDoor->lastcalc60i = door->lastcalc60i;

      ptr += sizeof(SavedDoorRecord);
      break;
    }

    case PROP_TYPE_OBJ: {
      ObjectRecord *obj = prop->obj;
      PropDefHeaderRecord *pdhr = obj;

      switch (prop->type) {
      case PROPDEF_KEY: {
        KeyRecord *key = prop->door;
        SavedKeyRecord *savedKey = ptr;
        SavedObjectRecord *savedObj = &savedKey->obj;

        if (obj == NULL || key == NULL) {
          break;
        }

        savedObj->extrascale = pdhr->extrascale;
        savedObj->state = pdhr->state;
        savedObj->type = pdhr->type;
        savedObj->obj = obj->obj;
        savedObj->pad = obj->pad;
        savedObj->flags = obj->flags;
        savedObj->flags2 = obj->flags2;
        savedObj->mtx = obj->mtx;
        savedObj->runtime_pos = obj->runtime_pos;
        savedObj->runtime_bitflags = obj->runtime_bitflags;
        savedObj->maxdamage = obj->maxdamage;
        savedObj->damage = obj->damage;
        savedObj->shadecol = obj->shadecol;
        savedObj->nextcol = obj->nextcol;

        savedKey->keyflags = key->keyflags;

        ptr += sizeof(SavedKeyRecord);
        break;
      }

        // TODO: Support all PROPDEF types

      default:
        break;
      }
      break;
    }

    // TODO: Support these prop types in future
    case PROP_TYPE_NUL:
    case PROP_TYPE_CHR:
    case PROP_TYPE_WEAPON:
    case PROP_TYPE_PLAYER:
    case PROP_TYPE_VIEWER:
    case PROP_TYPE_EXPLOSION:
    case PROP_TYPE_SMOKE:
    default: {
      break;
    }
    }

    savedProp->byteSize = ptr - (u8 *)savedProp;
    dst->recordCount++;
  }

  dst->size = (u32)(ptr - dst->data);
  dst->indexOfFirstEntry = get_prop_index(ptr_obj_pos_list_first_entry);
  dst->indexOfCurrentEntry = get_prop_index(ptr_obj_pos_list_current_entry);
  dst->indexOfFinalEntry = get_prop_index(ptr_obj_pos_list_final_entry);
  return TRUE;
}

static bool load_saved_object(char *typeName, ObjectRecord *obj,
                              SavedObjectRecord *savedObj, PropRecord *prop) {
  PropDefHeaderRecord *pdhr = obj;

  pdhr->extrascale = savedObj->extrascale;
  pdhr->state = savedObj->state;
  pdhr->type = savedObj->type;
  obj->obj = savedObj->obj;
  obj->pad = savedObj->pad;
  obj->flags = savedObj->flags;
  obj->flags2 = savedObj->flags2;
  obj->prop = prop;
  if (obj->model == NULL) {
    practiceLogWarn("%s prop has no model set", typeName);
    return FALSE;
  }
  obj->mtx = savedObj->mtx;
  obj->runtime_pos = savedObj->runtime_pos;
  obj->runtime_bitflags = savedObj->runtime_bitflags;
  if (obj->ptr_allocated_collisiondata_block == NULL) {
    practiceLogWarn("%s prop has no collision data set", typeName);
    return FALSE;
  }
  if (obj->projectile || obj->embedment) {
    practiceLogWarn("%s prop has projectile/embedment set", typeName);
    return FALSE;
  }
  obj->maxdamage = savedObj->maxdamage;
  obj->damage = savedObj->damage;
  obj->shadecol = savedObj->shadecol;
  obj->nextcol = savedObj->nextcol;

  return TRUE;
}

bool load_props_state(SavedPropsState *src) {
  const u8 *ptr = src->data;
  const u8 *end = src->data + src->size;
  s32 i;
  u16 nextIndexToRemove = 0;
  s32 c;
  u32 numDoors = 0;
  s16 linkedDoorIndices[POS_DATA_ENTRY_LEN];

  for (i = 0; i < src->recordCount; i++) {
    const SavedPropRecord *savedProp = (const SavedPropRecord *)ptr;
    const u8 *nextEntry = ptr + savedProp->byteSize;
    PropRecord *prop = get_prop_by_index(savedProp->index);
    bool supportedType = FALSE;

    if (ADD_AND_REMOVE_PROPS) {
      // Clear any props which were not saved
      // TODO: Maybe it is slightly better to modify existing if already correct
      // type instead of deallocating then reallocating?
      for (c = nextIndexToRemove; c < savedProp->index; c++) {
        removePropAtIndex(c);
      }
      nextIndexToRemove = savedProp->index + 1;
    }

    // Only modify props which still exist while testing
    if (!ADD_AND_REMOVE_PROPS &&
        (prop == NULL || !(prop->flags & PROPFLAG_ENABLED) ||
         prop->type != savedProp->type)) {
      ptr = nextEntry;
      continue;
    }

    // Only restore prop if we fully support their entire state to avoid bugs
    switch ((PROP_TYPE)prop->type) {
    case PROP_TYPE_DOOR:
      supportedType = TRUE;
    case PROP_TYPE_OBJ:
      switch (prop->obj && prop->obj->obj) {
      case PROPDEF_KEY:
        supportedType = TRUE;
      }
    }

    if (ADD_AND_REMOVE_PROPS || supportedType) {
      prop->type = savedProp->type;
      prop->flags = savedProp->flags;
      prop->timetoregen = savedProp->timetoregen;
      prop->pos = savedProp->pos;
      prop->stan = get_tile_by_offset(savedProp->stanOffset);
      prop->zDepth = savedProp->zDepth;
      if (ADD_AND_REMOVE_PROPS) {
        prop->parent = get_prop_by_index(savedProp->parentIndex);
        prop->child = get_prop_by_index(savedProp->childIndex);
        prop->prev = get_prop_by_index(savedProp->prevIndex);
        prop->next = get_prop_by_index(savedProp->nextIndex);
      }
      prop->rooms[0] = savedProp->rooms[0];
      prop->rooms[1] = savedProp->rooms[1];
      prop->rooms[2] = savedProp->rooms[2];
      prop->rooms[3] = savedProp->rooms[3];
      prop->unk30 = savedProp->unk30;
    }

    ptr += sizeof(SavedPropRecord);

    switch ((PROP_TYPE)savedProp->type) {
    case PROP_TYPE_DOOR: {
      const SavedDoorRecord *savedDoor = (const SavedDoorRecord *)ptr;
      const SavedObjectRecord *savedObj = &savedDoor->obj;
      DoorRecord *door = prop->door;
      ObjectRecord *obj = prop->obj;

      // Doors are never loaded or unloaded, meaning they should always occupy
      // the same slots and we don't need to allocate or free anything
      if (prop->type != PROP_TYPE_DOOR || prop->door == NULL) {
        practiceLogWarn("Prop at index %d is not a door (type=%d)",
                        savedProp->index, savedProp->type);
        return FALSE;
      }

      if (!load_saved_object("Door", obj, savedObj, prop))
        return FALSE;

      // TODO: Would it be simpler and faster to just memcpy all this?
      // door->linkedDoorOffset should never change
      door->maxFrac = savedDoor->maxFrac;
      door->perimFrac = savedDoor->perimFrac;
      door->accel = savedDoor->accel;
      door->decel = savedDoor->decel;
      door->maxSpeed = savedDoor->maxSpeed;
      door->doorFlags = savedDoor->doorFlags;
      door->doorType = savedDoor->doorType;
      door->keyflags = savedDoor->keyflags;
      door->autoCloseFrames = savedDoor->autoCloseFrames;
      door->doorOpenSound = savedDoor->doorOpenSound;
      door->frac = savedDoor->frac;
      door->unkac = savedDoor->unkac;
      door->unkb0 = savedDoor->unkb0;
      door->openPosition = savedDoor->openPosition;
      door->speed = savedDoor->speed;
      door->openstate = savedDoor->openstate;
      door->unkbd = savedDoor->unkbd;
      door->calculatedopacity = savedDoor->calculatedopacity;
      door->TintDist = savedDoor->TintDist;
      door->CullDist = savedDoor->CullDist;
      door->soundType = savedDoor->soundType;
      door->fadeTime60 = savedDoor->fadeTime60;
      // door->linkedDoor should never change
      // door->unkcc is restored later
      door->bbox = savedDoor->bbox;
      door->openedTime = savedDoor->openedTime;
      door->portalNumber = savedDoor->portalNumber;
      door->openSoundState = NULL;  // clear sound state
      door->closeSoundState = NULL; // clear sound state
      door->lastcalc60i = savedDoor->lastcalc60i;

      // Update bounds, collisions, and portals
      door7F052B00(door);

      // Handle vertical/sliding door vertex clipping (DOORFLAG_0004)
      if (door->doorFlags & DOORFLAG_0004) {
        Model *model = obj->model;
        ModelNode *node = model->obj->RootNode->Child->Child;
        struct ModelRoData_DisplayList_CollisionRecord *dlcRecord =
            (struct ModelRoData_DisplayList_CollisionRecord *)node->Data;
        struct ModelRwData_DisplayList_CollisionRecord *dst =
            (struct ModelRwData_DisplayList_CollisionRecord *)
                modelGetNodeRwData(model, node);

        // Restore pristine vertices back to unkcc buffer before applying state
        if (door->unkcc != NULL && dlcRecord != NULL &&
            dlcRecord->Vertices != NULL) {
          s32 var_a0;
          for (var_a0 = 0; var_a0 < dlcRecord->numVertices; var_a0++) {
            door->unkcc[var_a0] = dlcRecord->Vertices[var_a0];
          }
        }

        if (door->openPosition <= 0.0f) {
          if (door->unkcc != NULL) {
            dst->Vertices = door->unkcc;
          }
        } else {
          sub_GAME_7F052D8C(door);
        }
      }

      if (door->openPosition > 0.0f) {
        doorActivatePortal(door);
      } else {
        doorDeactivatePortal(door);
      }
      break;
    }

    case PROP_TYPE_OBJ: {
      switch (prop->obj && prop->obj->obj) {
      case PROPDEF_KEY: {
        const SavedKeyRecord *savedKey = (const SavedKeyRecord *)ptr;
        KeyRecord *key = prop->obj;

        if (!load_saved_object("Key", key, &savedKey->obj, prop))
          return FALSE;

        key->keyflags = savedKey->keyflags;

        break;
      }

        // TODO: Support all PROPDEF types

      default:
        break;
      }
      break;
    }

    // TODO: Support these prop types in future
    case PROP_TYPE_NUL:
    case PROP_TYPE_CHR:
    case PROP_TYPE_WEAPON:
    case PROP_TYPE_PLAYER:
    case PROP_TYPE_VIEWER:
    case PROP_TYPE_EXPLOSION:
    case PROP_TYPE_SMOKE:
    default:
      break;
    }

    ptr = nextEntry;
  }

  if (ADD_AND_REMOVE_PROPS) {
    // Clear any props which were not saved
    for (c = nextIndexToRemove; c < POS_DATA_ENTRY_LEN; c++) {
      removePropAtIndex(c);
    }

    ptr_obj_pos_list_first_entry = get_prop_by_index(src->indexOfFirstEntry);
    ptr_obj_pos_list_current_entry =
        get_prop_by_index(src->indexOfCurrentEntry);
    ptr_obj_pos_list_final_entry = get_prop_by_index(src->indexOfFinalEntry);
  }

  return TRUE;
}
