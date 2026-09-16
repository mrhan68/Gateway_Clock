#include "rule_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt_adapter.h"
#include "mock_peripherals.h"

static const char *TAG = "RULE_ENGINE";

static automation_rule_t s_rules[MAX_RULES];
static int s_rule_count = 0;
static SemaphoreHandle_t s_rule_mutex = NULL;

static void lock_rules(void)
{
    if (s_rule_mutex != NULL) {
        xSemaphoreTake(s_rule_mutex, portMAX_DELAY);
    }
}

static void unlock_rules(void)
{
    if (s_rule_mutex != NULL) {
        xSemaphoreGive(s_rule_mutex);
    }
}

static void load_default_rules(void)
{
    lock_rules();
    s_rule_count = 0;

    // Rule 1: High temperature turns on cooling fan
    s_rules[s_rule_count++] = (automation_rule_t){
        .id = 1,
        .device_id = "sensor_temp_01",
        .condition_type = RULE_CONDITION_SENSOR_GT,
        .threshold = 30.0f,
        .target_device_id = "fan_relay_01",
        .action_command = "ON",
        .enabled = true,
    };

    // Rule 2: Low temperature turns on heater
    s_rules[s_rule_count++] = (automation_rule_t){
        .id = 2,
        .device_id = "sensor_temp_01",
        .condition_type = RULE_CONDITION_SENSOR_LT,
        .threshold = 20.0f,
        .target_device_id = "heater_relay_01",
        .action_command = "ON",
        .enabled = true,
    };

    // Rule 3: High humidity turns on dehumidifier
    s_rules[s_rule_count++] = (automation_rule_t){
        .id = 3,
        .device_id = "sensor_hum_01",
        .condition_type = RULE_CONDITION_SENSOR_GT,
        .threshold = 75.0f,
        .target_device_id = "dehumidifier_01",
        .action_command = "START",
        .enabled = true,
    };

    unlock_rules();
    ESP_LOGI(TAG, "Loaded %d default automation rules", s_rule_count);
}

void rule_engine_reload_rules(void)
{
    ESP_LOGI(TAG, "Reloading rules from storage...");
    load_default_rules();
}

bool rule_engine_add_rule(const automation_rule_t *rule)
{
    if (rule == NULL) return false;

    lock_rules();
    // Check if rule ID already exists, update it
    for (int i = 0; i < s_rule_count; i++) {
        if (s_rules[i].id == rule->id) {
            s_rules[i] = *rule;
            unlock_rules();
            ESP_LOGI(TAG, "Updated existing rule #%d (dev='%s')", rule->id, rule->device_id);
            return true;
        }
    }

    if (s_rule_count >= MAX_RULES) {
        unlock_rules();
        ESP_LOGW(TAG, "Rule table full (max %d)", MAX_RULES);
        return false;
    }

    s_rules[s_rule_count++] = *rule;
    unlock_rules();
    ESP_LOGI(TAG, "Added new rule #%d (dev='%s', thr=%.1f -> target='%s' cmd='%s')",
             rule->id, rule->device_id, rule->threshold, rule->target_device_id, rule->action_command);
    return true;
}

void rule_engine_clear_rules(void)
{
    lock_rules();
    s_rule_count = 0;
    unlock_rules();
    ESP_LOGI(TAG, "Cleared all rules");
}

int rule_engine_get_rule_count(void)
{
    lock_rules();
    int cnt = s_rule_count;
    unlock_rules();
    return cnt;
}

int rule_engine_get_rules(automation_rule_t *dest, int max_rules)
{
    if (dest == NULL || max_rules <= 0) return 0;
    lock_rules();
    int count = (s_rule_count < max_rules) ? s_rule_count : max_rules;
    for (int i = 0; i < count; i++) {
        dest[i] = s_rules[i];
    }
    unlock_rules();
    return count;
}

int rule_engine_evaluate_telemetry(const sensor_telemetry_t *telem)
{
    if (telem == NULL) return 0;

    // FSD §4.7.5: If valid is false -> safe-default, do NOT evaluate rules!
    if (!telem->valid) {
        ESP_LOGW(TAG, "[SAFE-DEFAULT] Sensor '%s' data invalid (fault/offline). Preserving safe default!",
                 telem->device_id);
        printf("[RULE_ENGINE] Sensor '%s' reports INVALID data. Skipping rule evaluation (Safe Default).\n",
               telem->device_id);
        fflush(stdout);
        event_bus_post(EVT_SENSOR_VALUE_INVALID, NULL, 0, false);
        return 0;
    }

    int matched_count = 0;
    lock_rules();

    for (int i = 0; i < s_rule_count; i++) {
        automation_rule_t *r = &s_rules[i];
        if (!r->enabled) continue;

        if (strcmp(r->device_id, telem->device_id) != 0) {
            continue;
        }

        bool matched = false;
        if (r->condition_type == RULE_CONDITION_SENSOR_GT) {
            if (telem->value > r->threshold) {
                matched = true;
            }
        } else if (r->condition_type == RULE_CONDITION_SENSOR_LT) {
            if (telem->value < r->threshold) {
                matched = true;
            }
        }

        if (matched) {
            matched_count++;
            printf("\n[RULE_ENGINE] >>> MATCHED Rule #%d! Device '%s' val=%.2f %s thr=%.2f -> Target '%s' CMD='%s'\n",
                   r->id, r->device_id, telem->value,
                   (r->condition_type == RULE_CONDITION_SENSOR_GT) ? ">" : "<",
                   r->threshold, r->target_device_id, r->action_command);
            fflush(stdout);

            rule_action_request_t *req = (rule_action_request_t *)malloc(sizeof(rule_action_request_t));
            if (req != NULL) {
                strncpy(req->target_device_id, r->target_device_id, sizeof(req->target_device_id) - 1);
                req->target_device_id[sizeof(req->target_device_id) - 1] = '\0';
                strncpy(req->action_command, r->action_command, sizeof(req->action_command) - 1);
                req->action_command[sizeof(req->action_command) - 1] = '\0';
                req->source_rule_id = r->id;

                event_bus_post(EVT_RULE_ACTION_REQUEST, req, sizeof(rule_action_request_t), false);
            }
        }
    }

    unlock_rules();
    return matched_count;
}

static void on_telemetry_event(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(sensor_telemetry_t)) {
        const sensor_telemetry_t *telem = (const sensor_telemetry_t *)evt->data;
        rule_engine_evaluate_telemetry(telem);
    }
}

static void on_rs485_telemetry_event(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(rs485_telemetry_t)) {
        const rs485_telemetry_t *r = (const rs485_telemetry_t *)evt->data;
        sensor_telemetry_t telem = {
            .value = r->value,
            .valid = r->valid,
        };
        strncpy(telem.device_id, r->device_id, sizeof(telem.device_id) - 1);
        telem.device_id[sizeof(telem.device_id) - 1] = '\0';
        rule_engine_evaluate_telemetry(&telem);
    }
}

static void on_schedule_automation(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL) {
        const automation_rule_t *rule = (const automation_rule_t *)evt->data;
        printf("\n[RULE_ENGINE] Automation schedule triggered: Target='%s' CMD='%s'\n",
               rule->target_device_id, rule->action_command);
        fflush(stdout);

        rule_action_request_t *req = (rule_action_request_t *)malloc(sizeof(rule_action_request_t));
        if (req != NULL) {
            strncpy(req->target_device_id, rule->target_device_id, sizeof(req->target_device_id) - 1);
            req->target_device_id[sizeof(req->target_device_id) - 1] = '\0';
            strncpy(req->action_command, rule->action_command, sizeof(req->action_command) - 1);
            req->action_command[sizeof(req->action_command) - 1] = '\0';
            req->source_rule_id = rule->id;

            event_bus_post(EVT_RULE_ACTION_REQUEST, req, sizeof(rule_action_request_t), false);
        }
    }
}

static void on_rule_action_request(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL) {
        const rule_action_request_t *act = (const rule_action_request_t *)evt->data;
        printf("[ACTION_EXECUTOR] >>> Executed Action on Target='%s': Command='%s' (Rule #%d)\n",
               act->target_device_id, act->action_command, act->source_rule_id);
        fflush(stdout);

        // Notify cloud via MQTT if available
        if (mqtt_adapter_is_connected()) {
            char topic[96];
            char payload[128];
            snprintf(topic, sizeof(topic), "device/%s/action", act->target_device_id);
            snprintf(payload, sizeof(payload),
                     "{\"command\":\"%s\",\"source_rule\":%d,\"status\":\"EXECUTED\"}",
                     act->action_command, act->source_rule_id);
            mqtt_adapter_publish(topic, payload);
        }
    }
}

static void on_cloud_config_updated(const gateway_event_t *evt)
{
    (void)evt;
    rule_engine_reload_rules();
}

void rule_engine_init(void)
{
    if (s_rule_mutex == NULL) {
        s_rule_mutex = xSemaphoreCreateMutex();
    }

    load_default_rules();

    // Register handlers according to FSD §4.7.3
    event_bus_register_handler(EVT_BLE_DEVICE_DATA, on_telemetry_event);
    event_bus_register_handler(EVT_RS485_DEVICE_DATA, on_rs485_telemetry_event);
    event_bus_register_handler(EVT_MQTT_MESSAGE_RECEIVED, on_telemetry_event);
    event_bus_register_handler(EVT_SCHEDULE_AUTOMATION_TRIGGERED, on_schedule_automation);
    event_bus_register_handler(EVT_RULE_ACTION_REQUEST, on_rule_action_request);
    event_bus_register_handler(EVT_CONFIG_UPDATED_FROM_CLOUD, on_cloud_config_updated);

    ESP_LOGI(TAG, "Rule Engine initialized with %d active rules", s_rule_count);
}
