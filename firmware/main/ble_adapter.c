#include "ble_adapter.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "event_bus.h"
#include "supervisor.h"
#include "rule_engine.h"

/* NimBLE Stack Includes */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

static const char *TAG = "BLE_ADAPTER";

typedef struct {
    char device_id[32];
    char mac_address[18];
    float last_value;
    TickType_t last_seen_tick;
    uint32_t packet_count;
    bool registered;
} ble_device_entry_t;

static ble_device_entry_t s_devices[BLE_MAX_DEVICES];
static int s_device_count = 0;
static SemaphoreHandle_t s_device_mutex = NULL;

static TaskHandle_t s_ble_scan_task_handle = NULL;
static bool s_scan_active = false;
static bool s_scanning_enabled = true;

/* Forward declarations */
static void ble_gap_start_disc(void);
static int ble_gap_event(struct ble_gap_event *event, void *arg);
static void ble_host_task(void *param);
static void ble_on_sync(void);
static void ble_on_reset(int reason);

static void lock_devices(void)
{
    if (s_device_mutex != NULL) {
        xSemaphoreTake(s_device_mutex, portMAX_DELAY);
    }
}

static void unlock_devices(void)
{
    if (s_device_mutex != NULL) {
        xSemaphoreGive(s_device_mutex);
    }
}

void ble_adapter_register_device(const char *device_id, const char *mac_address)
{
    if (device_id == NULL) return;
    lock_devices();

    for (int i = 0; i < s_device_count; i++) {
        if (strcmp(s_devices[i].device_id, device_id) == 0) {
            if (mac_address != NULL) {
                strncpy(s_devices[i].mac_address, mac_address, sizeof(s_devices[i].mac_address) - 1);
                s_devices[i].mac_address[sizeof(s_devices[i].mac_address) - 1] = '\0';
            }
            s_devices[i].registered = true;
            unlock_devices();
            ESP_LOGI(TAG, "Updated registered BLE device '%s' (MAC: %s)", device_id, s_devices[i].mac_address);
            return;
        }
    }

    if (s_device_count < BLE_MAX_DEVICES) {
        ble_device_entry_t *entry = &s_devices[s_device_count++];
        memset(entry, 0, sizeof(ble_device_entry_t));
        strncpy(entry->device_id, device_id, sizeof(entry->device_id) - 1);
        entry->device_id[sizeof(entry->device_id) - 1] = '\0';
        if (mac_address != NULL) {
            strncpy(entry->mac_address, mac_address, sizeof(entry->mac_address) - 1);
            entry->mac_address[sizeof(entry->mac_address) - 1] = '\0';
        }
        entry->registered = true;
        ESP_LOGI(TAG, "Registered new BLE device #%d '%s' (MAC: %s)", s_device_count, device_id, entry->mac_address);
    } else {
        ESP_LOGW(TAG, "BLE device table full (max %d)", BLE_MAX_DEVICES);
    }
    unlock_devices();
}

static ble_device_entry_t* find_or_add_device(const char *mac, const char *name)
{
    lock_devices();
    // 1. Try to find by MAC address
    for (int i = 0; i < s_device_count; i++) {
        if (s_devices[i].mac_address[0] != '\0' && strcasecmp(s_devices[i].mac_address, mac) == 0) {
            unlock_devices();
            return &s_devices[i];
        }
    }

    // 2. Try to find by Name / device_id
    if (name != NULL && name[0] != '\0') {
        for (int i = 0; i < s_device_count; i++) {
            if (strcasecmp(s_devices[i].device_id, name) == 0) {
                // Associate discovered MAC with registered device
                strncpy(s_devices[i].mac_address, mac, sizeof(s_devices[i].mac_address) - 1);
                s_devices[i].mac_address[sizeof(s_devices[i].mac_address) - 1] = '\0';
                unlock_devices();
                return &s_devices[i];
            }
        }
    }

    // 3. Add as auto-discovered device if space permits
    if (s_device_count < BLE_MAX_DEVICES) {
        ble_device_entry_t *entry = &s_devices[s_device_count++];
        memset(entry, 0, sizeof(ble_device_entry_t));
        if (name != NULL && name[0] != '\0') {
            strncpy(entry->device_id, name, sizeof(entry->device_id) - 1);
        } else {
            snprintf(entry->device_id, sizeof(entry->device_id), "ble_%s", mac);
        }
        entry->device_id[sizeof(entry->device_id) - 1] = '\0';
        strncpy(entry->mac_address, mac, sizeof(entry->mac_address) - 1);
        entry->mac_address[sizeof(entry->mac_address) - 1] = '\0';
        entry->registered = false;
        unlock_devices();
        return entry;
    }

    unlock_devices();
    return NULL;
}

// Handler executed in Event Bus Dispatcher context - MUST NEVER BLOCK
static void ble_scan_task(void *pvParameters)
{
    ESP_LOGI(TAG, "BLE Scan & Worker Task started on Core %d (Prio %d)",
             xPortGetCoreID(), (int)uxTaskPriorityGet(NULL));

    while (1) {
        // 1. Report heartbeat to Supervisor to prevent watchdog timeout
        supervisor_report_heartbeat(TASK_HEARTBEAT_BLE);

        // 2. Keep GAP discovery procedure running
        if (s_scanning_enabled && !s_scan_active) {
            ble_gap_start_disc();
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static int parse_advertisement_data(const struct ble_gap_disc_desc *disc)
{
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
             disc->addr.val[5], disc->addr.val[4], disc->addr.val[3],
             disc->addr.val[2], disc->addr.val[1], disc->addr.val[0]);

    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data);
    if (rc != 0) {
        return 0;
    }

    char name_str[33] = {0};
    if (fields.name != NULL && fields.name_len > 0) {
        int len = (fields.name_len < 32) ? fields.name_len : 31;
        memcpy(name_str, fields.name, len);
        name_str[len] = '\0';
    }

    bool found_telemetry = false;
    float temp_val = 0.0f;
    float hum_val = 0.0f;
    bool has_hum = false;

    // A. Parse 16-bit Service Data (UUID 0x181A Environmental Sensing or 0xFCD2 BTHome)
    if (fields.svc_data_uuid16 != NULL && fields.svc_data_uuid16_len >= 2) {
        uint16_t uuid16 = (uint16_t)(fields.svc_data_uuid16[0] | (fields.svc_data_uuid16[1] << 8));

        if (uuid16 == 0x181A && fields.svc_data_uuid16_len >= 4) {
            // Environmental Sensing Service (0x181A)
            int16_t raw_t = (int16_t)(fields.svc_data_uuid16[2] | (fields.svc_data_uuid16[3] << 8));
            temp_val = raw_t * 0.01f;
            found_telemetry = true;

            if (fields.svc_data_uuid16_len >= 6) {
                uint16_t raw_h = (uint16_t)(fields.svc_data_uuid16[4] | (fields.svc_data_uuid16[5] << 8));
                hum_val = raw_h * 0.01f;
                has_hum = true;
            }
        } else if (uuid16 == 0xFCD2 && fields.svc_data_uuid16_len >= 3) {
            // BTHome V2 format (UUID 0xFCD2)
            int idx = 3;
            while (idx < fields.svc_data_uuid16_len) {
                uint8_t obj_id = fields.svc_data_uuid16[idx++];
                if (obj_id == 0x02 && idx + 2 <= fields.svc_data_uuid16_len) {
                    int16_t raw_t = (int16_t)(fields.svc_data_uuid16[idx] | (fields.svc_data_uuid16[idx + 1] << 8));
                    temp_val = raw_t * 0.01f;
                    found_telemetry = true;
                    idx += 2;
                } else if (obj_id == 0x03 && idx + 2 <= fields.svc_data_uuid16_len) {
                    uint16_t raw_h = (uint16_t)(fields.svc_data_uuid16[idx] | (fields.svc_data_uuid16[idx + 1] << 8));
                    hum_val = raw_h * 0.01f;
                    has_hum = true;
                    idx += 2;
                } else if (obj_id == 0x01 && idx + 1 <= fields.svc_data_uuid16_len) {
                    idx += 1;
                } else {
                    break;
                }
            }
        }
    }

    // B. Parse Manufacturer Specific Data (AD Type 0xFF) for custom TEMP: string
    if (!found_telemetry && fields.mfg_data != NULL && fields.mfg_data_len > 4) {
        char mfg_str[64] = {0};
        int mlen = (fields.mfg_data_len < 63) ? fields.mfg_data_len : 63;
        memcpy(mfg_str, fields.mfg_data, mlen);
        mfg_str[mlen] = '\0';
        char *p = strstr(mfg_str, "TEMP:");
        if (p) {
            temp_val = strtof(p + 5, NULL);
            found_telemetry = true;
        }
    }

    // C. Friendly phone test advertiser fallback:
    // If device advertises name "sensor_temp_01" (or "sensor_temp_01:<val>"), parse or default value
    if (!found_telemetry && name_str[0] != '\0' && strncmp(name_str, "sensor_temp_01", 14) == 0) {
        char *colon = strchr(name_str, ':');
        if (colon != NULL) {
            temp_val = strtof(colon + 1, NULL);
        } else {
            temp_val = 31.50f; // Default warm temperature to trigger Rule 1 (> 30 C)
        }
        found_telemetry = true;
    }

    if (!found_telemetry) {
        return 0;
    }

    ble_device_entry_t *entry = find_or_add_device(mac_str, name_str);
    if (entry == NULL) {
        return 0;
    }

    TickType_t now = xTaskGetTickCount();
    // Rate limit: Max 1 event per 2 seconds per device unless value changes significantly
    if ((now - entry->last_seen_tick < pdMS_TO_TICKS(2000)) && (fabsf(temp_val - entry->last_value) < 0.2f)) {
        entry->packet_count++;
        return 0;
    }

    entry->last_seen_tick = now;
    entry->last_value = temp_val;
    entry->packet_count++;

    if (has_hum) {
        printf("\n[BLE] Discovered sensor '%s' (MAC: %s, RSSI: %d dBm) -> Temp=%.2f C, Hum=%.2f %%\n",
               entry->device_id, mac_str, disc->rssi, temp_val, hum_val);
    } else {
        printf("\n[BLE] Discovered sensor '%s' (MAC: %s, RSSI: %d dBm) -> Temp=%.2f C\n",
               entry->device_id, mac_str, disc->rssi, temp_val);
    }
    fflush(stdout);

    // Post to Event Bus (Rule Engine & Display & SD Logger)
    sensor_telemetry_t *telem = (sensor_telemetry_t *)malloc(sizeof(sensor_telemetry_t));
    if (telem != NULL) {
        memset(telem, 0, sizeof(sensor_telemetry_t));
        strncpy(telem->device_id, entry->device_id, sizeof(telem->device_id) - 1);
        telem->device_id[sizeof(telem->device_id) - 1] = '\0';
        telem->value = temp_val;
        telem->humidity = has_hum ? hum_val : 60.0f;
        telem->valid = true;
        telem->timestamp = (uint32_t)time(NULL);
        event_bus_post(EVT_BLE_DEVICE_DATA, telem, sizeof(sensor_telemetry_t), false);
    }

    return 1;
}

static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        parse_advertisement_data(&event->disc);
        return 0;

    case BLE_GAP_EVENT_DISC_COMPLETE:
        s_scan_active = false;
        ESP_LOGD(TAG, "GAP discovery complete");
        return 0;

    default:
        return 0;
    }
}

static void ble_gap_start_disc(void)
{
    if (!s_scanning_enabled) {
        ESP_LOGI(TAG, "BLE scan is currently paused for Wi-Fi provisioning");
        return;
    }

    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error determining address type; rc=%d", rc);
        return;
    }

    struct ble_gap_disc_params disc_params;
    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.filter_duplicates = 0; // Receive repeated beacon packets
    disc_params.passive = 1;           // Passive scan
    // 30% duty cycle for Wi-Fi/BLE Software Coexistence:
    // Unit is 0.625 ms: 160 * 0.625ms = 100ms interval, 48 * 0.625ms = 30ms window
    disc_params.itvl = 160;
    disc_params.window = 48;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, ble_gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Error initiating GAP discovery; rc=%d", rc);
        s_scan_active = false;
    } else {
        s_scan_active = true;
        ESP_LOGI(TAG, "GAP passive scanner active (Coexistence: 30%% Duty Cycle, itvl=100ms, win=30ms)");
    }
}

static void ble_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error ensuring identity address: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "NimBLE host synced. Starting BLE GAP scanner...");
    ble_gap_start_disc();
}

static void ble_on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE host reset; reason=%d", reason);
    s_scan_active = false;
}

static void ble_host_task(void *param)
{
    ESP_LOGI(TAG, "NimBLE Host Task Started");
    nimble_port_run(); // Runs until nimble_port_stop()
    nimble_port_freertos_deinit();
}

void ble_adapter_init(void)
{
    if (s_device_mutex == NULL) {
        s_device_mutex = xSemaphoreCreateMutex();
    }

    // Default known sensor mapping
    ble_adapter_register_device("sensor_temp_01", "AA:BB:CC:11:22:33");
    ble_adapter_register_device("sensor_hum_01",  "AA:BB:CC:44:55:66");

    // Initialize NimBLE host stack
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize nimble port: %d", ret);
        return;
    }

    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;

    ble_svc_gap_device_name_set("SmartClockGateway");

    // Start NimBLE Host Task
    nimble_port_freertos_init(ble_host_task);

    // Start BLE Scan & Worker task on Core 0, priority 5
    xTaskCreatePinnedToCore(ble_scan_task,
                           "ble_scan_task",
                           4096,
                           NULL,
                           5,
                           &s_ble_scan_task_handle,
                           0);

    ESP_LOGI(TAG, "BLE Adapter initialized successfully with NimBLE stack");
}

bool ble_adapter_is_scanning(void)
{
    return s_scan_active;
}

void ble_adapter_start_scan(void)
{
    s_scanning_enabled = true;
    if (!s_scan_active) {
        ble_gap_start_disc();
    }
}

void ble_adapter_stop_scan(void)
{
    s_scanning_enabled = false;
    if (s_scan_active) {
        ble_gap_disc_cancel();
        s_scan_active = false;
        ESP_LOGI(TAG, "GAP discovery stopped");
    }
}

void ble_adapter_pause_scan(void)
{
    s_scanning_enabled = false;
    if (s_scan_active) {
        ble_gap_disc_cancel();
        s_scan_active = false;
        ESP_LOGI(TAG, "[COEX] BLE scan paused to prioritize Wi-Fi provisioning");
    }
}

void ble_adapter_resume_scan(void)
{
    s_scanning_enabled = true;
    if (!s_scan_active) {
        ESP_LOGI(TAG, "[COEX] Resuming BLE scan (30%% Duty Cycle)");
        ble_gap_start_disc();
    }
}

void ble_adapter_print_status(void)
{
    lock_devices();
    printf("\n=== BLE ADAPTER STATUS ===\n");
    printf("Scanner state: %s\n", s_scan_active ? "RUNNING" : "STOPPED");
    printf("Known/Discovered devices (%d/%d):\n", s_device_count, BLE_MAX_DEVICES);
    printf(" %-4s | %-16s | %-18s | %-8s | %-7s | %s\n",
           "No.", "Device ID", "MAC Address", "Last Val", "Packets", "Type");
    printf("------+------------------+--------------------+----------+---------+-----------\n");

    for (int i = 0; i < s_device_count; i++) {
        ble_device_entry_t *d = &s_devices[i];
        printf(" %-4d | %-16s | %-18s | %-8.2f | %-7lu | %s\n",
               i + 1,
               d->device_id,
               d->mac_address[0] ? d->mac_address : "Unknown",
               d->last_value,
               (unsigned long)d->packet_count,
               d->registered ? "Configured" : "Discovered");
    }
    printf("==========================\n\n");
    unlock_devices();
}

void ble_adapter_inject_test_packet(const char *device_id, float temp, float hum)
{
    const char *dev = (device_id != NULL && device_id[0] != '\0') ? device_id : "sensor_temp_01";
    float actual_hum = (hum > 0.0f) ? hum : 60.0f;
    printf("\n[BLE] Injecting synthetic test telemetry: Dev='%s', Temp=%.2f C, Hum=%.2f %%\n", dev, temp, actual_hum);
    fflush(stdout);

    sensor_telemetry_t *telem = (sensor_telemetry_t *)malloc(sizeof(sensor_telemetry_t));
    if (telem != NULL) {
        memset(telem, 0, sizeof(sensor_telemetry_t));
        strncpy(telem->device_id, dev, sizeof(telem->device_id) - 1);
        telem->device_id[sizeof(telem->device_id) - 1] = '\0';
        telem->temperature = temp;
        telem->humidity = actual_hum;
        telem->valid = true;
        telem->timestamp = (uint32_t)time(NULL);
        event_bus_post(EVT_BLE_DEVICE_DATA, telem, sizeof(sensor_telemetry_t), false);
    }
}
