/**
 * @file rule_engine.c
 * @brief Rule Engine Condition Evaluator with Hysteresis & Edge-Triggering (FSD §4.7)
 */

#include "rule_engine.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "mqtt_adapter.h"
#include "sd_logger.h"

static const char *TAG = "RULE_ENGINE";

static automation_rule_t s_rules[MAX_RULES];
static int s_rule_count = 0;
static SemaphoreHandle_t s_rule_mutex = NULL;
static int64_t s_manual_override_until_us[4] = {0};

static int target_to_relay_index(const char *target_id)
{
    if (!target_id) return -1;
    if (strcasecmp(target_id, "relay_1") == 0 ||
        strcasecmp(target_id, "fan_01") == 0 ||
        strcasecmp(target_id, "fan_relay_01") == 0 ||
        strcasecmp(target_id, "fan") == 0) {
        return 0;
    }
    if (strcasecmp(target_id, "relay_2") == 0 ||
        strcasecmp(target_id, "heater_01") == 0 ||
        strcasecmp(target_id, "heater_relay_01") == 0 ||
        strcasecmp(target_id, "heater") == 0) {
        return 1;
    }
    if (strcasecmp(target_id, "relay_3") == 0 ||
        strcasecmp(target_id, "dehum_01") == 0 ||
        strcasecmp(target_id, "dehumidifier_01") == 0 ||
        strcasecmp(target_id, "dehum") == 0 ||
        strcasecmp(target_id, "dehumidifier") == 0) {
        return 2;
    }
    if (strcasecmp(target_id, "relay_4") == 0 ||
        strcasecmp(target_id, "aux_01") == 0 ||
        strcasecmp(target_id, "light_01") == 0 ||
        strcasecmp(target_id, "office_ac_relay") == 0 ||
        strcasecmp(target_id, "relay_ch4") == 0 ||
        strcasecmp(target_id, "aux") == 0 ||
        strcasecmp(target_id, "light") == 0) {
        return 3;
    }
    return -1;
}

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

static void normalize_rule_target(const char *target, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    if (!target || target[0] == '\0') {
        out[0] = '\0';
        return;
    }
    if (strcasecmp(target, "fan_01") == 0 ||
        strcasecmp(target, "fan_relay_01") == 0 ||
        strcasecmp(target, "fan") == 0) {
        strncpy(out, "relay_1", out_sz - 1);
    } else if (strcasecmp(target, "heater_01") == 0 ||
               strcasecmp(target, "heater_relay_01") == 0 ||
               strcasecmp(target, "heater") == 0) {
        strncpy(out, "relay_2", out_sz - 1);
    } else if (strcasecmp(target, "dehumidifier_01") == 0 ||
               strcasecmp(target, "dehum_01") == 0 ||
               strcasecmp(target, "dehumidifier") == 0 ||
               strcasecmp(target, "dehum") == 0) {
        strncpy(out, "relay_3", out_sz - 1);
    } else if (strcasecmp(target, "aux_01") == 0 ||
               strcasecmp(target, "light_01") == 0 ||
               strcasecmp(target, "relay_ch4") == 0 ||
               strcasecmp(target, "office_ac_relay") == 0 ||
               strcasecmp(target, "aux") == 0 ||
               strcasecmp(target, "light") == 0) {
        strncpy(out, "relay_4", out_sz - 1);
    } else {
        strncpy(out, target, out_sz - 1);
    }
    out[out_sz - 1] = '\0';
}

static bool file_matches_content(const char *path, const char *content, size_t len)
{
    struct stat st;
    if (stat(path, &st) != 0 || (size_t)st.st_size != len) {
        return false;
    }
    FILE *f = fopen(path, "r");
    if (!f) return false;
    char *buf = (char *)malloc(len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t rd = fread(buf, 1, len, f);
    fclose(f);
    bool same = (rd == len && memcmp(buf, content, len) == 0);
    free(buf);
    return same;
}

static bool write_file_atomic(const char *path, const char *content, size_t len)
{
    if (file_matches_content(path, content, len)) {
        return true;
    }

    char tmp_path[96];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
    FILE *f = fopen(tmp_path, "w");
    if (f != NULL) {
        size_t wr = fwrite(content, 1, len, f);
        fclose(f);
        if (wr == len) {
            unlink(path);
            if (rename(tmp_path, path) == 0) {
                return true;
            }
        }
        unlink(tmp_path);
    }

    f = fopen(path, "w");
    if (!f) return false;
    size_t wr = fwrite(content, 1, len, f);
    fclose(f);
    return (wr == len);
}

static cJSON *build_rules_json_array_locked(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;

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
    return arr;
}

static void save_rules_to_storage(void)
{
    cJSON *arr = build_rules_json_array_locked();
    if (!arr) return;

    char *str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);

    if (str != NULL) {
        size_t len = strlen(str);
        bool sd_mounted = sd_logger_is_mounted();
        bool saved = false;

        if (sd_mounted) {
            if (write_file_atomic(SD_RULES_STORAGE_PATH, str, len)) {
                saved = true;
            } else {
                ESP_LOGW(TAG, "Failed to save rules to %s", SD_RULES_STORAGE_PATH);
            }
        }

        if (write_file_atomic(SPIFFS_RULES_STORAGE_PATH, str, len)) {
            saved = true;
        } else {
            ESP_LOGW(TAG, "Failed to save rules to %s", SPIFFS_RULES_STORAGE_PATH);
        }

        if (saved) {
            ESP_LOGI(TAG, "Saved %d rules to %s", s_rule_count,
                     sd_mounted ? SD_RULES_STORAGE_PATH : SPIFFS_RULES_STORAGE_PATH);
        }
        cJSON_free(str);
    }
}

static bool parse_rule_item(const cJSON *item, automation_rule_t *r)
{
    if (!item || !cJSON_IsObject(item) || !r) return false;

    const cJSON *id = cJSON_GetObjectItem(item, "id");
    const cJSON *sensor = cJSON_GetObjectItem(item, "sensor_device_id");
    if (!sensor) sensor = cJSON_GetObjectItem(item, "device_id");

    const cJSON *cond = cJSON_GetObjectItem(item, "condition_type");
    if (!cond) cond = cJSON_GetObjectItem(item, "condition");

    const cJSON *thresh = cJSON_GetObjectItem(item, "threshold");
    const cJSON *hyst = cJSON_GetObjectItem(item, "hysteresis");
    const cJSON *cool = cJSON_GetObjectItem(item, "cooldown_s");

    const cJSON *target = cJSON_GetObjectItem(item, "target_device_id");
    if (!target) target = cJSON_GetObjectItem(item, "target");

    const cJSON *act = cJSON_GetObjectItem(item, "action_command");
    if (!act) act = cJSON_GetObjectItem(item, "action");

    const cJSON *en = cJSON_GetObjectItem(item, "enabled");
    if (!en) en = cJSON_GetObjectItem(item, "is_active");

    if (!id || !cJSON_IsNumber(id) ||
        !sensor || !cJSON_IsString(sensor) ||
        !cond ||
        !thresh || !cJSON_IsNumber(thresh) ||
        !target || !cJSON_IsString(target) ||
        !act || !cJSON_IsString(act)) {
        return false;
    }

    memset(r, 0, sizeof(automation_rule_t));
    r->id = id->valueint;
    strncpy(r->device_id, sensor->valuestring, sizeof(r->device_id) - 1);

    if (cJSON_IsString(cond)) {
        if (strcasecmp(cond->valuestring, "lt") == 0 || strcmp(cond->valuestring, "<") == 0) {
            r->condition_type = RULE_CONDITION_SENSOR_LT;
        } else {
            r->condition_type = RULE_CONDITION_SENSOR_GT;
        }
    } else if (cJSON_IsNumber(cond)) {
        r->condition_type = (rule_condition_type_t)cond->valueint;
    }

    r->threshold = (float)thresh->valuedouble;
    float default_hyst = (strstr(r->device_id, "hum") != NULL) ? 2.0f : 1.5f;
    r->hysteresis = (hyst && cJSON_IsNumber(hyst) && hyst->valuedouble > 0) ? (float)hyst->valuedouble : default_hyst;
    r->cooldown_s = (cool && cJSON_IsNumber(cool) && cool->valueint > 0) ? (uint32_t)cool->valueint : 60;

    normalize_rule_target(target->valuestring, r->target_device_id, sizeof(r->target_device_id));
    strncpy(r->action_command, act->valuestring, sizeof(r->action_command) - 1);
    r->is_active = (en != NULL) ? cJSON_IsTrue(en) : true;
    return true;
}

static bool load_rules_from_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0 || sz > 16384) {
        fclose(f);
        return false;
    }

    char *buf = (char *)malloc((size_t)sz + 1);
    if (buf == NULL) {
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[read_bytes] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (root == NULL) {
        return false;
    }

    const cJSON *arr = NULL;
    if (cJSON_IsArray(root)) {
        arr = root;
    } else if (cJSON_IsObject(root)) {
        const cJSON *rules = cJSON_GetObjectItem(root, "rules");
        if (rules && cJSON_IsArray(rules)) {
            arr = rules;
        }
    }

    if (arr == NULL) {
        cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(arr);
    s_rule_count = 0;
    for (int i = 0; i < count && s_rule_count < MAX_RULES; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        automation_rule_t r;
        if (parse_rule_item(item, &r)) {
            s_rules[s_rule_count++] = r;
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d rules from storage %s", s_rule_count, path);
    return true;
}

static bool load_rules_from_storage(void)
{
    if (sd_logger_is_mounted() && load_rules_from_file(SD_RULES_STORAGE_PATH)) {
        return true;
    }
    if (load_rules_from_file(SPIFFS_RULES_STORAGE_PATH)) {
        return true;
    }
    ESP_LOGI(TAG, "Rules storage file not found. Using defaults.");
    return false;
}

void rule_engine_reload_rules(void)
{
    ESP_LOGI(TAG, "Reloading rules from storage...");
    lock_rules();
    if (!load_rules_from_storage()) {
        load_default_rules();
        save_rules_to_storage();
    }
    unlock_rules();
}

bool rule_engine_apply_cloud_json(const cJSON *rules_arr)
{
    if (rules_arr == NULL || !cJSON_IsArray(rules_arr)) {
        return false;
    }

    lock_rules();
    int prev_count = s_rule_count;
    automation_rule_t *prev_rules = NULL;
    if (prev_count > 0) {
        prev_rules = (automation_rule_t *)malloc(sizeof(automation_rule_t) * (size_t)prev_count);
        if (prev_rules != NULL) {
            memcpy(prev_rules, s_rules, sizeof(automation_rule_t) * (size_t)prev_count);
        } else {
            prev_count = 0;
        }
    }

    int count = cJSON_GetArraySize(rules_arr);
    s_rule_count = 0;
    for (int i = 0; i < count && s_rule_count < MAX_RULES; i++) {
        cJSON *item = cJSON_GetArrayItem(rules_arr, i);
        automation_rule_t r;
        if (!parse_rule_item(item, &r)) {
            continue;
        }

        const cJSON *hyst = cJSON_GetObjectItem(item, "hysteresis");
        const cJSON *cool = cJSON_GetObjectItem(item, "cooldown_s");
        bool has_hyst = (hyst && cJSON_IsNumber(hyst) && hyst->valuedouble > 0);
        bool has_cool = (cool && cJSON_IsNumber(cool) && cool->valueint > 0);

        for (int p = 0; p < prev_count; p++) {
            if (prev_rules[p].id == r.id) {
                if (!has_hyst && prev_rules[p].hysteresis > 0.0f) {
                    r.hysteresis = prev_rules[p].hysteresis;
                }
                if (!has_cool && prev_rules[p].cooldown_s > 0) {
                    r.cooldown_s = prev_rules[p].cooldown_s;
                }
                if (strcmp(prev_rules[p].device_id, r.device_id) == 0 &&
                    strcmp(prev_rules[p].target_device_id, r.target_device_id) == 0 &&
                    prev_rules[p].condition_type == r.condition_type) {
                    r.current_state = prev_rules[p].current_state;
                    r.last_triggered_mono_us = prev_rules[p].last_triggered_mono_us;
                    r.lockout_until_mono_us = prev_rules[p].lockout_until_mono_us;
                    r.last_triggered_ts = prev_rules[p].last_triggered_ts;
                    r.lockout_until_ts = prev_rules[p].lockout_until_ts;
                }
                break;
            }
        }

        s_rules[s_rule_count++] = r;
    }

    if (prev_rules != NULL) {
        free(prev_rules);
    }

    save_rules_to_storage();
    int applied = s_rule_count;
    unlock_rules();

    ESP_LOGI(TAG, "[CLOUD SYNC] Applied %d rule(s) from Cloud and saved to storage", applied);
    return true;
}

void rule_engine_publish_sync(void)
{
    if (!mqtt_adapter_is_connected()) {
        return;
    }

    lock_rules();
    cJSON *arr = build_rules_json_array_locked();
    unlock_rules();

    if (!arr) return;

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(arr);
        return;
    }
    cJSON_AddItemToObject(root, "rules", arr);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str != NULL) {
        bool ok = mqtt_adapter_publish("rules/sync", json_str);
        ESP_LOGI(TAG, "[CLOUD SYNC] Published %s to rules/sync (%d bytes)",
                 ok ? "OK" : "FAILED", (int)strlen(json_str));
        cJSON_free(json_str);
    }
}

bool rule_engine_add_rule(const automation_rule_t *rule)
{
    if (rule == NULL) return false;

    automation_rule_t norm = *rule;
    normalize_rule_target(rule->target_device_id, norm.target_device_id, sizeof(norm.target_device_id));
    if (norm.hysteresis <= 0.0f) {
        norm.hysteresis = (strstr(norm.device_id, "hum") != NULL) ? 2.0f : 1.5f;
    }
    if (norm.cooldown_s == 0) {
        norm.cooldown_s = 60;
    }

    lock_rules();
    // Check if rule ID already exists, update it
    for (int i = 0; i < s_rule_count; i++) {
        if (s_rules[i].id == norm.id) {
            s_rules[i] = norm;
            save_rules_to_storage();
            unlock_rules();
            ESP_LOGI(TAG, "Updated existing rule #%d (dev='%s')", norm.id, norm.device_id);
            rule_engine_publish_sync();
            return true;
        }
    }

    if (s_rule_count >= MAX_RULES) {
        unlock_rules();
        ESP_LOGW(TAG, "Rule table full (max %d)", MAX_RULES);
        return false;
    }

    s_rules[s_rule_count] = norm;
    s_rules[s_rule_count].current_state = false;
    s_rules[s_rule_count].last_triggered_ts = 0;
    s_rules[s_rule_count].lockout_until_ts = 0;
    s_rules[s_rule_count].last_triggered_mono_us = 0;
    s_rules[s_rule_count].lockout_until_mono_us = 0;
    s_rule_count++;

    save_rules_to_storage();
    unlock_rules();
    ESP_LOGI(TAG, "Added new rule #%d (dev='%s', thr=%.1f hyst=%.1f cool=%lu -> target='%s' cmd='%s')",
             norm.id, norm.device_id, norm.threshold, norm.hysteresis,
             (unsigned long)norm.cooldown_s, norm.target_device_id, norm.action_command);
    rule_engine_publish_sync();
    return true;
}

void rule_engine_clear_rules(void)
{
    lock_rules();
    s_rule_count = 0;
    save_rules_to_storage();
    unlock_rules();
    ESP_LOGI(TAG, "Cleared all rules");
    rule_engine_publish_sync();
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
    rule_engine_publish_sync();
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

static void post_rule_action(const char *target, const char *cmd, int rule_id, const char *reason)
{
    rule_action_request_t *req = (rule_action_request_t *)malloc(sizeof(rule_action_request_t));
    if (req != NULL) {
        memset(req, 0, sizeof(rule_action_request_t));
        snprintf(req->command_id, sizeof(req->command_id), "cmd-rule-%d-%lu",
                 rule_id, (unsigned long)get_current_epoch());
        strncpy(req->target_device_id, target, sizeof(req->target_device_id) - 1);
        strncpy(req->action_command, cmd, sizeof(req->action_command) - 1);
        if (reason != NULL && reason[0] != '\0') {
            strncpy(req->reason, reason, sizeof(req->reason) - 1);
        }
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

        bool device_match = (strcmp(r->device_id, telem->device_id) == 0) ||
                            (strstr(r->device_id, "hum") != NULL &&
                             telem->humidity > 0.0f &&
                             strstr(telem->device_id, "temp") != NULL);
        if (!device_match) {
            continue;
        }

        // FSD §4.7.5: Check 5-minute safety lockout (Monotonic clock)
        if (now_mono < r->lockout_until_mono_us || (r->lockout_until_ts > 0 && now < r->lockout_until_ts)) {
            ESP_LOGD(TAG, "Rule #%d is in lockout, skipping", r->id);
            continue;
        }

        // Manual Override Priority (Human Always Wins over Automation for 30 minutes)
        int r_idx = target_to_relay_index(r->target_device_id);
        if (r_idx >= 0 && now_mono < s_manual_override_until_us[r_idx]) {
            ESP_LOGD(TAG, "Rule #%d skipped: relay_%d in manual override", r->id, r_idx + 1);
            continue;
        }

        float val = telem->value;
        if ((strstr(r->device_id, "hum") != NULL ||
             strstr(r->target_device_id, "relay_3") != NULL ||
             strstr(r->target_device_id, "dehum") != NULL) && telem->humidity > 0.0f) {
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
                        post_rule_action(r->target_device_id, cmd, r->id, "RULE_THRESHOLD_TRIGGER");
                        if (r_idx == 0 && strcasecmp(cmd, "ON") == 0) {
                            // Cooling Safety Override: relay_controller immediately turns OFF relay_2 (Heater).
                            // Clear current_state on any rule targeting relay_2 so this pass doesn't emit a duplicate OFF.
                            for (int j = 0; j < s_rule_count; j++) {
                                if (target_to_relay_index(s_rules[j].target_device_id) == 1) {
                                    s_rules[j].current_state = false;
                                }
                            }
                        }
                    }
                }
            } else if (val <= (r->threshold - hyst)) {
                // Edge Trigger: only turn off if currently active (bypass cooldown, 0ms delay)
                if (r->current_state) {
                    r->current_state = false;
                    matched_count++;
                    ESP_LOGI(TAG, ">>> [GT EDGE-DOWN] Rule #%d RELEASED! Sensor '%s' val=%.2f <= release=%.2f -> '%s' CMD='OFF'",
                             r->id, r->device_id, val, r->threshold - hyst, r->target_device_id);
                    post_rule_action(r->target_device_id, "OFF", r->id, "RULE_HYSTERESIS_RELEASE");
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
                        post_rule_action(r->target_device_id, cmd, r->id, "RULE_THRESHOLD_TRIGGER");
                    }
                }
            } else if (val >= (r->threshold + hyst)) {
                // Edge Trigger: only turn off if currently active (bypass cooldown, 0ms delay)
                if (r->current_state) {
                    r->current_state = false;
                    matched_count++;
                    ESP_LOGI(TAG, ">>> [LT EDGE-DOWN] Rule #%d RELEASED! Sensor '%s' val=%.2f >= release=%.2f -> '%s' CMD='OFF'",
                             r->id, r->device_id, val, r->threshold + hyst, r->target_device_id);
                    post_rule_action(r->target_device_id, "OFF", r->id, "RULE_HYSTERESIS_RELEASE");
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

        post_rule_action(rule->target_device_id, rule->action_command, rule->id, "SCHEDULE_AUTOMATION");
    }
}

static void on_rule_action_request(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(rule_action_request_t)) {
        const rule_action_request_t *act = (const rule_action_request_t *)evt->data;
        const char *reason = (act->reason[0] != '\0') ? act->reason :
                             ((act->source_rule_id > 0) ?
                                 ((strcasecmp(act->action_command, "OFF") == 0) ? "RULE_HYSTERESIS_RELEASE" : "RULE_THRESHOLD_TRIGGER")
                                 : "MANUAL_CMD");
        const char *status_str = (strcmp(reason, "RULE_HYSTERESIS_RELEASE") == 0) ? "RELEASED" : "TRIGGERED";

        ESP_LOGI(TAG, "[ACTION_EXECUTOR] >>> Action on Target='%s': Command='%s' (Rule #%d, Reason=%s)",
                 act->target_device_id, act->action_command, act->source_rule_id, reason);

        // Notify cloud via MQTT if available
        if (mqtt_adapter_is_connected()) {
            char topic[96];
            char payload[224];
            const char *cmd_id = (strncmp(act->command_id, "cmd-", 4) == 0) ? act->command_id : "cmd-webui";
            snprintf(topic, sizeof(topic), "device/%s/action", act->target_device_id);
            snprintf(payload, sizeof(payload),
                     "{\"command\":\"%s\",\"source_rule\":%d,\"status\":\"%s\",\"reason\":\"%s\",\"command_id\":\"%s\"}",
                     act->action_command, act->source_rule_id, status_str, reason, cmd_id);
            mqtt_adapter_publish(topic, payload);
        }
    }
}

static void on_cloud_config_updated(const gateway_event_t *evt)
{
    (void)evt;
    ESP_LOGD(TAG, "[CLOUD SYNC] Config updated notification received");
}

static void on_relay_state_changed(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(relay_state_changed_t)) {
        return;
    }

    const relay_state_changed_t *rst = (const relay_state_changed_t *)evt->data;
    uint32_t now = get_current_epoch();
    int64_t now_mono = esp_timer_get_time();
    int idx = (rst->relay_id >= 1 && rst->relay_id <= 4)
              ? (rst->relay_id - 1)
              : target_to_relay_index(rst->device_id);

    lock_rules();
    if (strcmp(rst->reason, "MANUAL_CMD") == 0) {
        if (rst->state) {
            if (idx >= 0 && idx < 4) {
                s_manual_override_until_us[idx] = now_mono + RULE_MANUAL_OVERRIDE_US;
            }
            for (int i = 0; i < s_rule_count; i++) {
                automation_rule_t *r = &s_rules[i];
                if (strcmp(r->target_device_id, rst->device_id) == 0 ||
                    (idx >= 0 && target_to_relay_index(r->target_device_id) == idx)) {
                    r->current_state = false;
                }
            }
        } else {
            bool any_rule_was_active = false;
            for (int i = 0; i < s_rule_count; i++) {
                automation_rule_t *r = &s_rules[i];
                if (strcmp(r->target_device_id, rst->device_id) == 0 ||
                    (idx >= 0 && target_to_relay_index(r->target_device_id) == idx)) {
                    if (r->current_state) {
                        any_rule_was_active = true;
                        r->current_state = false;
                    }
                }
            }
            if (idx >= 0 && idx < 4) {
                if (any_rule_was_active) {
                    s_manual_override_until_us[idx] = now_mono + RULE_MANUAL_OVERRIDE_US;
                } else {
                    s_manual_override_until_us[idx] = 0;
                }
            }
        }
    } else {
        if (idx >= 0 && idx < 4 &&
            (strcmp(rst->reason, "SAFETY_CUTOFF") == 0 ||
             strcmp(rst->reason, "INTERLOCK_CUTOFF") == 0 ||
             strcmp(rst->reason, "SENSOR_OFFLINE") == 0 ||
             strcmp(rst->reason, "EMERGENCY_SHUTDOWN") == 0 ||
             strcmp(rst->reason, "COOLING_OVERRIDE") == 0)) {
            s_manual_override_until_us[idx] = 0;
        }
        for (int i = 0; i < s_rule_count; i++) {
            automation_rule_t *r = &s_rules[i];
            if (strcmp(r->target_device_id, rst->device_id) == 0 ||
                (idx >= 0 && target_to_relay_index(r->target_device_id) == idx)) {
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
                post_rule_action(r->target_device_id, "OFF", r->id, "SENSOR_OFFLINE");
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
        save_rules_to_storage();
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
    memset(s_manual_override_until_us, 0, sizeof(s_manual_override_until_us));
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
