/**
 * @file test_rule_engine.c
 * @brief Host verification for real firmware/main/rule_engine.c:
 *        Hysteresis deadband (30.0C +/- 1.5C), 60s cooldown, and multi-metric BLE packet routing.
 */
#include "test_harness.h"
#include "rule_engine.h"

extern int64_t g_mock_esp_timer_us;
extern int g_mock_rule_action_count;
extern rule_action_request_t g_mock_last_rule_action;

static void reset_rule_test_env(void) {
    g_mock_esp_timer_us = 1000000LL;
    g_mock_rule_action_count = 0;
    memset(&g_mock_last_rule_action, 0, sizeof(g_mock_last_rule_action));
    rule_engine_init();
    rule_engine_clear_rules();
    rule_engine_reset_lockouts();
}

static void test_hysteresis_deadband_and_cooldown_60s(void) {
    reset_rule_test_env();

    automation_rule_t r = {0};
    r.id = 1;
    strncpy(r.device_id, "sensor_temp_01", sizeof(r.device_id) - 1);
    r.condition_type = RULE_CONDITION_SENSOR_GT;
    r.threshold = 30.0f;
    r.hysteresis = 1.5f;
    r.cooldown_s = 60;
    strncpy(r.target_device_id, "relay_1", sizeof(r.target_device_id) - 1);
    strncpy(r.action_command, "ON", sizeof(r.action_command) - 1);
    r.is_active = true;
    TEST_ASSERT_TRUE(rule_engine_add_rule(&r));

    sensor_telemetry_t telem = {0};
    strncpy(telem.device_id, "sensor_temp_01", sizeof(telem.device_id) - 1);
    telem.valid = true;

    // 1. 30.5C >= 30.0C -> triggers ON
    telem.temperature = 30.5f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(1, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.target_device_id, "relay_1") == 0);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);

    // 2. 29.4C in deadband (28.5 < 29.4 < 30.0) -> stays ON (no new action)
    g_mock_esp_timer_us += 1000000LL;
    telem.temperature = 29.4f;
    TEST_ASSERT_EQUAL_INT(0, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(1, g_mock_rule_action_count);

    // 3. 28.5C <= (30.0 - 1.5) -> releases OFF immediately (0ms delay, bypasses cooldown)
    g_mock_esp_timer_us += 1000000LL;
    telem.temperature = 28.5f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(2, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "OFF") == 0);

    // 4. 30.5C at +10s (< 60s cooldown) -> suppressed by 60s cooldown
    g_mock_esp_timer_us += 10LL * 1000000LL;
    telem.temperature = 30.5f;
    TEST_ASSERT_EQUAL_INT(0, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(2, g_mock_rule_action_count);

    // 5. 30.5C at +61s (>= 60s cooldown) -> triggers ON again
    g_mock_esp_timer_us += 51LL * 1000000LL;
    telem.temperature = 30.5f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(3, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);
}

static void test_multi_metric_ble_packet_triggers_humidity_rule(void) {
    reset_rule_test_env();

    automation_rule_t hum_rule = {0};
    hum_rule.id = 2;
    strncpy(hum_rule.device_id, "sensor_hum_01", sizeof(hum_rule.device_id) - 1);
    hum_rule.condition_type = RULE_CONDITION_SENSOR_GT;
    hum_rule.threshold = 75.0f;
    hum_rule.hysteresis = 2.0f;
    hum_rule.cooldown_s = 60;
    strncpy(hum_rule.target_device_id, "relay_3", sizeof(hum_rule.target_device_id) - 1);
    strncpy(hum_rule.action_command, "ON", sizeof(hum_rule.action_command) - 1);
    hum_rule.is_active = true;
    TEST_ASSERT_TRUE(rule_engine_add_rule(&hum_rule));

    // Combined temp+hum packet arriving under device_id="sensor_temp_01"
    sensor_telemetry_t telem = {0};
    strncpy(telem.device_id, "sensor_temp_01", sizeof(telem.device_id) - 1);
    telem.temperature = 26.0f;
    telem.humidity = 80.0f;
    telem.valid = true;

    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(1, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.target_device_id, "relay_3") == 0);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);
}

static void simulate_relay_state_change(int relay_id, bool state, const char *reason) {
    relay_state_changed_t *rst = (relay_state_changed_t *)malloc(sizeof(relay_state_changed_t));
    memset(rst, 0, sizeof(relay_state_changed_t));
    rst->relay_id = relay_id;
    snprintf(rst->device_id, sizeof(rst->device_id), "relay_%d", relay_id);
    rst->state = state;
    strncpy(rst->reason, reason, sizeof(rst->reason) - 1);
    rst->monotonic_time_us = g_mock_esp_timer_us;
    rst->timestamp = (uint32_t)(g_mock_esp_timer_us / 1000000LL);
    event_bus_post(EVT_RELAY_STATE_CHANGED, rst, sizeof(relay_state_changed_t), false);
}

static void test_manual_override_priority(void) {
    reset_rule_test_env();

    automation_rule_t r = {0};
    r.id = 1;
    strncpy(r.device_id, "sensor_temp_01", sizeof(r.device_id) - 1);
    r.condition_type = RULE_CONDITION_SENSOR_GT;
    r.threshold = 30.0f;
    r.hysteresis = 1.5f;
    r.cooldown_s = 60;
    strncpy(r.target_device_id, "relay_1", sizeof(r.target_device_id) - 1);
    strncpy(r.action_command, "ON", sizeof(r.action_command) - 1);
    r.is_active = true;
    TEST_ASSERT_TRUE(rule_engine_add_rule(&r));

    sensor_telemetry_t telem = {0};
    strncpy(telem.device_id, "sensor_temp_01", sizeof(telem.device_id) - 1);
    telem.valid = true;

    // Scenario A: At 20.0C, user manually turns relay_1 ON (reason="MANUAL_CMD").
    // Evaluate telemetry 20.0C -> Rule #1 MUST NOT turn off the user's fan!
    simulate_relay_state_change(1, true, "MANUAL_CMD");
    telem.temperature = 20.0f;
    TEST_ASSERT_EQUAL_INT(0, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(0, g_mock_rule_action_count);

    // Scenario B: User manually turns relay_1 OFF -> manual override clears -> evaluate 31.0C -> Rule #1 turns fan ON
    g_mock_esp_timer_us += 5LL * 1000000LL;
    simulate_relay_state_change(1, false, "MANUAL_CMD");
    telem.temperature = 31.0f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(1, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);

    // Scenario C: While fan is ON by Rule #1 at 31.0C, user manually turns relay_1 OFF (reason="MANUAL_CMD")
    // -> advance clock by 120s (< 30m) -> evaluate 32.0C -> triggered_count == 0 (respects manual OFF)
    // -> advance clock past 30m -> evaluate 32.0C -> Rule #1 resumes Auto and triggers ON!
    g_mock_esp_timer_us += 10LL * 1000000LL;
    simulate_relay_state_change(1, false, "MANUAL_CMD");
    g_mock_esp_timer_us += 120LL * 1000000LL;
    telem.temperature = 32.0f;
    TEST_ASSERT_EQUAL_INT(0, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(1, g_mock_rule_action_count);

    // Advance past 30m (1800s)
    g_mock_esp_timer_us += 1700LL * 1000000LL;
    telem.temperature = 32.0f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(2, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);
}

static void test_cooling_override_spike_no_duplicate_heater_off(void) {
    reset_rule_test_env();

    // Rule 1: Fan > 30.0C (H=1.5) -> relay_1 ON
    automation_rule_t fan_rule = {0};
    fan_rule.id = 1;
    strncpy(fan_rule.device_id, "sensor_temp_01", sizeof(fan_rule.device_id) - 1);
    fan_rule.condition_type = RULE_CONDITION_SENSOR_GT;
    fan_rule.threshold = 30.0f;
    fan_rule.hysteresis = 1.5f;
    fan_rule.cooldown_s = 60;
    strncpy(fan_rule.target_device_id, "relay_1", sizeof(fan_rule.target_device_id) - 1);
    strncpy(fan_rule.action_command, "ON", sizeof(fan_rule.action_command) - 1);
    fan_rule.is_active = true;
    TEST_ASSERT_TRUE(rule_engine_add_rule(&fan_rule));

    // Rule 2: Heater < 22.0C (H=1.5) -> relay_2 ON
    automation_rule_t heater_rule = {0};
    heater_rule.id = 2;
    strncpy(heater_rule.device_id, "sensor_temp_01", sizeof(heater_rule.device_id) - 1);
    heater_rule.condition_type = RULE_CONDITION_SENSOR_LT;
    heater_rule.threshold = 22.0f;
    heater_rule.hysteresis = 1.5f;
    heater_rule.cooldown_s = 60;
    strncpy(heater_rule.target_device_id, "relay_2", sizeof(heater_rule.target_device_id) - 1);
    strncpy(heater_rule.action_command, "ON", sizeof(heater_rule.action_command) - 1);
    heater_rule.is_active = true;
    TEST_ASSERT_TRUE(rule_engine_add_rule(&heater_rule));

    sensor_telemetry_t telem = {0};
    strncpy(telem.device_id, "sensor_temp_01", sizeof(telem.device_id) - 1);
    telem.valid = true;

    // 1. 19.5C -> Heater (relay_2) turns ON
    telem.temperature = 19.5f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(1, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.target_device_id, "relay_2") == 0);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);

    // 2. Spike from 19.5C to 32.5C -> ONLY Fan (relay_1) ON must be emitted by rule_engine
    // (no duplicate relay_2 OFF in the same pass, since relay_controller enforces 0ms COOLING_OVERRIDE)
    g_mock_esp_timer_us += 2LL * 1000000LL;
    telem.temperature = 32.5f;
    TEST_ASSERT_EQUAL_INT(1, rule_engine_evaluate_telemetry(&telem));
    TEST_ASSERT_EQUAL_INT(2, g_mock_rule_action_count);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.target_device_id, "relay_1") == 0);
    TEST_ASSERT_TRUE(strcmp(g_mock_last_rule_action.action_command, "ON") == 0);
}

void run_rule_engine_tests(void) {
    RUN_TEST(test_hysteresis_deadband_and_cooldown_60s);
    RUN_TEST(test_multi_metric_ble_packet_triggers_humidity_rule);
    RUN_TEST(test_manual_override_priority);
    RUN_TEST(test_cooling_override_spike_no_duplicate_heater_off);
}
