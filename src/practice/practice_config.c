#include <bondconstants.h>
#include <ultra64.h>

#include "practice/practice_config.h"

struct PracticeConfig practice = {
    TRUE,           // skip_logos_on_startup
    TRUE,           // left_trigger_hotkeys
    LEVELID_RUNWAY, // boot_level
    FALSE,          // disable_intro_cutscenes
    TRUE,           // show_debug_logs
    5.0f,           // log_message_duration
    TRUE,           // show_hundredths_on_timer
    TRUE,           // show_mission_timer
    FALSE,          // grenade_cam
    TRUE,           // splits_enabled
};
