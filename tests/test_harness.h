#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <string.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define CHECK(cond, msg)                                             \
    do {                                                            \
        if (cond) {                                                 \
            tests_passed++;                                         \
            printf("  [PASS] %s\n", msg);                          \
        } else {                                                    \
            tests_failed++;                                         \
            printf("  [FAIL] %s  (%s:%d)\n", msg, __FILE__, __LINE__); \
        }                                                           \
    } while (0)

#define RUN_TEST(fn)                     \
    do {                                 \
        printf("[TEST] %s\n", #fn);      \
        fn();                            \
    } while (0)

#define TEST_SUMMARY()                                                    \
    do {                                                                  \
        printf("\n----------------------------------------\n");          \
        printf("Passed: %d   Failed: %d\n", tests_passed, tests_failed); \
        printf("----------------------------------------\n");            \
        return tests_failed == 0 ? 0 : 1;                                 \
    } while (0)

#endif /* TEST_HARNESS_H */
