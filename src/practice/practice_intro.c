#include "practice/practice_intro.h"
#include "practice/practice_config.h"
#include <bondconstants.h>

#define DAM_GATE_INTRO_CAMERA_INDEX 3

struct SetupIntroCamera *
practice_choose_intro_camera(struct SetupIntroCamera *camera, s32 camera_count,
                             s32 stage_id, u32 random_value) {
  s32 camera_index;
  s32 excluded_index;

  if (stage_id == LEVELID_DAM && !practice.dam_gate_intro_enabled &&
      camera_count > 1 && DAM_GATE_INTRO_CAMERA_INDEX < camera_count) {
    // Intro cameras are linked in reverse setup order
    excluded_index = camera_count - 1 - DAM_GATE_INTRO_CAMERA_INDEX;
    camera_index = random_value % (camera_count - 1);
    if (camera_index >= excluded_index) {
      camera_index++;
    }
  } else {
    camera_index = random_value % camera_count;
  }

  while (camera_index > 0) {
    camera_index--;
    camera = camera->prev;
  }

  return camera;
}
