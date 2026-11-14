#include "scheduler.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "supervisor.h"
#include "rtc.h"
#include "rule_engine.h"
#include "mqtt_adapter.h"
#include "sd_logger.h"

static const char *TAG = "SCHEDULER";

#define SCHEDULER_TASK_PRIO   4
#define SCHEDULER_TASK_CORE   0
#define SCHEDULER_STACK_SIZE  4096

static schedule_entry_t s_entries[MAX_SCHEDULE_ENTRIES];
static int s_entry_count = 0;
static SemaphoreHandle_t s_sched_mutex = NULL;
static int s_last_triggered_minute = -1;

static const char *s_day_names[7] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

static void lock_sched(void)
{
    if (s_sched_mutex != NULL) {
        xSemaphoreTake(s_sched_mutex, portMAX_DELAY);
    }
}

static void unlock_sched(void)
{
    if (s_sched_mutex != NULL) {
        xSemaphoreGive(s_sched_mutex);
    }
}

static void normalize_schedule_target(const char *target, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    if (!target || target[0] == '\0') {
        out[0] = '\0';
        return;
    }
    if (strcasecmp(target, "fan_01") == 0 || strcasecmp(target, "fan_relay_01") == 0) {
        strncpy(out, "relay_1", out_sz - 1);
    } else if (strcasecmp(target, "heater_01") == 0 || strcasecmp(target, "heater_relay_01") == 0) {
        strncpy(out, "relay_2", out_sz - 1);
    } else if (strcasecmp(target, "dehumidifier_01") == 0 || strcasecmp(target, "dehum_01") == 0) {
        strncpy(out, "relay_3", out_sz - 1);
    } else if (strcasecmp(target, "aux_01") == 0 ||
               strcasecmp(target, "light_01") == 0 ||
               strcasecmp(target, "relay_ch4") == 0 ||
               strcasecmp(target, "office_ac_relay") == 0) {
        strncpy(out, "relay_4", out_sz - 1);
    } else {
        strncpy(out, target, out_sz - 1);
    }
    out[out_sz - 1] = '\0';
}

static void format_repeat_days_str(const bool days[7], char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    size_t pos = 0;
    for (int d = 0; d < 7; d++) {
        if (!days[d]) continue;
        int n = snprintf(out + pos, out_sz - pos, "%s%s", (pos > 0) ? "," : "", s_day_names[d]);
        if (n < 0 || (size_t)n >= out_sz - pos) break;
        pos += (size_t)n;
    }
}

static void parse_repeat_days_str(const char *str, bool out_days[7])
{
    if (!str) return;
    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *saveptr = NULL;
    char *tok = strtok_r(buf, ",; ", &saveptr);
    while (tok != NULL) {
        for (int d = 0; d < 7; d++) {
            if (strcasecmp(tok, s_day_names[d]) == 0 ||
                (tok[0] == ('0' + d) && tok[1] == '\0')) {
                out_days[d] = true;
                break;
            }
        }
        tok = strtok_r(NULL, ",; ", &saveptr);
    }
}

static void parse_repeat_days_array(const cJSON *arr, bool out_days[7])
{
    if (!arr || !cJSON_IsArray(arr)) return;
    int n = cJSON_GetArraySize(arr);
    if (n == 0) return;

    cJSON *first = cJSON_GetArrayItem(arr, 0);
    if (n == 7 && first && cJSON_IsBool(first)) {
        for (int d = 0; d < 7; d++) {
            cJSON *el = cJSON_GetArrayItem(arr, d);
            out_days[d] = (el != NULL) && cJSON_IsTrue(el);
        }
        return;
    }

    for (int i = 0; i < n; i++) {
        cJSON *el = cJSON_GetArrayItem(arr, i);
        if (el && cJSON_IsNumber(el) && el->valueint >= 0 && el->valueint < 7) {
            out_days[el->valueint] = true;
        } else if (el && cJSON_IsString(el)) {
            parse_repeat_days_str(el->valuestring, out_days);
        }
    }
}

static void parse_repeat_days_json(const cJSON *item, bool out_days[7])
{
    memset(out_days, 0, sizeof(bool) * 7);
    const cJSON *j_rdi = cJSON_GetObjectItem(item, "repeat_days_indices");
    const cJSON *j_rd  = cJSON_GetObjectItem(item, "repeat_days");
    const cJSON *j_rdm = cJSON_GetObjectItem(item, "repeat_days_mask");

    if (j_rdi && cJSON_IsArray(j_rdi) && cJSON_GetArraySize(j_rdi) > 0) {
        parse_repeat_days_array(j_rdi, out_days);
    } else if (j_rd && cJSON_IsString(j_rd)) {
        parse_repeat_days_str(j_rd->valuestring, out_days);
    } else if (j_rd && cJSON_IsArray(j_rd)) {
        parse_repeat_days_array(j_rd, out_days);
    } else if (j_rdm && cJSON_IsArray(j_rdm) && cJSON_GetArraySize(j_rdm) == 7) {
        for (int d = 0; d < 7; d++) {
            cJSON *el = cJSON_GetArrayItem(j_rdm, d);
            out_days[d] = el && (cJSON_IsTrue(el) || (cJSON_IsNumber(el) && el->valueint != 0));
        }
    }
}

static bool parse_schedule_item(const cJSON *item, schedule_entry_t *out, int fallback_id)
{
    if (!item || !cJSON_IsObject(item) || !out) return false;

    const cJSON *j_hour = cJSON_GetObjectItem(item, "hour");
    const cJSON *j_min  = cJSON_GetObjectItem(item, "minute");
    if (!j_hour || !cJSON_IsNumber(j_hour) || !j_min || !cJSON_IsNumber(j_min)) {
        return false;
    }

    memset(out, 0, sizeof(schedule_entry_t));
    const cJSON *j_id = cJSON_GetObjectItem(item, "id");
    out->id = (j_id && cJSON_IsNumber(j_id)) ? j_id->valueint : fallback_id;

    const cJSON *j_type = cJSON_GetObjectItem(item, "type");
    if (j_type && cJSON_IsString(j_type) && j_type->valuestring[0] != '\0') {
        strncpy(out->type, j_type->valuestring, sizeof(out->type) - 1);
    } else {
        strncpy(out->type, "alarm", sizeof(out->type) - 1);
    }

    out->hour = j_hour->valueint;
    out->minute = j_min->valueint;
    if (out->hour < 0) out->hour = 0;
    if (out->hour > 23) out->hour = 23;
    if (out->minute < 0) out->minute = 0;
    if (out->minute > 59) out->minute = 59;

    parse_repeat_days_json(item, out->repeat_days);

    const cJSON *j_otd = cJSON_GetObjectItem(item, "one_time_date");
    if (j_otd && cJSON_IsString(j_otd) && j_otd->valuestring[0] != '\0') {
        strncpy(out->one_time_date, j_otd->valuestring, sizeof(out->one_time_date) - 1);
    }

    const cJSON *j_en = cJSON_GetObjectItem(item, "enabled");
    out->enabled = (j_en != NULL) ? cJSON_IsTrue(j_en) : true;

    const cJSON *j_tgt = cJSON_GetObjectItem(item, "target_device_id");
    if (j_tgt && cJSON_IsString(j_tgt)) {
        normalize_schedule_target(j_tgt->valuestring, out->target_device_id, sizeof(out->target_device_id));
    }

    const cJSON *j_act = cJSON_GetObjectItem(item, "action_command");
    if (j_act && cJSON_IsString(j_act)) {
        strncpy(out->action_command, j_act->valuestring, sizeof(out->action_command) - 1);
    }

    const cJSON *j_title = cJSON_GetObjectItem(item, "title");
    if (j_title && cJSON_IsString(j_title)) {
        strncpy(out->title, j_title->valuestring, sizeof(out->title) - 1);
    }

    const cJSON *j_buz = cJSON_GetObjectItem(item, "buzzer");
    out->buzzer = (j_buz != NULL) ? cJSON_IsTrue(j_buz) : (strcmp(out->type, "automation") != 0);

    return true;
}

static cJSON *build_schedules_json_array_locked(void)
{
    cJSON *arr = cJSON_CreateArray();
    if (!arr) return NULL;

    for (int i = 0; i < s_entry_count; i++) {
        const schedule_entry_t *e = &s_entries[i];
        cJSON *item = cJSON_CreateObject();
        if (!item) continue;

        char rd_str[32];
        format_repeat_days_str(e->repeat_days, rd_str, sizeof(rd_str));

        int mask[7];
        for (int d = 0; d < 7; d++) {
            mask[d] = e->repeat_days[d] ? 1 : 0;
        }

        cJSON_AddNumberToObject(item, "id", e->id);
        cJSON_AddStringToObject(item, "type", e->type);
        cJSON_AddNumberToObject(item, "hour", e->hour);
        cJSON_AddNumberToObject(item, "minute", e->minute);
        cJSON_AddStringToObject(item, "repeat_days", rd_str);
        cJSON_AddItemToObject(item, "repeat_days_mask", cJSON_CreateIntArray(mask, 7));
        cJSON_AddStringToObject(item, "one_time_date", e->one_time_date);
        cJSON_AddBoolToObject(item, "enabled", e->enabled);
        cJSON_AddStringToObject(item, "target_device_id", e->target_device_id);
        cJSON_AddStringToObject(item, "action_command", e->action_command);
        cJSON_AddStringToObject(item, "title", e->title);
        cJSON_AddBoolToObject(item, "buzzer", e->buzzer);

        cJSON_AddItemToArray(arr, item);
    }
    return arr;
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

static void save_schedules_to_storage_locked(void)
{
    cJSON *arr = build_schedules_json_array_locked();
    if (!arr) return;

    char *str = cJSON_PrintUnformatted(arr);
    cJSON_Delete(arr);
    if (str == NULL) return;

    size_t len = strlen(str);
    bool sd_mounted = sd_logger_is_mounted();
    bool saved = false;

    if (sd_mounted) {
        if (write_file_atomic(SD_SCHEDULE_PATH, str, len)) {
            saved = true;
        } else {
            ESP_LOGW(TAG, "Failed to save schedules to %s", SD_SCHEDULE_PATH);
        }
    }

    if (write_file_atomic(SPIFFS_SCHEDULE_PATH, str, len)) {
        saved = true;
    } else {
        ESP_LOGW(TAG, "Failed to save schedules to %s", SPIFFS_SCHEDULE_PATH);
    }

    if (saved) {
        ESP_LOGI(TAG, "Saved %d schedules to %s", s_entry_count,
                 sd_mounted ? SD_SCHEDULE_PATH : SPIFFS_SCHEDULE_PATH);
    }

    cJSON_free(str);
}

static bool load_schedules_from_file_locked(const char *path)
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
        const cJSON *sched = cJSON_GetObjectItem(root, "schedules");
        if (sched && cJSON_IsArray(sched)) {
            arr = sched;
        }
    }

    if (arr == NULL) {
        cJSON_Delete(root);
        return false;
    }

    int count = cJSON_GetArraySize(arr);
    s_entry_count = 0;
    for (int i = 0; i < count && s_entry_count < MAX_SCHEDULE_ENTRIES; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        schedule_entry_t entry;
        if (parse_schedule_item(item, &entry, i + 1)) {
            s_entries[s_entry_count++] = entry;
        }
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "Loaded %d schedules from %s", s_entry_count, path);
    return true;
}

static bool load_schedules_from_storage_locked(void)
{
    if (sd_logger_is_mounted() && load_schedules_from_file_locked(SD_SCHEDULE_PATH)) {
        return true;
    }
    if (load_schedules_from_file_locked(SPIFFS_SCHEDULE_PATH)) {
        return true;
    }
    ESP_LOGI(TAG, "No valid schedule storage file found. Using defaults.");
    return false;
}

static void load_default_schedules_locked(void)
{
    s_entry_count = 0;

    // Entry 1: Daily Alarm
    s_entries[s_entry_count++] = (schedule_entry_t){
        .id = 1,
        .type = "alarm",
        .hour = 7,
        .minute = 0,
        .repeat_days = {true, true, true, true, true, true, true},
        .one_time_date = "",
        .enabled = true,
        .title = "Good Morning Alarm",
        .buzzer = true,
    };

    // Entry 2: Office Automation (Turn off AC at 18:00)
    s_entries[s_entry_count++] = (schedule_entry_t){
        .id = 2,
        .type = "automation",
        .hour = 18,
        .minute = 0,
        .repeat_days = {false, true, true, true, true, true, false},
        .one_time_date = "",
        .enabled = true,
        .target_device_id = "relay_4",
        .action_command = "OFF",
        .title = "Office Auto AC Off",
        .buzzer = false,
    };

    ESP_LOGI(TAG, "Loaded %d default schedule entries", s_entry_count);
}

void scheduler_reload(void)
{
    ESP_LOGI(TAG, "Reloading schedules from storage...");
    lock_sched();
    s_last_triggered_minute = -1; // Allow immediate re-check if schedule changes
    if (!load_schedules_from_storage_locked()) {
        load_default_schedules_locked();
        save_schedules_to_storage_locked();
    }
    unlock_sched();
}

bool scheduler_apply_cloud_json(const cJSON *schedules_arr)
{
    if (schedules_arr == NULL || !cJSON_IsArray(schedules_arr)) {
        return false;
    }

    lock_sched();
    int count = cJSON_GetArraySize(schedules_arr);
    s_entry_count = 0;
    for (int i = 0; i < count && s_entry_count < MAX_SCHEDULE_ENTRIES; i++) {
        cJSON *item = cJSON_GetArrayItem(schedules_arr, i);
        schedule_entry_t entry;
        if (parse_schedule_item(item, &entry, i + 1)) {
            s_entries[s_entry_count++] = entry;
        }
    }
    s_last_triggered_minute = -1;
    save_schedules_to_storage_locked();
    int applied = s_entry_count;
    unlock_sched();

    ESP_LOGI(TAG, "[CLOUD SYNC] Applied %d schedule(s) from Cloud and saved to storage", applied);
    return true;
}

void scheduler_publish_sync(void)
{
    if (!mqtt_adapter_is_connected()) {
        return;
    }

    lock_sched();
    cJSON *arr = build_schedules_json_array_locked();
    unlock_sched();

    if (!arr) return;

    cJSON *root = cJSON_CreateObject();
    if (!root) {
        cJSON_Delete(arr);
        return;
    }
    cJSON_AddItemToObject(root, "schedules", arr);

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (json_str != NULL) {
        bool ok = mqtt_adapter_publish("schedule/sync", json_str);
        ESP_LOGI(TAG, "[CLOUD SYNC] Published %s to schedule/sync (%d bytes)",
                 ok ? "OK" : "FAILED", (int)strlen(json_str));
        cJSON_free(json_str);
    }
}

bool scheduler_add_entry(const schedule_entry_t *entry)
{
    if (entry == NULL) return false;

    schedule_entry_t norm = *entry;
    if (norm.target_device_id[0] != '\0') {
        normalize_schedule_target(entry->target_device_id, norm.target_device_id, sizeof(norm.target_device_id));
    }

    lock_sched();
    for (int i = 0; i < s_entry_count; i++) {
        if (s_entries[i].id == norm.id) {
            s_entries[i] = norm;
            save_schedules_to_storage_locked();
            unlock_sched();
            ESP_LOGI(TAG, "Updated existing schedule #%d ('%s')", norm.id, norm.title);
            scheduler_publish_sync();
            return true;
        }
    }

    if (s_entry_count >= MAX_SCHEDULE_ENTRIES) {
        unlock_sched();
        ESP_LOGW(TAG, "Schedule table full (max %d)", MAX_SCHEDULE_ENTRIES);
        return false;
    }

    s_entries[s_entry_count++] = norm;
    save_schedules_to_storage_locked();
    unlock_sched();
    ESP_LOGI(TAG, "Added schedule #%d: [%s] %02d:%02d ('%s')",
             norm.id, norm.type, norm.hour, norm.minute, norm.title);
    scheduler_publish_sync();
    return true;
}

bool scheduler_delete_entry(int entry_id)
{
    lock_sched();
    int idx = -1;
    for (int i = 0; i < s_entry_count; i++) {
        if (s_entries[i].id == entry_id) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        unlock_sched();
        return false;
    }

    for (int i = idx; i < s_entry_count - 1; i++) {
        s_entries[i] = s_entries[i + 1];
    }
    s_entry_count--;
    save_schedules_to_storage_locked();
    unlock_sched();
    ESP_LOGI(TAG, "Deleted schedule #%d", entry_id);
    scheduler_publish_sync();
    return true;
}

void scheduler_clear_entries(void)
{
    lock_sched();
    s_entry_count = 0;
    save_schedules_to_storage_locked();
    unlock_sched();
    ESP_LOGI(TAG, "Cleared all schedules");
    scheduler_publish_sync();
}

int scheduler_get_entry_count(void)
{
    lock_sched();
    int cnt = s_entry_count;
    unlock_sched();
    return cnt;
}

int scheduler_get_entries(schedule_entry_t *dest, int max_entries)
{
    if (dest == NULL || max_entries <= 0) return 0;
    lock_sched();
    int count = (s_entry_count < max_entries) ? s_entry_count : max_entries;
    for (int i = 0; i < count; i++) {
        dest[i] = s_entries[i];
    }
    unlock_sched();
    return count;
}

void scheduler_trigger_alarm(const char *title, bool buzzer)
{
    alarm_event_t *alarm = (alarm_event_t *)malloc(sizeof(alarm_event_t));
    if (alarm != NULL) {
        alarm->id = 999;
        strncpy(alarm->title, title ? title : "Manual Alarm Test", sizeof(alarm->title) - 1);
        alarm->title[sizeof(alarm->title) - 1] = '\0';
        alarm->buzzer = buzzer;

        time_t now = app_rtc_get_time();
        struct tm t;
        localtime_r(&now, &t);
        alarm->hour = t.tm_hour;
        alarm->minute = t.tm_min;

        event_bus_post(EVT_SCHEDULE_ALARM_TRIGGERED, alarm, sizeof(alarm_event_t), false);
    }
}

static void on_alarm_triggered(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL) {
        const alarm_event_t *alarm = (const alarm_event_t *)evt->data;
        printf("\n=======================================================\n");
        printf("   🔔 [ALARM NOTIFICATION TRIGGERED] 🔔\n");
        printf("   Title : %s\n", alarm->title);
        printf("   Time  : %02d:%02d\n", alarm->hour, alarm->minute);
        printf("   Buzzer: %s\n", alarm->buzzer ? "BEEPING (ACTIVE)" : "SILENT");
        printf("=======================================================\n\n");
        fflush(stdout);

        if (mqtt_adapter_is_connected()) {
            char payload[160];
            snprintf(payload, sizeof(payload),
                     "{\"alarm_id\":%d,\"title\":\"%s\",\"time\":\"%02d:%02d\",\"buzzer\":%s}",
                     alarm->id, alarm->title, alarm->hour, alarm->minute,
                     alarm->buzzer ? "true" : "false");
            mqtt_adapter_publish("alarm", payload);
        }
    }
}

static void scheduler_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Scheduler task started on Core %d", xPortGetCoreID());

    while (1) {
        supervisor_report_heartbeat(TASK_HEARTBEAT_SCHEDULER);

        time_t now = app_rtc_get_time();
        struct tm t;
        localtime_r(&now, &t);

        // Only evaluate if real time has been synchronized (FSD §4.2 / FR-RTC-01)
        if (rtc_is_valid()) {
            int cur_minute = t.tm_hour * 60 + t.tm_min;

            if (cur_minute != s_last_triggered_minute) {
                s_last_triggered_minute = cur_minute;

                char cur_date[32] = {0};
                strftime(cur_date, sizeof(cur_date), "%Y-%m-%d", &t);

                lock_sched();
                for (int i = 0; i < s_entry_count; i++) {
                    schedule_entry_t *entry = &s_entries[i];
                    if (!entry->enabled) continue;

                    if (entry->hour != t.tm_hour || entry->minute != t.tm_min) {
                        continue;
                    }

                    bool matches = false;
                    if (strlen(entry->one_time_date) > 0) {
                        if (strcmp(entry->one_time_date, cur_date) == 0) {
                            matches = true;
                        }
                    } else {
                        if (entry->repeat_days[t.tm_wday]) {
                            matches = true;
                        }
                    }

                    if (matches) {
                        if (strcmp(entry->type, "automation") == 0) {
                            automation_rule_t *rule = (automation_rule_t *)malloc(sizeof(automation_rule_t));
                            if (rule != NULL) {
                                rule->id = entry->id;
                                strncpy(rule->target_device_id, entry->target_device_id, sizeof(rule->target_device_id) - 1);
                                rule->target_device_id[sizeof(rule->target_device_id) - 1] = '\0';
                                strncpy(rule->action_command, entry->action_command, sizeof(rule->action_command) - 1);
                                rule->action_command[sizeof(rule->action_command) - 1] = '\0';
                                event_bus_post(EVT_SCHEDULE_AUTOMATION_TRIGGERED, rule, sizeof(automation_rule_t), false);
                            }
                        } else {
                            alarm_event_t *alarm = (alarm_event_t *)malloc(sizeof(alarm_event_t));
                            if (alarm != NULL) {
                                alarm->id = entry->id;
                                strncpy(alarm->title, entry->title, sizeof(alarm->title) - 1);
                                alarm->title[sizeof(alarm->title) - 1] = '\0';
                                alarm->buzzer = entry->buzzer;
                                alarm->hour = entry->hour;
                                alarm->minute = entry->minute;
                                event_bus_post(EVT_SCHEDULE_ALARM_TRIGGERED, alarm, sizeof(alarm_event_t), false);
                            }
                        }
                    }
                }
                unlock_sched();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

static void on_mqtt_connected(const gateway_event_t *evt)
{
    (void)evt;
    ESP_LOGI(TAG, "MQTT connected; ready for Cloud schedule sync");
}

void scheduler_init(void)
{
    if (s_sched_mutex == NULL) {
        s_sched_mutex = xSemaphoreCreateMutex();
    }

    lock_sched();
    if (!load_schedules_from_storage_locked()) {
        load_default_schedules_locked();
        save_schedules_to_storage_locked();
    }
    unlock_sched();

    event_bus_register_handler(EVT_SCHEDULE_ALARM_TRIGGERED, on_alarm_triggered);
    event_bus_register_handler(EVT_MQTT_CONNECTED, on_mqtt_connected);

    xTaskCreatePinnedToCore(scheduler_task,
                            "scheduler_task",
                            SCHEDULER_STACK_SIZE,
                            NULL,
                            SCHEDULER_TASK_PRIO,
                            NULL,
                            SCHEDULER_TASK_CORE);

    ESP_LOGI(TAG, "Scheduler initialized with %d entries", s_entry_count);
}
