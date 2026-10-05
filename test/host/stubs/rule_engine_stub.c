#include "esp_stubs.h"
const char *TAG = "RULE";

// Mock NVS
nvs_handle_t s_nvs_handle = 1;
int nvs_open(const char* name, int mode, nvs_handle_t *out) { *out = 1; return 0; }
int nvs_get_str(nvs_handle_t handle, const char* key, char* out, size_t *len) { return -1; }
int nvs_get_blob(nvs_handle_t handle, const char* key, void* out, size_t *len) { return -1; }
int nvs_set_blob(nvs_handle_t handle, const char* key, const void* val, size_t len) { return 0; }
int nvs_commit(nvs_handle_t handle) { return 0; }

// Mock Event Bus
void event_bus_post(int event, void *data, size_t len, bool retry) {}
void event_bus_register_handler(int event, void (*handler)(void*)) {}

// Mock Relay
int relay_controller_set(int relay_id, bool state) { return 0; }
int relay_controller_get_state(int relay_id) { return 0; }

// Mock Scheduler
int scheduler_get_snooze_status(void) { return 0; }

// Mock RTC
int rtc_get_time(int *hour, int *min, int *sec) { *hour = 12; *min = 0; *sec = 0; return 0; }

// Dummy FreeRTOS
int xTaskCreatePinnedToCore(void* a, const char* b, int c, void* d, int e, void** f, int g) { return 1; }

// Mock MQTT
int mqtt_adapter_is_connected(void) { return 1; }
void mqtt_adapter_publish(const char *topic, const char *payload) {}
