#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Định nghĩa chân GPIO phần cứng rơ-le (FSD §2.3)
 */
#define RELAY_1_GPIO    15  // Quạt làm mát (Fan)
#define RELAY_2_GPIO    16  // Máy sưởi nhiệt (Heater)
#define RELAY_3_GPIO    17  // Máy hút ẩm (Dehumidifier)
#define RELAY_4_GPIO    18  // Đèn / Tải phụ (Aux)

#define RELAY_COUNT     4

/**
 * @brief Giới hạn thời gian bật liên tục tối đa (FSD §4.10.2)
 */
#define RELAY_1_MAX_ON_SEC   (4 * 3600)  // 4 giờ (14400s)
#define RELAY_2_MAX_ON_SEC   (30 * 60)   // 30 phút (1800s) - Emergency Cutoff
#define RELAY_3_MAX_ON_SEC   (60 * 60)   // 60 phút (3600s)
#define RELAY_4_MAX_ON_SEC   0           // Không giới hạn

/**
 * @brief Thời gian trễ khóa liên động Quạt -> Sưởi (FSD §4.10.3)
 */
#define RELAY_INTERLOCK_DELAY_MS  500

#define ERR_RELAY_INTERLOCK_CONFLICT  3
#define ERR_RELAY_DWELL_LOCKED        4
#define ERR_RELAY_LOCKOUT_ACTIVE      5

/**
 * @brief Cấu hình an toàn mức GPIO vật lý (Boot Step 2, trước Event Bus)
 * Kéo GPIO 15..18 xuất mức LOW an toàn với trở kéo xuống 10kΩ.
 */
void relay_gpio_init(void);

/**
 * @brief Khởi tạo module điều khiển rơ-le và cấu hình an toàn phần cứng (FSD §4.10.5, Boot Step 5)
 * Đăng ký nhận EVT_DEVICE_COMMAND_REQUEST, EVT_RULE_ACTION_REQUEST, EVT_SENSOR_VALUE_INVALID, EVT_BLE_DEVICE_DATA, EVT_DEVICE_OFFLINE_ALERT.
 */
void relay_controller_init(void);

/**
 * @brief Điều khiển trạng thái rơ-le với bảo vệ khóa liên động và phản hồi ACK (FSD §4.10.5)
 * @param target_id Tên kênh hoặc alias (relay_1, fan_01, heater_01, ...)
 * @param state true: BẬT (ON), false: TẮT (OFF)
 * @param cmd_id UUID định danh lệnh nếu đến từ Cloud/CLI (hoặc NULL nếu tự động)
 * @param source Nguồn kích hoạt ("MANUAL_CMD", "RULE_TRIGGER", "SAFETY_CUTOFF", ...)
 * @return ESP_OK nếu thực thi thành công
 */
esp_err_t relay_controller_set_state(const char *target_id, bool state, const char *cmd_id, const char *source);

/**
 * @brief Đọc mức logic hiện tại của rơ-le (1..4) (FSD §4.10.5)
 * @param relay_id Kênh rơ-le từ 1 đến 4
 * @return true nếu đang BẬT, false nếu đang TẮT
 */
bool relay_controller_get_state(int relay_id);

/**
 * @brief Ngắt khẩn cấp toàn bộ 4 rơ-le về mức LOW (FSD §4.10.5)
 */
void relay_controller_emergency_shutdown_all(void);

/**
 * @brief Lấy thời gian bật liên tục hiện tại của rơ-le (giây)
 * @param relay_id Kênh rơ-le 1..4
 * @return Số giây đang bật liên tục, 0 nếu đang tắt
 */
uint32_t relay_controller_get_on_duration(int relay_id);

#ifdef __cplusplus
}
#endif

#endif // RELAY_CONTROLLER_H
