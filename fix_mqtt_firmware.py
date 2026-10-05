import os

# 1. Update config_manager.h
with open('firmware/main/config_manager.h', 'r') as f:
    conf_h = f.read()

if 'char mqtt_username[64];' not in conf_h:
    conf_h = conf_h.replace(
        'char mqtt_broker_uri[128];',
        'char mqtt_broker_uri[128];\n    char mqtt_username[64];\n    char mqtt_password[64];'
    )
with open('firmware/main/config_manager.h', 'w') as f:
    f.write(conf_h)


# 2. Update config_manager.c
with open('firmware/main/config_manager.c', 'r') as f:
    conf_c = f.read()

conf_c = conf_c.replace(
    'strncpy(s_active_config.mqtt_broker_uri, "mqtt://broker.emqx.io:1883", sizeof(s_active_config.mqtt_broker_uri) - 1);',
    'strncpy(s_active_config.mqtt_broker_uri, "mqtts://16a4e3...hivemq.cloud:8883", sizeof(s_active_config.mqtt_broker_uri) - 1);\n    strncpy(s_active_config.mqtt_username, "gw_client", sizeof(s_active_config.mqtt_username) - 1);\n    strncpy(s_active_config.mqtt_password, "your_mqtt_password", sizeof(s_active_config.mqtt_password) - 1);'
)

# NVS Read/Write
nvs_read = """
    len = sizeof(s_active_config.mqtt_broker_uri);
    nvs_get_str(handle, "mqtt_broker_uri", s_active_config.mqtt_broker_uri, &len);
"""
nvs_read_new = """
    len = sizeof(s_active_config.mqtt_broker_uri);
    nvs_get_str(handle, "mqtt_broker_uri", s_active_config.mqtt_broker_uri, &len);
    len = sizeof(s_active_config.mqtt_username);
    nvs_get_str(handle, "mqtt_username", s_active_config.mqtt_username, &len);
    len = sizeof(s_active_config.mqtt_password);
    nvs_get_str(handle, "mqtt_password", s_active_config.mqtt_password, &len);
"""
conf_c = conf_c.replace(nvs_read, nvs_read_new)

nvs_write = """
    if (strlen(s_active_config.mqtt_broker_uri) > 0) {
        nvs_set_str(handle, "mqtt_broker_uri", s_active_config.mqtt_broker_uri);
    }
"""
nvs_write_new = """
    if (strlen(s_active_config.mqtt_broker_uri) > 0) {
        nvs_set_str(handle, "mqtt_broker_uri", s_active_config.mqtt_broker_uri);
        nvs_set_str(handle, "mqtt_username", s_active_config.mqtt_username);
        nvs_set_str(handle, "mqtt_password", s_active_config.mqtt_password);
    }
"""
conf_c = conf_c.replace(nvs_write, nvs_write_new)

with open('firmware/main/config_manager.c', 'w') as f:
    f.write(conf_c)


# 3. Update mqtt_adapter.h
with open('firmware/main/mqtt_adapter.h', 'r') as f:
    mqtt_h = f.read()
mqtt_h = mqtt_h.replace(
    'void mqtt_adapter_init(const char *broker_uri, const char *gateway_id);',
    'void mqtt_adapter_init(const char *broker_uri, const char *gateway_id, const char *username, const char *password);'
)
with open('firmware/main/mqtt_adapter.h', 'w') as f:
    f.write(mqtt_h)

# 4. Update mqtt_adapter.c
with open('firmware/main/mqtt_adapter.c', 'r') as f:
    mqtt_c = f.read()

mqtt_c = mqtt_c.replace(
    'static char s_broker_uri[128] = "mqtt://broker.emqx.io:1883";',
    'static char s_broker_uri[128] = "mqtts://hivemq.cloud:8883";\nstatic char s_mqtt_username[64] = {0};\nstatic char s_mqtt_password[64] = {0};'
)
mqtt_c = mqtt_c.replace(
    'void mqtt_adapter_init(const char *broker_uri, const char *gateway_id)',
    'void mqtt_adapter_init(const char *broker_uri, const char *gateway_id, const char *username, const char *password)'
)

mqtt_init_inner = """
    if (broker_uri != NULL && strlen(broker_uri) > 0) {
        strncpy(s_broker_uri, broker_uri, sizeof(s_broker_uri) - 1);
    }
"""
mqtt_init_inner_new = """
    if (broker_uri != NULL && strlen(broker_uri) > 0) {
        strncpy(s_broker_uri, broker_uri, sizeof(s_broker_uri) - 1);
    }
    if (username != NULL) strncpy(s_mqtt_username, username, sizeof(s_mqtt_username) - 1);
    if (password != NULL) strncpy(s_mqtt_password, password, sizeof(s_mqtt_password) - 1);
"""
mqtt_c = mqtt_c.replace(mqtt_init_inner, mqtt_init_inner_new)

mqtt_cfg = """
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker = {
            .address = {
                .uri = s_broker_uri,
            },
        },
"""
mqtt_cfg_new = """
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
"""
mqtt_c = mqtt_c.replace(mqtt_cfg, mqtt_cfg_new)
with open('firmware/main/mqtt_adapter.c', 'w') as f:
    f.write(mqtt_c)

# 5. Update uplink.c
with open('firmware/main/uplink.c', 'r') as f:
    uplink_c = f.read()

uplink_c = uplink_c.replace(
    'const char *broker_uri = (cfg != NULL && strlen(cfg->mqtt_broker_uri) > 0) ?\n                             cfg->mqtt_broker_uri : "mqtt://broker.emqx.io:1883";',
    'const char *broker_uri = (cfg != NULL && strlen(cfg->mqtt_broker_uri) > 0) ?\n                             cfg->mqtt_broker_uri : "mqtts://hivemq.cloud:8883";\n    const char *mqtt_user = cfg ? cfg->mqtt_username : NULL;\n    const char *mqtt_pass = cfg ? cfg->mqtt_password : NULL;'
)
uplink_c = uplink_c.replace(
    'mqtt_adapter_init(broker_uri, gateway_id);',
    'mqtt_adapter_init(broker_uri, gateway_id, mqtt_user, mqtt_pass);'
)
with open('firmware/main/uplink.c', 'w') as f:
    f.write(uplink_c)

print("MQTT Firmware updated to support MQTTS and Credentials.")
