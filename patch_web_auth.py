
with open('firmware/main/web_server.c', 'r') as f:
    code = f.read()

auth_func = """
static bool check_api_auth(httpd_req_t *req) {
    char auth_header[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", auth_header, sizeof(auth_header)) == ESP_OK) {
        // Ponytail: simple static token for now. In real prod, this reads from NVS.
        if (strstr(auth_header, "Bearer gateway-lan-secret-123")) {
            return true;
        }
    }
    httpd_resp_send_err(req, HTTPD_401_UNAUTHORIZED, "Missing or invalid Authorization header");
    return false;
}
"""

if "check_api_auth" not in code:
    code = code.replace("static const char *TAG = \"WEB_SERVER\";", "static const char *TAG = \"WEB_SERVER\";\n" + auth_func)

# Add check to sensitive endpoints
def inject_auth(handler_name):
    global code
    search = f"static esp_err_t {handler_name}(httpd_req_t *req)\n{{"
    replace = f"static esp_err_t {handler_name}(httpd_req_t *req)\n{{\n    if (!check_api_auth(req)) return ESP_OK;"
    code = code.replace(search, replace)

inject_auth("post_relay_handler")
inject_auth("post_rules_handler")
inject_auth("delete_rules_handler")
inject_auth("post_system_reboot_handler")
inject_auth("post_system_factory_reset_handler")
inject_auth("post_softap_set_password_handler")
inject_auth("post_wifi_save_handler")
inject_auth("post_wifi_delete_handler")
inject_auth("post_wifi_connect_handler")

with open('firmware/main/web_server.c', 'w') as f:
    f.write(code)
