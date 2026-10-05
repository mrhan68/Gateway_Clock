#include "mqtt_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "event_bus.h"
#include "config_manager.h"

static const char *TAG = "MQTT_ADAPTER";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_is_connected = false;
static char s_gateway_id[32] = "gw_default";
static char s_broker_uri[128] = "mqtts://hivemq.cloud:8883";
static char s_mqtt_username[64] = {0};
static char s_mqtt_password[64] = {0};
static char s_status_topic[64] = {0};
static char s_cmd_sub_topic[64] = {0};
static char s_sched_sub_topic[64] = {0};

static void on_wifi_connected(const gateway_event_t *evt)
{
    (void)evt;
    if (s_client != NULL) {
        ESP_LOGI(TAG, "Wi-Fi UP: Starting/restarting MQTT client...");
        esp_mqtt_client_start(s_client);
    }
}

static void on_wifi_lost(const gateway_event_t *evt)
{
    (void)evt;
    if (s_client != NULL && s_is_connected) {
        ESP_LOGW(TAG, "Wi-Fi DOWN: Stopping MQTT client...");
        esp_mqtt_client_stop(s_client);
        s_is_connected = false;
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch (event_id) {
    case MQTT_EVENT_CONNECTED:
        s_is_connected = true;
        // Format chuan cho Journey Test JRN-04: "[MQTT] Connected to broker"
        printf("\n[MQTT] Connected to broker\n");
        ESP_LOGI(TAG, "Connected to broker: %s", s_broker_uri);
        fflush(stdout);

        // Publish LWT clear (status: online, retain: 1)
        esp_mqtt_client_publish(s_client, s_status_topic, "online", 6, 1, 1);

        // Subscribe topic lenh va schedule (FSD §4.6.4)
        esp_mqtt_client_subscribe(s_client, s_cmd_sub_topic, 1);
        esp_mqtt_client_subscribe(s_client, s_sched_sub_topic, 1);
        ESP_LOGI(TAG, "Subscribed to '%s' and '%s'", s_cmd_sub_topic, s_sched_sub_topic);

        event_bus_post(EVT_MQTT_CONNECTED, NULL, 0, false);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_is_connected = false;
        printf("[MQTT] Disconnected from broker\n");
        ESP_LOGW(TAG, "Disconnected from MQTT broker");
        fflush(stdout);
        event_bus_post(EVT_MQTT_DISCONNECTED, NULL, 0, false);
        break;

    case MQTT_EVENT_DATA:
        if (event != NULL) {
            ESP_LOGI(TAG, "[MQTT RX] Topic: %.*s, Data: %.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);
            // Post remote command len event bus
            event_bus_post(EVT_REMOTE_COMMAND_REQUEST, NULL, 0, false);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error occurred");
        break;

    default:
        break;
    }
}

void mqtt_adapter_init(const char *broker_uri, const char *gateway_id, const char *username, const char *password)
{
    if (gateway_id != NULL && strlen(gateway_id) > 0) {
        strncpy(s_gateway_id, gateway_id, sizeof(s_gateway_id) - 1);
    }
    if (broker_uri != NULL && strlen(broker_uri) > 0) {
        strncpy(s_broker_uri, broker_uri, sizeof(s_broker_uri) - 1);
    }
    if (username != NULL) strncpy(s_mqtt_username, username, sizeof(s_mqtt_username) - 1);
    if (password != NULL) strncpy(s_mqtt_password, password, sizeof(s_mqtt_password) - 1);

    snprintf(s_status_topic, sizeof(s_status_topic), "gateway/%s/status", s_gateway_id);
    snprintf(s_cmd_sub_topic, sizeof(s_cmd_sub_topic), "gateway/%s/device/+/command", s_gateway_id);
    snprintf(s_sched_sub_topic, sizeof(s_sched_sub_topic), "gateway/%s/config/schedule", s_gateway_id);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = s_broker_uri,
            },
        },
        .credentials = {
            .username = s_mqtt_username[0] ? s_mqtt_username : NULL,
            .authentication = {
                .password = s_mqtt_password[0] ? s_mqtt_password : NULL,
            },
        },
        .session = {
            .last_will = {
                .topic = s_status_topic,
                .msg = "offline",
                .msg_len = 7,
                .qos = 1,
                .retain = 1,
            },
            .keepalive = 60,
        },
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    ESP_LOGI(TAG, "MQTT Adapter initialized with Broker='%s', LWT='%s'", s_broker_uri, s_status_topic);

    // Dang ky Event Bus handlers de tu dong bat/tat khi co/mat Wi-Fi
    event_bus_register_handler(EVT_WIFI_CONNECTED, on_wifi_connected);
    event_bus_register_handler(EVT_WIFI_LOST, on_wifi_lost);
}

bool mqtt_adapter_publish(const char *topic_suffix, const char *payload_json)
{
    if (s_client == NULL || !s_is_connected) {
        return false;
    }

    char full_topic[128];
    if (strncmp(topic_suffix, "gateway/", 8) == 0) {
        strncpy(full_topic, topic_suffix, sizeof(full_topic) - 1);
    } else {
        snprintf(full_topic, sizeof(full_topic), "gateway/%s/%s", s_gateway_id, topic_suffix);
    }

    // timeout=0 (non-blocking - chi enqueue vao esp-mqtt outbox) theo FSD §4.6.3
    int msg_id = esp_mqtt_client_publish(s_client, full_topic, payload_json, 0, 0, 0);
    return (msg_id >= 0);
}

bool mqtt_adapter_is_connected(void)
{
    return s_is_connected;
}
