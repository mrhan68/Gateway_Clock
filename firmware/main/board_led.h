#ifndef BOARD_LED_H
#define BOARD_LED_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Các chân GPIO thường gắn LED RGB (WS2812 / discrete) trên các bo mạch ESP32-S3
#define BOARD_LED_PIN_DEVKITC1    48  // ESP32-S3-DevKitC-1 & YD-ESP32-S3 RGB (WS2812)
#define BOARD_LED_PIN_ALT1        38  // Một số phiên bản S3 DevKit / LilyGO
#define BOARD_LED_PIN_ZERO        21  // ESP32-S3-Zero / SuperMini (WS2812 / User LED)
#define BOARD_LED_PIN_USER_BLUE    2  // User Blue LED trên YD-ESP32-S3
#define BOARD_LED_PIN_ALT2        47  // AI-Thinker / Generic S3
#define BOARD_LED_PIN_ALT3        14  // Stamp-S3


/**
 * @brief Tắt toàn bộ LED RGB trên bo mạch (hỗ trợ cả WS2812 NeoPixel và discrete LED)
 */
void board_led_all_off(void);

/**
 * @brief Đặt màu cho LED WS2812 trên chân chỉ định (G, R, B)
 * @param gpio Chân GPIO kết nối LED
 * @param r Độ sáng Đỏ (0..255)
 * @param g Độ sáng Xanh lá (0..255)
 * @param b Độ sáng Xanh dương (0..255)
 */
void board_led_set_rgb(gpio_num_t gpio, uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Khởi tạo module LED bo mạch và tắt ngay lập tức khi khởi động
 */
void board_led_init(void);

#ifdef __cplusplus
}
#endif

#endif // BOARD_LED_H
