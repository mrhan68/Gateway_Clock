#ifndef WIFI_PROVISIONING_H
#define WIFI_PROVISIONING_H

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo hệ thống Wi-Fi (FSD §4.4)
 */
void wifi_provisioning_init(void);

/**
 * @brief Kiểm tra xem thiết bị đã được cấu hình Wi-Fi trong NVS chưa
 */
bool wifi_provisioning_is_configured(void);

/**
 * @brief Bắt buộc xóa cấu hình Wi-Fi và quay về SoftAP Captive Portal
 */
void wifi_provisioning_force_reset(void);

/**
 * @brief Kết nối tới mạng Wi-Fi với SSID và Password cụ thể
 */
esp_err_t wifi_provisioning_connect(const char *ssid, const char *password);

/**
 * @brief Lấy trạng thái kết nối Wi-Fi hiện tại
 */
bool wifi_provisioning_is_connected(void);

/**
 * @brief Kiểm tra xem SoftAP đang phát sóng hay không
 */
bool wifi_provisioning_is_softap(void);

/**
 * @brief Lấy địa chỉ IP hiện tại (chuỗi)
 */
const char* wifi_provisioning_get_ip(void);

/**
 * @brief Quét danh sách các mạng Wi-Fi lân cận và in ra console
 */
void wifi_provisioning_scan_and_print(void);

/**
 * @brief Quét danh sách các mạng Wi-Fi và trả về chuỗi JSON cho Web App
 * @param json_buf Buffer lưu JSON array
 * @param max_len Kích thước buffer
 * @return Số byte đã ghi vào buffer
 */
int wifi_provisioning_scan_to_json(char *json_buf, size_t max_len);

/**
 * @brief Lấy SSID của mạng theo số thứ tự (1-based index) từ lần quét gần nhất
 */
bool wifi_provisioning_get_scanned_ssid(int index, char *out_ssid, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // WIFI_PROVISIONING_H
