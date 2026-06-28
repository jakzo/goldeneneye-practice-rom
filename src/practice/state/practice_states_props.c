#include "practice_states_props.h"
#include "chr.h"
#include "chrai.h"
#include "chrlv.h"
#include "chrobjhandler.h"
#include "explosions.h"
#include "objecthandler.h"
#include "practice_states.h"
#include "practice_states_chr.h"
#include "practice_states_props.h"
#include "practice_states_utils.h"
#include "practice_ui.h"
#include <bondconstants.h>
#include <string.h>
#include <ultra64.h>

extern void doorActivatePortal(DoorRecord *door);
extern void doorDeactivatePortal(DoorRecord *door);
extern void door7F052B00(DoorRecord *door);
extern void sub_GAME_7F052D8C(DoorRecord *door);
extern union ModelRwData *modelGetNodeRwData(Model *Objinst, ModelNode *root);
extern void sub_GAME_7F050DE8(Model *model);
extern void objDeform(ObjectRecord *obj, s32 arg1);
extern PathRecord *pathFindById(s32 ID);
extern s32 chraiGetAIListID(AIRecord *AIList, bool *isGlobalAIList);
extern AIRecord *ailistFindById(s32 ID);
extern void projectileFree(Projectile *projectile);
extern void embedmentFree(Embedment *embedment);
extern void projectileReset(Projectile *projectile);
extern void chrpropDelist(PropRecord *prop);
extern s32 chrGetNumFree(void);
extern void clear_aircraft_model_obj(Model *model);
extern PropRecord *hatCreateForChr(ChrRecord *chr, s32 modelnum, u32 flags);

typedef union {
  ObjectRecord base;
  KeyRecord key;
  CCTVRecord cctv;
  AmmoCrateRecord mag;
  WeaponObjRecord wpn;
  MonitorObjRecord mon;
  MultiMonitorObjRecord mmon;
  AutogunRecord agun;
  MultiAmmoCrateRecord ammo;
  BodyArmourRecord arm;
  VehichleRecord veh;
  AircraftRecord air;
  GlassRecord gl;
  TintedGlassRecord tgl;
  TankRecord tank;
} TempObjectRecord;

static void retain_prop_from_free_list(PropRecord *prop) {
  PropRecord *current = ptr_obj_pos_list_final_entry;
  PropRecord *previous = NULL;

  while (current != NULL) {
    if (current == prop) {
      if (previous == NULL) {
        ptr_obj_pos_list_final_entry = current->prev;
      } else {
        previous->prev = current->prev;
      }
      current->prev = NULL;
      return;
    }
    previous = current;
    current = current->prev;
  }
}

static void clear_plain_prop(PropRecord *prop, bool release_prop) {
  if (prop->flags & PROPFLAG_ENABLED) {
    chrpropDeregisterRooms(prop);
    chrpropDelist(prop);
    chrpropDisable(prop);
  }

  prop->voidp = NULL;
  prop->parent = NULL;
  prop->child = NULL;
  prop->prev = NULL;
  prop->next = NULL;
  prop->stan = NULL;
  prop->rooms[0] = 0xff;

  if (release_prop) {
    chrpropFree(prop);
  }
}

static void destroy_chr_prop(PropRecord *prop, bool release_prop) {
  if (prop == NULL || prop->type != PROP_TYPE_CHR) {
    return;
  }

  if (prop->chr != NULL && prop->chr->model != NULL) {
    disable_sounds_attached_to_player_then_something(prop);
  } else if (prop->flags & PROPFLAG_ENABLED) {
    chrpropDeregisterRooms(prop);
  }

  if (prop->flags & PROPFLAG_ENABLED) {
    chrpropDelist(prop);
    chrpropDisable(prop);
  }

  clear_plain_prop(prop, release_prop);
}

static bool clear_prop_for_replacement(PropRecord *prop) {
  if (prop == NULL) {
    return TRUE;
  }

  if (!(prop->flags & PROPFLAG_ENABLED) && prop->parent == NULL) {
    return TRUE;
  }

  if (prop->type == PROP_TYPE_CHR) {
    destroy_chr_prop(prop, FALSE);
    return TRUE;
  }

  if ((prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_DOOR ||
       prop->type == PROP_TYPE_WEAPON) &&
      prop->obj != NULL) {
    // Keep the PropRecord itself out of the free list because the saved record
    // at this index is about to reuse it.
    objFreePermanently(prop->obj, FALSE);
    clear_plain_prop(prop, FALSE);
    return TRUE;
  }

  if (prop->type == PROP_TYPE_EXPLOSION && prop->explosion != NULL) {
    prop->explosion->prop = NULL;
    clear_plain_prop(prop, FALSE);
    return TRUE;
  }

  if (prop->type == PROP_TYPE_SMOKE && prop->smoke != NULL) {
    prop->smoke->prop = NULL;
    clear_plain_prop(prop, FALSE);
    return TRUE;
  }

  if (prop->type == PROP_TYPE_VIEWER || prop->type == PROP_TYPE_NUL) {
    clear_plain_prop(prop, FALSE);
    return TRUE;
  }

  // Future prop types need equivalent retained-slot teardown before the flag
  // can be enabled.
  return FALSE;
}

static PropRecord *create_chr_prop(PropRecord *prop,
                                   const ChrAllocationState *allocation,
                                   const coord3d *pos, s32 stan_offset) {
  Model *model;
  PropRecord *result;

  if (prop == NULL || allocation->bodynum < 0 || chrGetNumFree() < 1) {
    return NULL;
  }

  model = retrieve_header_for_body_and_head(allocation->bodynum,
                                            allocation->headnum, 0);
  if (model == NULL) {
    return NULL;
  }

  retain_prop_from_free_list(prop);

  prop->flags = 0;
  prop->parent = NULL;
  prop->child = NULL;
  prop->prev = NULL;
  prop->next = NULL;
  prop->rooms[0] = 0xff;

  result = init_GUARDdata_with_set_values(
      prop, model, (coord3d *)pos, allocation->heading,
      get_tile_by_offset(stan_offset), NULL);
  if (result == NULL || result->chr == NULL) {
    clear_aircraft_model_obj(model);
    chrpropFree(prop);
    return NULL;
  }

  result->chr->headnum = allocation->headnum;
  result->chr->bodynum = allocation->bodynum;

  // Initialization calculates and registers rooms. The saved common prop
  // payload will install the authoritative room list immediately afterward.
  chrpropDeregisterRooms(result);
  return result;
}

static PropRecord *get_chr_attachment_prop(s16 index, s32 object_type) {
  PropRecord *prop = get_prop_by_index(index);

  if (prop == NULL ||
      (prop->type != PROP_TYPE_OBJ && prop->type != PROP_TYPE_WEAPON) ||
      prop->obj == NULL || prop->obj->type != object_type ||
      prop->obj->prop != prop || prop->obj->model == NULL) {
    return NULL;
  }

  return prop;
}

static bool prop_is_chr_attachment(PropRecord *prop,
                                   PropRecord *attachments[4]) {
  s32 i;

  for (i = 0; i < 4; i++) {
    if (attachments[i] == prop) {
      return TRUE;
    }
  }

  return FALSE;
}

static void detach_old_chr_attachment(ChrRecord *chr, PropRecord *prop,
                                      PropRecord *saved_attachments[4]) {
  if (prop == NULL || prop_is_chr_attachment(prop, saved_attachments) ||
      prop->parent != chr->prop || prop->obj == NULL) {
    return;
  }

  objDetach(prop);
  if (prop->obj->model != NULL) {
    prop->obj->model->attachedto = NULL;
  }
  chrpropActivate(prop);
  chrpropEnable(prop);
  chrpropRegisterRooms(prop);
}

static bool attach_prop_to_chr(ChrRecord *chr, PropRecord *prop,
                               s32 switch_index) {
  ObjectRecord *obj;

  if (prop == NULL || chr->prop == NULL || chr->model == NULL ||
      chr->model->obj == NULL || switch_index >= chr->model->obj->numSwitches ||
      chr->model->obj->Switches[switch_index] == NULL) {
    return FALSE;
  }

  obj = prop->obj;

  if (prop->parent != chr->prop) {
    if (prop->parent != NULL) {
      objDetach(prop);
    } else if (prop->flags & PROPFLAG_ENABLED) {
      chrpropDeregisterRooms(prop);
      chrpropDelist(prop);
    }
    chrpropReparent(prop, chr->prop);
  }

  // Held equipment is neither a deposited projectile nor an embedded object.
  // The projectile/embedment pools were already restored from the save, so a
  // pointer retained by a weapon dropped after the save must not be reused.
  obj->projectile = NULL;
  obj->runtime_bitflags &= ~(RUNTIMEBITFLAG_DEPOSIT | RUNTIMEBITFLAG_EMBEDDED);
  obj->runtime_bitflags |= RUNTIMEBITFLAG_HASOWNER;
  obj->model->attachedto = chr->model;
  obj->model->attachedto_objinst = chr->model->obj->Switches[switch_index];
  chrpropDisable(prop);
  return TRUE;
}

static void restore_chr_attachments(PropRecord *chr_prop,
                                    ChrAttachmentIndices *indices) {
  ChrRecord *chr = chr_prop->chr;
  PropRecord *saved[4];
  PropRecord *old[4];
  s32 i;

  if (chr == NULL || chr->prop != chr_prop) {
    return;
  }

  saved[0] = get_chr_attachment_prop(indices->weapons_held[GUNRIGHT],
                                     PROPDEF_COLLECTABLE);
  saved[1] = get_chr_attachment_prop(indices->weapons_held[GUNLEFT],
                                     PROPDEF_COLLECTABLE);
  saved[2] = get_prop_by_index(indices->weapons_held[2]);
  saved[3] = get_chr_attachment_prop(indices->hat, PROPDEF_HAT);

  if (ADD_AND_REMOVE_PROPS) {
    if (saved[GUNRIGHT] == NULL && indices->weapon_model[GUNRIGHT] >= 0) {
      saved[GUNRIGHT] = chrGiveWeapon(chr, indices->weapon_model[GUNRIGHT],
                                      indices->weaponnum[GUNRIGHT],
                                      indices->weapon_flags[GUNRIGHT]);
    }
    if (saved[GUNLEFT] == NULL && indices->weapon_model[GUNLEFT] >= 0) {
      saved[GUNLEFT] = chrGiveWeapon(chr, indices->weapon_model[GUNLEFT],
                                     indices->weaponnum[GUNLEFT],
                                     indices->weapon_flags[GUNLEFT]);
    }
    if (saved[3] == NULL && indices->hat_model >= 0) {
      saved[3] = hatCreateForChr(chr, indices->hat_model, indices->hat_flags);
    }
  }

  if (saved[0] == saved[1]) {
    saved[1] = NULL;
  }
  if (saved[3] == saved[0] || saved[3] == saved[1]) {
    saved[3] = NULL;
  }

  old[0] = chr->weapons_held[0];
  old[1] = chr->weapons_held[1];
  old[2] = chr->weapons_held[2];
  old[3] = chr->handle_positiondata_hat;

  for (i = 0; i < 4; i++) {
    detach_old_chr_attachment(chr, old[i], saved);
  }

  chr->weapons_held[0] = NULL;
  chr->weapons_held[1] = NULL;
  chr->weapons_held[2] = NULL;
  chr->handle_positiondata_hat = NULL;

  if (!attach_prop_to_chr(chr, saved[GUNRIGHT], 3)) {
    saved[GUNRIGHT] = NULL;
  }
  if (!attach_prop_to_chr(chr, saved[GUNLEFT], 5)) {
    saved[GUNLEFT] = NULL;
  }
  if (!attach_prop_to_chr(chr, saved[3], 6)) {
    saved[3] = NULL;
  }

  chr->weapons_held[GUNRIGHT] = saved[GUNRIGHT];
  chr->weapons_held[GUNLEFT] = saved[GUNLEFT];
  // The third legacy slot has no readers or attachment-node semantics in the
  // current decompilation. Preserve it only when it already names this CHR's
  // child; normal runtime state leaves it NULL.
  if (saved[2] != NULL && saved[2]->parent == chr_prop) {
    chr->weapons_held[2] = saved[2];
  }
  chr->handle_positiondata_hat = saved[3];
}

static void removePropAtIndex(s16 index) {
  PropRecord *toClear = get_prop_by_index(index);
  if (toClear == NULL || !(toClear->flags & PROPFLAG_ENABLED))
    return;

  if (toClear->type == PROP_TYPE_CHR) {
    destroy_chr_prop(toClear, TRUE);
  } else if (toClear->type == PROP_TYPE_OBJ ||
             toClear->type == PROP_TYPE_DOOR ||
             toClear->type == PROP_TYPE_WEAPON) {
    objFreePermanently(toClear->obj, TRUE);
  } else if (toClear->type == PROP_TYPE_EXPLOSION) {
    if (toClear->explosion != NULL) {
      toClear->explosion->prop = NULL;
    }
    clear_plain_prop(toClear, TRUE);
  } else if (toClear->type == PROP_TYPE_SMOKE) {
    if (toClear->smoke != NULL) {
      toClear->smoke->prop = NULL;
    }
    clear_plain_prop(toClear, TRUE);
  } else {
    clear_plain_prop(toClear, TRUE);
  }
}

static s16 get_prop_index_for_object(ObjectRecord *obj) {
  s32 i;

  if (obj == NULL) {
    return -1;
  }

  // TODO: Is there a faster way to check than looping over all prop entries?
  for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
    PropRecord *prop = get_enabled_prop_by_index(i);
    if (prop != NULL &&
        // TODO: Can we remove this type check?
        (prop->type == PROP_TYPE_OBJ || prop->type == PROP_TYPE_DOOR ||
         prop->type == PROP_TYPE_WEAPON) &&
        prop->obj == obj) {
      return i;
    }
  }

  return -1;
}

static void save_projectile(StateStream *stream, Projectile *proj) {
  write_u32(stream, proj->flags);
  write_bytes(stream, &proj->speed, sizeof(coord3d));
  write_bytes(stream, &proj->unk10, sizeof(coord3d));
  write_f32(stream, proj->unk1C);
  write_bytes(stream, &proj->mtx, sizeof(Mtxf));
  write_f32(stream, proj->unk60);
  write_f32(stream, proj->unk64);
  write_bytes(stream, proj->unk68, sizeof(proj->unk68));
  write_bytes(stream, proj->unk78, sizeof(proj->unk78));
  write_u16(stream, get_prop_index(proj->ownerprop));
  write_f32(stream, proj->unk8C);
  write_u32(stream, proj->unk90);
  write_f32(stream, proj->unk94);
  write_u32(stream, proj->unkA0);
  write_u32(stream, proj->unkA4);
  write_u32(stream, proj->unkA8);
  write_u32(stream, proj->unkAC);
  write_f32(stream, proj->unkB0);
  write_f32(stream, proj->unkB4);
  write_u32(stream, proj->droptype);
  write_u32(stream, proj->refreshrate);
  write_f32(stream, proj->unkC0);
  write_f32(stream, proj->unkC4);
  write_f32(stream, proj->unkC8);
  write_u8(stream, proj->unkCC);
  write_u8(stream, proj->unkCD);
  write_u8(stream, proj->unkCE);
  write_u8(stream, proj->unkCF);
  write_u32(stream, proj->unkD0);
  write_f32(stream, proj->unkd4);
  write_u32(stream, proj->unkD8);
  write_u32(stream, proj->unkDC);
  write_u32(stream, proj->unkE0);
  write_u16(stream, get_prop_index_for_object(proj->obj));
  write_u32(stream, proj->unkE8);
}

static void load_projectile(StateStream *stream, Projectile *proj,
                            s16 *ownerprop_idx, s16 *obj_idx) {
  proj->flags = read_u32(stream);
  read_bytes(stream, &proj->speed, sizeof(coord3d));
  read_bytes(stream, &proj->unk10, sizeof(coord3d));
  proj->unk1C = read_f32(stream);
  read_bytes(stream, &proj->mtx, sizeof(Mtxf));
  proj->unk60 = read_f32(stream);
  proj->unk64 = read_f32(stream);
  read_bytes(stream, proj->unk68, sizeof(proj->unk68));
  read_bytes(stream, proj->unk78, sizeof(proj->unk78));
  *ownerprop_idx = (s16)read_u16(stream);
  proj->ownerprop = NULL;
  proj->unk8C = read_f32(stream);
  proj->unk90 = read_u32(stream);
  proj->unk94 = read_f32(stream);
  proj->sound1 = NULL;
  proj->sound2 = NULL;
  proj->unkA0 = read_u32(stream);
  proj->unkA4 = read_u32(stream);
  proj->unkA8 = read_u32(stream);
  proj->unkAC = read_u32(stream);
  proj->unkB0 = read_f32(stream);
  proj->unkB4 = read_f32(stream);
  proj->droptype = read_u32(stream);
  proj->refreshrate = read_u32(stream);
  proj->unkC0 = read_f32(stream);
  proj->unkC4 = read_f32(stream);
  proj->unkC8 = read_f32(stream);
  proj->unkCC = read_u8(stream);
  proj->unkCD = read_u8(stream);
  proj->unkCE = read_u8(stream);
  proj->unkCF = read_u8(stream);
  proj->unkD0 = read_u32(stream);
  proj->unkd4 = read_f32(stream);
  proj->unkD8 = read_u32(stream);
  proj->unkDC = read_u32(stream);
  proj->unkE0 = read_u32(stream);
  *obj_idx = (s16)read_u16(stream);
  proj->obj = NULL;
  proj->unkE8 = read_u32(stream);
}

static void save_embedment(StateStream *stream, Embedment *emb) {
  write_u32(stream, emb->flags);
  write_bytes(stream, &emb->matrix, sizeof(Mtxf));
  write_u16(stream,
            emb->projectile ? (u16)(emb->projectile - g_Projectiles) : -1);
}

static void load_embedment(StateStream *stream, Embedment *emb) {
  emb->flags = read_u32(stream);
  read_bytes(stream, &emb->matrix, sizeof(Mtxf));
  {
    s16 proj_idx = (s16)read_u16(stream);
    emb->projectile = (proj_idx != -1) ? &g_Projectiles[proj_idx] : NULL;
  }
}

static void save_object_base(StateStream *stream, ObjectRecord *obj) {
  s16 projIdx = -1;
  s16 embIdx = -1;
  u32 switchStates = 0;

  write_u16(stream, obj->extrascale);
  write_u8(stream, obj->state);
  write_u8(stream, obj->type);
  write_u16(stream, obj->obj);
  write_u16(stream, obj->pad);
  write_u32(stream, obj->flags);
  write_u32(stream, obj->flags2);
  write_bytes(stream, &obj->mtx, sizeof(Mtxf));
  write_bytes(stream, &obj->runtime_pos, sizeof(coord3d));
  write_u32(stream, obj->runtime_bitflags);
  write_f32(stream, obj->maxdamage);
  write_f32(stream, obj->damage);
  write_u32(stream, *(u32 *)&obj->shadecol);
  write_u32(stream, *(u32 *)&obj->nextcol);

  if ((obj->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT) &&
      obj->projectile != NULL) {
    projIdx = (s16)(obj->projectile - g_Projectiles);
  }
  write_u16(stream, projIdx);

  if ((obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) &&
      obj->embedment != NULL) {
    embIdx = (s16)(obj->embedment - g_Embedments);
  }
  write_u16(stream, embIdx);

  if (obj->model != NULL && obj->model->obj != NULL) {
    s32 i;
    s32 numSw = obj->model->obj->numSwitches;
    if (numSw > 32) {
      numSw = 32;
    }
    for (i = 0; i < numSw; i++) {
      ModelNode *switchNode = obj->model->obj->Switches[i];
      if (switchNode != NULL &&
          (switchNode->Opcode & 0xFF) == MODELNODE_OPCODE_SWITCH) {
        union ModelRwData *rwdata = modelGetNodeRwData(obj->model, switchNode);
        if (rwdata != NULL && rwdata->Switch.visible) {
          switchStates |= (1 << i);
        }
      }
    }
  }
  write_u32(stream, switchStates);
}

static bool load_object_base(StateStream *stream, ObjectRecord *obj,
                             PropRecord *prop) {
  PropDefHeaderRecord *pdhr = (PropDefHeaderRecord *)obj;
  s32 destroyedLvl;
  s16 projectileIdx;
  s16 embedmentIdx;
  u32 switchStates;

  pdhr->extrascale = read_u16(stream);
  pdhr->state = read_u8(stream);
  pdhr->type = read_u8(stream);
  obj->obj = read_u16(stream);
  obj->pad = read_u16(stream);
  obj->flags = read_u32(stream);
  obj->flags2 = read_u32(stream);
  read_bytes(stream, &obj->mtx, sizeof(Mtxf));
  read_bytes(stream, &obj->runtime_pos, sizeof(coord3d));
  obj->runtime_bitflags = read_u32(stream);
  obj->maxdamage = read_f32(stream);
  obj->damage = read_f32(stream);
  *(u32 *)&obj->shadecol = read_u32(stream);
  *(u32 *)&obj->nextcol = read_u32(stream);

  projectileIdx = (s16)read_u16(stream);
  embedmentIdx = (s16)read_u16(stream);
  switchStates = read_u32(stream);

  // Projectile and embedment occupy the same union slot. Restore only the
  // member selected by the runtime flags; writing both would overwrite the
  // first pointer with the second.
  if (prop != NULL && (obj->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT) &&
      projectileIdx >= 0 && projectileIdx < PROJECTILES_ARR_MAX) {
    obj->projectile = &g_Projectiles[projectileIdx];
    obj->projectile->obj = obj;
  } else if (prop != NULL &&
             (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED) &&
             embedmentIdx >= 0 && embedmentIdx < EMBEDMENT_ARR_MAX) {
    obj->embedment = &g_Embedments[embedmentIdx];
  } else {
    obj->projectile = NULL;
    obj->runtime_bitflags &=
        ~(RUNTIMEBITFLAG_DEPOSIT | RUNTIMEBITFLAG_EMBEDDED);
  }

  if (prop != NULL && obj->model != NULL && obj->model->obj != NULL) {
    s32 i;
    s32 numSw = obj->model->obj->numSwitches;
    if (numSw > 32) {
      numSw = 32;
    }
    for (i = 0; i < numSw; i++) {
      ModelNode *switchNode = obj->model->obj->Switches[i];
      if (switchNode != NULL &&
          (switchNode->Opcode & 0xFF) == MODELNODE_OPCODE_SWITCH) {
        union ModelRwData *rwdata = modelGetNodeRwData(obj->model, switchNode);
        if (rwdata != NULL) {
          rwdata->Switch.visible = (switchStates & (1 << i)) != 0;
        }
      }
    }
    sub_GAME_7F050DE8(obj->model);
  }

  if (prop != NULL) {
    destroyedLvl = objGetDestroyedLevel(obj);
    if (destroyedLvl > 0) {
      objDeform(obj, destroyedLvl);
    }
  }

  return TRUE;
}

static void save_monitor_record(StateStream *stream,
                                struct MonitorRecord *mon) {
  write_u32(stream, (u32)mon->cmdlist);
  write_u16(stream, mon->offset);
  write_u16(stream, mon->pause60);
  write_u32(stream, (u32)mon->tconfig);
  write_f32(stream, mon->rot);
  write_f32(stream, mon->xscale);
  write_f32(stream, mon->xscalefrac);
  write_f32(stream, mon->xscaleinc);
  write_f32(stream, mon->xscaleold);
  write_f32(stream, mon->xscalenew);
  write_f32(stream, mon->yscale);
  write_f32(stream, mon->yscalefrac);
  write_f32(stream, mon->yscaleinc);
  write_f32(stream, mon->yscaleold);
  write_f32(stream, mon->yscalenew);
  write_f32(stream, mon->xmid);
  write_f32(stream, mon->xmidfrac);
  write_f32(stream, mon->xmidinc);
  write_f32(stream, mon->xmidold);
  write_f32(stream, mon->xmidnew);
  write_f32(stream, mon->ymid);
  write_f32(stream, mon->ymidfrac);
  write_f32(stream, mon->ymidinc);
  write_f32(stream, mon->ymidold);
  write_f32(stream, mon->ymidnew);
  write_u8(stream, mon->red);
  write_u8(stream, mon->redold);
  write_u8(stream, mon->rednew);
  write_u8(stream, mon->green);
  write_u8(stream, mon->greenold);
  write_u8(stream, mon->greennew);
  write_u8(stream, mon->blue);
  write_u8(stream, mon->blueold);
  write_u8(stream, mon->bluenew);
  write_u8(stream, mon->alpha);
  write_u8(stream, mon->alphaold);
  write_u8(stream, mon->alphanew);
  write_f32(stream, mon->colfrac);
  write_f32(stream, mon->colinc);
}

static void load_monitor_record(StateStream *stream,
                                struct MonitorRecord *mon) {
  mon->cmdlist = (u32 *)read_u32(stream);
  mon->offset = read_u16(stream);
  mon->pause60 = read_u16(stream);
  mon->tconfig = (void *)read_u32(stream);
  mon->rot = read_f32(stream);
  mon->xscale = read_f32(stream);
  mon->xscalefrac = read_f32(stream);
  mon->xscaleinc = read_f32(stream);
  mon->xscaleold = read_f32(stream);
  mon->xscalenew = read_f32(stream);
  mon->yscale = read_f32(stream);
  mon->yscalefrac = read_f32(stream);
  mon->yscaleinc = read_f32(stream);
  mon->yscaleold = read_f32(stream);
  mon->yscalenew = read_f32(stream);
  mon->xmid = read_f32(stream);
  mon->xmidfrac = read_f32(stream);
  mon->xmidinc = read_f32(stream);
  mon->xmidold = read_f32(stream);
  mon->xmidnew = read_f32(stream);
  mon->ymid = read_f32(stream);
  mon->ymidfrac = read_f32(stream);
  mon->ymidinc = read_f32(stream);
  mon->ymidold = read_f32(stream);
  mon->ymidnew = read_f32(stream);
  mon->red = read_u8(stream);
  mon->redold = read_u8(stream);
  mon->rednew = read_u8(stream);
  mon->green = read_u8(stream);
  mon->greenold = read_u8(stream);
  mon->greennew = read_u8(stream);
  mon->blue = read_u8(stream);
  mon->blueold = read_u8(stream);
  mon->bluenew = read_u8(stream);
  mon->alpha = read_u8(stream);
  mon->alphaold = read_u8(stream);
  mon->alphanew = read_u8(stream);
  mon->colfrac = read_f32(stream);
  mon->colinc = read_f32(stream);
}

static void load_door_record(StateStream *stream, DoorRecord *door) {
  door->maxFrac = read_f32(stream);
  door->perimFrac = read_f32(stream);
  door->accel = read_f32(stream);
  door->decel = read_f32(stream);
  door->maxSpeed = read_f32(stream);
  door->doorFlags = read_u16(stream);
  door->doorType = read_u16(stream);
  door->keyflags = read_u32(stream);
  door->autoCloseFrames = read_u32(stream);
  door->doorOpenSound = read_u32(stream);
  door->frac = read_f32(stream);
  door->unkac = read_f32(stream);
  door->unkb0 = read_f32(stream);
  door->openPosition = read_f32(stream);
  door->speed = read_f32(stream);
  door->openstate = read_u8(stream);
  door->unkbd = read_u8(stream);
  door->calculatedopacity = read_u16(stream);
  door->TintDist = read_u32(stream);
  door->CullDist = read_u16(stream);
  door->soundType = read_u8(stream);
  door->fadeTime60 = read_u8(stream);
  read_bytes(stream, &door->bbox, sizeof(struct ModelRoData_BoundingBoxRecord));
  door->openedTime = read_u32(stream);
  door->portalNumber = read_u32(stream);
  door->openSoundState = NULL;
  door->closeSoundState = NULL;
  door->lastcalc60i = read_u32(stream);
}

static void save_explosion_record(StateStream *stream,
                                  struct Explosion *explosion) {
  write_u16(stream, get_prop_index(explosion->source));
  write_bytes(stream, explosion->parts, sizeof(explosion->parts));
  write_u16(stream, explosion->age);
  write_u16(stream, explosion->unk3CA);
  write_u8(stream, explosion->explosion_type);
  write_u8(stream, explosion->unk3CD);
  write_u8(stream, explosion->player);
  write_u8(stream, explosion->unk3CF);
  write_bytes(stream, &explosion->pos, sizeof(explosion->pos));
  write_u16(stream, explosion->room);
  write_u16(stream, explosion->unk3DE);
}

static void load_explosion_record(StateStream *stream,
                                  struct Explosion *explosion) {
  s16 source_index = read_u16(stream);

  explosion->source = get_enabled_prop_by_index(source_index);
  read_bytes(stream, explosion->parts, sizeof(explosion->parts));
  explosion->age = read_u16(stream);
  explosion->unk3CA = read_u16(stream);
  explosion->explosion_type = read_u8(stream);
  explosion->unk3CD = read_u8(stream);
  explosion->player = read_u8(stream);
  explosion->unk3CF = read_u8(stream);
  read_bytes(stream, &explosion->pos, sizeof(explosion->pos));
  explosion->room = read_u16(stream);
  explosion->unk3DE = read_u16(stream);
}

static void save_smoke_record(StateStream *stream, struct Smoke *smoke) {
  write_u16(stream, smoke->duration);
  write_u16(stream, smoke->smoke_type);
  write_bytes(stream, smoke->parts, sizeof(smoke->parts));
}

static void load_smoke_record(StateStream *stream, struct Smoke *smoke) {
  smoke->duration = read_u16(stream);
  smoke->smoke_type = read_u16(stream);
  read_bytes(stream, smoke->parts, sizeof(smoke->parts));
}

static void load_object_subtype(StateStream *stream, ObjectRecord *obj) {
  switch (obj->type) {
  case PROPDEF_PROP:
  case PROPDEF_ALARM:
  case PROPDEF_RACK:
  case PROPDEF_HAT:
  case PROPDEF_UNK41:
  case PROPDEF_GAS_RELEASING:
    break;
  case PROPDEF_KEY: {
    KeyRecord *key = (KeyRecord *)obj;
    key->keyflags = read_u32(stream);
    break;
  }
  case PROPDEF_CCTV: {
    CCTVRecord *cctv = (CCTVRecord *)obj;
    cctv->cctv_pad = read_u32(stream);
    read_bytes(stream, &cctv->unk84, sizeof(Mtxf));
    cctv->unkC4 = read_f32(stream);
    cctv->unkC8 = read_f32(stream);
    cctv->unkCC = read_f32(stream);
    cctv->unkD0 = read_f32(stream);
    cctv->unkD4 = read_u32(stream);
    cctv->unkD8 = read_f32(stream);
    cctv->unkDC = read_f32(stream);
    cctv->timer = read_u32(stream);
    cctv->convert_to_f32 = read_u32(stream);
    cctv->unkE8 = read_f32(stream);
    cctv->unkEC = read_u32(stream);
    cctv->unkF0 = read_u32(stream);
    cctv->unkF4 = read_u32(stream);
    cctv->unkF8 = read_u32(stream);
    break;
  }
  case PROPDEF_MAGAZINE: {
    AmmoCrateRecord *mag = (AmmoCrateRecord *)obj;
    mag->ammoType = read_u32(stream);
    break;
  }
  case PROPDEF_COLLECTABLE: {
    WeaponObjRecord *wpn = (WeaponObjRecord *)obj;
    PropRecord *dualProp;
    wpn->weaponnum = read_u8(stream);
    wpn->LinkedWeaponType = read_u8(stream);
    wpn->timer = read_u16(stream);
    s16 dualIdx = read_u16(stream);
    dualProp = get_enabled_prop_by_index(dualIdx);
    wpn->dualweapon = dualProp != NULL &&
                              (dualProp->type == PROP_TYPE_OBJ ||
                               dualProp->type == PROP_TYPE_WEAPON) &&
                              dualProp->obj != NULL &&
                              dualProp->obj->type == PROPDEF_COLLECTABLE
                          ? dualProp->weapon
                          : NULL;
    break;
  }
  case PROPDEF_MONITOR: {
    MonitorObjRecord *mon = (MonitorObjRecord *)obj;
    load_monitor_record(stream, &mon->Monitor);
    mon->OwnerOffset = read_u32(stream);
    mon->OwnerPart = read_u32(stream);
    mon->ImageNum = read_u32(stream);
    break;
  }
  case PROPDEF_MULTI_MONITOR: {
    MultiMonitorObjRecord *mmon = (MultiMonitorObjRecord *)obj;
    s32 m;
    for (m = 0; m < 4; m++) {
      load_monitor_record(stream, &mmon->Monitor[m]);
    }
    mmon->ImageNums[0] = read_u8(stream);
    mmon->ImageNums[1] = read_u8(stream);
    mmon->ImageNums[2] = read_u8(stream);
    mmon->ImageNums[3] = read_u8(stream);
    break;
  }
  case PROPDEF_AUTOGUN: {
    AutogunRecord *agun = (AutogunRecord *)obj;
    agun->padID = read_u32(stream);
    agun->rot_related = read_f32(stream);
    agun->unk88 = read_f32(stream);
    agun->unk8C = read_f32(stream);
    agun->unk90 = read_f32(stream);
    agun->unk94 = read_f32(stream);
    agun->unk98 = read_f32(stream);
    agun->unk9C = read_f32(stream);
    agun->unkA0 = read_f32(stream);
    agun->speed = read_f32(stream);
    agun->aimdist = read_f32(stream);
    agun->unkAC = read_u32(stream);
    agun->unkB0 = read_f32(stream);
    agun->unkB4 = read_f32(stream);
    agun->unkB8 = read_u32(stream);
    agun->unkBC = read_u32(stream);
    agun->unkC0 = read_u32(stream);

    struct beam b;
    read_bytes(stream, &b, sizeof(struct beam));
    agun->unkC4 = NULL;
    agun->unkC8 = NULL;
    if (agun->beam != NULL) {
      *agun->beam = b;
    }

    agun->is_active = read_u32(stream);
    agun->unkD4 = read_f32(stream);
    break;
  }
  case PROPDEF_AMMO: {
    MultiAmmoCrateRecord *ammo = (MultiAmmoCrateRecord *)obj;
    ammo->unk80 = read_u16(stream);
    s32 a;
    for (a = 0; a < AMMOTYPE_GLOBAL_MAX; a++) {
      ammo->quantities[a] = read_u16(stream);
    }
    break;
  }
  case PROPDEF_ARMOUR: {
    BodyArmourRecord *arm = (BodyArmourRecord *)obj;
    arm->initialamount = read_f32(stream);
    arm->amount = read_f32(stream);
    break;
  }
  case PROPDEF_VEHICHLE: {
    VehichleRecord *veh = (VehichleRecord *)obj;
    u32 ailistID = read_u32(stream);
    veh->ailist = (ailistID != 0) ? ailistFindById(ailistID) : NULL;
    veh->aioffset = read_u16(stream);
    veh->aireturnlist = read_u16(stream);
    veh->speed = read_f32(stream);
    veh->wheelxrot = read_f32(stream);
    veh->wheelyrot = read_f32(stream);
    veh->speedaim = read_f32(stream);
    veh->speedtime60 = read_f32(stream);
    veh->turnrot60 = read_f32(stream);
    veh->roty = read_f32(stream);
    u32 pathID = read_u32(stream);
    veh->path = (pathID != 0) ? pathFindById(pathID) : NULL;
    veh->nextstep = read_u32(stream);
    break;
  }
  case PROPDEF_AIRCRAFT: {
    AircraftRecord *air = (AircraftRecord *)obj;
    u32 ailistID = read_u32(stream);
    air->ailist = (ailistID != 0) ? ailistFindById(ailistID) : NULL;
    air->aioffset = read_u16(stream);
    air->aireturnlist = read_u16(stream);
    air->rotoryrot = read_f32(stream);
    air->rotaryspeed = read_f32(stream);
    air->rotaryspeedaim = read_f32(stream);
    air->rotaryspeedtime = read_f32(stream);
    air->speed = read_f32(stream);
    air->speedaim = read_f32(stream);
    air->speedtime60 = read_f32(stream);
    air->yrot = read_f32(stream);
    air->nextstep = read_u32(stream);
    u32 pathID = read_u32(stream);
    air->path = (pathID != 0) ? pathFindById(pathID) : NULL;
    break;
  }
  case PROPDEF_GLASS:
  case PROPDEF_SAFE: {
    GlassRecord *gl = (GlassRecord *)obj;
    read_bytes(stream, &gl->normal, sizeof(coord3d));
    break;
  }
  case PROPDEF_TINTED_GLASS: {
    TintedGlassRecord *tgl = (TintedGlassRecord *)obj;
    tgl->TintDist = read_u32(stream);
    tgl->CullDist = read_u32(stream);
    tgl->calculatedopacity = read_u32(stream);
    tgl->portalnum = read_u32(stream);
    tgl->unk90 = read_f32(stream);
    break;
  }
  case PROPDEF_TANK: {
    TankRecord *tank = (TankRecord *)obj;
    read_bytes(stream, &tank->rect, sizeof(rect4f));
    tank->unkA4 = read_u32(stream);
    tank->unkA8 = read_u32(stream);
    tank->unkAC = read_u32(stream);
    tank->unkB0 = read_u32(stream);
    tank->unkB4 = read_u32(stream);
    tank->unkB8 = read_u32(stream);
    tank->unkBC = read_u32(stream);
    tank->unkC0 = read_u32(stream);
    tank->is_firing_tank = read_u32(stream);
    tank->turret_vertical_angle = read_f32(stream);
    tank->turret_orientation_angle = read_f32(stream);
    tank->unkD0 = read_f32(stream);
    tank->stan_y = read_f32(stream);
    tank->unkD8 = read_u32(stream);
    tank->tank_orientation_angle = read_f32(stream);
    break;
  }
  default:
    break;
  }
}

static void skip_prop_data(StateStream *stream, u8 type) {
  if (type == PROP_TYPE_DOOR) {
    ObjectRecord temp_obj;
    DoorRecord temp_door;
    load_object_base(stream, &temp_obj, NULL);
    load_door_record(stream, &temp_door);
  } else if (type == PROP_TYPE_OBJ || type == PROP_TYPE_WEAPON) {
    TempObjectRecord temp_obj;
    if (load_object_base(stream, &temp_obj.base, NULL)) {
      load_object_subtype(stream, &temp_obj.base);
    }
  } else if (type == PROP_TYPE_EXPLOSION) {
    struct Explosion temp_explosion;
    load_explosion_record(stream, &temp_explosion);
  } else if (type == PROP_TYPE_SMOKE) {
    struct Smoke temp_smoke;
    load_smoke_record(stream, &temp_smoke);
  } else if (type == PROP_TYPE_CHR) {
    ChrRecord temp_chr;
    temp_chr.hidden = 0;
    temp_chr.model = NULL;
    load_chr_record(stream, &temp_chr, NULL);
  }
}

bool save_props_state(StateStream *stream) {
  u32 headerOffset;
  u32 dataStart;
  u16 recordCount = 0;
  s32 i;
  u32 pi;

  headerOffset = stream->base_address + stream->total_processed;

  // Placeholder header fields
  write_u32(stream, 0); // size
  write_u16(stream, 0); // recordCount
  write_u16(stream, get_prop_index(ptr_obj_pos_list_first_entry));
  write_u16(stream, get_prop_index(ptr_obj_pos_list_current_entry));
  write_u16(stream, get_prop_index(ptr_obj_pos_list_final_entry));

  dataStart = stream->base_address + stream->total_processed;

  write_u32(stream, g_NumExplosionEntries);
  write_u32(stream, g_NumSmokeEntries);

  /* Save active projectiles. */
  {
    u16 activeProjCount = 0;
    for (pi = 0; pi < PROJECTILES_ARR_MAX; pi++) {
      if (!(g_Projectiles[pi].flags & PROJECTILEFLAG_FREE)) {
        activeProjCount++;
      }
    }
    write_u16(stream, activeProjCount);
    for (pi = 0; pi < PROJECTILES_ARR_MAX; pi++) {
      if (!(g_Projectiles[pi].flags & PROJECTILEFLAG_FREE)) {
        write_u16(stream, pi);
        save_projectile(stream, &g_Projectiles[pi]);
      }
    }
  }

  /* Save active embedments. */
  {
    u16 activeEmbCount = 0;
    for (pi = 0; pi < EMBEDMENT_ARR_MAX; pi++) {
      if (!(g_Embedments[pi].flags & EMBEDMENTFLAG_FREE)) {
        activeEmbCount++;
      }
    }
    write_u16(stream, activeEmbCount);
    for (pi = 0; pi < EMBEDMENT_ARR_MAX; pi++) {
      if (!(g_Embedments[pi].flags & EMBEDMENTFLAG_FREE)) {
        write_u16(stream, pi);
        save_embedment(stream, &g_Embedments[pi]);
      }
    }
  }

  for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
    PropRecord *prop = get_prop_by_index(i);

    if (prop == NULL || !(prop->flags & PROPFLAG_ENABLED)) {
      continue;
    }

    // Write base prop record fields
    write_u16(stream, i);
    write_u8(stream, prop->type);
    write_u8(stream, prop->flags);
    write_u16(stream, prop->timetoregen);
    write_bytes(stream, &prop->pos, sizeof(coord3d));
    write_u32(stream, get_tile_offset(prop->stan));
    write_f32(stream, prop->zDepth);
    write_u16(stream, get_prop_index(prop->parent));
    write_u16(stream, get_prop_index(prop->child));
    write_u16(stream, get_prop_index(prop->prev));
    write_u16(stream, get_prop_index(prop->next));
    write_u8(stream, prop->rooms[0]);
    write_u8(stream, prop->rooms[1]);
    write_u8(stream, prop->rooms[2]);
    write_u8(stream, prop->rooms[3]);
    write_u32(stream, prop->unk30);

    switch ((PROP_TYPE)prop->type) {
    case PROP_TYPE_DOOR: {
      DoorRecord *door = prop->door;
      if (door != NULL) {
        save_object_base(stream, (ObjectRecord *)door);

        write_f32(stream, door->maxFrac);
        write_f32(stream, door->perimFrac);
        write_f32(stream, door->accel);
        write_f32(stream, door->decel);
        write_f32(stream, door->maxSpeed);
        write_u16(stream, door->doorFlags);
        write_u16(stream, door->doorType);
        write_u32(stream, door->keyflags);
        write_u32(stream, door->autoCloseFrames);
        write_u32(stream, door->doorOpenSound);
        write_f32(stream, door->frac);
        write_f32(stream, door->unkac);
        write_f32(stream, door->unkb0);
        write_f32(stream, door->openPosition);
        write_f32(stream, door->speed);
        write_u8(stream, door->openstate);
        write_u8(stream, door->unkbd);
        write_u16(stream, door->calculatedopacity);
        write_u32(stream, door->TintDist);
        write_u16(stream, door->CullDist);
        write_u8(stream, door->soundType);
        write_u8(stream, door->fadeTime60);
        write_bytes(stream, &door->bbox,
                    sizeof(struct ModelRoData_BoundingBoxRecord));
        write_u32(stream, door->openedTime);
        write_u32(stream, door->portalNumber);
        write_u32(stream, door->lastcalc60i);
      }
      break;
    }

    case PROP_TYPE_OBJ:
    case PROP_TYPE_WEAPON: {
      ObjectRecord *obj = prop->obj;
      if (obj != NULL) {
        save_object_base(stream, obj);

        switch (obj->type) {
        case PROPDEF_PROP:
        case PROPDEF_ALARM:
        case PROPDEF_RACK:
        case PROPDEF_HAT:
        case PROPDEF_UNK41:
        case PROPDEF_GAS_RELEASING:
          break;
        case PROPDEF_KEY: {
          KeyRecord *key = (KeyRecord *)obj;
          write_u32(stream, key->keyflags);
          break;
        }
        case PROPDEF_CCTV: {
          CCTVRecord *cctv = (CCTVRecord *)obj;
          write_u32(stream, cctv->cctv_pad);
          write_bytes(stream, &cctv->unk84, sizeof(Mtxf));
          write_f32(stream, cctv->unkC4);
          write_f32(stream, cctv->unkC8);
          write_f32(stream, cctv->unkCC);
          write_f32(stream, cctv->unkD0);
          write_u32(stream, cctv->unkD4);
          write_f32(stream, cctv->unkD8);
          write_f32(stream, cctv->unkDC);
          write_u32(stream, cctv->timer);
          write_u32(stream, cctv->convert_to_f32);
          write_f32(stream, cctv->unkE8);
          write_u32(stream, cctv->unkEC);
          write_u32(stream, cctv->unkF0);
          write_u32(stream, cctv->unkF4);
          write_u32(stream, cctv->unkF8);
          break;
        }
        case PROPDEF_MAGAZINE: {
          AmmoCrateRecord *mag = (AmmoCrateRecord *)obj;
          write_u32(stream, mag->ammoType);
          break;
        }
        case PROPDEF_COLLECTABLE: {
          WeaponObjRecord *wpn = (WeaponObjRecord *)obj;
          write_u8(stream, wpn->weaponnum);
          write_u8(stream, wpn->LinkedWeaponType);
          write_u16(stream, wpn->timer);
          s16 dualIdx = (wpn->dualweapon && wpn->dualweapon->prop)
                            ? (s16)get_prop_index(wpn->dualweapon->prop)
                            : -1;
          write_u16(stream, dualIdx);
          break;
        }
        case PROPDEF_MONITOR: {
          MonitorObjRecord *mon = (MonitorObjRecord *)obj;
          save_monitor_record(stream, &mon->Monitor);
          write_u32(stream, mon->OwnerOffset);
          write_u32(stream, mon->OwnerPart);
          write_u32(stream, mon->ImageNum);
          break;
        }
        case PROPDEF_MULTI_MONITOR: {
          MultiMonitorObjRecord *mmon = (MultiMonitorObjRecord *)obj;
          s32 m;
          for (m = 0; m < 4; m++) {
            save_monitor_record(stream, &mmon->Monitor[m]);
          }
          write_u8(stream, mmon->ImageNums[0]);
          write_u8(stream, mmon->ImageNums[1]);
          write_u8(stream, mmon->ImageNums[2]);
          write_u8(stream, mmon->ImageNums[3]);
          break;
        }
        case PROPDEF_AUTOGUN: {
          AutogunRecord *agun = (AutogunRecord *)obj;
          write_u32(stream, agun->padID);
          write_f32(stream, agun->rot_related);
          write_f32(stream, agun->unk88);
          write_f32(stream, agun->unk8C);
          write_f32(stream, agun->unk90);
          write_f32(stream, agun->unk94);
          write_f32(stream, agun->unk98);
          write_f32(stream, agun->unk9C);
          write_f32(stream, agun->unkA0);
          write_f32(stream, agun->speed);
          write_f32(stream, agun->aimdist);
          write_u32(stream, agun->unkAC);
          write_f32(stream, agun->unkB0);
          write_f32(stream, agun->unkB4);
          write_u32(stream, agun->unkB8);
          write_u32(stream, agun->unkBC);
          write_u32(stream, agun->unkC0);

          struct beam b;
          if (agun->beam != NULL) {
            b = *agun->beam;
          } else {
            b.age = -1;
            b.weaponnum = 0;
            b.from.x = 0.0f;
            b.from.y = 0.0f;
            b.from.z = 0.0f;
            b.dir.x = 0.0f;
            b.dir.y = 0.0f;
            b.dir.z = 0.0f;
            b.maxdist = 0.0f;
            b.speed = 0.0f;
            b.mindist = 0.0f;
            b.dist = 0.0f;
          }
          write_bytes(stream, &b, sizeof(struct beam));

          write_u32(stream, agun->is_active);
          write_f32(stream, agun->unkD4);
          break;
        }
        case PROPDEF_AMMO: {
          MultiAmmoCrateRecord *ammo = (MultiAmmoCrateRecord *)obj;
          write_u16(stream, ammo->unk80);
          s32 a;
          for (a = 0; a < AMMOTYPE_GLOBAL_MAX; a++) {
            write_u16(stream, ammo->quantities[a]);
          }
          break;
        }
        case PROPDEF_ARMOUR: {
          BodyArmourRecord *arm = (BodyArmourRecord *)obj;
          write_f32(stream, arm->initialamount);
          write_f32(stream, arm->amount);
          break;
        }
        case PROPDEF_VEHICHLE: {
          VehichleRecord *veh = (VehichleRecord *)obj;
          bool isGlobal;
          write_u32(stream,
                    veh->ailist ? chraiGetAIListID(veh->ailist, &isGlobal) : 0);
          write_u16(stream, veh->aioffset);
          write_u16(stream, veh->aireturnlist);
          write_f32(stream, veh->speed);
          write_f32(stream, veh->wheelxrot);
          write_f32(stream, veh->wheelyrot);
          write_f32(stream, veh->speedaim);
          write_f32(stream, veh->speedtime60);
          write_f32(stream, veh->turnrot60);
          write_f32(stream, veh->roty);
          write_u32(stream, veh->path ? veh->path->ID : 0);
          write_u32(stream, veh->nextstep);
          break;
        }
        case PROPDEF_AIRCRAFT: {
          AircraftRecord *air = (AircraftRecord *)obj;
          bool isGlobal;
          write_u32(stream,
                    air->ailist ? chraiGetAIListID(air->ailist, &isGlobal) : 0);
          write_u16(stream, air->aioffset);
          write_u16(stream, air->aireturnlist);
          write_f32(stream, air->rotoryrot);
          write_f32(stream, air->rotaryspeed);
          write_f32(stream, air->rotaryspeedaim);
          write_f32(stream, air->rotaryspeedtime);
          write_f32(stream, air->speed);
          write_f32(stream, air->speedaim);
          write_f32(stream, air->speedtime60);
          write_f32(stream, air->yrot);
          write_u32(stream, air->nextstep);
          write_u32(stream, air->path ? air->path->ID : 0);
          break;
        }
        case PROPDEF_GLASS:
        case PROPDEF_SAFE: {
          GlassRecord *gl = (GlassRecord *)obj;
          write_bytes(stream, &gl->normal, sizeof(coord3d));
          break;
        }
        case PROPDEF_TINTED_GLASS: {
          TintedGlassRecord *tgl = (TintedGlassRecord *)obj;
          write_u32(stream, tgl->TintDist);
          write_u32(stream, tgl->CullDist);
          write_u32(stream, tgl->calculatedopacity);
          write_u32(stream, tgl->portalnum);
          write_f32(stream, tgl->unk90);
          break;
        }
        case PROPDEF_TANK: {
          TankRecord *tank = (TankRecord *)obj;
          write_bytes(stream, &tank->rect, sizeof(rect4f));
          write_u32(stream, tank->unkA4);
          write_u32(stream, tank->unkA8);
          write_u32(stream, tank->unkAC);
          write_u32(stream, tank->unkB0);
          write_u32(stream, tank->unkB4);
          write_u32(stream, tank->unkB8);
          write_u32(stream, tank->unkBC);
          write_u32(stream, tank->unkC0);
          write_u32(stream, tank->is_firing_tank);
          write_f32(stream, tank->turret_vertical_angle);
          write_f32(stream, tank->turret_orientation_angle);
          write_f32(stream, tank->unkD0);
          write_f32(stream, tank->stan_y);
          write_u32(stream, tank->unkD8);
          write_f32(stream, tank->tank_orientation_angle);
          break;
        }
        default:
          break;
        }
      }
      break;
    }

    case PROP_TYPE_SMOKE:
      if (prop->smoke != NULL) {
        save_smoke_record(stream, prop->smoke);
      }
      break;

    case PROP_TYPE_EXPLOSION:
      if (prop->explosion != NULL) {
        save_explosion_record(stream, prop->explosion);
      }
      break;

    case PROP_TYPE_NUL:
      practiceLogDebug(
          "INVARIANT VIOLATION: Enabled PROP_TYPE_NUL at prop index %d", i);
      break;
    case PROP_TYPE_CHR:
      if (prop->chr == NULL) {
        practiceLogWarn("Enabled CHR prop at index %d has no ChrRecord", i);
        return FALSE;
      }
      save_chr_record(stream, prop->chr);
      break;
    case PROP_TYPE_PLAYER:
      practiceLogDebug(
          "INVARIANT VIOLATION: Enabled PROP_TYPE_PLAYER at prop index %d", i);
      break;
    case PROP_TYPE_VIEWER:
    default:
      break;
    }

    recordCount++;
  }

  // Player payloads follow all prop records so they can safely refer to any
  // restored prop, regardless of prop array order.
  if (!save_viewer_players_state(stream)) {
    return FALSE;
  }

  /* Patch the props header with the real size and record count. */
  u32 totalPropsSize = stream->total_processed - dataStart;
  stream_seek(stream, headerOffset);
  write_u32(stream, totalPropsSize);
  write_u16(stream, recordCount);
  write_u16(stream, get_prop_index(ptr_obj_pos_list_first_entry));
  write_u16(stream, get_prop_index(ptr_obj_pos_list_current_entry));
  write_u16(stream, get_prop_index(ptr_obj_pos_list_final_entry));

  // Resume stream offset at the end
  stream_seek(stream, dataStart + totalPropsSize);

  return TRUE;
}

bool load_props_state(StateStream *stream) {
  u32 dataStart;
  s32 i;
  u16 nextIndexToRemove = 0;
  s32 c;
  u32 pi;
  s16 projectileOwnerPropIndices[PROJECTILES_ARR_MAX];
  s16 projectileObjPropIndices[PROJECTILES_ARR_MAX];
  ChrAttachmentIndices pendingChrAttachments[POS_DATA_ENTRY_LEN];
  bool pendingChrAttachmentsValid[POS_DATA_ENTRY_LEN];

  for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
    pendingChrAttachmentsValid[i] = FALSE;
  }

  u32 totalPropsSize = read_u32(stream);
  u16 recordCount = read_u16(stream);
  s16 indexOfFirstEntry = read_u16(stream);
  s16 indexOfCurrentEntry = read_u16(stream);
  s16 indexOfFinalEntry = read_u16(stream);

  dataStart = stream->base_address + stream->total_processed;

  g_NumExplosionEntries = read_u32(stream);
  g_NumSmokeEntries = read_u32(stream);

  /* Clear and restore active projectiles. */
  for (pi = 0; pi < PROJECTILES_ARR_MAX; pi++) {
    projectileOwnerPropIndices[pi] = -1;
    projectileObjPropIndices[pi] = -1;
    if (!(g_Projectiles[pi].flags & PROJECTILEFLAG_FREE)) {
      projectileFree(&g_Projectiles[pi]);
    }
  }

  {
    u16 activeProjCount = read_u16(stream);
    for (pi = 0; pi < activeProjCount; pi++) {
      u16 index = read_u16(stream);
      load_projectile(stream, &g_Projectiles[index],
                      &projectileOwnerPropIndices[index],
                      &projectileObjPropIndices[index]);
    }
  }

  /* Clear and restore active embedments. */
  for (pi = 0; pi < EMBEDMENT_ARR_MAX; pi++) {
    if (!(g_Embedments[pi].flags & EMBEDMENTFLAG_FREE)) {
      embedmentFree(&g_Embedments[pi]);
    }
  }

  {
    u16 activeEmbCount = read_u16(stream);
    for (pi = 0; pi < activeEmbCount; pi++) {
      u16 index = read_u16(stream);
      load_embedment(stream, &g_Embedments[index]);
    }
  }

  for (i = 0; i < recordCount; i++) {
    u16 savedPropIndex = read_u16(stream);
    u8 savedPropType = read_u8(stream);
    u8 savedPropFlags = read_u8(stream);
    s16 savedPropTimetoregen = read_u16(stream);
    coord3d savedPropPos;
    read_bytes(stream, &savedPropPos, sizeof(coord3d));
    s32 savedPropStanOffset = read_u32(stream);
    f32 savedPropZDepth = read_f32(stream);
    u16 savedPropParentIdx = read_u16(stream);
    u16 savedPropChildIdx = read_u16(stream);
    u16 savedPropPrevIdx = read_u16(stream);
    u16 savedPropNextIdx = read_u16(stream);
    u8 savedPropRooms[4];
    savedPropRooms[0] = read_u8(stream);
    savedPropRooms[1] = read_u8(stream);
    savedPropRooms[2] = read_u8(stream);
    savedPropRooms[3] = read_u8(stream);
    s32 savedPropUnk30 = read_u32(stream);
    ChrAllocationState savedChrAllocation;
    bool hasChrAllocation = savedPropType == PROP_TYPE_CHR;
    bool createdChrProp = FALSE;

    if (hasChrAllocation) {
      load_chr_allocation_state(stream, &savedChrAllocation);
    }

    PropRecord *prop = get_prop_by_index(savedPropIndex);

    if (ADD_AND_REMOVE_PROPS) {
      for (c = nextIndexToRemove; c < savedPropIndex; c++) {
        removePropAtIndex(c);
      }
      nextIndexToRemove = savedPropIndex + 1;

      if (savedPropType == PROP_TYPE_CHR) {
        // Rebuild CHRs even when the array slot is still occupied. This avoids
        // retaining stale ChrRecord/Model allocations from the current world.
        if (prop->type == PROP_TYPE_CHR &&
            (prop->chr != NULL || (prop->flags & PROPFLAG_ENABLED))) {
          destroy_chr_prop(prop, FALSE);
        } else if (!clear_prop_for_replacement(prop)) {
          practiceLogWarn(
              "Cannot retain prop slot %d while replacing type %d with CHR",
              savedPropIndex, prop->type);
          skip_prop_data(stream, savedPropType);
          return FALSE;
        }

        prop = create_chr_prop(prop, &savedChrAllocation, &savedPropPos,
                               savedPropStanOffset);
        if (prop == NULL) {
          practiceLogWarn("Could not recreate CHR prop at index %d",
                          savedPropIndex);
          skip_prop_data(stream, savedPropType);
          return FALSE;
        }
        createdChrProp = TRUE;
      }
    }

    /* Only modify props which still exist while testing. */
    if (!ADD_AND_REMOVE_PROPS &&
        (prop == NULL || !(prop->flags & PROPFLAG_ENABLED) ||
         prop->type != savedPropType)) {
      skip_prop_data(stream, savedPropType);
      continue;
    }

    bool supportedType = FALSE;
    switch ((PROP_TYPE)savedPropType) {
    case PROP_TYPE_DOOR:
      supportedType = TRUE;
      break;
    case PROP_TYPE_OBJ:
      if (prop->obj != NULL) {
        switch (prop->obj->type) {
        case PROPDEF_PROP:
        case PROPDEF_ALARM:
        case PROPDEF_RACK:
        case PROPDEF_HAT:
        case PROPDEF_UNK41:
        case PROPDEF_GAS_RELEASING:
        case PROPDEF_KEY:
        case PROPDEF_CCTV:
        case PROPDEF_MAGAZINE:
        case PROPDEF_COLLECTABLE:
        case PROPDEF_MONITOR:
        case PROPDEF_MULTI_MONITOR:
        case PROPDEF_AUTOGUN:
        case PROPDEF_AMMO:
        case PROPDEF_ARMOUR:
        case PROPDEF_VEHICHLE:
        case PROPDEF_AIRCRAFT:
        case PROPDEF_GLASS:
        case PROPDEF_SAFE:
        case PROPDEF_TINTED_GLASS:
        case PROPDEF_TANK:
          supportedType = TRUE;
          break;
        }
      }
      break;
    case PROP_TYPE_WEAPON:
      supportedType =
          prop->weapon != NULL && prop->weapon->type == PROPDEF_COLLECTABLE;
      break;
    case PROP_TYPE_SMOKE:
      supportedType = prop->smoke != NULL && prop->smoke->prop == prop;
      break;
    case PROP_TYPE_EXPLOSION:
      supportedType = prop->explosion != NULL && prop->explosion->prop == prop;
      break;
    case PROP_TYPE_VIEWER:
      supportedType = TRUE;
      break;
    case PROP_TYPE_CHR:
      // The CHR module restores the spatial subset of the base PropRecord
      // together with its model, collision, and movement history. Other
      // common PropRecord fields remain unchanged for now.
      supportedType = FALSE;
      break;
    case PROP_TYPE_NUL:
    case PROP_TYPE_PLAYER:
    case PROP_TYPE_MAX:
      supportedType = FALSE;
      break;
    }

    if (ADD_AND_REMOVE_PROPS || supportedType) {
      if (savedPropType == PROP_TYPE_VIEWER) {
        chrpropDeregisterRooms(prop);
      }

      // A weapon or hat which was on the ground after the save may currently
      // be registered in rooms. Remove that registration before replacing its
      // room bytes with the saved attached state.
      if (!ADD_AND_REMOVE_PROPS &&
          (savedPropType == PROP_TYPE_OBJ ||
           savedPropType == PROP_TYPE_WEAPON) &&
          prop->parent == NULL) {
        PropRecord *saved_parent = get_prop_by_index(savedPropParentIdx);
        if (saved_parent != NULL && saved_parent->type == PROP_TYPE_CHR) {
          chrpropDeregisterRooms(prop);
        }
      }

      prop->type = savedPropType;
      prop->flags = savedPropFlags;
      prop->timetoregen = savedPropTimetoregen;
      prop->pos = savedPropPos;
      prop->stan = get_tile_by_offset(savedPropStanOffset);
      prop->zDepth = savedPropZDepth;
      if (ADD_AND_REMOVE_PROPS) {
        prop->parent = get_prop_by_index(savedPropParentIdx);
        prop->child = get_prop_by_index(savedPropChildIdx);
        prop->prev = get_prop_by_index(savedPropPrevIdx);
        prop->next = get_prop_by_index(savedPropNextIdx);
      }
      prop->rooms[0] = savedPropRooms[0];
      prop->rooms[1] = savedPropRooms[1];
      prop->rooms[2] = savedPropRooms[2];
      prop->rooms[3] = savedPropRooms[3];
      prop->unk30 = savedPropUnk30;

      if (savedPropType == PROP_TYPE_VIEWER) {
        chrpropRegisterRooms(prop);
      }
    }

    switch ((PROP_TYPE)savedPropType) {
    case PROP_TYPE_DOOR: {
      DoorRecord *door = prop->door;
      ObjectRecord *obj = prop->obj;

      if (prop->type != PROP_TYPE_DOOR || prop->door == NULL) {
        practiceLogWarn("Prop at index %d is not a door (type=%d)",
                        savedPropIndex, savedPropType);
        return FALSE;
      }

      if (!load_object_base(stream, obj, prop)) {
        return FALSE;
      }

      load_door_record(stream, door);

      door7F052B00(door);

      if (door->doorFlags & DOORFLAG_0004) {
        Model *model = obj->model;
        ModelNode *node = model->obj->RootNode->Child->Child;
        struct ModelRoData_DisplayList_CollisionRecord *dlcRecord =
            (struct ModelRoData_DisplayList_CollisionRecord *)node->Data;
        struct ModelRwData_DisplayList_CollisionRecord *dst =
            (struct ModelRwData_DisplayList_CollisionRecord *)
                modelGetNodeRwData(model, node);

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

    case PROP_TYPE_OBJ:
    case PROP_TYPE_WEAPON: {
      ObjectRecord *obj = prop->obj;
      if (obj == NULL) {
        // Just skip the rest
        TempObjectRecord temp_obj;
        if (load_object_base(stream, &temp_obj.base, NULL)) {
          load_object_subtype(stream, &temp_obj.base);
        }
        break;
      }

      if (obj->model != NULL && obj->model->obj != NULL) {
        s32 swIdx;
        for (swIdx = 0; swIdx < obj->model->obj->numSwitches; swIdx++) {
          ModelNode *switchNode = obj->model->obj->Switches[swIdx];
          if (switchNode != NULL &&
              (switchNode->Opcode & 0xFF) == MODELNODE_OPCODE_SWITCH) {
            union ModelRwData *rwdata =
                modelGetNodeRwData(obj->model, switchNode);
            if (rwdata != NULL) {
              rwdata->Switch.visible = TRUE;
            }
          }
        }
        sub_GAME_7F050DE8(obj->model);
      }

      if (!load_object_base(stream, obj, prop)) {
        return FALSE;
      }

      load_object_subtype(stream, obj);
      break;
    }

    case PROP_TYPE_SMOKE:
      // TODO: When loading can add or replace props, allocate/select the saved
      // smoke buffer entry and set both prop->smoke and smoke->prop instead of
      // assuming the existing active prop already has the correct backlink.
      if (prop->smoke == NULL || prop->smoke->prop != prop) {
        struct Smoke temp_smoke;
        load_smoke_record(stream, &temp_smoke);
      } else {
        load_smoke_record(stream, prop->smoke);
      }
      break;

    case PROP_TYPE_EXPLOSION:
      // TODO: When loading can add or replace props, allocate/select the saved
      // explosion buffer entry and set both prop->explosion and explosion->prop
      // instead of assuming the existing active prop has the correct backlink.
      if (prop->explosion == NULL || prop->explosion->prop != prop) {
        struct Explosion temp_explosion;
        load_explosion_record(stream, &temp_explosion);
      } else {
        load_explosion_record(stream, prop->explosion);
      }
      break;

    case PROP_TYPE_CHR:
      // Replacement mode rebuilt the PropRecord, ChrRecord, and Model before
      // applying common fields. Testing mode reaches this only for a matching
      // existing active CHR. In replacement mode the saved body and head were
      // used to allocate the model; in testing mode model allocation is
      // intentionally retained, but the immutable configuration IDs are still
      // restored with the rest of the ChrRecord.
      if (prop->chr == NULL) {
        skip_prop_data(stream, PROP_TYPE_CHR);
      } else {
        prop->chr->headnum = savedChrAllocation.headnum;
        prop->chr->bodynum = savedChrAllocation.bodynum;
        load_chr_prop_spatial_state(prop, &savedPropPos, savedPropStanOffset,
                                    savedPropRooms, createdChrProp);
        load_chr_record(stream, prop->chr,
                        &pendingChrAttachments[savedPropIndex]);
        pendingChrAttachmentsValid[savedPropIndex] = TRUE;
      }
      break;

    case PROP_TYPE_NUL:
    case PROP_TYPE_PLAYER:
    // Viewer state loaded after rest of props
    case PROP_TYPE_VIEWER:
    default:
      break;
    }
  }

  // Resolve CHR equipment only after every referenced prop has loaded. In
  // replacement mode indices refer to the rebuilt prop table; testing mode
  // resolves only records which remain in their original slots.
  for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
    if (pendingChrAttachmentsValid[i]) {
      PropRecord *chr_prop = ADD_AND_REMOVE_PROPS
                                 ? get_prop_by_index(i)
                                 : get_enabled_prop_by_index(i);
      if (chr_prop != NULL && chr_prop->type == PROP_TYPE_CHR) {
        restore_chr_attachments(chr_prop, &pendingChrAttachments[i]);
      }
    }
  }

  /* Resolve projectile references only after all saved props are processed. */
  for (pi = 0; pi < PROJECTILES_ARR_MAX; pi++) {
    Projectile *proj = &g_Projectiles[pi];
    PropRecord *objProp;

    if (proj->flags & PROJECTILEFLAG_FREE) {
      continue;
    }

    proj->ownerprop = get_enabled_prop_by_index(projectileOwnerPropIndices[pi]);
    objProp = get_enabled_prop_by_index(projectileObjPropIndices[pi]);

    if (objProp != NULL &&
        (objProp->type == PROP_TYPE_OBJ || objProp->type == PROP_TYPE_DOOR ||
         objProp->type == PROP_TYPE_WEAPON) &&
        objProp->obj != NULL) {
      proj->obj = objProp->obj;
    } else {
      proj->obj = NULL;
      projectileFree(proj);
    }
  }

  if (ADD_AND_REMOVE_PROPS) {
    // Clear any props which were not saved
    for (c = nextIndexToRemove; c < POS_DATA_ENTRY_LEN; c++) {
      removePropAtIndex(c);
    }

    ptr_obj_pos_list_first_entry = get_prop_by_index(indexOfFirstEntry);
    ptr_obj_pos_list_current_entry = get_prop_by_index(indexOfCurrentEntry);
    ptr_obj_pos_list_final_entry = get_prop_by_index(indexOfFinalEntry);
  }

  if (!load_viewer_players_state(stream)) {
    return FALSE;
  }

  return TRUE;
}
