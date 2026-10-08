#include "mqtt_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "mqtt_client.h"
#include "event_bus.h"
#include "config_manager.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "cJSON.h"

static const char *TAG = "MQTT_ADAPTER";

static esp_mqtt_client_handle_t s_client = NULL;
static bool s_is_connected = false;
static char s_gateway_id[32] = "gw_default";
static char s_broker_uri[128] = "mqtts://your-emqx-broker.emqxsl.com:8883";
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
        if (event != NULL && event->topic_len > 0 && event->data_len > 0) {
            char topic[128] = {0};
            char data[256] = {0};
            size_t tlen = event->topic_len < (sizeof(topic) - 1) ? event->topic_len : (sizeof(topic) - 1);
            size_t dlen = event->data_len < (sizeof(data) - 1) ? event->data_len : (sizeof(data) - 1);
            memcpy(topic, event->topic, tlen);
            memcpy(data, event->data, dlen);

            ESP_LOGI(TAG, "[MQTT RX] Topic: %s, Data: %s", topic, data);

            // Parse device command: gateway/<gw_id>/device/<dev_id>/command
            char *p_dev = strstr(topic, "/device/");
            char *p_cmd = strstr(topic, "/command");
            if (p_dev != NULL && p_cmd != NULL && p_cmd > p_dev) {
                p_dev += 8;
                char target_dev[32] = {0};
                size_t dev_len = (size_t)(p_cmd - p_dev);
                if (dev_len > 0 && dev_len < sizeof(target_dev)) {
                    strncpy(target_dev, p_dev, dev_len);
                }

                device_command_req_t *req = (device_command_req_t *)malloc(sizeof(device_command_req_t));
                if (req != NULL) {
                    memset(req, 0, sizeof(device_command_req_t));
                    strncpy(req->target_device_id, target_dev, sizeof(req->target_device_id) - 1);
                    req->ttl_seconds = 60;
                    req->timestamp = (uint32_t)(esp_timer_get_time() / 1000000ULL);

                    cJSON *json = cJSON_Parse(data);
                    if (json != NULL) {
                        cJSON *j_cmd = cJSON_GetObjectItem(json, "command");
                        cJSON *j_act = cJSON_GetObjectItem(json, "action");
                        cJSON *j_id = cJSON_GetObjectItem(json, "command_id");
                        cJSON *j_target = cJSON_GetObjectItem(json, "target_device_id");
                        cJSON *j_ttl = cJSON_GetObjectItem(json, "ttl_seconds");

                        if (j_cmd && cJSON_IsString(j_cmd)) {
                            strncpy(req->action, j_cmd->valuestring, sizeof(req->action) - 1);
                        } else if (j_act && cJSON_IsString(j_act)) {
                            strncpy(req->action, j_act->valuestring, sizeof(req->action) - 1);
                        }
                        if (j_id && cJSON_IsString(j_id)) {
                            strncpy(req->command_id, j_id->valuestring, sizeof(req->command_id) - 1);
                        }
                        if (j_target && cJSON_IsString(j_target)) {
                            strncpy(req->target_device_id, j_target->valuestring, sizeof(req->target_device_id) - 1);
                        }
                        if (j_ttl && cJSON_IsNumber(j_ttl)) {
                            req->ttl_seconds = (uint32_t)j_ttl->valueint;
                        }
                        cJSON_Delete(json);
                    } else {
                        strncpy(req->action, data, sizeof(req->action) - 1);
                        snprintf(req->command_id, sizeof(req->command_id), "cmd_%lu", (unsigned long)req->timestamp);
                    }

                    if (req->action[0] != '\0') {
                        event_bus_post(EVT_DEVICE_COMMAND_REQUEST, req, sizeof(device_command_req_t), false);
                        ESP_LOGI(TAG, "Dispatched device command: target='%s', action='%s', cmd_id='%s'",
                                 req->target_device_id, req->action, req->command_id);
                    } else {
                        free(req);
                    }
                }
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error occurred");
        break;

    default:
        break;
    }
}

static void on_command_response(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL && evt->len >= sizeof(command_ack_t)) {
        const command_ack_t *ack = (const command_ack_t *)evt->data;
        char sub_topic[128];
        snprintf(sub_topic, sizeof(sub_topic), "device/%s/command_ack", ack->target_device_id);
        cJSON *root = cJSON_CreateObject();
        if (root != NULL) {
            cJSON_AddStringToObject(root, "command_id", ack->command_id);
            cJSON_AddBoolToObject(root, "success", ack->success);
            cJSON_AddNumberToObject(root, "error_code", ack->error_code);
            cJSON_AddStringToObject(root, "message", ack->message);
            cJSON_AddNumberToObject(root, "executed_at", (double)ack->executed_at);
            char *str = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);
            if (str != NULL) {
                mqtt_adapter_publish(sub_topic, str);
                free(str);
            }
        }
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
            .verification = {
                .crt_bundle_attach = esp_crt_bundle_attach,
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
    event_bus_register_handler(EVT_COMMAND_RESPONSE, on_command_response);
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
