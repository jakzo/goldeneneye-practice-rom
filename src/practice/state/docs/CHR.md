# Character Prop State

This document covers `PROP_TYPE_CHR`, the final prop type without save/load
support. `ChrRecord` owns a character's AI, combat, movement, damage, and
render-facing state. Its top-level layout is documented in
[`PROPS.md`](PROPS.md#4-chrrecord-prop_type_chr); this file documents the
supporting structs stored in or referenced by that record.

## Current implementation boundary

The first CHR serialization slice is implemented. It restores only the
following scalar behavior parameters:

```c
ChrRecord::accuracyrating; /* Signed shooting-accuracy adjustment (-128..127). */
ChrRecord::speedrating;    /* Signed reaction/movement-speed adjustment (-128..127). */
ChrRecord::arghrating;     /* Signed damage-reaction-speed adjustment (-128..127). */
ChrRecord::grenadeprob;    /* Grenade-attempt threshold (0..255). */
ChrRecord::visionrange;    /* Maximum visual-detection distance. */
ChrRecord::hearingscale;   /* Hearing-distance multiplier; initialized to 1.0. */
ChrRecord::morale;         /* AI-script morale byte, 0-255. */
ChrRecord::alertness;      /* AI-script alertness byte, 0-255. */
```

These values are self-contained inputs to AI calculations. Restoring them does
not change prop allocation, linked lists, pointers, animation state, collision
state, model state, or the current AI instruction. This makes them a
conservative first slice.

The second CHR serialization slice restores the following scalar AI-memory
fields:

```c
ChrRecord::numarghs;      /* Confirmed hit-reaction count, 0 initially. */
ChrRecord::numclosearghs; /* Near-miss/close-hit count, 0 initially. */
ChrRecord::random;        /* Latched AI random byte, 0-255. */
ChrRecord::padpreset1;    /* Resolved stage pad ID, or -1 when unset. */
ChrRecord::chrpreset1;    /* Resolved character ID, or -1 when unset. */
ChrRecord::chrseeshot;    /* Character seen being shot, or CHR_FREE (-1). */
ChrRecord::chrseedie;     /* Character seen dying, or CHR_FREE (-1). */
```

`numarghs` increments when a character enters a confirmed damage reaction and
is compared by the `IFMyNumArghs*` AI commands. `numclosearghs` is exposed to
the equivalent close-argh commands; its increment path remains unidentified
in the current decompilation, so its exact near-miss semantics are TODO.

`random` is updated only when `AI_SetNewRandom` runs and is reused by random,
morale, and alertness branches until replaced. Restoring it preserves the
saved branch input without advancing either global random seed.

`padpreset1` is the concrete pad substituted for `PAD_PRESET1` (`9000`).
`chrpreset1` is the concrete character ID substituted for `CHR_PRESET` (`-4`).
The preset fields contain IDs rather than pointers, so they remain valid while
loading within the same level.

`chrseeshot` and `chrseedie` contain the character ID propagated to nearby
guards when a character is shot or dies. AI resolves `CHR_SEE_SHOT` (`-6`) and
`CHR_SEE_DIE` (`-5`) through these fields. Both initialize to and are reset to
`CHR_FREE` (`-1`) after an AI action tick, so nonnegative values are transient
but valid save-state data.

The third CHR serialization slice restores the character's perception memory:

```c
ChrRecord::lastseetarget60;      /* Global tick of the last visual detection. */
ChrRecord::lastknowntargetpos;   /* Remembered target world position. */
ChrRecord::targetTile;           /* StandTile containing that position. */
ChrRecord::seen_bond_time;       /* Global tick of the last direct Bond sighting. */
ChrRecord::lastheartarget60;     /* Global tick of the last auditory detection. */
```

The three time fields are absolute `g_GlobalTimer` values, not elapsed
durations. Zero means no recorded detection. `chrSawTargetRecently` and
`chrHeardTargetRecently` treat a timestamp as recent for 600 NTSC ticks or 500
PAL ticks (ten seconds). `seen_bond_time` is also checked against the most
recent 120 NTSC or 100 PAL ticks while deciding whether a guard may fire.
These timestamps remain coherent because `g_GlobalTimer` is restored by the
global-state payload.

Both visual and auditory alerts copy Bond's current position into
`lastknowntargetpos` and his stand tile into `targetTile`. `targetTile` is
serialized as a byte offset from `standTileStart`, not as an address. A saved
offset is stable because save states can only be loaded into the same level;
`-1` represents `NULL`.

The fourth CHR serialization slice restores these independent runtime fields:

```c
ChrRecord::chrnum;       /* Runtime character ID. */
ChrRecord::flags2;       /* Complete secondary AI-script flag byte. */
ChrRecord::timer60;      /* Per-character AI timer value. */
CHRHIDDEN_TIMER_ACTIVE;  /* Timer running/paused control bit only. */
ChrRecord::shotbondsum;  /* Fractional accumulated damage against Bond. */
ChrRecord::shadecol;     /* Current four-channel tile-lighting colour. */
ChrRecord::nextcol;      /* Target four-channel tile-lighting colour. */
```

`chrnum` is the character's runtime identity used by `chrFindById` and AI
character selectors. It is normally a nonnegative level-assigned ID, can be
replaced by `AI_SetMyChrNum`, and uses signed 16-bit storage. Restoring all
surviving CHRs returns their script-visible identities to the saved values.

`flags2` is restored as a complete byte. AI commands set, unset, and test
arbitrary masks in this field. Known bits are `FLAGS2_DONT_POINT_AT_BOND`
(`0x01`), unknown/script-reserved `FLAGS2_02` (`0x02`), and `FLAGS2_04`
(`0x04`), which alarm AI uses as persistent state. The remaining bits are
available to script masks but have no documented built-in meaning.

`timer60` initializes to zero and increments by `g_ClockTimer` only while
`CHRHIDDEN_TIMER_ACTIVE` (`0x0040`) is set. AI can start, reset, pause, resume,
and compare this timer. The timer value and active bit are restored together,
but only that bit is merged into `hidden`; all unrelated `hidden` state is
preserved. Restoring the whole `hidden` field is unsafe until removal, firing,
movement, freeze, background-AI, and action state are coordinated.

`shotbondsum` is a fractional damage accumulator used while guard fire is
calculated. It initializes to `0.0`, accumulates accuracy-adjusted contributions
and normally remains below `1.0`; reaching `1.0` applies damage to Bond and
resets it to zero.

`shadecol` is the current character lighting colour and `nextcol` is the target
calculated from the character's stand tile. Both are `rgba_u8` values with four
channels in the range 0-255. They are restored as a pair so an in-progress
lighting transition remains internally consistent.

The fifth CHR serialization slice restores the AI interpreter state:

```c
ChrRecord::ailist;       /* Active AI command list, encoded by stable list ID. */
ChrRecord::aioffset;     /* Byte offset of the next command in that list. */
ChrRecord::aireturnlist; /* AI list ID used by the Return command. */
ChrRecord::sleep;        /* Ticks before the next AI/action update. */
```

`ailist` is a pointer into static global or level setup data and is never saved
as an address. `chraiGetAIListID` converts it to its stable ID and
`ailistFindById` resolves that ID on load. Global list IDs occupy `0-1024`,
level character lists occupy `1025-4095`, and background lists start at
`4096`. ID `0` is valid, so the serialized null/not-found sentinel is `-1`.

`aioffset` is an unsigned 16-bit byte offset to the next command within
`ailist`. It is updated when an AI list yields. The same-ROM, same-level load
restriction guarantees that saved command boundaries still describe the
resolved list.

`aireturnlist` is a signed 16-bit stable list ID used by `AI_Return`; `-1`
means no return list has been assigned. It does not contain an address.

`sleep` is a signed eight-bit tick delay. Positive values postpone AI and
action processing and are normally capped at 127; it is decremented by
`g_ClockTimer`, and reaching a negative value permits the next AI update. This
field also spaces some animation/action updates, so it is restored with the
interpreter state rather than independently.

The payload is implemented in `practice_states_chr.c` and dispatched by
`practice_states_props.c`. The common `PropRecord` payload is deliberately not
restored for CHRs yet: changing its position, stand tile, or rooms without also
synchronizing the character model, collision, and movement history is unsafe.

Do not include the following in that slice:

- `damage`, `maxdamage`, action data, `actiontype`, movement fields, held
  weapons, model state, or flags. Those values have coupled state which must be
  investigated and restored together.
- The complete `hidden` and `chrflags` fields. They contain action-coupled and
  destructive bits which must be restored with character action, model,
  movement, damage, and allocation state.
- Prop position. A CHR position change must keep the prop, character movement
  history, stand tile, rooms, collision bounds, and model transform coherent.

TODO: CHR loading currently assumes the saved character still has the same
enabled `PropRecord`, `ChrRecord`, and `Model`. Preserve their existing
back-pointers rather than serializing addresses. Revisit this when loading can
remove destroyed characters or create missing characters.

## Action discriminator

`ChrRecord::actiontype` selects exactly one member of the action union. Valid
runtime values are:

```c
typedef enum ACT_TYPE {
    ACT_INIT = 0,         /* Newly created; initialize on a later tick. */
    ACT_STAND = 1,        /* Idle, turning, aiming, or facing a target. */
    ACT_KNEEL = 2,        /* Kneeling transition/pose. */
    ACT_ANIM = 3,         /* AI-requested scripted animation. */
    ACT_DIE = 4,          /* Active death animation and impulses. */
    ACT_DEAD = 5,         /* Corpse fade/reap state. */
    ACT_ARGH = 6,         /* Immediate hit reaction. */
    ACT_PREARGH = 7,      /* Deferred hit reaction. */
    ACT_ATTACK = 8,       /* Stationary attack. */
    ACT_ATTACKWALK = 9,   /* Attack while moving. */
    ACT_ATTACKROLL = 10,  /* Attack during an evasive roll. */
    ACT_SIDESTEP = 11,    /* Lateral dodge. */
    ACT_JUMPOUT = 12,     /* Jump out from cover. */
    ACT_RUNPOS = 13,      /* Run to an arbitrary position. */
    ACT_PATROL = 14,      /* Follow a configured patrol path. */
    ACT_GOPOS = 15,       /* Pathfind to a target position/tile. */
    ACT_SURRENDER = 16,   /* Surrender animation/state. */
    ACT_LOOKATTARGET = 17,/* Look-at-target animation/state. */
    ACT_SURPRISED = 18,   /* Surprise or fluster animation. */
    ACT_STARTALARM = 19,  /* Reach for an alarm. */
    ACT_THROWGRENADE = 20,/* Grenade-throw animation/state. */
    ACT_TURNDIR = 21,     /* Scripted turn. */
    ACT_TEST = 22,        /* Internal/test action. */
    ACT_BONDINTRO = 23,   /* Single-player intro action. */
    ACT_BONDDIE = 24,     /* Single-player death action. */
    ACT_BONDMULTI = 25,   /* Multiplayer player-model action. */
    ACT_NULL = 26,        /* No action. */
    ACT_TYPE_MAX = 27
} ACT_TYPE;
```

The action union is model- and animation-coupled. Restoring its bytes without
also restoring the selected model animation can make the action tick interpret
incompatible state. Pointer members also cannot be restored as addresses.

## Action records

All action members overlay the same 120-byte region at `ChrRecord` offset
`0x2c`. Unlisted bytes in shorter records are inactive union storage, not
independent fields.

### `act_init`, `act_kneel`, and marker-only actions

```c
struct act_init         { int padding[30]; };
struct act_kneel        { int padding[30]; };
struct act_surrender    { int padding[30]; };
struct act_lookattarget { int padding[30]; };
struct act_startalarm   { int padding[30]; };
struct act_turndir      { int padding[30]; };
struct act_test         { int padding[30]; };
struct act_bondintro    { int padding[30]; };
struct act_bonddie      { int padding[30]; };
struct act_null         { int padding[30]; };
```

These declarations reserve the full union size but do not describe distinct
fields for the named action. Their behavior is selected by `actiontype` and,
where applicable, the current `Model` animation. The decompilation sometimes
accesses known fields through `act_init.padding`: `[0]` aliases the first field
of `act_dead`, `act_die`, and `act_argh`; `[1]` and `[3]` alias
`act_anim.holdLastFrame` and `idleOnEnd`; and `[0x13]` aliases
`act_patrol.lastvisible60`. These are alternate views of typed fields, not
independent padding state.

TODO: Determine whether any remaining bytes have action-specific meaning for
`ACT_INIT`, `ACT_KNEEL`, `ACT_SURRENDER`, `ACT_LOOKATTARGET`,
`ACT_STARTALARM`, `ACT_TURNDIR`, `ACT_TEST`, `ACT_BONDINTRO`, or
`ACT_BONDDIE` before serializing these actions.

### `act_stand`

Idle/standing state, including current facing and re-aim work.

```c
struct act_stand {
    s32 prestand;         /* 0x00 - Boolean: entered standing from kneeling. */
    s32 face_entitytype;  /* 0x04 - Entity/attack target bitfield; 0 means none. */
    s32 face_entityid;    /* 0x08 - ID interpreted according to face_entitytype. */
    s32 reaim;            /* 0x0c - Boolean/state requesting weapon re-aim. */
    s32 turning;          /* 0x10 - Turn state; initialized to 2. */
    u32 checkfacingwall;  /* 0x14 - Boolean: check/correct facing into walls. */
    s32 wallcount;        /* 0x18 - Countdown until the next wall-facing check. */
    f32 mergetime;        /* 0x1c - Animation interpolation duration. */
    s8  face_target;      /* 0x20 - Boolean: face the character's current target. */
};
```

`face_entitytype` uses the same target-selection bits as `ATTACKTYPE` where the
standing code aims at an entity: Bond `0x01`, forward `0x02`, character `0x04`,
pad `0x08`, direction `0x10`, aim-only `0x20`, do-not-turn `0x40`, and current
target `0x80`. Some standing paths also use `0x10` as a transient direction
target, so it must be interpreted with the active code path.

TODO: Determine the complete value sets for `reaim`, `turning`, and
`face_target`.

### `act_anim`

Parameters for an AI-requested animation. The live `Model` holds the actual
animation pointer, frame, speed, loop, and interpolation state.

```c
struct act_anim {
    u32 unk02c;        /* 0x00 - Boolean from ANIM_UNKNOWN (0x01). */
    u32 holdLastFrame; /* 0x04 - Boolean from ANIM_LOOP_HOLD_LAST_FRAME. */
    u32 playSfx;       /* 0x08 - Boolean from ANIM_PLAY_SFX. */
    u32 idleOnEnd;     /* 0x0c - Boolean from ANIM_IDLE_POSE_WHEN_COMPLETE. */
    u32 noTranslate;   /* 0x10 - Boolean from ANIM_NO_TRANSLATION. */
    u8  unk040;        /* 0x14 - TODO: determine purpose and values. */
    u8  unk041;        /* 0x15 - TODO: determine purpose and values. */
    u16 unk042;        /* 0x16 - TODO: determine purpose and values. */
    u16 unk044;        /* 0x18 - TODO: determine purpose and values. */
    u16 unk046;        /* 0x1a - TODO: determine purpose and values. */
    s16 animnum;       /* 0x1c - Animation-table index. */
    u8  flip;          /* 0x1e - Boolean: mirror the animation. */
    f32 startframe;    /* 0x20 - First animation frame. */
    f32 endframe;      /* 0x24 - Last animation frame; negative means default end. */
    f32 unk054;        /* 0x28 - TODO: determine purpose and values. */
    f32 unk058;        /* 0x2c - TODO: determine purpose and values. */
};
```

### `act_die`

State for an active death animation, including sound-trigger frames and
post-impact motion.

```c
struct act_die {
    s32     notifychrindex;     /* 0x00 - Character ID to notify; 0 means none. */
    f32     thudframe1;         /* 0x04 - First thud SFX frame; -1 after firing/none. */
    f32     thudframe2;         /* 0x08 - Second thud SFX frame; -1 after firing/none. */
    f32     timeextra;          /* 0x0c - Duration of extra impact movement. */
    f32     elapseextra;        /* 0x10 - Elapsed extra-movement time. */
    coord3d extraspeed;         /* 0x14 - Extra velocity from the killing impact. */
    s16     drcarollimagedelay; /* 0x20 - Delay before special death imagery. */
};
```

TODO: Confirm the units and terminal value for `drcarollimagedelay`, and whether
`notifychrindex == 0` is always the no-notification sentinel.

### `act_dead`

Corpse lifetime state.

```c
struct act_dead {
    bool allowfade;      /* 0x00 - Boolean; nonzero permits corpse fading. */
    bool allowreap;      /* 0x04 - Boolean; nonzero permits corpse removal. */
    s32  reaptimer;      /* 0x08 - Countdown/time controlling removal. */
    s32  fadetimer;      /* 0x0c - Countdown/time controlling alpha fade. */
    s32  notifychrindex; /* 0x10 - Character ID to notify, or no-target sentinel. */
};
```

Loading this record alone is unsafe: fade alpha, model state, held items, prop
existence, and mission death bookkeeping are coupled to it.

### `act_argh`

Immediate damage-flinch state.

```c
struct act_argh {
    s32 notifychrindex; /* 0x00 - Character ID to notify; initialized to 0. */
    s32 unk30;          /* 0x04 - Global tick captured for the hit reaction. */
};
```

### `act_preargh`

Deferred hit information consumed when the character transitions into a flinch
or death action.

```c
struct act_preargh {
    coord3d pos;    /* 0x00 - World-space hit position. */
    f32     unk038; /* 0x0c - Relative shot angle. */
    s32     unk03c; /* 0x10 - Model hit-part index. */
    s32     unk040; /* 0x14 - ITEM_IDS weapon ID which caused the hit. */
    u32     unk044; /* 0x18 - TODO: determine purpose and values. */
};
```

### `act_attack`

Stationary aiming/firing state. Most unnamed fields are animation phase,
per-hand timing, and aiming interpolation state.

```c
struct act_attack {
    struct weapon_firing_animation_table *animfloats; /* 0x00 - Static table row. */
    s8  unk30, unk31, unk32, unk33;                   /* 0x04 - TODO. */
    s8  unk34, unk35, unk36, unk37;                   /* 0x08 - TODO. */
    s8  unk38[2], unk3a[2];                           /* 0x0c - Per-hand state. */
    s8  unk3c[2], unk3e, unk3f;                       /* 0x10 - Per-hand/state bytes. */
    u32 unk40;                                         /* 0x14 - TODO. */
    s32 unk44;                                         /* 0x18 - Previous/related attack time. */
    s32 attack_time;                                   /* 0x1c - Attack elapsed/countdown ticks. */
    u32 attacktype;                                    /* 0x20 - ATTACKTYPE bitfield. */
    u32 entityid;                                      /* 0x24 - Target ID for CHR/PAD/DIRECTION. */
    u32 unk54;                                         /* 0x28 - TODO. */
    s32 type_of_motion;                                /* 0x2c - Re-aim/motion state. */
    u32 unk5C, unk60, unk64, unk68, unk6c;             /* 0x30 - TODO. */
    u32 unk70, unk74, unk78, unk7c;                    /* 0x44 - TODO. */
    s8  attack_item;                                   /* 0x54 - ITEM_IDS weapon ID. */
    u8  unk81, unk82, unk83;                           /* 0x55 - TODO. */
};
```

Valid `attacktype` bits are `ATTACKTYPE_BOND` (`0x01`),
`ATTACKTYPE_FORWARD` (`0x02`), `ATTACKTYPE_CHR` (`0x04`),
`ATTACKTYPE_PAD` (`0x08`), `ATTACKTYPE_DIRECTION` (`0x10`),
`ATTACKTYPE_AIMONLY` (`0x20`), `ATTACKTYPE_DONTTURN` (`0x40`), and
`ATTACKTYPE_TARGET` (`0x80`); zero means no attack target.

`animfloats` points into static weapon animation tables, but it must eventually
be encoded as a stable table identity/index rather than saved as an address.

TODO: Name and determine the possible values of the remaining attack fields
before this action is serialized.

### `act_attackwalk`

Moving attack state. It parallels `act_attack` but adds movement speed and
separate clocks.

```c
struct act_attackwalk {
    u32 unk02c;                          /* 0x00 - TODO. */
    s32 clock_timer30, clock_timer34;    /* 0x04 - Action clocks/timers. */
    u32 unk038;                          /* 0x0c - TODO. */
    struct weapon_firing_animation_table *animfloats; /* 0x10 - Static table row. */
    s32 timer40;                         /* 0x14 - Action timer. */
    s32 unk044;                          /* 0x18 - TODO. */
    s8  unk48[2], unk4a[2], unk4C[2];   /* 0x1c - Per-hand/action bytes. */
    u8  flip;                            /* 0x22 - Boolean animation mirror. */
    s8  unk4f;                           /* 0x23 - TODO. */
    s32 unk50;                           /* 0x24 - TODO. */
    f32 speed;                           /* 0x28 - Current movement speed. */
    u32 unk58, unk5C, unk60, unk64;      /* 0x2c - TODO. */
    u32 unk68, unk6c, unk70, unk74;      /* 0x3c - TODO. */
    u32 unk78, unk7c;                    /* 0x4c - TODO. */
    s8  attack_item;                     /* 0x54 - ITEM_IDS weapon ID. */
    u8  unk81, unk82, unk83;             /* 0x55 - TODO. */
};
```

TODO: Determine the meaning and valid ranges of the unnamed fields and how the
movement state synchronizes with `ChrRecord::prevpos`, `fallspeed`, and the
model animation.

### `act_attackroll`

Evasive-roll attack state. Its leading firing fields parallel `act_attack`;
the middle arrays hold roll-specific state.

```c
struct act_attackroll {
    struct weapon_firing_animation_table *animfloats; /* 0x00 - Static table row. */
    s8  unk30, unk31, unk32, unk33;                   /* 0x04 - TODO. */
    s8  unk34, unk35, unk36, unk37;                   /* 0x08 - TODO. */
    s8  unk38[2], unk3a[2];                           /* 0x0c - Per-hand state. */
    s8  unk3c[2], unk3e, unk3f;                       /* 0x10 - Per-hand/state bytes. */
    u32 unk40;                                         /* 0x14 - TODO. */
    s32 unk44;                                         /* 0x18 - Previous/related attack time. */
    s32 attack_time;                                   /* 0x1c - Attack elapsed/countdown ticks. */
    s32 unk4c[2], unk54[2];                            /* 0x20 - Roll-specific state. */
    u32 unk5C, unk60, unk64, unk68, unk6c;             /* 0x30 - TODO. */
    u32 unk70, unk74, unk78, unk7c;                    /* 0x44 - TODO. */
    s8  attack_item;                                   /* 0x54 - ITEM_IDS weapon ID. */
    u8  unk81, unk82, unk83;                           /* 0x55 - TODO. */
};
```

TODO: Determine the meaning and possible values of all unnamed roll fields.

### `act_sidestep` and `act_jumpout`

```c
struct act_sidestep { bool side; }; /* 0 = one side, nonzero = opposite side. */
struct act_jumpout  { bool side; }; /* 0 = one side, nonzero = opposite side. */
```

These records select the mirrored direction of their animation/movement.
TODO: Confirm which value maps to left and right.

### `act_runpos`

Direct movement toward an arbitrary point.

```c
struct act_runpos {
    coord3d pos;       /* 0x00 - Destination in world coordinates. */
    f32     neardist;  /* 0x0c - Arrival radius; initialized to 30.0. */
    s32     eta60;     /* 0x10 - Estimated ticks remaining; counts down to zero. */
    f32     turnspeed; /* 0x14 - Current turn interpolation/speed. */
};
```

### `patrol_path`

Runtime patrol-path descriptor referenced by `act_patrol`.

```c
struct patrol_path {
    s32 *data;  /* 0x00 - Pointer to path waypoint IDs. */
    u8   unk04; /* 0x04 - TODO: determine purpose and values. */
    u8   flags; /* 0x05 - TODO: determine path flag bits. */
    u16  len;   /* 0x06 - Number of path entries. */
};
```

The path and its `data` are pointers and need stable IDs/offsets. Do not save
their addresses.

### `waydata`

Incremental path-travel state embedded by patrol and go-to-position actions.

```c
struct waydata {
    s8      mode;         /* 0x00 - WAYMODE_0..WAYMODE_5 or WAYMODE_MAGIC (0-6). */
    s8      unk01;        /* 0x01 - Candidate/next mode; TODO. */
    s8      unk02;        /* 0x02 - Boolean/state associated with pos; TODO. */
    s8      unk03;        /* 0x03 - Boolean/state associated with pos_copy; TODO. */
    coord3d pos;          /* 0x04 - Current path segment position. */
    coord3d pos2;         /* 0x10 - Secondary segment position; TODO verify. */
    coord3d pos3;         /* 0x1c - Tertiary segment position. */
    s32     age;          /* 0x28 - Number of updates in current path state. */
    coord3d pos_copy;     /* 0x2c - Saved/copy position used by travel logic. */
    f32     segdistdone;  /* 0x38 - Distance completed in cheap travel mode. */
    f32     segdisttotal; /* 0x3c - Total segment distance in cheap travel mode. */
};
```

Modes `0` through `5` are phases of normal incremental navigation.
`WAYMODE_MAGIC` (`6`) selects the cheaper off-screen path-travel method.
TODO: Give semantic names to modes `0` through `5` after their transition
conditions are fully understood.

### `act_patrol`

Configured path patrol state.

```c
struct act_patrol {
    struct patrol_path *path; /* 0x00 - Active patrol descriptor. */
    s32  nextstep;            /* 0x04 - Next index in path->data. */
    bool forward;             /* 0x08 - Direction through the path. */
    struct waydata waydata;   /* 0x0c - Current segment travel state. */
    s32 lastvisible60;        /* 0x4c - Last global tick at which path was visible. */
    f32 speed;                /* 0x50 - Current patrol speed. */
    s32 unk80, unk84, unk88, unk8c; /* 0x54 - TODO. */
    s32 unk90, unk94, unk98, unk9c, unka0; /* 0x64 - TODO. */
};
```

Loading this action requires stable path identification and consistent
`waydata`, position, stand tile, room, and model movement state.

### `waypoint`

Static navigation-graph node referenced by `act_gopos`.

```c
typedef struct waypoint {
    s32  padID;       /* Pad anchoring this node. */
    s32 *neighbours;  /* -1-terminated neighbouring waypoint-ID array. */
    s32  groupNum;    /* Index of the containing waygroup. */
    s32  dist;        /* Mutable pathfinder heuristic/work distance. */
} waypoint;
```

Pointers to waypoints can be serialized only as indices into the level's
waypoint table. `neighbours` is static level data and is not CHR-owned.

### `act_gopos`

Pathfinding state for movement to a selected position/tile.

```c
struct act_gopos {
    coord3d targetpos;                         /* 0x00 - Final world destination. */
    StandTile *target;                         /* 0x0c - Final navigation tile. */
    waypoint  *target_path;                    /* 0x10 - Final waypoint. */
    waypoint  *waypoints[MAX_CHRWAYPOINTS];    /* 0x14 - Next path nodes. */
    u8         curindex;                       /* 0x2c - Current entry; normally 0-2. */
    u8         unk59;                          /* 0x2d - Flags/room guess; TODO. */
    u16        unk5a;                          /* 0x2e - Clock/restart value; TODO. */
    struct waydata waydata;                    /* 0x30 - Current segment state. */
    s32        unk9c;                          /* 0x70 - Last path update tick. */
    f32        speed;                          /* 0x74 - Current movement speed. */
};
```

When `curindex` reaches 3 the route is recalculated and the waypoint array is
replaced. `target` must use a stand-tile offset and waypoint pointers must use
level-table indices; raw addresses are unsafe.

### `act_surprised`

```c
struct act_surprised {
    u32 type; /* Surprise-animation subtype. */
};
```

TODO: Determine whether `type` is used in GoldenEye and document its valid
values. Several transition functions select the animation directly on the
model without assigning this member.

### `act_throwgrenade`

Target and hand state for grenade throwing.

```c
struct act_throwgrenade {
    u32  entitytype; /* TODO: determine target selector values. */
    u32  entityid;   /* TODO: determine ID interpretation and sentinels. */
    u32  hand;       /* TODO: determine whether this is a GUNHAND value. */
    bool needsequip; /* TODO: verify meaning; expected Boolean storage. */
};
```

No GoldenEye consumer currently references these named members; the throw
animation is driven by the model and other CHR state. Do not serialize inferred
semantics until the fields are identified.

### `act_bondmulti`

Multiplayer player-model animation state.

```c
struct act_bondmulti {
    f32 *unk2c; /* Pointer to animation data; exact table type is unknown. */
    s32  unk30, unk34, unk38, unk3c; /* Animation state; TODO. */
    int  padding[25];                /* Unnamed remainder of union storage. */
};
```

The animation pointer needs a stable table identity/index. TODO: identify the
pointed-to table and document the named state fields before serialization.

### Raw action aliases

```c
struct act_bytes  { s8 padding[120]; };
struct act_ubytes { u8 padding[120]; };
```

These are decompilation views of the same union bytes, not distinct runtime
action types. Known accesses include byte `84`, which aliases `attack_item` in
the attack records, and byte `45`, which aliases an action-dependent value used
as a hit-sound selector. They must not be serialized in addition to the
selected typed member. TODO: identify the typed field represented by byte 45.

## Other supporting records

### `weapon_firing_animation_table`

Static aiming/firing interpolation data referenced by `act_attack`,
`act_attackwalk`, and `act_attackroll`.

```c
struct weapon_firing_animation_table {
    s32 anonymous_0;
    f32 anonymous_1,  anonymous_2,  anonymous_3,  anonymous_4;
    f32 anonymous_5,  anonymous_6,  anonymous_7,  anonymous_8;
    f32 anonymous_9,  anonymous_10, anonymous_11, anonymous_12;
    f32 anonymous_13;
    f32 anonymous_14; /* Minimum used by chrlvUpdateAimendsideback. */
    f32 anonymous_15; /* Maximum used by chrlvUpdateAimendsideback. */
    f32 anonymous_16, anonymous_17;
};
```

Rows live in static weapon/stance-specific tables. The pointer is reconstructible
in principle, but the table and row need stable identifiers. TODO: determine
the meaning and range of all columns before serializing attack actions.

### `ChrRecord_f180`

Per-hand cache used while calculating and rendering weapon beam/tracer
endpoints. `ChrRecord::unk180[0]` is the right-hand cache and `[1]` is the
left-hand cache.

```c
typedef struct ChrRecord_f180 {
    s8      unk00;   /* Cache/activity state; initialized to -1 (inactive). */
    s8      item_id; /* ITEM_IDS value associated with the cached shot. */
    char    unk02;   /* TODO: determine purpose and values. */
    char    unk03;   /* TODO: determine purpose and values. */
    coord3d pos;     /* Cached beam endpoint/origin position. */
    coord3d delta;   /* Cached beam direction/delta. */
    f32     unk1c;   /* TODO: beam-length/interpolation value. */
    f32     unk20;   /* TODO: determine purpose and values. */
    f32     unk24;   /* TODO: determine purpose and values. */
    f32     unk28;   /* TODO: determine purpose and values. */
} ChrRecord_f180;
```

This is transient visual/weapon state and is not part of the conservative
first save/load slice.
