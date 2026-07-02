#ifndef PRACTICE_TIMESCALE_H
#define PRACTICE_TIMESCALE_H

#include <ultra64.h>

extern f32 g_TimeScale;
extern f32 g_TimeScaleFinal;
extern f32 g_FractionalClockTimerAcc;
extern s32 g_IsTimeScaleChanged;
extern s32 g_IsTimePaused;
extern s32 g_TimeScaleDeltaFrames;
extern s32 g_PrePauseDeltaFrames;

void set_time_scale(f32 scale);
void pause();
void unpause();
void restore_rng_if_frame_dropped(void);

#endif /* PRACTICE_TIMESCALE_H */
