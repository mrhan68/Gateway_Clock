#ifndef BLE_ADAPTER_H
#define BLE_ADAPTER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "event_bus.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BLE_MAX_DEVICES         16

// Note: ble_telemetry_t is defined in event_bus.h (FSD §4.1.5)

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
 * @brief Pause GAP scan to prioritize Wi-Fi provisioning radio time
 */
void ble_adapter_pause_scan(void);

/**
 * @brief Resume GAP scan with standard coexistence duty cycle (30%)
 */
void ble_adapter_resume_scan(void);

/**
 * @brief Print status of BLE adapter, known devices, and telemetry statistics
 */
void ble_adapter_print_status(void);

/**
 * @brief Inject synthetic telemetry data into Event Bus (test harness & CLI hook)
 * @param device_id Target device ID
 * @param temp Temperature value (°C)
 * @param hum Humidity value (%RH)
 */
void ble_adapter_inject_test_packet(const char *device_id, float temp, float hum);

/**
 * @brief Synchronize registered BLE sensor list from Cloud JSON array
 * @param devices_arr cJSON array of objects containing device_id and mac_address
 */
void ble_adapter_sync_devices_from_json(const cJSON *devices_arr);

/**
 * @brief Build a JSON array of known/discovered BLE devices and their runtime telemetry stats
 * @return Newly allocated cJSON array (caller must cJSON_Delete), or NULL on allocation failure
 */
cJSON *ble_adapter_get_devices_json(void);

#ifdef __cplusplus
}
#endif

#endif // BLE_ADAPTER_H
