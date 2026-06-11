# Save/Load States

This document logs our findings, analysis, and architecture specifications for the practice ROM's save/load state functionality, serving as a guide for future developers and AI agents.

You should update this document whenever new information is learned. Keep it concise and information-dense.

If you have been told to continue work on this feature, read the "current goal" section below to understand what we want to achieve. Also look at the key learnings and struct analysis for helpful information and leads to continue investigating.

Above all, make sure you investigate thoroughly to fully understand where pieces of data are used, their purpose and the implications of setting them to certain values on load. Once confident about a piece of data, add it to the save/load system, document your findings in this file, and stop and tell the user about what you found and how they can test it. Do not continue work unless the user explicitly tells you to.

## Current Goal

Currently we want to be able to save and load details about the player in a single player game. Things like position, orientation, velocity, ammo, inventory, health, etc.

### Next Goals

After the current goal we will need to handle:

- Props (PropRecord type-by-type)
- NPCs (position, animation, AI, etc.)
- Global state (mission timer, objectives, etc.)

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
  // Values:
  // - 0: Normal gameplay mode (processes portal geometry, wall/floor collisions, respects physical speed).
  // - 1: Cutscene/Bypass mode (bypasses collision and portal calculations, camera snaps/moves directly to the coordinates in pos. Used during intro sweeps, death cameras, or scripted control sequences).
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
  // Look angles (horizontal and vertical orientation in degrees).
  // - vv_theta: Horizontal look rotation. Bounded between 0.0f and 360.0f (degrees). Used to calculate radial direction.
  // - vv_verta: Vertical look rotation (pitch). Bounded between -80.0f (looking straight down) and 80.0f (looking straight up).
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
  // - speedsideways: Current velocity moving sideways (left/right) relative to camera heading. Bounded typically by joystick limits.
  // - speedstrafe: Horizontal joystick deflection component. Range: -70 to +70 (or equivalent scaled joystick inputs).
  // - speedforwards: Current velocity moving forwards/backwards relative to camera heading. Bounded by joystick and current speed limits.
  // - speedmaxtime60: Continuous forward-running frame/tick counter. Increments by g_ClockTimer (60Hz US, 50Hz JP/EU) when running forward at full speed. Bounded between 0 and THREE_SECOND_TICKS (180 US, 150 JP/EU). Resets to 0 if forward movement ceases.
  // - speedboost: Running speed acceleration factor. Bounded between 1.0f (SPEED_REGULAR_MAX) and 1.25f (SPEED_RUN_MAX). When running forward at full speed continuously for three seconds (speedmaxtime60 >= THREE_SECOND_TICKS), it increments by 0.01f (SPEED_TICK_ADJUST) per frame up to 1.25f. Decays back to 1.0f when full forward speed is not maintained.
  f32 speedsideways;
  f32 speedstrafe;
  f32 speedforwards;
  s32 speedmaxtime60;
  f32 speedboost;

  // Saved: yes
  // Crouch/duck positioning and height offsets:
  // - vertical_bounce_adjust: Visual camera bounce vertical adjustment applied on landing impact or when descending stairs/ramps.
  // - ducking_height_offset: Vertical visual/logical camera height offset. Bounded between 0.0f (standing) and -100.0f (fully crouched / FULL_CROUCH_OFFSET). Interpolates smoothly towards the target height matching crouchpos.
  // - crouchpos: Target crouch state enum/flag.
  //   Values:
  //   - 0: CROUCH_SQUAT (fully crouched / target height offset -100.0f)
  //   - 1: CROUCH_HALF (kneeling / target height offset -60.0f)
  //   - 2: CROUCH_STAND (standing / target height offset 0.0f)
  f32 vertical_bounce_adjust;
  f32 ducking_height_offset;
  s32 crouchpos;

  // Saved: yes
  // Health and armor values:
  // - bondhealth, bondarmour: Actual logical player health and armor. Bounded between 0.0f (no health/armor) and 1.0f (full health/armor).
  // - oldhealth, oldarmour: The health and armor values from the previous frame. Bounded between 0.0f and 1.0f.
  // - apparenthealth, apparentarmour: Visual health/armor level displayed on the HUD bars. Bounded between 0.0f and 1.0f. Interpolates smoothly towards bondhealth and bondarmour over time (sliding bar effect).
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
  // X, Y, Z velocity/momentum vector (units: coordinate units per frame):
  // - field_78: Sideways velocity (X axis). Positive values move right, negative left.
  // - field_7C: Vertical velocity (Y axis). Bounded by terminal velocity. Negative when falling, positive when jumping or pushed up.
  // - field_80: Forward/backward velocity (Z axis). Positive moves forward, negative backward.
  // - speedgo: Forward/backward acceleration factor. Bounded between 0.0f (stopped) and 1.0f (full speed).
  // - bondshotspeed: Recoil velocity vector applied to the player upon taking gunshots or explosive blast impact.
  // - speedtheta, speedverta: Rotational look speeds (yaw/pitch rates) per frame.
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

  // Saved: yes
  // Inventory & Ammo Arrays:
  // - ammoheldarr: current ammunition counts for each ammo type.
  // - ptr_inventory_first_in_cycle: pointer to the first item in the inventory circular linked list. Saved address-safely as flat item definitions and reconstructed on load using inventory APIs.
  // - equipallguns: All Guns cheat flag.
  // - equipcuritem: current equipped weapon/item ID.
  s32 ammoheldarr[30];
  InvItem *ptr_inventory_first_in_cycle;
  s32 equipallguns;
  s32 equipcuritem;

  // Saved: yes
  // Weapons & Hands Rendering:
  // - weapon/item IDs currently loaded in the left and right hands.
  ITEM_IDS hand_item[2];
  // Saved: yes
  // structure for each hand tracking animation status, next weapons, and weapon_ammo_in_magazine. Rebuilt address-safely; triggers model reloading via currentPlayerEquipWeaponWrapper if hand weapons change on load.
  struct hand hands[2];

  // Saved: yes
  // Player status & cheat settings:
  // Player invincibility status flag. Bounded to 0 (vulnerable) or 1 (invincible, active cheat). When 1, all incoming gunshots and explosive damage to player health are bypassed.
  u8 bondinvincible;

  // Saved: yes
  // HUD Display overlay states:
  // - damageshowtime: Remaining display duration (in frames/ticks) for the fullscreen damage flash or HUD warnings. Bounded from -1 (hidden/inactive) up to total duration (e.g. 120 frames).
  // - healthshowtime: Remaining display duration (in frames/ticks) for the health/armor HUD overlay bars. Bounded from -1 (hidden/inactive) up to total display duration. Counts up or down and resets to -1 when animation finishes.
#if defined(VERSION_JP) || defined(VERSION_EU)
  f32 damageshowtime;
  f32 healthshowtime;
#else
  s32 damageshowtime;
  s32 healthshowtime;
#endif
  // Saved: yes
  // Unused padding/unreferenced field (0x00fc).
  s32 healthshowmode;

  // Saved: yes
  // Default screen fade transition timers/fractions (used when transitioning between rooms, starting missions, etc.):
  // Current frame timer for the default screen fade. Bounded from 0.0f to bondfadetimemax60. Set to -1.0f when no default screen fade is active.
  f32 bondfadetime60;
  // Saved: yes
  // Total duration (in frames) of the default screen fade transition.
  f32 bondfadetimemax60;
  // Saved: yes
  // Starting default screen opacity fraction. Bounded between 0.0f (transparent) and 1.0f (fully opaque).
  f32 bondfadefracold;
  // Saved: yes
  // Target/ending default screen opacity fraction. Bounded between 0.0f and 1.0f.
  f32 bondfadefracnew;
  // Saved: yes
  // Player breathing cycle intensity. Bounded between 0.0f and 1.0f. Drives the frequency/amplitude of camera height bobbing when standing idle.
  f32 bondbreathing;

  // Saved: yes
  // Weapon Sway cycle details:
  // vertical and horizontal weapon sway amplitudes.
  f32 gunposamplitude;
  f32 gunxamplitude;
  // Saved: yes
  // weapon sway timing and synchronization state variables.
  f32 gunsync;
  f32 syncchange;
  f32 synccount;
  s32 syncoffset;
  // Saved: yes
  // smoothed breathing and sway accumulators.
  f32 field_107C;
  f32 field_1080;

  // Saved: yes
  // RGB channels of screen color tint (0-255). Damage flashes are red (255, 0, 0) and fade transitions are black (0, 0, 0).
  s32 colourscreenred;
  s32 colourscreengreen;
  s32 colourscreenblue;
  // Saved: yes
  // Active screen color overlay transparency fraction. Bounded between 0.0f (no overlay) and 1.0f (opaque).
  f32 colourscreenfrac;
  // Saved: yes
  // Color tint fade timers (in frames). Bounded from 0.0f to colourfadetimemax60.
  f32 colourfadetime60;
  f32 colourfadetimemax60;
  // Saved: yes
  // Starting and target RGB color/fraction boundaries for fade interpolation.
  s32 colourfaderedold;
  s32 colourfaderednew;
  s32 colourfadegreenold;
  s32 colourfadegreennew;
  s32 colourfadeblueold;
  s32 colourfadebluenew;
  f32 colourfadefracold;
  f32 colourfadefracnew;

  // Saved: yes
  // crosshair_angle: Final calculated 2D viewport coordinates of the crosshair (X, Y). Recomputed on aim input.
  coord2d crosshair_angle;
  // Saved: yes
  // crosshair_x_pos, crosshair_y_pos: Raw horizontal and vertical offsets of the crosshair. Bounded by manual aim movement limits (-1.0f to 1.0f).
  f32 crosshair_x_pos;
  f32 crosshair_y_pos;
  // Saved: yes
  // guncrossdamp, gunaimdamp: Dampening interpolation factors for crosshair and gun model aiming drift (0.0f to 1.0f).
  f32 guncrossdamp;
  f32 gunaimdamp;
  // Saved: yes
  // field_FFC: 2D screen coordinate projection of the weapon/bullet direction (X, Y). Transformed into 3D coords for shot trajectory projection.
  coord2d field_FFC;
  // Saved: yes
  // gun_azimuth_angle, gun_azimuth_turning: Horizontal gun yaw angle and turning velocity when aiming (in radians).
  f32 gun_azimuth_angle;
  f32 gun_azimuth_turning;
  // Saved: yes
  // field_1010: 3D look/aim angle vector (pitch, yaw, roll). Y is initialized to -M_PI.
  coord3d field_1010;
  // Saved: yes
  // field_101C: Float 3D rotation matrix (Mtxf) tracking the gun's visual orientation, generated from field_1010.
  Mtxf field_101C;
  // Saved: yes
  // last_z_trigger_timer: Ticks elapsed since Z trigger was pressed (used to track weapon firing cooldown cycles).
  s32 last_z_trigger_timer;
  // Saved: yes
  // copiedgoldeneye: Boolean flag indicating if the GoldenEye Key has been analyzed/copied (Control Room objective status). Bounded to FALSE (0) or TRUE (1).
  s32 copiedgoldeneye;
  // Saved: yes
  // gunammooff: Bitfield of reasons for hiding the HUD ammo display (shares GUNSIGHTREASON_ constants). 0 if ammo is visible.
  s32 gunammooff;
  // Saved: yes
  // field_1068: Padding/unreferenced field in C source (likely padding or used in assembly).
  s32 field_1068;
  // Saved: yes
  // Current FOV values of the camera. Bounded between 60.0f (default) and 7.0f (maximum Sniper zoom).
  f32 sniper_zoom;
  f32 camera_zoom;

  // Saved: yes
  // Bitfield of reasons for hiding the crosshair sight (0 if sight is visible).
  //   - 0x01 (GUNSIGHTREASON_1): Watch menu active / paused.
  //   - 0x02 (GUNSIGHTREASON_NOTAIMING): Sight hidden because manual aim button is not held.
  //   - 0x04 (GUNSIGHTREASON_NOCONTROL): Sight hidden because the player lacks control.
  //   - 0x10 (GUNSIGHTREASON_DAMAGE): Sight hidden due to taking damage.
  s32 gunsightmode;
  // Saved: yes
  // Padding/unreferenced field in C source (likely padding or used in assembly).
  s32 field_112C;
  // Saved: yes
  // Transition timer and duration in frames/ticks for watch/sniper zoom transitions. Bounded between 0.0f and zoomintimemax (usually 30.0f or 45.0f).
  f32 zoomintime;
  f32 zoomintimemax;
  // Saved: yes
  // Current, starting, and ending target FOV angles (in degrees) for watch/sniper transitions.
  f32 zoominfovy;
  f32 zoominfovyold;
  f32 zoominfovynew;
  // Saved: yes
  // Render perspective parameters. Default FOV is 60.0f, default aspect is 1.333f (4:3) or 1.777f (16:9).
  f32 fovy;
  f32 aspect;

  // TODO: Investigate, document, determine save/load strategy and then provide plan on how the user should manually test for the following remaining fields:

  // Uncategorized / Padding / System fields
  s32 field_5C;
  s32 field_60;
  s32 field_64;
  s32 field_68;
  s32 field_8C;
  s32 field_94;
  f32 field_98;
  f32 field_A4;
  s32 field_AC;
  struct rect4f collision_bounds;
  s32 field_D0;
  s32 field_100;
  s32 field_104;
  s32 field_108;
  s32 field_10C;
  s32 movecentrerelease;
  s32 lookaheadcentreenabled;
  s32 automovecentreenabled;
  s32 fastmovecentreenabled;
  s32 automovecentre;

  // Auto-aim state
  s32 insightaimmode;
  s32 autoyaimenabled;
  f32 autoaimy;
  struct PropRecord *autoaim_target_y;
  s32 autoyaimtime60;
  s32 autoxaimenabled;
  f32 autoaimx;
  struct PropRecord *autoaim_target_x;
  s32 autoxaimtime60;

  // Fade & Breathing
  s32 field_1A0;
  s32 field_1A4;
  s32 field_1A8;
  s32 field_1AC;
  s32 field_1B0;
  s32 field_1B4;
  s32 field_1B8;
  s32 field_1BC;

  // Watch / Pause Menu state
  s32 watch_pause_time;
  s32 timer_1C4;
  s32 watch_animation_state;
  s32 outside_watch_menu;
  s32 open_close_solo_watch_menu;
  f32 field_1D4;
  f32 field_1D8;
  f32 pause_watch_position;
  f32 field_1E0;
  f32 field_1E4;
  f32 field_1E8;
  f32 field_1EC;
  f32 field_1F0;
  f32 field_1F4;
  s32 field_1F8;
  s32 field_1FC;
  s32 pausing_flag;
  f32 pause_starting_angle;
  f32 pause_related;
  f32 pause_target_angle;
  f32 field_210;
  f32 field_214;
  s32 field_218;
  s32 field_21C;
  s32 step_in_view_watch_animation;
  f32 pause_animation_counter;
  f32 pause_watch_related;
  f32 pause_watch_related_scaled;
  s32 something_with_watch_object_instance;
  s32 field_234;
  s32 field_238;
  s32 field_23C;
  s32 field_240;
  s32 watch_scale_destination;
  s32 field_248;
  s32 field_24C;
  s32 field_250;
  s32 field_254;
  f32 pause_watch_related_adjust;
  s32 field_25C; ... s32 field_3B0; // Watch screen vertices / states padding

  // Buttons Input State
  u16 buttons_pressed;
  u16 prev_buttons_pressed;

  // Death State
  f32 thetadie;
  f32 vertadie;
  s32 bondtype;
  s32 startnewbonddie;
  s32 redbloodfinished;
  s32 deathanimfinished;
  s32 field_42c;
  s32 controldef;

  // Visual model & properties
  Model *model;
  s32 field_59C;
  s32 field_5A0;
  s32 field_5A4;
  s32 field_5A8;
  s32 field_5AC;
  s32 field_5B0;
  s32 field_5B4;
  s32 field_5B8;
  s8 animFlipFlag;
  s8 field_5BD;
  s8 field_5BE;
  s8 field_5BF;
  f32 field_5C0;
  s32 field_5C4; ... s32 field_6CC; // Visual layout matrices / switches padding

  // Head Matrix / Viewports
  Mtxf bondheadmatrices[4];
  Vp viewports[2];
  s16 viewx;
  s16 viewy;
  s16 viewleft;
  s16 viewtop;

  // Weapons & Hands Rendering
  s32 hand_invisible[2];
  ModelFileHeader *ptr_hand_weapon_buffer[2];
  ModelFileHeader copy_of_body_obj_header[2];
  struct texpool item_related[2];
  s32 field_FC8;
  s32 field_FCC;
  s32 field_FD0;
  s32 z_trigger_timer;
  s32 field_FD8;
  struct rgba_u8 tileColor;
  s32 resetshadecol;
  s32 aimtype;

  // Rendering Matrices & Camera projection variables
  f32 c_screenwidth;
  f32 c_screenheight;
  f32 c_screenleft;
  f32 c_screentop;
  f32 c_perspnear;
  f32 c_perspfovy;
  f32 c_perspaspect;
  f32 c_halfwidth;
  f32 c_halfheight;
  f32 c_scalex;
  f32 c_scaley;
  f32 c_recipscalex;
  f32 c_recipscaley;
  Mtx* field_10C4;
  Mtx* field_10C8;
  Mtxf* field_10CC;
  s32 field_10D0;
  Mtxf* field_10D4;
  Mtx* projmatrix;
  Mtxf* projmatrixf;
  s32 field_10E0;
  s32 field_10E4;
  Mtxf* field_10E8;
  Mtxf* field_10EC;
  f32 c_scalelod60;
  f32 c_scalelod;
  f32 c_lodscalez;
  u32 c_lodscalezu32;
  coord3d c_cameratopnorm;
  coord3d c_cameraleftnorm;
  f32 screenxminf;
  f32 screenyminf;
  f32 screenxmaxf;
  f32 screenymaxf;

  // Inventory & Ammo Arrays
  u8 *bloodImgCur;
  u8 *bloodImgNxt;
  u8 *bloodImgBufPtrArray[2];
  s32 bloodImgIdx;
  s32 hudmessoff;
  s32 bondmesscnt;
  InvItem *p_itemcur;
  s32 equipmaxitems;
  textoverride *textoverrides;
  gunheld gunheldarr[10];

  // Weapon sway & control
  s32 magnetattracttime;
  f32 swaytarget;
  f32 swayoffset0;
  f32 swayoffset2;
  f32 field_1280;
  s32 players_cur_animation;
  f32 field_1288;

  // Cheat text display & status
  u16 cheat_display_text_related[20];
  u8 something_with_cheat_text;
  u8 can_display_cheat_text;
  u8 field_12B7;

  // Armor & Health Damage HUD overlay caches
  struct damage_display_parent armor_display_values[23];
  struct damage_display_parent health_display_values[23];

  // Watch vertices
  struct WatchRectangle buffer_for_watch_greenbackdrop_vertices[WATCH_NUMBER_SCREENS];
  struct WatchRectangle buffer_for_watch_static_vertices[1];
  s32 watch_body_armor_bar_gdl;
  s32 watch_health_bar_gdl;

  // Padding fields (field_19FC to field_243C)
  s32 field_19FC; ... s32 field_243C;

  // Control type & viewport bounds
  s32 lock_hand_model[2];
  s32 cur_player_control_type_0;
  s32 cur_player_control_type_1;
  f32 cur_player_control_type_2;
  s32 neg_vspacing_for_control_type_entry;
  u32 has_set_control_type_data;
  s32 field_2A6C;
  struct StandTile *field_2A70;
  s32 field_2A74;
  s32 field_2A78;
  s32 field_2A7C;
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

// Doubly-linked circular inventory item node.
struct InvItem {
  s32 type; // Item type: 1 = INV_ITEM_WEAPON, 2 = INV_ITEM_PROP, 3 = INV_ITEM_DUAL
  union {
    struct invitem_weap type_weap; // weapon ID
    struct invitem_prop type_prop; // PropRecord pointer for keys/objectives
    struct invitem_dual type_dual; // right/left hand weapon IDs for dual wielding
  } type_inv_item;
  struct InvItem *next; // circular pointer to next item
  struct InvItem *prev; // circular pointer to previous item
}

// Logical status, ammo tracking, and state of each hand viewport/model.
struct hand {
  ITEM_IDS weaponnum; // current active logical weapon
  ITEM_IDS weaponnum_watchmenu; // weapon selected in watch menu or -1
  ITEM_IDS previous_weapon; // previously held weapon
  s8 weapon_firing_status; // firing status
  s32 weapon_hold_time; // duration weapon has been held
  s32 when_detonating_mines_is_0; // mine detonator logic status
  s32 weapon_current_animation; // active animation index (0 = idle, 9 = reload, 0xE = draw)
  s32 weapon_ammo_in_magazine; // bullets currently in the magazine
  s32 weapon_next_weapon; // weapon being equipped
  s32 weapon_animation_trigger; // animation trigger flag
  ALSoundState *audioHandle; // absolute audio handle pointer (nullified on load to prevent crashes)
}
```
