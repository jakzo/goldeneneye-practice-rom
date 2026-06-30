#ifndef PRACTICE_INTRO_H
#define PRACTICE_INTRO_H

#include <bondtypes.h>
#include <ultra64.h>

struct SetupIntroCamera *
practice_choose_intro_camera(struct SetupIntroCamera *camera, s32 camera_count,
                             s32 stage_id, u32 random_value);

#endif /* PRACTICE_INTRO_H */
