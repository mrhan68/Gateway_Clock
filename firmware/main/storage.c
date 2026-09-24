#include "storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "esp_log.h"
#include "esp_spiffs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "event_bus.h"

static const char *TAG = "STORAGE";

static bool s_mounted = false;
static SemaphoreHandle_t s_storage_mutex = NULL;

bool storage_is_mounted(void)
{
    return s_mounted;
}

static bool write_file_string(const char *path, const char *str)
{
    if (!s_mounted || !path || !str) return false;

    if (s_storage_mutex) xSemaphoreTake(s_storage_mutex, portMAX_DELAY);

    FILE *f = fopen(path, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file '%s' for writing", path);
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return false;
    }

    size_t len = strlen(str);
    size_t written = fwrite(str, 1, len, f);
    fclose(f);

    if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
    return (written == len);
}

static char* read_file_string(const char *path, const char *default_val)
{
    if (!s_mounted || !path) {
        return default_val ? strdup(default_val) : NULL;
    }

    if (s_storage_mutex) xSemaphoreTake(s_storage_mutex, portMAX_DELAY);

    struct stat st;
    if (stat(path, &st) != 0 || st.st_size == 0) {
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return default_val ? strdup(default_val) : NULL;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return default_val ? strdup(default_val) : NULL;
    }

    char *buf = (char *)malloc(st.st_size + 1);
    if (!buf) {
        fclose(f);
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return default_val ? strdup(default_val) : NULL;
    }

    size_t read_bytes = fread(buf, 1, st.st_size, f);
    fclose(f);
    buf[read_bytes] = '\0';

    if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
    return buf;
}

bool storage_save_schedule_json(const char *json_str)
{
    ESP_LOGI(TAG, "Saving schedule to %s...", STORAGE_SCHEDULE_PATH);
    return write_file_string(STORAGE_SCHEDULE_PATH, json_str);
}

char *storage_load_schedule_json(void)
{
    return read_file_string(STORAGE_SCHEDULE_PATH, "[]");
}

bool storage_save_device_list_json(const char *json_str)
{
    ESP_LOGI(TAG, "Saving device list to %s...", STORAGE_DEVICE_LIST_PATH);
    return write_file_string(STORAGE_DEVICE_LIST_PATH, json_str);
}

char *storage_load_device_list_json(void)
{
    return read_file_string(STORAGE_DEVICE_LIST_PATH, "[]");
}

void storage_init(void)
{
    ESP_LOGI(TAG, "Mounting SPIFFS partition '%s' at '%s'...", SPIFFS_PARTITION_LABEL, SPIFFS_BASE_PATH);

    if (s_storage_mutex == NULL) {
        s_storage_mutex = xSemaphoreCreateMutex();
    }

    esp_vfs_spiffs_conf_t conf = {
        .base_path = SPIFFS_BASE_PATH,
        .partition_label = SPIFFS_PARTITION_LABEL,
        .max_files = 5,
        .format_if_mount_failed = true
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format SPIFFS filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition '%s'", SPIFFS_PARTITION_LABEL);
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        s_mounted = false;
        return;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(SPIFFS_PARTITION_LABEL, &total, &used);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SPIFFS mounted successfully! Total: %d KB, Used: %d KB", (int)(total / 1024), (int)(used / 1024));
        s_mounted = true;
    } else {
        ESP_LOGW(TAG, "Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
        s_mounted = true;
    }

    ESP_LOGI(TAG, "Storage subsystem initialized successfully");
}
