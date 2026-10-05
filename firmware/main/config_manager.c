#include "config_manager.h"
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "ble_adapter.h"

static const char *TAG = "CONFIG_MGR";
static const char *NVS_NAMESPACE = "sys_cfg";

static gateway_config_t s_active_config;

static void set_default_config(void)
{
    memset(&s_active_config, 0, sizeof(s_active_config));
    strncpy(s_active_config.gateway_id, "gw_smartclock_01", sizeof(s_active_config.gateway_id) - 1);
    strncpy(s_active_config.timezone, "ICT-7", sizeof(s_active_config.timezone) - 1);
    strncpy(s_active_config.wifi_ssid, "", sizeof(s_active_config.wifi_ssid) - 1);
    strncpy(s_active_config.wifi_password, "", sizeof(s_active_config.wifi_password) - 1);
    s_active_config.softap_password[0] = '\0';
    strncpy(s_active_config.mqtt_broker_uri, "mqtt://broker.emqx.io:1883", sizeof(s_active_config.mqtt_broker_uri) - 1);
    s_active_config.mqtt_telemetry_interval_s = 10;
    s_active_config.saved_wifi_count = 0;
    memset(s_active_config.saved_wifi, 0, sizeof(s_active_config.saved_wifi));
}

esp_err_t config_manager_load_from_file(const char *filepath)
{
    if (!filepath) {
        filepath = "/storage/config.json";
    }

    struct stat st;
    if (stat(filepath, &st) != 0) {
        if (strcmp(filepath, "/storage/config.json") == 0 && stat("/spiffs/config.json", &st) == 0) {
            filepath = "/spiffs/config.json";
        } else {
            ESP_LOGD(TAG, "Config file '%s' does not exist, skipping", filepath);
            return ESP_ERR_NOT_FOUND;
        }
    }

    FILE *f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open config file: %s", filepath);
        return ESP_FAIL;
    }

    char *buf = (char *)malloc(st.st_size + 1);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate memory for config file read (%ld bytes)", st.st_size);
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    size_t read_bytes = fread(buf, 1, st.st_size, f);
    fclose(f);
    buf[read_bytes] = '\0';

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) {
        ESP_LOGE(TAG, "Failed to parse JSON from config file '%s'", filepath);
        return ESP_ERR_INVALID_ARG;
    }

    // gateway_id
    cJSON *gw_id = cJSON_GetObjectItem(root, "gateway_id");
    if (gw_id && cJSON_IsString(gw_id) && gw_id->valuestring[0] != '\0') {
        strncpy(s_active_config.gateway_id, gw_id->valuestring, sizeof(s_active_config.gateway_id) - 1);
        s_active_config.gateway_id[sizeof(s_active_config.gateway_id) - 1] = '\0';
    }

    // timezone (mặc định ICT-7 theo FSD §4.12.2)
    cJSON *tz = cJSON_GetObjectItem(root, "timezone");
    if (tz && cJSON_IsString(tz) && tz->valuestring[0] != '\0') {
        strncpy(s_active_config.timezone, tz->valuestring, sizeof(s_active_config.timezone) - 1);
        s_active_config.timezone[sizeof(s_active_config.timezone) - 1] = '\0';
    }

    // wifi
    cJSON *wifi = cJSON_GetObjectItem(root, "wifi");
    if (wifi && cJSON_IsObject(wifi)) {
        cJSON *ssid = cJSON_GetObjectItem(wifi, "ssid");
        cJSON *pass = cJSON_GetObjectItem(wifi, "password");
        if (ssid && cJSON_IsString(ssid)) {
            strncpy(s_active_config.wifi_ssid, ssid->valuestring, sizeof(s_active_config.wifi_ssid) - 1);
            s_active_config.wifi_ssid[sizeof(s_active_config.wifi_ssid) - 1] = '\0';
        }
        if (pass && cJSON_IsString(pass)) {
            strncpy(s_active_config.wifi_password, pass->valuestring, sizeof(s_active_config.wifi_password) - 1);
            s_active_config.wifi_password[sizeof(s_active_config.wifi_password) - 1] = '\0';
        }
    }

    // mqtt
    cJSON *mqtt = cJSON_GetObjectItem(root, "mqtt");
    if (mqtt && cJSON_IsObject(mqtt)) {
        cJSON *uri = cJSON_GetObjectItem(mqtt, "broker_uri");
        cJSON *interval = cJSON_GetObjectItem(mqtt, "telemetry_interval_s");
        if (uri && cJSON_IsString(uri)) {
            strncpy(s_active_config.mqtt_broker_uri, uri->valuestring, sizeof(s_active_config.mqtt_broker_uri) - 1);
            s_active_config.mqtt_broker_uri[sizeof(s_active_config.mqtt_broker_uri) - 1] = '\0';
        }
        if (interval && cJSON_IsNumber(interval)) {
            s_active_config.mqtt_telemetry_interval_s = (uint16_t)interval->valueint;
        }
    }

    // ble_sensors dynamic registration
    cJSON *sensors = cJSON_GetObjectItem(root, "ble_sensors");
    if (sensors && cJSON_IsArray(sensors)) {
        int sensor_count = cJSON_GetArraySize(sensors);
        for (int i = 0; i < sensor_count; i++) {
            cJSON *item = cJSON_GetArrayItem(sensors, i);
            if (item && cJSON_IsObject(item)) {
                cJSON *dev_id = cJSON_GetObjectItem(item, "device_id");
                cJSON *mac = cJSON_GetObjectItem(item, "mac_address");
                if (dev_id && cJSON_IsString(dev_id) && dev_id->valuestring[0] != '\0') {
                    const char *mac_str = (mac && cJSON_IsString(mac)) ? mac->valuestring : NULL;
                    ble_adapter_register_device(dev_id->valuestring, mac_str);
                    ESP_LOGI(TAG, "Registered BLE device from config: %s (MAC: %s)",
                             dev_id->valuestring, mac_str ? mac_str : "any");
                }
            }
        }
    }

    cJSON_Delete(root);

    // Lưu vào NVS để đồng bộ
    config_manager_save();

    // FR-CFG-01 Expected log line
    ESP_LOGI(TAG, "[CFG] Loaded configuration successfully from %s", filepath);
    return ESP_OK;
}

esp_err_t config_manager_init(void)
{
    set_default_config();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated/corrupt, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return ret;
    }

    nvs_handle_t handle;
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s': %s", NVS_NAMESPACE, esp_err_to_name(ret));
        return ret;
    }

    // Đọc Gateway ID
    size_t len = sizeof(s_active_config.gateway_id);
    nvs_get_str(handle, "gw_id", s_active_config.gateway_id, &len);

    // Đọc Timezone (mặc định "ICT-7")
    len = sizeof(s_active_config.timezone);
    if (nvs_get_str(handle, "timezone", s_active_config.timezone, &len) != ESP_OK || s_active_config.timezone[0] == '\0') {
        strncpy(s_active_config.timezone, "ICT-7", sizeof(s_active_config.timezone) - 1);
    }

    // Đọc Wi-Fi SSID & Password từ NVS Flash
    len = sizeof(s_active_config.wifi_ssid);
    nvs_get_str(handle, "wifi_ssid", s_active_config.wifi_ssid, &len);

    len = sizeof(s_active_config.wifi_password);
    nvs_get_str(handle, "wifi_pass", s_active_config.wifi_password, &len);

    // Đọc số lượng và danh sách mạng Wi-Fi đã lưu (FSD §4.4.6)
    uint8_t w_count = 0;
    if (nvs_get_u8(handle, "w_count", &w_count) == ESP_OK && w_count <= MAX_SAVED_WIFI) {
        s_active_config.saved_wifi_count = w_count;
        for (uint8_t i = 0; i < w_count; i++) {
            char k_ssid[16], k_pass[16];
            snprintf(k_ssid, sizeof(k_ssid), "w_ssid_%u", i);
            snprintf(k_pass, sizeof(k_pass), "w_pass_%u", i);
            size_t slen = sizeof(s_active_config.saved_wifi[i].ssid);
            size_t plen = sizeof(s_active_config.saved_wifi[i].password);
            nvs_get_str(handle, k_ssid, s_active_config.saved_wifi[i].ssid, &slen);
            nvs_get_str(handle, k_pass, s_active_config.saved_wifi[i].password, &plen);
        }
    } else if (s_active_config.wifi_ssid[0] != '\0') {
        s_active_config.saved_wifi_count = 1;
        strncpy(s_active_config.saved_wifi[0].ssid, s_active_config.wifi_ssid, sizeof(s_active_config.saved_wifi[0].ssid) - 1);
        strncpy(s_active_config.saved_wifi[0].password, s_active_config.wifi_password, sizeof(s_active_config.saved_wifi[0].password) - 1);
    }

    // Đọc SoftAP Password (mặc định "" (OPEN) theo Adaptive SoftAP Security WP-04 / FR-WIFI-01)
    len = sizeof(s_active_config.softap_password);
    if (nvs_get_str(handle, "softap_pass", s_active_config.softap_password, &len) != ESP_OK ||
        strcmp(s_active_config.softap_password, "changeme") == 0 ||
        (s_active_config.wifi_ssid[0] == '\0' && s_active_config.saved_wifi_count == 0)) {
        s_active_config.softap_password[0] = '\0';
    }

    // Đọc MQTT Broker
    len = sizeof(s_active_config.mqtt_broker_uri);
    nvs_get_str(handle, "mqtt_uri", s_active_config.mqtt_broker_uri, &len);
    if (strcmp(s_active_config.mqtt_broker_uri, "mqtt://192.168.1.100:1883") == 0) {
        strncpy(s_active_config.mqtt_broker_uri, "mqtt://broker.emqx.io:1883", sizeof(s_active_config.mqtt_broker_uri) - 1);
    }

    nvs_get_u16(handle, "mqtt_interval", &s_active_config.mqtt_telemetry_interval_s);

    nvs_close(handle);

    // Nạp tệp cấu hình /spiffs/config.json nếu tồn tại (FSD §4.12.2 & FR-CFG-01)
    config_manager_load_from_file("/spiffs/config.json");

    ESP_LOGI(TAG, "Configuration loaded successfully from NVS Flash (Timezone: %s)", s_active_config.timezone);
    return ESP_OK;
}

const gateway_config_t* config_manager_get(void)
{
    return &s_active_config;
}

uint8_t config_manager_get_saved_wifi_count(void)
{
    return s_active_config.saved_wifi_count;
}

bool config_manager_get_saved_wifi(uint8_t index, char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len)
{
    if (index >= s_active_config.saved_wifi_count) return false;
    if (out_ssid && ssid_len > 0) {
        strncpy(out_ssid, s_active_config.saved_wifi[index].ssid, ssid_len - 1);
        out_ssid[ssid_len - 1] = '\0';
    }
    if (out_pass && pass_len > 0) {
        strncpy(out_pass, s_active_config.saved_wifi[index].password, pass_len - 1);
        out_pass[pass_len - 1] = '\0';
    }
    return true;
}

esp_err_t config_manager_add_or_update_wifi(const char *ssid, const char *password)
{
    if (!ssid || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;
    int found_idx = -1;
    for (uint8_t i = 0; i < s_active_config.saved_wifi_count; i++) {
        if (strcmp(s_active_config.saved_wifi[i].ssid, ssid) == 0) {
            found_idx = i;
            break;
        }
    }

    if (found_idx >= 0) {
        if (password) {
            strncpy(s_active_config.saved_wifi[found_idx].password, password, sizeof(s_active_config.saved_wifi[found_idx].password) - 1);
            s_active_config.saved_wifi[found_idx].password[sizeof(s_active_config.saved_wifi[found_idx].password) - 1] = '\0';
        } else {
            s_active_config.saved_wifi[found_idx].password[0] = '\0';
        }
    } else {
        if (s_active_config.saved_wifi_count < MAX_SAVED_WIFI) {
            found_idx = s_active_config.saved_wifi_count++;
        } else {
            found_idx = MAX_SAVED_WIFI - 1;
        }
        strncpy(s_active_config.saved_wifi[found_idx].ssid, ssid, sizeof(s_active_config.saved_wifi[found_idx].ssid) - 1);
        s_active_config.saved_wifi[found_idx].ssid[sizeof(s_active_config.saved_wifi[found_idx].ssid) - 1] = '\0';
        if (password) {
            strncpy(s_active_config.saved_wifi[found_idx].password, password, sizeof(s_active_config.saved_wifi[found_idx].password) - 1);
            s_active_config.saved_wifi[found_idx].password[sizeof(s_active_config.saved_wifi[found_idx].password) - 1] = '\0';
        } else {
            s_active_config.saved_wifi[found_idx].password[0] = '\0';
        }
    }

    // Luôn cập nhật mạng active hiện tại
    strncpy(s_active_config.wifi_ssid, ssid, sizeof(s_active_config.wifi_ssid) - 1);
    s_active_config.wifi_ssid[sizeof(s_active_config.wifi_ssid) - 1] = '\0';
    if (password) {
        strncpy(s_active_config.wifi_password, password, sizeof(s_active_config.wifi_password) - 1);
        s_active_config.wifi_password[sizeof(s_active_config.wifi_password) - 1] = '\0';
    } else {
        s_active_config.wifi_password[0] = '\0';
    }

    ESP_LOGI(TAG, "Saved Wi-Fi '%s' (Total saved: %u)", ssid, s_active_config.saved_wifi_count);
    return config_manager_save();
}

esp_err_t config_manager_delete_wifi(const char *ssid)
{
    if (!ssid || ssid[0] == '\0') return ESP_ERR_INVALID_ARG;
    int found_idx = -1;
    for (uint8_t i = 0; i < s_active_config.saved_wifi_count; i++) {
        if (strcmp(s_active_config.saved_wifi[i].ssid, ssid) == 0) {
            found_idx = i;
            break;
        }
    }
    if (found_idx < 0) return ESP_ERR_NOT_FOUND;

    for (uint8_t i = found_idx; i < s_active_config.saved_wifi_count - 1; i++) {
        s_active_config.saved_wifi[i] = s_active_config.saved_wifi[i + 1];
    }
    s_active_config.saved_wifi_count--;
    memset(&s_active_config.saved_wifi[s_active_config.saved_wifi_count], 0, sizeof(wifi_profile_t));

    // Nếu vừa xóa mạng đang active, chuyển sang mạng còn lại hoặc reset
    if (strcmp(s_active_config.wifi_ssid, ssid) == 0) {
        if (s_active_config.saved_wifi_count > 0) {
            strncpy(s_active_config.wifi_ssid, s_active_config.saved_wifi[0].ssid, sizeof(s_active_config.wifi_ssid) - 1);
            strncpy(s_active_config.wifi_password, s_active_config.saved_wifi[0].password, sizeof(s_active_config.wifi_password) - 1);
        } else {
            s_active_config.wifi_ssid[0] = '\0';
            s_active_config.wifi_password[0] = '\0';
        }
    }

    ESP_LOGI(TAG, "Deleted Wi-Fi '%s' (Remaining saved: %u)", ssid, s_active_config.saved_wifi_count);
    return config_manager_save();
}

esp_err_t config_manager_set_wifi(const char *ssid, const char *password)
{
    if (!ssid) return ESP_ERR_INVALID_ARG;
    return config_manager_add_or_update_wifi(ssid, password);
}

esp_err_t config_manager_set_softap_password(const char *password)
{
    if (!password || password[0] == '\0') {
        s_active_config.softap_password[0] = '\0';
    } else {
        strncpy(s_active_config.softap_password, password, sizeof(s_active_config.softap_password) - 1);
        s_active_config.softap_password[sizeof(s_active_config.softap_password) - 1] = '\0';
    }
    return config_manager_save();
}

esp_err_t config_manager_factory_reset(void)
{
    ESP_LOGW(TAG, "[FACTORY_RESET] Erasing NVS credentials...");
    s_active_config.wifi_ssid[0] = '\0';
    s_active_config.wifi_password[0] = '\0';
    s_active_config.softap_password[0] = '\0';
    s_active_config.saved_wifi_count = 0;
    memset(s_active_config.saved_wifi, 0, sizeof(s_active_config.saved_wifi));
    config_manager_save();

    nvs_handle_t wifi_h;
    if (nvs_open("wifi_cfg", NVS_READWRITE, &wifi_h) == ESP_OK) {
        nvs_erase_all(wifi_h);
        nvs_commit(wifi_h);
        nvs_close(wifi_h);
    }
    ESP_LOGI(TAG, "[FACTORY_RESET] Wi-Fi credentials wiped. SoftAP restored to OPEN mode.");
    return ESP_OK;
}

esp_err_t config_manager_set_mqtt(const char *uri, uint16_t interval_s)
{
    if (!uri) return ESP_ERR_INVALID_ARG;
    strncpy(s_active_config.mqtt_broker_uri, uri, sizeof(s_active_config.mqtt_broker_uri) - 1);
    s_active_config.mqtt_broker_uri[sizeof(s_active_config.mqtt_broker_uri) - 1] = '\0';
    if (interval_s > 0) {
        s_active_config.mqtt_telemetry_interval_s = interval_s;
    }
    return config_manager_save();
}

esp_err_t config_manager_set_timezone(const char *tz)
{
    if (!tz || tz[0] == '\0') return ESP_ERR_INVALID_ARG;
    strncpy(s_active_config.timezone, tz, sizeof(s_active_config.timezone) - 1);
    s_active_config.timezone[sizeof(s_active_config.timezone) - 1] = '\0';
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
    nvs_set_str(handle, "timezone", s_active_config.timezone);

    if (s_active_config.wifi_ssid[0] != '\0') {
        nvs_set_str(handle, "wifi_ssid", s_active_config.wifi_ssid);
        nvs_set_str(handle, "wifi_pass", s_active_config.wifi_password);
    } else {
        nvs_erase_key(handle, "wifi_ssid");
        nvs_erase_key(handle, "wifi_pass");
    }

    nvs_set_u8(handle, "w_count", s_active_config.saved_wifi_count);
    for (uint8_t i = 0; i < MAX_SAVED_WIFI; i++) {
        char k_ssid[16], k_pass[16];
        snprintf(k_ssid, sizeof(k_ssid), "w_ssid_%u", i);
        snprintf(k_pass, sizeof(k_pass), "w_pass_%u", i);
        if (i < s_active_config.saved_wifi_count) {
            nvs_set_str(handle, k_ssid, s_active_config.saved_wifi[i].ssid);
            nvs_set_str(handle, k_pass, s_active_config.saved_wifi[i].password);
        } else {
            nvs_erase_key(handle, k_ssid);
            nvs_erase_key(handle, k_pass);
        }
    }

    nvs_set_str(handle, "softap_pass", s_active_config.softap_password);
    nvs_set_str(handle, "mqtt_uri", s_active_config.mqtt_broker_uri);
    nvs_set_u16(handle, "mqtt_interval", s_active_config.mqtt_telemetry_interval_s);

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Configuration committed to NVS Flash successfully");
    } else {
        ESP_LOGE(TAG, "Failed to commit NVS changes: %s", esp_err_to_name(ret));
    }
    return ret;
}

void config_manager_print(void)
{
    printf("\n=== SMART CLOCK GATEWAY CONFIGURATION ===\n");
    printf("Gateway ID       : %s\n", s_active_config.gateway_id);
    printf("Timezone         : %s\n", s_active_config.timezone);
    printf("Wi-Fi SSID       : %s\n", s_active_config.wifi_ssid[0] ? s_active_config.wifi_ssid : "(not configured)");
    printf("Wi-Fi Password   : %s\n", s_active_config.wifi_password[0] ? "********" : "(none)");
    printf("SoftAP Password  : %s\n", s_active_config.softap_password[0] ? "********" : "(None - Open)");
    printf("MQTT Broker URI  : %s\n", s_active_config.mqtt_broker_uri);
    printf("MQTT Interval    : %u s\n", s_active_config.mqtt_telemetry_interval_s);
    printf("=========================================\n\n");
}
