#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char gateway_id[32];
    char wifi_ssid[32];
    char wifi_password[64];
    char mqtt_broker_uri[128];
    uint16_t mqtt_telemetry_interval_s;
    uint32_t rs485_baudrate;
    uint16_t rs485_timeout_ms;
} gateway_config_t;

/**
 * @brief Khởi tạo NVS Flash và nạp cấu hình hệ thống
 */
esp_err_t config_manager_init(void);

/**
 * @brief Lấy con trỏ cấu hình hiện tại đang áp dụng
 */
const gateway_config_t* config_manager_get(void);

/**
 * @brief Cập nhật thông tin Wi-Fi
 */
esp_err_t config_manager_set_wifi(const char *ssid, const char *password);

/**
 * @brief Cập nhật thông tin MQTT Broker
 */
esp_err_t config_manager_set_mqtt(const char *uri, uint16_t interval_s);

/**
 * @brief Cập nhật thông số đường truyền RS485
 */
esp_err_t config_manager_set_rs485(uint32_t baudrate, uint16_t timeout_ms);

/**
 * @brief Lưu toàn bộ cấu hình vào bộ nhớ NVS Flash nội bộ
 */
esp_err_t config_manager_save(void);

/**
 * @brief In cấu hình hiện tại ra Serial Console
 */
void config_manager_print(void);

#ifdef __cplusplus
}
#endif

#endif // CONFIG_MANAGER_H
