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
    coord3d            pos;  /* 0x08 - 3D world position coordinates */
    StandTile         *stan; /* 0x14 - Pointer to the StandTile the prop is resting on */
    f32                zDepth;                          /* 0x18 - Render depth sorting value */
    struct PropRecord *parent;                          /* 0x1c - Parent pointer in attachment hierarchy */
    struct PropRecord *child;                           /* 0x20 - Child pointer in attachment hierarchy */
    struct PropRecord *prev;                            /* 0x24 - Previous prop in active list */
    struct PropRecord *next;                            /* 0x28 - Next prop in active list */
    u8                 rooms[PROPRECORD_STAN_ROOM_LEN]; /* 0x2c - Rooms occupied by this prop (0xFF terminated) */
    s32                unk30;                           /* 0x30 - Unused/Unknown. */
} PropRecord;
```

### Prop Types (`PROP_TYPE`)

Each `PropRecord` defines its entity type in its `type` field:

- `PROP_TYPE_NUL` (0): Null / unused.
- `PROP_TYPE_OBJ` (1): Interactive game objects (crates, armor, computers, etc. - uses `obj`).
- `PROP_TYPE_DOOR` (2): Doors (uses `door`).
- `PROP_TYPE_CHR` (3): Characters/NPCs (uses `chr`).
- `PROP_TYPE_WEAPON` (4): Weapon pickups (uses `weapon`).
- `PROP_TYPE_PLAYER` (5): Player (uses `chr`).
- `PROP_TYPE_VIEWER` (6): Multi-player/cutscene viewer (uses `chr`).
- `PROP_TYPE_EXPLOSION` (7): Explosions (uses `explosion`).
- `PROP_TYPE_SMOKE` (8): Smoke particles (uses `smoke`).

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

### 3. `WeaponObjRecord` (PROP_TYPE_WEAPON)

A weapon or ammunition container resting on the ground, collectable by Bond or NPCs. Extends `ObjectRecord`.

```c
typedef struct WeaponObjRecord
{
    inherits                ObjectRecord;         /* 0x00 - Inner ObjectRecord base structure */
    s8                      weaponnum;            /* 0x80 - Item ID index (ITEM_IDS enum) */
    s8                      LinkedWeaponType;     /* 0x81 - Paired weapon type */
    s16                     timer;                /* 0x82 - Transient/disappearing timer value */
    struct WeaponObjRecord *dualweapon;           /* 0x84 - Companion weapon for dual pickups */
} WeaponObjRecord;
```

---

### 4. `ChrRecord` (PROP_TYPE_CHR)

The active runtime character entity representing guards, scientists, key targets, and multiplayer opponents.

```c
typedef struct ChrRecord
{
    s16         chrnum;                           /* 0x0000 - Index of this guard in level registry */
    s8          accuracyrating;                   /* 0x0002 - Accuracy parameter (0-100) */
    s8          speedrating;                      /* 0x0003 - Speed/Movement scaling parameter */
    u8          firecount[2];                     /* 0x0004 - Shot count tracking for each hand */
    s8          headnum;                          /* 0x0006 - Head mesh configuration index */
    ACT_TYPE    actiontype : 8;                   /* 0x0007 - Active behavior action state (ACT_TYPE) */
    s8          sleep;                            /* 0x0008 - Ticks to sleep/idle before activation */
    s8          invalidmove;                      /* 0x0009 - Collision/unreachable path flag */
    s8          numclosearghs;                    /* 0x000A - Damage animation tracking */
    s8          numarghs;                         /* 0x000B - Total damage reaction calls */
    u8          fadealpha;                        /* 0x000C - Alpha blending for dying/spawning chrs */
    s8          arghrating;                       /* 0x000D */
    s8          aimendcount;                      /* 0x000E */
    s8          bodynum;                          /* 0x000F - Body mesh configuration index */
    u8          grenadeprob;                      /* 0x0010 - Percentage chance of throwing grenade */
    s8          flinchcnt;                        /* 0x0011 */
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
    void       *field_20;                         /* 0x0020 - Pathfinding navigation buffer */
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
    f32         sumground;                        /* 0x00A4 */
    f32         manground;                        /* 0x00A8 */
    f32         ground;                           /* 0x00AC - Current height above level ground floor */
    coord3d     fallspeed;                        /* 0x00B0 - 3D velocity vectors (used for gravity/knocks) */
    coord3d     prevpos;                          /* 0x00BC - Position in previous update frame */
    s32         lastwalk60;                       /* 0x00C8 */
    s32         lastmoveok60;                     /* 0x00CC */
    f32         visionrange;                      /* 0x00D0 - Sight distance parameter in world units */
    s32         lastseetarget60;                  /* 0x00D4 - Global timer when target was last seen */
    coord3d     lastknowntargetpos;               /* 0x00D8 - Position vector of last seen target location */
    void       *targetTile;                       /* 0x00E4 - Navmesh tile of target */
    union {
        s32     seen_bond_time;                   /* 0x00E8 - Timer since target detection */
        struct {
            s16 lastshooter;                      /* 0x00E8 - ID of actor who last shot this guard */
            s16 timeshooter;                      /* 0x00EA - Shots count fired by shooter */
        };
    };
    f32         hearingscale;                     /* 0x00EC - Audio detection radius modifier */
    s32         lastheartarget60;                 /* 0x00F0 - Global timer when target was last heard */
    rgba_u8     shadecol;                         /* 0x00F4 - Character base lighting colors */
    rgba_u8     nextcol;                          /* 0x00F8 - Shading target color for transition */
    f32         damage;                           /* 0x00FC - Current damage accumulated */
    f32         maxdamage;                        /* 0x0100 - Maximum damage tolerance health */
    AIRecord   *ailist;                           /* 0x0104 - Pointer to the AI action routine list */
    u16         aioffset;                         /* 0x0108 - Active instruction offset in AI routine */
    s16         aireturnlist;                     /* 0x010A - Return address stack offset for AI list call */
    u8          morale;                           /* 0x010C - Morale check thresholds */
    u8          alertness;                        /* 0x010D - Active target detection alert level */
    u8          flags2;                           /* 0x010E - Secondary flags:
                                                   *   0x01: FLAGS2_DONT_POINT_AT_BOND (AI doesn't raise gun at player)
                                                   */
    u8          random;                           /* 0x010F - Random seed value refreshed regularly */
    s32         timer60;                          /* 0x0110 */
    s16         padpreset1;                       /* 0x0114 - Preset path pad */
    s16         chrpreset1;                       /* 0x0116 - Preset target character ID */
    s16         chrseeshot;                       /* 0x0118 */
    s16         chrseedie;                        /* 0x011A */
    rect4f      collision_bounds;                 /* 0x011C - Bounding box boundaries (4 coordinate pairs) */
    f32         shotbondsum;                      /* 0x013C */
    f32         aimuplshoulder;                   /* 0x0140 - Arms rotation angles */
    f32         aimuprshoulder;                   /* 0x0144 */
    f32         aimupback;                        /* 0x0148 */
    f32         aimsideback;                      /* 0x014C */
    f32         aimendlshoulder;                  /* 0x0150 */
    f32         aimendrshoulder;                  /* 0x0154 */
    f32         aimendback;                       /* 0x0158 */
    f32         aimendsideback;                   /* 0x015C */
    PropRecord *weapons_held[3];                  /* 0x0160 - Pointers to right, left, and hat props held */
    union {
        s32     fireslot_word;                    /* 0x016C */
        s8      fireslot[2];                      /* 0x016C */
    };
    int        *ptr_SEbuffer3;                    /* 0x0170 - Active sound nodes buffers */
    int        *ptr_SEbuffer4;                    /* 0x0174 */
    int         field_178[2];                     /* 0x0178 */
    ChrRecord_f180 unk180[2];                     /* 0x0180 - Attack vector trajectory cache */
    PropRecord *handle_positiondata_hat;          /* 0x01CC - Pointer to hat prop definition */
} ChrRecord;
```

## Save/Load Guidelines

1. **Mapping Pointers to Indices/Offsets**:
    - `PropRecord *` targets are resolved to stable indices via `get_prop_index(prop)`.
    - `StandTile *` targets are converted to byte offsets from `standTileStart` using `get_tile_offset(tile)`.
2. **Audio System Nullification**:
    - Active sound state nodes (`ALSoundState *`) are dynamically allocated. On reload, these pointer members must be set to `NULL` to prevent crashes when the sound engine tries to modify a dead address.
3. **List Integrity**:
    - Care must be taken not to alter list linkage (`prev`/`next`) directly unless allocating/deallocating a prop, as list order and pointers are crucial to the engine’s update loop.
