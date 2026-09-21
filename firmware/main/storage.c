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

bool storage_append_event_log(const char *event_json_line)
{
    if (!event_json_line) return false;

    if (!s_mounted) {
        ESP_LOGW(TAG, "Storage not mounted, cannot append event log: %s", event_json_line);
        return false;
    }

    if (s_storage_mutex) xSemaphoreTake(s_storage_mutex, portMAX_DELAY);

    FILE *f = fopen(STORAGE_EVENT_LOG_PATH, "a");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s for append", STORAGE_EVENT_LOG_PATH);
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return false;
    }

    fprintf(f, "%s\n", event_json_line);
    fclose(f);

    if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);

    // Phát sự kiện lên Event Bus
    event_bus_post(EVT_EVENT_LOG_APPENDED, NULL, 0, false);
    return true;
}

char *storage_get_pending_event_log(void)
{
    if (!s_mounted) return strdup("[]");

    if (s_storage_mutex) xSemaphoreTake(s_storage_mutex, portMAX_DELAY);

    FILE *f = fopen(STORAGE_EVENT_LOG_PATH, "r");
    if (!f) {
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return strdup("[]");
    }

    // Đọc từng dòng và đóng gói thành mảng JSON: [ line1, line2, ... ]
    size_t cap = 1024;
    char *out = (char *)malloc(cap);
    if (!out) {
        fclose(f);
        if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
        return strdup("[]");
    }

    strcpy(out, "[");
    bool first = true;
    char line[256];

    while (fgets(line, sizeof(line), f)) {
        // Cắt bỏ newline
        size_t l = strlen(line);
        while (l > 0 && (line[l - 1] == '\r' || line[l - 1] == '\n')) {
            line[--l] = '\0';
        }
        if (l == 0) continue;

        size_t needed = strlen(out) + l + 3;
        if (needed >= cap) {
            cap = needed + 1024;
            char *new_out = (char *)realloc(out, cap);
            if (!new_out) break;
            out = new_out;
        }

        if (!first) {
            strcat(out, ",");
        }
        strcat(out, line);
        first = false;
    }

    strcat(out, "]");
    fclose(f);

    if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
    return out;
}

void storage_clear_synced_event_log(void)
{
    if (!s_mounted) return;

    if (s_storage_mutex) xSemaphoreTake(s_storage_mutex, portMAX_DELAY);
    FILE *f = fopen(STORAGE_EVENT_LOG_PATH, "w");
    if (f) {
        fclose(f);
        ESP_LOGI(TAG, "Cleared synced event log from %s", STORAGE_EVENT_LOG_PATH);
    }
    if (s_storage_mutex) xSemaphoreGive(s_storage_mutex);
}

void storage_trigger_cloud_sync(void)
{
    ESP_LOGI(TAG, "[STORAGE] Cloud sync triggered via event bus");
    event_bus_post(EVT_SYNC_COMPLETED, NULL, 0, false);
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
