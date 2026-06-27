#ifndef PRACTICE_TESTS_H
#define PRACTICE_TESTS_H

// --- start test cases ---
#define STATE_DOOR 1
#define STATE_PICKUP 2
#define STATE_BUNKER 3
// --- end test cases ---

#if TEST_CASE == STATE_DOOR || TEST_CASE == STATE_PICKUP
#define PRACTICE_TEST_BOOT_LEVEL LEVELID_RUNWAY
#elif TEST_CASE == STATE_BUNKER
#define PRACTICE_TEST_BOOT_LEVEL LEVELID_BUNKER1
#else
#define PRACTICE_TEST_BOOT_LEVEL BOOT_LEVELID
#endif

#if defined(DEV) || defined(TEST_CASE)
#define PRACTICE_TEST_SKIP_INTRO TRUE
#else
#define PRACTICE_TEST_SKIP_INTRO FALSE
#endif

void practice_tests_tick(void);

#endif /* PRACTICE_TESTS_H */
