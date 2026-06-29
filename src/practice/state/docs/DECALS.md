# Scorch marks, bullet holes & flying particles (`explosions.h`)

These effects live in global ring buffers allocated per stage by
`alloc_explosion_smoke_casing_scorch_impact_buffers` (`initexplosioncasing.c`),
not in `PropRecord`s. They are serialized at the end of the props payload
(`save_decals_state` / `load_decals_state` and
`save_flying_particles_state` / `load_flying_particles_state` in
`practice_states_props.c`) because bullet impacts reference props by index and
need the prop table rebuilt first.

## Scorch (`g_ScorchBuffer`, `g_NumScorchEntries`, `SCORCH_BUFFER_LEN = 20`)

Dark circular burn marks left on the ground/walls by explosions.

```c
struct Scorch {
    s16 roomid;          // -1 when the slot is unused
    u8 unk02;
    s8 unk03;
    struct coord3d pos;
    f32 explosion_size;
    struct Model *model; // unused by explosionRenderScorchBuffer
    Vtx vertex_list[4];  // world-space quad
};
```

- Allocated only in single-player (`getPlayerCount() == 1`); the buffer pointer
  is `NULL` otherwise and the renderer early-returns, so all access is
  NULL-guarded.
- `g_NumScorchEntries` is the ring cursor (next write slot, wraps at 20).
- Fully world-space; rendering checks only `roomid >= 0`, so a slot is cleared by
  setting `roomid = -1`.
- **Saved**: cursor + each active entry at its buffer index (`roomid`, `unk02`,
  `unk03`, `pos`, `explosion_size`, `vertex_list`). `model` is unused and reset
  to `NULL` on load.

## Bullet impacts (`g_BulletImpactBuffer`, `g_NumImpactEntries`, `BULLET_IMPACT_BUFFER_LEN = 100`)

Bullet holes on world surfaces and on props.

```c
struct BulletImpact {
    s16 room;            // -1 when the slot is unused
    s16 impact_type;     // index into g_ImpactTypes / impactimages
    u32 unk04;
    Vtx vertex_list[4];
    PropRecord* prop;    // NULL for world-surface holes
    s8 model_render_pos_index; // index into prop->model->render_pos
    s8 room_clear_flag;
    s16 unk4E;
};
```

- `g_NumImpactEntries` is the ring cursor (wraps at 100). Slots are cleared with
  `room = -1`.
- World holes (`prop == NULL`) are world-space and self-contained.
- Prop-attached holes store `prop` and a `model_render_pos_index` into that
  prop's model. The pointer is mapped to a prop index via `get_prop_index` and
  resolved with `get_enabled_prop_by_index`; if the owning prop no longer exists
  the impact is dropped (`room = -1`) so a stale model index is never rendered.
- **Saved**: cursor + each active entry at its buffer index, with `prop` stored
  as a prop index.

## Flying particles (`g_FlyingParticlesBuffer`, `g_NumParticleEntries`, `max_particles`)

Airborne shrapnel/debris flung out by explosions (not a stamped decal, but part
of the same explosion-effect family).

```c
struct FlyingParticles {
    s32 unk00;                   // > 0 = live; counts up to its lifetime cap
    struct coord3d position;
    struct coord3d rotation;
    struct coord3d position_drift; // velocity; gravity is applied to .f[1]
    struct coord3d rotation_drift; // angular velocity
    s32 unk34;                   // unused
    Vtx vertex_list[4];          // per-particle local quad (size/texcoord/shade)
};
```

- `max_particles` is set at stage load (`MAX_FLYING_PARTICLES / playercount`,
  halved on Streets/Depot), so it matches between save and load of the same
  level. `g_NumParticleEntries` is the ring cursor (wraps at `max_particles`).
- Fully world-space and pointer-free. `vertex_list` is fixed at spawn; the
  renderer rebuilds the world matrix from `position`/`rotation` each frame, so
  only `unk00 <= 0` (cleared) deactivates a slot.
- **Saved**: cursor + each live particle's whole struct at its buffer index
  (~122 bytes on the wire per live particle: 2-byte index + 120-byte struct).
  Saved sparsely, so the cost is ~0 unless the state is captured during the
  brief window an explosion's debris is still airborne.
