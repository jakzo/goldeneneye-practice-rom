#include <os_extension.h>
#include <ultra64.h>
#include <bondgame.h>
#include <bondconstants.h>
#include <joy.h>
#include "os.h"
#include "player_2.h"
#include "practice_states.h"
#include "practice_config.h"
#include "practice_timescale.h"
#include "practice_ui.h"

extern int sprintf(char *dst, const char *fmt, ...);

bool practice_check_hotkeys(void)
{
    u16 jgbptf;

    u16 jgb = joyGetButtons(get_cur_playernum(), ANY_BUTTON);
    u16 trigger = practice.left_trigger_hotkeys ? L_TRIG : R_TRIG;

    if (!(jgb & trigger)) {
        if (g_IsTimePaused) unpause();
        return FALSE;
    }

    if (!g_IsTimePaused) pause();

    jgbptf = joyGetButtonsPressedThisFrame(get_cur_playernum(), ANY_BUTTON);

    if (jgbptf & D_JPAD) {
        save_game_state();
        return TRUE;
    }
    if (jgbptf & U_JPAD) {
        load_game_state();
        return TRUE;
    }

    if (jgbptf & L_JPAD) {
        char msg[32];
        f32 new_time_scale = ((s32)(g_TimeScale * 10.0f + 0.5f) - 1) * 0.1f;
        set_time_scale(new_time_scale);
        sprintf(msg, "Time scale: %.1f", g_TimeScale);
        practice_ui_display_message(msg);
        return TRUE;
    }
    if (jgbptf & R_JPAD) {
        char msg[32];
        f32 new_time_scale = ((s32)(g_TimeScale * 10.0f + 0.5f) + 1) * 0.1f;
        set_time_scale(new_time_scale);
        sprintf(msg, "Time scale: %.1f", g_TimeScale);
        practice_ui_display_message(msg);
        return TRUE;
    }

    return TRUE;
}
