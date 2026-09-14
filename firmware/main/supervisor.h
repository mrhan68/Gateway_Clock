#ifndef SUPERVISOR_H
#define SUPERVISOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Các cờ Heartbeat đại diện cho từng task trong hệ thống
#define TASK_HEARTBEAT_EVENT_BUS   (1 << 0)
#define TASK_HEARTBEAT_RS485       (1 << 1)
#define TASK_HEARTBEAT_UPLINK      (1 << 2)
#define TASK_HEARTBEAT_SD_LOGGER   (1 << 3)
#define TASK_HEARTBEAT_SCHEDULER   (1 << 4)
#define TASK_HEARTBEAT_DISPLAY     (1 << 5)
#define TASK_HEARTBEAT_CLI         (1 << 6)

#define SUPERVISOR_TIMEOUT_MS      15000  // 15 giây không báo nhịp tim -> Coi như task bị treo
#define SUPERVISOR_TASK_PRIO       7      // Mức ưu tiên cao nhất Core 0
#define SUPERVISOR_TASK_CORE       0

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
