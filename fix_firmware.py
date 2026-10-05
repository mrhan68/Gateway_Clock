import os

# 1. wifi_provisioning.c: Fix SoftAP restart & BLE pause timer & remove WIFI_PS_NONE
with open('firmware/main/wifi_provisioning.c', 'r') as f:
    wifi_c = f.read()

# Remove WIFI_PS_NONE (causes errors when BT is on)
wifi_c = wifi_c.replace('esp_wifi_set_ps(WIFI_PS_NONE); // Disable modem sleep so SoftAP never drops beacons', '// esp_wifi_set_ps(WIFI_PS_NONE); // Removed, incompatible with Coex')

# Fix start_softap_internal to apply config
wifi_c = wifi_c.replace(
    'static void start_softap_internal(void)\n{\n    if (s_in_softap_mode) return;\n    s_in_softap_mode = true;',
    'static void start_softap_internal(void)\n{\n    if (s_in_softap_mode) esp_wifi_stop(); // Apply new pass\n    s_in_softap_mode = true;'
)

# Add BLE rescue timer
rescue_timer = """
static esp_timer_handle_t s_ble_rescue_timer = NULL;
static void ble_rescue_cb(void* arg) {
    ESP_LOGW(TAG, "[WIFI_PRESCAN] 60s timeout without Wi-Fi connection, rescuing BLE scan...");
    ble_adapter_resume_scan();
}
"""
if 'ble_rescue_cb' not in wifi_c:
    wifi_c = wifi_c.replace('static void background_scan_task', rescue_timer + '\nstatic void background_scan_task')

wifi_c = wifi_c.replace(
    'ble_adapter_pause_scan();',
    'ble_adapter_pause_scan();\n        if (!s_ble_rescue_timer) {\n            esp_timer_create_args_t t_args = {.callback = &ble_rescue_cb, .name = "ble_rescue"};\n            esp_timer_create(&t_args, &s_ble_rescue_timer);\n        }\n        esp_timer_start_once(s_ble_rescue_timer, 60000000);'
)
wifi_c = wifi_c.replace(
    'ble_adapter_resume_scan();',
    'ble_adapter_resume_scan();\n        if (s_ble_rescue_timer) esp_timer_stop(s_ble_rescue_timer);'
)

with open('firmware/main/wifi_provisioning.c', 'w') as f:
    f.write(wifi_c)

# 2. web_server.c: Fix url_decode for softap password
with open('firmware/main/web_server.c', 'r') as f:
    web_c = f.read()

# SoftAP url decode fix
web_c = web_c.replace(
    'strncpy(password, p_pass, len);',
    'char raw_pass[128] = {0};\n            strncpy(raw_pass, p_pass, len);\n            url_decode(password, raw_pass, sizeof(password));'
)
with open('firmware/main/web_server.c', 'w') as f:
    f.write(web_c)

print("Fix applied successfully.")
