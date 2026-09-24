/**
 * @file rule_engine.c
 * @brief Rule Engine Condition Evaluator with Hysteresis & Edge-Triggering (FSD §4.7)
 */

#include "rule_engine.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "mqtt_adapter.h"

static const char *TAG = "RULE_ENGINE";

#define RULES_STORAGE_PATH "/spiffs/rules.json"

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

static uint32_t get_current_epoch(void)
{
    time_t now = time(NULL);
    if (now > 100000) {
        return (uint32_t)now;
    }
    return (uint32_t)(esp_timer_get_time() / 1000000ULL);
}

static void load_default_rules(void)
{
    s_rule_count = 0;

    // Rule 1: High temperature turns on cooling fan (FSD §4.7.5: threshold 30.0, hysteresis 1.5, cooldown 60)
    s_rules[s_rule_count++] = (automation_rule_t){
        .id = 1,
        .device_id = "sensor_temp_01",
        .condition_type = RULE_CONDITION_SENSOR_GT,
        .threshold = 30.0f,
        .hysteresis = 1.5f,
        .cooldown_s = 60,
        .target_device_id = "relay_1",
        .action_command = "ON",
        .is_active = true,
        .current_state = false,
        .last_triggered_ts = 0,
        .lockout_until_ts = 0,
        .last_triggered_mono_us = 0,
        .lockout_until_mono_us = 0,
    };

    // Rule 2: Low temperature turns on heater (FSD §4.7.5: threshold 22.0, hysteresis 1.5, cooldown 60)
    s_rules[s_rule_count++] = (automation_rule_t){
        .id = 2,
        .device_id = "sensor_temp_01",
        .condition_type = RULE_CONDITION_SENSOR_LT,
        .threshold = 22.0f,
        .hysteresis = 1.5f,
        .cooldown_s = 60,
        .target_device_id = "relay_2",
        .action_command = "ON",
        .is_active = true,
        .current_state = false,
        .last_triggered_ts = 0,
        .lockout_until_ts = 0,
        .last_triggered_mono_us = 0,
        .lockout_until_mono_us = 0,
    };

    // Rule 3: High humidity turns on dehumidifier (threshold 75.0, hysteresis 2.0, cooldown 60)
    s_rules[s_rule_count++] = (automation_rule_t){
        .id = 3,
        .device_id = "sensor_hum_01",
        .condition_type = RULE_CONDITION_SENSOR_GT,
        .threshold = 75.0f,
        .hysteresis = 2.0f,
        .cooldown_s = 60,
        .target_device_id = "relay_3",
        .action_command = "ON",
        .is_active = true,
        .current_state = false,
        .last_triggered_ts = 0,
        .lockout_until_ts = 0,
        .last_triggered_mono_us = 0,
        .lockout_until_mono_us = 0,
    };

    ESP_LOGI(TAG, "Loaded %d default automation rules", s_rule_count);
}

static void save_rules_to_storage(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return;

    for (int i = 0; i < s_rule_count; i++) {
        cJSON *item = cJSON_CreateObject();
        if (!item) continue;
        cJSON_AddNumberToObject(item, "id", s_rules[i].id);
        cJSON_AddStringToObject(item, "sensor_device_id", s_rules[i].device_id);
        cJSON_AddStringToObject(item, "condition_type",
            (s_rules[i].condition_type == RULE_CONDITION_SENSOR_GT) ? "gt" : "lt");
        cJSON_AddNumberToObject(item, "threshold", s_rules[i].threshold);
        cJSON_AddNumberToObject(item, "hysteresis", s_rules[i].hysteresis);
        cJSON_AddNumberToObject(item, "cooldown_s", s_rules[i].cooldown_s);
        cJSON_AddStringToObject(item, "target_device_id", s_rules[i].target_device_id);
        cJSON_AddStringToObject(item, "action_command", s_rules[i].action_command);
        cJSON_AddBoolToObject(item, "enabled", s_rules[i].is_active);
        cJSON_AddItemToArray(arr, item);
    }

    char *str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (str != NULL) {
        FILE *f = fopen(RULES_STORAGE_PATH, "w");
        if (f != NULL) {
            fputs(str, f);
            fclose(f);
            ESP_LOGI(TAG, "Saved %d rules to %s", s_rule_count, RULES_STORAGE_PATH);
        } else {
            ESP_LOGW(TAG, "Failed to open %s for writing", RULES_STORAGE_PATH);
        }
        free(str);
    }
}

static bool load_rules_from_storage(void)
{
    FILE *f = fopen(RULES_STORAGE_PATH, "r");
    if (f == NULL) {
        ESP_LOGI(TAG, "Rules file %s not found. Using defaults.", RULES_STORAGE_PATH);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 16384) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc(sz + 1);
    if (buf == NULL) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, sz, f);
    fclose(f);
    buf[read_bytes] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (root == NULL || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(root);
    if (count == 0) {
        cJSON_Delete(root);
        return false;
    }

    s_rule_count = 0;
    for (int i = 0; i < count && s_rule_count < MAX_RULES; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        if (!item || !cJSON_IsObject(item)) continue;

        cJSON *id = cJSON_GetObjectItem(item, "id");
        cJSON *sensor = cJSON_GetObjectItem(item, "sensor_device_id");
        if (!sensor) sensor = cJSON_GetObjectItem(item, "device_id");

        cJSON *cond = cJSON_GetObjectItem(item, "condition_type");
        if (!cond) cond = cJSON_GetObjectItem(item, "condition");

        cJSON *thresh = cJSON_GetObjectItem(item, "threshold");
        cJSON *hyst = cJSON_GetObjectItem(item, "hysteresis");
        cJSON *cool = cJSON_GetObjectItem(item, "cooldown_s");

        cJSON *target = cJSON_GetObjectItem(item, "target_device_id");
        if (!target) target = cJSON_GetObjectItem(item, "target");

        cJSON *act = cJSON_GetObjectItem(item, "action_command");
        if (!act) act = cJSON_GetObjectItem(item, "action");

        cJSON *en = cJSON_GetObjectItem(item, "enabled");
        if (!en) en = cJSON_GetObjectItem(item, "is_active");

        if (!id || !sensor || !cond || !thresh || !target || !act) continue;

        automation_rule_t *r = &s_rules[s_rule_count];
        memset(r, 0, sizeof(automation_rule_t));

        r->id = id->valueint;
        strncpy(r->device_id, sensor->valuestring, sizeof(r->device_id) - 1);

        if (cJSON_IsString(cond)) {
            if (strcmp(cond->valuestring, "lt") == 0 || strcmp(cond->valuestring, "<") == 0) {
                r->condition_type = RULE_CONDITION_SENSOR_LT;
            } else {
                r->condition_type = RULE_CONDITION_SENSOR_GT;
            }
        } else if (cJSON_IsNumber(cond)) {
            r->condition_type = (rule_condition_type_t)cond->valueint;
        }

        r->threshold = (float)thresh->valuedouble;
        r->hysteresis = (hyst && cJSON_IsNumber(hyst) && hyst->valuedouble > 0) ? (float)hyst->valuedouble : 1.5f;
        r->cooldown_s = (cool && cJSON_IsNumber(cool) && cool->valueint > 0) ? (uint32_t)cool->valueint : 60;

        strncpy(r->target_device_id, target->valuestring, sizeof(r->target_device_id) - 1);
        strncpy(r->action_command, act->valuestring, sizeof(r->action_command) - 1);
        r->is_active = (en != NULL) ? cJSON_IsTrue(en) : true;
        r->current_state = false;
        r->last_triggered_ts = 0;
        r->lockout_until_ts = 0;
        r->last_triggered_mono_us = 0;
        r->lockout_until_mono_us = 0;

        s_rule_count++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d rules from storage %s", s_rule_count, RULES_STORAGE_PATH);
    return (s_rule_count > 0);
}

void rule_engine_reload_rules(void)
{
    ESP_LOGI(TAG, "Reloading rules from storage...");
    lock_rules();
    if (!load_rules_from_storage()) {
        load_default_rules();
    }
    unlock_rules();
}

bool rule_engine_add_rule(const automation_rule_t *rule)
{
    if (rule == NULL) return false;

    lock_rules();
    // Check if rule ID already exists, update it
    for (int i = 0; i < s_rule_count; i++) {
        if (s_rules[i].id == rule->id) {
            s_rules[i] = *rule;
            if (s_rules[i].hysteresis <= 0.0f) s_rules[i].hysteresis = 1.5f;
            if (s_rules[i].cooldown_s == 0) s_rules[i].cooldown_s = 60;
            save_rules_to_storage();
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

    s_rules[s_rule_count] = *rule;
    if (s_rules[s_rule_count].hysteresis <= 0.0f) s_rules[s_rule_count].hysteresis = 1.5f;
    if (s_rules[s_rule_count].cooldown_s == 0) s_rules[s_rule_count].cooldown_s = 60;
    s_rules[s_rule_count].current_state = false;
    s_rules[s_rule_count].last_triggered_ts = 0;
    s_rules[s_rule_count].lockout_until_ts = 0;
    s_rules[s_rule_count].last_triggered_mono_us = 0;
    s_rules[s_rule_count].lockout_until_mono_us = 0;
    s_rule_count++;

    save_rules_to_storage();
    unlock_rules();
    ESP_LOGI(TAG, "Added new rule #%d (dev='%s', thr=%.1f hyst=%.1f cool=%lu -> target='%s' cmd='%s')",
             rule->id, rule->device_id, rule->threshold, rule->hysteresis,
             (unsigned long)rule->cooldown_s, rule->target_device_id, rule->action_command);
    return true;
}

void rule_engine_clear_rules(void)
{
    lock_rules();
    s_rule_count = 0;
    save_rules_to_storage();
    unlock_rules();
    ESP_LOGI(TAG, "Cleared all rules");
}

bool rule_engine_delete_rule(int rule_id)
{
    lock_rules();
    int found_idx = -1;
    for (int i = 0; i < s_rule_count; i++) {
        if (s_rules[i].id == rule_id) {
            found_idx = i;
            break;
        }
    }

    if (found_idx == -1) {
        unlock_rules();
        ESP_LOGW(TAG, "Rule #%d not found for deletion", rule_id);
        return false;
    }

    for (int i = found_idx; i < s_rule_count - 1; i++) {
        s_rules[i] = s_rules[i + 1];
    }
    s_rule_count--;
    save_rules_to_storage();
    unlock_rules();
    ESP_LOGI(TAG, "Deleted rule #%d (remaining rules: %d)", rule_id, s_rule_count);
    return true;
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

static void post_rule_action(const char *target, const char *cmd, int rule_id)
{
    rule_action_request_t *req = (rule_action_request_t *)malloc(sizeof(rule_action_request_t));
    if (req != NULL) {
        memset(req, 0, sizeof(rule_action_request_t));
        snprintf(req->command_id, sizeof(req->command_id), "cmd-rule-%d-%lu",
                 rule_id, (unsigned long)get_current_epoch());
        strncpy(req->target_device_id, target, sizeof(req->target_device_id) - 1);
        strncpy(req->action_command, cmd, sizeof(req->action_command) - 1);
        req->source_rule_id = rule_id;
        req->timestamp = get_current_epoch();

        event_bus_post(EVT_RULE_ACTION_REQUEST, req, sizeof(rule_action_request_t), false);
    }
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

    int64_t now_mono = esp_timer_get_time();
    uint32_t now = get_current_epoch();
    int matched_count = 0;
    lock_rules();

    for (int i = 0; i < s_rule_count; i++) {
        automation_rule_t *r = &s_rules[i];
        if (!r->is_active) continue;

        if (strcmp(r->device_id, telem->device_id) != 0) {
            continue;
        }

        // FSD §4.7.5: Check 5-minute safety lockout (Monotonic clock)
        if (now_mono < r->lockout_until_mono_us || (r->lockout_until_ts > 0 && now < r->lockout_until_ts)) {
            ESP_LOGD(TAG, "Rule #%d is in lockout, skipping", r->id);
            continue;
        }

        float val = telem->value;
        if (strstr(r->device_id, "hum") != NULL && telem->humidity > 0.0f) {
            val = telem->humidity;
        }

        float hyst = (r->hysteresis > 0.0f) ? r->hysteresis : 1.5f;
        int64_t cooldown_us = (r->cooldown_s > 0) ? ((int64_t)r->cooldown_s * 1000000ULL) : 60000000ULL;

        if (r->condition_type == RULE_CONDITION_SENSOR_GT) {
            // Upper Threshold (e.g. Cooling Fan: Turn ON when val >= threshold, Turn OFF when val <= threshold - hysteresis)
            if (val >= r->threshold) {
                // Edge Trigger: only trigger if currently inactive and cooldown elapsed
                if (!r->current_state) {
                    if (r->last_triggered_mono_us == 0 || (now_mono - r->last_triggered_mono_us >= cooldown_us)) {
                        r->current_state = true;
                        r->last_triggered_mono_us = now_mono;
                        r->last_triggered_ts = now;
                        matched_count++;
                        const char *cmd = (strlen(r->action_command) > 0) ? r->action_command : "ON";
                        ESP_LOGI(TAG, ">>> [GT EDGE-UP] Rule #%d TRIGGERED! Sensor '%s' val=%.2f >= thr=%.2f -> '%s' CMD='%s'",
                                 r->id, r->device_id, val, r->threshold, r->target_device_id, cmd);
                        post_rule_action(r->target_device_id, cmd, r->id);
                    }
                }
            } else if (val <= (r->threshold - hyst)) {
                // Edge Trigger: only turn off if currently active (bypass cooldown, 0ms delay)
                if (r->current_state) {
                    r->current_state = false;
                    matched_count++;
                    ESP_LOGI(TAG, ">>> [GT EDGE-DOWN] Rule #%d RELEASED! Sensor '%s' val=%.2f <= release=%.2f -> '%s' CMD='OFF'",
                             r->id, r->device_id, val, r->threshold - hyst, r->target_device_id);
                    post_rule_action(r->target_device_id, "OFF", r->id);
                }
            }
            // In deadband (threshold - hyst < val < threshold): preserve current state
        } else if (r->condition_type == RULE_CONDITION_SENSOR_LT) {
            // Lower Threshold (e.g. Heater: Turn ON when val <= threshold, Turn OFF when val >= threshold + hysteresis)
            if (val <= r->threshold) {
                // Edge Trigger: only trigger if currently inactive and cooldown elapsed
                if (!r->current_state) {
                    if (r->last_triggered_mono_us == 0 || (now_mono - r->last_triggered_mono_us >= cooldown_us)) {
                        r->current_state = true;
                        r->last_triggered_mono_us = now_mono;
                        r->last_triggered_ts = now;
                        matched_count++;
                        const char *cmd = (strlen(r->action_command) > 0) ? r->action_command : "ON";
                        ESP_LOGI(TAG, ">>> [LT EDGE-UP] Rule #%d TRIGGERED! Sensor '%s' val=%.2f <= thr=%.2f -> '%s' CMD='%s'",
                                 r->id, r->device_id, val, r->threshold, r->target_device_id, cmd);
                        post_rule_action(r->target_device_id, cmd, r->id);
                    }
                }
            } else if (val >= (r->threshold + hyst)) {
                // Edge Trigger: only turn off if currently active (bypass cooldown, 0ms delay)
                if (r->current_state) {
                    r->current_state = false;
                    matched_count++;
                    ESP_LOGI(TAG, ">>> [LT EDGE-DOWN] Rule #%d RELEASED! Sensor '%s' val=%.2f >= release=%.2f -> '%s' CMD='OFF'",
                             r->id, r->device_id, val, r->threshold + hyst, r->target_device_id);
                    post_rule_action(r->target_device_id, "OFF", r->id);
                }
            }
            // In deadband (threshold < val < threshold + hyst): preserve current state
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

static void on_schedule_automation(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL) {
        const automation_rule_t *rule = (const automation_rule_t *)evt->data;
        ESP_LOGI(TAG, "[RULE_ENGINE] Automation schedule triggered: Target='%s' CMD='%s'",
                 rule->target_device_id, rule->action_command);

        post_rule_action(rule->target_device_id, rule->action_command, rule->id);
    }
}

static void on_rule_action_request(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(rule_action_request_t)) {
        const rule_action_request_t *act = (const rule_action_request_t *)evt->data;
        ESP_LOGI(TAG, "[ACTION_EXECUTOR] >>> Executed Action on Target='%s': Command='%s' (Rule #%d)",
                 act->target_device_id, act->action_command, act->source_rule_id);

        // Notify cloud via MQTT if available
        if (mqtt_adapter_is_connected()) {
            char topic[96];
            char payload[160];
            snprintf(topic, sizeof(topic), "device/%s/action", act->target_device_id);
            snprintf(payload, sizeof(payload),
                     "{\"command\":\"%s\",\"source_rule\":%d,\"status\":\"EXECUTED\",\"command_id\":\"%s\"}",
                     act->action_command, act->source_rule_id, act->command_id);
            mqtt_adapter_publish(topic, payload);
        }
    }
}

static void on_cloud_config_updated(const gateway_event_t *evt)
{
    (void)evt;
    rule_engine_reload_rules();
}

static void on_relay_state_changed(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(relay_state_changed_t)) {
        return;
    }

    const relay_state_changed_t *rst = (const relay_state_changed_t *)evt->data;
    uint32_t now = get_current_epoch();
    int64_t now_mono = esp_timer_get_time();

    lock_rules();
    for (int i = 0; i < s_rule_count; i++) {
        automation_rule_t *r = &s_rules[i];
        if (strcmp(r->target_device_id, rst->device_id) == 0) {
            // Two-way synchronization: reflect actual hardware state
            r->current_state = rst->state;

            // FSD §4.7.5: Engage 5-minute safety lockout if shut down by safety or interlock
            if (!rst->state && (strcmp(rst->reason, "SAFETY_CUTOFF") == 0 ||
                               strcmp(rst->reason, "INTERLOCK_CUTOFF") == 0 ||
                               strcmp(rst->reason, "SENSOR_OFFLINE") == 0)) {
                r->lockout_until_mono_us = now_mono + 300000000ULL; // 5-minute lockout (300s)
                r->lockout_until_ts = now + 300;
                ESP_LOGW(TAG, "[SAFETY LOCKOUT] Rule #%d (target '%s') locked out for 300s due to %s",
                         r->id, rst->device_id, rst->reason);
            }
        }
    }
    unlock_rules();
}

static void on_command_response(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(command_ack_t)) {
        return;
    }

    const command_ack_t *ack = (const command_ack_t *)evt->data;
    // If a rule-initiated action failed or was rejected (e.g., interlock conflict, compressor dwell locked)
    if (!ack->success && strncmp(ack->command_id, "cmd-rule-", 9) == 0) {
        lock_rules();
        for (int i = 0; i < s_rule_count; i++) {
            if (strcmp(s_rules[i].target_device_id, ack->target_device_id) == 0) {
                s_rules[i].current_state = false;
                ESP_LOGW(TAG, "[RULE_ENGINE] Action rejected on '%s' (err=%d: %s). Reverted current_state=false",
                         ack->target_device_id, ack->error_code, ack->message);
            }
        }
        unlock_rules();
    }
}

static void on_device_offline_alert(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(device_alert_t)) {
        return;
    }

    const device_alert_t *alert = (const device_alert_t *)evt->data;
    if (alert->alert_type == ALERT_SENSOR_OFFLINE || alert->alert_type == ALERT_SENSOR_CORRUPTED) {
        lock_rules();
        for (int i = 0; i < s_rule_count; i++) {
            automation_rule_t *r = &s_rules[i];
            if (strcmp(r->device_id, alert->device_id) == 0) {
                // If sensor offline or corrupted: engage 300s lockout, mark current_state false
                r->current_state = false;
                r->lockout_until_mono_us = esp_timer_get_time() + 300000000ULL;
                r->lockout_until_ts = get_current_epoch() + 300;
                ESP_LOGW(TAG, "[FAIL-SAFE] Sensor '%s' offline/corrupted alert received. Rule #%d locked out for 300s",
                         alert->device_id, r->id);
                // Post immediate shutoff for target device
                post_rule_action(r->target_device_id, "OFF", r->id);
            }
        }
        unlock_rules();
        event_bus_post(EVT_SENSOR_VALUE_INVALID, NULL, 0, false);
    }
}

void rule_engine_init(void)
{
    if (s_rule_mutex == NULL) {
        s_rule_mutex = xSemaphoreCreateMutex();
    }

    lock_rules();
    if (!load_rules_from_storage()) {
        load_default_rules();
    }
    unlock_rules();

    // Register handlers according to FSD §4.7.3 & §4.7.5
    event_bus_register_handler(EVT_BLE_DEVICE_DATA, on_telemetry_event);
    event_bus_register_handler(EVT_MQTT_MESSAGE_RECEIVED, on_telemetry_event);
    event_bus_register_handler(EVT_SCHEDULE_AUTOMATION_TRIGGERED, on_schedule_automation);
    event_bus_register_handler(EVT_RULE_ACTION_REQUEST, on_rule_action_request);
    event_bus_register_handler(EVT_CONFIG_UPDATED_FROM_CLOUD, on_cloud_config_updated);
    event_bus_register_handler(EVT_RELAY_STATE_CHANGED, on_relay_state_changed);
    event_bus_register_handler(EVT_COMMAND_RESPONSE, on_command_response);
    event_bus_register_handler(EVT_DEVICE_OFFLINE_ALERT, on_device_offline_alert);

    ESP_LOGI(TAG, "Rule Engine initialized with %d active rules (Hysteresis & Safety Lockout ready)", s_rule_count);
}

void rule_engine_reset_lockouts(void)
{
    lock_rules();
    for (int i = 0; i < s_rule_count; i++) {
        s_rules[i].lockout_until_ts = 0;
        s_rules[i].lockout_until_mono_us = 0;
        s_rules[i].last_triggered_ts = 0;
        s_rules[i].last_triggered_mono_us = 0;
        s_rules[i].current_state = false;
    }
    unlock_rules();
    ESP_LOGI(TAG, "All rule safety lockouts, cooldowns, and states reset.");
}
