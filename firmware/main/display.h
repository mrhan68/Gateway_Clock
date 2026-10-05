#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Định nghĩa chân phần cứng giao tiếp giao diện người dùng (FSD §2.3)
#define DISPLAY_I2C_PORT        0
#define DISPLAY_I2C_SDA_GPIO    8
#define DISPLAY_I2C_SCL_GPIO    9
#define DISPLAY_I2C_ADDR        0x3C
#define DISPLAY_I2C_FREQ_HZ     400000

#define DISPLAY_BUZZER_GPIO     1
#define DISPLAY_BTN_BOOT_GPIO   0
#define DISPLAY_BTN_SCREEN_GPIO 4

#define DISPLAY_TASK_PRIO       3
#define DISPLAY_TASK_CORE       0

// Danh mục các trang màn hình luân phiên (FSD §4.9.3 & DI-07)
typedef enum {
    DISPLAY_SCREEN_CLOCK_TEMP = 0,
    DISPLAY_SCREEN_UPCOMING_EVENT,
    DISPLAY_SCREEN_DEVICE_STATUS,
    DISPLAY_SCREEN_PROVISIONING,
    DISPLAY_SCREEN_COUNT
} display_screen_index_t;

/**
 * @brief Khởi tạo hệ thống hiển thị OLED, Buzzer và Nút bấm (FSD §4.9 & KPI-BOOT-01)
 * Vẽ màn hình đồng hồ ngay lập tức (<1.5s) và tạo display_task trên Core 0, Prio 3.
 */
void display_init(void);

/**
 * @brief Kết xuất (Render) nội dung màn hình hiện hành ra OLED / Virtual Buffer
 */
void display_render(void);

/**
 * @brief Chuyển sang màn hình cụ thể (0..2)
 */
void display_set_screen(uint8_t screen_index);

/**
 * @brief Kích hoạt chuông và giao diện báo thức đè (Alarm Overlay) (FSD §4.9.4)
 * @param title Tiêu đề báo thức
 * @param buzzer true nếu bật còi báo động PWM
 */
void display_trigger_alarm(const char *title, bool buzzer);

/**
 * @brief Tắt báo thức và dừng chuông còi ngay lập tức
 */
void display_stop_alarm(void);

/**
 * @brief Hoãn báo thức (Snooze) thêm số phút quy định (mặc định +9m)
 */
void display_snooze_alarm(uint8_t minutes);

/**
 * @brief Kiểm tra trạng thái báo thức hiện tại
 */
bool display_is_alarm_active(void);

#ifdef __cplusplus
}
#endif

#endif // DISPLAY_H
