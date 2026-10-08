#include "uplink.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "supervisor.h"
#include "mqtt_adapter.h"
#include "wifi_provisioning.h"
#include "event_bus.h"
#include "display.h"

static const char *TAG = "UPLINK";

#define UPLINK_TASK_PRIO  3
#define UPLINK_TASK_CORE  1

static void on_ble_device_data(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(ble_telemetry_t)) {
        const ble_telemetry_t *t = (const ble_telemetry_t *)evt->data;
        if (mqtt_adapter_is_connected()) {
            char payload[160];
            uint64_t uptime_s = esp_timer_get_time() / 1000000ULL;
            snprintf(payload, sizeof(payload),
                     "{\"device_id\":\"%s\",\"temperature\":%.2f,\"humidity\":%.2f,\"uptime_s\":%llu,\"valid\":%s}",
                     t->device_id, t->temperature, t->humidity, uptime_s, t->valid ? "true" : "false");
            mqtt_adapter_publish("telemetry", payload);
            ESP_LOGI(TAG, "Sensor telemetry forwarded to MQTT: %s", payload);
        }
    }
}

static void uplink_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Uplink telemetry task started on Core %d", UPLINK_TASK_CORE);
    uint32_t elapsed_s = 0;

    while (1) {
        // Bao cao nhip tim cho Task Supervisor
        supervisor_report_heartbeat(TASK_HEARTBEAT_UPLINK);

        const gateway_config_t *cfg = config_manager_get();
        uint16_t interval = (cfg != NULL && cfg->mqtt_telemetry_interval_s > 0) ? cfg->mqtt_telemetry_interval_s : 10;

        elapsed_s++;
        if (elapsed_s >= interval) {
            elapsed_s = 0;
            if (mqtt_adapter_is_connected()) {
                uint64_t uptime_s = esp_timer_get_time() / 1000000ULL;
                uint32_t free_heap = esp_get_free_heap_size();
                const char *ip = wifi_provisioning_get_ip();
                char payload[160];
                if (display_get_latest_valid()) {
                    float cur_temp = display_get_latest_temp();
                    float cur_hum = display_get_latest_hum();
                    snprintf(payload, sizeof(payload),
                             "{\"uptime_s\":%llu,\"free_heap\":%lu,\"ip\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f}",
                             uptime_s, (unsigned long)free_heap, ip, cur_temp, cur_hum);
                } else {
                    snprintf(payload, sizeof(payload),
                             "{\"uptime_s\":%llu,\"free_heap\":%lu,\"ip\":\"%s\"}",
                             uptime_s, (unsigned long)free_heap, ip);
                }

                mqtt_adapter_publish("telemetry", payload);
                ESP_LOGD(TAG, "Uplink telemetry published: %s", payload);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void uplink_init(void)
{
    const gateway_config_t *cfg = config_manager_get();
    const char *broker_uri = (cfg != NULL && strlen(cfg->mqtt_broker_uri) > 0) ?
                             cfg->mqtt_broker_uri : "mqtts://your-emqx-broker.emqxsl.com:8883";
    const char *mqtt_user = cfg ? cfg->mqtt_username : NULL;
    const char *mqtt_pass = cfg ? cfg->mqtt_password : NULL;
    const char *gateway_id = (cfg != NULL && strlen(cfg->gateway_id) > 0) ?
                             cfg->gateway_id : "gw_esp32_01";

    mqtt_adapter_init(broker_uri, gateway_id, mqtt_user, mqtt_pass);
    event_bus_register_handler(EVT_BLE_DEVICE_DATA, on_ble_device_data);

    xTaskCreatePinnedToCore(uplink_task,
                            "uplink_task",
                            4096,
                            NULL,
                            UPLINK_TASK_PRIO,
                            NULL,
                            UPLINK_TASK_CORE);
}
