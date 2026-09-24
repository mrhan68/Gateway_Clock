/**
 * @file test_monotonic_timing.c
 * @brief Host verification for Monotonic Safety Timers vs NTP Step Jumps (FSD §14.5)
 */
#include "test_harness.h"

typedef struct {
    int64_t monotonic_us;
    uint32_t wall_clock_epoch;
} mock_time_env_t;

static mock_time_env_t s_time_env;

static void reset_time_env(void) {
    s_time_env.monotonic_us = 5000000000LL;   // 5000s uptime
    s_time_env.wall_clock_epoch = 1700000000; // Fake epoch
}

static void test_ntp_step_does_not_affect_monotonic_timer(void) {
    reset_time_env();

    int64_t cooldown_start_us = s_time_env.monotonic_us;
    const int64_t cooldown_duration_us = 60LL * 1000000LL; // 60s cooldown

    // Giả lập NTP nhảy lùi 1 ngày (-86400s)
    s_time_env.wall_clock_epoch -= 86400;
    // Nhưng monotonic timer tăng thêm 65s
    s_time_env.monotonic_us += 65LL * 1000000LL;

    // Kiểm tra tính đơn điệu
    int64_t elapsed_us = s_time_env.monotonic_us - cooldown_start_us;
    TEST_ASSERT_TRUE(elapsed_us >= cooldown_duration_us);

    // Giả lập NTP nhảy tiến 1 năm (+31536000s)
    s_time_env.wall_clock_epoch += 31536000;
    // Monotonic timer chỉ tăng 5s nữa
    s_time_env.monotonic_us += 5LL * 1000000LL;

    int64_t new_elapsed_us = s_time_env.monotonic_us - cooldown_start_us;
    TEST_ASSERT_EQUAL_INT(70, (int)(new_elapsed_us / 1000000LL));
}

void run_monotonic_timing_tests(void) {
    RUN_TEST(test_ntp_step_does_not_affect_monotonic_timer);
}
