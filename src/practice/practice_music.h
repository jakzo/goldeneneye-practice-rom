#ifndef PRACTICE_MUSIC_H
#define PRACTICE_MUSIC_H

#include <ultra64.h>

void practice_music_pause(void);
void practice_music_resume(void);
s32 practice_music_track_was_playing(s32 track_index);
void practice_music_set_paused_track_state(s32 track_index, s32 was_playing);

#endif /* PRACTICE_MUSIC_H */
