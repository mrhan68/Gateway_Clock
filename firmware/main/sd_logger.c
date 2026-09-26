#include "sd_logger.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "event_bus.h"
#include "supervisor.h"
#include "mqtt_adapter.h"

static const char *TAG = "SD_LOGGER";

#define SD_QUEUE_DEPTH          30
#define SD_TASK_PRIO            4
#define SD_TASK_CORE            1
#define SD_BACKFILL_BATCH_SIZE  20

typedef struct {
    uint8_t  record_type;
    uint8_t  flags;
    uint32_t timestamp;
    char     device_id[16];
    float    value;
    bool     valid;
} sd_log_item_t;

static bool s_sd_mounted = false;
static sdmmc_card_t *s_sd_card = NULL;
static QueueHandle_t s_sd_log_queue = NULL;
static SemaphoreHandle_t s_sd_mutex = NULL;
static TaskHandle_t s_sd_task_handle = NULL;

static uint32_t calculate_crc32(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1) {
                crc = (crc >> 1) ^ 0xEDB88320;
            } else {
                crc >>= 1;
            }
        }
    }
    return ~crc;
}

bool sd_logger_is_mounted(void)
{
    return s_sd_mounted;
}

static void ensure_sync_queue_header(FILE *f, sync_queue_header_t *out_hdr)
{
    fseek(f, 0, SEEK_END);
    long size = ftell(f);

    if (size < (long)sizeof(sync_queue_header_t)) {
        // Khởi tạo file mới với 24-byte header chuẩn (FSD §4.11.3)
        memset(out_hdr, 0, sizeof(sync_queue_header_t));
        out_hdr->magic = SYNC_QUEUE_MAGIC;
        out_hdr->write_offset = sizeof(sync_queue_header_t); // Bắt đầu ghi tại byte 24
        out_hdr->read_offset = sizeof(sync_queue_header_t);  // Bắt đầu đọc tại byte 24
        out_hdr->total_records = 0;
        out_hdr->unacked_count = 0;
        out_hdr->crc32 = calculate_crc32((const uint8_t *)out_hdr, 20);

        fseek(f, 0, SEEK_SET);
        fwrite(out_hdr, 1, sizeof(sync_queue_header_t), f);
        fflush(f);
    } else {
        fseek(f, 0, SEEK_SET);
        fread(out_hdr, 1, sizeof(sync_queue_header_t), f);
        uint32_t expected_crc = calculate_crc32((const uint8_t *)out_hdr, 20);
        if (out_hdr->magic != SYNC_QUEUE_MAGIC || out_hdr->crc32 != expected_crc) {
            ESP_LOGW(TAG, "Sync queue header corrupt or invalid magic! Reinitializing...");
            out_hdr->magic = SYNC_QUEUE_MAGIC;
            out_hdr->write_offset = sizeof(sync_queue_header_t);
            out_hdr->read_offset = sizeof(sync_queue_header_t);
            out_hdr->total_records = 0;
            out_hdr->unacked_count = 0;
            out_hdr->crc32 = calculate_crc32((const uint8_t *)out_hdr, 20);
            fseek(f, 0, SEEK_SET);
            fwrite(out_hdr, 1, sizeof(sync_queue_header_t), f);
            fflush(f);
        }
    }
}

static void append_sync_queue_record(const sd_log_item_t *item)
{
    if (!s_sd_mounted) return;

    if (s_sd_mutex) xSemaphoreTake(s_sd_mutex, portMAX_DELAY);

    mkdir("/sdcard/data", 0755);
    FILE *f = fopen(SD_SYNC_QUEUE_PATH, "r+b");
    if (!f) {
        f = fopen(SD_SYNC_QUEUE_PATH, "w+b");
    }
    if (!f) {
        ESP_LOGE(TAG, "Cannot open %s for writing (errno=%d: %s)", SD_SYNC_QUEUE_PATH, errno, strerror(errno));
        if (s_sd_mutex) xSemaphoreGive(s_sd_mutex);
        return;
    }

    sync_queue_header_t hdr;
    ensure_sync_queue_header(f, &hdr);

    // Đóng gói 172-byte binary record chuẩn FSD §4.11.3 / test_sync_queue.py
    sync_queue_record_t rec;
    memset(&rec, 0, sizeof(sync_queue_record_t));
    rec.timestamp = item->timestamp;
    rec.record_type = item->record_type;
    rec.flags = item->flags;
    strncpy(rec.device_id, item->device_id, sizeof(rec.device_id) - 1);
    rec.device_id[sizeof(rec.device_id) - 1] = '\0';
    snprintf(rec.payload, sizeof(rec.payload), "{\"val\":%.2f,\"ts\":%lu}",
             item->value, (unsigned long)item->timestamp);
    rec.payload_len = (uint16_t)strlen(rec.payload);
    rec.crc32 = calculate_crc32((const uint8_t *)&rec, 168);

    // Ghi record vào vị trí write_offset
    fseek(f, hdr.write_offset, SEEK_SET);
    fwrite(&rec, 1, sizeof(sync_queue_record_t), f);

    // Cập nhật header
    hdr.write_offset += sizeof(sync_queue_record_t);
    hdr.total_records++;
    hdr.unacked_count++;
    hdr.crc32 = calculate_crc32((const uint8_t *)&hdr, 20);

    fseek(f, 0, SEEK_SET);
    fwrite(&hdr, 1, sizeof(sync_queue_header_t), f);
    fflush(f);

    fclose(f);
    if (s_sd_mutex) xSemaphoreGive(s_sd_mutex);
}

static void append_telemetry_csv(const sd_log_item_t *item)
{
    if (!s_sd_mounted) return;

    time_t t = (time_t)item->timestamp;
    struct tm ti;
    localtime_r(&t, &ti);

    char filepath[64];
    snprintf(filepath, sizeof(filepath), "%s/%04d-%02d-%02d.csv",
             SD_TELEMETRY_DIR, ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday);

    if (s_sd_mutex) xSemaphoreTake(s_sd_mutex, portMAX_DELAY);

    FILE *f = fopen(filepath, "a");
    if (f) {
        fprintf(f, "%lu,%s,%.2f,%s\n",
                (unsigned long)item->timestamp, item->device_id, item->value, item->valid ? "OK" : "ERR");
        fclose(f);
    }

    if (s_sd_mutex) xSemaphoreGive(s_sd_mutex);
}

void sd_logger_append_crash_log(const char *crash_text)
{
    if (!crash_text) return;

    ESP_LOGE(TAG, "[CRASH LOG] %s", crash_text);

    const char *target_path = s_sd_mounted ? SD_CRASH_LOG_PATH : "/spiffs/crash.log";

    if (s_sd_mutex) xSemaphoreTake(s_sd_mutex, portMAX_DELAY);

    FILE *f = fopen(target_path, "a");
    if (f) {
        time_t now = time(NULL);
        fprintf(f, "[%ld] %s\n", (long)now, crash_text);
        fflush(f);
        fclose(f);
    }

    if (s_sd_mutex) xSemaphoreGive(s_sd_mutex);
}

esp_err_t sd_logger_enqueue_telemetry(const char *device_id, float value, uint32_t timestamp)
{
    if (!s_sd_log_queue) return ESP_ERR_INVALID_STATE;

    sd_log_item_t item;
    item.record_type = SYNC_REC_TYPE_TELEMETRY;
    item.flags = 0;
    item.timestamp = timestamp ? timestamp : (uint32_t)(esp_timer_get_time() / 1000000ULL);
    strncpy(item.device_id, device_id ? device_id : "unknown", sizeof(item.device_id) - 1);
    item.device_id[sizeof(item.device_id) - 1] = '\0';
    item.value = value;
    item.valid = true;

    if (xQueueSend(s_sd_log_queue, &item, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "s_sd_log_queue full! Dropped telemetry for '%s'", item.device_id);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t sd_logger_enqueue_event(uint8_t record_type, const char *device_id, float value, uint32_t timestamp)
{
    if (!s_sd_log_queue) return ESP_ERR_INVALID_STATE;

    sd_log_item_t item;
    item.record_type = record_type;
    item.flags = 0;
    item.timestamp = timestamp ? timestamp : (uint32_t)(esp_timer_get_time() / 1000000ULL);
    strncpy(item.device_id, device_id ? device_id : "sys_event", sizeof(item.device_id) - 1);
    item.device_id[sizeof(item.device_id) - 1] = '\0';
    item.value = value;
    item.valid = true;

    if (xQueueSend(s_sd_log_queue, &item, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGW(TAG, "s_sd_log_queue full! Dropped event %d", record_type);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void sd_logger_trigger_backfill(void)
{
    if (!s_sd_mounted || !mqtt_adapter_is_connected()) {
        return;
    }

    if (s_sd_mutex) xSemaphoreTake(s_sd_mutex, portMAX_DELAY);

    FILE *f = fopen(SD_SYNC_QUEUE_PATH, "r+b");
    if (!f) {
        if (s_sd_mutex) xSemaphoreGive(s_sd_mutex);
        return;
    }

    sync_queue_header_t hdr;
    ensure_sync_queue_header(f, &hdr);

    if (hdr.unacked_count > 0 && hdr.read_offset < hdr.write_offset) {
        ESP_LOGI(TAG, "[BACKFILL] Starting sync backlog (%lu unacked records)...",
                 (unsigned long)hdr.unacked_count);

        uint32_t processed = 0;
        while (processed < SD_BACKFILL_BATCH_SIZE && hdr.read_offset < hdr.write_offset) {
            fseek(f, hdr.read_offset, SEEK_SET);
            sync_queue_record_t rec;
            if (fread(&rec, 1, sizeof(sync_queue_record_t), f) != sizeof(sync_queue_record_t)) {
                break;
            }

            uint32_t expected_crc = calculate_crc32((const uint8_t *)&rec, 168);
            if (rec.crc32 == expected_crc) {
                // Publish bản tin telemetry bù lên MQTT broker
                char topic[64];
                snprintf(topic, sizeof(topic), "gateway/local/telemetry/batch");
                mqtt_adapter_publish(topic, rec.payload);
            } else {
                ESP_LOGW(TAG, "[BACKFILL] Record at offset %lu corrupt, skipping", (unsigned long)hdr.read_offset);
            }

            hdr.read_offset += sizeof(sync_queue_record_t);
            if (hdr.unacked_count > 0) hdr.unacked_count--;
            processed++;
        }

        // Cập nhật lại header sau batch
        hdr.crc32 = calculate_crc32((const uint8_t *)&hdr, 20);
        fseek(f, 0, SEEK_SET);
        fwrite(&hdr, 1, sizeof(sync_queue_header_t), f);
        fflush(f);

        ESP_LOGI(TAG, "[BACKFILL] Backfilled %lu records (Remaining unacked: %lu)",
                 (unsigned long)processed, (unsigned long)hdr.unacked_count);
    }

    fclose(f);
    if (s_sd_mutex) xSemaphoreGive(s_sd_mutex);
}

static void sd_log_task(void *pvParameters)
{
    ESP_LOGI(TAG, "SD Card Logger Task running on Core %d (Prio %d)", xPortGetCoreID(), SD_TASK_PRIO);

    sd_log_item_t item;
    int items_since_flush = 0;

    while (1) {
        // Báo cáo nhịp tim định kỳ tới Supervisor (FSD §4.13.2)
        supervisor_report_heartbeat(TASK_HEARTBEAT_SD_LOGGER);

        if (xQueueReceive(s_sd_log_queue, &item, pdMS_TO_TICKS(1000)) == pdTRUE) {
            if (s_sd_mounted) {
                append_telemetry_csv(&item);
                append_sync_queue_record(&item);
                items_since_flush++;
            }
        }

        // Định kỳ kiểm tra backfill nếu đang kết nối MQTT
        if (mqtt_adapter_is_connected()) {
            sd_logger_trigger_backfill();
        }
    }
}

static void on_ble_device_data(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(ble_telemetry_t)) {
        const ble_telemetry_t *t = (const ble_telemetry_t *)evt->data;
        sd_logger_enqueue_telemetry(t->device_id, t->value, t->timestamp);
    }
}

static void on_mqtt_connected(const gateway_event_t *evt)
{
    (void)evt;
    sd_logger_trigger_backfill();
}

esp_err_t sd_logger_remount(void)
{
    if (s_sd_mounted && s_sd_card != NULL) {
        esp_vfs_fat_sdcard_unmount(SDCARD_MOUNT_POINT, s_sd_card);
        s_sd_card = NULL;
        s_sd_mounted = false;
        ESP_LOGI(TAG, "Unmounted previous MicroSD card.");
    }

    // 1. Pull-up nội cho các chân SPI để ổn định giao tiếp trên breadboard
    gpio_set_pull_mode(SD_PIN_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_PIN_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_PIN_CLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(SD_PIN_CS, GPIO_PULLUP_ONLY);

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 4000; // 4MHz cho breadboard

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = SD_PIN_CS;
    slot_config.host_id = SPI2_HOST;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    esp_err_t ret = ESP_FAIL;
    for (int retry = 1; retry <= 3; retry++) {
        ESP_LOGI(TAG, "Attempting MicroSD mount (attempt %d/3, freq: %d kHz)...", retry, host.max_freq_khz);
        ret = esp_vfs_fat_sdspi_mount(SDCARD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_sd_card);
        if (ret == ESP_OK) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(150));
    }

    if (ret == ESP_OK) {
        s_sd_mounted = true;
        ESP_LOGI(TAG, "MicroSD card mounted successfully at %s!", SDCARD_MOUNT_POINT);
        mkdir("/sdcard/data", 0755);
        mkdir(SD_TELEMETRY_DIR, 0755);
        mkdir(SD_LOGS_DIR, 0755);
    } else {
        s_sd_mounted = false;
        ESP_LOGW(TAG, "MicroSD mount failed (%s / 0x%x). System running with local SPIFFS fallback.",
                 esp_err_to_name(ret), ret);
        if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "HINT: 0x107 (TIMEOUT) usually means:\n"
                          "  1. Card needs FAT32 format (Windows: Right-click -> Format -> File System: FAT32).\n"
                          "  2. MicroSD module power: If module has 3.3V LDO (e.g. AMS1117 chip), connect VCC to 5V/VIN.\n"
                          "  3. Check pin wiring: CS=10, MOSI=11, SCK/CLK=12, MISO=13.");
        }
    }

    // Thông báo trạng thái SD lên Event Bus
    sd_status_t *status = (sd_status_t *)malloc(sizeof(sd_status_t));
    if (status) {
        status->mounted = s_sd_mounted;
        status->free_kb = s_sd_mounted ? 1000000 : 0;
        status->total_kb = s_sd_mounted ? 2000000 : 0;
        event_bus_post(EVT_SD_STATUS_CHANGED, status, sizeof(sd_status_t), false);
    }

    return ret;
}

void sd_logger_init(void)
{
    ESP_LOGI(TAG, "Initializing MicroSD SPI2_HOST (CS=10, MOSI=11, CLK=12, MISO=13)...");

    if (s_sd_mutex == NULL) {
        s_sd_mutex = xSemaphoreCreateMutex();
    }
    if (s_sd_log_queue == NULL) {
        s_sd_log_queue = xQueueCreate(SD_QUEUE_DEPTH, sizeof(sd_log_item_t));
    }

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = SD_PIN_MOSI,
        .miso_io_num = SD_PIN_MISO,
        .sclk_io_num = SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus initialize returned %s", esp_err_to_name(ret));
    }

    sd_logger_remount();

    // Đăng ký nhận sự kiện từ Event Bus
    event_bus_register_handler(EVT_BLE_DEVICE_DATA, on_ble_device_data);
    event_bus_register_handler(EVT_MQTT_CONNECTED, on_mqtt_connected);

    // 2. Tạo Task SD Logger trên Core 1, Priority 4, Stack 4096 (FSD §4.11.1)
    xTaskCreatePinnedToCore(
        sd_log_task,
        "sd_log_task",
        4096,
        NULL,
        SD_TASK_PRIO,
        &s_sd_task_handle,
        SD_TASK_CORE
    );

    ESP_LOGI(TAG, "MicroSD Logger Subsystem initialized successfully (Queue depth: %d)", SD_QUEUE_DEPTH);
}
