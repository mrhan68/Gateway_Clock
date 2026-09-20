#include "config_manager.h"
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "CONFIG_MGR";
static const char *NVS_NAMESPACE = "sys_cfg";

static gateway_config_t s_active_config;

static void set_default_config(void)
{
    memset(&s_active_config, 0, sizeof(s_active_config));
    strncpy(s_active_config.gateway_id, "GW_SMARTCLOCK_01", sizeof(s_active_config.gateway_id) - 1);
    strncpy(s_active_config.wifi_ssid, "", sizeof(s_active_config.wifi_ssid) - 1);
    strncpy(s_active_config.wifi_password, "", sizeof(s_active_config.wifi_password) - 1);
    strncpy(s_active_config.mqtt_broker_uri, "mqtt://broker.emqx.io:1883", sizeof(s_active_config.mqtt_broker_uri) - 1);
    s_active_config.mqtt_telemetry_interval_s = 10;
    s_active_config.rs485_baudrate = 9600;
    s_active_config.rs485_timeout_ms = 300;
}

esp_err_t config_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated/corrupt, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        set_default_config();
        return ret;
    }

    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(ret));
        set_default_config();
        return ret;
    }

    set_default_config();

    // Đọc Gateway ID
    size_t len = sizeof(s_active_config.gateway_id);
    nvs_get_str(handle, "gw_id", s_active_config.gateway_id, &len);

    // Đọc Wi-Fi SSID & Password từ NVS Flash
    len = sizeof(s_active_config.wifi_ssid);
    nvs_get_str(handle, "wifi_ssid", s_active_config.wifi_ssid, &len);

    len = sizeof(s_active_config.wifi_password);
    nvs_get_str(handle, "wifi_pass", s_active_config.wifi_password, &len);

    // Đọc MQTT Broker
    len = sizeof(s_active_config.mqtt_broker_uri);
    nvs_get_str(handle, "mqtt_uri", s_active_config.mqtt_broker_uri, &len);
    if (strcmp(s_active_config.mqtt_broker_uri, "mqtt://192.168.1.100:1883") == 0) {
        strncpy(s_active_config.mqtt_broker_uri, "mqtt://broker.emqx.io:1883", sizeof(s_active_config.mqtt_broker_uri) - 1);
    }

    nvs_get_u16(handle, "mqtt_interval", &s_active_config.mqtt_telemetry_interval_s);

    // Đọc RS485 Params
    nvs_get_u32(handle, "rs485_baud", &s_active_config.rs485_baudrate);
    nvs_get_u16(handle, "rs485_timeout", &s_active_config.rs485_timeout_ms);

    nvs_close(handle);

    ESP_LOGI(TAG, "Configuration loaded successfully from NVS Flash");
    return ESP_OK;
}

const gateway_config_t* config_manager_get(void)
{
    return &s_active_config;
}

esp_err_t config_manager_set_wifi(const char *ssid, const char *password)
{
    if (!ssid) return ESP_ERR_INVALID_ARG;
    strncpy(s_active_config.wifi_ssid, ssid, sizeof(s_active_config.wifi_ssid) - 1);
    if (password) {
        strncpy(s_active_config.wifi_password, password, sizeof(s_active_config.wifi_password) - 1);
    } else {
        s_active_config.wifi_password[0] = '\0';
    }
    ESP_LOGI(TAG, "Wi-Fi set in volatile RAM (will be lost on reset)");
    return ESP_OK;
}

esp_err_t config_manager_set_mqtt(const char *uri, uint16_t interval_s)
{
    if (!uri) return ESP_ERR_INVALID_ARG;
    strncpy(s_active_config.mqtt_broker_uri, uri, sizeof(s_active_config.mqtt_broker_uri) - 1);
    if (interval_s > 0) {
        s_active_config.mqtt_telemetry_interval_s = interval_s;
    }
    return config_manager_save();
}

esp_err_t config_manager_set_rs485(uint32_t baudrate, uint16_t timeout_ms)
{
    if (baudrate == 0) return ESP_ERR_INVALID_ARG;
    s_active_config.rs485_baudrate = baudrate;
    if (timeout_ms > 0) {
        s_active_config.rs485_timeout_ms = timeout_ms;
    }
    return config_manager_save();
}

esp_err_t config_manager_save(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS for writing: %s", esp_err_to_name(ret));
        return ret;
    }

    nvs_set_str(handle, "gw_id", s_active_config.gateway_id);
    // Lưu ý: Wi-Fi được giữ trong RAM tạm thời, không lưu NVS để mất khi reset
    nvs_erase_key(handle, "wifi_ssid");
    nvs_erase_key(handle, "wifi_pass");
    nvs_set_str(handle, "mqtt_uri", s_active_config.mqtt_broker_uri);
    nvs_set_u16(handle, "mqtt_interval", s_active_config.mqtt_telemetry_interval_s);
    nvs_set_u32(handle, "rs485_baud", s_active_config.rs485_baudrate);
    nvs_set_u16(handle, "rs485_timeout", s_active_config.rs485_timeout_ms);

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration committed to NVS Flash (Wi-Fi in volatile RAM)");
    } else {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    }
    return ret;
}

void config_manager_print(void)
{
    printf("\n=== SMART CLOCK GATEWAY CONFIGURATION ===\n");
    printf("Gateway ID       : %s\n", s_active_config.gateway_id);
    printf("Wi-Fi SSID       : %s\n", s_active_config.wifi_ssid[0] ? s_active_config.wifi_ssid : "(not configured)");
    printf("Wi-Fi Password   : %s\n", s_active_config.wifi_password[0] ? "********" : "(none)");
    printf("MQTT Broker URI  : %s\n", s_active_config.mqtt_broker_uri);
    printf("MQTT Interval    : %u s\n", s_active_config.mqtt_telemetry_interval_s);
    printf("RS485 Baudrate   : %lu bps\n", (unsigned long)s_active_config.rs485_baudrate);
    printf("RS485 Timeout    : %u ms\n", s_active_config.rs485_timeout_ms);
    printf("=========================================\n\n");
}
