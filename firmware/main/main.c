#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "event_bus.h"
#include "supervisor.h"
#include "config_manager.h"
#include "serial_cli.h"
#include "rtc.h"
#include "wifi_provisioning.h"
#include "uplink.h"
#include "rule_engine.h"
#include "scheduler.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // Doi 1.5 giay de USB-UART tren Host on dinh enumeration
    vTaskDelay(pdMS_TO_TICKS(1500));

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "[BOOT] Starting Smart Clock Edge Gateway");
    ESP_LOGI(TAG, "Hardware: ESP32-S3 Bare-board (/dev/ttyACM0 / COM7)");
    ESP_LOGI(TAG, "Firmware: Wave 1 - Core + Rule Engine + Scheduler");
    ESP_LOGI(TAG, "==================================================");

    // 1. NVS Flash & Configuration Manager
    ESP_LOGI(TAG, "[INIT 1/9] Initializing Config Manager & NVS Flash...");
    config_manager_init();

    // 2. Event Bus Queue
    ESP_LOGI(TAG, "[INIT 2/9] Initializing Event Bus Queue...");
    event_bus_init();

    // 3. Task Supervisor & Watchdog
    ESP_LOGI(TAG, "[INIT 3/9] Initializing Task Supervisor & Watchdog...");
    supervisor_init();

    // 4. RTC & SNTP Synchronization
    ESP_LOGI(TAG, "[INIT 4/9] Initializing RTC System Clock...");
    app_rtc_init();

    // 5. Uplink Telemetry & MQTT Adapter
    ESP_LOGI(TAG, "[INIT 5/9] Initializing MQTT Uplink...");
    uplink_init();

    // 6. Wi-Fi Provisioning & Connection
    ESP_LOGI(TAG, "[INIT 6/9] Initializing Wi-Fi & Provisioning...");
    wifi_provisioning_init();

    // 7. Rule Engine Condition Evaluator
    ESP_LOGI(TAG, "[INIT 7/9] Initializing Rule Engine...");
    rule_engine_init();

    // 8. Local Scheduler & Real-Time Alarm
    ESP_LOGI(TAG, "[INIT 8/9] Initializing Local Scheduler & Alarms...");
    scheduler_init();

    // Dang ky cac task can theo doi heartbeat
    supervisor_set_active_tasks(TASK_HEARTBEAT_EVENT_BUS |
                                TASK_HEARTBEAT_CLI |
                                TASK_HEARTBEAT_UPLINK |
                                TASK_HEARTBEAT_SCHEDULER);

    // 9. Serial CLI Console
    ESP_LOGI(TAG, "[INIT 9/9] Starting Serial CLI Console...");
    serial_cli_init();

    // Bat dau Dispatcher sau khi tat ca cac module da dang ky handler
    event_bus_start_dispatcher();

    // Phat tin hieu song Journey Test JRN-01
    printf("\n[SYS] Alive marker 1\n");
    printf("[SYS] System fully operational in bare-board network mode.\n\n");
    fflush(stdout);

    int marker_count = 1;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(3000));
        char time_str[32] = {0};
        app_rtc_get_formatted_time(time_str, sizeof(time_str));
        printf("[SYS] Alive marker %d (Time: %s, Free Heap: %lu B)\r\n",
               ++marker_count, time_str, (unsigned long)esp_get_free_heap_size());
        fflush(stdout);
    }
}
