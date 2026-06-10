# Save/Load States

This document logs our findings, analysis, and architecture specifications for the practice ROM's save/load state functionality, serving as a guide for future developers and AI agents.

You should update this document whenever new information is learned. Keep it concise and information-dense.

If you have been told to continue work on this feature, read the "current goal" section below to understand what we want to achieve. Also look at the key learnings and struct analysis for helpful information and leads to continue investigating.

Above all, make sure you investigate thoroughly to fully understand where pieces of data are used, their purpose and the implications of setting them to certain values on load. Once confident about a piece of data, add it to the save/load system, document your findings in this file, and stop and tell the user about what you found and how they can test it. Do not continue work unless the user explicitly tells you to.

## Current Goal

Currently we want to be able to save and load details about the player in a single player game. Things like position, orientation, velocity, ammo, inventory, health, etc.

## Key Learnings

Add any general advice helpful for future agents working on this feature here. Be sure to read and understand these before starting work on handling more state in the system.

- **Implement one feature at a time then manually test**: Since crashes and hangs are so common and can be unexpectedly caused by even seemingly innocuous changes, implement a small number of safe properties in the save/load code. After this, update this file and report back to the user what was added, what was learned and what they should now do in the game to test that the newly added properties are loaded correctly.
- **Stale Pointers**: Any struct member ending in `*` (e.g., `ALSoundState *`, `ObjectRecord *`, `PropRecord *`) is an absolute memory address. If the game engine deallocates or reallocates the target object, loading a saved state that retains the old pointer will cause a crash. All such pointers must either be relocated (mapped back to correct indices) or it will not affect gameplay nullified (set to `NULL`).
- **Sound System Crashing**: Sound structures (`ALSoundState`) are allocated dynamically. Nullifying properties like `audioHandle`, `openSoundState`, and `closeSoundState` upon loading prevents the sound engine from trying to modify a defunct sound node.
- **NULL Prop Handling (Cutscenes vs Gameplay)**: The player's world physical presence `g_CurrentPlayer->prop` is `NULL` during intro cutscenes, level loading, or death. When loading a saved state:
  - Track whether the saved state had a valid prop using a `has_prop` boolean flag.
  - If the saved state has a prop but the current player does not (e.g., loading gameplay into a cutscene), allocate a new prop using `chrpropAllocate()`, initialize its fields, activate it, enable it, and register it to its rooms.
  - If the saved state does not have a prop but the current player does (e.g., loading a cutscene into active gameplay), deregister, delist, disable, and free the current player prop using `chrpropDeregisterRooms`, `chrpropDelist`, `chrpropDisable`, and `chrpropFree`.
  - If both have a prop, update its coordinates and rooms safely using room deregistration/registration.


## Struct Analysis

Keep all the properties of the structs and global variables we save and load here. Document them and their properties, along with whether they affect gameplay, their status in the save/load code and any other important information. Use this as a guide when reading source code to understand what each property does.

If you are missing any information about a struct or variable, mark with a TODO comment what should still be investigated and documented.

```c
// player.c / player.h
// Global reference to the active player state (for the current player viewport).
// Also referenced in the g_playerPointers[4] array indexed by player_num.
struct player *g_CurrentPlayer;

// bondview.h
// State of the player (Bond).
struct player {
  // Saved: yes
  // Flag indicating player control state / viewport focus.
  // Values: 1 (bypasses collision and portal calculations, returning raw position coordinates directly, e.g. during cutscenes or frozen camera tick), 0 (normal gameplay mode where logical position is calculated from collision structures).
  s32 unknown;

  // Saved: yes
  // 3D coordinate of the player's camera position (eye level/head) in the game world.
  coord3d pos;

  // Saved: yes
  // Camera look-at target position (point in 3D space the camera is directed towards), used primarily for cutscenes, transition cameras, or frozen/free camera ticks.
  coord3d pos2;

  // Saved: yes
  // 3D coordinate of the player's feet / floor level, adjusted based on level stand tile heights.
  coord3d pos3;

  // Saved: yes
  // Visual camera offset/translation vector.
  coord3d offset;

  // Saved: yes
  // Pointer to the level geometry StandTile the player is currently on.
  // Address Safety: Safe to load *within the same level run*, as level geometry StandTiles are loaded statically on stage load and do not move or deallocate during gameplay. However, loading a state from a different level or after reloading the level is unsafe because the tiles are reallocated in memory.
  StandTile *room_pointer;

  // Saved: yes (restored via deregistration/registration functions)
  // Pointer to the player's world PropRecord which tracks its position, active stand tile, and registered rooms in the game's spatial grid.
  // Address Safety: The pointer itself is safe to load within the same level run. However, moving the player prop requires deregistering it from its old rooms (using `chrpropDeregisterRooms`) and registering it to its new rooms (using `chrpropRegisterRooms`) to prevent coordinate-room desync and crashes.
  PropRecord *prop;

  // Recomputed: yes (synced via setsuboffset and setsubroty)
  // Pointer to the player's 3D visual model instance in stage memory.
  struct Model *ptr_char_objectinstance;

  // Saved: yes
  // Origin coordinates of the current level room the player is in, used to transform rendering matrices to prevent N64 floating point precision issues.
  // Scaling:
  // - current_model_pos: scaled origin coordinates of the current room (multiplied by `room_data_float2` which is the global coordinate scaling factor of the level geometry).
  // - previous_model_pos: scaled origin coordinates of the previous frame's room.
  // - current_room_pos: unscaled origin coordinates of the current room (obtained by multiplying `current_model_pos` by `room_data_float1` which is `1.0f / room_data_float2`).
  coord3d current_model_pos;
  coord3d previous_model_pos;
  coord3d current_room_pos;

  // Saved: yes
  // Index into the active rendering matrices allocation buffer array (roomMatrices / roomOwners of size 300) to keep track of the room origin mapping for viewport rendering.
  s32 curRoomIndex;

  // Saved: yes
  // Portal boundary and collision data structure representing player's logical bounds, collision positions, and portal travel logic.
  struct collision434 field_488;

  // Saved: yes
  // Look angles (horizontal and vertical orientation).
  f32 vv_theta;
  f32 vv_verta;

  // Recomputed: yes (calculated on load by bondviewApplyVertaTheta)
  // Derived trigonometric values of the look angles:
  // - vv_costheta, vv_sintheta: cached sine and cosine of vv_theta.
  // - vv_verta360: vertical look angle normalized to [0, 360].
  // - vv_cosverta, vv_sinverta: cached sine and cosine of vv_verta360.
  f32 vv_costheta;
  f32 vv_sintheta;
  f32 vv_verta360;
  f32 vv_cosverta;
  f32 vv_sinverta;

  // Saved: yes
  // Movement speeds and physics multipliers:
  // - speedsideways: velocity moving left/right relative to view.
  // - speedstrafe: Current strafe velocity (dynamically updated each frame based on horizontal joystick deflection/input).
  // - speedforwards: velocity moving forward/backward relative to view.
  // - speedmaxtime60: duration (ticks) player has been continuously running.
  // - speedboost: running speed factor (starts at 1.0f, accelerates to 1.35f after 3 seconds of continuous running).
  f32 speedsideways;
  f32 speedstrafe;
  f32 speedforwards;
  s32 speedmaxtime60;
  f32 speedboost;

  // Saved: yes
  // Crouch/duck positioning and height offsets:
  // - vertical_bounce_adjust: visual bounce vertical adjustment applied when falling or descending ramps/stairs.
  // - ducking_height_offset: vertical visual/logical eye offset (0.0f to -100.0f) representing current crouch level.
  // - crouchpos: crouch state enum/flag (0 = CROUCH_STAND, 1 = CROUCH_KNEEL, 2 = CROUCH_SQUAT).
  f32 vertical_bounce_adjust;
  f32 ducking_height_offset;
  s32 crouchpos;

  // Saved: yes
  // Health and armor values:
  // - bondhealth, bondarmour: actual logical health and armor (0.0f to 1.0f).
  // - oldhealth, oldarmour: health and armor values in the previous frame.
  // - apparenthealth, apparentarmour: the visual values displayed on the HUD bars, which smoothly slide and interpolate towards the actual values when taking damage or gaining armor to provide visual polish.
  f32 bondhealth;
  f32 bondarmour;
  f32 oldhealth;
  f32 oldarmour;
  f32 apparenthealth;
  f32 apparentarmour;

  // Saved: yes
  // Previous frame's camera coordinates, used to prevent physics/collision jumps on the frame immediately following a load.
  coord3d bondprevpos;

  // Saved: yes
  // 3D velocity/momentum vector:
  // - field_78: X velocity
  // - field_7C: Y (vertical) velocity, used to track falls, jumps, and landing impacts.
  // - field_80: Z velocity
  // - speedgo: forward/backward movement velocity ramp-up and deceleration timer. Overwriting this stops player post-load drift.
  // - bondshotspeed: recoil and impact velocity vector from explosions or bullets.
  // - speedtheta, speedverta: current frame turning speeds.
  f32 field_78;
  f32 field_7C;
  f32 field_80;
  f32 speedgo;
  coord3d bondshotspeed;
  f32 speedtheta;
  f32 speedverta;

  // Saved: yes
  // Dampened landing/impact vertical bounce offsets.
  f32 field_84;
  f32 field_88;

  // Saved: yes
  // Height tracking variables:
  // - field_6C: raw height above the ground before dampening/gravity checks.
  // - field_70: smoothed floor/ground level tracking height.
  // - stanHeight: height of the StandTile currently below the player.
  f32 field_6C;
  f32 field_70;
  f32 stanHeight;

  // Saved: yes
  // Coordinate smoothing exponential accumulator:
  // - field_3B8: smoothed coordinate accumulator vector.
  // - field_3C4, field_3C8, field_3CC: derived X, Y, Z camera coordinates calculated from field_3B8. Overwriting these avoids 1-frame coordinate rendering jumps or visual rubberbanding.
  coord3d field_3B8;
  f32 field_3C4;
  f32 field_3C8;
  f32 field_3CC;

  // Saved: yes
  // Previous frame's collision context structure.
  struct collision434 previous_collision_info;

  // Saved: yes
  // Walk & Head Bob Animation State:
  // - resetheadpos, resetheadrot, resetheadtick: head position and rotation reset flags.
  // - headanim: active head bob/walk animation index.
  // - headdamp: look/head dampening multiplier.
  // - headwalkingtime60: current time/frame index of the walking animation cycle. Overwriting this resets the walk cycle.
  // - headamplitude, sideamplitude: vertical and side amplitudes of the head bob cycle.
  // - headpos, headlook, headup: current frame walk bob offsets/look vectors.
  // - headpossum, headlooksum, headupsum: accumulated walk bob offsets/look vectors that drive camera position changes.
  // - headbodyoffset, standbodyoffset: visual body-relative offsets.
  // - standheight, standfrac: stationary ground offsets and interpolation fraction.
  // - standlook, standup: array of 2 look/up vectors representing standing state history.
  // - standcnt: stand state accumulator.
  s32 resetheadpos;
  s32 resetheadrot;
  s32 resetheadtick;
  s32 headanim;
  f32 headdamp;
  s32 headwalkingtime60;
  f32 headamplitude;
  f32 sideamplitude;
  coord3d headpos;
  coord3d headlook;
  coord3d headup;
  coord3d headpossum;
  coord3d headlooksum;
  coord3d headupsum;
  coord3d headbodyoffset;
  f32 standheight;
  coord3d standbodyoffset;
  f32 standfrac;
  coord3d standlook[2];
  coord3d standup[2];
  s32 standcnt;
}

// Collision data representing player bounds, collision coordinates, and portal tracking.
struct collision434 {
  // Pointer to the current level StandTile the player is standing on.
  struct StandTile *current_tile_ptr;

  // Exact coordinates resolved after collision calculations.
  coord3d collision_position;

  // View rotation vector component that affects Bond's movement (but not the viewport).
  struct coord3d theta_transform;

  // Alternate/previous floor level coordinate.
  coord3d pos3;

  // Radius of the player's bounding cylinder for collision checks.
  f32 collision_radius;

  // Alternate/previous camera eye level coordinate.
  coord3d pos;

  // Visual camera rotation applied vector.
  struct coord3d applied_view;

  // Secondary visual camera rotation vector.
  struct coord3d applied_view2;

  // Pointer to the StandTile crossed during portal/room transitions.
  StandTile *current_tile_ptr_for_portals;
}

// Level pathfinding/collision grid polygon tile.
struct StandTile {
  // Tile ID bitfield containing GroupID and other identifiers.
  u32 id : 24;

  // The room index this tile belongs to.
  u8 room;

  // Midpoint tile header/half-word links.
  union {
    StandTileHeaderMid headerMid;
    s16 half;
  } mid;

  // Tail-end links and connection bitfields.
  union {
    StandTileHeaderTail hdrTail;
    s16 half;
  } tail;

  // Array of points defining the vertices of the tile polygon.
  StandTilePoint points[];
}

// Global prop definition mapping objects, doors, and characters in the world.
struct PropRecord {
  // Prop type enum.
  u8 type;

  // Flags representing visibility and interaction states.
  u8 flags;

  // Ticks remaining before regenerating/respawning.
  s16 timetoregen;

  // Union pointing to the underlying entity type.
  union {
    struct ChrRecord *chr;
    struct ObjectRecord *obj;
    struct DoorRecord *door;
    struct WeaponObjRecord *weapon;
    struct Explosion *explosion;
    struct Smoke *smoke;
    struct Scorch *scorch;
    void *voidp;
  };

  // 3D world coordinate position.
  coord3d pos;

  // Pointer to the StandTile the prop is currently resting on.
  StandTile *stan;

  // Render depth for camera sorting.
  f32 zDepth;

  // Relationships inside the prop hierarchy:
  struct PropRecord *parent;
  struct PropRecord *child;
  struct PropRecord *prev;
  struct PropRecord *next;

  // The room indices this prop occupies (terminated by 0xFF).
  u8 rooms[PROPRECORD_STAN_ROOM_LEN];

  s32 unk30;
}

// 3D model instance representation in stage memory.
struct Model {
  s16 unk00;
  s16 Type;

  // Pointer to the character entity record.
  struct ChrRecord *chr;

  // Pointer to the model file header containing vertices and hierarchy.
  ModelFileHeader *obj;

  // View render matrix mappings.
  RenderPosView *render_pos;
  union ModelRwData **datas;

  // Model visual scale.
  f32 scale;

  // Attachment hierarchy relationships:
  struct Model *attachedto;
  ModelNode *attachedto_objinst;

  // Pointer to current animation controller.
  ModelAnimation *anim;

  s8 gunhand;
  s8 unk25;
  s8 animlooping;
  s8 unk27;

  f32 unk28;
  f32 unk2c;

  s16 framea;
}

// TODO: Define other relevant structs (e.g. inventory/ammo arrays) here as we implement them. If you reference a struct in a property above, you must define it here and document its properties.
```
