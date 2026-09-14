#ifndef APP_RTC_H
#define APP_RTC_H

#include <time.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khoi tao RTC va kiem tra DS3231 qua I2C (FSD §4.2)
 * Tranh xung dot voi rtc_init noi bo cua ESP-IDF.
 */
void app_rtc_init(void);

/**
 * @brief Lay thoi gian UNIX epoch hien tai (FSD §4.2.3)
 */
time_t app_rtc_get_time(void);

/**
 * @brief Dong bo gio tu server NTP va luu vao DS3231 neu co
 */
bool app_rtc_sync_from_ntp_and_save(void);

/**
 * @brief Thiet lap timezone cho he thong
 */
void app_rtc_set_timezone(const char *tz_string);

/**
 * @brief Lay chuoi thoi gian da duoc format (YYYY-MM-DD HH:MM:SS)
 */
void app_rtc_get_formatted_time(char *buf, size_t max_len);

#ifdef __cplusplus
}
#endif

#endif // APP_RTC_H
