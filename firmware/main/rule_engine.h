/**
 * @file rule_engine.h
 * @brief Rule Engine Condition Evaluator with Hysteresis & Edge-Triggering (FSD §4.7)
 *
 * Implements local IF-condition-THEN-action evaluation for automation rules.
 * Supports dual-threshold hysteresis (GT & LT), 60s cooldown on ON, 0ms immediate
 * turn-off on OFF, 5-minute safety lockout on cutoff, and /spiffs/rules.json sync.
 */

#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "event_bus.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RULES 16
#define SD_RULES_STORAGE_PATH     "/sdcard/rules.json"
#define SPIFFS_RULES_STORAGE_PATH "/spiffs/rules.json"

/**
 * @brief Rule condition types (FSD §4.7.4)
 */
typedef enum {
    RULE_CONDITION_SENSOR_GT = 0,   // sensor_value >= threshold (Ngưỡng trên, vd: Quạt làm mát)
    RULE_CONDITION_SENSOR_LT = 1,   // sensor_value <= threshold (Ngưỡng dưới, vd: Máy sưởi nhiệt)
    RULE_CONDITION_TIME_MATCH = 2,  // Đúng giờ hẹn (do scheduler lọc sẵn)
} rule_condition_type_t;

/**
 * @brief Automation Rule Data Model (FSD §4.7.4)
 */
typedef struct {
    int id;
    char device_id[32];              // Cảm biến phát sinh dữ liệu (vd: "sensor_temp_01")
    rule_condition_type_t condition_type;
    float threshold;                 // Ngưỡng nhiệt độ / độ ẩm kích hoạt
    float hysteresis;                // Biên trễ chống dao động tiếp điểm (mặc định 1.5°C)
    uint32_t cooldown_s;             // Thời gian làm nguội tối thiểu cho chiều BẬT (mặc định 60s)
    char target_device_id[32];       // Relay đích ("relay_1", "relay_2", ...)
    char action_command[32];         // "ON", "OFF", "SET:75"
    union {
        bool is_active;              // Trạng thái bật/tắt của luật (FSD §4.7.4)
        bool enabled;                // Tương thích ngược với web_server và serial_cli
    };
    // Quản lý trạng thái nội bộ (Runtime State)
    bool current_state;              // true: đang trong vùng kích hoạt (Active Zone)
    int64_t last_triggered_mono_us;  // Monotonic microsecond timestamp
    int64_t lockout_until_mono_us;   // Monotonic microsecond timestamp
    uint32_t last_triggered_ts;      // Epoch thời điểm kích hoạt gần nhất (tương thích ngược)
    uint32_t lockout_until_ts;       // Epoch thời hạn khóa an toàn (tương thích ngược)
} automation_rule_t;

/**
 * @brief Sensor telemetry type alias to ble_telemetry_t for unified event bus compatibility
 */
typedef ble_telemetry_t sensor_telemetry_t;

/**
 * @brief Initialize Rule Engine: loads rules from /spiffs/rules.json (or defaults)
 *        and registers event bus handlers.
 */
void rule_engine_init(void);

/**
 * @brief Reload rules from storage (/spiffs/rules.json) or defaults.
 */
void rule_engine_reload_rules(void);

/**
 * @brief Add or update an automation rule in memory and persist to /spiffs/rules.json.
 * @param rule Pointer to automation rule struct.
 * @return true if added/updated, false if table full.
 */
bool rule_engine_add_rule(const automation_rule_t *rule);

/**
 * @brief Clear all active automation rules.
 */
void rule_engine_clear_rules(void);

/**
 * @brief Delete a specific automation rule by ID.
 * @param rule_id Unique rule ID.
 * @return true if found and deleted, false if not found.
 */
bool rule_engine_delete_rule(int rule_id);

/**
 * @brief Get total number of active rules.
 */
int rule_engine_get_rule_count(void);

/**
 * @brief Copy active rules into user-provided destination array.
 * @param dest Output buffer for rules.
 * @param max_rules Max number of rules dest can hold.
 * @return Number of rules copied.
 */
int rule_engine_get_rules(automation_rule_t *dest, int max_rules);

int rule_engine_evaluate_telemetry(const sensor_telemetry_t *telem);

/**
 * @brief Reset all active rule safety lockouts and cooldowns (test harness hook)
 */
void rule_engine_reset_lockouts(void);

/**
 * @brief Apply automation rules array received from Cloud MQTT config/schedule without re-publishing sync.
 */
bool rule_engine_apply_cloud_json(const cJSON *rules_arr);

/**
 * @brief Publish current rules to Cloud MQTT (gateway/<gw_id>/rules/sync).
 */
void rule_engine_publish_sync(void);

#ifdef __cplusplus
}
#endif

#endif // RULE_ENGINE_H
