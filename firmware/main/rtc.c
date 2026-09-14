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

static void time_sync_notification_cb(struct timeval *tv)
{
    (void)tv;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "[RTC] NTP Time synchronized successfully: %s", strftime_buf);
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

    if (err == ESP_OK) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (DS3231_I2C_ADDR << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        esp_err_t probe_err = i2c_master_cmd_begin(I2C_MASTER_PORT, cmd, pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (probe_err == ESP_OK) {
            s_ds3231_present = true;
            ESP_LOGI(TAG, "[RTC] DS3231 RTC hardware found at I2C address 0x%02X", DS3231_I2C_ADDR);
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

    if (strstr(tz_string, "Ho_Chi_Minh") || strstr(tz_string, "Bangkok") || strstr(tz_string, "Hanoi")) {
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
