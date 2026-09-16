#ifndef RULE_ENGINE_H
#define RULE_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "event_bus.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_RULES 16

typedef enum {
    RULE_CONDITION_SENSOR_GT = 0,   // sensor_value > threshold
    RULE_CONDITION_SENSOR_LT = 1,   // sensor_value < threshold
    RULE_CONDITION_TIME_MATCH = 2,  // matched by scheduled time
} rule_condition_type_t;

typedef struct {
    int id;
    char device_id[32];              // Sensor or trigger source device ID
    rule_condition_type_t condition_type;
    float threshold;
    char target_device_id[32];       // Actuator or output device ID
    char action_command[32];         // "ON", "OFF", "SET:75", etc.
    bool enabled;
} automation_rule_t;

typedef struct {
    char target_device_id[32];
    char action_command[32];
    int source_rule_id;
} rule_action_request_t;

typedef struct {
    char device_id[32];
    float value;
    bool valid;
} sensor_telemetry_t;

/**
 * @brief Initialize Rule Engine: registers event bus handlers and loads default rules.
 */
void rule_engine_init(void);

/**
 * @brief Reload rules from storage or default configuration.
 */
void rule_engine_reload_rules(void);

/**
 * @brief Add or update an automation rule.
 */
bool rule_engine_add_rule(const automation_rule_t *rule);

/**
 * @brief Clear all active automation rules.
 */
void rule_engine_clear_rules(void);

/**
 * @brief Get total number of active rules.
 */
int rule_engine_get_rule_count(void);

/**
 * @brief Copy active rules into user-provided array.
 */
int rule_engine_get_rules(automation_rule_t *dest, int max_rules);

/**
 * @brief Evaluate telemetry directly against active rules.
 * @return Number of rules matched and triggered.
 */
int rule_engine_evaluate_telemetry(const sensor_telemetry_t *telem);

#ifdef __cplusplus
}
#endif

#endif // RULE_ENGINE_H
