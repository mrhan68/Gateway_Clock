/**
 * @file test_ble_parsers.c
 * @brief Host verification for BLE Telemetry Parsers (BTHome V2, SIG 0x181A) (FSD §4.5, §14.5)
 */
#include "test_harness.h"

typedef struct {
    float temperature;
    float humidity;
    float battery_pct;
    bool valid;
} ble_parsed_data_t;

// Minimal parser for SIG Environmental Sensing (UUID 0x181A)
static bool parse_sig_env(const uint8_t *payload, size_t len, ble_parsed_data_t *out) {
    if (!payload || len < 4 || !out) return false;
    // Temp: int16_t (0.01 deg C)
    int16_t raw_temp = (int16_t)(payload[0] | (payload[1] << 8));
    // Humidity: uint16_t (0.01 %)
    uint16_t raw_hum = (uint16_t)(payload[2] | (payload[3] << 8));
    out->temperature = raw_temp / 100.0f;
    out->humidity = raw_hum / 100.0f;
    out->valid = true;
    return true;
}

// Minimal parser for BTHome V2 unencrypted packet
static bool parse_bthome_v2(const uint8_t *payload, size_t len, ble_parsed_data_t *out) {
    if (!payload || len < 5 || !out) return false;
    if (payload[0] != 0x40) return false; // BTHome v2 header

    size_t idx = 1;
    while (idx < len) {
        uint8_t obj_id = payload[idx++];
        if (obj_id == 0x02) { // Temperature (sint16 * 0.01)
            if (idx + 2 > len) break;
            int16_t raw = (int16_t)(payload[idx] | (payload[idx + 1] << 8));
            out->temperature = raw * 0.01f;
            idx += 2;
        } else if (obj_id == 0x03) { // Humidity (uint16 * 0.01)
            if (idx + 2 > len) break;
            uint16_t raw = (uint16_t)(payload[idx] | (payload[idx + 1] << 8));
            out->humidity = raw * 0.01f;
            idx += 2;
        } else if (obj_id == 0x01) { // Battery (uint8)
            if (idx + 1 > len) break;
            out->battery_pct = (float)payload[idx++];
        } else {
            break;
        }
    }
    out->valid = true;
    return true;
}

static void test_sig_0x181a_parser(void) {
    // 25.50 deg C = 2550 = 0x09F6; 60.00% RH = 6000 = 0x1770
    uint8_t raw[4] = { 0xF6, 0x09, 0x70, 0x17 };
    ble_parsed_data_t data = { 0 };

    TEST_ASSERT_TRUE(parse_sig_env(raw, sizeof(raw), &data));
    TEST_ASSERT_TRUE(data.temperature >= 25.49f && data.temperature <= 25.51f);
    TEST_ASSERT_TRUE(data.humidity >= 59.99f && data.humidity <= 60.01f);
}

static void test_bthome_v2_parser(void) {
    // Header 0x40, Temp (0x02, 23.45C = 2345 = 0x0929), Bat (0x01, 85%)
    uint8_t raw[] = { 0x40, 0x02, 0x29, 0x09, 0x01, 85 };
    ble_parsed_data_t data = { 0 };

    TEST_ASSERT_TRUE(parse_bthome_v2(raw, sizeof(raw), &data));
    TEST_ASSERT_TRUE(data.temperature >= 23.44f && data.temperature <= 23.46f);
    TEST_ASSERT_EQUAL_INT(85, (int)data.battery_pct);
}

void run_ble_parser_tests(void) {
    RUN_TEST(test_sig_0x181a_parser);
    RUN_TEST(test_bthome_v2_parser);
}
