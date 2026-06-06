#include <ultra64.h>
#include <fr.h>
#include <PR/os.h>
#include <bondtypes.h>
#include <bondconstants.h>
#include "practice_ui.h"
#include "game/textrelated.h"
#include "player_2.h"
#include "str.h"

#if defined(VERSION_US) || defined(VERSION_JP)
    #define BONDVIEW_VIEW_TOP_OFFSET_1 0x0C
    #define BONDVIEW_VIEW_TOP_OFFSET_2 0x28
    #define BONDVIEW_VIEW_TOP_OFFSET_3 0x10
#elif defined(VERSION_EU)
    #define BONDVIEW_VIEW_TOP_OFFSET_1 0x16
    #define BONDVIEW_VIEW_TOP_OFFSET_2 0x32
    #define BONDVIEW_VIEW_TOP_OFFSET_3 0x14
#endif

extern s32 ptrFontZurichBold;
extern s32 ptrFontZurichBoldChars;

// Declaring draw_blackbox_to_screen as it's not in textrelated.h
extern Gfx* draw_blackbox_to_screen(Gfx *glist, s32 *ulx, s32 *uly, s32 *lrx, s32 *lry);

// Extern game status functions
extern s32 get_ammo_type_for_weapon(s32 weapon_id);
extern s32 getCurrentPlayerWeaponId(s32 hand);
extern s32 is_clock_drawn_onscreen(void);

static char g_PracticeUiMessage[128] = "";
static OSTime g_PracticeUiExpiryTime = 0;

void practice_ui_display_message(const char *message) {
    if (message == NULL) {
        g_PracticeUiExpiryTime = 0;
        g_PracticeUiMessage[0] = '\0';
        return;
    }
    strncpy(g_PracticeUiMessage, message, sizeof(g_PracticeUiMessage) - 1);
    g_PracticeUiMessage[sizeof(g_PracticeUiMessage) - 1] = '\0';
    
    // Set expiration to exactly 2 seconds (2,000,000 microseconds) in real-world time
    g_PracticeUiExpiryTime = osGetTime() + OS_USEC_TO_CYCLES(2000000);
}

void practice_ui_tick(void) {
    if (g_PracticeUiExpiryTime == 0 || osGetTime() < g_PracticeUiExpiryTime) {
        return;
    }
    g_PracticeUiMessage[0] = '\0';
    g_PracticeUiExpiryTime = 0;
}

Gfx *practice_ui_render(Gfx *gdl) {
    s32 view_left;
    s32 view_vert;
    s32 view_horiz;
    s32 view_top;
    s32 view_top_offset;
    s32 view_left_offset;

    if (g_PracticeUiExpiryTime != 0 && g_PracticeUiMessage[0] != '\0') {
        gdl = microcode_constructor(gdl);
        view_left_offset = 0;
        view_top_offset = 0;

        // Measure text size using ZurichBold font
        textMeasure(&view_top_offset, &view_left_offset, g_PracticeUiMessage, (struct fontchar *)ptrFontZurichBoldChars, (struct font *)ptrFontZurichBold, 0);

        // Position it right-aligned at the bottom-right
        view_left = viGetViewLeft() + viGetViewWidth() - 0x1E - view_left_offset;
        view_horiz = view_left + view_left_offset;

        // Vertical offset calculation based on player split-screen layout and HUD status bar
        if (getPlayerCount() < 3) {
            if ((get_ammo_type_for_weapon(getCurrentPlayerWeaponId(GUNLEFT)) == 0) && (is_clock_drawn_onscreen() == 0)) {
                view_top = (viGetViewTop() + viGetViewHeight()) - BONDVIEW_VIEW_TOP_OFFSET_1;
            } else {
                view_top = (viGetViewTop() + viGetViewHeight()) - BONDVIEW_VIEW_TOP_OFFSET_2;
            }
#if !defined(VERSION_EU)
            if (get_cur_playernum() == 1) {
                view_top -= 8;
            }
#endif
        } else {
            view_top = viGetViewTop() + BONDVIEW_VIEW_TOP_OFFSET_3;
        }

        view_vert = view_top - view_top_offset;

        // Draw dark background box
        gdl = draw_blackbox_to_screen(gdl, &view_left, &view_vert, &view_horiz, &view_top);

        // Render text with glow
        gdl = combiner_bayer_lod_perspective(
            textRenderGlow(gdl, &view_left, &view_vert, g_PracticeUiMessage, ptrFontZurichBoldChars, ptrFontZurichBold, -1, 0x646464FF, (s16)(s32)viGetX(), (s16)viGetY(), 0, 0)
        );
    }

    return gdl;
}
