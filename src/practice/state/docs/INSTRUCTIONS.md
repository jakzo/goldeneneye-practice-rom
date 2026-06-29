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
- [current] Props (PropRecord type-by-type, including add/remove on load)
- [done] NPCs (position, animation, AI, etc.)
- Global state (mission timer, objectives, etc.)

## Prompt

Read through [INSTRUCTIONS.md](src/practice/state/docs/INSTRUCTIONS.md) and implement the final task for props -- support for adding and removing props on load. Make sure to:

- See `practice_states_*.c` files for the current state of the code
- Change `ADD_AND_REMOVE_PROPS` from FALSE to implicitly TRUE by removing this gate and all code paths where it is false
- Add any code needed to create and remove props as necessary
- Double check all the code that there are no hidden assumptions of an existing prop (unless guaranteed the prop will never be removed) or an existing prop index or pointer location
- Fix any issues or TODOs related to prop add/removal
- Remove the the remaining groups section once implemented
- No need to update the changelog or SAVE_STATE_VERSION

## Remaining State to Restore

- Blood patches on enemies
- Sky
- Music
- Audio
- Lighting that slowly changes regardless of time scale (not sure if part of state or time scale bug?)
- ~~Scorch marks, bullet holes & flying explosion particles~~ (done; see [DECALS.md](DECALS.md))

## Key Learnings

Add any general advice helpful for future agents working on this feature here. Be sure to read and understand these before starting work on handling more state in the system.

- **Implement one feature at a time then manually test**: Since crashes and hangs are so common and can be unexpectedly caused by even seemingly innocuous changes, implement a small number of safe properties in the save/load code. After this, update this file and report back to the user what was added, what was learned and what they should now do in the game to test that the newly added properties are loaded correctly.
- **Stale Pointers**: Any struct member ending in `*` (e.g., `ALSoundState *`, `ObjectRecord *`, `PropRecord *`) is an absolute memory address. If the game engine deallocates or reallocates the target object, loading a saved state that retains the old pointer will cause a crash. All such pointers must either be relocated (mapped back to correct indices) or it will not affect gameplay nullified (set to `NULL`).
- **Sound System Crashing**: Sound structures (`ALSoundState`) are allocated dynamically. Nullifying properties like `audioHandle`, `openSoundState`, and `closeSoundState` upon loading prevents the sound engine from trying to modify a defunct sound node.
- **Object Projectile/Embedment Union**: `ObjectRecord::projectile` and `ObjectRecord::embedment` occupy the same union slot. On load, restore only the member selected by `RUNTIMEBITFLAG_DEPOSIT` or `RUNTIMEBITFLAG_EMBEDDED`. Restoring one and then clearing the other overwrites the shared pointer; a deposited object will retain its flag and crash on the next tick when the engine dereferences the null projectile.
- **Resolve Projectile Prop Indices After Loading All Props**: Do not temporarily store saved prop indices in `Projectile::ownerprop` or `Projectile::obj`. If a referenced prop was collected or otherwise removed after the save, its record is skipped and the integer remains disguised as a pointer; a later tick or second save will dereference it and crash. Keep indices in separate arrays, resolve them after all prop records are processed, and free projectiles whose object prop no longer exists.
- **Adding/Removing Props on Load**: The loader rebuilds the prop array to match the save exactly. Props are processed in ascending slot order; before each saved record, every enabled prop in the skipped slots is removed (`removePropAtIndex`), and after the last record all trailing enabled slots are removed. Each saved prop is restored into its _exact_ original slot so all index-based references (parent/child/prev/next, `weapons_held`, projectile `obj`) stay valid. When the current world has no compatible prop in a slot, it is recreated there:
    - CHR: torn down and rebuilt via `create_chr_prop` (body/head allocation metadata).
    - Object/weapon pickups (`PROP_TYPE_WEAPON`, and OBJ subtypes KEY/MAGAZINE/COLLECTABLE/HAT/AMMO/ARMOUR): rebuilt via `create_object_prop`. Level-defined objects reuse their persistent setup `ObjectRecord` (found through a serialized setup-command index from `setupGetCommandIndexByProp`); dynamically dropped/thrown collectables take a fresh record from the weapon pool (`weaponCreate`). The common fields then install the authoritative object data and rooms are registered.
    - Explosions/smoke: rebuilt via `create_explosion_prop`/`create_smoke_prop` using a free `g_ExplosionBuffer`/`g_SmokeBuffer` entry.
    - Static level geometry (doors, glass, autoguns, monitors, CCTV, vehicles, aircraft, tanks, etc.) is never freed by gameplay, so its slot is assumed to persist; a missing one is skipped safely rather than recreated.
      Each object/door/weapon record carries an `ObjAllocationState` (model id, subtype, setup-command index) ahead of its payload so the destination prop can be built before the payload is consumed, mirroring `ChrAllocationState` for CHRs. The active-list links (`first`/`current`) and the per-prop `prev/next` are rebuilt from the restored indices; the attachment graph (`parent`/`child`) is owned by `restore_chr_attachments`. **This is new, hard-to-test code** — disabled (mid-regen/attached) props are not serialized, so the dominant test is save → play (collect items, kill guards, throw grenades, open doors) → load and confirm the saved world is faithfully restored without crashes.
- **Never Restore the Prop Free List From Saved Indices — Rebuild It**: Only enabled props are serialized, so free-list entries (and disabled/attached props) have no saved record and their `prev` links — which chain the free list together — are never written. Restoring `ptr_obj_pos_list_final_entry` from the saved head index therefore points the free list into a chain of stale, pre-load `prev` pointers. `chrpropAllocate` then walks that chain and hands out slots that are actually in use, or a stale/misaligned pointer (e.g. `&pos_data_entry[n].obj`, a slot base + 4); the misaligned "prop" gets bound into a runtime pool entry (`g_HatSlots`/`g_WeaponSlots`) by `objInit`, and the next `hatCreate`/`weaponCreate` crashes in `objFree`/`objFreeEmbedmentOrProjectile` dereferencing it. Symptom: equipment restores inconsistently across reloads ("sometimes a hat, sometimes not"), then a hard crash after several reloads. Fix: after restoring all props and attachments, rebuild the free list from scratch — a slot is free iff `!(flags & PROPFLAG_ENABLED) && parent == NULL` (active props are enabled; held equipment has a non-NULL parent) — chaining each free slot exactly as `chrpropFree` does and ignoring the saved free-list head. The `prev`-chain integer round-trips through `get_prop_index`/`get_prop_by_index` cannot survive a save anyway: `prop - pos_data_entry` integer-divides a misaligned pointer back to an aligned slot, so any misalignment seen in memory is a *live* corruption, not saved data.
- **NULL Prop Handling (Cutscenes vs Gameplay)**: The player's world physical presence `g_CurrentPlayer->prop` is `NULL` during intro cutscenes, level loading, or death. When loading a saved state:
    - Track whether the saved state had a valid prop using a `has_prop` boolean flag.
    - If the saved state has a prop but the current player does not (e.g., loading gameplay into a cutscene), allocate a new prop using `chrpropAllocate()`, initialize its fields, activate it, enable it, and register it to its rooms.
    - If the saved state does not have a prop but the current player does (e.g., loading a cutscene into active gameplay), deregister, delist, disable, and free the current player prop using `chrpropDeregisterRooms`, `chrpropDelist`, `chrpropDisable`, and `chrpropFree`.
    - If both have a prop, update its coordinates and rooms safely using room deregistration/registration.

## Struct Analysis

Keep all the properties of the structs and global variables we save and load in files in this directory. Document them and their properties, along with whether they affect gameplay, their status in the save/load code and any other important information. Use this as a guide when reading source code to understand what each property does.

If you are missing any information about a struct or variable, mark with a TODO comment what should still be investigated and documented.
