# Prop System State Save/Load

This document logs our findings, analysis, and specifications for GoldenEye 007's prop system and how to save/load the states of various props.

## Prop System Overview

In GoldenEye 007, everything in the level that is not static level geometry (the room meshes) is represented as a **prop**. This includes characters (guards, scientists, Bond), doors, ammunition crates, weapons on the ground, surveillance cameras, drone guns, body armor, and even transient entities like explosions and smoke.

### Active Props List

- Props are globally allocated in a flat array:
    ```c
    PropRecord pos_data_entry[POS_DATA_ENTRY_LEN]; // POS_DATA_ENTRY_LEN = 600
    ```
- Active props in the world are chained in a doubly linked list using the `prev` and `next` pointers within the `PropRecord` structure.
- The entry points to this linked list are defined as:
    ```c
    extern PropRecord *ptr_obj_pos_list_first_entry; // Head
    extern PropRecord *ptr_obj_pos_list_current_entry; // Tail
    ```
- A prop is considered active/in-use if the `PROPFLAG_ENABLED` (value `0x00000004`) flag is set in its `flags` byte.

---

## Primary Structures

### `PropRecord`

The container struct representing any prop in the 3D game world.

```c
typedef struct PropRecord
{
    u8  type;        /* 0x00 - PROP_TYPE enum */
    u8  flags;       /* 0x01 - Bitflags:
                      *   0x01: PROPFLAG_RENDERPOSTBG (Render in foreground)
                      *   0x02: PROPFLAG_ONSCREEN     (Prop currently visible on-screen)
                      *   0x04: PROPFLAG_ENABLED      (Prop active/in-use in the world)
                      */
    s16 timetoregen; // 0x02 - Respawn ticks remaining (used for weapons/ammo in multiplayer/resets)
    union
    {
        struct ChrRecord       *chr;
        struct ObjectRecord    *obj;
        struct DoorRecord      *door;
        struct WeaponObjRecord *weapon;
        struct Explosion       *explosion;
        struct Smoke           *smoke;
        struct Scorch          *scorch;
        void                   *voidp;
    };                       /* 0x04 - Union pointing to the underlying entity type */
    coord3d            pos;  /* 0x08 - 3D world position in level units.
                              *        Restored for CHRs with their movement/model state. */
    StandTile         *stan; /* 0x14 - StandTile containing the prop, or NULL.
                              *        Serialized as a same-level byte offset, never an address. */
    f32                zDepth;                          /* 0x18 - Render depth sorting value */
    struct PropRecord *parent;                          /* 0x1c - Attachment owner, or NULL for a root/
                                                         *        active-list prop. CHR equipment points
                                                         *        back to the owning character prop. */
    struct PropRecord *child;                           /* 0x20 - Newest child in the attachment sibling
                                                         *        chain, or NULL when there are no children. */
    struct PropRecord *prev;                            /* 0x24 - Previous active prop when parent is NULL;
                                                         *        previous attachment sibling otherwise. */
    struct PropRecord *next;                            /* 0x28 - Next active prop when parent is NULL;
                                                         *        next attachment sibling otherwise. */
    u8                 rooms[PROPRECORD_STAN_ROOM_LEN]; /* 0x2c - Up to three room IDs (0-254), followed
                                                         *        by 0xFF; all four bytes are serialized.
                                                         *        CHRs are deregistered before replacement
                                                         *        and registered in the restored rooms. */
    s32                unk30;                           /* 0x30 - Unused/Unknown. */
} PropRecord;
```

### Prop Types (`PROP_TYPE`)

Each `PropRecord` defines its entity type in its `type` field:

- `PROP_TYPE_NUL` (0): Null / unused.
- `PROP_TYPE_OBJ` (1): Interactive game objects (crates, armor, computers, etc. - uses `obj`). Save/load implemented.
- `PROP_TYPE_DOOR` (2): Doors (uses `door`). Save/load implemented.
- `PROP_TYPE_CHR` (3): Characters/NPCs (uses `chr`). Partial save/load
  implemented for the initial scalar AI field set.
- `PROP_TYPE_WEAPON` (4): Weapon pickups, thrown weapons, and mines in the world (uses `weapon`). Save/load implemented.
- `PROP_TYPE_PLAYER` (5): Player (uses `chr`).
- `PROP_TYPE_VIEWER` (6): Multi-player/cutscene viewer (uses `chr`).
- `PROP_TYPE_EXPLOSION` (7): Active explosion effects (uses `explosion`). Save/load implemented.
- `PROP_TYPE_SMOKE` (8): Smoke clouds (uses `smoke`). Save/load implemented.

---

## Entity Records

### `PropDefHeaderRecord`

The header structure embedded at the beginning of all entity definition records (objects, doors, weapons, etc.).

```c
typedef struct PropDefHeaderRecord
{
    u16 extrascale; /* 0x00 - Scale multiplier in fixed-point u8.8 format (e.g. 0x0380 = 3.5x scale) */
    u8  state;      /* 0x02 - Bitflags tracking the damage, destruction, and interactive state:
                     *   0x01: PROPSTATE_DAMAGED (Object has taken damage)
                     *   0x04: PROPSTATE_RESPAWN (Object respawning enabled in multiplayer)
                     *   0x08: PROPSTATE_EXT_COLISION_BLOCK (Externally allocated collision block present)
                     *   0x40: PROPSTATE_ACTIVATED (Console/datathief/decoder active or triggered)
                     *   0x80: PROPSTATE_DESTROYED (Object is completely destroyed)
                     */
    u8  type;       /* 0x03 - Category type ID used by level preset loaders */
} PropDefHeaderRecord;
```

---

### 1. `ObjectRecord` (PROP_TYPE_OBJ)

The base record for interactive stage objects. Note that many other specific object types (doors, monitors, keycards, drone guns) extend/inherit `ObjectRecord` anonymously.

```c
typedef struct ObjectRecord
{
    inherits    PropDefHeaderRecord;              /* 0x00 - Unnamed header struct (type, state, scale) */
    s16         obj;                              /* 0x04 - Prop definition/model ID enum (e.g. PROP_ALARM1) */
    s16         pad;                              /* 0x06 - The stage pad index this object is placed at */
    u32         flags;                            /* 0x08 - Object setup/configuration flag bits:
                                                   *   0x80000000: Right-handed gun assignment / Doors: open by default
                                                   *   0x40000000: Weapon has no ammo on pickup / Doors: area behind door always visible
                                                   *   0x20000000: Object in motion / Doors: open backwards
                                                   *   0x10000000: Left-handed pickup / Doors: same as 0
                                                   *   0x08000000: Doors: always open away from player
                                                   *   0x02000000: Doors: player cannot manually activate
                                                   *   0x00400000: Immobile (no physics movement)
                                                   *   0x00100000: Uncollectable
                                                   *   0x00040000: Allows object pickup (chr_name objects only)
                                                   *   0x00020000: Invincibility
                                                   *   0x00008000: Contained inside another object (e.g. key in safe)
                                                   *   0x00004000: Object assigned to guard preset
                                                   *   0x00001000: Force absolute position
                                                   *   0x00000800: Free-standing glass
                                                   *   0x00000100: Force collisions
                                                   *   0x00000080 (0xB byte - 2xxx presets): Z set to preset bounds
                                                   *   0x00000040 (0xB byte - 2xxx presets): Y set to preset bounds
                                                   *   0x00000020 (0xB byte - 2xxx presets): X set to preset bounds
                                                   *   0x00000010 (0xB byte - 2xxx presets): Scale object to fit preset bounds
                                                   *   0x00000008 (0xB byte): 0xxx presets: upper limit, right side up / 2xxx: in-air
                                                   *   0x00000004 (0xB byte): 0xxx presets: upper limit, upside-down / 2xxx: upside-down
                                                   *   0x00000002 (0xB byte): 0xxx presets: upper limit, rotated Y 90 / 2xxx: rotated Y 90
                                                   *   0x00000001 (0xB byte): 0xxx presets: forced to ground / 2xxx: normal placement
                                                   */
    u32         flags2;                           /* 0x0C - Additional object setup flags (PROPFLAG2):
                                                   *   0x80000000: Alt coordinate system / rotating disabled drone gun
                                                   *   0x40000000: Deactivate special feature
                                                   *   0x20000000: Character AI cannot operate/open door
                                                   *   0x10000000: One-way lock (back side)
                                                   *   0x08000000: One-way lock (front side)
                                                   *   0x01000000: Don't load on 4-player multiplayer
                                                   *   0x00800000: Don't load on 3-player multiplayer
                                                   *   0x00400000: Don't load on 2-player multiplayer
                                                   *   0x00200000: Immune to explosions
                                                   *   0x00100000: Bulletproof glass
                                                   *   0x00008000: Can shoot/bullet penetrate through object
                                                   *   0x00004000: Immune to gunfire (explosives damage only)
                                                   *   0x00002000: Remove from game completely when destroyed
                                                   *   0x00000800: Only activate/interact at close range
                                                   *   0x00000080: Don't load on 007 difficulty
                                                   *   0x00000040: Don't load on 00 Agent difficulty
                                                   *   0x00000020: Don't load on Secret Agent difficulty
                                                   *   0x00000010: Don't load on Agent difficulty
                                                   *   0x00000008: Don't load in multiplayer games
                                                   *   0x00000004: Disable interaction text prompts (e.g. doors)
                                                   *   0x00000002: Lightweight (moves when shot / drops when destroyed)
                                                   */
    PropRecord *prop;                             /* 0x10 - Pointer back to the parent PropRecord */
    Model      *model;                            /* 0x14 - Pointer to the 3D visual Model instance */
    Mtxf        mtx;                              /* 0x18 - 4x4 scale and rotation matrix */
    coord3d     runtime_pos;                      /* 0x58 - Dynamic position updated at runtime */
    union {
        u32     runtime_bitflags;                 /* 0x64 - Runtime state:
                                                   *   0x00000004: RUNTIMEBITFLAG_REMOVE (marked for cleanup)
                                                   *   0x00000008: RUNTIMEBITFLAG_ISRETICK (run physics twice per tick)
                                                   *   0x00000010: RUNTIMEBITFLAG_TAGGED (armor/objective tracker)
                                                   *   0x00000040: RUNTIMEBITFLAG_EMBEDDED (stuck in wall/ceiling)
                                                   *   0x00000080: RUNTIMEBITFLAG_DEPOSIT (thrown weapon/item pickup)
                                                   *   0x00000200: RUNTIMEBITFLAG_BEENOPENED (door opened once)
                                                   *   0x00000400: RUNTIMEBITFLAG_DESTROYED (door/object destroyed)
                                                   *   0x00002000: RUNTIMEBITFLAG_PADLOCKEDDOOR (door is padlocked)
                                                   *   0x00004000: RUNTIMEBITFLAG_ACTIVATED (alarm sounding, mainframe online)
                                                   *   0x00060000: RUNTIMEBITFLAG_OWNER (2-bit owner player ID 0-3, shifted via >> 17)
                                                   *   0x00080000: RUNTIMEBITFLAG_HASOWNER (owner ID is assigned)
                                                   */
    };
    struct collision_data *ptr_allocated_collisiondata_block; /* 0x68 - Custom collision bounding box */
    union {
        struct Projectile *projectile;            /* 0x6C - Projectile trajectory info (if active/sticky) */
        struct Embedment  *embedment;             /* 0x6C - Wall attachment links for active projectiles */
    };
    f32         maxdamage;                        /* 0x70 - Initial maximum health points */
    f32         damage;                           /* 0x74 - Current health points / damage received */
    rgba_u8     shadecol;                         /* 0x78 - Current visual color shading */
    rgba_u8     nextcol;                          /* 0x7C - Target shading color for transition */
} ObjectRecord;
```

---

### 2. `DoorRecord` (PROP_TYPE_DOOR)

Handles door opening, closing, locks, double doors, and automated triggers. Extends `ObjectRecord`.

```c
typedef struct DoorRecord
{
    inherits           ObjectRecord;              /* 0x00 - Inner ObjectRecord base structure */
    s32                linkedDoorOffset;          /* 0x80 - Relative offset index to double door sibling */
    f32                maxFrac;                   /* 0x84 - Maximum displacement travel distance (total %) */
    f32                perimFrac;                 /* 0x88 - Travel percentage before disabling collision */
    f32                accel;                     /* 0x8C - Starting acceleration rate */
    f32                decel;                     /* 0x90 - Slowing down deceleration rate */
    f32                maxSpeed;                  /* 0x94 - Maximum movement speed per tick */
    u16                doorFlags;                 /* 0x98 - Configuration flags (sliding, shutter, etc.) */
    u16                doorType;                  /* 0x9A - Sub-type styling identifiers */
    u32                keyflags;                  /* 0x9C - Item ID required to unlock */
    u32                autoCloseFrames;           /* 0xA0 - Duration (in frames) the door stays open */
    u32                doorOpenSound;             /* 0xA4 - SFX ID assigned to opening */
    f32                frac;                      /* 0xA8 - Current open fractional percentage (0.0 - 1.0) */
    f32                unkac;                     /* 0xAC */
    f32                unkb0;                     /* 0xB0 */
    f32                openPosition;              /* 0xB4 - Travel offset calculation value */
    f32                speed;                     /* 0xB8 - Dynamic speed of opening/closing */
    s8                 openstate;                 /* 0xBC - DOORSTATE enum (closed, opening, open, closing) */
    u8                 unkbd;                     /* 0xBD */
    s16                calculatedopacity;         /* 0xBE - Visually applied fog opacity */
    s32                TintDist;                  /* 0xC0 */
    s16                CullDist;                  /* 0xC4 - Back-plane distance to cull */
    s8                 soundType;                 /* 0xC6 */
    s8                 fadeTime60;                /* 0xC7 */
    struct DoorRecord *linkedDoor;                /* 0xC8 - Pointer to linked sibling DoorRecord */
    Vertex*            unkcc;                     /* 0xCC - Vertices list cache */
    struct ModelRoData_BoundingBoxRecord bbox;    /* 0xD0 - Bounding box geometry definition */
    u32                openedTime;                /* 0xEC - Global timer tick when door fully opened */
    s32                portalNumber;              /* 0xF0 - Visportal identifier number */
    ALSoundState      *openSoundState;            /* 0xF4 - Active opening audio node pointer */
    ALSoundState      *closeSoundState;           /* 0xF8 - Active closing audio node pointer */
    union {
        s32            lastcalc60i;               /* 0xFC - Frame update check verification integer */
        f32            lastcalc60f;               /* 0xFC - Frame update check verification float */
    };
} DoorRecord;
```

---

### 3. `WeaponObjRecord` (PROP_TYPE_WEAPON) — Save/load implemented

A weapon or ammunition container resting on the ground, held by a character, or active as a thrown weapon or mine. Extends `ObjectRecord`. Top-level `PROP_TYPE_WEAPON` records use `PROPDEF_COLLECTABLE` as their object-definition type.

```c
typedef struct WeaponObjRecord
{
    inherits                ObjectRecord;         /* 0x00 - Inner ObjectRecord base structure */
    s8                      weaponnum;            /* 0x80 - ITEM_IDS value identifying the weapon/item.
                                                   * Values range from ITEM_UNARMED (0) through the
                                                   * weapon, gadget, key-item, and token IDs. */
    s8                      LinkedWeaponType;     /* 0x81 - ITEM_IDS value for the other weapon in a
                                                   * dual-wield pair. The game uses this when two
                                                   * weapon objects are linked so collecting them
                                                   * grants/records a dual-wield combination. A
                                                   * negative value means no linked weapon type. */
    s16                     timer;                /* 0x82 - Runtime countdown/state for thrown weapons.
                                                   * Grenades and timed mines count down to detonation;
                                                   * remote/proximity mines use 1 for armed, 0 to
                                                   * detonate, and -1 after detonation is requested. */
    struct WeaponObjRecord *dualweapon;           /* 0x84 - The other live weapon object in that
                                                   * dual-wield pair, or NULL.
                                                   * Serialized as its PropRecord array index and
                                                   * resolved back to the live WeaponObjRecord. */
} WeaponObjRecord;
```

The serializer also restores the inherited `ObjectRecord`, including transform, damage, model-switch visibility, projectile/embedment indices, and runtime flags. This preserves dropped or thrown weapon position and motion. `dualweapon` is not saved as an address because the address can become stale; it is converted to a stable prop index.

---

### 4. `ChrRecord` (PROP_TYPE_CHR)

The active runtime character entity representing guards, scientists, key targets, and multiplayer opponents.
The action-union and other supporting structs, pointer hazards, and the
conservative first field set selected for implementation are documented in
[`CHR.md`](CHR.md).

```c
typedef struct ChrRecord
{
    s16         chrnum;                           /* 0x0000 - Runtime character ID used by AI lookups;
                                                   *          normally level-assigned, but scripts can replace it */
    s8          accuracyrating;                   /* 0x0002 - Signed shooting-accuracy adjustment
                                                   *          (-128..127; scripts normally use rating values) */
    s8          speedrating;                      /* 0x0003 - Signed reaction/movement-speed adjustment
                                                   *          (-128..127; scripts normally use rating values) */
    u8          firecount[2];                     /* 0x0004 - Shot count tracking for each hand */
    s8          headnum;                          /* 0x0006 - Head mesh configuration index */
    ACT_TYPE    actiontype : 8;                   /* 0x0007 - Active behavior action state (ACT_TYPE) */
    s8          sleep;                            /* 0x0008 - Signed tick delay before the next AI/action
                                                   *          update; normally 0-127, decremented each tick */
    s8          invalidmove;                      /* 0x0009 - Movement failure flag: 0 means the last
                                                   *          attempted move was valid; 1 makes travel
                                                   *          actions reconsider or stop their route */
    s8          numclosearghs;                    /* 0x000A - AI-readable near-miss/close-hit counter;
                                                   *          initialized to 0 (exact increment path TODO) */
    s8          numarghs;                         /* 0x000B - Confirmed damage-reaction counter;
                                                   *          initialized to 0 and incremented on hit reactions */
    u8          fadealpha;                        /* 0x000C - Alpha blending for dying/spawning chrs */
    s8          arghrating;                       /* 0x000D - Signed damage-reaction-speed adjustment
                                                   *          (-128..127; scripts normally use rating values) */
    s8          aimendcount;                      /* 0x000E */
    s8          bodynum;                          /* 0x000F - Body mesh configuration index */
    u8          grenadeprob;                      /* 0x0010 - Grenade-attempt threshold, 0-255.
                                                   *          AI compares it with randomGetNext() % 255. */
    s8          flinchcnt;                        /* 0x0011 - Render-flinch phase; -1 is inactive,
                                                   *          1 through 29 are active NTSC phases */
    u16         hidden;                           /* 0x0012 - Visibility state tags (CHRHIDDEN):
                                                   *   0x0001: CHRHIDDEN_DROP_HELD_ITEMS (force drop items/weapons)
                                                   *   0x0002: CHRHIDDEN_ALERT_GUARD_RELATED (alerted guard state)
                                                   *   0x0004: CHRHIDDEN_FIRE_WEAPON_LEFT (firing left hand weapon)
                                                   *   0x0008: CHRHIDDEN_FIRE_WEAPON_RIGHT (firing right hand weapon)
                                                   *   0x0010: CHRHIDDEN_OFFSCREEN_PATROL (cheap pathing update offscreen)
                                                   *   0x0020: CHRHIDDEN_REMOVE (delete character on next tick)
                                                   *   0x0040: CHRHIDDEN_TIMER_ACTIVE (character behavior timer active)
                                                   *   0x0080: CHRHIDDEN_FIRE_TRACER (spawn weapon tracers)
                                                   *   0x0100: CHRHIDDEN_MOVING (character is traversing tiles)
                                                   *   0x0200: CHRHIDDEN_BACKGROUND_AI (AI cmd command routine running)
                                                   *   0x0800: CHRHIDDEN_FREEZE (freeze current animation pose)
                                                   *   0x1000: CHRHIDDEN_RAND_FLINCH_1 (random flinch bit 1)
                                                   *   0x2000: CHRHIDDEN_RAND_FLINCH_2 (random flinch bit 2)
                                                   *   0x4000: CHRHIDDEN_RAND_FLINCH_4 (random flinch bit 4)
                                                   *   0x8000: CHRHIDDEN_RAND_FLINCH_8 (random flinch bit 8)
                                                   */
    CHRFLAG     chrflags;                         /* 0x0014 - Main status flag bits (CHRFLAG):
                                                   *   0x00000001: CHRFLAG_INIT (initialize chr)
                                                   *   0x00000002: CHRFLAG_CLONE (clone on heard gunfire)
                                                   *   0x00000004: CHRFLAG_NEAR_MISS (resets every frame)
                                                   *   0x00000008: CHRFLAG_HAS_BEEN_ON_SCREEN (seen by Bond)
                                                   *   0x00000010: CHRFLAG_INVINCIBLE (takes no damage)
                                                   *   0x00000020: CHRSTART_FORCENOBLOOD (disables blood effects)
                                                   *   0x00000040: CHRFLAG_CAN_SHOOT_CHRS (can shoot other guards)
                                                   *   0x00000100: CHRFLAG_WAS_DAMAGED (taken damage, not invincible)
                                                   *   0x00000400: CHRFLAG_HIDDEN (disabled rendering & tracking)
                                                   *   0x00000800: CHRFLAG_NO_AUTOAIM (ignores magnetic autoaim)
                                                   *   0x00001000: CHRFLAG_LOCK_Y_POS (no gravity, e.g. bungee)
                                                   *   0x00002000: CHRFLAG_NO_SHADOW (no drop shadow rendered)
                                                   *   0x00004000: CHRFLAG_IGNORE_ANIM_TRANSLATION (static root joint)
                                                   *   0x00008000: CHRFLAG_IMPACT_ALWAYS (allows pushback/cradle falls)
                                                   *   0x00080000: CHRFLAG_INCREASE_RUNNING_SPEED (boss speed boost)
                                                   *   0x00100000: CHRFLAG_COUNT_DEATH_AS_CIVILIAN (civilian casualty)
                                                   *   0x00200000: CHRFLAG_WAS_HIT (hit even if invincible)
                                                   *   0x00800000: CHRFLAG_CULL_USING_HITBOX (cull using hitbox boundaries)
                                                   */
    PropRecord *prop;                             /* 0x0018 - Pointer to container PropRecord */
    Model      *model;                            /* 0x001C - Pointer to dynamic Model geometry instance */
    void       *field_20;                         /* 0x0020 - Head of a dynamically allocated skeletal
                                                   *          joint/matrix render list. Returned to its
                                                   *          global pool and cleared on load; the next
                                                   *          render reconstructs it from the live model. */
    f32         chrwidth;                         /* 0x0024 - Width of character collision cylinder */
    f32         chrheight;                        /* 0x0028 - Height of character collision cylinder */
    union {
        struct act_init         act_init;         /* 0x002C - Action initialisation parameters */
        struct act_stand        act_stand;        /* 0x002C - Standing AI data */
        struct act_kneel        act_kneel;        /* 0x002C - Kneeling/Crouching data */
        struct act_anim         act_anim;         /* 0x002C - Scripted animation state data */
        struct act_die          act_die;          /* 0x002C - Dying transition sequence data */
        struct act_dead         act_dead;         /* 0x002C - Post-mortem state parameters */
        struct act_argh         act_argh;         /* 0x002C - Standard damage flinch tracking */
        struct act_preargh      act_preargh;      /* 0x002C - Flashing response parameters */
        struct act_attack       act_attack;       /* 0x002C - Weapons shooting details */
        struct act_attackwalk   act_attackwalk;   /* 0x002C - Shoot while moving parameters */
        struct act_attackroll   act_attackroll;   /* 0x002C - Evading/rolling and shooting */
        struct act_sidestep     act_sidestep;     /* 0x002C - Sidestepping and dodging */
        struct act_jumpout      act_jumpout;      /* 0x002C - Jumping out of cover */
        struct act_runpos       act_runpos;       /* 0x002C - Relocation action targets */
        struct act_patrol       act_patrol;       /* 0x002C - Waypoint patrolling path structures */
        struct act_gopos        act_gopos;        /* 0x002C - Pathfinding coordinates & queues */
        struct act_surrender    act_surrender;    /* 0x002C - Unarmed surrender state */
        struct act_lookattarget act_lookattarget; /* 0x002C - Camera focus behavior tracking */
        struct act_surprised    act_surprised;    /* 0x002C - Alarm/reaction flags */
        struct act_startalarm   act_startalarm;   /* 0x002C - Running to alarm panel structures */
        struct act_throwgrenade act_throwgrenade; /* 0x002C - Weapon throwing data */
        struct act_turndir      act_turndir;      /* 0x002C - Rotating target adjustments */
        struct act_test         act_test;         /* 0x002C - Debug tests */
        struct act_bondintro    act_bondintro;    /* 0x002C - Cinematic camera intro actions */
        struct act_bonddie      act_bonddie;      /* 0x002C - Cinematic player death actions */
        struct act_bondmulti    act_bondmulti;    /* 0x002C - Multiplayer player model animation */
        struct act_null         act_null;         /* 0x002C - Empty placeholder */
        struct act_bytes        act_bytes;        /* 0x002C - Raw buffer representation */
        struct act_ubytes       act_ubytes;       /* 0x002C - Unsigned raw buffer representation */
    };
    f32         sumground;                        /* 0x00A4 - Ground-height accumulation value,
                                                   *          initialized to 0.0; exact smoothing use TODO */
    f32         manground;                        /* 0x00A8 - Companion/manual ground-height value,
                                                   *          initialized to 0.0; exact use TODO */
    f32         ground;                           /* 0x00AC - World Y coordinate of the supporting floor;
                                                   *          initialized/recomputed from model collision */
    coord3d     fallspeed;                        /* 0x00B0 - X/Y/Z falling or impact velocity in level
                                                   *          units; each component initializes to 0.0 */
    coord3d     prevpos;                          /* 0x00BC - Model-root world position before the previous
                                                   *          animation/movement update */
    s32         lastwalk60;                       /* 0x00C8 - Absolute g_GlobalTimer tick of the last travel
                                                   *          update; 0 means no recorded walk */
    s32         lastmoveok60;                     /* 0x00CC - Absolute g_GlobalTimer tick of the last valid
                                                   *          move, used to detect a stuck route */
    f32         visionrange;                      /* 0x00D0 - Sight distance parameter in world units */
    s32         lastseetarget60;                  /* 0x00D4 - Absolute g_GlobalTimer tick when the
                                                   *          target was last seen; 0 means never */
    coord3d     lastknowntargetpos;               /* 0x00D8 - Last target position recorded by
                                                   *          either visual or auditory detection */
    void       *targetTile;                       /* 0x00E4 - StandTile containing the remembered
                                                   *          target position, or NULL */
    union {
        s32     seen_bond_time;                   /* 0x00E8 - Absolute g_GlobalTimer tick of the
                                                   *          last direct Bond sighting; 0 means never */
        struct {
            s16 lastshooter;                      /* 0x00E8 - ID of actor who last shot this guard */
            s16 timeshooter;                      /* 0x00EA - Shots count fired by shooter */
        };
    };
    f32         hearingscale;                     /* 0x00EC - Audio detection radius modifier */
    s32         lastheartarget60;                 /* 0x00F0 - Absolute g_GlobalTimer tick when the
                                                   *          target was last heard; 0 means never */
    rgba_u8     shadecol;                         /* 0x00F4 - Character base lighting colors */
    rgba_u8     nextcol;                          /* 0x00F8 - Shading target color for transition */
    f32         damage;                           /* 0x00FC - Current damage accumulated */
    f32         maxdamage;                        /* 0x0100 - Maximum damage tolerance health */
    AIRecord   *ailist;                           /* 0x0104 - Active static AI command list.
                                                   *          Serialized by list ID, never by address. */
    u16         aioffset;                         /* 0x0108 - Byte offset of the next command in ailist */
    s16         aireturnlist;                     /* 0x010A - Stable AI list ID used by AI_Return;
                                                   *          -1 means no return list */
    u8          morale;                           /* 0x010C - Morale check thresholds */
    u8          alertness;                        /* 0x010D - Active target detection alert level */
    u8          flags2;                           /* 0x010E - Complete secondary AI-script flag byte:
                                                   *   0x01: FLAGS2_DONT_POINT_AT_BOND
                                                   *   0x02: FLAGS2_02 (unknown/script-reserved)
                                                   *   0x04: FLAGS2_04 (persistent alarm AI state)
                                                   */
    u8          random;                           /* 0x010F - Latched AI random byte, 0-255.
                                                   *          Updated by AI_SetNewRandom and reused by branches. */
    s32         timer60;                          /* 0x0110 - Per-character AI timer, incremented by
                                                   *          g_ClockTimer while CHRHIDDEN_TIMER_ACTIVE is set */
    s16         padpreset1;                       /* 0x0114 - Concrete pad ID substituted for
                                                   *          PAD_PRESET1 (9000), or -1 when unset */
    s16         chrpreset1;                       /* 0x0116 - Concrete character ID substituted for
                                                   *          CHR_PRESET (-4), or -1 when unset */
    s16         chrseeshot;                       /* 0x0118 - ID of character seen being shot;
                                                   *          CHR_FREE (-1) means no pending event */
    s16         chrseedie;                        /* 0x011A - ID of character seen dying;
                                                   *          CHR_FREE (-1) means no pending event */
    rect4f      collision_bounds;                 /* 0x011C - Four X/Z points forming the CHR collision
                                                   *          diamond around prop->pos, normally at
                                                   *          chrwidth radius */
    f32         shotbondsum;                      /* 0x013C - Fractional accumulated guard-fire damage
                                                   *          against Bond; normally 0.0 <= value < 1.0 */
    f32         aimuplshoulder;                   /* 0x0140 - Arms rotation angles */
    f32         aimuprshoulder;                   /* 0x0144 */
    f32         aimupback;                        /* 0x0148 */
    f32         aimsideback;                      /* 0x014C */
    f32         aimendlshoulder;                  /* 0x0150 */
    f32         aimendrshoulder;                  /* 0x0154 */
    f32         aimendback;                       /* 0x0158 */
    f32         aimendsideback;                   /* 0x015C */
    PropRecord *weapons_held[3];                  /* 0x0160 - [0] right-hand weapon, [1] left-hand weapon,
                                                   *          [2] legacy/unused slot. Entries are PropRecord
                                                   *          pointers or NULL; serialized as prop indices. */
    union {
        s32     fireslot_word;                    /* 0x016C */
        s8      fireslot[2];                      /* 0x016C */
    };
    int        *ptr_SEbuffer3;                    /* 0x0170 - Active sound nodes buffers */
    int        *ptr_SEbuffer4;                    /* 0x0174 */
    int         field_178[2];                     /* 0x0178 */
    ChrRecord_f180 unk180[2];                     /* 0x0180 - Attack vector trajectory cache */
    PropRecord *handle_positiondata_hat;          /* 0x01CC - Hat PropRecord attached to the head switch,
                                                   *          or NULL; serialized as a prop index */
} ChrRecord;
```

The implemented save/load slices restore
`accuracyrating`, `speedrating`, `arghrating`, `grenadeprob`, `visionrange`,
`hearingscale`, `morale`, `alertness`, `numarghs`, `numclosearghs`, `random`,
`padpreset1`, `chrpreset1`, `chrseeshot`, `chrseedie`,
`lastseetarget60`, `lastknowntargetpos`, `targetTile`, `seen_bond_time`, and
`lastheartarget60`, `chrnum`, `flags2`, `timer60`, the
`CHRHIDDEN_TIMER_ACTIVE` bit, `flinchcnt`, the four
`CHRHIDDEN_RAND_FLINCH_MASK` direction bits, the durable behavior/render
`chrflags` subset documented in `CHR.md`, `shotbondsum`, combat action/aiming state,
`firecount`, `aimendcount`, all eight shoulder/back angles, `fireslot_word`,
`field_178`, both `unk180` beam caches, combat/moving hidden bits, `shadecol`,
and `nextcol`. Dynamic `ptr_SEbuffer3`/`ptr_SEbuffer4` sound nodes are stopped
and cleared rather than serialized.
The AI interpreter fields `ailist`, `aioffset`, `aireturnlist`, and `sleep`
are also restored. Spatial restoration covers the CHR prop's position, stand
tile, rooms, model root transform, movement history, ground state, and
collision bounds. Equipment restoration covers both hand slots, the legacy
third slot when already valid, the hat pointer, CHR-owned prop hierarchy,
model attachment nodes, and `RUNTIMEBITFLAG_HASOWNER`. `flags2` is restored
completely; the timer, firing, tracer, and moving subsets of `hidden` are
restored. `targetTile` is
relocated through a stable stand-tile offset, while `ailist` is relocated
through a stable AI list ID. See `CHR.md` for the fields explicitly deferred
and the gated replacement architecture.

---

### 5. `Explosion` (PROP_TYPE_EXPLOSION) — Save/load implemented

A short-lived explosion effect which expands its damage volume, creates and animates flare sprites, optionally leaves a scorch mark, and eventually creates a smoke prop. Explosion records come from the fixed six-entry `g_ExplosionBuffer`.

```c
struct ExplosionPart
{
    coord3d pos;   /* 0x00 - World position of this flare sprite */
    f32 size;      /* 0x0C - First axis of the sprite's rotated size vector */
    f32 rot;       /* 0x10 - Second axis of the sprite's rotated size vector */
    s16 frame;     /* 0x14 - Flare animation frame; 0 means inactive */
    u8  bb;        /* 0x16 - Unknown/unused in the current explosion code */
};

struct Explosion
{
    PropRecord          *prop;           /* 0x000 - Back-pointer to the live explosion prop */
    PropRecord          *source;         /* 0x004 - Prop which originated the explosion, or NULL */
    struct ExplosionPart parts[40];      /* 0x008 - Independently positioned flare sprites */
    s16                  age;            /* 0x3C8 - Elapsed explosion updates */
    s16                  unk3CA;         /* 0x3CA - Next age at which radial damage is applied */
    s8                   explosion_type; /* 0x3CC - EXPLOSION_DEF index 0-21 */
    s8                   unk3CD;         /* 0x3CD - Boolean: create a scorch mark at half-life */
    s8                   player;         /* 0x3CE - Player credited for damage/kills; normally 0-3 */
    s8                   unk3CF;         /* 0x3CF - Unknown/unused in the current explosion code */
    coord3d              pos;            /* 0x3D0 - Position used when creating the scorch mark */
    s16                  room;           /* 0x3DC - Room receiving the scorch mark */
    s16                  unk3DE;         /* 0x3DE - Unknown/unused in the current explosion code */
};
```

`explosion_type` indexes `g_ExplosionTypes`, which defines damage and visual ranges, growth rates, lifetime, flare animation speed, shrapnel, smoke type, sound, and damage. Named values include `EXPLOSION_DEF_DRONE` (12), `EXPLOSION_DEF_STANDARD` (13, grenades and mines), `EXPLOSION_DEF_MASSIVE` (17), `EXPLOSION_DEF_PLAYER` (18), and `EXPLOSION_DEF_FACILITY_REMOTE` (19).

The serializer restores every non-pointer field and all 40 `ExplosionPart` records. `Explosion::prop` is preserved because the fixed buffer entry and live prop already point to each other. `Explosion::source` is serialized as a stable prop-array index and restored only if that prop is still enabled; otherwise it becomes `NULL`, which safely uses the explosion prop as the fallback location when follow-up smoke is created.

The associated `g_NumExplosionEntries` and `g_NumSmokeEntries` counters are also restored. Both normally range from 0 to 6 and control the short camera-shake sequence triggered by an explosion.

An explosion present in the save but missing from the current world is recreated in its saved slot using a free `g_ExplosionBuffer` entry (`create_explosion_prop`); one present in the world but absent from the save is removed. Explosions that were mid-effect are restored in place.

---

### 6. `Smoke` (PROP_TYPE_SMOKE) — Save/load implemented

A transient smoke cloud produced by an explosion or by a damaged object. Smoke records come from the fixed 20-entry `g_SmokeBuffer`; the associated prop points to its buffer entry and supplies the cloud's world position and rooms.

```c
struct SmokePart
{
    coord3d pos;    /* 0x00 - World position of this smoke sprite */
    f32 size;       /* 0x0C - Sprite size; 0.0 means this part is inactive */
    f32 rot;        /* 0x10 - Current sprite rotation in radians */
    f32 deltarot;   /* 0x14 - Rotation added on each update */
    f32 offset1;    /* 0x18 - First animated texture/noise offset */
    f32 offset2;    /* 0x1C - Second animated texture/noise offset */
    f32 alpha;      /* 0x20 - Current opacity; the part expires below 4.0 */
    s16 count;      /* 0x24 - Number of updates since this part appeared */
};

struct Smoke
{
    PropRecord      *prop;                    /* 0x000 - Back-pointer to the live smoke prop */
    s16              duration;                /* 0x004 - Number of smoke updates elapsed */
    s16              smoke_type;              /* 0x006 - Index 0-10 into g_SmokeTypes */
    struct SmokePart parts[SMOKE_PARTS_LEN];  /* 0x008 - Ten independently animated sprites */
};
```

`smoke_type` selects the duration, spawn/dissolve rates, size, rotation rates, colour, and cloud propagation duration from `g_SmokeTypes`. Values 0-7 are selected by explosion definitions; 8 and 9 are short-lived effects produced by damaged objects; 10 is the large cloud created while the tank fires. `duration` normally runs from 0 through the selected type's configured lifetime. Each part's `size` is the active marker, while `alpha` and `count` control its fade.

The serializer restores `duration`, `smoke_type`, and all ten `SmokePart` values. It deliberately preserves `Smoke::prop` rather than serializing its address: the live smoke prop and its fixed buffer slot already point to each other, and restoring an address is unnecessary and unsafe. A smoke cloud present in the save but missing from the current world is recreated in its saved slot using a free `g_SmokeBuffer` entry (`create_smoke_prop`); one present in the world but absent from the save is removed.

## Save/Load Guidelines

1. **Mapping Pointers to Indices/Offsets**:
    - `PropRecord *` targets are resolved to stable indices via `get_prop_index(prop)`.
    - `StandTile *` targets are converted to byte offsets from `standTileStart` using `get_tile_offset(tile)`.
2. **Audio System Nullification**:
    - Active sound state nodes (`ALSoundState *`) are dynamically allocated. On reload, these pointer members must be set to `NULL` to prevent crashes when the sound engine tries to modify a dead address.
3. **List Integrity**:
    - Care must be taken not to alter list linkage (`prev`/`next`) directly unless allocating/deallocating a prop, as list order and pointers are crucial to the engine’s update loop.
4. **Projectile/Embedment Union Integrity**:
    - `ObjectRecord::projectile` and `ObjectRecord::embedment` share one union slot. Restore only `projectile` for `RUNTIMEBITFLAG_DEPOSIT`, only `embedment` for `RUNTIMEBITFLAG_EMBEDDED`, or clear the slot and both flags when the saved index is invalid.
5. **Deferred Projectile Reference Resolution**:
    - Read saved `Projectile::ownerprop` and `Projectile::obj` indices into separate temporary arrays rather than casting them to pointers. Resolve them only after all prop records have been processed. If the object prop was collected or removed and is no longer enabled, mark the projectile free instead of retaining an invalid reference.
