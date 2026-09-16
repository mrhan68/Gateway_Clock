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
            xTaskCreate(stop_dns_task, "stop_dns_task", 3072, NULL, 3, NULL);
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

    // Web Server khoi dong san sang phuc vu ca SoftAP va Station
    web_server_start();

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

    stop_dns_only();
    xTaskCreate(dns_server_task, "dns_task", 3072, NULL, 5, &s_dns_task_handle);
    web_server_start();

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
