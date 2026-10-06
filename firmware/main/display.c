#include "display.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
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
#include "rule_engine.h"
#include "config_manager.h"
#include "font5x7.h"

static const char *TAG = "DISPLAY";

#define NOTIFY_BTN_BOOT         (1 << 0)
#define NOTIFY_BTN_SCREEN       (1 << 1)

// LEDC PWM Buzzer cấu hình
#define BUZZER_LEDC_TIMER       LEDC_TIMER_0
#define BUZZER_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define BUZZER_LEDC_CHANNEL     LEDC_CHANNEL_0
#define BUZZER_FREQ_HZ          2000
#define BUZZER_AUTO_OFF_SEC     60

// SSD1327 128x128 4-bit Grayscale Framebuffer (64 bytes/row, 128 rows = 8192 bytes)
#define SSD1327_WIDTH           128
#define SSD1327_HEIGHT          128
#define SSD1327_BYTES_PER_ROW   (SSD1327_WIDTH / 2)
#define SSD1327_BUFFER_SIZE     (SSD1327_BYTES_PER_ROW * SSD1327_HEIGHT)

static uint8_t s_oled_buffer[SSD1327_BUFFER_SIZE];

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

// =========================================================================
// SSD1327 OLED Low-Level I2C Hardware Driver
// =========================================================================

static esp_err_t ssd1327_send_cmd(uint8_t cmd)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (DISPLAY_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x00, true); // Control byte: Command stream
    i2c_master_write_byte(h, cmd, true);
    i2c_master_stop(h);
    esp_err_t ret = i2c_master_cmd_begin(DISPLAY_I2C_PORT, h, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(h);
    return ret;
}

static esp_err_t ssd1327_send_data(const uint8_t *data, size_t len)
{
    i2c_cmd_handle_t h = i2c_cmd_link_create();
    i2c_master_start(h);
    i2c_master_write_byte(h, (DISPLAY_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(h, 0x40, true); // Control byte: Data stream
    i2c_master_write(h, (uint8_t *)data, len, true);
    i2c_master_stop(h);
    esp_err_t ret = i2c_master_cmd_begin(DISPLAY_I2C_PORT, h, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(h);
    return ret;
}

static void ssd1327_init_hardware(void)
{
    ESP_LOGI(TAG, "Configuring SSD1327 (MSP1503 128x128 4-bit Grayscale)...");

    // Sequence chuẩn chính hãng LCDWiki MSP1503 SSD1327 128x128
    static const uint8_t init_cmds[] = {
        0xFD, 0x12,       // 1. Command unlock (BẮT BUỘC ĐẦU TIÊN để mở khóa chip nhận lệnh)
        0xAE,             // 2. Display OFF
        0x15, 0x00, 0x3F, // 3. Set Column Address: 0 to 63 (128 pixels, 2 pixels/col)
        0x75, 0x00, 0x7F, // 4. Set Row Address: 0 to 127
        0xA8, 0x7F,       // 5. Multiplex ratio: 127 (128 rows)
        0xA1, 0x00,       // 6. Start line: 0
        0xA2, 0x00,       // 7. Display offset: 0
        0xA0, 0x51,       // 8. Remap: 0x51 (horizontal increment, 8-bit bus, COM remap)
        0xAB, 0x01,       // 9. Select internal VDD regulator
        0x81, 0x70,       // 10. Contrast control: 0x70 (Độ tương phản sáng)
        0xB1, 0x51,       // 11. Phase length
        0xB3, 0x01,       // 12. Front clock divider
        0xB9,             // 13. Linear grayscale lookup table (Bảng sắc xám tuyến tính)
        0xBC, 0x08,       // 14. Pre-charge voltage
        0xBE, 0x07,       // 15. VCOMH voltage
        0xB6, 0x01,       // 16. Second pre-charge period
        0xD5, 0x62,       // 17. Function selection B
        0xA4,             // 18. Normal display
        0xAF              // 19. Display ON (BẬT SÁNG MÀN HÌNH)
    };

    for (size_t i = 0; i < sizeof(init_cmds); i++) {
        ssd1327_send_cmd(init_cmds[i]);
    }

    ESP_LOGI(TAG, "SSD1327 display powered ON successfully!");
}

static void ssd1327_clear(void)
{
    memset(s_oled_buffer, 0, sizeof(s_oled_buffer));
}

static void ssd1327_draw_pixel(int x, int y, uint8_t gray)
{
    if (x < 0 || x >= SSD1327_WIDTH || y < 0 || y >= SSD1327_HEIGHT) return;
    int idx = y * SSD1327_BYTES_PER_ROW + (x / 2);
    if ((x & 1) == 0) {
        // Pixel chẵn nằm ở 4 bit cao (High nibble)
        s_oled_buffer[idx] = (s_oled_buffer[idx] & 0x0F) | ((gray & 0x0F) << 4);
    } else {
        // Pixel lẻ nằm ở 4 bit thấp (Low nibble)
        s_oled_buffer[idx] = (s_oled_buffer[idx] & 0xF0) | (gray & 0x0F);
    }
}

static void ssd1327_draw_char(int x, int y, char c, uint8_t gray, int scale)
{
    if (c < 32 || c > 126) c = ' ';
    const uint8_t *bitmap = font5x7[c - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t line = bitmap[col];
        for (int row = 0; row < 8; row++) {
            if (line & (1 << row)) {
                if (scale <= 1) {
                    ssd1327_draw_pixel(x + col, y + row, gray);
                } else {
                    for (int dx = 0; dx < scale; dx++) {
                        for (int dy = 0; dy < scale; dy++) {
                            ssd1327_draw_pixel(x + col * scale + dx, y + row * scale + dy, gray);
                        }
                    }
                }
            }
        }
    }
}

static void ssd1327_draw_string(int x, int y, const char *str, uint8_t gray, int scale)
{
    if (!str) return;
    int cur_x = x;
    int char_w = 6 * scale;
    while (*str) {
        if (*str == '\n') {
            y += 8 * scale;
            cur_x = x;
        } else {
            ssd1327_draw_char(cur_x, y, *str, gray, scale);
            cur_x += char_w;
        }
        str++;
    }
}

static void ssd1327_draw_hline(int x, int y, int w, uint8_t gray)
{
    for (int i = 0; i < w; i++) {
        ssd1327_draw_pixel(x + i, y, gray);
    }
}

static void ssd1327_flush(void)
{
    if (!s_oled_detected) return;

    // Thiết lập vùng quét: Column 0..63 (128 pixels), Row 0..127
    ssd1327_send_cmd(0x15);
    ssd1327_send_cmd(0x00);
    ssd1327_send_cmd(0x3F);

    ssd1327_send_cmd(0x75);
    ssd1327_send_cmd(0x00);
    ssd1327_send_cmd(0x7F);

    // Gửi dữ liệu theo các khối 512 bytes (8 dòng) để tối ưu bus I2C, giảm thời gian chiếm dụng
    size_t remaining = sizeof(s_oled_buffer);
    size_t offset = 0;
    while (remaining > 0) {
        size_t chunk = (remaining > 512) ? 512 : remaining;
        ssd1327_send_data(&s_oled_buffer[offset], chunk);
        offset += chunk;
        remaining -= chunk;
    }
}

// =========================================================================
// Buzzer & Button Management
// =========================================================================

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

// =========================================================================
// Screen Render Logic (Physical OLED + Serial Virtual Console)
// =========================================================================

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
    bool wifi_ap = wifi_provisioning_is_softap();
    bool mqtt_ok = mqtt_adapter_is_connected();

    // 1. Kết xuất đồ họa lên màn hình OLED vật lý SSD1327 (128x128)
    if (s_oled_detected) {
        ssd1327_clear();

        if (s_alarm_active) {
            // Màn hình cảnh báo Báo thức
            ssd1327_draw_string(14, 8, "! ALARM !", 0xF, 2);
            ssd1327_draw_hline(6, 28, 116, 0xC);
            ssd1327_draw_string(8, 38, s_alarm_title, 0xF, 1);
            ssd1327_draw_string(8, 56, time_str, 0xE, 1);
            ssd1327_draw_hline(6, 78, 116, 0x8);
            ssd1327_draw_string(8, 90, "Btn 2: DISMISS", 0xF, 1);
            ssd1327_draw_string(8, 108, "Btn 1: SNOOZE 9m", 0xB, 1);
        } else {
            switch (s_current_screen) {
                case DISPLAY_SCREEN_CLOCK_TEMP: {
                    // Header Bar (Thứ, ngày/tháng, trạng thái Wi-Fi, MQTT)
                    char header_buf[32];
                    snprintf(header_buf, sizeof(header_buf), "%s %02d/%02d", wday, ti.tm_mday, ti.tm_mon + 1);
                    ssd1327_draw_string(4, 4, header_buf, 0xC, 1);

                    char net_buf[16];
                    snprintf(net_buf, sizeof(net_buf), "%s %s", wifi_ok ? "WF" : (wifi_ap ? "AP" : "--"), mqtt_ok ? "MQ" : "--");
                    ssd1327_draw_string(88, 4, net_buf, 0xA, 1);
                    ssd1327_draw_hline(0, 16, 128, 0x6);

                    // Đồng hồ lớn chính giữa màn hình (Scale 2)
                    char clock_buf[16];
                    snprintf(clock_buf, sizeof(clock_buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
                    ssd1327_draw_string(16, 26, clock_buf, 0xF, 2);

                    ssd1327_draw_hline(0, 48, 128, 0x6);

                    // Thông số Cảm biến Môi trường
                    if (s_temp_valid) {
                        char env_buf[32];
                        snprintf(env_buf, sizeof(env_buf), "Temp: %.1f C", s_latest_temp);
                        ssd1327_draw_string(8, 56, env_buf, 0xE, 1);
                        snprintf(env_buf, sizeof(env_buf), "Humi: %.1f %%", s_latest_hum);
                        ssd1327_draw_string(8, 72, env_buf, 0xE, 1);
                    } else {
                        ssd1327_draw_string(8, 56, "Temp: --.- C", 0x7, 1);
                        ssd1327_draw_string(8, 72, "Humi: --.- %", 0x7, 1);
                    }

                    // Thanh trạng thái dưới cùng
                    ssd1327_draw_hline(0, 94, 128, 0x5);
                    if (s_snooze_until_epoch > now_epoch) {
                        char snz[32];
                        snprintf(snz, sizeof(snz), "SNOOZE: %lus", (unsigned long)(s_snooze_until_epoch - now_epoch));
                        ssd1327_draw_string(8, 106, snz, 0xD, 1);
                    } else {
                        ssd1327_draw_string(8, 106, "SMART CLOCK EDGE", 0x9, 1);
                    }
                    break;
                }

                case DISPLAY_SCREEN_UPCOMING_EVENT: {
                    ssd1327_draw_string(4, 4, "UPCOMING EVENTS", 0xF, 1);
                    ssd1327_draw_hline(0, 16, 128, 0x6);
                    ssd1327_draw_string(6, 26, "06:30 Wakeup", 0xE, 1);
                    ssd1327_draw_string(6, 46, "17:30 Fan Auto", 0xC, 1);
                    ssd1327_draw_string(6, 66, "21:00 Aux Off", 0xA, 1);
                    ssd1327_draw_hline(0, 98, 128, 0x4);
                    ssd1327_draw_string(6, 108, "[SCREEN 1/3]", 0x8, 1);
                    break;
                }

                case DISPLAY_SCREEN_DEVICE_STATUS: {
                    ssd1327_draw_string(4, 4, "RELAY STATUS", 0xF, 1);
                    ssd1327_draw_hline(0, 16, 128, 0x6);
                    bool r1 = relay_controller_get_state(1);
                    bool r2 = relay_controller_get_state(2);
                    bool r3 = relay_controller_get_state(3);
                    bool r4 = relay_controller_get_state(4);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "R1 Fan:  %s", r1 ? "ON " : "OFF");
                    ssd1327_draw_string(6, 26, buf, r1 ? 0xF : 0x7, 1);
                    snprintf(buf, sizeof(buf), "R2 Heat: %s", r2 ? "ON " : "OFF");
                    ssd1327_draw_string(6, 44, buf, r2 ? 0xF : 0x7, 1);
                    snprintf(buf, sizeof(buf), "R3 Deh:  %s", r3 ? "ON " : "OFF");
                    ssd1327_draw_string(6, 62, buf, r3 ? 0xF : 0x7, 1);
                    snprintf(buf, sizeof(buf), "R4 Aux:  %s", r4 ? "ON " : "OFF");
                    ssd1327_draw_string(6, 80, buf, r4 ? 0xF : 0x7, 1);
                    ssd1327_draw_hline(0, 98, 128, 0x4);
                    ssd1327_draw_string(6, 108, "[SCREEN 2/4]", 0x8, 1);
                    break;
                }

                case DISPLAY_SCREEN_PROVISIONING: {
                    ssd1327_draw_string(14, 6, "WI-FI SETUP", 0xF, 1);
                    ssd1327_draw_hline(0, 18, 128, 0x6);
                    ssd1327_draw_string(6, 32, "AP: SmartClock-Setup", 0xE, 1);
                    ssd1327_draw_string(6, 52, "IP: 192.168.4.1", 0xF, 1);
                    ssd1327_draw_hline(0, 74, 128, 0x4);
                    ssd1327_draw_string(6, 88, "Connect & setup via", 0xA, 1);
                    ssd1327_draw_string(6, 104, "192.168.4.1 portal", 0x8, 1);
                    break;
                }

                default:
                    break;
            }
        }
        ssd1327_flush();
    }

    // 2. Ghi ra Serial Console cho Virtual UI Testing
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
                   wifi_ok ? "● OK " : (wifi_ap ? "◐ AP " : "○ OFF"), mqtt_ok ? "● OK " : "○ OFF");
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

        case DISPLAY_SCREEN_PROVISIONING: {
            printf("\n┌──────────────────────────────────────────────┐\n");
            printf("│ [MÀN HÌNH PROVISIONING: THIẾT LẬP WI-FI]      │\n");
            printf("│ Tiêu đề    : WI-FI SETUP                     │\n");
            printf("│ SoftAP SSID: SmartClock-Setup                │\n");
            printf("│ IP truy cập: 192.168.4.1                     │\n");
            printf("│ Trạng thái : ◐ AP (Đang chờ cấu hình)        │\n");
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
        // Đồng bộ thời gian thức dậy chính xác theo microsecond của đồng hồ thực tế (Wall-Clock Phase Locking)
        struct timeval tv;
        gettimeofday(&tv, NULL);
        uint32_t cur_ms_in_sec = (uint32_t)(tv.tv_usec / 1000);
        uint32_t wait_ms = (cur_ms_in_sec < 1000) ? (1000 - cur_ms_in_sec + 20) : 1000;
        if (wait_ms < 30) {
            wait_ms += 1000;
        }

        // Chờ nhận Direct Notification từ ISR nút bấm hoặc timeout đến đúng đầu giây tiếp theo
        BaseType_t notified = xTaskNotifyWait(0, ULONG_MAX, &notify_val, pdMS_TO_TICKS(wait_ms));
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
                    // Khi bình thường: kiểm tra nhấn giữ >= 5s (5000ms) để Factory Reset toàn diện
                    vTaskDelay(pdMS_TO_TICKS(100));
                    if (gpio_get_level(DISPLAY_BTN_BOOT_GPIO) == 0) {
                        ESP_LOGI(TAG, "BTN_BOOT pressed. Checking if held continuously for 5s...");
                        int count_sec = 0;
                        while (gpio_get_level(DISPLAY_BTN_BOOT_GPIO) == 0 && count_sec < 50) {
                            vTaskDelay(pdMS_TO_TICKS(100));
                            count_sec++;
                            supervisor_report_heartbeat(TASK_HEARTBEAT_DISPLAY);
                        }
                        if (count_sec >= 50) {
                            ESP_LOGW(TAG, "BTN_BOOT held >= 5s! Factory Reset: Wiping all user data, Wi-Fi credentials, rules, and schedules...");
                            buzzer_start();
                            vTaskDelay(pdMS_TO_TICKS(400));
                            buzzer_stop();

                            rule_engine_clear_rules();
                            scheduler_clear_entries();
                            wifi_provisioning_force_reset();

                            s_current_screen = DISPLAY_SCREEN_PROVISIONING;
                            display_render();
                            event_bus_post(EVT_WIFI_PROVISIONING_STARTED, NULL, 0, false);
                            ESP_LOGI(TAG, "[FACTORY_RESET] Completed. All personal data erased. SoftAP OPEN ready.");
                        } else {
                            ESP_LOGI(TAG, "BTN_BOOT released early (%d ms). Factory Reset aborted.", count_sec * 100);
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

static void on_wifi_connected_display(const gateway_event_t *evt)
{
    (void)evt;
    if (s_current_screen == DISPLAY_SCREEN_PROVISIONING) {
        ESP_LOGI(TAG, "Wi-Fi Station connected -> Auto-transitioning from Provisioning to Clock/Temp screen (FR-DISP-02)");
        s_current_screen = DISPLAY_SCREEN_CLOCK_TEMP;
        display_render();
    }
}

static void on_wifi_credentials_received_display(const gateway_event_t *evt)
{
    (void)evt;
    if (s_current_screen == DISPLAY_SCREEN_PROVISIONING) {
        ESP_LOGI(TAG, "Wi-Fi credentials / password configured -> Auto-transitioning to Clock screen");
        s_current_screen = DISPLAY_SCREEN_CLOCK_TEMP;
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
        // Khởi tạo phần cứng màn hình vật lý SSD1327 MSP1503
        ssd1327_init_hardware();
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
    event_bus_register_handler(EVT_WIFI_CONNECTED, on_wifi_connected_display);
    event_bus_register_handler(EVT_WIFI_CREDENTIALS_RECEIVED, on_wifi_credentials_received_display);

    // FSD v1.7 §4.9 DI-07 & FR-DISP-02: Nếu chưa kết nối Wi-Fi Station và chưa có mật khẩu/cấu hình, mặc định hiển thị trang Provisioning
    const gateway_config_t *cfg = config_manager_get();
    bool has_creds = (cfg && (cfg->softap_password[0] != '\0' || cfg->wifi_ssid[0] != '\0' || cfg->saved_wifi_count > 0));
    if (!wifi_provisioning_is_connected() && !has_creds) {
        s_current_screen = DISPLAY_SCREEN_PROVISIONING;
    } else {
        s_current_screen = DISPLAY_SCREEN_CLOCK_TEMP;
    }

    // 5. Đạt chuẩn KPI-BOOT-01: Vẽ màn hình ngay lập tức (<1.5s độc lập mạng)
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

    ESP_LOGI(TAG, "Display Subsystem initialized successfully (Physical OLED + Direct Notification enabled)");
}
