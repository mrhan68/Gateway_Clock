#include "scheduler.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "supervisor.h"
#include "rtc.h"
#include "rule_engine.h"
#include "mqtt_adapter.h"

static const char *TAG = "SCHEDULER";

#define SCHEDULER_TASK_PRIO   5
#define SCHEDULER_TASK_CORE   0
#define SCHEDULER_STACK_SIZE  4096

static schedule_entry_t s_entries[MAX_SCHEDULE_ENTRIES];
static int s_entry_count = 0;
static SemaphoreHandle_t s_sched_mutex = NULL;
static int s_last_triggered_minute = -1;

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

static void load_default_schedules(void)
{
    lock_sched();
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
        .target_device_id = "office_ac_relay",
        .action_command = "OFF",
        .title = "Office Auto AC Off",
        .buzzer = false,
    };

    unlock_sched();
    ESP_LOGI(TAG, "Loaded %d default schedule entries", s_entry_count);
}

void scheduler_reload(void)
{
    ESP_LOGI(TAG, "Reloading schedules from storage...");
    lock_sched();
    s_last_triggered_minute = -1; // Allow immediate re-check if schedule changes
    unlock_sched();
    load_default_schedules();
}

bool scheduler_add_entry(const schedule_entry_t *entry)
{
    if (entry == NULL) return false;

    lock_sched();
    for (int i = 0; i < s_entry_count; i++) {
        if (s_entries[i].id == entry->id) {
            s_entries[i] = *entry;
            unlock_sched();
            ESP_LOGI(TAG, "Updated existing schedule #%d ('%s')", entry->id, entry->title);
            return true;
        }
    }

    if (s_entry_count >= MAX_SCHEDULE_ENTRIES) {
        unlock_sched();
        ESP_LOGW(TAG, "Schedule table full (max %d)", MAX_SCHEDULE_ENTRIES);
        return false;
    }

    s_entries[s_entry_count++] = *entry;
    unlock_sched();
    ESP_LOGI(TAG, "Added schedule #%d: [%s] %02d:%02d ('%s')",
             entry->id, entry->type, entry->hour, entry->minute, entry->title);
    return true;
}

void scheduler_clear_entries(void)
{
    lock_sched();
    s_entry_count = 0;
    unlock_sched();
    ESP_LOGI(TAG, "Cleared all schedules");
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

        // Only evaluate if real time has been synchronized (year > 2020)
        if (t.tm_year >= (2020 - 1900)) {
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

void scheduler_init(void)
{
    if (s_sched_mutex == NULL) {
        s_sched_mutex = xSemaphoreCreateMutex();
    }

    load_default_schedules();

    event_bus_register_handler(EVT_SCHEDULE_ALARM_TRIGGERED, on_alarm_triggered);

    xTaskCreatePinnedToCore(scheduler_task,
                            "scheduler_task",
                            SCHEDULER_STACK_SIZE,
                            NULL,
                            SCHEDULER_TASK_PRIO,
                            NULL,
                            SCHEDULER_TASK_CORE);

    ESP_LOGI(TAG, "Scheduler initialized with %d entries", s_entry_count);
}
