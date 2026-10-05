import re

with open('firmware/main/wifi_provisioning.c', 'r') as f:
    content = f.read()

# 1. Timer logic for Bug 9
timer_code = """
static esp_timer_handle_t s_ble_resume_timer = NULL;

static void ble_resume_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "BLE scan resume timer expired. Resuming scan...");
    ble_adapter_resume_scan();
}
"""
if "ble_resume_timer_cb" not in content:
    content = content.replace('static int s_dns_sock = -1;', 'static int s_dns_sock = -1;' + timer_code)

# Insert into start_softap_internal for bug 9
pause_ble_code = """ble_adapter_pause_scan();"""
pause_ble_replacement = """ble_adapter_pause_scan();
        if (s_ble_resume_timer == NULL) {
            esp_timer_create_args_t timer_args = {
                .callback = &ble_resume_timer_cb,
                .name = "ble_res_tmr"
            };
            esp_timer_create(&timer_args, &s_ble_resume_timer);
        }
        esp_timer_start_once(s_ble_resume_timer, 60000000ULL); // 60 seconds"""
if "esp_timer_start_once" not in content:
    content = content.replace(pause_ble_code, pause_ble_replacement)

# Stop timer in GOT_IP
resume_ble_code = """ble_adapter_resume_scan();"""
resume_ble_replacement = """ble_adapter_resume_scan();
        if (s_ble_resume_timer) {
            esp_timer_stop(s_ble_resume_timer);
        }"""
if "esp_timer_stop(s_ble_resume_timer)" not in content:
    content = content.replace(resume_ble_code, resume_ble_replacement)


# 2. Bug 7 logic
# Update start_softap_internal to just re-apply if already in mode
start_code = """static void start_softap_internal(void)
{
    if (s_in_softap_mode) return;
    s_in_softap_mode = true;"""
start_replace = """static void start_softap_internal(bool force_reconfig)
{
    if (s_in_softap_mode && !force_reconfig) return;
    s_in_softap_mode = true;"""
content = content.replace(start_code, start_replace)

# Replace all calls to start_softap_internal() with start_softap_internal(false)
content = content.replace("start_softap_internal();", "start_softap_internal(false);")

# Add the apply function
func = """
void wifi_provisioning_apply_softap_config(void)
{
    start_softap_internal(true);
}
"""
if "wifi_provisioning_apply_softap_config" not in content:
    content += func

with open('firmware/main/wifi_provisioning.c', 'w') as f:
    f.write(content)

# Header
with open('firmware/main/wifi_provisioning.h', 'r') as f:
    hcontent = f.read()

hfunc = "void wifi_provisioning_apply_softap_config(void);\n"
if hfunc not in hcontent:
    hcontent = hcontent.replace("#ifdef __cplusplus", hfunc + "\n#ifdef __cplusplus", 1)

with open('firmware/main/wifi_provisioning.h', 'w') as f:
    f.write(hcontent)
