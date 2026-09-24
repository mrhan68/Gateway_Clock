/**
 * @file test_relay_interlock.c
 * @brief Host verification for Asymmetrical Safety Interlocking & Dwell Time (FSD §4.10, §14.5)
 */
#include "test_harness.h"

#define RELAY_ID_FAN         1
#define RELAY_ID_HEATER      2
#define RELAY_ID_COMPRESSOR  3
#define RELAY_ID_AUX         4

#define ERR_OK                      0
#define ERR_RELAY_INTERLOCK_CONFLICT 3
#define ERR_RELAY_DWELL_LOCKED      4

typedef struct {
    bool state;
    int64_t last_off_time_us;
} mock_relay_t;

static mock_relay_t s_relays[5];
static int64_t s_mock_monotonic_us = 1000000000LL;

static void reset_mock_relays(void) {
    memset(s_relays, 0, sizeof(s_relays));
    s_mock_monotonic_us = 1000000000LL;
}

static int mock_relay_set(int relay_id, bool target_state) {
    if (relay_id < 1 || relay_id > 4) return -1;

    if (target_state) {
        // 1. Asymmetrical interlock: Quạt ON -> Từ chối Bật Sưởi
        if (relay_id == RELAY_ID_HEATER && s_relays[RELAY_ID_FAN].state) {
            return ERR_RELAY_INTERLOCK_CONFLICT;
        }

        // 2. Cooling Override: Sưởi ON -> Bật Quạt -> Ngắt Sưởi tức thì 0ms, Bật Quạt thành công
        if (relay_id == RELAY_ID_FAN && s_relays[RELAY_ID_HEATER].state) {
            s_relays[RELAY_ID_HEATER].state = false;
            s_relays[RELAY_ID_HEATER].last_off_time_us = s_mock_monotonic_us;
        }

        // 3. Dwell time 180s cho Relay 3 (Máy nén)
        if (relay_id == RELAY_ID_COMPRESSOR) {
            if (s_relays[RELAY_ID_COMPRESSOR].last_off_time_us > 0) {
                int64_t elapsed_us = s_mock_monotonic_us - s_relays[RELAY_ID_COMPRESSOR].last_off_time_us;
                if (elapsed_us < 180LL * 1000000LL) {
                    return ERR_RELAY_DWELL_LOCKED;
                }
            }
        }
    } else {
        s_relays[relay_id].last_off_time_us = s_mock_monotonic_us;
    }

    s_relays[relay_id].state = target_state;
    return ERR_OK;
}

static void test_fan_on_rejects_heater(void) {
    reset_mock_relays();
    TEST_ASSERT_EQUAL_INT(ERR_OK, mock_relay_set(RELAY_ID_FAN, true));
    TEST_ASSERT_TRUE(s_relays[RELAY_ID_FAN].state);

    int rc = mock_relay_set(RELAY_ID_HEATER, true);
    TEST_ASSERT_EQUAL_INT(ERR_RELAY_INTERLOCK_CONFLICT, rc);
    TEST_ASSERT_FALSE(s_relays[RELAY_ID_HEATER].state);
    TEST_ASSERT_TRUE(s_relays[RELAY_ID_FAN].state);
}

static void test_heater_on_cooling_override_fan(void) {
    reset_mock_relays();
    TEST_ASSERT_EQUAL_INT(ERR_OK, mock_relay_set(RELAY_ID_HEATER, true));
    TEST_ASSERT_TRUE(s_relays[RELAY_ID_HEATER].state);

    int rc = mock_relay_set(RELAY_ID_FAN, true);
    TEST_ASSERT_EQUAL_INT(ERR_OK, rc);
    TEST_ASSERT_FALSE(s_relays[RELAY_ID_HEATER].state);
    TEST_ASSERT_TRUE(s_relays[RELAY_ID_FAN].state);
}

static void test_compressor_dwell_time_180s(void) {
    reset_mock_relays();
    TEST_ASSERT_EQUAL_INT(ERR_OK, mock_relay_set(RELAY_ID_COMPRESSOR, true));
    TEST_ASSERT_EQUAL_INT(ERR_OK, mock_relay_set(RELAY_ID_COMPRESSOR, false));

    // Ngay sau khi tắt (10s sau) -> Phải bị từ chối
    s_mock_monotonic_us += 10LL * 1000000LL;
    int rc = mock_relay_set(RELAY_ID_COMPRESSOR, true);
    TEST_ASSERT_EQUAL_INT(ERR_RELAY_DWELL_LOCKED, rc);
    TEST_ASSERT_FALSE(s_relays[RELAY_ID_COMPRESSOR].state);

    // Sau 181s -> Phải bật được thành công
    s_mock_monotonic_us += 175LL * 1000000LL; // Tổng 185s
    rc = mock_relay_set(RELAY_ID_COMPRESSOR, true);
    TEST_ASSERT_EQUAL_INT(ERR_OK, rc);
    TEST_ASSERT_TRUE(s_relays[RELAY_ID_COMPRESSOR].state);
}

void run_relay_interlock_tests(void) {
    RUN_TEST(test_fan_on_rejects_heater);
    RUN_TEST(test_heater_on_cooling_override_fan);
    RUN_TEST(test_compressor_dwell_time_180s);
}
