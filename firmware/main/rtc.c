#include "rtc.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "driver/i2c.h"
#include "event_bus.h"

static const char *TAG = "APP_RTC";

#define I2C_MASTER_SDA_IO      8
#define I2C_MASTER_SCL_IO      9
#define I2C_MASTER_FREQ_HZ     400000
#define I2C_MASTER_PORT        I2C_NUM_0
#define DS3231_I2C_ADDR        0x68

static bool s_ds3231_present = false;
static bool s_ntp_initialized = false;

// BCD conversion helpers
static inline uint8_t bcd2bin(uint8_t val) {
    return val - 6 * (val >> 4);
}

static inline uint8_t bin2bcd(uint8_t val) {
    return val + 6 * (val / 10);
}

static bool ds3231_get_time(struct tm *timeinfo)
{
    if (!s_ds3231_present || !timeinfo) return false;

    uint8_t reg = 0x00;
    uint8_t data[7] = {0};

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, data, 6, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &data[6], I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) return false;

    timeinfo->tm_sec  = bcd2bin(data[0] & 0x7F);
    timeinfo->tm_min  = bcd2bin(data[1] & 0x7F);
    timeinfo->tm_hour = bcd2bin(data[2] & 0x3F); // 24-hour mode
    timeinfo->tm_wday = (data[3] & 0x07) - 1;    // 1-7 -> 0-6
    if (timeinfo->tm_wday < 0 || timeinfo->tm_wday > 6) timeinfo->tm_wday = 0;
    timeinfo->tm_mday = bcd2bin(data[4] & 0x3F);
    timeinfo->tm_mon  = bcd2bin(data[5] & 0x1F) - 1; // 1-12 -> 0-11
    timeinfo->tm_year = bcd2bin(data[6]) + 2000 - 1900; // 00-99 -> years since 1900
    timeinfo->tm_isdst = 0;

    return true;
}

static bool ds3231_write_time(const struct tm *timeinfo)
{
    if (!s_ds3231_present || !timeinfo) return false;

    uint8_t data[8];
    data[0] = 0x00; // Register pointer
    data[1] = bin2bcd(timeinfo->tm_sec % 60);
    data[2] = bin2bcd(timeinfo->tm_min % 60);
    data[3] = bin2bcd(timeinfo->tm_hour % 24);
    data[4] = (timeinfo->tm_wday >= 0 && timeinfo->tm_wday <= 6) ? (timeinfo->tm_wday + 1) : 1;
    data[5] = bin2bcd(timeinfo->tm_mday ? timeinfo->tm_mday : 1);
    data[6] = bin2bcd((timeinfo->tm_mon + 1) % 13);
    data[7] = bin2bcd((timeinfo->tm_year + 1900) % 100);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write(cmd, data, 8, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
    i2c_cmd_link_delete(cmd);

    return (ret == ESP_OK);
}

bool app_rtc_set_time(const struct tm *timeinfo)
{
    if (!timeinfo) return false;

    struct tm ti = *timeinfo;
    time_t epoch = mktime(&ti);
    if (epoch != (time_t)-1) {
        struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
        settimeofday(&tv, NULL);
    }

    if (s_ds3231_present) {
        ds3231_write_time(&ti);
        ESP_LOGI(TAG, "[RTC] Written to DS3231 hardware clock: %04d-%02d-%02d %02d:%02d:%02d",
                 ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday, ti.tm_hour, ti.tm_min, ti.tm_sec);
    }
    return true;
}

static void time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;
    time_t now = time(NULL);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "[RTC] NTP Time synchronized successfully: %s", strftime_buf);

    if (s_ds3231_present) {
        ds3231_write_time(&timeinfo);
        ESP_LOGI(TAG, "[RTC] Saved NTP synchronized time into DS3231 battery-backed registers.");
    }

    // FSD v1.7 §4.2 & FR-RTC-01: Ngắt hoàn toàn SNTP client sau lần đồng bộ đầu tiên
    // Triệt tiêu lưu lượng UDP NTP dư thừa, giải phóng socket, để DS3231 chạy độc lập
    esp_sntp_stop();
    s_ntp_initialized = false;
    ESP_LOGI(TAG, "[RTC] One-shot SNTP stopped. Hardware RTC now running independently");
}

static void on_wifi_connected(const gateway_event_t *evt)
{
    (void)evt;
    ESP_LOGI(TAG, "Wi-Fi connection detected on Event Bus. Triggering NTP sync...");
    app_rtc_sync_from_ntp_and_save();
}

void app_rtc_init(void)
{
    app_rtc_set_timezone("ICT-7");

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    esp_err_t err = i2c_param_config(I2C_MASTER_PORT, &conf);
    if (err == ESP_OK) {
        err = i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
    }

    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t probe_err = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (probe_err == ESP_OK) {
            s_ds3231_present = true;
            ESP_LOGI(TAG, "[RTC] DS3231 RTC hardware found at I2C address 0x%02X", DS3231_I2C_ADDR);

            // Đọc thời gian thực tế đang lưu trong chip DS3231
            struct tm hw_time;
            if (ds3231_get_time(&hw_time)) {
                if (hw_time.tm_year >= (2024 - 1900)) {
                    time_t now = mktime(&hw_time);
                    struct timeval tv = { .tv_sec = now, .tv_usec = 0 };
                    settimeofday(&tv, NULL);
                    ESP_LOGI(TAG, "[RTC] Successfully restored time from DS3231: %04d-%02d-%02d %02d:%02d:%02d",
                             hw_time.tm_year + 1900, hw_time.tm_mon + 1, hw_time.tm_mday,
                             hw_time.tm_hour, hw_time.tm_min, hw_time.tm_sec);
                } else {
                    ESP_LOGW(TAG, "[RTC] DS3231 time uninitialized (%04d-%02d-%02d). Seeding with current date/time.",
                             hw_time.tm_year + 1900, hw_time.tm_mon + 1, hw_time.tm_mday);
                    // Cài đặt thời gian mốc khởi tạo ban đầu: 2026-09-26 19:00:00 (ICT-7)
                    struct tm init_time = {
                        .tm_year = 2026 - 1900,
                        .tm_mon  = 9 - 1, // Tháng 9
                        .tm_mday = 26,    // Ngày 26
                        .tm_hour = 19,
                        .tm_min  = 0,
                        .tm_sec  = 0,
                        .tm_wday = 6,     // Thứ 7
                        .tm_isdst = 0
                    };
                    app_rtc_set_time(&init_time);
                }
            }
        } else {
            s_ds3231_present = false;
            ESP_LOGW(TAG, "[RTC] DS3231 not detected on I2C (bare-board mode), falling back to system SNTP clock.");
        }
    } else {
        s_ds3231_present = false;
        ESP_LOGW(TAG, "[RTC] I2C driver install failed. Bare-board mode.");
    }

    event_bus_register_handler(EVT_WIFI_CONNECTED, on_wifi_connected);
}

time_t app_rtc_get_time(void)
{
    return time(NULL);
}

bool app_rtc_sync_from_ntp_and_save(void)
{
    if (!s_ntp_initialized) {
        ESP_LOGI(TAG, "Initializing SNTP client (server: pool.ntp.org)...");
        esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "pool.ntp.org");
        esp_sntp_setservername(1, "time.google.com");
        esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
        esp_sntp_init();
        s_ntp_initialized = true;
    } else {
        ESP_LOGI(TAG, "Restarting SNTP sync cycle...");
        esp_sntp_restart();
    }
    return true;
}

void app_rtc_set_timezone(const char *tz_string)
{
    if (tz_string == NULL || strlen(tz_string) == 0) return;

    if (strstr(tz_string, "Ho_Chi_Minh") || strstr(tz_string, "Bangkok") || strstr(tz_string, "Hanoi") || strstr(tz_string, "ICT")) {
        setenv("TZ", "ICT-7", 1);
    } else {
        setenv("TZ", tz_string, 1);
    }
    tzset();
    ESP_LOGI(TAG, "System Timezone set to '%s'", getenv("TZ"));
}

void app_rtc_get_formatted_time(char *buf, size_t max_len)
{
    time_t now = app_rtc_get_time();
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    strftime(buf, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}

bool rtc_is_valid(void)
{
    time_t now = app_rtc_get_time();
    struct tm t;
    localtime_r(&now, &t);
    return (t.tm_year >= (2024 - 1900));
}
