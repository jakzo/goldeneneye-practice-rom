#include "practice_states_chr.h"
#include "practice_states_utils.h"
#include "chrai.h"
#include "objecthandler.h"
#include <bondconstants.h>

extern s32 chraiGetAIListID(AIRecord *AIList, bool *isGlobalAIList);

void save_chr_record(StateStream *stream, const ChrRecord *chr) {
  s32 ailist_id = -1;
  bool has_model_transform =
      chr->model != NULL && chr->model->obj != NULL &&
      chr->model->obj->RootNode != NULL &&
      (chr->model->obj->RootNode->Opcode & 0xff) == MODELNODE_OPCODE_HEADER;

  // Allocation metadata is consumed before the destination ChrRecord exists
  // when ADD_AND_REMOVE_PROPS is enabled.
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

  write_u16(stream, get_prop_index(chr->weapons_held[0]));
  write_u16(stream, get_prop_index(chr->weapons_held[1]));
  write_u16(stream, get_prop_index(chr->weapons_held[2]));
  write_u16(stream, get_prop_index(chr->handle_positiondata_hat));

  {
    s32 hand;
    for (hand = 0; hand < 2; hand++) {
      PropRecord *prop = chr->weapons_held[hand];
      WeaponObjRecord *weapon =
          prop != NULL && prop->obj != NULL &&
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

  chr->accuracyrating = (s8)read_u8(stream);
  chr->speedrating = (s8)read_u8(stream);
  chr->arghrating = (s8)read_u8(stream);
  chr->grenadeprob = read_u8(stream);
  chr->visionrange = read_f32(stream);
  chr->hearingscale = read_f32(stream);
  chr->morale = read_u8(stream);
  chr->alertness = read_u8(stream);

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
