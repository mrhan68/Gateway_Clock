#include "wifi_provisioning.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "event_bus.h"

static const char *TAG = "WIFI_PROV";

#define SOFTAP_SSID      "SmartClock-Setup"
#define SOFTAP_MAX_CONN  4
#define DNS_PORT         53

static esp_netif_t *s_netif_sta = NULL;
static esp_netif_t *s_netif_ap = NULL;
static httpd_handle_t s_http_server = NULL;
static TaskHandle_t s_dns_task_handle = NULL;
static bool s_is_connected = false;
static char s_current_ip[16] = "0.0.0.0";
static bool s_in_softap_mode = false;
static int s_dns_sock = -1;

#define MAX_SCANNED_APS    16
typedef struct {
    char ssid[33];
    int8_t rssi;
    uint8_t authmode;
    uint8_t channel;
} scanned_ap_info_t;

static scanned_ap_info_t s_scanned_aps[MAX_SCANNED_APS];
static uint16_t s_scanned_count = 0;

#pragma pack(push, 1)
typedef struct {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;
#pragma pack(pop)

static void dns_server_task(void *pvParameters)
{
    uint8_t rx_buffer[256];
    uint8_t tx_buffer[256];
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    s_dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_sock < 0) {
        ESP_LOGE(TAG, "DNS: Failed to create socket");
        vTaskDelete(NULL);
        return;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(DNS_PORT);

    if (bind(s_dns_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "DNS: Failed to bind socket");
        close(s_dns_sock);
        s_dns_sock = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Captive Portal DNS Server started on port 53");

    while (1) {
        int len = recvfrom(s_dns_sock, rx_buffer, sizeof(rx_buffer), 0,
                           (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < (int)sizeof(dns_header_t)) {
            if (len < 0 && s_dns_sock < 0) break;
            continue;
        }

        dns_header_t *resp = (dns_header_t *)tx_buffer;
        memcpy(tx_buffer, rx_buffer, len);
        resp->flags = htons(0x8180);
        resp->ancount = htons(1);

        int offset = len;
        tx_buffer[offset++] = 0xC0;
        tx_buffer[offset++] = 0x0C;
        tx_buffer[offset++] = 0x00;
        tx_buffer[offset++] = 0x01;
        tx_buffer[offset++] = 0x00;
        tx_buffer[offset++] = 0x01;
        tx_buffer[offset++] = 0x00;
        tx_buffer[offset++] = 0x00;
        tx_buffer[offset++] = 0x00;
        tx_buffer[offset++] = 0x3C;
        tx_buffer[offset++] = 0x00;
        tx_buffer[offset++] = 0x04;
        tx_buffer[offset++] = 192;
        tx_buffer[offset++] = 168;
        tx_buffer[offset++] = 4;
        tx_buffer[offset++] = 1;

        sendto(s_dns_sock, tx_buffer, offset, 0, (struct sockaddr *)&client_addr, client_addr_len);
    }

    vTaskDelete(NULL);
}

static const char* authmode_to_str(uint8_t authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN: return "OPEN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA_PSK";
    case WIFI_AUTH_WPA2_PSK: return "WPA2_PSK";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2_ENT";
    case WIFI_AUTH_WPA3_PSK: return "WPA3_PSK";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WPA2/WPA3";
    default: return "UNKNOWN";
    }
}

static uint16_t perform_wifi_scan(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 150,
    };

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_start failed: 0x%x", err);
        return 0;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) return 0;

    wifi_ap_record_t *records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!records) return 0;

    esp_wifi_scan_get_ap_records(&ap_count, records);

    s_scanned_count = 0;
    for (int i = 0; i < ap_count && s_scanned_count < MAX_SCANNED_APS; i++) {
        if (strlen((char *)records[i].ssid) == 0) continue;

        bool duplicate = false;
        for (int j = 0; j < s_scanned_count; j++) {
            if (strcmp(s_scanned_aps[j].ssid, (char *)records[i].ssid) == 0) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) continue;

        strncpy(s_scanned_aps[s_scanned_count].ssid, (char *)records[i].ssid, sizeof(s_scanned_aps[0].ssid) - 1);
        s_scanned_aps[s_scanned_count].rssi = records[i].rssi;
        s_scanned_aps[s_scanned_count].authmode = records[i].authmode;
        s_scanned_aps[s_scanned_count].channel = records[i].primary;
        s_scanned_count++;
    }

    free(records);
    return s_scanned_count;
}

void wifi_provisioning_scan_and_print(void)
{
    printf("Scanning nearby Wi-Fi networks (2-3 seconds)...\n");
    uint16_t found = perform_wifi_scan();
    if (found == 0) {
        printf("No Wi-Fi networks found.\n");
        return;
    }

    printf("\n=== NEARBY WI-FI NETWORKS FOUND (%u) ===\n", found);
    printf("  #   SSID                             RSSI (dBm)  Channel  Security\n");
    printf(" --------------------------------------------------------------------\n");
    for (int i = 0; i < found; i++) {
        printf(" [%2d] %-32s  %4d dBm     %2d     %s\n",
               i + 1,
               s_scanned_aps[i].ssid,
               s_scanned_aps[i].rssi,
               s_scanned_aps[i].channel,
               authmode_to_str(s_scanned_aps[i].authmode));
    }
    printf(" --------------------------------------------------------------------\n");
    printf("Type 'wifi select <index> [password]' or 'wifi connect <ssid> [password]' to connect.\n\n");
}

int wifi_provisioning_scan_to_json(char *json_buf, size_t max_len)
{
    uint16_t found = perform_wifi_scan();
    int offset = snprintf(json_buf, max_len, "[");

    for (int i = 0; i < found; i++) {
        int written = snprintf(json_buf + offset, max_len - offset,
                               "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
                               (i > 0) ? "," : "",
                               s_scanned_aps[i].ssid,
                               s_scanned_aps[i].rssi,
                               (int)s_scanned_aps[i].authmode);
        if (written > 0) offset += written;
        if ((size_t)offset >= max_len - 10) break;
    }

    offset += snprintf(json_buf + offset, max_len - offset, "]");
    return offset;
}

bool wifi_provisioning_get_scanned_ssid(int index, char *out_ssid, size_t max_len)
{
    if (index < 1 || index > s_scanned_count) return false;
    strncpy(out_ssid, s_scanned_aps[index - 1].ssid, max_len - 1);
    out_ssid[max_len - 1] = '\0';
    return true;
}

static const char CAPTIVE_PORTAL_HTML[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Smart Clock Gateway Setup</title>"
"<style>"
"* { box-sizing: border-box; }"
"body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #0d1117; color: #c9d1d9; margin: 0; padding: 20px; display: flex; justify-content: center; align-items: center; min-height: 100vh; }"
".card { background: #161b22; padding: 24px; border-radius: 12px; border: 1px solid #30363d; width: 100%; max-width: 400px; box-shadow: 0 8px 24px rgba(0,0,0,0.5); }"
"h2 { color: #58a6ff; margin: 0 0 6px; text-align: center; font-size: 20px; }"
"p { color: #8b949e; font-size: 13px; text-align: center; margin: 0 0 16px; }"
"label { font-size: 13px; font-weight: 600; display: block; margin-bottom: 6px; color: #e6edf3; }"
"select, input { width: 100%; padding: 10px 12px; border-radius: 6px; border: 1px solid #30363d; background: #0d1117; color: #fff; font-size: 14px; margin-bottom: 14px; outline: none; }"
"select:focus, input:focus { border-color: #58a6ff; }"
".btn-scan { background: #21262d; border: 1px solid #30363d; color: #58a6ff; padding: 6px 10px; border-radius: 6px; cursor: pointer; font-size: 12px; font-weight: 600; }"
".btn-scan:hover { background: #30363d; }"
".btn-submit { width: 100%; padding: 12px; background: #238636; color: #fff; border: none; border-radius: 6px; font-weight: 600; font-size: 15px; cursor: pointer; margin-top: 6px; }"
".btn-submit:hover { background: #2ea043; }"
".footer { margin-top: 16px; text-align: center; font-size: 11px; color: #484f58; }"
"#status-msg { font-size: 12px; color: #8b949e; margin-bottom: 8px; min-height: 16px; }"
"</style></head>"
"<body><div class='card'>"
"<h2>Cấu Hình Wi-Fi</h2><p>Smart Clock & Industrial Gateway</p>"
"<form method='POST' action='/save'>"
"<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:6px;'>"
"<label style='margin:0;'>Chọn Mạng Wi-Fi</label>"
"<button type='button' class='btn-scan' onclick='fetchScan()'>&#x21bb; Quét lại</button>"
"</div>"
"<div id='status-msg'>Đang quét Wi-Fi lân cận...</div>"
"<select id='wifi-select' onchange='onSelectChange(this.value)'>"
"<option value='' disabled selected>-- Đang tìm sóng Wi-Fi... --</option>"
"</select>"
"<div id='manual-box' style='display:none;'>"
"<label>Tên Wi-Fi (SSID)</label>"
"<input type='text' id='manual-ssid' placeholder='Nhập tên mạng (kể cả mạng ẩn)'>"
"</div>"
"<input type='hidden' name='ssid' id='ssid-input'>"
"<label>Mật Khẩu</label>"
"<input type='password' name='password' id='password' placeholder='Nhập mật khẩu Wi-Fi'>"
"<input type='hidden' name='timezone' id='tz_field'>"
"<button type='submit' class='btn-submit'>Lưu & Kết Nối</button>"
"</form>"
"<div class='footer'>ESP32-S3 AICLP Gateway v1.1</div></div>"
"<script>"
"document.getElementById('tz_field').value = Intl.DateTimeFormat().resolvedOptions().timeZone || 'Asia/Ho_Chi_Minh';"
"function fetchScan() {"
"  var sel = document.getElementById('wifi-select');"
"  var msg = document.getElementById('status-msg');"
"  msg.innerText = 'Đang quét sóng Wi-Fi...';"
"  sel.innerHTML = '<option value=\"\" disabled selected>-- Đang quét... --</option>';"
"  fetch('/scan')"
"    .then(r => r.json())"
"    .then(list => {"
"      sel.innerHTML = '';"
"      if (!list || list.length === 0) {"
"        msg.innerText = 'Không tìm thấy Wi-Fi. Nhập thủ công:';"
"      } else {"
"        msg.innerText = 'Tìm thấy ' + list.length + ' mạng Wi-Fi:';"
"        var defOpt = document.createElement('option');"
"        defOpt.value = ''; defOpt.disabled = true; defOpt.selected = true;"
"        defOpt.text = '-- Chọn mạng Wi-Fi của bạn --';"
"        sel.appendChild(defOpt);"
"        list.forEach(item => {"
"          var opt = document.createElement('option');"
"          opt.value = item.ssid;"
"          var lock = item.auth > 0 ? ' [Khóa]' : ' [Mở]';"
"          opt.text = item.ssid + ' (' + item.rssi + ' dBm)' + lock;"
"          sel.appendChild(opt);"
"        });"
"      }"
"      var manualOpt = document.createElement('option');"
"      manualOpt.value = '__manual__';"
"      manualOpt.text = '+ [ Nhập mạng khác / Mạng ẩn ]';"
"      sel.appendChild(manualOpt);"
"    })"
"    .catch(err => {"
"      msg.innerText = 'Lỗi quét sóng. Vui lòng nhập tay:';"
"      sel.innerHTML = '<option value=\"__manual__\" selected>+ [ Nhập thủ công ]</option>';"
"      onSelectChange('__manual__');"
"    });"
"}"
"function onSelectChange(val) {"
"  var box = document.getElementById('manual-box');"
"  var hiddenSsid = document.getElementById('ssid-input');"
"  if (val === '__manual__') {"
"    box.style.display = 'block';"
"    hiddenSsid.value = document.getElementById('manual-ssid').value;"
"  } else {"
"    box.style.display = 'none';"
"    hiddenSsid.value = val;"
"  }"
"}"
"document.getElementById('manual-ssid').addEventListener('input', function(e) {"
"  if (document.getElementById('wifi-select').value === '__manual__') {"
"    document.getElementById('ssid-input').value = e.target.value;"
"  }"
"});"
"document.querySelector('form').addEventListener('submit', function(e) {"
"  var ssid = document.getElementById('ssid-input').value;"
"  if (!ssid || ssid === '__manual__') {"
"    alert('Vui lòng chọn hoặc nhập tên Wi-Fi!');"
"    e.preventDefault();"
"  }"
"});"
"fetchScan();"
"</script></body></html>";

static esp_err_t captive_portal_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, CAPTIVE_PORTAL_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t captive_portal_post_save_handler(httpd_req_t *req)
{
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = 0;

    char ssid[33] = {0};
    char password[65] = {0};

    char *p_ssid = strstr(buf, "ssid=");
    char *p_pass = strstr(buf, "password=");

    if (p_ssid) {
        p_ssid += 5;
        char *end = strchr(p_ssid, '&');
        int len = end ? (end - p_ssid) : strlen(p_ssid);
        if (len > 32) len = 32;
        strncpy(ssid, p_ssid, len);
    }
    if (p_pass) {
        p_pass += 9;
        char *end = strchr(p_pass, '&');
        int len = end ? (end - p_pass) : strlen(p_pass);
        if (len > 64) len = 64;
        strncpy(password, p_pass, len);
    }

    ESP_LOGI(TAG, "Received Wi-Fi credentials: SSID='%s'", ssid);
    config_manager_set_wifi(ssid, password);
    config_manager_save();

    const char *resp_str = "<html><body style='background:#0d1117;color:#fff;text-align:center;padding:50px;'>"
                           "<h2>Da luu!</h2><p>Dang ket noi toi Wi-Fi...</p></body></html>";
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    event_bus_post(EVT_WIFI_CREDENTIALS_RECEIVED, NULL, 0, false);
    vTaskDelay(pdMS_TO_TICKS(1000));
    wifi_provisioning_connect(ssid, password);
    return ESP_OK;
}

static esp_err_t captive_portal_scan_handler(httpd_req_t *req)
{
    char json_buf[1536];
    int len = wifi_provisioning_scan_to_json(json_buf, sizeof(json_buf));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_buf, len);
}

static void start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 12;
    config.stack_size = 8192;

    if (httpd_start(&s_http_server, &config) == ESP_OK) {
        httpd_uri_t get_uri = { .uri = "/", .method = HTTP_GET, .handler = captive_portal_get_handler };
        httpd_register_uri_handler(s_http_server, &get_uri);

        httpd_uri_t get_scan = { .uri = "/scan", .method = HTTP_GET, .handler = captive_portal_scan_handler };
        httpd_register_uri_handler(s_http_server, &get_scan);

        httpd_uri_t post_save = { .uri = "/save", .method = HTTP_POST, .handler = captive_portal_post_save_handler };
        httpd_register_uri_handler(s_http_server, &post_save);
        httpd_uri_t get_hotspot = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = captive_portal_get_handler };
        httpd_register_uri_handler(s_http_server, &get_hotspot);
        httpd_uri_t get_generate_204 = { .uri = "/generate_204", .method = HTTP_GET, .handler = captive_portal_get_handler };
        httpd_register_uri_handler(s_http_server, &get_generate_204);
        ESP_LOGI(TAG, "Captive Portal Web Server running on port 80");
    }
}

static void stop_web_and_dns(void)
{
    if (s_http_server) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }
    if (s_dns_sock >= 0) {
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    if (s_dns_task_handle) {
        vTaskDelete(s_dns_task_handle);
        s_dns_task_handle = NULL;
    }
}

static void stop_web_and_dns_task(void *pv)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    stop_web_and_dns();
    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (wifi_provisioning_is_configured()) {
            ESP_LOGI(TAG, "Wi-Fi Station started, connecting...");
            esp_wifi_connect();
        } else {
            ESP_LOGI(TAG, "Wi-Fi Station idle, waiting for configuration...");
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_is_connected = false;
        strcpy(s_current_ip, "0.0.0.0");
        ESP_LOGW(TAG, "[WIFI] Disconnected from AP.");
        event_bus_post(EVT_WIFI_LOST, NULL, 0, false);
        if (wifi_provisioning_is_configured()) {
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, s_current_ip, sizeof(s_current_ip));
        s_is_connected = true;

        printf("\n[WIFI] Connected, IP: %s\n", s_current_ip);
        fflush(stdout);

        event_bus_post(EVT_WIFI_CONNECTED, NULL, 0, false);

        if (s_in_softap_mode) {
            s_in_softap_mode = false;
            xTaskCreate(stop_web_and_dns_task, "stop_ap_task", 3072, NULL, 3, NULL);
        }
    }
}

void wifi_provisioning_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    s_netif_sta = esp_netif_create_default_wifi_sta();
    s_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    cfg.nvs_enable = false;
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_wifi_restore();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    const gateway_config_t *sys_cfg = config_manager_get();
    if (sys_cfg != NULL && strlen(sys_cfg->wifi_ssid) > 0) {
        ESP_LOGI(TAG, "Found stored Wi-Fi credentials: SSID='%s'", sys_cfg->wifi_ssid);
        wifi_provisioning_connect(sys_cfg->wifi_ssid, sys_cfg->wifi_password);
    } else {
        ESP_LOGI(TAG, "No Wi-Fi credentials (Volatile RAM mode). Starting SoftAP '%s'...", SOFTAP_SSID);
        s_in_softap_mode = true;

        wifi_config_t ap_config = {
            .ap = {
                .ssid = SOFTAP_SSID,
                .ssid_len = strlen(SOFTAP_SSID),
                .channel = 1,
                .password = "",
                .max_connection = SOFTAP_MAX_CONN,
                .authmode = WIFI_AUTH_OPEN
            },
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        xTaskCreate(dns_server_task, "dns_task", 3072, NULL, 5, &s_dns_task_handle);
        start_web_server();

        event_bus_post(EVT_WIFI_PROVISIONING_STARTED, NULL, 0, false);
        ESP_LOGI(TAG, "[PROVISIONING] SoftAP active. Connect to '%s' (IP: 192.168.4.1)", SOFTAP_SSID);
    }
}

bool wifi_provisioning_is_configured(void)
{
    const gateway_config_t *cfg = config_manager_get();
    return (cfg != NULL && strlen(cfg->wifi_ssid) > 0);
}

void wifi_provisioning_force_reset(void)
{
    ESP_LOGW(TAG, "Force reset Wi-Fi!");
    config_manager_set_wifi("", "");
    config_manager_save();

    esp_wifi_disconnect();
    esp_wifi_stop();

    s_in_softap_mode = true;
    wifi_config_t ap_config = {
        .ap = {
            .ssid = SOFTAP_SSID,
            .ssid_len = strlen(SOFTAP_SSID),
            .channel = 1,
            .password = "",
            .max_connection = SOFTAP_MAX_CONN,
            .authmode = WIFI_AUTH_OPEN
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    stop_web_and_dns();
    xTaskCreate(dns_server_task, "dns_task", 3072, NULL, 5, &s_dns_task_handle);
    start_web_server();

    event_bus_post(EVT_WIFI_PROVISIONING_STARTED, NULL, 0, false);
}

esp_err_t wifi_provisioning_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password != NULL && strlen(password) > 0) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_LOGI(TAG, "Setting Wi-Fi Station to SSID='%s'", ssid);
    esp_wifi_stop();
    ESP_ERROR_CHECK(esp_wifi_set_mode(s_in_softap_mode ? WIFI_MODE_APSTA : WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

bool wifi_provisioning_is_connected(void)
{
    return s_is_connected;
}

const char* wifi_provisioning_get_ip(void)
{
    return s_current_ip;
}
