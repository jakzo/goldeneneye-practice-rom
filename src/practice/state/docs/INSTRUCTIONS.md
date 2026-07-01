# Save/Load States

This document logs our findings, analysis, and architecture specifications for the practice ROM's save/load state functionality, serving as a guide for future developers and AI agents.

You should update this document whenever new information is learned. Keep it concise and information-dense.

If you have been told to continue work on this feature, read the "current goal" section below to understand what we want to achieve. Also look at the key learnings and struct analysis for helpful information and leads to continue investigating.

Above all, make sure you investigate thoroughly to fully understand where pieces of data are used, their purpose and the implications of setting them to certain values on load. Once confident about a piece of data, add it to the save/load system, document your findings in this file, and stop and tell the user about what you found and how they can test it. Do not continue work unless the user explicitly tells you to.

## Current Goal

Props are fully implemented for save and restore, including adding props that
the save contains but the current world does not, and removing props that exist
but are absent from the save. The next goal is global state.

### Next Goals

- [done] Bond
- [done] Props (PropRecord type-by-type, including add/remove on load)
- [done] NPCs (position, animation, AI, etc.)
- [current] Global state (mission timer, objectives, etc.)

## Prompt

Read through [INSTRUCTIONS.md](src/practice/state/docs/INSTRUCTIONS.md) and implement support for saving/loading state of the first item in "Remaining State to Restore". Make sure to:

- See `practice_states_*.c` files for the current state of the code
- Remove the item from the "Remaining State to Restore" section once implemented
- No need to update the changelog or SAVE_STATE_VERSION

## Remaining State to Restore

- When watch is open then loading to out of watch nothing is rendered (and opposite maybe?)
- Sky
- Music
- Audio
- Lighting that slowly changes regardless of time scale (not sure if part of state or time scale bug?)

## Key Learnings

Add any general advice helpful for future agents working on this feature here. Be sure to read and understand these before starting work on handling more state in the system.

- **Implement one feature at a time then manually test**: Since crashes and hangs are so common and can be unexpectedly caused by even seemingly innocuous changes, implement a small number of safe properties in the save/load code. After this, update this file and report back to the user what was added, what was learned and what they should now do in the game to test that the newly added properties are loaded correctly.
- **Stale Pointers**: Any struct member ending in `*` (e.g., `ALSoundState *`, `ObjectRecord *`, `PropRecord *`) is an absolute memory address. If the game engine deallocates or reallocates the target object, loading a saved state that retains the old pointer will cause a crash. All such pointers must either be relocated (mapped back to correct indices) or it will not affect gameplay nullified (set to `NULL`).
- **Sound System Crashing**: Sound structures (`ALSoundState`) are allocated dynamically. Nullifying properties like `audioHandle`, `openSoundState`, and `closeSoundState` upon loading prevents the sound engine from trying to modify a defunct sound node.
- **Alarm State**: `alarm_timer` is both the active flag and the elapsed alarm
  duration, so it must be restored exactly rather than reduced to a Boolean.
  `ptr_alarm_sfx` is a dynamically allocated sound handle; clear it on load so
  the next alarm update starts a fresh sound for an active restored alarm.
  Alarm consequences can also be latched in the global
  `objectiveregisters1` stage-script bitfield. For example, Bunker 1 sets bit
  `0x100` when the alarm activates, then a separate controller uses it to
  dispatch reinforcements later. Restore the complete bitfield so delayed
  stage-script actions agree with the restored alarm and mission state.
- **Objective State**: Objective completion is split across several owners.
  `objectiveregisters1` contains mission-wide AI/script flags. Enter-room,
  deposit-item, and photograph progress is stored in mutable criteria records
  linked from the loaded stage setup; serialize their values in stable list
  order, not their pointers. `objectiveStatuses` caches the last displayed
  status and must agree with restored progress to avoid false HUD transition
  messages. Civilian casualties live in `g_playerPlayerData`, outside the main
  player struct, and can drive mission-failure AI.
- **Watch Clock Hands**: The analog watch hands are derived from the global
  `watch_time_0`, which advances every gameplay tick and is initialized from an
  optional stage `INTROTYPE_WATCH` record. Its representation is version
  dependent: `s32` in US and `f32` in JP/EU, so serialize it with matching
  integer/float stream operations.
- **Transient Gun Effects**: The global impact-flare/spark/dust pools
  (`dword_CODE_bss_8007A170`, plus
  `dword_CODE_bss_8007A4E0` outside EU) are independent of props. Serialize
  their live entries at exact pool indices and clear all live markers before
  loading. Map effect-image pointers to stable IDs instead of persisting
  absolute addresses. Player/NPC tracer beams are held in the restored
  hand/CHR records, but their rendered age must not advance while
  `g_ClockTimer == 0`.
- **Muzzle Flash (GUNFIRE Node)**: A firing weapon's muzzle flash is the
  GUNFIRE-node `visible` flag (`ModelRwData_GunfireRecord`) on the weapon's
  `Model`, set/read by `weaponSetGunfireVisible`/`weaponIsGunfireVisible`. It is
  _latched_, not per-frame: `chrlvFireWeaponRelated` → `chrSetFiring` raises it
  on a fire trigger and it is only cleared by stop-firing, so it survives across
  ticks and is **not** re-derived on load. NPC/CHR-held weapons are disabled
  child props that the prop loop never serializes, so the flag is saved per held
  weapon inside the CHR record (next to its model/weaponnum/flags) and reapplied
  in `restore_chr_attachments` _after_ each weapon is reattached — weapon
  (re)creation resets the node to hidden, so restoring it earlier would be lost.
  Standalone (enabled) weapon props on the ground never fire, so their flag is
  always clear and is not serialized. The player's own first-person muzzle flash
  is a separate view-model effect driven each frame from the saved hand firing
  state, not this prop flag.
- **Object Projectile/Embedment Union**: `ObjectRecord::projectile` and `ObjectRecord::embedment` occupy the same union slot. On load, restore only the member selected by `RUNTIMEBITFLAG_DEPOSIT` or `RUNTIMEBITFLAG_EMBEDDED`. Restoring one and then clearing the other overwrites the shared pointer; a deposited object will retain its flag and crash on the next tick when the engine dereferences the null projectile.
- **Resolve Projectile Prop Indices After Loading All Props**: Do not temporarily store saved prop indices in `Projectile::ownerprop` or `Projectile::obj`. If a referenced prop was collected or otherwise removed after the save, its record is skipped and the integer remains disguised as a pointer; a later tick or second save will dereference it and crash. Keep indices in separate arrays, resolve them after all prop records are processed, and free projectiles whose object prop no longer exists.
- **Adding/Removing Props on Load**: The loader rebuilds the prop array to match the save exactly. Props are processed in ascending slot order; before each saved record, every enabled prop in the skipped slots is removed (`removePropAtIndex`), and after the last record all trailing enabled slots are removed. Each saved prop is restored into its _exact_ original slot so all index-based references (parent/child/prev/next, `weapons_held`, projectile `obj`) stay valid. When the current world has no compatible prop in a slot, it is recreated there:
    - CHR: torn down and rebuilt via `create_chr_prop` (body/head allocation metadata).
    - Object/weapon pickups (`PROP_TYPE_WEAPON`, and OBJ subtypes KEY/MAGAZINE/COLLECTABLE/HAT/AMMO/ARMOUR): rebuilt via `create_object_prop`. Level-defined objects reuse their persistent setup `ObjectRecord` (found through a serialized setup-command index from `setupGetCommandIndexByProp`); dynamically dropped/thrown collectables take a fresh record from the weapon pool (`weaponCreate`). The common fields then install the authoritative object data and rooms are registered.
    - Explosions/smoke: rebuilt via `create_explosion_prop`/`create_smoke_prop` using a free `g_ExplosionBuffer`/`g_SmokeBuffer` entry.
    - Static level geometry (doors, glass, autoguns, monitors, CCTV, vehicles, aircraft, tanks, etc.) is never freed by gameplay, so its slot is assumed to persist; a missing one is skipped safely rather than recreated.
      Each object/door/weapon record carries an `ObjAllocationState` (model id, subtype, setup-command index) ahead of its payload so the destination prop can be built before the payload is consumed, mirroring `ChrAllocationState` for CHRs. The active-list links (`first`/`current`) and the per-prop `prev/next` are rebuilt from the restored indices; the attachment graph (`parent`/`child`) is owned by `restore_chr_attachments`. **This is new, hard-to-test code** — disabled (mid-regen/attached) props are not serialized, so the dominant test is save → play (collect items, kill guards, throw grenades, open doors) → load and confirm the saved world is faithfully restored without crashes.
- **Never Restore the Prop Free List From Saved Indices — Rebuild It**: Only enabled props are serialized, so free-list entries (and disabled/attached props) have no saved record and their `prev` links — which chain the free list together — are never written. Restoring `ptr_obj_pos_list_final_entry` from the saved head index therefore points the free list into a chain of stale, pre-load `prev` pointers. `chrpropAllocate` then walks that chain and hands out slots that are actually in use, or a stale/misaligned pointer (e.g. `&pos_data_entry[n].obj`, a slot base + 4); the misaligned "prop" gets bound into a runtime pool entry (`g_HatSlots`/`g_WeaponSlots`) by `objInit`, and the next `hatCreate`/`weaponCreate` crashes in `objFree`/`objFreeEmbedmentOrProjectile` dereferencing it. Symptom: equipment restores inconsistently across reloads ("sometimes a hat, sometimes not"), then a hard crash after several reloads. Fix: after restoring all props and attachments, rebuild the free list from scratch — a slot is free iff `!(flags & PROPFLAG_ENABLED) && parent == NULL` (active props are enabled; held equipment has a non-NULL parent) — chaining each free slot exactly as `chrpropFree` does and ignoring the saved free-list head. The `prev`-chain integer round-trips through `get_prop_index`/`get_prop_by_index` cannot survive a save anyway: `prop - pos_data_entry` integer-divides a misaligned pointer back to an aligned slot, so any misalignment seen in memory is a _live_ corruption, not saved data.
- **NULL Prop Handling (Cutscenes vs Gameplay)**: The player's world physical presence `g_CurrentPlayer->prop` is `NULL` during intro cutscenes, level loading, or death. When loading a saved state:
    - Track whether the saved state had a valid prop using a `has_prop` boolean flag.
    - If the saved state has a prop but the current player does not (e.g., loading gameplay into a cutscene), allocate a new prop using `chrpropAllocate()`, initialize its fields, activate it, enable it, and register it to its rooms.
    - If the saved state does not have a prop but the current player does (e.g., loading a cutscene into active gameplay), deregister, delist, disable, and free the current player prop using `chrpropDeregisterRooms`, `chrpropDelist`, `chrpropDisable`, and `chrpropFree`.
    - If both have a prop, update its coordinates and rooms safely using room deregistration/registration.

## Struct Analysis

Keep all the properties of the structs and global variables we save and load in files in this directory. Document them and their properties, along with whether they affect gameplay, their status in the save/load code and any other important information. Use this as a guide when reading source code to understand what each property does.

If you are missing any information about a struct or variable, mark with a TODO comment what should still be investigated and documented.
