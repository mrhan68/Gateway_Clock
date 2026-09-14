#ifndef EVENT_BUS_H
#define EVENT_BUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// Mã sự kiện hệ thống (đồng bộ với FSD v1.1 §4.1.4)
typedef enum {
    EVT_NONE = 0,
    EVT_WIFI_PROVISIONING_STARTED,
    EVT_WIFI_CREDENTIALS_RECEIVED,
    EVT_WIFI_CONNECTED,
    EVT_WIFI_LOST,
    EVT_RS485_DEVICE_DATA,
    EVT_RS485_COMMAND_REQUEST,
    EVT_DEVICE_OFFLINE_ALERT,
    EVT_SD_STATUS_CHANGED,
    EVT_CONFIG_RELOAD_REQUEST,
    EVT_BLE_DEVICE_DATA,
    EVT_MQTT_MESSAGE_RECEIVED,
    EVT_MQTT_CONNECTED,
    EVT_MQTT_DISCONNECTED,
    EVT_REMOTE_COMMAND_REQUEST,
    EVT_COMMAND_RESPONSE,
    EVT_SCHEDULE_AUTOMATION_TRIGGERED,
    EVT_SCHEDULE_ALARM_TRIGGERED,
    EVT_RULE_ACTION_REQUEST,
    EVT_SENSOR_VALUE_INVALID,
    EVT_BUTTON_ALARM_STOP,
    EVT_BUTTON_SCREEN_NEXT,
    EVT_DISPLAY_UPDATE_REQUEST,
    EVT_CONFIG_UPDATED_FROM_CLOUD,
    EVT_EVENT_LOG_APPENDED,
    EVT_SYNC_COMPLETED,
    EVT_SYSTEM_HEARTBEAT,
    EVT_MAX_CODES
} gateway_event_code_t;

// Cấu trúc một event trên bus
typedef struct {
    gateway_event_code_t code;
    void *data;        // Con trỏ tới payload được malloc bởi producer
    size_t len;        // Kích thước payload
    uint32_t timestamp;// Thời điểm sinh event
} gateway_event_t;

// Kiểu hàm callback xử lý event
typedef void (*gateway_event_handler_t)(const gateway_event_t *evt);

#define EVENT_BUS_QUEUE_LEN       20
#define EVENT_BUS_MAX_HANDLERS     4
#define EVENT_BUS_DISPATCHER_PRIO  6
#define EVENT_BUS_DISPATCHER_CORE  0

/**
 * @brief Khởi tạo hàng đợi Event Bus
 */
void event_bus_init(void);

/**
 * @brief Đăng ký hàm xử lý cho một event code cụ thể
 */
bool event_bus_register_handler(gateway_event_code_t code, gateway_event_handler_t handler);

/**
 * @brief Đẩy một event vào hàng đợi bus
 * @param code Mã event
 * @param data Con trỏ dữ liệu heap (malloc). Dispatcher sẽ free() sau khi gọi hết handler!
 * @param len Kích thước payload
 * @param from_isr true nếu gọi từ trong ngắt GPIO/Timer ISR
 */
BaseType_t event_bus_post(gateway_event_code_t code, void *data, size_t len, bool from_isr);

/**
 * @brief Bắt đầu task điều phối (Dispatcher Task)
 * Gọi SAU CÙNG khi tất cả module đã đăng ký handler
 */
void event_bus_start_dispatcher(void);

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
