#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_SAVED_WIFI 5

typedef struct {
    char ssid[32];
    char password[64];
} wifi_profile_t;

typedef struct {
    char gateway_id[32];
    char timezone[16];
    char wifi_ssid[32];
    char wifi_password[64];
    char softap_password[64];
    char mqtt_broker_uri[128];
    char mqtt_username[64];
    char mqtt_password[64];
    uint16_t mqtt_telemetry_interval_s;
    uint8_t saved_wifi_count;
    wifi_profile_t saved_wifi[MAX_SAVED_WIFI];
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
 * @brief Lấy số lượng mạng Wi-Fi đã lưu trong NVS
 */
uint8_t config_manager_get_saved_wifi_count(void);

/**
 * @brief Lấy thông tin mạng Wi-Fi đã lưu theo chỉ số (0..count-1)
 */
bool config_manager_get_saved_wifi(uint8_t index, char *out_ssid, size_t ssid_len, char *out_pass, size_t pass_len);

/**
 * @brief Thêm mới hoặc cập nhật mật khẩu mạng Wi-Fi vào danh sách NVS
 */
esp_err_t config_manager_add_or_update_wifi(const char *ssid, const char *password);

/**
 * @brief Xóa mạng Wi-Fi khỏi danh sách đã lưu trong NVS
 */
esp_err_t config_manager_delete_wifi(const char *ssid);

/**
 * @brief Cập nhật mật khẩu WPA2-PSK cho SoftAP (FSD §4.4.6, mặc định "")
 */
esp_err_t config_manager_set_softap_password(const char *password);

/**
 * @brief Factory Reset: Xóa sạch Wi-Fi credentials trong NVS, đặt lại SoftAP pass về "" (FSD §4.4 WP-07)
 */
esp_err_t config_manager_factory_reset(void);

/**
 * @brief Cập nhật thông tin MQTT Broker
 */
esp_err_t config_manager_set_mqtt(const char *uri, uint16_t interval_s);

/**
 * @brief Cập nhật chuỗi POSIX Timezone (VD: "ICT-7")
 */
esp_err_t config_manager_set_timezone(const char *tz);

/**
 * @brief Nạp cấu hình từ tệp JSON trên hệ thống tệp (LittleFS / SD card)
 * @param filepath Đường dẫn tệp (mặc định "/storage/config.json" nếu truyền NULL)
 */
esp_err_t config_manager_load_from_file(const char *filepath);

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
