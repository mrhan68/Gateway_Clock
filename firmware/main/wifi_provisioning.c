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
#include "web_server.h"
#include "ble_adapter.h"
#include "esp_timer.h"

static const char *TAG = "WIFI_PROV";

#define SOFTAP_SSID      "SmartClock-Setup"
#define SOFTAP_MAX_CONN  4
#define DNS_PORT         53

static esp_netif_t *s_netif_sta = NULL;
static esp_netif_t *s_netif_ap = NULL;
static TaskHandle_t s_dns_task_handle = NULL;
static bool s_is_connected = false;
static char s_current_ip[16] = "0.0.0.0";
static bool s_in_softap_mode = false;
static int s_sta_retry_count = 0;
static int s_dns_sock = -1;
static esp_timer_handle_t s_ble_resume_timer = NULL;
static esp_timer_handle_t s_reconnect_timer = NULL;

static void wifi_reconnect_timer_cb(void *arg)
{
    if (wifi_provisioning_is_configured() && !s_is_connected) {
        ESP_LOGI(TAG, "[WIFI] Attempting scheduled reconnect to AP (retry #%d)...", s_sta_retry_count);
        esp_wifi_connect();
    }
}

static void ble_resume_timer_cb(void *arg)
{
    ESP_LOGI(TAG, "BLE scan resume timer expired. Resuming scan...");
    ble_adapter_resume_scan();
    if (s_ble_resume_timer) {
        esp_timer_stop(s_ble_resume_timer);
    }
}


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

        dns_header_t *req = (dns_header_t *)rx_buffer;
        dns_header_t *resp = (dns_header_t *)tx_buffer;

        // Parse Question section to find end of question
        int q_pos = sizeof(dns_header_t);
        while (q_pos < len && rx_buffer[q_pos] != 0) {
            q_pos += 1 + rx_buffer[q_pos];
        }
        q_pos += 1; // Skip null terminator
        if (q_pos + 4 > len) {
            continue;
        }

        uint16_t qtype = ((uint8_t)rx_buffer[q_pos] << 8) | (uint8_t)rx_buffer[q_pos + 1];
        q_pos += 4; // Skip QTYPE (2) + QCLASS (2)

        if (q_pos > len || q_pos + 16 > (int)sizeof(tx_buffer)) {
            continue;
        }

        // Copy Header and Question
        memcpy(tx_buffer, rx_buffer, q_pos);

        // Standard response header: QR=1, AA=1, RA=1, RCODE=0
        resp->flags = htons(0x8180);
        resp->qdcount = req->qdcount;
        resp->nscount = 0;
        resp->arcount = 0; // Strip EDNS0 / additional records

        int offset = q_pos;
        if (qtype == 0x0001 || qtype == 0x00FF) {
            // Type A (IPv4) or ANY: return 192.168.4.1
            resp->ancount = htons(1);
            tx_buffer[offset++] = 0xC0;
            tx_buffer[offset++] = 0x0C; // Pointer to QNAME at byte 12
            tx_buffer[offset++] = 0x00;
            tx_buffer[offset++] = 0x01; // Type A
            tx_buffer[offset++] = 0x00;
            tx_buffer[offset++] = 0x01; // Class IN
            tx_buffer[offset++] = 0x00;
            tx_buffer[offset++] = 0x00;
            tx_buffer[offset++] = 0x00;
            tx_buffer[offset++] = 0x3C; // TTL 60s
            tx_buffer[offset++] = 0x00;
            tx_buffer[offset++] = 0x04; // Length 4
            tx_buffer[offset++] = 192;
            tx_buffer[offset++] = 168;
            tx_buffer[offset++] = 4;
            tx_buffer[offset++] = 1;
        } else {
            // Non-A query (e.g. Type AAAA IPv6): return NOERROR with 0 answer records
            resp->ancount = 0;
        }

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

static int64_t s_last_scan_time_ms = 0;
static bool s_is_scanning = false;
static volatile bool s_suppress_sta_autoreconnect = false;

static void finish_wifi_scan_cleanup(void)
{
    s_suppress_sta_autoreconnect = false;
    s_is_scanning = false;

    if (s_is_connected) {
        ble_adapter_resume_scan();
    } else if (s_ble_resume_timer) {
        esp_timer_stop(s_ble_resume_timer);
        esp_timer_start_once(s_ble_resume_timer, 60000000ULL);
    }

    if (!s_is_connected && wifi_provisioning_is_configured()) {
        if (s_reconnect_timer == NULL) {
            esp_timer_create_args_t timer_args = {
                .callback = &wifi_reconnect_timer_cb,
                .name = "wifi_reconnect_timer"
            };
            esp_timer_create(&timer_args, &s_reconnect_timer);
        }
        if (s_reconnect_timer) {
            esp_timer_stop(s_reconnect_timer);
            esp_timer_start_once(s_reconnect_timer, 15000000ULL);
        }
    }
}

static uint16_t perform_wifi_scan_internal(bool force)
{
    int64_t now_ms = esp_timer_get_time() / 1000;
    if (!force && s_scanned_count > 0 && (now_ms - s_last_scan_time_ms) < 60000) {
        ESP_LOGI(TAG, "Returning %u cached Wi-Fi APs (age: %lld ms)", s_scanned_count, (long long)(now_ms - s_last_scan_time_ms));
        return s_scanned_count;
    }
    if (!force && s_last_scan_time_ms > 0 && (now_ms - s_last_scan_time_ms) < 3000) {
        ESP_LOGI(TAG, "Returning %u Wi-Fi APs from recent scan (%lld ms ago)", s_scanned_count, (long long)(now_ms - s_last_scan_time_ms));
        return s_scanned_count;
    }

    if (s_is_scanning) {
        ESP_LOGI(TAG, "Wi-Fi scan already in progress, returning %u existing APs", s_scanned_count);
        return s_scanned_count;
    }

    s_is_scanning = true;
    ble_adapter_pause_scan();

    if (!s_is_connected) {
        if (s_reconnect_timer) {
            esp_timer_stop(s_reconnect_timer);
        }
        s_suppress_sta_autoreconnect = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(60));
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 40,
        .scan_time.active.max = 80,
    };

    ESP_LOGI(TAG, "Starting Wi-Fi scan...");
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);
    if (err == ESP_ERR_WIFI_STATE) {
        s_suppress_sta_autoreconnect = true;
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(100));
        err = esp_wifi_scan_start(&scan_config, true);
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_start failed: 0x%x", err);
        finish_wifi_scan_cleanup();
        return s_scanned_count;
    }

    s_last_scan_time_ms = esp_timer_get_time() / 1000;

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        finish_wifi_scan_cleanup();
        return 0;
    }

    wifi_ap_record_t *records = malloc(ap_count * sizeof(wifi_ap_record_t));
    if (!records) {
        finish_wifi_scan_cleanup();
        return s_scanned_count;
    }

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
        s_scanned_aps[s_scanned_count].ssid[sizeof(s_scanned_aps[0].ssid) - 1] = '\0';
        s_scanned_aps[s_scanned_count].rssi = records[i].rssi;
        s_scanned_aps[s_scanned_count].authmode = records[i].authmode;
        s_scanned_aps[s_scanned_count].channel = records[i].primary;
        s_scanned_count++;
    }

    free(records);
    s_last_scan_time_ms = esp_timer_get_time() / 1000;
    finish_wifi_scan_cleanup();
    ESP_LOGI(TAG, "Wi-Fi scan finished: found %u valid unique APs", s_scanned_count);
    return s_scanned_count;
}

static void escape_json_string(const char *in, char *out, size_t max_out)
{
    size_t o = 0;
    for (size_t i = 0; in[i] != '\0' && o < max_out - 6; i++) {
        unsigned char c = (unsigned char)in[i];
        if (c < 32 || c == 127) {
            continue; // Bỏ qua ký tự điều khiển để JSON không bị lỗi parse trên trình duyệt điện thoại
        }
        if (c == '"' || c == '\\') {
            out[o++] = '\\';
        }
        out[o++] = c;
    }
    out[o] = '\0';
}

void wifi_provisioning_scan_and_print(void)
{
    printf("Scanning nearby Wi-Fi networks (fast scan)...\n");
    uint16_t found = perform_wifi_scan_internal(true);
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
    uint16_t found = perform_wifi_scan_internal(false);
    int offset = snprintf(json_buf, max_len, "[");

    for (int i = 0; i < found; i++) {
        char esc_ssid[68];
        escape_json_string(s_scanned_aps[i].ssid, esc_ssid, sizeof(esc_ssid));

        int written = snprintf(json_buf + offset, max_len - offset,
                               "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
                               (i > 0) ? "," : "",
                               esc_ssid,
                               s_scanned_aps[i].rssi,
                               (int)s_scanned_aps[i].authmode);
        if (written > 0 && written < (int)(max_len - offset)) {
            offset += written;
        } else {
            break;
        }
        if ((size_t)offset >= max_len - 10) break;
    }

    if ((size_t)offset < max_len - 1) {
        json_buf[offset++] = ']';
        json_buf[offset] = '\0';
    } else if (max_len > 1) {
        json_buf[max_len - 2] = ']';
        json_buf[max_len - 1] = '\0';
        offset = max_len - 1;
    }
    return offset;
}

uint16_t wifi_provisioning_rescan(void)
{
    return perform_wifi_scan_internal(true);
}

bool wifi_provisioning_get_scanned_ssid(int index, char *out_ssid, size_t max_len)
{
    if (index < 1 || index > s_scanned_count) return false;
    strncpy(out_ssid, s_scanned_aps[index - 1].ssid, max_len - 1);
    out_ssid[max_len - 1] = '\0';
    return true;
}

static void stop_dns_only(void)
{
    if (s_dns_sock >= 0) {
        close(s_dns_sock);
        s_dns_sock = -1;
    }
    if (s_dns_task_handle) {
        vTaskDelete(s_dns_task_handle);
        s_dns_task_handle = NULL;
    }
    ESP_LOGI(TAG, "DNS captive portal stopped (Web server remains active)");
}

static void stop_dns_task(void *pv)
{
    vTaskDelay(pdMS_TO_TICKS(500));
    stop_dns_only();
    vTaskDelete(NULL);
}

static void background_scan_task(void *pv)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
    int wait_loops = 0;
    while (wifi_provisioning_is_configured() && !s_is_connected && s_sta_retry_count == 0 && wait_loops < 25) {
        vTaskDelay(pdMS_TO_TICKS(200));
        wait_loops++;
    }
    ESP_LOGI(TAG, "[WIFI_PRESCAN] Performing initial background Wi-Fi scan...");
    perform_wifi_scan_internal(true);
    ESP_LOGI(TAG, "[WIFI_PRESCAN] Background scan complete: %u APs found", s_scanned_count);
    vTaskDelete(NULL);
}

static void start_softap_internal(bool force_reconfig)
{
    if (s_in_softap_mode && !force_reconfig) return;
    s_in_softap_mode = true;

    const gateway_config_t *sys_cfg = config_manager_get();
    bool has_custom_pass = (sys_cfg && sys_cfg->softap_password[0] != '\0');

    wifi_config_t ap_config = {
        .ap = {
            .ssid = SOFTAP_SSID,
            .ssid_len = strlen(SOFTAP_SSID),
            .channel = 1,
            .beacon_interval = 100, // 100 TU = 102.4ms (Required for phone compatibility)
            .dtim_period = 2,       // DTIM period 2
            .max_connection = SOFTAP_MAX_CONN,
            .authmode = has_custom_pass ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
            .pairwise_cipher = has_custom_pass ? WIFI_CIPHER_TYPE_CCMP : WIFI_CIPHER_TYPE_NONE,
            .pmf_cfg = {
                .required = false,
                .capable = false
            }
        },
    };
    if (has_custom_pass) {
        strncpy((char *)ap_config.ap.password, sys_cfg->softap_password, sizeof(ap_config.ap.password) - 1);
    } else {
        ap_config.ap.password[0] = '\0';
    }

    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_set_bandwidth(WIFI_IF_AP, WIFI_BW_HT20);
    esp_wifi_set_protocol(WIFI_IF_AP, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
    esp_wifi_start();
    esp_wifi_set_max_tx_power(80); // Maximum RF transmit power (20dBm)
    esp_wifi_set_ps(WIFI_PS_NONE); // Disable modem sleep so SoftAP never drops beacons

    stop_dns_only();
    if (!s_is_connected) {
        xTaskCreate(dns_server_task, "dns_task", 4096, NULL, 5, &s_dns_task_handle);
        // WP-09: Tạm dừng BLE scanner để nhường 100% tài nguyên RF cho SoftAP bắt tay điện thoại
        ble_adapter_pause_scan();
        if (s_ble_resume_timer == NULL) {
            esp_timer_create_args_t timer_args = {
                .callback = &ble_resume_timer_cb,
                .name = "ble_res_tmr"
            };
            esp_timer_create(&timer_args, &s_ble_resume_timer);
        }
        esp_timer_start_once(s_ble_resume_timer, 60000000ULL); // 60 seconds
        xTaskCreate(background_scan_task, "bg_scan_task", 4096, NULL, 3, NULL);
    }
    web_server_start();

    event_bus_post(EVT_WIFI_PROVISIONING_STARTED, NULL, 0, false);
    if (has_custom_pass) {
        ESP_LOGI(TAG, "[PROVISIONING] SoftAP active (WPA2). Connect to '%s' (IP: 192.168.4.1)", SOFTAP_SSID);
    } else {
        ESP_LOGI(TAG, "[PROVISIONING] SoftAP active (OPEN). Connect to '%s' (No Password, IP: 192.168.4.1)", SOFTAP_SSID);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi Station started (connect handled by wifi_provisioning_connect when configured).");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t *)event_data;
        s_is_connected = false;
        strcpy(s_current_ip, "0.0.0.0");
        if (s_suppress_sta_autoreconnect || s_is_scanning) {
            ESP_LOGI(TAG, "[WIFI] STA disconnected for Wi-Fi scan (Reason: %d), suppressing auto-reconnect.", disconn ? disconn->reason : -1);
            return;
        }
        ESP_LOGW(TAG, "[WIFI] Disconnected from AP (Reason code: %d).", disconn ? disconn->reason : -1);
        event_bus_post(EVT_WIFI_LOST, NULL, 0, false);

        s_sta_retry_count++;
        if (s_sta_retry_count >= 3 && !s_in_softap_mode) {
            ESP_LOGW(TAG, "STA connection failed (%d retries). Starting SoftAP fallback...", s_sta_retry_count);
            start_softap_internal(false);
        }

        if (wifi_provisioning_is_configured()) {
            // Chống Reconnect Storm: Áp dụng Exponential Backoff (1s, 2s, 4s, 8s, tối đa 30s)
            uint32_t delay_sec = 1;
            if (s_sta_retry_count > 1) {
                delay_sec = 1 << (s_sta_retry_count > 5 ? 5 : (s_sta_retry_count - 1));
                if (delay_sec > 30) delay_sec = 30;
            }
            ESP_LOGI(TAG, "[WIFI] Scheduling reconnect in %lu second(s) (retry #%d)...", (unsigned long)delay_sec, s_sta_retry_count);
            if (s_reconnect_timer == NULL) {
                esp_timer_create_args_t timer_args = {
                    .callback = &wifi_reconnect_timer_cb,
                    .name = "wifi_reconnect_timer"
                };
                esp_timer_create(&timer_args, &s_reconnect_timer);
            }
            if (s_reconnect_timer) {
                esp_timer_stop(s_reconnect_timer);
                esp_timer_start_once(s_reconnect_timer, (uint64_t)delay_sec * 1000000ULL);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_ip4addr_ntoa(&event->ip_info.ip, s_current_ip, sizeof(s_current_ip));
        s_is_connected = true;
        if (s_reconnect_timer) {
            esp_timer_stop(s_reconnect_timer);
        }
        s_sta_retry_count = 0;

        printf("\n[WIFI] Connected, IP: %s\n", s_current_ip);
        fflush(stdout);

        event_bus_post(EVT_WIFI_CONNECTED, NULL, 0, false);

        // WP-08 & FR-WIFI-01: SoftAP duy trì liên tục (Concurrent AP+STA)
        // Chỉ dừng DNS redirect để giải phóng port 53 khi đã có IP Station
        if (s_dns_sock >= 0) {
            xTaskCreate(stop_dns_task, "stop_dns_task", 3072, NULL, 3, NULL);
        }

        // BA-02 & WP-09: Wi-Fi đã kết nối ổn định -> Kích hoạt lại BLE scanner với 30% duty cycle
        ble_adapter_resume_scan();
        if (s_ble_resume_timer) {
            esp_timer_stop(s_ble_resume_timer);
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

    // Web Server khởi động sẵn sàng phục vụ cả SoftAP và Station
    web_server_start();

    // FSD v1.7 §4.4 WP-08 & FR-WIFI-01: SoftAP luôn bật (Concurrent AP+STA)
    start_softap_internal(false);

    const gateway_config_t *sys_cfg = config_manager_get();
    if (sys_cfg != NULL && strlen(sys_cfg->wifi_ssid) > 0) {
        ESP_LOGI(TAG, "Found stored Wi-Fi credentials: SSID='%s'", sys_cfg->wifi_ssid);
        wifi_provisioning_connect(sys_cfg->wifi_ssid, sys_cfg->wifi_password);
    } else {
        ESP_LOGI(TAG, "No Wi-Fi credentials (Volatile RAM mode). SoftAP '%s' ready for setup.", SOFTAP_SSID);
    }
}

bool wifi_provisioning_is_configured(void)
{
    const gateway_config_t *cfg = config_manager_get();
    return (cfg != NULL && strlen(cfg->wifi_ssid) > 0);
}

void wifi_provisioning_force_reset(void)
{
    ESP_LOGW(TAG, "[FACTORY_RESET] Force reset Wi-Fi & restoring SoftAP credentials...");
    config_manager_factory_reset();

    if (s_reconnect_timer) {
        esp_timer_stop(s_reconnect_timer);
    }
    esp_wifi_disconnect();
    s_is_connected = false;
    strcpy(s_current_ip, "0.0.0.0");

    s_in_softap_mode = false;
    start_softap_internal(false);
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
    if (s_reconnect_timer) {
        esp_timer_stop(s_reconnect_timer);
    }
    s_sta_retry_count = 0;
    start_softap_internal(false);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    esp_wifi_connect();

    return ESP_OK;
}

bool wifi_provisioning_is_connected(void)
{
    return s_is_connected;
}

bool wifi_provisioning_is_softap(void)
{
    return true; // FSD v1.7 WP-08: SoftAP duy trì liên tục
}

const char* wifi_provisioning_get_ip(void)
{
    if (s_is_connected) {
        return s_current_ip;
    }
    return "192.168.4.1";
}

void wifi_provisioning_apply_softap_config(void)
{
    start_softap_internal(true);
}
