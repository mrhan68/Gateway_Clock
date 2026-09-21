#include "supervisor.h"
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_attr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SUPERVISOR";

#define NUM_TRACKED_TASKS 7

RTC_NOINIT_ATTR static rtc_crash_dump_t s_crash_dump;

static uint32_t s_active_mask = TASK_ACTIVE_MASK_PRODUCTION;
static uint64_t s_last_heartbeats[NUM_TRACKED_TASKS];
static portMUX_TYPE s_supervisor_spinlock = portMUX_INITIALIZER_UNLOCKED;

void supervisor_set_active_tasks(uint32_t mask)
{
    portENTER_CRITICAL(&s_supervisor_spinlock);
    s_active_mask = mask;
    uint64_t now = esp_timer_get_time() / 1000ULL;
    for (int i = 0; i < NUM_TRACKED_TASKS; i++) {
        if (mask & (1 << i)) {
            s_last_heartbeats[i] = now;
        }
    }
    portEXIT_CRITICAL(&s_supervisor_spinlock);
    ESP_LOGI(TAG, "Active task monitoring mask updated: 0x%02X", (unsigned int)mask);
}

void supervisor_report_heartbeat(uint32_t task_mask)
{
    uint64_t now = esp_timer_get_time() / 1000ULL;
    portENTER_CRITICAL(&s_supervisor_spinlock);
    for (int i = 0; i < NUM_TRACKED_TASKS; i++) {
        if (task_mask & (1 << i)) {
            s_last_heartbeats[i] = now;
        }
    }
    portEXIT_CRITICAL(&s_supervisor_spinlock);
}

void supervisor_get_status(uint32_t *active_mask, uint32_t *alive_mask, uint32_t *free_heap)
{
    uint64_t now = esp_timer_get_time() / 1000ULL;
    uint32_t alive = 0;

    portENTER_CRITICAL(&s_supervisor_spinlock);
    if (active_mask) *active_mask = s_active_mask;
    for (int i = 0; i < NUM_TRACKED_TASKS; i++) {
        if ((s_active_mask & (1 << i)) && (now - s_last_heartbeats[i] <= SUPERVISOR_TIMEOUT_MS)) {
            alive |= (1 << i);
        }
    }
    if (alive_mask) *alive_mask = alive;
    portEXIT_CRITICAL(&s_supervisor_spinlock);

    if (free_heap) *free_heap = esp_get_free_heap_size();
}

static const char* get_task_name_by_index(int idx)
{
    switch (idx) {
        case 0: return "event_bus";
        case 1: return "ble_scan";
        case 2: return "uplink";
        case 3: return "sd_logger";
        case 4: return "scheduler";
        case 5: return "display";
        case 6: return "serial_cli";
        default: return "unknown";
    }
}

static void supervisor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Supervisor task running on Core %d (Prio %d)", xPortGetCoreID(), SUPERVISOR_TASK_PRIO);
    uint32_t loop_count = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        uint64_t now = esp_timer_get_time() / 1000ULL;
        loop_count++;

        // Kiểm tra xem có task nào bị treo quá thời gian quy định không (15s)
        for (int i = 0; i < NUM_TRACKED_TASKS; i++) {
            uint32_t bit = (1 << i);
            if (s_active_mask & bit) {
                uint64_t elapsed;
                portENTER_CRITICAL(&s_supervisor_spinlock);
                elapsed = now - s_last_heartbeats[i];
                portEXIT_CRITICAL(&s_supervisor_spinlock);

                if (elapsed > SUPERVISOR_TIMEOUT_MS) {
                    uint32_t heap = (uint32_t)esp_get_free_heap_size();
                    uint32_t uptime = (uint32_t)(esp_timer_get_time() / 1000000ULL);
                    time_t now_epoch = 0;
                    time(&now_epoch);

                    // Ghi chẩn đoán sự cố vào RTC Slow Memory trước khi khởi động lại
                    s_crash_dump.magic = 0xDEADBEEF;
                    s_crash_dump.failed_task_bit = (uint8_t)i;
                    strncpy(s_crash_dump.task_name, get_task_name_by_index(i), sizeof(s_crash_dump.task_name) - 1);
                    s_crash_dump.task_name[sizeof(s_crash_dump.task_name) - 1] = '\0';
                    s_crash_dump.free_heap = heap;
                    s_crash_dump.uptime_s = uptime;
                    s_crash_dump.timestamp = (uint32_t)now_epoch;

                    ESP_LOGE(TAG, "==================================================");
                    ESP_LOGE(TAG, "[SUP] CRITICAL: Task '%s' STALLED > 15s! Dumping to RTC Memory & restarting...",
                             s_crash_dump.task_name);
                    ESP_LOGE(TAG, "Failed task bit: %u, Free heap: %lu bytes, Uptime: %lu s",
                             (unsigned int)i, (unsigned long)heap, (unsigned long)uptime);
                    ESP_LOGE(TAG, "==================================================");
                    vTaskDelay(pdMS_TO_TICKS(500)); // Cho phép flush log serial ra console
                    esp_restart();
                }
            }
        }

        // Định kỳ 10 giây in một dòng log sức khỏe tổng thể
        if (loop_count % 10 == 0) {
            uint32_t active, alive, heap;
            supervisor_get_status(&active, &alive, &heap);
            ESP_LOGI(TAG, "[SYS HEALTH] Active: 0x%02X | Alive: 0x%02X | Free Heap: %lu B",
                     (unsigned int)active, (unsigned int)alive, (unsigned long)heap);
        }
    }
}

void supervisor_init(void)
{
    // Kiểm tra chẩn đoán crash từ lần reset trước trong RTC Slow Memory (FSD §4.13.2)
    if (s_crash_dump.magic == 0xDEADBEEF) {
        ESP_LOGW(TAG, "==================================================");
        ESP_LOGW(TAG, "[SUP] CRASH DUMP DETECTED FROM PREVIOUS RUN!");
        ESP_LOGW(TAG, "Failed Task Bit: %u (Task Name: '%s')",
                 s_crash_dump.failed_task_bit, s_crash_dump.task_name);
        ESP_LOGW(TAG, "Free Heap at stall: %lu bytes", (unsigned long)s_crash_dump.free_heap);
        ESP_LOGW(TAG, "Uptime before crash: %lu s", (unsigned long)s_crash_dump.uptime_s);
        ESP_LOGW(TAG, "Crash Timestamp: %lu (epoch)", (unsigned long)s_crash_dump.timestamp);
        ESP_LOGW(TAG, "==================================================");
        s_crash_dump.magic = 0; // Reset cờ sau khi đã ghi nhận
    }

    // Thiết lập active mask mặc định chuẩn Production (0x7F)
    s_active_mask = TASK_ACTIVE_MASK_PRODUCTION;
    uint64_t now = esp_timer_get_time() / 1000ULL;
    for (int i = 0; i < NUM_TRACKED_TASKS; i++) {
        s_last_heartbeats[i] = now;
    }

    xTaskCreatePinnedToCore(
        supervisor_task,
        "supervisor_task",
        3072,
        NULL,
        SUPERVISOR_TASK_PRIO,
        NULL,
        SUPERVISOR_TASK_CORE
    );

    ESP_LOGI(TAG, "Task supervisor initialized successfully (Active Mask: 0x%02X)", (unsigned int)s_active_mask);
}
