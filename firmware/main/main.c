/**
 * @file main.c
 * @brief ESP32-S3 Smart Clock Gateway Main Entry Point
 *
 * Implements the standardized 16-step Boot Sequence according to FSD §4.14:
 * 1. nvs_flash_init()
 * 2. relay_controller_init() (hardware fail-safe GPIO 15..18 LOW)
 * 3. event_bus_init() (32-slot queue)
 * 4. supervisor_init() (active mask 0x7F)
 * 5. app_rtc_init() (DS3231 & TZ ICT-7)
 * 6. display_init() (KPI-BOOT-01 < 1.5s clock display)
 * 7. storage_init() (mount /spiffs)
 * 8. sd_logger_init() (mount /sdcard & sync_queue.bin)
 * 9. config_manager_init() (load /spiffs/config.json)
 * 10. wifi_provisioning_init() (SoftAP portal & web server)
 * 11. uplink_init() (MQTT client Core 1 QoS 1)
 * 12. rule_engine_init() (load /spiffs/rules.json & Hysteresis)
 * 13. scheduler_init() (schedule.json scanner Core 0 Prio 4)
 * 14. ble_adapter_init() (NimBLE stack Core 0 Prio 5)
 * 15. serial_cli_init() (UART0 rescue console Core 1 Prio 2)
 * 16. event_bus_start_dispatcher() (Dispatcher Core 0 Prio 6 - ALWAYS LAST)
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
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
#include "ble_adapter.h"
#include "relay_controller.h"
#include "display.h"
#include "storage.h"
#include "sd_logger.h"
#include "board_led.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    // Tắt ngay lập tức LED RGB trên bo mạch khi vừa cấp nguồn
    board_led_init();

    // Chờ 1.5s để USB-UART trên Host ổn định enumeration
    vTaskDelay(pdMS_TO_TICKS(1500));

    ESP_LOGI(TAG, "==================================================");
    ESP_LOGI(TAG, "[BOOT] Starting Smart Clock Edge Gateway");
    ESP_LOGI(TAG, "Hardware: ESP32-S3 Bare-board (/dev/ttyACM0 / COM7)");
    ESP_LOGI(TAG, "Firmware: Complete FSD Ecosystem (Relays + Display + Storage + SD)");
    ESP_LOGI(TAG, "==================================================");

    // 1. NVS Flash: Tham số Wi-Fi, System, Timezone (FSD §4.14 Step 1)
    ESP_LOGI(TAG, "[BOOT 1/16] Initializing NVS Flash...");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Hardware Safety GPIO Fail-Safe: Kéo GPIO 15..18 LOW an toàn trước khi Event Bus chạy (FSD §4.14 Step 2)
    ESP_LOGI(TAG, "[BOOT 2/16] Hardware Safety Fail-Safe (GPIO 15..18 LOW, 10k pulldown)...");
    relay_gpio_init();

    // 3. Event Bus: Khởi tạo Event Queue trung tâm (32 slots) (FSD §4.14 Step 3)
    ESP_LOGI(TAG, "[BOOT 3/16] Initializing Central Event Bus Queue (32 slots)...");
    event_bus_init();

    // 4. Relay Controller: Đăng ký Event Handlers và Khởi tạo Safety Timer (FSD §4.14 Step 4)
    ESP_LOGI(TAG, "[BOOT 4/16] Initializing Relay Controller (Event Handlers & Interlock)...");
    relay_controller_init();

    // 5. Task Supervisor & Watchdog (FSD §4.14 Step 5)
    ESP_LOGI(TAG, "[BOOT 5/16] Initializing Task Supervisor (Active Mask 0x7F, TWDT 15s)...");
    supervisor_init();
    supervisor_set_active_tasks(TASK_ACTIVE_MASK_PRODUCTION);

    // 6. RTC & SNTP Clock (FSD §4.14 Step 6)
    ESP_LOGI(TAG, "[BOOT 6/16] Initializing RTC System Clock (DS3231 I2C0 & TZ ICT-7)...");
    app_rtc_init();

    // 7. OLED Display, Buzzer & Buttons: Đạt KPI-BOOT-01 < 1.5s (FSD §4.14 Step 7)
    ESP_LOGI(TAG, "[BOOT 7/16] Initializing Display Subsystem (OLED + Buzzer + Buttons)...");
    display_init();

    // 8. Storage: Gắn kết phân vùng SPIFFS /spiffs (FSD §4.14 Step 8)
    ESP_LOGI(TAG, "[BOOT 8/17] Mounting SPIFFS storage /spiffs...");
    storage_init();

    // 9. MicroSD Logger & Store-and-Forward Sync Queue (FSD §4.14 Step 9)
    ESP_LOGI(TAG, "[BOOT 9/17] Initializing MicroSD Logger & Sync Queue...");
    sd_logger_init();

    // 10. Configuration Manager: Nạp /spiffs/config.json (FSD §4.14 Step 10)
    ESP_LOGI(TAG, "[BOOT 10/17] Initializing Config Manager (loading config.json)...");
    config_manager_init();

    // 11. Wi-Fi Provisioning & Connection (FSD §4.14 Step 11)
    ESP_LOGI(TAG, "[BOOT 11/17] Initializing Wi-Fi & Web Provisioning Server...");
    wifi_provisioning_init();

    // 12. Uplink Telemetry & MQTT Adapter (FSD §4.14 Step 12)
    ESP_LOGI(TAG, "[BOOT 12/17] Initializing MQTT Adapter & Uplink Telemetry...");
    uplink_init();

    // 13. Rule Engine Condition Evaluator (FSD §4.14 Step 13)
    ESP_LOGI(TAG, "[BOOT 13/17] Initializing Rule Engine (loading rules.json & Hysteresis)...");
    rule_engine_init();

    // 14. Local Scheduler & Real-Time Alarm (FSD §4.14 Step 14)
    ESP_LOGI(TAG, "[BOOT 14/17] Initializing Local Scheduler & Alarms...");
    scheduler_init();

    // 15. BLE Wireless Telemetry Adapter (FSD §4.14 Step 15)
    ESP_LOGI(TAG, "[BOOT 15/17] Initializing BLE Adapter & NimBLE Stack...");
    ble_adapter_init();

    // 16. Serial CLI Console (FSD §4.14 Step 16)
    ESP_LOGI(TAG, "[BOOT 16/17] Starting Rescue Serial CLI Console...");
    serial_cli_init();

    // 17. Bắt đầu Dispatcher sau cùng khi tất cả module đã đăng ký handler (FSD §4.14 Step 17)
    ESP_LOGI(TAG, "[BOOT 17/17] Starting Event Bus Dispatcher (Core 0, Priority 6)...");
    event_bus_start_dispatcher();

    // Phát tín hiệu sống Journey Test JRN-01
    printf("\n[SYS] Alive marker 1\n");
    printf("[SYS] System fully operational with all 7 production tasks active (Mask 0x7F).\n\n");
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
