#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "event_bus.h"
#include "supervisor.h"
#include "rtc.h"
#include "wifi_provisioning.h"
#include "mqtt_adapter.h"
#include "relay_controller.h"
#include "scheduler.h"

static const char *TAG = "DISPLAY";

#define NOTIFY_BTN_BOOT         (1 << 0)
#define NOTIFY_BTN_SCREEN       (1 << 1)

// LEDC PWM Buzzer cấu hình
#define BUZZER_LEDC_TIMER       LEDC_TIMER_0
#define BUZZER_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BUZZER_FREQ_HZ          2000
#define BUZZER_AUTO_OFF_SEC     60

static TaskHandle_t s_display_task_handle = NULL;
static SemaphoreHandle_t s_display_mutex = NULL;

static display_screen_index_t s_current_screen = DISPLAY_SCREEN_CLOCK_TEMP;
static bool s_oled_detected = false;

// Trạng thái Báo thức & Còi
static bool s_alarm_active = false;
static char s_alarm_title[64] = "Báo thức";
static bool s_buzzer_sounding = false;
static uint64_t s_buzzer_start_time = 0;
static uint32_t s_snooze_until_epoch = 0;

// Trạng thái Telemetry lưu tạm để hiển thị
static float s_latest_temp = 25.0f;
static float s_latest_hum = 60.0f;
static bool s_temp_valid = false;

// Khai báo hàm lưu log ngoại lệ local theo FSD §4.9.5
__attribute__((weak)) void storage_append_event_log(const char *json_log)
{
    ESP_LOGI("STORAGE", "[EVENT_LOG LOCAL] %s", json_log);
}

static void buzzer_start(void)
{
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 512); // 50% duty
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    s_buzzer_sounding = true;
    s_buzzer_start_time = esp_timer_get_time() / 1000000ULL;
    ESP_LOGI(TAG, "Buzzer ON (Auto-off timeout: %d s)", BUZZER_AUTO_OFF_SEC);
}

static void buzzer_stop(void)
{
    ledc_set_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL, 0);
    ledc_update_duty(BUZZER_LEDC_MODE, BUZZER_LEDC_CHANNEL);
    s_buzzer_sounding = false;
    s_buzzer_start_time = 0;
}

static void IRAM_ATTR gpio_button_isr_handler(void *arg)
{
    int gpio_num = (int)arg;
    BaseType_t high_task_awoken = pdFALSE;
    uint32_t notify_val = (gpio_num == DISPLAY_BTN_BOOT_GPIO) ? NOTIFY_BTN_BOOT : NOTIFY_BTN_SCREEN;

    if (s_display_task_handle != NULL) {
        xTaskNotifyFromISR(s_display_task_handle, notify_val, eSetBits, &high_task_awoken);
        if (high_task_awoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

void display_trigger_alarm(const char *title, bool buzzer)
{
    if (s_display_mutex != NULL) xSemaphoreTake(s_display_mutex, portMAX_DELAY);
    s_alarm_active = true;
    if (title != NULL && title[0] != '\0') {
        strncpy(s_alarm_title, title, sizeof(s_alarm_title) - 1);
        s_alarm_title[sizeof(s_alarm_title) - 1] = '\0';
    } else {
        strncpy(s_alarm_title, "Báo thức", sizeof(s_alarm_title) - 1);
    }

    if (buzzer) {
        buzzer_start();
    }
    if (s_display_mutex != NULL) xSemaphoreGive(s_display_mutex);

    display_render();
    ESP_LOGI(TAG, "ALARM TRIGGERED: '%s' (Buzzer: %s)", s_alarm_title, buzzer ? "YES" : "NO");
}

void display_stop_alarm(void)
{
    if (s_display_mutex != NULL) xSemaphoreTake(s_display_mutex, portMAX_DELAY);
    s_alarm_active = false;
    buzzer_stop();
    if (s_display_mutex != NULL) xSemaphoreGive(s_display_mutex);

    // Ghi nhận log dừng báo thức ngoại lệ local FSD §4.9.5
    char log_buf[96];
    snprintf(log_buf, sizeof(log_buf), "{\"type\":\"alarm_stopped\",\"ts\":%ld}", (long)time(NULL));
    storage_append_event_log(log_buf);

    display_render();
    ESP_LOGI(TAG, "Alarm stopped, buzzer dismissed");
}

void display_snooze_alarm(uint8_t minutes)
{
    if (s_display_mutex != NULL) xSemaphoreTake(s_display_mutex, portMAX_DELAY);
    s_alarm_active = false;
    buzzer_stop();
    s_snooze_until_epoch = (uint32_t)time(NULL) + (minutes * 60);
    if (s_display_mutex != NULL) xSemaphoreGive(s_display_mutex);

    ESP_LOGI(TAG, "Alarm snoozed for %u minutes", (unsigned int)minutes);
    display_render();
}

bool display_is_alarm_active(void)
{
    return s_alarm_active;
}

void display_set_screen(uint8_t screen_index)
{
    if (screen_index < DISPLAY_SCREEN_COUNT) {
        s_current_screen = (display_screen_index_t)screen_index;
        display_render();
    }
}

void display_render(void)
{
    char time_str[32] = {0};
    app_rtc_get_formatted_time(time_str, sizeof(time_str));

    time_t now_epoch = time(NULL);
    struct tm ti;
    localtime_r(&now_epoch, &ti);

    static const char *wday_names[] = {"CN", "T2", "T3", "T4", "T5", "T6", "T7"};
    const char *wday = (ti.tm_wday >= 0 && ti.tm_wday <= 6) ? wday_names[ti.tm_wday] : "??";

    bool wifi_ok = wifi_provisioning_is_connected();
    bool mqtt_ok = mqtt_adapter_is_connected();

    if (s_alarm_active) {
        printf("\n==================================================\n");
        printf("               ⚠️  BÁO THỨC ĐANG REO  ⚠️          \n");
        printf("         Tiêu đề : %s\n", s_alarm_title);
        printf("         Thời gian: %s\n", time_str);
        printf("   [Bấm Nút 2 (GPIO 4): TẮT CHUÔNG NGAY]\n");
        printf("   [Bấm Nút 1 (GPIO 0): SNOOZE 9 PHÚT]\n");
        printf("==================================================\n\n");
        fflush(stdout);
        return;
    }

    switch (s_current_screen) {
        case DISPLAY_SCREEN_CLOCK_TEMP: {
            printf("\n┌──────────────────────────────────────────────┐\n");
            printf("│ [MÀN HÌNH 0: ĐỒNG HỒ & MÔI TRƯỜNG]            │\n");
            printf("│ Giờ & Ngày : %02d:%02d:%02d  (%s, %02d/%02d/%04d) │\n",
                   ti.tm_hour, ti.tm_min, ti.tm_sec, wday, ti.tm_mday, ti.tm_mon + 1, ti.tm_year + 1900);
            if (s_temp_valid) {
                printf("│ Nhiệt & Ẩm : %.1f °C  |  %.1f %%RH              │\n", s_latest_temp, s_latest_hum);
            } else {
                printf("│ Nhiệt & Ẩm : --.- °C  |  --.- %%RH              │\n");
            }
            printf("│ Mạng       : Wi-Fi: %s  |  MQTT: %s       │\n",
                   wifi_ok ? "● OK " : "○ OFF", mqtt_ok ? "● OK " : "○ OFF");
            if (s_snooze_until_epoch > now_epoch) {
                printf("│ Trạng thái : SNOOZE 9m (còn %lu s)           │\n",
                       (unsigned long)(s_snooze_until_epoch - now_epoch));
            }
            printf("└──────────────────────────────────────────────┘\n");
            break;
        }

        case DISPLAY_SCREEN_UPCOMING_EVENT: {
            printf("\n┌──────────────────────────────────────────────┐\n");
            printf("│ [MÀN HÌNH 1: LỊCH BIỂU SẮP TỚI]              │\n");
            printf("│ Hiện tại   : %s                 │\n", time_str);
            printf("│ Sắp tới:                                     │\n");
            printf("│   ⏰ 06:30 : Báo thức thức dậy                │\n");
            printf("│   🔔 17:30 : Bật đèn ban công (Tự động)       │\n");
            printf("│   📅 21:00 : Đọc sách                         │\n");
            printf("└──────────────────────────────────────────────┘\n");
            break;
        }

        case DISPLAY_SCREEN_DEVICE_STATUS: {
            bool r1 = relay_controller_get_state(1);
            bool r2 = relay_controller_get_state(2);
            bool r3 = relay_controller_get_state(3);
            bool r4 = relay_controller_get_state(4);

            printf("\n┌──────────────────────────────────────────────┐\n");
            printf("│ [MÀN HÌNH 2: TRẠNG THÁI THIẾT BỊ RƠ-LE]       │\n");
            printf("│ Kênh 1 (Quạt làm mát)  : %s (Max 4h)      │\n", r1 ? "[ BẬT - ON ]" : "[ TẮT - OFF]");
            printf("│ Kênh 2 (Máy sưởi nhiệt): %s (Max 30m)     │\n", r2 ? "[ BẬT - ON ]" : "[ TẮT - OFF]");
            printf("│ Kênh 3 (Máy hút ẩm)    : %s (Max 60m)     │\n", r3 ? "[ BẬT - ON ]" : "[ TẮT - OFF]");
            printf("│ Kênh 4 (Đèn phụ trợ)   : %s               │\n", r4 ? "[ BẬT - ON ]" : "[ TẮT - OFF]");
            printf("└──────────────────────────────────────────────┘\n");
            break;
        }

        default:
            s_current_screen = DISPLAY_SCREEN_CLOCK_TEMP;
            break;
    }
    fflush(stdout);
}

static void display_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Display task running on Core %d (Priority %d, 1 FPS)",
             xPortGetCoreID(), DISPLAY_TASK_PRIO);

    uint32_t notify_val = 0;
    uint64_t last_btn_boot_time = 0;
    uint64_t last_btn_screen_time = 0;

    while (1) {
        // Chờ nhận Direct Notification từ ISR nút bấm hoặc timeout 1000ms (1 FPS)
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notify_val, pdMS_TO_TICKS(1000));
        uint64_t now_ms = esp_timer_get_time() / 1000ULL;

        // Báo cáo nhịp tim định kỳ tới Task Supervisor (FSD §4.13.2)
        supervisor_report_heartbeat(TASK_HEARTBEAT_DISPLAY);

        if (notified == pdTRUE && notify_val != 0) {
            // 1. Xử lý Nút 1 (BTN_BOOT, GPIO 0)
            if ((notify_val & NOTIFY_BTN_BOOT) && (now_ms - last_btn_boot_time > 200)) {
                last_btn_boot_time = now_ms;
                if (s_alarm_active) {
                    // Khi chuông đang kêu: Nhấn nhanh (<2s) -> SNOOZE 9 phút
                    ESP_LOGI(TAG, "Button 1 pressed during alarm -> SNOOZE 9m");
                    display_snooze_alarm(9);
                } else {
                    // Khi bình thường: kiểm tra nhấn giữ >= 5s để Reset Wi-Fi
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (gpio_get_level(DISPLAY_BTN_BOOT_GPIO) == 0) {
                        ESP_LOGI(TAG, "BTN_BOOT pressed. Checking if held for 5s...");
                        int count_sec = 0;
                        while (gpio_get_level(DISPLAY_BTN_BOOT_GPIO) == 0 && count_sec < 50) {
                            vTaskDelay(pdMS_TO_TICKS(100));
                            count_sec++;
                        }
                        if (count_sec >= 50) {
                            ESP_LOGW(TAG, "BTN_BOOT held >= 5s! Resetting Wi-Fi to SoftAP 'SmartClock-Setup'...");
                            wifi_provisioning_force_reset();
                            event_bus_post(EVT_WIFI_PROVISIONING_STARTED, NULL, 0, false);
                        }
                    }
                }
            }

            // 2. Xử lý Nút 2 (BTN_SCREEN_NEXT, GPIO 4)
            if ((notify_val & NOTIFY_BTN_SCREEN) && (now_ms - last_btn_screen_time > 200)) {
                last_btn_screen_time = now_ms;
                if (s_alarm_active) {
                    // Khi chuông đang kêu: Nhấn nhanh (<500ms) -> TẮT CHUÔNG NGAY (DISMISS, FSD §4.9.4 & M13)
                    ESP_LOGI(TAG, "Button 2 pressed during alarm -> DISMISS ALARM IMMEDIATELY");
                    display_stop_alarm();
                    event_bus_post(EVT_BUTTON_ALARM_STOP, NULL, 0, false);
                } else {
                    // Khi bình thường: Chuyển đổi luân phiên màn hình OLED
                    s_current_screen = (display_screen_index_t)((s_current_screen + 1) % DISPLAY_SCREEN_COUNT);
                    ESP_LOGI(TAG, "Button 2 pressed -> Switched to Screen %d", s_current_screen);
                    event_bus_post(EVT_BUTTON_SCREEN_NEXT, NULL, 0, false);
                    display_render();
                }
            }
        }

        // Tự ngắt còi buzzer nếu kêu liên tục vượt quá 60 giây (FSD §4.9.6)
        if (s_buzzer_sounding && s_buzzer_start_time > 0) {
            uint64_t now_s = esp_timer_get_time() / 1000000ULL;
            if (now_s - s_buzzer_start_time >= BUZZER_AUTO_OFF_SEC) {
                ESP_LOGW(TAG, "Buzzer exceeded %d s continuous ringing! Auto-stopping buzzer.", BUZZER_AUTO_OFF_SEC);
                buzzer_stop();
            }
        }

        // Kiểm tra hết hạn Snooze để kích hoạt lại chuông
        if (!s_alarm_active && s_snooze_until_epoch > 0) {
            if ((uint32_t)time(NULL) >= s_snooze_until_epoch) {
                s_snooze_until_epoch = 0;
                display_trigger_alarm(s_alarm_title, true);
            }
        }

        // Cập nhật kết xuất màn hình định kỳ 1 giây
        display_render();
    }
}

// Event Bus Callback Handlers
static void on_alarm_triggered(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(alarm_event_t)) {
        const alarm_event_t *alarm = (const alarm_event_t *)evt->data;
        display_trigger_alarm(alarm->title, alarm->buzzer);
    } else {
        display_trigger_alarm("Báo thức hệ thống", true);
    }
}

static void on_alarm_stop(const gateway_event_t *evt)
{
    (void)evt;
    display_stop_alarm();
}

static void on_screen_next(const gateway_event_t *evt)
{
    (void)evt;
    s_current_screen = (display_screen_index_t)((s_current_screen + 1) % DISPLAY_SCREEN_COUNT);
    display_render();
}

static void on_display_update_request(const gateway_event_t *evt)
{
    (void)evt;
    display_render();
}

static void on_ble_device_data(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(ble_telemetry_t)) {
        const ble_telemetry_t *telem = (const ble_telemetry_t *)evt->data;
        s_latest_temp = telem->temperature;
        s_latest_hum = telem->humidity;
        s_temp_valid = telem->valid;
    }
}

static void on_relay_state_changed(const gateway_event_t *evt)
{
    (void)evt;
    // Cập nhật màn hình nếu đang ở trang xem trạng thái thiết bị
    if (s_current_screen == DISPLAY_SCREEN_DEVICE_STATUS) {
        display_render();
    }
}

void display_init(void)
{
    ESP_LOGI(TAG, "Initializing Display Subsystem (SSD1327 OLED + Buzzer + Buttons)...");

    if (s_display_mutex == NULL) {
        s_display_mutex = xSemaphoreCreateMutex();
    }

    // 1. Khởi tạo Buzzer PWM qua LEDC Timer
    ledc_timer_config_t timer_conf = {
        .speed_mode = BUZZER_LEDC_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = BUZZER_LEDC_TIMER,
        .freq_hz = BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {
        .gpio_num = DISPLAY_BUZZER_GPIO,
        .speed_mode = BUZZER_LEDC_MODE,
        .channel = BUZZER_LEDC_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = BUZZER_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&ch_conf);
    buzzer_stop();

    // 2. Khởi tạo Nút bấm GPIO 0 & 4 (Pull-up, ngắt cạnh xuống)
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << DISPLAY_BTN_BOOT_GPIO) | (1ULL << DISPLAY_BTN_SCREEN_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(DISPLAY_BTN_BOOT_GPIO, gpio_button_isr_handler, (void *)DISPLAY_BTN_BOOT_GPIO);
    gpio_isr_handler_add(DISPLAY_BTN_SCREEN_GPIO, gpio_button_isr_handler, (void *)DISPLAY_BTN_SCREEN_GPIO);

    // 3. Kiểm tra kết nối phần cứng OLED SSD1327 qua I2C0 (0x3C)
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DISPLAY_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(DISPLAY_I2C_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        s_oled_detected = true;
        ESP_LOGI(TAG, "OLED SSD1327 detected at I2C address 0x%02X", DISPLAY_I2C_ADDR);
    } else {
        s_oled_detected = false;
        ESP_LOGW(TAG, "OLED SSD1327 not detected at I2C 0x%02X (using virtual UI display)", DISPLAY_I2C_ADDR);
    }

    // 4. Đăng ký các Handler trên Event Bus
    event_bus_register_handler(EVT_SCHEDULE_ALARM_TRIGGERED, on_alarm_triggered);
    event_bus_register_handler(EVT_BUTTON_ALARM_STOP, on_alarm_stop);
    event_bus_register_handler(EVT_BUTTON_SCREEN_NEXT, on_screen_next);
    event_bus_register_handler(EVT_DISPLAY_UPDATE_REQUEST, on_display_update_request);
    event_bus_register_handler(EVT_BLE_DEVICE_DATA, on_ble_device_data);
    event_bus_register_handler(EVT_RELAY_STATE_CHANGED, on_relay_state_changed);

    // 5. Đạt chuẩn KPI-BOOT-01: Vẽ màn hình đồng hồ ngay lập tức (<1.5s độc lập mạng)
    display_render();

    // 6. Tạo Task Display trên Core 0, Priority 3, Stack 4096 (FSD §4.9.4 & §4.14)
    xTaskCreatePinnedToCore(
        display_task,
        "display_task",
        4096,
        NULL,
        DISPLAY_TASK_PRIO,
        &s_display_task_handle,
        DISPLAY_TASK_CORE
    );

    ESP_LOGI(TAG, "Display Subsystem initialized successfully (Direct-to-Task Notification enabled)");
}
