#include "practice_states_props.h"
#include "chrai.h"
#include "chrobjhandler.h"
#include "memp.h"
#include "practice_states.h"
#include "practice_states_utils.h"
#include "practice_storage.h"
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
extern void sub_GAME_7F050DE8(Model *model);
extern void sub_GAME_7F06EFC4(Model *model);
extern void objDeform(ObjectRecord *obj, s32 arg1);
extern PathRecord *pathFindById(s32 ID);

static void removePropAtIndex(s16 index) {
  PropRecord *toClear = get_prop_by_index(index);
  if (toClear == NULL || ADD_AND_REMOVE_PROPS)
    return;

  objFreePermanently(toClear->obj, TRUE);
  // TODO: Do I need to free the other records, models, etc. pointed to by
  // this entry? Or do some special cleanup for certain prop types? Like
  // removing them from global variable lists?
}

static void save_object_base(SavedObjectRecord *savedObj, ObjectRecord *obj) {
  PropDefHeaderRecord *pdhr = obj;
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

  if (obj->runtime_bitflags & RUNTIMEBITFLAG_DEPOSIT &&
      obj->projectile != NULL) {
    savedObj->projectileIdx = (s16)(obj->projectile - g_Projectiles);
  } else {
    savedObj->projectileIdx = -1;
  }

  if (obj->runtime_bitflags & RUNTIMEBITFLAG_EMBEDDED &&
      obj->embedment != NULL) {
    savedObj->embedmentIdx = (s16)(obj->embedment - g_Embedments);
  } else {
    savedObj->embedmentIdx = -1;
  }

  savedObj->switchStates = 0;
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
          savedObj->switchStates |= (1 << i);
        }
      }
    }
  }
}

/* ------------------------------------------------------------------ */
/* Save — streams each prop record to storage via the working memory.  */
/* ------------------------------------------------------------------ */

bool save_props_state(StorageCursor *cur, SaveWorkMem *work) {
  u32 headerOffset;
  u32 dataStart;
  PropsHeaderSection hdr; /* local — NOT in the union (work->propRecord
                              overwrites the union during the loop) */
  s32 i;
  u32 pi;

  headerOffset = cur->offset;
  hdr.recordCount = 0;
  hdr.size = 0;
  hdr.indexOfFirstEntry = get_prop_index(ptr_obj_pos_list_first_entry);
  hdr.indexOfCurrentEntry = get_prop_index(ptr_obj_pos_list_current_entry);
  hdr.indexOfFinalEntry = get_prop_index(ptr_obj_pos_list_final_entry);

  /* Write placeholder header (patched at the end). */
  storage_write(cur, &hdr, sizeof(hdr));
  dataStart = cur->offset;

  for (i = 0; i < POS_DATA_ENTRY_LEN; i++) {
    PropRecord *prop = get_prop_by_index(i);
    SavedPropRecord *savedProp = &work->propRecord.prop;
    u16 byteSize = sizeof(SavedPropRecord);

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

    switch ((PROP_TYPE)prop->type) {
    case PROP_TYPE_DOOR: {
      DoorRecord *door = prop->door;
      SavedDoorRecord *savedDoor = &work->propRecord.type.door;

      if (door == NULL) {
        break;
      }

      save_object_base(&savedDoor->obj, (ObjectRecord *)door);

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

      byteSize = sizeof(SavedPropRecord) + sizeof(SavedDoorRecord);
      break;
    }

    case PROP_TYPE_OBJ: {
      ObjectRecord *obj = prop->obj;

      if (obj == NULL) {
        break;
      }

      switch (obj->type) {
      case PROPDEF_PROP:
      case PROPDEF_ALARM:
      case PROPDEF_RACK:
      case PROPDEF_HAT:
      case PROPDEF_UNK41:
      case PROPDEF_GAS_RELEASING: {
        SavedObjectRecord *savedObj = &work->propRecord.type.door.obj;
        save_object_base(savedObj, obj);
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedObjectRecord);
        break;
      }
      case PROPDEF_KEY: {
        KeyRecord *key = (KeyRecord *)obj;
        SavedKeyRecord *savedKey = &work->propRecord.type.key;
        save_object_base(&savedKey->obj, obj);
        savedKey->keyflags = key->keyflags;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedKeyRecord);
        break;
      }
      case PROPDEF_CCTV: {
        CCTVRecord *cctv = (CCTVRecord *)obj;
        SavedCCTVRecord *savedCCTV = &work->propRecord.type.cctv;
        save_object_base(&savedCCTV->obj, obj);
        savedCCTV->pad = cctv->pad;
        savedCCTV->unk84 = cctv->unk84;
        savedCCTV->unkC4 = cctv->unkC4;
        savedCCTV->unkC8 = cctv->unkC8;
        savedCCTV->unkCC = cctv->unkCC;
        savedCCTV->unkD0 = cctv->unkD0;
        savedCCTV->unkD4 = cctv->unkD4;
        savedCCTV->unkD8 = cctv->unkD8;
        savedCCTV->unkDC = cctv->unkDC;
        savedCCTV->timer = cctv->timer;
        savedCCTV->convert_to_f32 = cctv->convert_to_f32;
        savedCCTV->unkE8 = cctv->unkE8;
        savedCCTV->unkEC = cctv->unkEC;
        savedCCTV->unkF0 = cctv->unkF0;
        savedCCTV->unkF4 = cctv->unkF4;
        savedCCTV->unkF8 = cctv->unkF8;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedCCTVRecord);
        break;
      }
      case PROPDEF_MAGAZINE: {
        AmmoCrateRecord *mag = (AmmoCrateRecord *)obj;
        SavedAmmoCrateRecord *savedMag = &work->propRecord.type.magazine;
        save_object_base(&savedMag->obj, obj);
        savedMag->ammoType = mag->ammoType;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedAmmoCrateRecord);
        break;
      }
      case PROPDEF_COLLECTABLE: {
        WeaponObjRecord *wpn = (WeaponObjRecord *)obj;
        SavedWeaponObjRecord *savedWpn = &work->propRecord.type.weaponObj;
        save_object_base(&savedWpn->obj, obj);
        savedWpn->weaponnum = wpn->weaponnum;
        savedWpn->LinkedWeaponType = wpn->LinkedWeaponType;
        savedWpn->timer = wpn->timer;
        savedWpn->dualweaponPropIndex =
            (wpn->dualweapon && wpn->dualweapon->prop)
                ? get_prop_index(wpn->dualweapon->prop)
                : -1;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedWeaponObjRecord);
        break;
      }
      case PROPDEF_MONITOR: {
        MonitorObjRecord *mon = (MonitorObjRecord *)obj;
        SavedMonitorObjRecord *savedMon = &work->propRecord.type.monitor;
        save_object_base(&savedMon->obj, obj);

        savedMon->Monitor.cmdlist = (u32)mon->Monitor.cmdlist;
        savedMon->Monitor.offset = mon->Monitor.offset;
        savedMon->Monitor.pause60 = mon->Monitor.pause60;
        savedMon->Monitor.tconfig = (u32)mon->Monitor.tconfig;
        savedMon->Monitor.rot = mon->Monitor.rot;
        savedMon->Monitor.xscale = mon->Monitor.xscale;
        savedMon->Monitor.xscalefrac = mon->Monitor.xscalefrac;
        savedMon->Monitor.xscaleinc = mon->Monitor.xscaleinc;
        savedMon->Monitor.xscaleold = mon->Monitor.xscaleold;
        savedMon->Monitor.xscalenew = mon->Monitor.xscalenew;
        savedMon->Monitor.yscale = mon->Monitor.yscale;
        savedMon->Monitor.yscalefrac = mon->Monitor.yscalefrac;
        savedMon->Monitor.yscaleinc = mon->Monitor.yscaleinc;
        savedMon->Monitor.yscaleold = mon->Monitor.yscaleold;
        savedMon->Monitor.yscalenew = mon->Monitor.yscalenew;
        savedMon->Monitor.xmid = mon->Monitor.xmid;
        savedMon->Monitor.xmidfrac = mon->Monitor.xmidfrac;
        savedMon->Monitor.xmidinc = mon->Monitor.xmidinc;
        savedMon->Monitor.xmidold = mon->Monitor.xmidold;
        savedMon->Monitor.xmidnew = mon->Monitor.xmidnew;
        savedMon->Monitor.ymid = mon->Monitor.ymid;
        savedMon->Monitor.ymidfrac = mon->Monitor.ymidfrac;
        savedMon->Monitor.ymidinc = mon->Monitor.ymidinc;
        savedMon->Monitor.ymidold = mon->Monitor.ymidold;
        savedMon->Monitor.ymidnew = mon->Monitor.ymidnew;
        savedMon->Monitor.red = mon->Monitor.red;
        savedMon->Monitor.redold = mon->Monitor.redold;
        savedMon->Monitor.rednew = mon->Monitor.rednew;
        savedMon->Monitor.green = mon->Monitor.green;
        savedMon->Monitor.greenold = mon->Monitor.greenold;
        savedMon->Monitor.greennew = mon->Monitor.greennew;
        savedMon->Monitor.blue = mon->Monitor.blue;
        savedMon->Monitor.blueold = mon->Monitor.blueold;
        savedMon->Monitor.bluenew = mon->Monitor.bluenew;
        savedMon->Monitor.alpha = mon->Monitor.alpha;
        savedMon->Monitor.alphaold = mon->Monitor.alphaold;
        savedMon->Monitor.alphanew = mon->Monitor.alphanew;
        savedMon->Monitor.colfrac = mon->Monitor.colfrac;
        savedMon->Monitor.colinc = mon->Monitor.colinc;

        savedMon->OwnerOffset = mon->OwnerOffset;
        savedMon->OwnerPart = mon->OwnerPart;
        savedMon->ImageNum = mon->ImageNum;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedMonitorObjRecord);
        break;
      }
      case PROPDEF_MULTI_MONITOR: {
        MultiMonitorObjRecord *mmon = (MultiMonitorObjRecord *)obj;
        SavedMultiMonitorObjRecord *savedMmon =
            &work->propRecord.type.multiMonitor;
        s32 m;
        save_object_base(&savedMmon->obj, obj);

        for (m = 0; m < 4; m++) {
          savedMmon->Monitor[m].cmdlist = (u32)mmon->Monitor[m].cmdlist;
          savedMmon->Monitor[m].offset = mmon->Monitor[m].offset;
          savedMmon->Monitor[m].pause60 = mmon->Monitor[m].pause60;
          savedMmon->Monitor[m].tconfig = (u32)mmon->Monitor[m].tconfig;
          savedMmon->Monitor[m].rot = mmon->Monitor[m].rot;
          savedMmon->Monitor[m].xscale = mmon->Monitor[m].xscale;
          savedMmon->Monitor[m].xscalefrac = mmon->Monitor[m].xscalefrac;
          savedMmon->Monitor[m].xscaleinc = mmon->Monitor[m].xscaleinc;
          savedMmon->Monitor[m].xscaleold = mmon->Monitor[m].xscaleold;
          savedMmon->Monitor[m].xscalenew = mmon->Monitor[m].xscalenew;
          savedMmon->Monitor[m].yscale = mmon->Monitor[m].yscale;
          savedMmon->Monitor[m].yscalefrac = mmon->Monitor[m].yscalefrac;
          savedMmon->Monitor[m].yscaleinc = mmon->Monitor[m].yscaleinc;
          savedMmon->Monitor[m].yscaleold = mmon->Monitor[m].yscaleold;
          savedMmon->Monitor[m].yscalenew = mmon->Monitor[m].yscalenew;
          savedMmon->Monitor[m].xmid = mmon->Monitor[m].xmid;
          savedMmon->Monitor[m].xmidfrac = mmon->Monitor[m].xmidfrac;
          savedMmon->Monitor[m].xmidinc = mmon->Monitor[m].xmidinc;
          savedMmon->Monitor[m].xmidold = mmon->Monitor[m].xmidold;
          savedMmon->Monitor[m].xmidnew = mmon->Monitor[m].xmidnew;
          savedMmon->Monitor[m].ymid = mmon->Monitor[m].ymid;
          savedMmon->Monitor[m].ymidfrac = mmon->Monitor[m].ymidfrac;
          savedMmon->Monitor[m].ymidinc = mmon->Monitor[m].ymidinc;
          savedMmon->Monitor[m].ymidold = mmon->Monitor[m].ymidold;
          savedMmon->Monitor[m].ymidnew = mmon->Monitor[m].ymidnew;
          savedMmon->Monitor[m].red = mmon->Monitor[m].red;
          savedMmon->Monitor[m].redold = mmon->Monitor[m].redold;
          savedMmon->Monitor[m].rednew = mmon->Monitor[m].rednew;
          savedMmon->Monitor[m].green = mmon->Monitor[m].green;
          savedMmon->Monitor[m].greenold = mmon->Monitor[m].greenold;
          savedMmon->Monitor[m].greennew = mmon->Monitor[m].greennew;
          savedMmon->Monitor[m].blue = mmon->Monitor[m].blue;
          savedMmon->Monitor[m].blueold = mmon->Monitor[m].blueold;
          savedMmon->Monitor[m].bluenew = mmon->Monitor[m].bluenew;
          savedMmon->Monitor[m].alpha = mmon->Monitor[m].alpha;
          savedMmon->Monitor[m].alphaold = mmon->Monitor[m].alphaold;
          savedMmon->Monitor[m].alphanew = mmon->Monitor[m].alphanew;
          savedMmon->Monitor[m].colfrac = mmon->Monitor[m].colfrac;
          savedMmon->Monitor[m].colinc = mmon->Monitor[m].colinc;
        }

        savedMmon->ImageNums[0] = mmon->ImageNums[0];
        savedMmon->ImageNums[1] = mmon->ImageNums[1];
        savedMmon->ImageNums[2] = mmon->ImageNums[2];
        savedMmon->ImageNums[3] = mmon->ImageNums[3];
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedMultiMonitorObjRecord);
        break;
      }
      case PROPDEF_AUTOGUN: {
        AutogunRecord *agun = (AutogunRecord *)obj;
        SavedAutogunRecord *savedAgun = &work->propRecord.type.autogun;
        save_object_base(&savedAgun->obj, obj);
        savedAgun->padID = agun->padID;
        savedAgun->rot_related = agun->rot_related;
        savedAgun->unk88 = agun->unk88;
        savedAgun->unk8C = agun->unk8C;
        savedAgun->unk90 = agun->unk90;
        savedAgun->unk94 = agun->unk94;
        savedAgun->unk98 = agun->unk98;
        savedAgun->unk9C = agun->unk9C;
        savedAgun->unkA0 = agun->unkA0;
        savedAgun->speed = agun->speed;
        savedAgun->aimdist = agun->aimdist;
        savedAgun->unkAC = agun->unkAC;
        savedAgun->unkB0 = agun->unkB0;
        savedAgun->unkB4 = agun->unkB4;
        savedAgun->unkB8 = agun->unkB8;
        savedAgun->unkBC = agun->unkBC;
        savedAgun->unkC0 = agun->unkC0;
        if (agun->beam != NULL) {
          savedAgun->beam = *agun->beam;
        } else {
          savedAgun->beam.age = -1;
          savedAgun->beam.weaponnum = 0;
          savedAgun->beam.from.x = 0.0f;
          savedAgun->beam.from.y = 0.0f;
          savedAgun->beam.from.z = 0.0f;
          savedAgun->beam.dir.x = 0.0f;
          savedAgun->beam.dir.y = 0.0f;
          savedAgun->beam.dir.z = 0.0f;
          savedAgun->beam.maxdist = 0.0f;
          savedAgun->beam.speed = 0.0f;
          savedAgun->beam.mindist = 0.0f;
          savedAgun->beam.dist = 0.0f;
        }
        savedAgun->is_active = agun->is_active;
        savedAgun->unkD4 = agun->unkD4;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedAutogunRecord);
        break;
      }
      case PROPDEF_AMMO: {
        MultiAmmoCrateRecord *ammo = (MultiAmmoCrateRecord *)obj;
        SavedMultiAmmoCrateRecord *savedAmmo = &work->propRecord.type.ammoCrate;
        s32 a;
        save_object_base(&savedAmmo->obj, obj);
        savedAmmo->unk80 = ammo->unk80;
        for (a = 0; a < AMMOTYPE_GLOBAL_MAX; a++) {
          savedAmmo->quantities[a] = ammo->quantities[a];
        }
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedMultiAmmoCrateRecord);
        break;
      }
      case PROPDEF_ARMOUR: {
        BodyArmourRecord *arm = (BodyArmourRecord *)obj;
        SavedBodyArmourRecord *savedArm = &work->propRecord.type.armour;
        save_object_base(&savedArm->obj, obj);
        savedArm->initialamount = arm->initialamount;
        savedArm->amount = arm->amount;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedBodyArmourRecord);
        break;
      }
      case PROPDEF_VEHICHLE: {
        VehichleRecord *veh = (VehichleRecord *)obj;
        SavedVehichleRecord *savedVeh = &work->propRecord.type.vehicle;
        bool isGlobal;
        save_object_base(&savedVeh->obj, obj);
        savedVeh->ailist =
            veh->ailist ? chraiGetAIListID(veh->ailist, &isGlobal) : 0;
        savedVeh->aioffset = veh->aioffset;
        savedVeh->aireturnlist = veh->aireturnlist;
        savedVeh->speed = veh->speed;
        savedVeh->wheelxrot = veh->wheelxrot;
        savedVeh->wheelyrot = veh->wheelyrot;
        savedVeh->speedaim = veh->speedaim;
        savedVeh->speedtime60 = veh->speedtime60;
        savedVeh->turnrot60 = veh->turnrot60;
        savedVeh->roty = veh->roty;
        savedVeh->path = veh->path ? veh->path->ID : 0;
        savedVeh->nextstep = veh->nextstep;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedVehichleRecord);
        break;
      }
      case PROPDEF_AIRCRAFT: {
        AircraftRecord *air = (AircraftRecord *)obj;
        SavedAircraftRecord *savedAir = &work->propRecord.type.aircraft;
        bool isGlobal;
        save_object_base(&savedAir->obj, obj);
        savedAir->ailist =
            air->ailist ? chraiGetAIListID(air->ailist, &isGlobal) : 0;
        savedAir->aioffset = air->aioffset;
        savedAir->aireturnlist = air->aireturnlist;
        savedAir->rotoryrot = air->rotoryrot;
        savedAir->rotaryspeed = air->rotaryspeed;
        savedAir->rotaryspeedaim = air->rotaryspeedaim;
        savedAir->rotaryspeedtime = air->rotaryspeedtime;
        savedAir->speed = air->speed;
        savedAir->speedaim = air->speedaim;
        savedAir->speedtime60 = air->speedtime60;
        savedAir->yrot = air->yrot;
        savedAir->nextstep = air->nextstep;
        savedAir->path = air->path ? air->path->ID : 0;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedAircraftRecord);
        break;
      }
      case PROPDEF_GLASS:
      case PROPDEF_SAFE: {
        GlassRecord *gl = (GlassRecord *)obj;
        SavedGlassRecord *savedGl = &work->propRecord.type.glass;
        save_object_base(&savedGl->obj, obj);
        savedGl->normal = gl->normal;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedGlassRecord);
        break;
      }
      case PROPDEF_TINTED_GLASS: {
        TintedGlassRecord *tgl = (TintedGlassRecord *)obj;
        SavedTintedGlassRecord *savedTgl = &work->propRecord.type.tintedGlass;
        save_object_base(&savedTgl->obj, obj);
        savedTgl->TintDist = tgl->TintDist;
        savedTgl->CullDist = tgl->CullDist;
        savedTgl->calculatedopacity = tgl->calculatedopacity;
        savedTgl->portalnum = tgl->portalnum;
        savedTgl->unk90 = tgl->unk90;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedTintedGlassRecord);
        break;
      }
      case PROPDEF_TANK: {
        TankRecord *tank = (TankRecord *)obj;
        SavedTankRecord *savedTank = &work->propRecord.type.tank;
        save_object_base(&savedTank->obj, obj);
        savedTank->rect = tank->rect;
        savedTank->unkA4 = tank->unkA4;
        savedTank->unkA8 = tank->unkA8;
        savedTank->unkAC = tank->unkAC;
        savedTank->unkB0 = tank->unkB0;
        savedTank->unkB4 = tank->unkB4;
        savedTank->unkB8 = tank->unkB8;
        savedTank->unkBC = tank->unkBC;
        savedTank->unkC0 = tank->unkC0;
        savedTank->is_firing_tank = tank->is_firing_tank;
        savedTank->turret_vertical_angle = tank->turret_vertical_angle;
        savedTank->turret_orientation_angle = tank->turret_orientation_angle;
        savedTank->unkD0 = tank->unkD0;
        savedTank->stan_y = tank->stan_y;
        savedTank->unkD8 = tank->unkD8;
        savedTank->tank_orientation_angle = tank->tank_orientation_angle;
        byteSize = sizeof(SavedPropRecord) + sizeof(SavedTankRecord);
        break;
      }
      default:
        break;
      }
      break;
    }

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

    savedProp->byteSize = byteSize;
    storage_write(cur, &work->propRecord, byteSize);
    hdr.recordCount++;
  }

  /* Save projectile snapshots. */
  // TODO: Potentially could exclude matrixes from projectile/embed data
  for (pi = 0; pi < PROJECTILES_ARR_MAX; pi++) {
    SavedProjectileEntry *projEntry = &work->projectile;
    projEntry->index = (s32)pi;
    projEntry->projectile = g_Projectiles[pi];
    projEntry->projectile.obj = NULL;
    projEntry->projectile.ownerprop = NULL;
    projEntry->projectile.sound1 = NULL;
    projEntry->projectile.sound2 = NULL;
    storage_write(cur, projEntry, sizeof(*projEntry));
  }

  /* Save embedment snapshots. */
  for (pi = 0; pi < EMBEDMENT_ARR_MAX; pi++) {
    SavedEmbedmentEntry *embEntry = &work->embedment;
    s32 projIdx = -1;
    embEntry->index = (s32)pi;
    embEntry->embedment = g_Embedments[pi];
    if (g_Embedments[pi].projectile != NULL) {
      projIdx = g_Embedments[pi].projectile - g_Projectiles;
    }
    embEntry->embedment.projectile = (struct Projectile *)(s32)projIdx;
    storage_write(cur, embEntry, sizeof(*embEntry));
  }

  /* Patch the props header with the real size and record count. */
  hdr.size = cur->offset - dataStart;
  {
    StorageCursor patchCur;
    storage_cursor_init(&patchCur, headerOffset);
    storage_write(&patchCur, &hdr, sizeof(hdr));
  }

  return TRUE;
}

/* ------------------------------------------------------------------ */
/* Load                                                                */
/* ------------------------------------------------------------------ */

static bool load_saved_object(char *typeName, ObjectRecord *obj,
                              SavedObjectRecord *savedObj, PropRecord *prop) {
  PropDefHeaderRecord *pdhr = obj;
  s32 destroyedLvl;

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
  obj->maxdamage = savedObj->maxdamage;
  obj->damage = savedObj->damage;
  obj->shadecol = savedObj->shadecol;
  obj->nextcol = savedObj->nextcol;

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
        if (rwdata != NULL) {
          rwdata->Switch.visible = (savedObj->switchStates & (1 << i)) != 0;
        }
      }
    }
    sub_GAME_7F06EFC4(obj->model);
  }

  destroyedLvl = objGetDestroyedLevel(obj);
  if (destroyedLvl > 0) {
    objDeform(obj, destroyedLvl);
  }

  return TRUE;
}

bool load_props_state(StorageCursor *cur, SaveWorkMem *work) {
  PropsHeaderSection hdr; /* local — NOT in the union */
  u32 dataStart;
  s32 i;
  u16 nextIndexToRemove = 0;
  s32 c;
  u32 pi;

  /* Read props header. */
  storage_read(cur, &hdr, sizeof(hdr));
  dataStart = cur->offset;

  for (i = 0; i < hdr.recordCount; i++) {
    SavedPropRecord *savedProp = &work->propRecord.prop;
    u16 byteSize;
    PropRecord *prop;
    bool supportedType = FALSE;

    /* Read the SavedPropRecord header to get byteSize. */
    storage_read(cur, savedProp, sizeof(SavedPropRecord));
    byteSize = savedProp->byteSize;

    /* Read the type-specific data into the union. */
    if (byteSize > sizeof(SavedPropRecord)) {
      storage_read(cur, &work->propRecord.type,
                   byteSize - sizeof(SavedPropRecord));
    }

    prop = get_prop_by_index(savedProp->index);

    if (ADD_AND_REMOVE_PROPS) {
      for (c = nextIndexToRemove; c < savedProp->index; c++) {
        removePropAtIndex(c);
      }
      nextIndexToRemove = savedProp->index + 1;
    }

    /* Only modify props which still exist while testing. */
    if (!ADD_AND_REMOVE_PROPS &&
        (prop == NULL || !(prop->flags & PROPFLAG_ENABLED) ||
         prop->type != savedProp->type)) {
      continue;
    }

    switch ((PROP_TYPE)prop->type) {
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

    switch ((PROP_TYPE)savedProp->type) {
    case PROP_TYPE_DOOR: {
      const SavedDoorRecord *savedDoor = &work->propRecord.type.door;
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
      // TODO: Figure out why maxFrac is always set to 0xFFFFFFFF!?!?
      // door->maxFrac = savedDoor->maxFrac;
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
      ObjectRecord *obj = prop->obj;
      if (obj == NULL) {
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

      switch (obj->type) {
      case PROPDEF_PROP:
      case PROPDEF_ALARM:
      case PROPDEF_RACK:
      case PROPDEF_HAT:
      case PROPDEF_UNK41:
      case PROPDEF_GAS_RELEASING: {
        const SavedObjectRecord *savedObj =
            (const SavedObjectRecord *)&work->propRecord.type;
        if (!load_saved_object("Prop/Generic", obj,
                               (SavedObjectRecord *)savedObj, prop))
          return FALSE;
        break;
      }
      case PROPDEF_KEY: {
        const SavedKeyRecord *savedKey = &work->propRecord.type.key;
        KeyRecord *key = (KeyRecord *)obj;
        if (!load_saved_object("Key", (ObjectRecord *)key, &savedKey->obj,
                               prop))
          return FALSE;
        key->keyflags = savedKey->keyflags;
        break;
      }
      case PROPDEF_CCTV: {
        const SavedCCTVRecord *savedCCTV = &work->propRecord.type.cctv;
        CCTVRecord *cctv = (CCTVRecord *)obj;
        if (!load_saved_object("CCTV", (ObjectRecord *)cctv, &savedCCTV->obj,
                               prop))
          return FALSE;
        cctv->pad = savedCCTV->pad;
        cctv->unk84 = savedCCTV->unk84;
        cctv->unkC4 = savedCCTV->unkC4;
        cctv->unkC8 = savedCCTV->unkC8;
        cctv->unkCC = savedCCTV->unkCC;
        cctv->unkD0 = savedCCTV->unkD0;
        cctv->unkD4 = savedCCTV->unkD4;
        cctv->unkD8 = savedCCTV->unkD8;
        cctv->unkDC = savedCCTV->unkDC;
        cctv->timer = savedCCTV->timer;
        cctv->convert_to_f32 = savedCCTV->convert_to_f32;
        cctv->unkE8 = savedCCTV->unkE8;
        cctv->unkEC = savedCCTV->unkEC;
        cctv->unkF0 = savedCCTV->unkF0;
        cctv->unkF4 = savedCCTV->unkF4;
        cctv->unkF8 = savedCCTV->unkF8;
        break;
      }
      case PROPDEF_MAGAZINE: {
        const SavedAmmoCrateRecord *savedMag = &work->propRecord.type.magazine;
        AmmoCrateRecord *mag = (AmmoCrateRecord *)obj;
        if (!load_saved_object("Magazine", (ObjectRecord *)mag, &savedMag->obj,
                               prop))
          return FALSE;
        mag->ammoType = savedMag->ammoType;
        break;
      }
      case PROPDEF_COLLECTABLE: {
        const SavedWeaponObjRecord *savedWpn = &work->propRecord.type.weaponObj;
        WeaponObjRecord *wpn = (WeaponObjRecord *)obj;
        if (!load_saved_object("Weapon", (ObjectRecord *)wpn, &savedWpn->obj,
                               prop))
          return FALSE;
        wpn->weaponnum = savedWpn->weaponnum;
        wpn->LinkedWeaponType = savedWpn->LinkedWeaponType;
        wpn->timer = savedWpn->timer;
        wpn->dualweapon =
            savedWpn->dualweaponPropIndex != -1
                ? get_prop_by_index(savedWpn->dualweaponPropIndex)->weapon
                : NULL;
        break;
      }
      case PROPDEF_MONITOR: {
        const SavedMonitorObjRecord *savedMon = &work->propRecord.type.monitor;
        MonitorObjRecord *mon = (MonitorObjRecord *)obj;
        if (!load_saved_object("Monitor", (ObjectRecord *)mon, &savedMon->obj,
                               prop))
          return FALSE;

        mon->Monitor.cmdlist = (u32 *)savedMon->Monitor.cmdlist;
        mon->Monitor.offset = savedMon->Monitor.offset;
        mon->Monitor.pause60 = savedMon->Monitor.pause60;
        mon->Monitor.tconfig =
            (struct sImageTableEntry *)savedMon->Monitor.tconfig;
        mon->Monitor.rot = savedMon->Monitor.rot;
        mon->Monitor.xscale = savedMon->Monitor.xscale;
        mon->Monitor.xscalefrac = savedMon->Monitor.xscalefrac;
        mon->Monitor.xscaleinc = savedMon->Monitor.xscaleinc;
        mon->Monitor.xscaleold = savedMon->Monitor.xscaleold;
        mon->Monitor.xscalenew = savedMon->Monitor.xscalenew;
        mon->Monitor.yscale = savedMon->Monitor.yscale;
        mon->Monitor.yscalefrac = savedMon->Monitor.yscalefrac;
        mon->Monitor.yscaleinc = savedMon->Monitor.yscaleinc;
        mon->Monitor.yscaleold = savedMon->Monitor.yscaleold;
        mon->Monitor.yscalenew = savedMon->Monitor.yscalenew;
        mon->Monitor.xmid = savedMon->Monitor.xmid;
        mon->Monitor.xmidfrac = savedMon->Monitor.xmidfrac;
        mon->Monitor.xmidinc = savedMon->Monitor.xmidinc;
        mon->Monitor.xmidold = savedMon->Monitor.xmidold;
        mon->Monitor.xmidnew = savedMon->Monitor.xmidnew;
        mon->Monitor.ymid = savedMon->Monitor.ymid;
        mon->Monitor.ymidfrac = savedMon->Monitor.ymidfrac;
        mon->Monitor.ymidinc = savedMon->Monitor.ymidinc;
        mon->Monitor.ymidold = savedMon->Monitor.ymidold;
        mon->Monitor.ymidnew = savedMon->Monitor.ymidnew;
        mon->Monitor.red = savedMon->Monitor.red;
        mon->Monitor.redold = savedMon->Monitor.redold;
        mon->Monitor.rednew = savedMon->Monitor.rednew;
        mon->Monitor.green = savedMon->Monitor.green;
        mon->Monitor.greenold = savedMon->Monitor.greenold;
        mon->Monitor.greennew = savedMon->Monitor.greennew;
        mon->Monitor.blue = savedMon->Monitor.blue;
        mon->Monitor.blueold = savedMon->Monitor.blueold;
        mon->Monitor.bluenew = savedMon->Monitor.bluenew;
        mon->Monitor.alpha = savedMon->Monitor.alpha;
        mon->Monitor.alphaold = savedMon->Monitor.alphaold;
        mon->Monitor.alphanew = savedMon->Monitor.alphanew;
        mon->Monitor.colfrac = savedMon->Monitor.colfrac;
        mon->Monitor.colinc = savedMon->Monitor.colinc;

        mon->OwnerOffset = savedMon->OwnerOffset;
        mon->OwnerPart = savedMon->OwnerPart;
        mon->ImageNum = savedMon->ImageNum;
        break;
      }
      case PROPDEF_MULTI_MONITOR: {
        const SavedMultiMonitorObjRecord *savedMmon =
            &work->propRecord.type.multiMonitor;
        MultiMonitorObjRecord *mmon = (MultiMonitorObjRecord *)obj;
        s32 m;
        if (!load_saved_object("MultiMonitor", (ObjectRecord *)mmon,
                               &savedMmon->obj, prop))
          return FALSE;

        for (m = 0; m < 4; m++) {
          mmon->Monitor[m].cmdlist = (u32 *)savedMmon->Monitor[m].cmdlist;
          mmon->Monitor[m].offset = savedMmon->Monitor[m].offset;
          mmon->Monitor[m].pause60 = savedMmon->Monitor[m].pause60;
          mmon->Monitor[m].tconfig =
              (struct sImageTableEntry *)savedMmon->Monitor[m].tconfig;
          mmon->Monitor[m].rot = savedMmon->Monitor[m].rot;
          mmon->Monitor[m].xscale = savedMmon->Monitor[m].xscale;
          mmon->Monitor[m].xscalefrac = savedMmon->Monitor[m].xscalefrac;
          mmon->Monitor[m].xscaleinc = savedMmon->Monitor[m].xscaleinc;
          mmon->Monitor[m].xscaleold = savedMmon->Monitor[m].xscaleold;
          mmon->Monitor[m].xscalenew = savedMmon->Monitor[m].xscalenew;
          mmon->Monitor[m].yscale = savedMmon->Monitor[m].yscale;
          mmon->Monitor[m].yscalefrac = savedMmon->Monitor[m].yscalefrac;
          mmon->Monitor[m].yscaleinc = savedMmon->Monitor[m].yscaleinc;
          mmon->Monitor[m].yscaleold = savedMmon->Monitor[m].yscaleold;
          mmon->Monitor[m].yscalenew = savedMmon->Monitor[m].yscalenew;
          mmon->Monitor[m].xmid = savedMmon->Monitor[m].xmid;
          mmon->Monitor[m].xmidfrac = savedMmon->Monitor[m].xmidfrac;
          mmon->Monitor[m].xmidinc = savedMmon->Monitor[m].xmidinc;
          mmon->Monitor[m].xmidold = savedMmon->Monitor[m].xmidold;
          mmon->Monitor[m].xmidnew = savedMmon->Monitor[m].xmidnew;
          mmon->Monitor[m].ymid = savedMmon->Monitor[m].ymid;
          mmon->Monitor[m].ymidfrac = savedMmon->Monitor[m].ymidfrac;
          mmon->Monitor[m].ymidinc = savedMmon->Monitor[m].ymidinc;
          mmon->Monitor[m].ymidold = savedMmon->Monitor[m].ymidold;
          mmon->Monitor[m].ymidnew = savedMmon->Monitor[m].ymidnew;
          mmon->Monitor[m].red = savedMmon->Monitor[m].red;
          mmon->Monitor[m].redold = savedMmon->Monitor[m].redold;
          mmon->Monitor[m].rednew = savedMmon->Monitor[m].rednew;
          mmon->Monitor[m].green = savedMmon->Monitor[m].green;
          mmon->Monitor[m].greenold = savedMmon->Monitor[m].greenold;
          mmon->Monitor[m].greennew = savedMmon->Monitor[m].greennew;
          mmon->Monitor[m].blue = savedMmon->Monitor[m].blue;
          mmon->Monitor[m].blueold = savedMmon->Monitor[m].blueold;
          mmon->Monitor[m].bluenew = savedMmon->Monitor[m].bluenew;
          mmon->Monitor[m].alpha = savedMmon->Monitor[m].alpha;
          mmon->Monitor[m].alphaold = savedMmon->Monitor[m].alphaold;
          mmon->Monitor[m].alphanew = savedMmon->Monitor[m].alphanew;
          mmon->Monitor[m].colfrac = savedMmon->Monitor[m].colfrac;
          mmon->Monitor[m].colinc = savedMmon->Monitor[m].colinc;
        }

        mmon->ImageNums[0] = savedMmon->ImageNums[0];
        mmon->ImageNums[1] = savedMmon->ImageNums[1];
        mmon->ImageNums[2] = savedMmon->ImageNums[2];
        mmon->ImageNums[3] = savedMmon->ImageNums[3];
        break;
      }
      case PROPDEF_AUTOGUN: {
        const SavedAutogunRecord *savedAgun = &work->propRecord.type.autogun;
        AutogunRecord *agun = (AutogunRecord *)obj;
        if (!load_saved_object("Autogun", (ObjectRecord *)agun, &savedAgun->obj,
                               prop))
          return FALSE;
        agun->padID = savedAgun->padID;
        agun->rot_related = savedAgun->rot_related;
        agun->unk88 = savedAgun->unk88;
        agun->unk8C = savedAgun->unk8C;
        agun->unk90 = savedAgun->unk90;
        agun->unk94 = savedAgun->unk94;
        agun->unk98 = savedAgun->unk98;
        agun->unk9C = savedAgun->unk9C;
        agun->unkA0 = savedAgun->unkA0;
        agun->speed = savedAgun->speed;
        agun->aimdist = savedAgun->aimdist;
        agun->unkAC = savedAgun->unkAC;
        agun->unkB0 = savedAgun->unkB0;
        agun->unkB4 = savedAgun->unkB4;
        agun->unkB8 = savedAgun->unkB8;
        agun->unkBC = savedAgun->unkBC;
        agun->unkC0 = savedAgun->unkC0;
        agun->unkC4 = NULL;
        agun->unkC8 = NULL;
        if (agun->beam != NULL) {
          *agun->beam = savedAgun->beam;
        }
        agun->is_active = savedAgun->is_active;
        agun->unkD4 = savedAgun->unkD4;
        break;
      }
      case PROPDEF_AMMO: {
        const SavedMultiAmmoCrateRecord *savedAmmo =
            &work->propRecord.type.ammoCrate;
        MultiAmmoCrateRecord *ammo = (MultiAmmoCrateRecord *)obj;
        s32 a;
        if (!load_saved_object("AmmoCrate", (ObjectRecord *)ammo,
                               &savedAmmo->obj, prop))
          return FALSE;
        ammo->unk80 = savedAmmo->unk80;
        for (a = 0; a < AMMOTYPE_GLOBAL_MAX; a++) {
          ammo->quantities[a] = savedAmmo->quantities[a];
        }
        break;
      }
      case PROPDEF_ARMOUR: {
        const SavedBodyArmourRecord *savedArm = &work->propRecord.type.armour;
        BodyArmourRecord *arm = (BodyArmourRecord *)obj;
        if (!load_saved_object("Armour", (ObjectRecord *)arm, &savedArm->obj,
                               prop))
          return FALSE;
        arm->initialamount = savedArm->initialamount;
        arm->amount = savedArm->amount;
        break;
      }
      case PROPDEF_VEHICHLE: {
        const SavedVehichleRecord *savedVeh = &work->propRecord.type.vehicle;
        VehichleRecord *veh = (VehichleRecord *)obj;
        if (!load_saved_object("Vehicle", (ObjectRecord *)veh, &savedVeh->obj,
                               prop))
          return FALSE;
        veh->ailist =
            savedVeh->ailist ? ailistFindById(savedVeh->ailist) : NULL;
        veh->aioffset = savedVeh->aioffset;
        veh->aireturnlist = savedVeh->aireturnlist;
        veh->speed = savedVeh->speed;
        veh->wheelxrot = savedVeh->wheelxrot;
        veh->wheelyrot = savedVeh->wheelyrot;
        veh->speedaim = savedVeh->speedaim;
        veh->speedtime60 = savedVeh->speedtime60;
        veh->turnrot60 = savedVeh->turnrot60;
        veh->roty = savedVeh->roty;
        veh->path = savedVeh->path ? pathFindById(savedVeh->path) : NULL;
        veh->nextstep = savedVeh->nextstep;
        veh->Sound = NULL;
        break;
      }
      case PROPDEF_AIRCRAFT: {
        const SavedAircraftRecord *savedAir = &work->propRecord.type.aircraft;
        AircraftRecord *air = (AircraftRecord *)obj;
        if (!load_saved_object("Aircraft", (ObjectRecord *)air, &savedAir->obj,
                               prop))
          return FALSE;
        air->ailist =
            savedAir->ailist ? ailistFindById(savedAir->ailist) : NULL;
        air->aioffset = savedAir->aioffset;
        air->aireturnlist = savedAir->aireturnlist;
        air->rotoryrot = savedAir->rotoryrot;
        air->rotaryspeed = savedAir->rotaryspeed;
        air->rotaryspeedaim = savedAir->rotaryspeedaim;
        air->rotaryspeedtime = savedAir->rotaryspeedtime;
        air->speed = savedAir->speed;
        air->speedaim = savedAir->speedaim;
        air->speedtime60 = savedAir->speedtime60;
        air->yrot = savedAir->yrot;
        air->nextstep = savedAir->nextstep;
        air->path = savedAir->path ? pathFindById(savedAir->path) : NULL;
        air->Sound = NULL; // Clear sound state
        break;
      }
      case PROPDEF_GLASS:
      case PROPDEF_SAFE: {
        const SavedGlassRecord *savedGl = &work->propRecord.type.glass;
        GlassRecord *gl = (GlassRecord *)obj;
        if (!load_saved_object("Glass/Safe", (ObjectRecord *)gl, &savedGl->obj,
                               prop))
          return FALSE;
        gl->normal = savedGl->normal;
        break;
      }
      case PROPDEF_TINTED_GLASS: {
        const SavedTintedGlassRecord *savedTgl =
            &work->propRecord.type.tintedGlass;
        TintedGlassRecord *tgl = (TintedGlassRecord *)obj;
        if (!load_saved_object("TintedGlass", (ObjectRecord *)tgl,
                               &savedTgl->obj, prop))
          return FALSE;
        tgl->TintDist = savedTgl->TintDist;
        tgl->CullDist = savedTgl->CullDist;
        tgl->calculatedopacity = savedTgl->calculatedopacity;
        tgl->portalnum = savedTgl->portalnum;
        tgl->unk90 = savedTgl->unk90;
        break;
      }
      case PROPDEF_TANK: {
        const SavedTankRecord *savedTank = &work->propRecord.type.tank;
        TankRecord *tank = (TankRecord *)obj;
        if (!load_saved_object("Tank", (ObjectRecord *)tank, &savedTank->obj,
                               prop))
          return FALSE;
        tank->rect = savedTank->rect;
        tank->unkA4 = savedTank->unkA4;
        tank->unkA8 = savedTank->unkA8;
        tank->unkAC = savedTank->unkAC;
        tank->unkB0 = savedTank->unkB0;
        tank->unkB4 = savedTank->unkB4;
        tank->unkB8 = savedTank->unkB8;
        tank->unkBC = savedTank->unkBC;
        tank->unkC0 = savedTank->unkC0;
        tank->is_firing_tank = savedTank->is_firing_tank;
        tank->turret_vertical_angle = savedTank->turret_vertical_angle;
        tank->turret_orientation_angle = savedTank->turret_orientation_angle;
        tank->unkD0 = savedTank->unkD0;
        tank->stan_y = savedTank->stan_y;
        tank->unkD8 = savedTank->unkD8;
        tank->tank_orientation_angle = savedTank->tank_orientation_angle;
        break;
      }
      default:
        break;
      }
      break;
    }

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
  }

  if (ADD_AND_REMOVE_PROPS) {
    // Clear any props which were not saved
    for (c = nextIndexToRemove; c < POS_DATA_ENTRY_LEN; c++) {
      removePropAtIndex(c);
    }

    ptr_obj_pos_list_first_entry = get_prop_by_index(hdr.indexOfFirstEntry);
    ptr_obj_pos_list_current_entry = get_prop_by_index(hdr.indexOfCurrentEntry);
    ptr_obj_pos_list_final_entry = get_prop_by_index(hdr.indexOfFinalEntry);
  }

  /* Restore projectile snapshots. */
  for (pi = 0; pi < PROJECTILES_ARR_MAX; pi++) {
    storage_read(cur, &work->projectile, sizeof(work->projectile));
    g_Projectiles[work->projectile.index] = work->projectile.projectile;
  }

  /* Restore embedment snapshots. */
  for (pi = 0; pi < EMBEDMENT_ARR_MAX; pi++) {
    storage_read(cur, &work->embedment, sizeof(work->embedment));
    g_Embedments[work->embedment.index] = work->embedment.embedment;
    {
      s32 projIdx = (s32)g_Embedments[work->embedment.index].projectile;
      if (projIdx >= 0 && projIdx < PROJECTILES_ARR_MAX) {
        g_Embedments[work->embedment.index].projectile =
            &g_Projectiles[projIdx];
      }
    }
  }

  /* Fix up projectile/embedment back-pointers for restored props.
     Re-read the prop records from storage to get the saved object indices. */
  {
    StorageCursor fixupCur;
    s32 fi;

    storage_cursor_init(&fixupCur, dataStart);
    for (fi = 0; fi < hdr.recordCount; fi++) {
      SavedPropRecord *savedProp = &work->propRecord.prop;
      storage_read(&fixupCur, savedProp, sizeof(SavedPropRecord));

      if (savedProp->byteSize > sizeof(SavedPropRecord)) {
        storage_read(&fixupCur, &work->propRecord.type,
                     savedProp->byteSize - sizeof(SavedPropRecord));
      }

      if (savedProp->type == PROP_TYPE_OBJ) {
        PropRecord *prop = get_prop_by_index(savedProp->index);
        if (prop != NULL && prop->obj != NULL) {
          ObjectRecord *obj = prop->obj;
          SavedObjectRecord *savedObj =
              (SavedObjectRecord *)&work->propRecord.type;
          if (savedObj->projectileIdx >= 0 &&
              savedObj->projectileIdx < PROJECTILES_ARR_MAX) {
            obj->projectile = &g_Projectiles[savedObj->projectileIdx];
            g_Projectiles[savedObj->projectileIdx].obj = obj;
            g_Projectiles[savedObj->projectileIdx].ownerprop = prop;
          }
          if (savedObj->embedmentIdx >= 0 &&
              savedObj->embedmentIdx < EMBEDMENT_ARR_MAX) {
            obj->embedment = &g_Embedments[savedObj->embedmentIdx];
          }
        }
      }
    }
  }

  return TRUE;
}
