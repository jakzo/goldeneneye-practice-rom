#include <bondconstants.h>
#include <ultra64.h>

#include "practice/practice_config.h"
#include "practice_tests.h"

struct PracticeConfig practice = {
    TRUE,                     // skip_logos_on_startup
    TRUE,                     // left_trigger_hotkeys
    PRACTICE_TEST_BOOT_LEVEL, // boot_level
    PRACTICE_TEST_SKIP_INTRO, // disable_intro_cutscenes
    5.0f,                     // log_message_duration
    TRUE,                     // show_hundredths_on_timer
    TRUE,                     // show_mission_timer
    FALSE,                    // grenade_cam
    TRUE,                     // splits_enabled
};
