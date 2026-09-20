#ifndef STORAGE_H
#define STORAGE_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPIFFS_BASE_PATH         "/spiffs"
#define SPIFFS_PARTITION_LABEL   "storage"

#define STORAGE_SCHEDULE_PATH    "/spiffs/schedule.json"
#define STORAGE_DEVICE_LIST_PATH "/spiffs/device_list.json"
#define STORAGE_EVENT_LOG_PATH   "/spiffs/event_log.json"
#define STORAGE_RULES_PATH       "/spiffs/rules.json"
#define STORAGE_CONFIG_PATH      "/spiffs/config.json"

/**
 * @brief Khởi tạo phân vùng SPIFFS tại /spiffs, tạo semaphore và cloud_sync_task (FSD §4.3.3)
 */
void storage_init(void);

/**
 * @brief Lưu chuỗi JSON lịch biểu vào /spiffs/schedule.json
 */
bool storage_save_schedule_json(const char *json_str);

/**
 * @brief Đọc nội dung /spiffs/schedule.json
 * @return Con trỏ chuỗi heap (caller PHẢI free), hoặc "[]" nếu file rỗng/không tồn tại
 */
char *storage_load_schedule_json(void);

/**
 * @brief Lưu chuỗi JSON danh sách thiết bị vào /spiffs/device_list.json
 */
bool storage_save_device_list_json(const char *json_str);

/**
 * @brief Đọc nội dung /spiffs/device_list.json
 * @return Con trỏ chuỗi heap (caller PHẢI free)
 */
char *storage_load_device_list_json(void);

/**
 * @brief Ghi nối tiếp 1 dòng JSON vào /spiffs/event_log.json và phát EVT_EVENT_LOG_APPENDED
 */
bool storage_append_event_log(const char *event_json_line);

/**
 * @brief Đọc toàn bộ các sự kiện chưa đồng bộ từ /spiffs/event_log.json
 * @return Chuỗi JSON array (caller PHẢI free)
 */
char *storage_get_pending_event_log(void);

/**
 * @brief Xóa các dòng log đã đồng bộ thành công lên Cloud
 */
void storage_clear_synced_event_log(void);

/**
 * @brief Kích hoạt tiến trình đồng bộ Cloud (Cloud Sync) ngay lập tức
 */
void storage_trigger_cloud_sync(void);

/**
 * @brief Kiểm tra xem phân vùng SPIFFS đã được mount thành công chưa
 */
bool storage_is_mounted(void);

#ifdef __cplusplus
}
#endif

#endif // STORAGE_H
