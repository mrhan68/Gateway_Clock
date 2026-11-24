#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#include "event_bus.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SCHEDULE_ENTRIES 16
#define SD_SCHEDULE_PATH     "/sdcard/schedule.json"
#define SPIFFS_SCHEDULE_PATH "/spiffs/schedule.json"

typedef struct {
    int id;
    char type[16];           // "automation" | "alarm" | "personal_event"
    int hour;                // 0-23
    int minute;              // 0-59
    bool repeat_days[7];     // [0=Sun, 1=Mon, ..., 6=Sat]
    char one_time_date[11];  // "YYYY-MM-DD" or empty ""
    bool enabled;

    // For automation:
    char target_device_id[32];
    char action_command[32];

    // For alarm / personal_event:
    char title[64];
    bool buzzer;
} schedule_entry_t;

typedef struct {
    int id;
    char title[64];
    bool buzzer;
    int hour;
    int minute;
} alarm_event_t;

/**
 * @brief Initialize Scheduler: creates scheduler_task and registers event handlers.
 */
void scheduler_init(void);

/**
 * @brief Reload schedules from storage / default config.
 */
void scheduler_reload(void);

/**
 * @brief Add or update a schedule entry.
 */
bool scheduler_add_entry(const schedule_entry_t *entry);

/**
 * @brief Delete a schedule entry by ID and sync to storage/cloud.
 */
bool scheduler_delete_entry(int entry_id);

/**
 * @brief Clear all schedule entries.
 */
void scheduler_clear_entries(void);

/**
 * @brief Get total number of schedule entries.
 */
int scheduler_get_entry_count(void);

/**
 * @brief Copy schedule entries into user-provided array.
 */
int scheduler_get_entries(schedule_entry_t *dest, int max_entries);

/**
 * @brief Compute and copy up to max_out upcoming enabled schedule entries sorted by time delta from ti.
 *        Uses minimal stack (no full schedule_entry_t arrays on stack).
 */
int scheduler_get_upcoming_entries(const struct tm *ti, schedule_entry_t *out_entries, int max_out);

/**
 * @brief Manually trigger an alarm event (for test / CLI verification).
 */
void scheduler_trigger_alarm(const char *title, bool buzzer);

/**
 * @brief Apply schedule array received from Cloud MQTT config/schedule without re-publishing sync.
 */
bool scheduler_apply_cloud_json(const cJSON *schedules_arr);

/**
 * @brief Publish current schedules to Cloud MQTT (gateway/<gw_id>/schedule/sync).
 */
void scheduler_publish_sync(void);

#ifdef __cplusplus
}
#endif

#endif // SCHEDULER_H
