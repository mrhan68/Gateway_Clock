#ifndef MQTT_ADAPTER_H
#define MQTT_ADAPTER_H

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern C {
#endif

/**
 * @brief Khoi tao MQTT Adapter (FSD §4.6)
 * @param broker_uri Dia chi broker MQTT (VD: mqtt://broker.emqx.io:1883)
 * @param gateway_id ID dinh danh duy nhat cua Gateway
 */
void mqtt_adapter_init(const char *broker_uri, const char *gateway_id, const char *username, const char *password);

/**
 * @brief Publish mot message len MQTT Broker
 * @param topic_suffix Phan duoi cua topic (VD: "device/fan_01/telemetry")
 *        Se duoc tu dong ghep thanh "gateway/{gateway_id}/{topic_suffix}"
 * @param payload_json Noi dung chuoi JSON hoac text
 */
bool mqtt_adapter_publish(const char *topic_suffix, const char *payload_json);

/**
 * @brief Kiem tra trang thai ket noi toi broker
 */
bool mqtt_adapter_is_connected(void);

#ifdef __cplusplus
}
#endif

#endif // MQTT_ADAPTER_H
