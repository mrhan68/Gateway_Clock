#include "supervisor.h"
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "SUPERVISOR";

#define NUM_TRACKED_TASKS 8

static uint32_t s_active_mask = 0;
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
        case 0: return "EVENT_BUS";
        case 1: return "BLE_SCAN";
        case 2: return "UPLINK";
        case 3: return "SD_LOGGER";
        case 4: return "SCHEDULER";
        case 5: return "DISPLAY";
        case 6: return "SERIAL_CLI";
        case 7: return "MOCK_PERIPHERALS";
        default: return "UNKNOWN";
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

        // Kiểm tra xem có task nào bị treo quá thời gian quy định không
        for (int i = 0; i < NUM_TRACKED_TASKS; i++) {
            uint32_t bit = (1 << i);
            if (s_active_mask & bit) {
                uint64_t elapsed;
                portENTER_CRITICAL(&s_supervisor_spinlock);
                elapsed = now - s_last_heartbeats[i];
                portEXIT_CRITICAL(&s_supervisor_spinlock);

                if (elapsed > SUPERVISOR_TIMEOUT_MS) {
                    ESP_LOGE(TAG, "==================================================");
                    ESP_LOGE(TAG, "CRITICAL: Task '%s' STALLED for %llu ms (limit=%d ms)!",
                             get_task_name_by_index(i), elapsed, SUPERVISOR_TIMEOUT_MS);
                    ESP_LOGE(TAG, "System will restart to recover healthy state...");
                    ESP_LOGE(TAG, "Free heap: %lu bytes", (unsigned long)esp_get_free_heap_size());
                    ESP_LOGE(TAG, "==================================================");
                    vTaskDelay(pdMS_TO_TICKS(500)); // Đợi flush log serial
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

    ESP_LOGI(TAG, "Task supervisor initialized successfully");
}
