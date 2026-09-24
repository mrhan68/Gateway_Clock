/**
 * @file test_scheduler_snooze.c
 * @brief Host verification for Scheduler Alarm Snooze (+9 min) & Anti-Retroactive Alarm (FSD §4.8, §14.5)
 */
#include "test_harness.h"

typedef struct {
    int hour;
    int minute;
} clock_time_t;

static clock_time_t calculate_snooze_time(int current_hour, int current_minute) {
    clock_time_t snooze;
    int total_min = current_hour * 60 + current_minute + 9;
    snooze.hour = (total_min / 60) % 24;
    snooze.minute = total_min % 60;
    return snooze;
}

static bool should_trigger_alarm(int sched_hour, int sched_min, int current_hour, int current_min, bool already_triggered_today) {
    if (already_triggered_today) return false;
    // Chỉ kích hoạt nếu đúng phút
    return (sched_hour == current_hour && sched_min == current_min);
}

static void test_snooze_standard_addition(void) {
    // 07:00 + 9 min = 07:09
    clock_time_t t1 = calculate_snooze_time(7, 0);
    TEST_ASSERT_EQUAL_INT(7, t1.hour);
    TEST_ASSERT_EQUAL_INT(9, t1.minute);

    // 07:55 + 9 min = 08:04
    clock_time_t t2 = calculate_snooze_time(7, 55);
    TEST_ASSERT_EQUAL_INT(8, t2.hour);
    TEST_ASSERT_EQUAL_INT(4, t2.minute);
}

static void test_snooze_midnight_rollover(void) {
    // 23:55 + 9 min = 00:04 ngày hôm sau
    clock_time_t t = calculate_snooze_time(23, 55);
    TEST_ASSERT_EQUAL_INT(0, t.hour);
    TEST_ASSERT_EQUAL_INT(4, t.minute);
}

static void test_no_retroactive_alarm_on_reboot(void) {
    // Lịch hẹn là 07:00. Thiết bị reboot lúc 07:15.
    // Không được kích hoạt chuông báo ngược (No retroactive alarm)
    bool triggered = should_trigger_alarm(7, 0, 7, 15, false);
    TEST_ASSERT_FALSE(triggered);
}

void run_scheduler_snooze_tests(void) {
    RUN_TEST(test_snooze_standard_addition);
    RUN_TEST(test_snooze_midnight_rollover);
    RUN_TEST(test_no_retroactive_alarm_on_reboot);
}
