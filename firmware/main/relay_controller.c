#include "relay_controller.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "event_bus.h"

static const char *TAG = "RELAY_CTRL";

static const gpio_num_t s_relay_pins[RELAY_COUNT] = {
    RELAY_1_GPIO,
    RELAY_2_GPIO,
    RELAY_3_GPIO,
    RELAY_4_GPIO
};

static const uint32_t s_relay_max_on_sec[RELAY_COUNT] = {
    RELAY_1_MAX_ON_SEC,
    RELAY_2_MAX_ON_SEC,
    RELAY_3_MAX_ON_SEC,
    RELAY_4_MAX_ON_SEC
};

static bool s_relay_states[RELAY_COUNT] = {false, false, false, false};
static uint64_t s_relay_on_timestamp[RELAY_COUNT] = {0, 0, 0, 0};
static uint64_t s_last_valid_sensor_time = 0;
static SemaphoreHandle_t s_relay_mutex = NULL;
static esp_timer_handle_t s_safety_timer = NULL;

static int map_target_to_relay_id(const char *target_id)
{
    if (!target_id) return 0;
    if (strcasecmp(target_id, "relay_1") == 0 ||
        strcasecmp(target_id, "fan_01") == 0 ||
        strcasecmp(target_id, "fan_relay_01") == 0 ||
        strcasecmp(target_id, "fan") == 0) {
        return 1;
    }
    if (strcasecmp(target_id, "relay_2") == 0 ||
        strcasecmp(target_id, "heater_01") == 0 ||
        strcasecmp(target_id, "heater_relay_01") == 0 ||
        strcasecmp(target_id, "heater") == 0) {
        return 2;
    }
    if (strcasecmp(target_id, "relay_3") == 0 ||
        strcasecmp(target_id, "dehum_01") == 0 ||
        strcasecmp(target_id, "dehumidifier_01") == 0 ||
        strcasecmp(target_id, "dehum") == 0 ||
        strcasecmp(target_id, "dehumidifier") == 0) {
        return 3;
    }
    if (strcasecmp(target_id, "relay_4") == 0 ||
        strcasecmp(target_id, "aux_01") == 0 ||
        strcasecmp(target_id, "light_01") == 0 ||
        strcasecmp(target_id, "aux") == 0 ||
        strcasecmp(target_id, "light") == 0) {
        return 4;
    }
    return 0;
}

static void post_relay_state_changed_event(int relay_id, bool state, const char *reason)
{
    relay_state_changed_t *payload = (relay_state_changed_t *)malloc(sizeof(relay_state_changed_t));
    if (payload != NULL) {
        payload->relay_id = relay_id;
        snprintf(payload->device_id, sizeof(payload->device_id), "relay_%d", relay_id);
        payload->state = state;
        strncpy(payload->reason, reason ? reason : "MANUAL_CMD", sizeof(payload->reason) - 1);
        payload->reason[sizeof(payload->reason) - 1] = '\0';
        payload->timestamp = (uint32_t)(esp_timer_get_time() / 1000000ULL);

        event_bus_post(EVT_RELAY_STATE_CHANGED, payload, sizeof(relay_state_changed_t), false);
    }
}

static void post_command_ack_event(const char *cmd_id, int relay_id, bool success, int err_code, const char *msg)
{
    if (cmd_id == NULL || cmd_id[0] == '\0') {
        return;
    }

    command_ack_t *ack = (command_ack_t *)malloc(sizeof(command_ack_t));
    if (ack != NULL) {
        strncpy(ack->command_id, cmd_id, sizeof(ack->command_id) - 1);
        ack->command_id[sizeof(ack->command_id) - 1] = '\0';
        snprintf(ack->target_device_id, sizeof(ack->target_device_id), "relay_%d", relay_id);
        ack->success = success;
        ack->error_code = err_code;
        strncpy(ack->message, msg ? msg : "OK", sizeof(ack->message) - 1);
        ack->message[sizeof(ack->message) - 1] = '\0';
        ack->executed_at = (uint32_t)(esp_timer_get_time() / 1000000ULL);

        event_bus_post(EVT_COMMAND_RESPONSE, ack, sizeof(command_ack_t), false);
    }
}

esp_err_t relay_controller_set_state(const char *target_id, bool state, const char *cmd_id, const char *source)
{
    int relay_id = map_target_to_relay_id(target_id);
    if (relay_id < 1 || relay_id > RELAY_COUNT) {
        ESP_LOGW(TAG, "Unknown relay target identifier: '%s'", target_id ? target_id : "(null)");
        post_command_ack_event(cmd_id, 0, false, 2, "Invalid target device ID");
        return ESP_ERR_NOT_FOUND;
    }

    int idx = relay_id - 1;

    if (s_relay_mutex != NULL) {
        xSemaphoreTake(s_relay_mutex, portMAX_DELAY);
    }

    // 1. Nếu lệnh là TẮT (state == false):
    // FSD §4.10.3: Immediate shutoff! 0ms delay, bypass cooldown.
    if (!state) {
        gpio_set_level(s_relay_pins[idx], 0);
        s_relay_states[idx] = false;
        s_relay_on_timestamp[idx] = 0;

        ESP_LOGI(TAG, "Relay %d (%s) turned OFF [Reason: %s]",
                 relay_id, target_id, source ? source : "UNKNOWN");

        post_relay_state_changed_event(relay_id, false, source);
        post_command_ack_event(cmd_id, relay_id, true, 0, "Relay turned OFF successfully");

        if (s_relay_mutex != NULL) {
            xSemaphoreGive(s_relay_mutex);
        }
        return ESP_OK;
    }

    // 2. Nếu lệnh là BẬT (state == true): Áp dụng khóa liên động an toàn (Interlocking)
    if (relay_id == 2) {
        // Lệnh BẬT Sưởi (relay_2 = ON):
        // Nếu Quạt (relay_1) đang BẬT -> Cưỡng chế ngắt Quạt, trễ an toàn 500ms
        if (s_relay_states[0]) {
            ESP_LOGW(TAG, "[INTERLOCK] Turning ON Heater while Fan is active! Forcing Fan OFF and waiting 500ms...");
            gpio_set_level(s_relay_pins[0], 0);
            s_relay_states[0] = false;
            s_relay_on_timestamp[0] = 0;
            post_relay_state_changed_event(1, false, "INTERLOCK_CUTOFF");

            // Trễ 500ms để triệt tiêu dòng cảm ứng động cơ quạt trước khi đóng tiếp điểm sưởi
            vTaskDelay(pdMS_TO_TICKS(RELAY_INTERLOCK_DELAY_MS));
        }
    } else if (relay_id == 1) {
        // Lệnh BẬT Quạt (relay_1 = ON):
        // Cooling Safety Override! Ưu tiên chống quá nhiệt luôn thắng -> ngắt Sưởi ngay lập tức (0ms delay)
        if (s_relay_states[1]) {
            ESP_LOGW(TAG, "[INTERLOCK] Cooling Safety Override! Turning Heater OFF immediately (0ms delay)...");
            gpio_set_level(s_relay_pins[1], 0);
            s_relay_states[1] = false;
            s_relay_on_timestamp[1] = 0;
            post_relay_state_changed_event(2, false, "COOLING_OVERRIDE");
        }
    }

    // Xuất mức HIGH ra chân GPIO vật lý
    gpio_set_level(s_relay_pins[idx], 1);
    s_relay_states[idx] = true;
    s_relay_on_timestamp[idx] = esp_timer_get_time() / 1000000ULL;

    ESP_LOGI(TAG, "Relay %d (%s) turned ON [Reason: %s, Max Duration: %lu s]",
             relay_id, target_id, source ? source : "UNKNOWN",
             (unsigned long)s_relay_max_on_sec[idx]);

    post_relay_state_changed_event(relay_id, true, source);
    post_command_ack_event(cmd_id, relay_id, true, 0, "Relay turned ON successfully");

    if (s_relay_mutex != NULL) {
        xSemaphoreGive(s_relay_mutex);
    }
    return ESP_OK;
}

bool relay_controller_get_state(int relay_id)
{
    if (relay_id < 1 || relay_id > RELAY_COUNT) {
        return false;
    }
    return s_relay_states[relay_id - 1];
}

uint32_t relay_controller_get_on_duration(int relay_id)
{
    if (relay_id < 1 || relay_id > RELAY_COUNT) {
        return 0;
    }
    int idx = relay_id - 1;
    if (!s_relay_states[idx] || s_relay_on_timestamp[idx] == 0) {
        return 0;
    }
    uint64_t now = esp_timer_get_time() / 1000000ULL;
    return (uint32_t)(now - s_relay_on_timestamp[idx]);
}

void relay_controller_emergency_shutdown_all(void)
{
    ESP_LOGW(TAG, "[PANIC] EMERGENCY SHUTDOWN: Forcefully switching all 4 relays LOW!");
    if (s_relay_mutex != NULL) {
        xSemaphoreTake(s_relay_mutex, portMAX_DELAY);
    }

    for (int i = 0; i < RELAY_COUNT; i++) {
        gpio_set_level(s_relay_pins[i], 0);
        if (s_relay_states[i]) {
            s_relay_states[i] = false;
            s_relay_on_timestamp[i] = 0;
            post_relay_state_changed_event(i + 1, false, "EMERGENCY_SHUTDOWN");
        }
    }

    if (s_relay_mutex != NULL) {
        xSemaphoreGive(s_relay_mutex);
    }
}

static void on_device_command_request(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(device_command_req_t)) {
        return;
    }

    const device_command_req_t *req = (const device_command_req_t *)evt->data;
    int relay_id = map_target_to_relay_id(req->target_device_id);
    if (relay_id == 0) {
        return; // Không phải rơ-le, bỏ qua cho module khác xử lý
    }

    bool new_state = false;
    if (strcasecmp(req->action, "ON") == 0) {
        new_state = true;
    } else if (strcasecmp(req->action, "OFF") == 0) {
        new_state = false;
    } else if (strcasecmp(req->action, "TOGGLE") == 0) {
        new_state = !relay_controller_get_state(relay_id);
    } else {
        ESP_LOGW(TAG, "Unsupported device command action '%s' on target '%s'",
                 req->action, req->target_device_id);
        post_command_ack_event(req->command_id, relay_id, false, 2, "Unsupported action");
        return;
    }

    relay_controller_set_state(req->target_device_id, new_state, req->command_id, "MANUAL_CMD");
}

static void on_rule_action_request(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(rule_action_request_t)) {
        return;
    }

    const rule_action_request_t *act = (const rule_action_request_t *)evt->data;
    int relay_id = map_target_to_relay_id(act->target_device_id);
    if (relay_id == 0) {
        return; // Không thuộc đối tượng rơ-le
    }

    bool new_state = false;
    if (strcasecmp(act->command, "ON") == 0) {
        new_state = true;
    } else if (strcasecmp(act->command, "OFF") == 0) {
        new_state = false;
    } else if (strcasecmp(act->command, "TOGGLE") == 0) {
        new_state = !relay_controller_get_state(relay_id);
    } else {
        ESP_LOGW(TAG, "Unsupported rule action command '%s' on target '%s'",
                 act->command, act->target_device_id);
        return;
    }

    relay_controller_set_state(act->target_device_id, new_state, act->command_id, "RULE_TRIGGER");
}

static void on_sensor_value_invalid(const gateway_event_t *evt)
{
    (void)evt;
    // FSD §4.10.4: Khi cảm biến lỗi hoặc gửi cờ valid=false -> ngắt cưỡng bức Máy sưởi ngay lập tức
    if (relay_controller_get_state(2)) {
        ESP_LOGW(TAG, "[FAIL-SAFE] Invalid sensor reading detected! Forcefully turning OFF Heater.");
        relay_controller_set_state("relay_2", false, NULL, "SAFETY_CUTOFF");
    }
}

static void on_ble_device_data(const gateway_event_t *evt)
{
    if (evt == NULL || evt->data == NULL || evt->len < sizeof(ble_telemetry_t)) {
        return;
    }

    const ble_telemetry_t *telem = (const ble_telemetry_t *)evt->data;
    if (telem->valid) {
        s_last_valid_sensor_time = esp_timer_get_time() / 1000000ULL;
    } else {
        // Cảm biến báo valid = false -> kích hoạt Fail-Safe
        if (relay_controller_get_state(2)) {
            ESP_LOGW(TAG, "[FAIL-SAFE] BLE telemetry marked valid=false! Shutting down Heater.");
            relay_controller_set_state("relay_2", false, NULL, "SAFETY_CUTOFF");
        }
    }
}

static void relay_safety_timer_cb(void *arg)
{
    uint64_t now = esp_timer_get_time() / 1000000ULL;

    // 1. Kiểm tra giới hạn thời gian BẬT liên tục tối đa (FSD §4.10.2)
    for (int i = 0; i < RELAY_COUNT; i++) {
        if (s_relay_states[i] && s_relay_max_on_sec[i] > 0) {
            if (now - s_relay_on_timestamp[i] >= s_relay_max_on_sec[i]) {
                ESP_LOGE(TAG, "[SAFETY CUTOFF] Relay %d exceeded maximum continuous ON time (%lu s)! Auto-cutoff.",
                         i + 1, (unsigned long)s_relay_max_on_sec[i]);
                char name[16];
                snprintf(name, sizeof(name), "relay_%d", i + 1);
                relay_controller_set_state(name, false, NULL, "SAFETY_CUTOFF");
            }
        }
    }

    // 2. Kiểm tra cảm biến mất tín hiệu > 60s khi Máy sưởi đang chạy (FSD §4.10.4)
    if (s_relay_states[1]) { // Relay 2 = Heater
        if (s_last_valid_sensor_time > 0 && (now - s_last_valid_sensor_time > 60)) {
            ESP_LOGE(TAG, "[SAFETY CUTOFF] Sensor communication lost > 60s while Heater active! Emergency shutoff.");
            relay_controller_set_state("relay_2", false, NULL, "SAFETY_CUTOFF");
        }
    }
}

void relay_controller_init(void)
{
    ESP_LOGI(TAG, "Initializing Relay Controller (GPIO 15..18, Active HIGH, 10k pull-down)...");

    if (s_relay_mutex == NULL) {
        s_relay_mutex = xSemaphoreCreateMutex();
    }

    // Cấu hình GPIO output, trở kéo xuống phần cứng 10k, ban đầu xuất mức LOW an toàn (FSD §4.14 Boot Step 2)
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT,
        .pin_bit_mask = (1ULL << RELAY_1_GPIO) |
                        (1ULL << RELAY_2_GPIO) |
                        (1ULL << RELAY_3_GPIO) |
                        (1ULL << RELAY_4_GPIO),
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&io_conf);

    for (int i = 0; i < RELAY_COUNT; i++) {
        gpio_set_level(s_relay_pins[i], 0);
        s_relay_states[i] = false;
        s_relay_on_timestamp[i] = 0;
    }

    s_last_valid_sensor_time = esp_timer_get_time() / 1000000ULL;

    // Đăng ký nhận sự kiện trên Event Bus
    event_bus_register_handler(EVT_DEVICE_COMMAND_REQUEST, on_device_command_request);
    event_bus_register_handler(EVT_RULE_ACTION_REQUEST, on_rule_action_request);
    event_bus_register_handler(EVT_SENSOR_VALUE_INVALID, on_sensor_value_invalid);
    event_bus_register_handler(EVT_BLE_DEVICE_DATA, on_ble_device_data);

    // Khởi tạo timer kiểm tra định kỳ an toàn 1 giây (Safe Cutoff & Timeout Watchdog)
    if (s_safety_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &relay_safety_timer_cb,
            .name = "relay_safety_watchdog"
        };
        esp_timer_create(&timer_args, &s_safety_timer);
        esp_timer_start_periodic(s_safety_timer, 1000000ULL); // Mỗi 1000ms
    }

    ESP_LOGI(TAG, "Relay Controller initialized successfully (Safety Interlock enabled)");
}
