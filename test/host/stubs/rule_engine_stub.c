#include "esp_stubs.h"
#include "event_bus.h"

const char *TAG = "RULE";

int64_t g_mock_esp_timer_us = 1000000LL;
int g_mock_rule_action_count = 0;
rule_action_request_t g_mock_last_rule_action;
static gateway_event_handler_t s_registered_handlers[32] = {0};

// Mock NVS
nvs_handle_t s_nvs_handle = 1;
int nvs_open(const char* name, int mode, nvs_handle_t *out) { *out = 1; return 0; }
int nvs_get_str(nvs_handle_t handle, const char* key, char* out, size_t *len) { return -1; }
int nvs_get_blob(nvs_handle_t handle, const char* key, void* out, size_t *len) { return -1; }
int nvs_set_blob(nvs_handle_t handle, const char* key, const void* val, size_t len) { return 0; }
int nvs_commit(nvs_handle_t handle) { return 0; }

// Mock Event Bus
BaseType_t event_bus_post(gateway_event_code_t code, void *data, size_t len, bool from_isr) {
    (void)from_isr;
    if (code == EVT_RULE_ACTION_REQUEST && data != NULL && len >= sizeof(rule_action_request_t)) {
        memcpy(&g_mock_last_rule_action, data, sizeof(rule_action_request_t));
        g_mock_rule_action_count++;
    } else if (code >= 0 && code < 32 && s_registered_handlers[code] != NULL) {
        gateway_event_t evt = {
            .code = code,
            .data = data,
            .len = len,
            .timestamp = (uint32_t)(g_mock_esp_timer_us / 1000000LL)
        };
        s_registered_handlers[code](&evt);
    }
    if (data != NULL) {
        free(data);
    }
    return pdTRUE;
}

bool event_bus_register_handler(gateway_event_code_t code, gateway_event_handler_t handler) {
    if (code >= 0 && code < 32) {
        s_registered_handlers[code] = handler;
    }
    return true;
}

// Mock Relay
int relay_controller_set(int relay_id, bool state) { (void)relay_id; (void)state; return 0; }
int relay_controller_get_state(int relay_id) { (void)relay_id; return 0; }

// Mock Scheduler
int scheduler_get_snooze_status(void) { return 0; }

// Mock RTC
int rtc_get_time(int *hour, int *min, int *sec) { *hour = 12; *min = 0; *sec = 0; return 0; }

// Dummy FreeRTOS
int xTaskCreatePinnedToCore(void* a, const char* b, int c, void* d, int e, void** f, int g) { return 1; }

// Mock MQTT & SD Logger
int mqtt_adapter_is_connected(void) { return 1; }
bool mqtt_adapter_publish(const char *topic, const char *payload) { (void)topic; (void)payload; return true; }
bool sd_logger_is_mounted(void) { return false; }
