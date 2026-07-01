#include "practice_music.h"
#include "practice_timescale.h"
#include <music.h>
#include <PR/libaudio.h>

extern ALCSPlayer *g_musicXTrack1SeqPlayer;
extern ALCSPlayer *g_musicXTrack2SeqPlayer;
extern ALCSPlayer *g_musicXTrack3SeqPlayer;

static s32 g_PausedMusicTrackWasPlaying[3];

static ALCSPlayer *get_music_player(s32 track_index) {
  switch (track_index) {
  case 0:
    return g_musicXTrack1SeqPlayer;
  case 1:
    return g_musicXTrack2SeqPlayer;
  case 2:
    return g_musicXTrack3SeqPlayer;
  default:
    return NULL;
  }
}

void practice_music_pause(void) {
  s32 i;

  for (i = 0; i < 3; i++) {
    ALCSPlayer *player = get_music_player(i);
    g_PausedMusicTrackWasPlaying[i] = alCSPGetState(player) != 0;
    if (g_PausedMusicTrackWasPlaying[i]) {
      alCSPStop(player);
    }
  }
}

void practice_music_resume(void) {
  s32 i;

  for (i = 0; i < 3; i++) {
    if (g_PausedMusicTrackWasPlaying[i]) {
      alCSPPlay(get_music_player(i));
    }
    g_PausedMusicTrackWasPlaying[i] = FALSE;
  }
}

s32 practice_music_track_was_playing(s32 track_index) {
  ALCSPlayer *player = get_music_player(track_index);

  if (player == NULL) {
    return FALSE;
  }

  if (g_IsTimePaused) {
    return g_PausedMusicTrackWasPlaying[track_index];
  }

  return alCSPGetState(player) != 0;
}

void practice_music_set_paused_track_state(s32 track_index, s32 was_playing) {
  ALCSPlayer *player = get_music_player(track_index);

  if (player == NULL) {
    return;
  }

  if (g_IsTimePaused) {
    g_PausedMusicTrackWasPlaying[track_index] = was_playing;
    alCSPStop(player);
  } else {
    if (was_playing) {
      alCSPPlay(player);
    } else {
      alCSPStop(player);
    }
  }
}
