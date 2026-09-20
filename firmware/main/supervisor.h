#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Các cờ Heartbeat đại diện cho 7 task production (FSD §4.13.2, Mask 0x7F)
#define TASK_HEARTBEAT_EVENT_BUS   (1 << 0)
#define TASK_HEARTBEAT_BLE         (1 << 1)
#define TASK_HEARTBEAT_UPLINK      (1 << 2)
#define TASK_HEARTBEAT_SD_LOGGER   (1 << 3)
#define TASK_HEARTBEAT_SCHEDULER   (1 << 4)
#define TASK_HEARTBEAT_DISPLAY     (1 << 5)
#define TASK_HEARTBEAT_CLI         (1 << 6)

// Cờ kiểm thử cho Mock / Test Harness giả lập ngoại vi (Bit 7)
#define TASK_HEARTBEAT_MOCK        (1 << 7)
#define TASK_HEARTBEAT_RS485       TASK_HEARTBEAT_MOCK

#define TASK_ACTIVE_MASK_PRODUCTION (0x7F)

#define SUPERVISOR_TIMEOUT_MS      15000  // 15 giây không báo nhịp tim -> Coi như task bị treo
#define SUPERVISOR_TASK_PRIO       7      // Mức ưu tiên cao nhất Core 0
#define SUPERVISOR_TASK_CORE       0

/**
 * @brief Cấu trúc chẩn đoán sự cố trong RTC Slow Memory (FSD §4.13.2)
 */
typedef struct {
    uint32_t magic;           // 0xDEADBEEF
    uint8_t  failed_task_bit; // Bit task bị treo (0..6)
    char     task_name[16];   // Tên task (vd: "display_task")
    uint32_t free_heap;       // Dung lượng heap tại thời điểm treo
    uint32_t uptime_s;        // Thời gian hoạt động trước khi chết
    uint32_t timestamp;       // UNIX epoch
} rtc_crash_dump_t;

/**
 * @brief Khởi tạo hệ thống giám sát Task Supervisor & Watchdog
 */
void supervisor_init(void);

/**
 * @brief Báo cáo nhịp tim từ một task
 * @param task_mask Bit đại diện cho task (VD: TASK_HEARTBEAT_EVENT_BUS)
 */
void supervisor_report_heartbeat(uint32_t task_mask);

/**
 * @brief Cấu hình danh sách các task đang hoạt động cần giám sát
 * @param mask Tổ hợp các cờ task (OR)
 */
void supervisor_set_active_tasks(uint32_t mask);

/**
 * @brief Lấy trạng thái sức khỏe tổng quan của hệ thống
 */
void supervisor_get_status(uint32_t *active_mask, uint32_t *alive_mask, uint32_t *free_heap);

#ifdef __cplusplus
}
#endif

#endif // SUPERVISOR_H
