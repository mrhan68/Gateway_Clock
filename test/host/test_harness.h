#ifndef TEST_HARNESS_H
#define TEST_HARNESS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

extern int g_tests_run;
extern int g_tests_failed;

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("  [FAIL] %s:%d: Assertion '%s' failed.\n", __FILE__, __LINE__, #cond); \
        g_tests_failed++; \
        return; \
    } \
} while (0)

#define TEST_ASSERT_EQUAL_INT(expected, actual) do { \
    int exp_val = (int)(expected); \
    int act_val = (int)(actual); \
    if (exp_val != act_val) { \
        printf("  [FAIL] %s:%d: Expected %d, got %d.\n", __FILE__, __LINE__, exp_val, act_val); \
        g_tests_failed++; \
        return; \
    } \
} while (0)

#define TEST_ASSERT_TRUE(cond) TEST_ASSERT((cond))
#define TEST_ASSERT_FALSE(cond) TEST_ASSERT(!(cond))

#define RUN_TEST(func) do { \
    int failed_before = g_tests_failed; \
    g_tests_run++; \
    func(); \
    if (g_tests_failed == failed_before) { \
        printf("  [PASS] %s\n", #func); \
    } \
} while (0)

// Declarations of individual test suites
void run_relay_interlock_tests(void);
void run_monotonic_timing_tests(void);
void run_ble_parser_tests(void);
void run_scheduler_snooze_tests(void);

#endif // TEST_HARNESS_H
