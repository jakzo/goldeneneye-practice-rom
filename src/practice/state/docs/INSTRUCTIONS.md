# Save/Load States

This document logs our findings, analysis, and architecture specifications for the practice ROM's save/load state functionality, serving as a guide for future developers and AI agents.

You should update this document whenever new information is learned. Keep it concise and information-dense.

If you have been told to continue work on this feature, read the "current goal" section below to understand what we want to achieve. Also look at the key learnings and struct analysis for helpful information and leads to continue investigating.

Above all, make sure you investigate thoroughly to fully understand where pieces of data are used, their purpose and the implications of setting them to certain values on load. Once confident about a piece of data, add it to the save/load system, document your findings in this file, and stop and tell the user about what you found and how they can test it. Do not continue work unless the user explicitly tells you to.

## Current Goal

Currently we want to complete `PROP_TYPE_CHR`, the final prop type. The first
thirteen conservative field sets, including character/model configuration,
spatial, movement, equipment, and all pointer-free/model-only actions, are
implemented; supporting structures and the remaining restore surface are
documented in `CHR.md`. Do not broaden the implementation without investigating
and documenting the additional state coupling.

### Next Goals

After the current goal we will need to handle:

- [done] Bond
- [current] Props (PropRecord type-by-type)
- NPCs (position, animation, AI, etc.)
- Global state (mission timer, objectives, etc.)

## Prompt

Read through [INSTRUCTIONS.md](src/practice/state/docs/INSTRUCTIONS.md) and implement a set of props in the "Remaining Groups". These are intended to be a group of fields that are easy to restore since they don't rely on other fields. They are flexible so if one field is too hard or the grouping doesn't make sense you can edit the groupings. Make sure to:

- See practice_states_chr.c for the current state of the code
- Document their purpose, possible values, etc. in the docs files the same way as the existing fields using the same format, and move them out of the TODO section
- Remove them from the remaining groups section once implemented
- Tell me what to do in the game to test that it works and doesn't crash/hang
- No need to update the changelog or SAVE_STATE_VERSION
- Right now the code only restores state to existing active props, but eventually once the code supports all types it will replace all props with the restored ones, so if you assume things based on this (like a pointer to a prop being correct already since we only restore to active props) then make sure to include code which does it properly without the assumption behind the ADD_AND_REMOVE_PROPS flag

## Remaining Groups

Remaining action and model-animation batches
The common live model animation identity, frame, speed, looping,
interpolation, and transform state is implemented for the completed simple
and navigation batches. Implement the remaining action-specific payloads in
batches:
Combat: attack, attack-walk, and attack-roll.
Payload-bearing player action: bond-multi.
Death and hit-reaction actions remain with damage/lifecycle below.

Combat aiming state
firecount, aimendcount, eight shoulder/back aim values, fireslot, and unk180 beam caches.
These depend on equipped weapons, attack action, and model pose, so they should follow equipment and combat-action support.

Damage and lifecycle
damage, maxdamage, fadealpha, flinchcnt, chrflags, remaining hidden bits, and die/dead/argh actions.
Whole chrflags and hidden restoration belongs here. Their bits can trigger initialization, firing, movement, freezing, item drops, or character removal.

Audio and transient runtime allocations
ptr_SEbuffer3, ptr_SEbuffer4, field_178, and field_20.
The sound pointers and field_20's dynamically allocated skeletal joint/matrix
list should be cleared or reconstructed, not serialized as addresses.

Allocation and recreation
prop, model, missing CHR creation, and removal of characters absent from the save.
The gated CHR path now tears down and recreates each saved CHR in its exact
PropRecord slot using serialized body/head allocation metadata, and removes
enabled CHRs absent from the save. `ADD_AND_REMOVE_PROPS` must remain false
until equivalent retained-slot allocation/teardown exists for every prop type
and every CHR payload required to initialize a new character is implemented.
The complete generic attachment graph and free/active lists must be rebuilt as
part of enabling it.

## Key Learnings

Add any general advice helpful for future agents working on this feature here. Be sure to read and understand these before starting work on handling more state in the system.

- **Implement one feature at a time then manually test**: Since crashes and hangs are so common and can be unexpectedly caused by even seemingly innocuous changes, implement a small number of safe properties in the save/load code. After this, update this file and report back to the user what was added, what was learned and what they should now do in the game to test that the newly added properties are loaded correctly.
- **Stale Pointers**: Any struct member ending in `*` (e.g., `ALSoundState *`, `ObjectRecord *`, `PropRecord *`) is an absolute memory address. If the game engine deallocates or reallocates the target object, loading a saved state that retains the old pointer will cause a crash. All such pointers must either be relocated (mapped back to correct indices) or it will not affect gameplay nullified (set to `NULL`).
- **Sound System Crashing**: Sound structures (`ALSoundState`) are allocated dynamically. Nullifying properties like `audioHandle`, `openSoundState`, and `closeSoundState` upon loading prevents the sound engine from trying to modify a defunct sound node.
- **Object Projectile/Embedment Union**: `ObjectRecord::projectile` and `ObjectRecord::embedment` occupy the same union slot. On load, restore only the member selected by `RUNTIMEBITFLAG_DEPOSIT` or `RUNTIMEBITFLAG_EMBEDDED`. Restoring one and then clearing the other overwrites the shared pointer; a deposited object will retain its flag and crash on the next tick when the engine dereferences the null projectile.
- **Resolve Projectile Prop Indices After Loading All Props**: Do not temporarily store saved prop indices in `Projectile::ownerprop` or `Projectile::obj`. If a referenced prop was collected or otherwise removed after the save, its record is skipped and the integer remains disguised as a pointer; a later tick or second save will dereference it and crash. Keep indices in separate arrays, resolve them after all prop records are processed, and free projectiles whose object prop no longer exists.
- **NULL Prop Handling (Cutscenes vs Gameplay)**: The player's world physical presence `g_CurrentPlayer->prop` is `NULL` during intro cutscenes, level loading, or death. When loading a saved state:
    - Track whether the saved state had a valid prop using a `has_prop` boolean flag.
    - If the saved state has a prop but the current player does not (e.g., loading gameplay into a cutscene), allocate a new prop using `chrpropAllocate()`, initialize its fields, activate it, enable it, and register it to its rooms.
    - If the saved state does not have a prop but the current player does (e.g., loading a cutscene into active gameplay), deregister, delist, disable, and free the current player prop using `chrpropDeregisterRooms`, `chrpropDelist`, `chrpropDisable`, and `chrpropFree`.
    - If both have a prop, update its coordinates and rooms safely using room deregistration/registration.

## Struct Analysis

Keep all the properties of the structs and global variables we save and load in files in this directory. Document them and their properties, along with whether they affect gameplay, their status in the save/load code and any other important information. Use this as a guide when reading source code to understand what each property does.

If you are missing any information about a struct or variable, mark with a TODO comment what should still be investigated and documented.
