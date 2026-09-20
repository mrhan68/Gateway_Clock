#ifndef BLE_ADAPTER_H
#define BLE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_MAX_DEVICES         16
#define BLE_CMD_QUEUE_DEPTH     10

/**
 * @brief BLE Telemetry structure (conforms to FSD v1.1 §4.1.5 and rule_engine sensor_telemetry_t)
 */
typedef struct {
    char device_id[32];  // Matched device_id (e.g., "sensor_temp_01")
    float value;         // Sensor reading (temperature or humidity)
    bool valid;          // Data validity flag
} ble_telemetry_t;

/**
 * @brief BLE Downlink Command
 */
typedef struct {
    char device_id[32];
    char command[32];
} ble_cmd_t;

/**
 * @brief Initialize BLE Adapter: initializes NimBLE stack, event handlers, and scan task.
 */
void ble_adapter_init(void);

/**
 * @brief Register a known BLE device by MAC address or Name
 * @param device_id Virtual ID known to Rule Engine / MQTT (e.g., "sensor_temp_01")
 * @param mac_address Hex string formatted as "AA:BB:CC:DD:EE:FF" or identifier
 */
void ble_adapter_register_device(const char *device_id, const char *mac_address);

/**
 * @brief Check if GAP passive scanner is active
 */
bool ble_adapter_is_scanning(void);

/**
 * @brief Start/resume GAP scan
 */
void ble_adapter_start_scan(void);

/**
 * @brief Stop GAP scan
 */
void ble_adapter_stop_scan(void);

/**
 * @brief Print status of BLE adapter, known devices, and telemetry statistics
 */
void ble_adapter_print_status(void);

/**
 * @brief Inject synthetic telemetry data into Event Bus (test harness & CLI hook)
 * @param device_id Target device ID
 * @param value Telemetry value
 */
void ble_adapter_inject_test_packet(const char *device_id, float value);

#ifdef __cplusplus
}
#endif

#endif // BLE_ADAPTER_H
