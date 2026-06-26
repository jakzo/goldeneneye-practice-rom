#include <bondconstants.h>
#include <ultra64.h>

#include "practice/practice_config.h"
#include "practice_debug.h"

#ifndef BOOT_LEVELID
#define BOOT_LEVELID LEVELID_TITLE
#endif

#if TEST_CASE == STATE_DOOR || TEST_CASE == STATE_PICKUP
#define BOOT_LEVELID LEVELID_RUNWAY
#endif

#if defined(DEV) || defined(TEST_CASE)
#define SKIP_CUTSCENES TRUE
#else
#define SKIP_CUTSCENES FALSE
#endif

struct PracticeConfig practice = {
    TRUE,           // skip_logos_on_startup
    TRUE,           // left_trigger_hotkeys
    BOOT_LEVELID,   // boot_level
    SKIP_CUTSCENES, // disable_intro_cutscenes
    5.0f,           // log_message_duration
    TRUE,           // show_hundredths_on_timer
    TRUE,           // show_mission_timer
    FALSE,          // grenade_cam
    TRUE,           // splits_enabled
};
