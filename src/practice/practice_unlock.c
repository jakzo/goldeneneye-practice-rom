#include "practice_unlock.h"
#include "bondconstants.h"
#include "game/file.h"
#include "game/file2.h"
#include <ultra64.h>

extern s32 fileGetSaveStageDifficultyTime(save_data *save,
                                          LEVEL_SOLO_SEQUENCE levelid,
                                          DIFFICULTY difficulty);
extern save_data *fileGetSaveForFoldernum(u32 folder);
extern void fileBuildWriteNewSave(u32 folder);
extern void fileSetDifficultyStageTime(save_data *save,
                                       LEVEL_SOLO_SEQUENCE levelid,
                                       DIFFICULTY difficulty, s32 newtime);
extern void fileWriteSave(save_data *save);

void practice_unlock_default_profile(void) {
  save_data *save;
  bool isUnlocked;
  s32 level;
  s32 diff;
  LEVEL_SOLO_SEQUENCE highestStageLevelId;
  DIFFICULTY highestStageDifficulty;

  save = fileGetSaveForFoldernum(FOLDER1);

  if (!save) {
    fileBuildWriteNewSave(FOLDER1);
    save = fileGetSaveForFoldernum(FOLDER1);
    if (!save)
      return;
  }

  isUnlocked = save->unlocked_cheats_1 == 0xFF &&
               save->unlocked_cheats_2 == 0xFF &&
               save->unlocked_cheats_3 == 0xFF && (save->flag_007 & 1) != 0;
  if (isUnlocked)
    return;

  fileGetHighestStageDifficultyCompletedForFolder(FOLDER1, &highestStageLevelId,
                                                  &highestStageDifficulty);
  if (highestStageLevelId < LEVELID_DAM ||
      highestStageDifficulty < DIFFICULTY_AGENT) {
    save->options = DEFAULT_OPTIONS |
                    ((CONTROLLER_CONFIG_SOLITARE << 8) & OPTION_CONTROLTYPE);
  }

  for (level = SP_LEVEL_DAM; level < SP_LEVEL_MAX; level++) {
    for (diff = DIFFICULTY_AGENT; diff < DIFFICULTY_007; diff++) {
      if (fileGetSaveStageDifficultyTime(save, level, diff) == 0) {
        fileSetDifficultyStageTime(save, level, diff, 0x3FF);
      }
    }
  }

  save->unlocked_cheats_1 = 0xFF;
  save->unlocked_cheats_2 = 0xFF;
  save->unlocked_cheats_3 = 0xFF;
  save->flag_007 |= 1;

  fileWriteSave(save);
}
