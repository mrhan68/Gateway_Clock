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

/**
 * @brief Gateway Central Event Codes (FSD v1.1 §4.1.4)
 */
typedef enum {
    EVT_NONE = 0,
    EVT_WIFI_PROVISIONING_STARTED,
    EVT_WIFI_CREDENTIALS_RECEIVED,
    EVT_WIFI_CONNECTED,
    EVT_WIFI_LOST,
    EVT_BLE_DEVICE_DATA,
    EVT_BLE_COMMAND_REQUEST,
    EVT_DEVICE_OFFLINE_ALERT,
    EVT_SD_STATUS_CHANGED,
    EVT_CONFIG_RELOAD_REQUEST,
    EVT_MQTT_MESSAGE_RECEIVED,
    EVT_MQTT_CONNECTED,
    EVT_MQTT_DISCONNECTED,
    EVT_DEVICE_COMMAND_REQUEST,
    EVT_RULE_ACTION_REQUEST,
    EVT_COMMAND_RESPONSE,
    EVT_RELAY_STATE_CHANGED,
    EVT_SCHEDULE_AUTOMATION_TRIGGERED,
    EVT_SCHEDULE_ALARM_TRIGGERED,
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

// Deprecated aliases for backwards compatibility
#define EVT_REMOTE_COMMAND_REQUEST EVT_DEVICE_COMMAND_REQUEST
#define EVT_RS485_DEVICE_DATA      EVT_NONE
#define EVT_RS485_COMMAND_REQUEST  EVT_NONE

/* ========================================================================= */
/* Shared Payload Structs (FSD v1.1 §4.1.5)                                   */
/* ========================================================================= */

/**
 * @brief BLE Telemetry structure (FSD §4.1.5)
 */
typedef struct {
    char     device_id[32]; // Mã thiết bị (VD: "sensor_temp_01")
    union {
        float temperature;  // Nhiệt độ (°C)
        float value;        // Giá trị đo (tương thích ngược)
    };
    float    humidity;      // Độ ẩm (%RH)
    float    battery_pct;   // Mức pin (0 - 100%)
    bool     valid;         // false -> rule engine PHẢI áp dụng safe-default
    uint32_t timestamp;     // Thời gian đọc (UNIX epoch từ RTC)
} ble_telemetry_t;

/**
 * @brief Relay State Change Event Payload (FSD §4.1.5)
 */
typedef struct {
    int      relay_id;             // 1: Fan, 2: Heater, 3: Dehumidifier, 4: Aux
    char     device_id[32];        // "relay_1", "relay_2", ...
    bool     state;                // true: ON, false: OFF
    char     reason[32];           // "MANUAL_CMD", "RULE_TRIGGER", "SAFETY_CUTOFF", "SENSOR_OFFLINE"
    uint32_t timestamp;            // Epoch
} relay_state_changed_t;

/**
 * @brief Device Command Request Payload (FSD §4.1.5)
 */
typedef struct {
    char     command_id[36];       // UUID v4 định danh lệnh (chống trùng lặp & đối soát ACK)
    char     target_device_id[32]; // Thiết bị nhận lệnh
    char     action[32];           // "ON", "OFF", "SET:75", "TOGGLE"
    uint32_t ttl_seconds;          // Thời hạn hiệu lực của lệnh (mặc định 60s)
    uint32_t timestamp;            // Epoch thời điểm phát lệnh
} device_command_req_t;

/**
 * @brief Command Response / ACK Payload (FSD §4.1.5)
 */
typedef struct {
    char     command_id[36];       // Khớp command_id của yêu cầu gửi xuống
    char     target_device_id[32]; // Thiết bị thực thi
    bool     success;              // true nếu thực thi thành công
    int      error_code;           // 0: OK, 1: TIMEOUT, 2: INVALID_CMD, 3: EXPIRED
    char     message[64];          // Mô tả kết quả thực thi
    uint32_t executed_at;          // Thời điểm thực thi xong
} command_ack_t;

/**
 * @brief Rule Action Request Payload (FSD §4.1.5)
 */
typedef struct {
    char     command_id[36];       // Tự sinh UUID cho action do Rule kích hoạt
    char     target_device_id[32]; // Adapter tự tra cứu danh sách thiết bị
    union {
        char command[32];          // VD: "ON", "OFF", "SET:75"
        char action_command[32];   // Tương thích ngược với rule_engine cũ
    };
    uint32_t timestamp;            // Epoch kích hoạt
    int      source_rule_id;       // Source rule ID
} rule_action_request_t;

/**
 * @brief Configuration Update Notification Payload (FSD §4.1.5)
 */
typedef struct {
    bool schedule_changed;         // schedule.json đã được cập nhật từ Cloud
    bool rules_changed;            // rules.json đã được cập nhật từ Cloud
    bool ble_devices_changed;      // device_list.json đã được cập nhật
} config_updated_t;

/**
 * @brief Device Offline / Alert Payload (FSD §4.1.5)
 */
typedef struct {
    char     device_id[32];        // Mã thiết bị
    char     alert_type[32];       // Loại cảnh báo (VD: "OFFLINE", "LOW_BATTERY")
    uint32_t timestamp;            // Epoch
} device_alert_t;

/**
 * @brief SD Card Status Payload (FSD §4.1.5)
 */
typedef struct {
    bool     mounted;              // true nếu thẻ nhớ đã mount
    uint32_t free_kb;              // Dung lượng trống (KB)
    uint32_t total_kb;             // Tổng dung lượng (KB)
} sd_status_t;

/* ========================================================================= */
/* Event Bus Core Structures and APIs                                        */
/* ========================================================================= */

// Cấu trúc một event trên bus
typedef struct {
    gateway_event_code_t code;
    void *data;        // Con trỏ tới payload được malloc bởi producer
    size_t len;        // Kích thước payload
    uint32_t timestamp;// Thời điểm sinh event
} gateway_event_t;

// Kiểu hàm callback xử lý event
typedef void (*gateway_event_handler_t)(const gateway_event_t *evt);

#define EVENT_BUS_QUEUE_LEN       32
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

/**
 * @brief Freeze the event bus dispatcher indefinitely (FR-SUP-02 test hook)
 */
void event_bus_freeze(void);

#ifdef __cplusplus
}
#endif

#endif // EVENT_BUS_H
