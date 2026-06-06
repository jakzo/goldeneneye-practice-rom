#ifndef PRACTICE_UI_H
#define PRACTICE_UI_H

#include <ultra64.h>

void practice_ui_display_message(const char *message);
void practice_ui_tick(void);
Gfx *practice_ui_render(Gfx *gdl);

#endif /* PRACTICE_UI_H */
