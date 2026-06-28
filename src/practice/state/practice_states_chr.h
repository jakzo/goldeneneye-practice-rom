#ifndef PRACTICE_STATES_CHR_H
#define PRACTICE_STATES_CHR_H

#include "practice_states_stream.h"
#include <bondtypes.h>

typedef struct ChrAttachmentIndices {
  s16 weapons_held[3];
  s16 hat;
  s16 weapon_model[2];
  s8 weaponnum[2];
  u32 weapon_flags[2];
  s16 hat_model;
  u32 hat_flags;
} ChrAttachmentIndices;

typedef struct ChrAllocationState {
  s8 headnum;
  s8 bodynum;
  f32 heading;
} ChrAllocationState;

void save_chr_record(StateStream *stream, const ChrRecord *chr);
void load_chr_allocation_state(StateStream *stream,
                               ChrAllocationState *allocation);
void load_chr_record(StateStream *stream, ChrRecord *chr,
                     ChrAttachmentIndices *attachments);
void load_chr_prop_spatial_state(PropRecord *prop, const coord3d *pos,
                                 s32 stan_offset, const u8 rooms[4],
                                 bool is_new_prop);

#endif /* PRACTICE_STATES_CHR_H */
