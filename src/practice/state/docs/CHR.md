# Character Prop State

This document covers the incremental save/load support for `PROP_TYPE_CHR`,
the final prop type. `ChrRecord` owns a character's AI, combat, movement, damage, and
render-facing state. Its top-level layout is documented in
[`PROPS.md`](PROPS.md#4-chrrecord-prop_type_chr); this file documents the
supporting structs stored in or referenced by that record.

## Current implementation boundary

Every CHR payload starts with allocation metadata:

```c
ChrAllocationState::headnum; /* Signed head model index; -1 means body-owned head. */
ChrAllocationState::bodynum; /* Signed body model index. */
ChrAllocationState::heading; /* Initial model Y heading in radians. */
```

This header is consumed before the destination `ChrRecord` exists. The current
CHR at the saved prop index is fully torn down, a body/head model is allocated
with `retrieve_header_for_body_and_head`, and `init_GUARDdata_with_set_values`
establishes a new `ChrRecord`/`Model`/`PropRecord` relationship before any
payload fields are applied. CHRs in enabled prop slots absent from the save are
cleaned up and released. Add/remove of props on load is now enabled for every
prop type (see `INSTRUCTIONS.md` → Key Learnings → "Adding/Removing Props on
Load").

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

The second CHR serialization slice restores the character/model configuration:

```c
ChrRecord::headnum;   /* Signed head-model configuration index. */
ChrRecord::bodynum;   /* Signed body-model configuration index. */
ChrRecord::chrwidth;  /* Horizontal collision radius in world units. */
ChrRecord::chrheight; /* Full standing collision height in world units. */
```

`headnum` and `bodynum` identify entries in the character model configuration
tables and use signed eight-bit storage. Ordinary level characters have
nonnegative IDs. A negative head ID is accepted by character-spawn paths to
request a body-compatible head choice; once a character is spawned, its record
normally contains the selected concrete head ID.

These IDs must describe the allocated model rather than merely label it.
`ChrAllocationState` is read before a destination character exists and the
saved body/head pair is passed to `retrieve_header_for_body_and_head`; only
then is the newly allocated `ChrRecord` populated. The loader therefore does
not assume that a pre-existing model matches the saved IDs.

`chrwidth` is the radius used for character/world collision, path obstruction,
movement probes, and the four-point `collision_bounds` diamond. It initializes
to `20.0`. The wall-slump death path can reduce it to `10.0`, so the live value
is serialized rather than recomputed from body identity. `chrheight`
initializes to `185.0`; collision queries generally expose `chrheight - 20.0`
as the usable vertical extent, while ground placement uses the full height.
Both values are floating-point world units and are restored before the next
character tick. The already implemented spatial slice separately restores the
saved collision diamond, keeping it consistent with a nondefault width until
normal collision processing recalculates it.

The third CHR serialization slice restores the following scalar AI-memory
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

The fourth CHR serialization slice restores the character's perception memory:

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

The fifth CHR serialization slice restores these independent runtime fields:

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

The sixth CHR serialization slice restores the AI interpreter state:

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

The seventh CHR serialization slice restores spatial and movement state as one
unit:

```c
PropRecord::pos;              /* Current world position. */
PropRecord::stan;             /* Current StandTile, encoded as a byte offset. */
PropRecord::rooms[4];         /* 0xFF-terminated occupied-room IDs. */
ChrRecord::invalidmove;       /* Whether the most recent move failed. */
ChrRecord::sumground;         /* Ground-height accumulator. */
ChrRecord::manground;         /* Companion/manual ground-height value. */
ChrRecord::ground;            /* Supporting floor's world Y coordinate. */
ChrRecord::fallspeed;         /* Current X/Y/Z fall or impact velocity. */
ChrRecord::prevpos;           /* Previous model-root world position. */
ChrRecord::lastwalk60;        /* Last travel-update g_GlobalTimer tick. */
ChrRecord::lastmoveok60;      /* Last successful-move g_GlobalTimer tick. */
ChrRecord::collision_bounds;  /* Four-point X/Z collision diamond. */
Model root offset;            /* Current rendered model world position. */
Model root heading;           /* Current Y-axis heading in radians. */
```

`PropRecord::pos` and the model root offset are both three finite floating-point
world coordinates. They normally agree after a character tick, but both are
saved because animation processing owns the model root and copies it into the
prop. The root heading is an angle in radians, normally normalized to
`0 <= heading < 2*pi`. The simple-action slice described below additionally
restores the complete pointer-free root working record while its action is
supported.

`stan` is serialized as a byte offset from `standTileStart`; `-1` represents
`NULL`. This is stable under the existing same-level restriction. `rooms`
contains up to three room IDs in the range 0-254 followed by `0xff`. Loading
first deregisters the active prop from its old rooms, replaces all four room
bytes and its stand tile, then registers it in the restored rooms.

`invalidmove` is set to `0` when travel starts or succeeds and `1` on a failed
movement attempt. Patrol, go-position, and run-position actions use it to
decide whether to stop or reconsider a route.

`ground` is the world Y coordinate of the supporting floor, not a distance
above it. `fallspeed` is the character's three-component falling or
impact-motion velocity and initializes to `(0, 0, 0)`. `sumground` and
`manground` also initialize to `0.0`; their exact ground-smoothing roles are
not exposed by the currently decompiled C, so naming their individual
semantics remains TODO. They are restored with `ground` because all three form
the contiguous ground-motion state.

`prevpos` records the model root before the previous movement/animation tick.
Travel actions compare it with the current prop position to detect crossing a
destination. `lastwalk60` and `lastmoveok60` are absolute `g_GlobalTimer`
ticks: the former records recent travel processing, while the latter is
refreshed after a valid move and is tested to detect a stuck route. Zero is the
initial/no-walk value for `lastwalk60`; `lastmoveok60` initializes to the
current global timer.

`collision_bounds` contains four X/Z coordinate pairs. For an ordinary CHR,
they form a diamond centered on `prop->pos` with points at plus/minus
`chrwidth` on each axis. Saving the live values also preserves any
action-specific bound update until the engine calculates them again.

The eighth CHR serialization slice restores equipment and its attachment
ownership:

```c
ChrRecord::weapons_held[0];        /* Right-hand weapon prop, or NULL. */
ChrRecord::weapons_held[1];        /* Left-hand weapon prop, or NULL. */
ChrRecord::weapons_held[2];        /* Legacy unused slot; normally NULL. */
ChrRecord::handle_positiondata_hat;/* Headwear prop, or NULL. */
ChrAttachmentIndices::weapon_model[2]; /* Weapon model IDs for recreation. */
ChrAttachmentIndices::weaponnum[2];    /* ITEM_IDS values for recreation. */
ChrAttachmentIndices::weapon_flags[2]; /* Object setup flags for recreation. */
ChrAttachmentIndices::gunfire_visible[2]; /* Per-hand muzzle flash latch. */
ChrAttachmentIndices::hat_model;       /* Hat model ID for recreation. */
ChrAttachmentIndices::hat_flags;       /* Hat object setup flags. */
ModelRwData_GunfireRecord::visible;    /* Held weapon's GUNFIRE node flash. */
PropRecord::parent;                /* Owning CHR prop for attached equipment. */
PropRecord::child/prev/next;       /* CHR child/sibling attachment links. */
ObjectRecord::runtime_bitflags;    /* RUNTIMEBITFLAG_HASOWNER ownership bit. */
Model::attachedto;                 /* Owning CHR model. */
Model::attachedto_objinst;         /* Hand/head model switch node. */
```

The four CHR pointers are encoded as signed prop-array indices: `-1` means
`NULL`, while `0..POS_DATA_ENTRY_LEN-1` identifies a `PropRecord`. They are
resolved only after all prop payloads have loaded. Held equipment is normally
disabled and therefore does not appear as a standalone enabled-prop payload.
A hand entry is accepted when the indexed prop still has a matching live
`PROPDEF_COLLECTABLE` object/model and a correct object-to-prop back-pointer.
The hat entry additionally requires `PROPDEF_HAT`. Missing, mismatched, reused,
or duplicate targets are resolved to `NULL`.

Held equipment is disabled while attached, so it is not guaranteed to have a
standalone enabled-prop record in the save. The allocation metadata therefore
also stores each hand weapon's object model ID, `ITEM_IDS` value, and complete
object setup flags, plus the hat model ID and flags. In replacement mode, a
missing weapon is recreated with `chrGiveWeapon` and a missing hat with
`hatCreateForChr`; the returned props replace the unresolved saved indices.
Testing mode never invokes these constructors.

`weapons_held[0]` and `[1]` are selected by `GUNRIGHT` (`0`) and `GUNLEFT`
(`1`). `weapons_held[2]` is initialized to `NULL` and has no readers or
writers beyond initialization in the current decompilation. It is retained
for layout compatibility and restored only if the referenced prop is already
a child of the CHR; no attachment-node semantics are inferred for it.
`handle_positiondata_hat` is the independently used headwear pointer.

An attached prop uses `parent` for the owning CHR. In this state, `prev` and
`next` are attachment-sibling links and `parent->child` points to the newest
child. When `parent` is `NULL`, `prev` and `next` instead belong to the global
active-prop list. Restoration therefore uses `chrpropDetach`,
`chrpropDelist`, `chrpropActivate`, and `chrpropReparent` rather than assigning
these pointers independently. Equipment held after the save but absent from
the saved slots is detached and returned to the active list; saved equipment
which was dropped later is removed from its rooms and active list before being
reattached.

The equipment model's `attachedto` points to the live CHR model.
`attachedto_objinst` selects model switch `3` for the right hand, `5` for the
left hand, and `6` for the head/hat. Attached objects also receive
`RUNTIMEBITFLAG_HASOWNER` (`0x00080000`) and are disabled while parented.

Each hand also serializes its weapon's muzzle flash visibility
(`gunfire_visible[hand]`), read via `weaponIsGunfireVisible`. This is the
GUNFIRE-node `visible` flag in the weapon model's RwData — a latched effect set
when the CHR fires (`chrSetFiring`) and only cleared on stop-firing, so it does
not reconstruct itself on load. It is reapplied with `weaponSetGunfireVisible`
as the final step of `restore_chr_attachments`, after each weapon is reattached,
because weapon creation/attachment resets the node to hidden. The call is a
no-op on non-weapon-skeleton props. See the "Muzzle Flash" learning in
`INSTRUCTIONS.md`.
Reattaching a weapon dropped after the save clears stale
`RUNTIMEBITFLAG_DEPOSIT`/`RUNTIMEBITFLAG_EMBEDDED` state because the associated
projectile and embedment pools have already been restored independently.
Detaching clears ownership and the attachment node, then activates and enables
the prop in the world. Together these fields make rendering, dropping, firing,
and hat-hit handling agree with the CHR pointer fields.

The ninth CHR serialization slice restores the first action batch and its
live model-animation controller:

```c
ChrRecord::actiontype;       /* Supported simple and navigation ACT_TYPE values. */
ChrRecord::act_stand;        /* Active stand-facing and wall-check fields. */
ChrRecord::act_runpos;       /* Destination, arrival distance, ETA, turn speed. */
Model::anim, anim2;          /* Animation-table identities, encoded as offsets. */
Model primary/merge frames;  /* Current frame pairs and fractional positions. */
Model speed state;           /* Current/target/old speed and interpolation clocks. */
Model loop state;            /* Loop enable, loop frame, and loop merge duration. */
Model play-speed state;      /* Current/target play rate and interpolation clocks. */
Model root RW data;          /* Pointer-free animation translation/heading state. */
```

Every `ACT_TYPE` discriminator is now supported: `ACT_INIT`, `ACT_STAND`,
`ACT_KNEEL`, `ACT_ANIM`, `ACT_DIE`, `ACT_DEAD`, `ACT_ARGH`, `ACT_PREARGH`,
`ACT_ATTACK`, `ACT_ATTACKWALK`, `ACT_ATTACKROLL`, `ACT_SIDESTEP`,
`ACT_JUMPOUT`, `ACT_RUNPOS`, `ACT_PATROL`, `ACT_GOPOS`,
`ACT_SURRENDER`, `ACT_LOOKATTARGET`, `ACT_SURPRISED`, `ACT_STARTALARM`,
`ACT_THROWGRENADE`, `ACT_TURNDIR`, `ACT_TEST`, `ACT_BONDINTRO`, `ACT_BONDDIE`,
`ACT_BONDMULTI`, and `ACT_NULL`. `is_supported_chr_action` still rejects a
combat or `ACT_BONDMULTI` action whose `animfloats` pointer does not map to a
registered firing-table row; such an action writes no action/model payload and
loading leaves the destination action live.

The action records occupy one 120-byte union. The payload does not copy those
120 bytes wholesale: doing so would waste substantial SRAM per CHR and would
serialize pointer representations from attack, patrol, go-position, and
player-action layouts. Instead, every meaningful field of each supported
active member is written, with pointer fields converted to stable identities.
Inactive overlay bytes are omitted; the engine neither reads nor initializes
them for the supported action.

`Model::anim` and `anim2` point into the permanent `ptr_animation_table`.
They are serialized as signed byte offsets from `ptr_animation_table->data`;
`-1` represents `NULL`. This reconstructs valid pointers for both an existing
model and a model newly allocated by the gated replacement path. No saved
address or existing-model identity is reused.

The primary frame is represented by `framea`, `frameb`, `unk28` (the current
fractional frame), and `unk2c` (the interpolation fraction). The merge/previous
animation uses the corresponding `frame2a`, `frame2b`, `unk58`, and `unk5c`
fields. Frame values are signed 16-bit animation-frame indices and must remain
within the selected animation. `endframe` and `unk6c` are end-frame overrides;
`-1.0` means use the animation's natural final frame.

`speed` is the current frame advance per animation tick. `newspeed`,
`oldspeed`, `timespeed`, and `elapsespeed` describe an in-progress speed
transition. The equivalent secondary-animation values, loop controls,
merge weights/clocks, `playspeed`, and play-speed transition fields are
restored together because advancing only part of this controller can select
different frames after loading. Cached decoded-frame slots are not serialized;
matrix calculation regenerates them from the restored animation and frame
indices. `animflipfunc` is cleared rather than treated as data because it is a
function pointer and no GoldenEye caller installs one for these CHR actions.

Character models use a `MODELNODE_OPCODE_HEADER` root whose
`ModelRwData_HeaderRecord` contains only bytes, coordinates, angles, and
interpolation values. The complete record is copied after resolving the model
allocation. This includes the primary and merge translation/heading working
values that `modelSetAnimation` normally initializes, so replacement mode does
not assume that the destination model was already playing the saved animation.

The tenth CHR serialization slice adds the two navigation actions:

```c
ChrRecord::act_patrol; /* Path ID, step/direction, waydata, visibility tick, speed. */
ChrRecord::act_gopos;  /* Destination, route indices, waydata, clocks, speed mode. */
```

Patrol and go-position use the same live model-animation payload as the simple
actions. Their movement also depends on the already restored prop position,
stand tile, rooms, collision bounds, previous position, ground state, and
movement timestamps.

The payload is implemented in `practice_states_chr.c` and dispatched by
`practice_states_props.c`. For CHRs, only the spatial subset of the common
`PropRecord` payload (`pos`, `stan`, and `rooms`) is restored. The remaining
common fields stay live until their dependencies are implemented.

Do not include the following in the implemented slices:

- The complete `hidden` and `chrflags` fields. They still contain destructive
  and allocation-coupled bits (item drops, character removal, freezing, cloning,
  initialization, and AI-event latches) which must be restored with the
  allocation/teardown work, not speculatively. Only the documented safe subsets
  are restored. (`damage`, `maxdamage`, `fadealpha`, `CHRFLAG_INVINCIBLE`, the
  hit-reaction actions `ACT_ARGH`/`ACT_PREARGH`, and the death actions
  `ACT_DIE`/`ACT_DEAD` are now restored by the nineteenth through twenty-first
  slices; see below.)
The replacement path does not retain its `ChrRecord` or `Model` back-pointers:
it recreates them first, then applies spatial/model state and resolves
attachments against the rebuilt prop table. Allocation currently uses zero
model spawn flags, so model variants represented only by spawn flags are not
yet distinguished by `ChrAllocationState`; add them there if such variants are
found to load incorrectly.

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
fields for the named action. No GoldenEye setter or tick function reads or
writes union bytes while any of these discriminators is active, so they are
implemented as payload-free actions.

The decompilation sometimes accesses known fields through `act_init.padding`,
but only while another typed action is active: `[0]` aliases the first field
of `act_dead`, `act_die`, and `act_argh`; `[1]` and `[3]` alias
`act_anim.holdLastFrame` and `idleOnEnd`; and `[0x13]` aliases
`act_patrol.lastvisible60`. These are alternate views of typed fields, not
state belonging to `ACT_INIT` or the other marker actions.

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
    f32 mergetime;        /* 0x1c - Inactive/uninitialized in GoldenEye. */
    s8  face_target;      /* 0x20 - Inactive/uninitialized in GoldenEye. */
};
```

`face_entitytype` uses the same target-selection bits as `ATTACKTYPE` where the
standing code aims at an entity: Bond `0x01`, forward `0x02`, character `0x04`,
pad `0x08`, direction `0x10`, aim-only `0x20`, do-not-turn `0x40`, and current
target `0x80`. Some standing paths also use `0x10` as a transient direction
target, so it must be interpreted with the active code path.

`prestand`, `reaim`, and `checkfacingwall` are Boolean values. `turning` uses
`1` while turning, `2` when the animation frame has reached its terminal
frame, and `3` when the requested heading has been reached; stand
initialization starts it at `2`. `wallcount` starts at a random value from
180 through 299 and is reduced on stand updates until a new facing check is
performed. `mergetime` and `face_target` have no reads, writes, or
initialization in the GoldenEye codebase; they are inactive union bytes and
are deliberately not serialized.

### `act_anim`

Parameters for an AI-requested animation. The live `Model` holds the actual
animation pointer, frame, speed, loop, and interpolation state.

```c
struct act_anim {
    u32 unk02c;        /* 0x00 - Boolean from ANIM_UNKNOWN (0x02). */
    u32 holdLastFrame; /* 0x04 - Boolean from ANIM_LOOP_HOLD_LAST_FRAME. */
    u32 playSfx;       /* 0x08 - Boolean from ANIM_PLAY_SFX. */
    u32 idleOnEnd;     /* 0x0c - Boolean from ANIM_IDLE_POSE_WHEN_COMPLETE. */
    u32 noTranslate;   /* 0x10 - Boolean from ANIM_NO_TRANSLATION. */
    u8  unk040;        /* 0x14 - Inactive/uninitialized in GoldenEye. */
    u8  unk041;        /* 0x15 - Inactive/uninitialized in GoldenEye. */
    u16 unk042;        /* 0x16 - Inactive/uninitialized in GoldenEye. */
    u16 unk044;        /* 0x18 - Inactive/uninitialized in GoldenEye. */
    u16 unk046;        /* 0x1a - Inactive/uninitialized in GoldenEye. */
    s16 animnum;       /* 0x1c - Inactive/uninitialized in GoldenEye. */
    u8  flip;          /* 0x1e - Inactive/uninitialized in GoldenEye. */
    f32 startframe;    /* 0x20 - Inactive/uninitialized in GoldenEye. */
    f32 endframe;      /* 0x24 - Inactive/uninitialized in GoldenEye. */
    f32 unk054;        /* 0x28 - Inactive/uninitialized in GoldenEye. */
    f32 unk058;        /* 0x2c - Inactive/uninitialized in GoldenEye. */
};
```

GoldenEye initializes only the first five Boolean members. The apparent
animation identity, frame, and flip fields are not read or written by this
game; the live `Model` owns those values. They are inactive union overlay bytes
and are deliberately omitted from the save payload.

### `act_die`

State for an active death animation, including sound-trigger frames. Restored by
the twenty-first slice.

```c
struct act_die {
    s32     notifychrindex;     /* 0x00 - Guard-notification scan cursor (aliased as act_init.padding[0]). */
    f32     thudframe1;         /* 0x04 - First thud SFX frame; -1 after firing/none. */
    f32     thudframe2;         /* 0x08 - Second thud SFX frame; -1 after firing/none. */
    f32     timeextra;          /* 0x0c - Written at death entry; never read. Not serialized. */
    f32     elapseextra;        /* 0x10 - Written at death entry; never read. Not serialized. */
    coord3d extraspeed;         /* 0x14 - Written at death entry; never read. Not serialized. */
    s16     drcarollimagedelay; /* 0x20 - No reader or writer in GoldenEye. Not serialized. */
};
```

The destructive death consequences (item drops, body count, `chrStopFiring`)
fire in `triggered_on_shot_hit` when the character *enters* `ACT_DIE`, so
restoring this already-active action does not replay them. Only the
guard-scan cursor and the two one-shot thud frames are live tick state and are
serialized; the post-impact-motion fields are write-only and omitted.

### `act_dead`

Corpse lifetime state. Restored by the twenty-first slice.

```c
struct act_dead {
    bool allowfade;      /* 0x00 - Set to -1 on entry; this word is the fade timer (act_init.padding[0]). */
    bool allowreap;      /* 0x04 - No reader. Not serialized. */
    s32  reaptimer;      /* 0x08 - No reader. Not serialized. */
    s32  fadetimer;      /* 0x0c - No reader. Not serialized. */
    s32  notifychrindex; /* 0x10 - No reader. Not serialized. */
};
```

Despite the named fields, `chrlvTickDead` drives the corpse entirely through the
offset-0 word read as `int act_init.padding[0]`: `-1` immediately after entry,
then an up-counter from which `fadealpha` is derived and which triggers
`CHRHIDDEN_REMOVE` at `CHRLV_TICK_DEAD_CHECK`. Only that word is serialized.

### `act_argh`

Immediate damage-flinch state. Restored by the twentieth slice.

```c
struct act_argh {
    s32 notifychrindex; /* 0x00 - Guard-notification scan cursor (aliased as act_init.padding[0]). */
    s32 unk30;          /* 0x04 - Global tick captured for the hit reaction. */
};
```

### `act_preargh`

Deferred hit information consumed when the character transitions into a flinch
or death action. Restored by the twentieth slice; `unk044` has no reader or
writer in GoldenEye and is omitted.

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
    s8  unk30;      /* 0x04 - Heading/turn interpolation state. */
    s8  unk31;      /* 0x05 - Boolean: firing window entered for this phase. */
    s8  unk32;      /* 0x06 - Active hand index, GUNRIGHT/GUNLEFT (0/1). */
    s8  unk33;      /* 0x07 - Completed firing-window count. */
    s8  unk34;      /* 0x08 - Number of firing windows requested. */
    s8  unk35;      /* 0x09 - Inactive for ACT_ATTACK. */
    s8  unk36;      /* 0x0a - Boolean attack-animation transition mode. */
    s8  unk37;      /* 0x0b - Boolean alternate end-frame mode. */
    s8  unk38[2];   /* 0x0c - Per-hand enabled/aim phase bytes. */
    s8  unk3a[2];   /* 0x0e - Per-hand firing-window completion bytes. */
    s8  unk3c[2];   /* 0x10 - Per-hand firing/weapon phase bytes. */
    s8  unk3e, unk3f; /* 0x12 - Inactive padding in GoldenEye. */
    u32 unk40;      /* 0x14 - Firing-window tick accumulator. */
    s32 unk44;      /* 0x18 - Global tick of the latest phase transition. */
    s32 attack_time;/* 0x1c - Global tick of the latest emitted attack. */
    u32 attacktype;                                    /* 0x20 - ATTACKTYPE bitfield. */
    u32 entityid;                                      /* 0x24 - Target ID for CHR/PAD/DIRECTION. */
    u32 unk54;      /* 0x28 - Boolean: run the retarget transition callback. */
    s32 type_of_motion; /* 0x2c - Re-aim state: 0 normal, 1 entering, 2 ending. */
    u32 unk5C, unk60, unk64, unk68, unk6c; /* 0x30 - Inactive tail storage. */
    u32 unk70, unk74, unk78, unk7c;        /* 0x44 - Inactive tail storage. */
    s8  attack_item;                                   /* 0x54 - ITEM_IDS weapon ID. */
    u8  unk81, unk82, unk83; /* 0x55 - Inactive tail bytes in GoldenEye. */
};
```

Valid `attacktype` bits are `ATTACKTYPE_BOND` (`0x01`),
`ATTACKTYPE_FORWARD` (`0x02`), `ATTACKTYPE_CHR` (`0x04`),
`ATTACKTYPE_PAD` (`0x08`), `ATTACKTYPE_DIRECTION` (`0x10`),
`ATTACKTYPE_AIMONLY` (`0x20`), `ATTACKTYPE_DONTTURN` (`0x40`), and
`ATTACKTYPE_TARGET` (`0x80`); zero means no attack target.

`animfloats` is encoded as the stable table/row identity described above.
Signed phase bytes use their full `-128..127` storage range, though initialized
Boolean/hand values are normally `0` or `1`. `unk33` and `unk34` are small
nonnegative volley counters. The fields marked inactive have no GoldenEye
reads or initialization and are not serialized.

### `act_attackwalk`

Moving attack state. It parallels `act_attack` but adds movement speed and
separate clocks.

```c
struct act_attackwalk {
    u32 unk02c;                          /* 0x00 - Inactive overlay storage. */
    s32 clock_timer30;                   /* 0x04 - Elapsed action ticks. */
    s32 clock_timer34;                   /* 0x08 - Randomized action deadline. */
    u32 unk038;                          /* 0x0c - Boolean movement completed/failed. */
    struct weapon_firing_animation_table *animfloats; /* 0x10 - Static table row. */
    s32 timer40;                         /* 0x14 - Next/previous firing phase tick. */
    s32 unk044;                          /* 0x18 - Active hand index, 0 or 1. */
    s8  unk48[2], unk4a[2], unk4C[2];   /* 0x1c - Per-hand firing phase bytes. */
    u8  flip;                            /* 0x22 - Inactive; flip lives in Model. */
    s8  unk4f;                           /* 0x23 - Inactive overlay storage. */
    s32 unk50;                           /* 0x24 - Inactive overlay storage. */
    f32 speed;                           /* 0x28 - Current movement speed. */
    u32 unk58, unk5C, unk60, unk64;      /* 0x2c - Inactive tail storage. */
    u32 unk68, unk6c, unk70, unk74;      /* 0x3c - Inactive tail storage. */
    u32 unk78, unk7c;                    /* 0x4c - Inactive tail storage. */
    s8  attack_item;                     /* 0x54 - ITEM_IDS weapon ID. */
    u8  unk81, unk82, unk83;             /* 0x55 - Inactive tail bytes. */
};
```

The elapsed clock starts at zero and the deadline is initialized to a randomized
reaction interval. `speed` is updated by `chrlvApplySpeed`; the already restored
position, `prevpos`, fall state, collision bounds, and model root keep that
movement coherent. Only the fields consumed by the attack-walk tick are saved.

### `act_attackroll`

Evasive-roll attack state. Its leading firing fields parallel `act_attack`;
the middle arrays hold roll-specific state.

```c
struct act_attackroll {
    struct weapon_firing_animation_table *animfloats; /* 0x00 - Static table row. */
    s8  unk30, unk31, unk32, unk33; /* 0x04 - Turn/hand/volley phase bytes. */
    s8  unk34;                     /* 0x08 - Requested firing-window count. */
    s8  unk35;                     /* 0x09 - Boolean roll-transition variant. */
    s8  unk36, unk37;              /* 0x0a - Animation transition mode bytes. */
    s8  unk38[2], unk3a[2];        /* 0x0c - Per-hand phase bytes. */
    s8  unk3c[2], unk3e, unk3f;    /* 0x10 - Per-hand bytes then inactive padding. */
    u32 unk40;                     /* 0x14 - Firing-window tick accumulator. */
    s32 unk44;                     /* 0x18 - Global tick of latest phase transition. */
    s32 attack_time;               /* 0x1c - Global tick of latest emitted attack. */
    s32 unk4c[2], unk54[2];        /* 0x20 - Roll target/re-aim transition words. */
    u32 unk5C, unk60, unk64, unk68, unk6c; /* 0x30 - Inactive tail storage. */
    u32 unk70, unk74, unk78, unk7c;        /* 0x44 - Inactive tail storage. */
    s8  attack_item;                                   /* 0x54 - ITEM_IDS weapon ID. */
    u8  unk81, unk82, unk83; /* 0x55 - Inactive tail bytes in GoldenEye. */
};
```

The leading bytes share stationary-attack semantics. The four transition words
are read through both the roll member and compatible stationary-action aliases;
all four are therefore restored. Tail storage with no readers is omitted.

### `act_sidestep` and `act_jumpout`

```c
struct act_sidestep { bool side; }; /* Inactive/uninitialized in GoldenEye. */
struct act_jumpout  { bool side; }; /* Inactive/uninitialized in GoldenEye. */
```

GoldenEye never reads or assigns either declared `side` member. Direction is
encoded by the live animation identity and `Model::gunhand`/flip byte instead.
These actions therefore have no serialized union payload; restoring their
action discriminator and model controller is complete for the current code.

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
    u8   unk04; /* 0x04 - Level-local path ID, 0-255. */
    u8   flags; /* 0x05 - Bit 0: loop from the end to the start. */
    u16  len;   /* 0x06 - Number of path entries. */
};
```

This is layout-compatible with the setup `PathRecord`: `data` aliases its
`waypoints` member and `unk04` aliases `ID`. The save payload stores the ID and
resolves it with `pathFindById`; `-1` is the serialized `NULL` sentinel.
Neither `path` nor `data` is saved as an address. When flag bit 0 is clear,
patrol reverses at either end; when set, it wraps to the opposite end.

### `waydata`

Incremental path-travel state embedded by patrol and go-to-position actions.

```c
struct waydata {
    s8      mode;         /* 0x00 - WAYMODE_0..WAYMODE_5 or WAYMODE_MAGIC (0-6). */
    s8      unk01;        /* 0x01 - Detour-probe retry/phase counter, normally 0-6. */
    s8      unk02;        /* 0x02 - Boolean: direct/path candidate was accepted. */
    s8      unk03;        /* 0x03 - Boolean: retain pos_copy as a detour target. */
    coord3d pos;          /* 0x04 - Current path segment position. */
    coord3d pos2;         /* 0x10 - First lateral detour candidate. */
    coord3d pos3;         /* 0x1c - Second lateral detour candidate. */
    s32     age;          /* 0x28 - Number of updates in current path state. */
    coord3d pos_copy;     /* 0x2c - Saved/copy position used by travel logic. */
    f32     segdistdone;  /* 0x38 - Distance completed in cheap travel mode. */
    f32     segdisttotal; /* 0x3c - Total segment distance in cheap travel mode. */
};
```

Modes `0` and `2` test direct travel and generate the two lateral candidates.
Modes `1` and `3` probe those candidates over several updates. Mode `4`
validates the selected detour, and mode `5` refines a failed detour by choosing
the candidate with the smaller angular deviation. `WAYMODE_MAGIC` (`6`)
selects cheaper off-screen interpolation using `segdistdone` and
`segdisttotal`. `age` advances once per action tick and schedules route,
door, and shortcut checks at ten-tick intervals.

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
`waydata`, position, stand tile, room, and model movement state. These are now
restored together. `nextstep` is an index from `0` through `path->len - 1`;
`forward` is Boolean. `lastvisible60` is an absolute `g_GlobalTimer` value used
to decide when an off-screen patrol may enter cheap travel. `speed` is the
current turn/speed interpolation value updated by `chrlvApplySpeed`.

The nine trailing `unk80` through `unka0` words have no readers or writers in
GoldenEye. They are inactive union storage and are not serialized.

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
    u8         curindex;                       /* 0x2c - Current route entry, 0-3. */
    u8         unk59;                          /* 0x2d - SPEED: walk 0, run 1, sprint 2. */
    u16        unk5a;                          /* 0x2e - Route-restart countdown. */
    struct waydata waydata;                    /* 0x30 - Current segment state. */
    s32        unk9c;                          /* 0x70 - Last path update tick. */
    f32        speed;                          /* 0x74 - Current movement speed. */
};
```

The route array contains six entries and uses a `NULL` entry to transition from
the final graph waypoint to `targetpos`. When route advancement passes entry
3, the remaining route is recalculated into the same six slots and
`curindex` becomes 1. `unk5a` is initialized from the estimated travel time
plus 300 NTSC ticks, counts down by `g_ClockTimer`, and triggers replanning at
zero. `unk9c` is an absolute global tick for visible/off-screen route updates.

`target` is serialized as a stand-tile offset. `target_path` and all six route
entries are signed 16-bit indices into `g_CurrentSetup.pathwaypoints`; `-1`
represents `NULL`. These are resolved against the current level for both
retained and newly allocated CHRs, so no existing pointer is assumed.

The eleventh CHR serialization slice adds grenade-throw actions:

```c
ChrRecord::actiontype;        /* ACT_THROWGRENADE discriminator. */
Model animation/controller;   /* Throw identity, frame, speed, flip, and hand. */
ChrRecord::weapons_held[0..1];/* Existing or recreated grenade attachment. */
```

`chrlvThrowGrenadeAnimationRelated` changes the action discriminator and starts
`ANIM_DATA_fire_throw_grenade`, but does not initialize any member of
`act_throwgrenade`. `chrlvTickThrowGrenade` likewise reads only the live model
frame and `Model::gunhand`, then resolves the corresponding held weapon through
`weapons_held`. The action-union payload is therefore empty in GoldenEye; the
declared members are inactive overlay bytes and are not serialized.

The common model payload restores the exact throw frame, end frame, play speed,
flip/hand selection, and interpolation state. The equipment slice restores the
selected grenade prop and its model attachment. A grenade attachment that does
not have a standalone saved prop is recreated from its serialized model ID,
`ITEM_GRENADE` weapon ID, and object flags before the next character tick. This
avoids relying on either a retained held-prop pointer or an existing character
model.

At animation frame 20 the grenade becomes visible, at frame 61 its weapon timer
is armed, and at frame 119 it is marked dropped and
`CHRHIDDEN_DROP_HELD_ITEMS` is set. Those effects remain derived transitions:
loading a frame before a threshold lets the tick perform it again, while
loading a frame after a threshold uses the restored attachment/object state.
The hidden bit is not an input to `chrlvTickThrowGrenade`, so it remains in the
damage/lifecycle batch rather than being restored speculatively here.

The twelfth CHR serialization slice adds every other payload-free action:

```c
ACT_INIT;         /* Converts itself to initialized standing on the next tick. */
ACT_KNEEL;        /* Holds the current kneeling model animation. */
ACT_SURRENDER;    /* Advances/loops the current surrender model animation. */
ACT_LOOKATTARGET; /* Unused GoldenEye marker; no action tick. */
ACT_STARTALARM;   /* Activates the alarm at model frame 60. */
ACT_TURNDIR;      /* Unused GoldenEye marker; no action tick. */
ACT_TEST;         /* Death-stagger model animation used by GoldenEye. */
ACT_BONDINTRO;    /* Advances the player intro model animation. */
ACT_BONDDIE;      /* Removed/no-op player-death action tick. */
ACT_NULL;         /* Explicit no-action marker; no action tick. */
```

None of these actions reads a member of the 120-byte action union. Their saved
payload is therefore the discriminator plus the common model-animation
controller when a model exists. `sleep`, equipment attachments, spatial state,
and model transforms are restored by their existing independent slices.

`ACT_INIT` is handled before the action switch: its next tick sets
`CHRFLAG_INIT`, converts the character to a fully initialized `ACT_STAND`, and
creates fresh stand-union values. It never consumes the saved union overlay.
`ACT_KNEEL` only forces `sleep` to zero. `ACT_SURRENDER` examines its model
animation/frame and may switch to the looping unarmed surrender animation;
weapon-drop ownership is represented by the already restored equipment graph,
not by action-union bytes. `ACT_STARTALARM`, `ACT_TEST`, and `ACT_BONDINTRO`
likewise branch exclusively on model identity/frame.

`ACT_LOOKATTARGET` and `ACT_TURNDIR` have neither setters nor tick cases in the
GoldenEye codebase. `ACT_BONDDIE` has a dispatched but empty tick, and
`ACT_NULL` has no tick case. They are retained enum/layout markers; restoring
their discriminator and model state exactly preserves their inert behavior.
Damage/lifecycle actions remain excluded.

The thirteenth CHR serialization slice adds `ACT_ANIM`:

```c
ChrRecord::act_anim.unk02c;        /* Boolean ANIM_UNKNOWN option. */
ChrRecord::act_anim.holdLastFrame; /* Boolean hold/loop terminal-frame option. */
ChrRecord::act_anim.playSfx;       /* Boolean ambient-animation/SFX option. */
ChrRecord::act_anim.idleOnEnd;     /* Boolean idle-after-completion option. */
ChrRecord::act_anim.noTranslate;   /* Boolean no-root-translation option. */
CHRFLAG_02000000;                  /* Sneeze SFX has crossed its trigger frame. */
```

All five action values are normalized to `0` or `1`. They originate from the
`PlayAnimation` command's option byte: `ANIM_UNKNOWN` (`0x02`),
`ANIM_LOOP_HOLD_LAST_FRAME` (`0x04`), `ANIM_PLAY_SFX` (`0x08`),
`ANIM_IDLE_POSE_WHEN_COMPLETE` (`0x10`), and `ANIM_NO_TRANSLATION` (`0x40`).
The animation identity, frame range, direction, flip, speed, translation, and
interpolation state already live in the common `Model` payload, so no static
animation pointer is added to the action payload.

`holdLastFrame == 0` makes `chrlvTickAnim` return to a normal standing/kneeling
state when the model reaches its terminal frame; a nonzero value retains the
action. `idleOnEnd` causes the action to keep assigning short sleep intervals
after completion. `playSfx` also makes `chrHasStoppedOrPatroling` treat the
animation as interruptible/stopped. `unk02c` and `noTranslate` are initialized
from command options but have no later direct `ChrRecord` consumers in the
current decompilation; they are still serialized because they are defined live
action configuration rather than stale overlay bytes.

`CHRFLAG_02000000` is cleared whenever `ACT_ANIM` starts and set when the sneeze
animation crosses frame 42. It suppresses replay of that one-shot sneeze sound.
Only this bit is merged into `chrflags`; all unrelated lifecycle and behavior
bits remain untouched. This is safe for both retained CHRs and newly allocated
replacement-mode CHRs and does not serialize an audio pointer.

The fourteenth CHR serialization slice adds the three combat actions and their
shared aiming/firing state:

```c
ChrRecord::act_attack;      /* Stationary attack phase and target state. */
ChrRecord::act_attackwalk;  /* Moving attack clocks, hand phases, and speed. */
ChrRecord::act_attackroll;  /* Roll attack phase and transition state. */
ChrRecord::firecount[2];    /* Per-hand trigger/update counts, 0-255. */
ChrRecord::aimendcount;     /* Remaining aim interpolation ticks. */
Eight shoulder/back angles; /* Current and target upper-body angles, radians. */
ChrRecord::fireslot_word;   /* Complete legacy per-hand firing-slot storage. */
ChrRecord::field_178[2];    /* Per-hand absolute sound/tracer throttle ticks. */
ChrRecord::unk180[2];       /* Per-hand beam/tracer endpoint caches. */
Combat hidden bits;         /* Fire-left/right, tracer, and moving bits. */
```

Every `animfloats` pointer is encoded as a 16-bit table/row identity. The high
byte selects one of the permanent rifle, pistol, dual-weapon, crouched, roll,
or attack-walk tables, and the low byte selects a validated row. `-1` represents
`NULL` or an unrecognized pointer. A combat action is serialized only when its
live pointer maps to a registered row, so loading can never install an
integer, stale address, or unvalidated pointer. Replacement mode resolves the
same static row before the first combat tick and does not rely on the previous
CHR allocation.

The action payload writes only fields initialized or consumed by GoldenEye.
For stationary attacks these are heading/volley phase bytes, active-hand and
per-hand phase arrays, phase clocks, last-shot ticks, target selector and ID,
re-aim state, and the signed `ITEM_IDS` weapon ID. Attack-walk stores its
elapsed/deadline clocks, movement-result flag, per-hand firing phases, current
movement speed, and weapon ID. Attack-roll stores the stationary-prefix phase
bytes plus its four roll/target transition words and weapon ID. Unreferenced
tail words and padding in the declarations remain omitted.

`firecount[0]` and `[1]` belong to the right and left hands. Automatic weapons
use them modulo the weapon firing rate; semiautomatic weapons increment and
decrement them around individual trigger events. `aimendcount` is normally set
to `10`, decreases by `g_ClockTimer`, and blends the four current angles toward
the four target angles. The shoulder and back values are radians and may be
positive or negative within limits supplied by the selected firing table.

`fireslot_word` initializes to zero. Its two named signed-byte slots are legacy
per-hand firing state; the current C decompilation has no named consumer, so
the complete 32-bit union storage is preserved rather than inferring which
bytes are inactive. `field_178[hand]` is compared with `g_GlobalTimer` to
throttle per-hand firing sounds/tracers and is restored as an absolute tick.

`unk180[hand]` contains an activity byte (`-1` means inactive), the associated
`ITEM_IDS` value, two small state bytes, two world-space vectors, and five
floating-point beam length/interpolation values. It contains no pointers, so
the complete cache is safe to serialize. The restored hidden subset consists
of `CHRHIDDEN_FIRE_WEAPON_LEFT` (`0x0004`),
`CHRHIDDEN_FIRE_WEAPON_RIGHT` (`0x0008`), `CHRHIDDEN_FIRE_TRACER` (`0x0080`),
and `CHRHIDDEN_MOVING` (`0x0100`). Only those bits are merged; destructive and
lifecycle bits remain live.

`ptr_SEbuffer3` and `ptr_SEbuffer4` are dynamic `ALSoundState` addresses and
are never serialized. Loading deactivates any currently playing nodes and
sets both pointers to `NULL`; subsequent combat ticks may allocate fresh sound
state. The same clearing is safe for a newly allocated CHR, whose pointers
start as `NULL`.

The fifteenth CHR serialization slice adds `ACT_BONDMULTI`:

```c
ChrRecord::act_bondmulti.unk2c; /* Static firing-animation table row, or NULL. */
```

Despite its name, GoldenEye has no setter or action-tick case for
`ACT_BONDMULTI`; it appears to be a retained multiplayer-player action from a
related engine version. Its only GoldenEye consumer is
`chrlvGetSubrotySideback`, which reads element `3` as the same side/back heading
offset read from `weapon_firing_animation_table::anonymous_3` for
`ACT_ATTACK` and `ACT_ATTACKROLL`. The pointer type and access layout therefore
identify `unk2c` as a pointer to one of the permanent firing-animation table
rows.

The pointer is serialized with the same validated 16-bit table/row identity
used by combat actions; `-1` represents `NULL`. An `ACT_BONDMULTI` payload is
saved only when this pointer is `NULL` or maps to a registered row. Loading
resolves the identity against the static tables even for a newly allocated
replacement-mode CHR, so it never depends on a retained action pointer or
existing prop.

The common model-animation payload separately restores the live animation,
frames, speeds, interpolation, looping, and root transform. The four following
named words in `act_bondmulti` have no reads or writes anywhere in GoldenEye
and are inactive union storage, not live animation state; they are deliberately
omitted.

The sixteenth CHR serialization slice handles the transient render list:

```c
ChrRecord::field_20; /* Head of the current skeletal joint render-node list. */
```

`field_20` is either `NULL` or the head of a linked list drawn by
`drawjointlist`. Each node contains a live `Model *`, a `ModelNode *`, a joint
size, and pool links. The nodes come from the 600-entry global render-node pool
initialized by `sub_GAME_7F005450`; their addresses and contents are derived
from the current model traversal and must not be serialized.

Character rendering returns the preceding list to the pool with
`sub_GAME_7F06B248`, clears `field_20`, and builds a fresh list from the current
model and visible joints. Loading now performs the same return-and-clear
operation before applying saved CHR state. The next render rebuilds the list
from the restored model. This prevents stale model/node pointers and also
prevents leaking pool entries across repeated loads.

In replacement mode, `init_GUARDdata_with_set_values` initializes a newly
created CHR's `field_20` to `NULL`, so the same path is a no-op and does not
assume a retained character or model allocation. No bytes are added to the
save payload because this is derived, frame-local state.

The seventeenth CHR serialization slice restores the independent render-flinch
controller:

```c
ChrRecord::flinchcnt;   /* Current render-flinch phase, or -1 when inactive. */
CHRHIDDEN_RAND_FLINCH_MASK; /* Four random flinch-direction bits. */
```

`flinchcnt` is a signed render-time phase. `-1` means inactive.
`chrSetHiddenToRandom` starts a flinch at `1`, choosing zero or one bit from
each of the pairs `0x1000`/`0x2000` and `0x4000`/`0x8000`. The resulting nine
direction combinations control small procedural rotations applied while
character matrices are calculated. Visible-character rendering advances the
counter by `g_ClockTimer` and returns it to `-1` at the terminal phase (30
NTSC ticks in the practice build).

The counter and all four direction bits are restored together. Only
`CHRHIDDEN_RAND_FLINCH_MASK` is merged into `hidden`; removal, freezing,
item-drop, background-AI, and other lifecycle bits remain untouched. A newly
allocated replacement CHR starts with `flinchcnt == -1` and no hidden bits,
then receives the same saved scalar/mask pair, so this does not rely on a
retained prop, model, or previous random choice.

`maxdamage` remains deferred with `damage`: restoring only the threshold could
leave a retained character's current damage already above it without the
corresponding fatal action transition.

The eighteenth CHR serialization slice restores this durable behavior/render
configuration subset of `chrflags`:

```c
CHRSTART_FORCENOBLOOD;           /* Suppress blood effects. */
CHRFLAG_CAN_SHOOT_CHRS;          /* Permit firing at other characters. */
CHRFLAG_NO_AUTOAIM;              /* Exclude the character from Bond's autoaim. */
CHRFLAG_LOCK_Y_POS;              /* Keep the character's vertical position fixed. */
CHRFLAG_NO_SHADOW;               /* Do not render the character's floor shadow. */
CHRFLAG_IGNORE_ANIM_TRANSLATION; /* Ignore animation root translation. */
CHRFLAG_IMPACT_ALWAYS;           /* Apply fallback fatal-hit impact force. */
CHRFLAG_INCREASE_RUNNING_SPEED;  /* Select the faster running speed. */
CHRFLAG_COUNT_DEATH_AS_CIVILIAN; /* Count a future death as a civilian casualty. */
CHRFLAG_CULL_USING_HITBOX;       /* Use model hitbox bounds for view culling. */
```

Each value is an independent Boolean bit and arbitrary combinations are valid.
The bits control future damage visuals, targeting eligibility, movement/model
translation, rendering, fatal-hit impulse selection, travel speed, and mission
death classification. None is a pointer, one-shot latch, action discriminator,
or allocation result. They are restored as one masked 32-bit value, preserving
every unlisted `chrflags` bit.

The mask works for both retained and replacement-mode CHRs. A replacement CHR
receives defaults from `init_GUARDdata_with_set_values` and then the saved
configuration bits, without depending on the previous allocation. Spatial,
fall-speed, model-animation, and collision state used by the lock-Y,
ignore-translation, and hitbox-culling options are already restored by their
existing slices.

`CHRFLAG_INVINCIBLE` is deferred to the damage slice (the nineteenth slice
below) because changing it determines whether intervening hits accumulated
damage, so it is restored together with `damage`/`maxdamage`. `CHRFLAG_CLONE` and
`CHRFLAG_HAS_BEEN_ON_SCREEN` remain with allocation/recreation because they
gate character spawning. Initialization, near-miss, damage-event, hidden,
forced-action-tick, fade-mode, crushed-death, unknown, and other one-shot bits
also remain deferred. `CHRFLAG_02000000` continues to be restored only for
`ACT_ANIM`, where it is the sneeze-SFX trigger latch.

The nineteenth CHR serialization slice restores the damage scalars and the
render fade level:

```c
ChrRecord::fadealpha; /* Render alpha, 0x00 transparent .. 0xFF opaque. */
ChrRecord::damage;    /* Accumulated damage; character dies at maxdamage. */
ChrRecord::maxdamage; /* Death threshold, normally health_mod * 4.0. */
CHRFLAG_INVINCIBLE;   /* Gates whether incoming hits accumulate damage. */
```

`damage` and `maxdamage` are restored together as a pair so the
`damage < maxdamage` invariant a live character maintains is preserved. The
death transition is driven by `chrlvTick` comparing `damage >= maxdamage`;
restoring only the threshold (as the eighteenth slice warned) could leave a
retained character's current damage already above a newly applied lower
threshold and trigger an unintended death. Restoring both from the same saved,
internally consistent snapshot avoids that: a save of a live character always
has `damage < maxdamage`, and the same-ROM/same-level load restriction keeps the
health modifier baked into `maxdamage` valid. `damage` is `0.0` initially, may be
driven negative by `chrAddHealth` (armour/healing), and is read by the AI
health-threshold commands and the explosion/hit paths. `maxdamage` initializes
to `get_007_health_mod() * 4.0` and is otherwise a passive threshold. Neither is
a pointer, latch, or action discriminator.

`fadealpha` is a self-contained eight-bit render alpha that initializes to
`0xFF` (opaque). For any living character — and therefore for every supported
`actiontype` the action slices restore — it stays `0xFF`, so restoring it is
exact. It is only driven below `0xFF` by the `ACT_DEAD` corpse-fade tick, which
recomputes it every frame from the death timer, and by the player fade in
`bondview`. Restoring it therefore never strands a stale value: a corpse's tick
overwrites it on the next frame from its own (still deferred) death-timer state.
Because it carries no pointer and is recomputed wherever it is dynamic, it is
restored here rather than waiting on the death/fade batch.

`CHRFLAG_INVINCIBLE` (`0x00000010`) is restored in the same masked operation as
the behaviour-flags slice but with the damage scalars, because it determines
whether incoming hits accumulate `damage`. It is a plain behaviour bit that the
gunfire and explosion damage paths test before applying damage, and AI scripts
may set or clear it; it carries no pointer, allocation result, or one-shot
semantics. Restoring it with `damage`/`maxdamage` keeps the "did intervening hits
count" decision consistent with the saved snapshot. Every other unlisted
`chrflags` bit is preserved.

These values work identically for retained and replacement-mode CHRs: a
replacement CHR receives `damage = 0`, `maxdamage = health_mod * 4.0`,
`fadealpha = 0xFF`, and default flags from `init_GUARDdata_with_set_values`, then
the saved values, without depending on the previous allocation.

The one-shot/destructive `chrflags` and the remaining `hidden` bits remain
deferred. They drive item drops, character removal, freezing, and cloning, and
must be restored together with allocation/teardown state.

The twentieth CHR serialization slice restores the two hit-reaction actions:

```c
ChrRecord::act_argh.notifychrindex; /* Guard-notification scan cursor (offset 0). */
ChrRecord::act_argh.unk30;    /* g_GlobalTimer captured when the flinch began. */
ChrRecord::act_preargh.pos;   /* World-space hit position. */
ChrRecord::act_preargh.unk038;/* Relative shot angle, radians. */
ChrRecord::act_preargh.unk03c;/* Model hit-part index. */
ChrRecord::act_preargh.unk040;/* ITEM_IDS weapon ID that caused the hit. */
```

`ACT_ARGH` is the immediate damage flinch and `ACT_PREARGH` is a hit deferred
until the current animation finishes. They are restorable now that the damage
scalars and the common model-animation controller are restored, because their
ticks branch only on that already-restored state and on `g_GlobalTimer` (restored
by the global-state payload).

`chrlvTickArgh` advances the flinch by the model frame and, on completion,
returns the character to an idle or kneeling animation, then calls
`chrlvIterateGuardSeeShotDie`. That helper reuses the offset-0 union word —
named `act_argh.notifychrindex` here, but aliased as `act_init.padding[0]` — as a
persistent cursor that scans up to four nearby guards per tick and stamps their
`chrseeshot`. Because it is live cross-tick state (not a write-only field, as an
earlier draft assumed), it is serialized so the scan resumes from the saved
position instead of re-alerting from guard zero. `act_argh.unk30` is read only
inside `triggered_on_shot_hit`, when a *new* hit lands mid-flinch, to decide
whether to merge animations; it is an absolute `g_GlobalTimer` tick and is
serialized as such.

`chrlvTickPreArgh` waits for the model animation to finish and then calls
`triggered_on_shot_hit` with the four stored hit parameters, which re-resolves the
hit into either `ACT_ARGH` (survives) or `ACT_DIE` (dies) using the restored
`damage`/`maxdamage`. All four fields are pointer-free scalars (a coordinate, an
angle, and two enum/index integers). Re-running this resolution after a load is
correct: the save captured the pre-resolution state, so the deferred hit is
applied exactly once, exactly as it would have been without the save.
`act_preargh.unk044` has no reader or writer in GoldenEye and is omitted.

Both actions save the common model-animation payload like the other supported
actions, so the flinch/defer animation, frame, and interpolation resume exactly.

The twenty-first CHR serialization slice restores the two death actions, which
completes coverage of every `ACT_TYPE`:

```c
ChrRecord::act_die.notifychrindex; /* Guard-notification scan cursor (offset 0). */
ChrRecord::act_die.thudframe1;     /* First body-thud SFX frame; -1.0 once played. */
ChrRecord::act_die.thudframe2;     /* Second body-thud SFX frame; -1.0 once played. */
ChrRecord::act_init.padding[0];    /* ACT_DEAD corpse fade/reap timer (offset 0). */
```

The destructive consequences of *becoming* dead — `chrStopFiring`, item drops,
mission body-count bookkeeping, and the choice of death animation — all happen in
`triggered_on_shot_hit` at the moment the character *enters* `ACT_DIE`, not in the
`ACT_DIE`/`ACT_DEAD` ticks. A character that is already in one of these actions
has therefore already produced those one-shot effects, so restoring the action
discriminator and tick state does not replay them. This is what makes the death
actions restorable without the still-deferred allocation/teardown work; only
character *creation/removal* itself remains gated.

`chrlvTickDie` plays a one-shot thud SFX when the model frame reaches
`thudframe1`/`thudframe2`, then sets that frame to `-1.0` so it never replays.
Both frames are restored exactly, so a load before the frame still fires the SFX
once and a load after it stays silent. The death animation's completion (read
from the restored model controller) drives the transition to `ACT_DEAD` via
`chrlvActorFadeAway`. `act_die.notifychrindex` is the same offset-0 guard-scan
cursor described for `ACT_ARGH`. `timeextra`, `elapseextra`, `extraspeed`, and
`drcarollimagedelay` are written at death entry but never read, so they are
omitted.

`chrlvTickDead` is driven entirely by the offset-0 word, which the engine reads
as `act_init.padding[0]` (the same storage as the `bool act_dead.allowfade`). It
holds `-1` immediately after `chrlvActorFadeAway` enters `ACT_DEAD`, then counts
up by `g_ClockTimer`; `fadealpha` is recomputed from it each frame, and reaching
`CHRLV_TICK_DEAD_CHECK` sets `CHRHIDDEN_REMOVE` to retire the corpse. Restoring
this single word resumes the corpse fade at the exact saved point; the already
restored `fadealpha` matches, and the removal threshold fires on schedule. The
other named `act_dead` fields (`allowreap`, `reaptimer`, `fadetimer`,
`notifychrindex`) have no readers and are not serialized. Because the offset-0
word is accessed as a full `int`, the loader uses `act_init.padding[0]` rather
than the narrower `bool` alias.

`CHRHIDDEN_REMOVE` itself is still not serialized: it is re-set by the corpse tick
when the restored timer crosses the threshold, and actual removal proceeds on a
later tick through the engine's normal path. Item-drop and removal *bits* remain
in the deferred hidden/flag group.

### `act_surprised`

```c
struct act_surprised {
    u32 type; /* Inactive/uninitialized in GoldenEye. */
};
```

GoldenEye never reads or assigns `type`. Spotting, surrendering, and
looking-around variants are selected directly through `Model::anim`, so this
action has no serialized union payload.

### `act_throwgrenade`

Declared overlay for grenade throwing.

```c
struct act_throwgrenade {
    u32  entitytype; /* Inactive/uninitialized in GoldenEye. */
    u32  entityid;   /* Inactive/uninitialized in GoldenEye. */
    u32  hand;       /* Inactive/uninitialized in GoldenEye. */
    bool needsequip; /* Inactive/uninitialized in GoldenEye. */
};
```

No GoldenEye code reads or writes these named members. The declarations appear
to preserve a later-engine layout, but in this game the throw animation is
driven by the live model and held-weapon attachment. Direction/hand comes from
`Model::gunhand`; whether the grenade was temporarily equipped is represented
by the attached weapon object's state and the animation's starting frame.
Serializing these union bytes would preserve stale data from the preceding
action, so the implemented action payload deliberately omits them.

### `act_bondmulti`

Multiplayer player-model animation state.

```c
struct act_bondmulti {
    f32 *unk2c; /* Static weapon_firing_animation_table row, or NULL. */
    s32  unk30, unk34, unk38, unk3c; /* Inactive in GoldenEye. */
    int  padding[25];                /* Unnamed remainder of union storage. */
};
```

`unk2c` is restored by stable table/row identity as described in the fifteenth
slice. The four named words and padding have no GoldenEye readers or writers,
so saving them would preserve stale bytes from the preceding union member.

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

Rows live in static weapon/stance-specific tables and are resolved through the
stable table/row registry used by the combat payload. Columns `anonymous_1`
through `anonymous_11` are animation phase boundaries/end frames,
`anonymous_12` and `anonymous_13` are base shoulder/back angular limits,
`anonymous_14` and `anonymous_15` clamp horizontal aim, and
`anonymous_16`/`anonymous_17` scale the opposite shoulder for positive and
negative aim. The action tick consumes the static row directly after load; the
72-byte table contents are not duplicated in SRAM.

### `ChrRecord_f180`

Per-hand cache used while calculating and rendering weapon beam/tracer
endpoints. `ChrRecord::unk180[0]` is the right-hand cache and `[1]` is the
left-hand cache.

```c
typedef struct ChrRecord_f180 {
    s8      unk00;   /* Cache/activity state; initialized to -1 (inactive). */
    s8      item_id; /* ITEM_IDS value associated with the cached shot. */
    char    unk02;   /* Small beam/tracer phase byte. */
    char    unk03;   /* Small beam/tracer phase byte. */
    coord3d pos;     /* Cached beam endpoint/origin position. */
    coord3d delta;   /* Cached beam direction/delta. */
    f32     unk1c;   /* Beam-length/interpolation value. */
    f32     unk20;   /* Beam endpoint/interpolation working value. */
    f32     unk24;   /* Beam endpoint/interpolation working value. */
    f32     unk28;   /* Beam endpoint/interpolation working value. */
} ChrRecord_f180;
```

This pointer-free transient visual/weapon state is serialized with the combat
slice so a loaded tracer resumes with coherent endpoints and interpolation.

### Blood patches

Enemy blood patches are stored as cloned vertex arrays on character model
`MODELNODE_OPCODE_DLCOLLISION` nodes. A hit allocates a clone from the game's
tracked vertex pool, redirects the node's writable `Vertices` pointer away from
the model's original read-only vertices, and changes vertex alpha bytes to
select the blood shading.

The save records each modified node by its model-component identity (body or
the owning head-placeholder writable-data index), its writable-data index
within that component, its vertex count, and the per-vertex alpha values that
encode the patches. A traversal ordinal is not stable when a dead guard is
recreated because model relation state can change the nodes visited before the
patched node.

Traversal does not follow mutable `Parent` links: attached head model nodes are
shared between character instances, so those links can point into another
guard's body. Instead, traversal follows `Child`/`Next` and carries the correct
writable-data base through each head placeholder.

Loading first releases every current clone and restores the original vertex
pointers, then allocates tracked replacements, copies the original vertices,
and applies the saved alpha values. This also removes blood added after the
save and works for recreated characters without retaining model or vertex
pointers. A node counts as modified only when its writable vertex pointer is
inside the tracked `0xCCCC` vertex pool; some attached-head writable slots are
not initialized to the original vertex pointer, so pointer inequality alone
produces false positives.
