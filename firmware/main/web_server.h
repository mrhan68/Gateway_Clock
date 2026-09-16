#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <esp_err.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi động HTTP Web Server tích hợp Dashboard và toàn bộ REST API
 */
esp_err_t web_server_start(void);

/**
 * @brief Dừng HTTP Web Server
 */
void web_server_stop(void);

/**
 * @brief Kiểm tra trạng thái hoạt động của Web Server
 */
bool web_server_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // WEB_SERVER_H
