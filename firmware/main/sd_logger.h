#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SDCARD_MOUNT_POINT       "/sdcard"
#define SD_PIN_CS                10
#define SD_PIN_MOSI              11
#define SD_PIN_CLK               12
#define SD_PIN_MISO              13

#define SD_SYNC_QUEUE_PATH       "/sdcard/data/sync_queue.bin"
#define SD_CRASH_LOG_PATH        "/sdcard/logs/crash.log"
#define SD_TELEMETRY_DIR         "/sdcard/data/telemetry"
#define SD_LOGS_DIR              "/sdcard/logs"

#define SYNC_QUEUE_MAGIC         0x53594E43  // "SYNC" in ASCII

// Kiểu bản ghi Store-and-Forward
#define SYNC_REC_TYPE_TELEMETRY  0x01
#define SYNC_REC_TYPE_EVENT_LOG  0x02
#define SYNC_REC_TYPE_CMD_ACK    0x03

/**
 * @brief Cấu trúc Header nhị phân Store-and-Forward Sync Queue (FSD §4.11.3, đúng 24 bytes)
 * Khớp 100% với tests/host/test_sync_queue.py
 */
typedef struct __attribute__((packed)) {
    uint32_t magic;          // 0x53594E43 ("SYNC")
    uint32_t write_offset;   // Con trỏ vị trí byte ghi tiếp theo
    uint32_t read_offset;    // Con trỏ vị trí byte đã ack
    uint32_t total_records;  // Tổng số bản ghi
    uint32_t unacked_count;  // Số bản ghi chờ gửi
    uint32_t crc32;          // Checksum CRC32 của 20 bytes đầu
} sync_queue_header_t;

/**
 * @brief Cấu trúc Bản ghi nhị phân Store-and-Forward Sync Queue (FSD §4.11.3, đúng 32 bytes)
 * Khớp 100% với tests/host/test_sync_queue.py
 */
typedef struct __attribute__((packed)) {
    uint32_t timestamp;      // 4 bytes: Unix epoch timestamp
    uint8_t  record_type;    // 1 byte: 0x01=telemetry, 0x02=event_log, 0x03=command_ack
    uint8_t  flags;          // 1 byte: bit 0: QoS1, bit 1: encrypted
    uint16_t reserved;       // 2 bytes: alignment padding
    char     device_id[16];  // 16 bytes: Định danh thiết bị (null-terminated)
    float    value;          // 4 bytes: Giá trị đo telemetry (IEEE 754 float)
    uint32_t crc32;          // 4 bytes: Checksum CRC32 của 28 bytes đầu
} sync_queue_record_t;

/**
 * @brief Khởi tạo giao tiếp MicroSD SPI2_HOST, mount FATFS và tạo sd_log_task (FSD §4.11)
 */
void sd_logger_init(void);

/**
 * @brief Kiểm tra xem thẻ MicroSD đã được mount thành công hay chưa
 */
bool sd_logger_is_mounted(void);

/**
 * @brief Đưa một điểm đo telemetry vào hàng đợi ghi bất đồng bộ (s_sd_log_queue)
 */
esp_err_t sd_logger_enqueue_telemetry(const char *device_id, float value, uint32_t timestamp);

/**
 * @brief Đưa một sự kiện vào hàng đợi ghi nhị phân Store-and-Forward
 */
esp_err_t sd_logger_enqueue_event(uint8_t record_type, const char *device_id, float value, uint32_t timestamp);

/**
 * @brief Ghi trực tiếp thông điệp chẩn đoán sự cố vào /sdcard/logs/crash.log
 */
void sd_logger_append_crash_log(const char *crash_text);

/**
 * @brief Kích hoạt tiến trình đồng bộ bù dữ liệu ngoại tuyến (Backfill) lên Cloud
 */
void sd_logger_trigger_backfill(void);

#ifdef __cplusplus
}
#endif

#endif // SD_LOGGER_H
