#ifndef PRACTICE_TESTS_H
#define PRACTICE_TESTS_H

// --- start test cases ---
#define STATE_DOOR 1
#define STATE_GRENADE 2
#define STATE_BUNKER 3
#define STATE_DAM 4
#define FIRE_SLOWMO 5
#define RNG_LOAD 6
// --- end test cases ---

#ifndef PRACTICE_TEST_BOOT_LEVEL
#if TEST_CASE == STATE_DOOR || TEST_CASE == STATE_GRENADE ||                   \
    TEST_CASE == FIRE_SLOWMO || TEST_CASE == RNG_LOAD
#define PRACTICE_TEST_BOOT_LEVEL LEVELID_RUNWAY
#elif TEST_CASE == STATE_BUNKER
#define PRACTICE_TEST_BOOT_LEVEL LEVELID_BUNKER1
#elif TEST_CASE == STATE_DAM
#define PRACTICE_TEST_BOOT_LEVEL LEVELID_DAM
#elif defined(PRACTICE_BOOT_LEVEL_OVERRIDE)
#define PRACTICE_TEST_BOOT_LEVEL BOOT_LEVELID
#endif
#endif

#ifndef PRACTICE_TEST_SKIP_INTRO
#if defined(DEV) || defined(TEST_CASE)
#define PRACTICE_TEST_SKIP_INTRO TRUE
#endif
#endif

void practice_tests_tick();
void practice_tests_frame();

#endif /* PRACTICE_TESTS_H */
