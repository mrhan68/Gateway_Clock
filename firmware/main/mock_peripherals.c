#include "mock_peripherals.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "event_bus.h"
#include "rtc.h"
#include "rule_engine.h"
#include "supervisor.h"

static const char *TAG = "MOCK_PERIPH";

#define MOCK_TASK_STACK_SIZE 4096
#define MOCK_TASK_PRIO       3
#define MOCK_TASK_CORE       0

// State of Mock RS485
static TaskHandle_t s_mock_task_handle = NULL;
static SemaphoreHandle_t s_mock_mutex = NULL;
static bool s_mock_rs485_running = false;
static uint32_t s_mock_poll_interval_ms = 4000;
static uint32_t s_mock_poll_count = 0;
static float s_manual_temp = -999.0f;
static float s_manual_hum = -999.0f;
static bool s_manual_valid = true;
static char s_fault_mode[16] = "none"; // "none", "timeout", "invalid"
static bool s_mock_frozen = false;

// State of Mock Relay Board (Slave ID 2)
static uint8_t s_mock_relay_coils[4] = {0, 0, 0, 0};

// State of Mock Storage
static char s_mock_csv_log[10][96];
static int s_mock_csv_count = 0;

static void mock_rs485_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Mock RS485 polling task running on Core %d", xPortGetCoreID());

    while (1) {
        if (s_mock_frozen) {
            // FSD §4.13.4 FR-SUP-02: Deliberately freeze task to test watchdog stall detection
            vTaskDelay(portMAX_DELAY);
        }

        supervisor_report_heartbeat(TASK_HEARTBEAT_RS485);

        if (!s_mock_rs485_running) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        s_mock_poll_count++;

        // 1. Simulate Bus Timeout Fault Injection
        if (strcmp(s_fault_mode, "timeout") == 0) {
            ESP_LOGW(TAG, "[MOCK_RS485] Bus timeout simulated! Slave #1 did not respond.");
            vTaskDelay(pdMS_TO_TICKS(s_mock_poll_interval_ms));
            continue;
        }

        // 2. Compute Temperature & Humidity
        bool is_valid = true;
        float temp, hum;
        if (s_manual_temp > -100.0f) {
            temp = s_manual_temp;
            hum = s_manual_hum;
            is_valid = s_manual_valid;
        } else {
            // Sine wave oscillation: 28 +/- 5 deg C, 65 +/- 15 %RH
            temp = 28.0f + 5.0f * sinf((float)s_mock_poll_count * 0.3f);
            hum = 65.0f + 15.0f * cosf((float)s_mock_poll_count * 0.3f);
        }

        // Fault injection override: "invalid" forces valid = false (FSD §4.7.5 Safe-Default)
        if (strcmp(s_fault_mode, "invalid") == 0) {
            is_valid = false;
        }

        time_t now = app_rtc_get_time();

        printf("\n[MOCK_RS485] Polled Slave 0x01 (FC03): Temp=%.1f C, Hum=%.1f %% (CRC: %s)\n",
               temp, hum, is_valid ? "VALID" : "INVALID");
        fflush(stdout);

        // 3. Post Temperature Telemetry (FSD §4.3)
        rs485_telemetry_t *t_temp = (rs485_telemetry_t *)malloc(sizeof(rs485_telemetry_t));
        if (t_temp != NULL) {
            strncpy(t_temp->device_id, "sensor_temp_01", sizeof(t_temp->device_id) - 1);
            t_temp->device_id[sizeof(t_temp->device_id) - 1] = '\0';
            t_temp->slave_id = 1;
            t_temp->reg_addr = 0x0000;
            t_temp->value = temp;
            t_temp->valid = is_valid;
            t_temp->timestamp = (uint32_t)now;

            event_bus_post(EVT_RS485_DEVICE_DATA, t_temp, sizeof(rs485_telemetry_t), false);
        }

        // 4. Post Humidity Telemetry (FSD §4.3)
        rs485_telemetry_t *t_hum = (rs485_telemetry_t *)malloc(sizeof(rs485_telemetry_t));
        if (t_hum != NULL) {
            strncpy(t_hum->device_id, "sensor_hum_01", sizeof(t_hum->device_id) - 1);
            t_hum->device_id[sizeof(t_hum->device_id) - 1] = '\0';
            t_hum->slave_id = 1;
            t_hum->reg_addr = 0x0001;
            t_hum->value = hum;
            t_hum->valid = is_valid;
            t_hum->timestamp = (uint32_t)now;

            event_bus_post(EVT_RS485_DEVICE_DATA, t_hum, sizeof(rs485_telemetry_t), false);
        }

        // 5. Append to Mock Storage CSV (under mutex)
        if (s_mock_mutex != NULL && xSemaphoreTake(s_mock_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (s_mock_csv_count < 10) {
                snprintf(s_mock_csv_log[s_mock_csv_count++], 96,
                         "%ld,sensor_temp_01,%.1f,C,%s",
                         (long)now, temp, is_valid ? "OK" : "ERR");
            } else {
                for (int i = 0; i < 9; i++) {
                    strncpy(s_mock_csv_log[i], s_mock_csv_log[i + 1], 96);
                }
                snprintf(s_mock_csv_log[9], 96,
                         "%ld,sensor_temp_01,%.1f,C,%s",
                         (long)now, temp, is_valid ? "OK" : "ERR");
            }
            xSemaphoreGive(s_mock_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(s_mock_poll_interval_ms));
    }
}

static void on_rule_action_mock(const gateway_event_t *evt)
{
    if (evt != NULL && evt->data != NULL) {
        const rule_action_request_t *act = (const rule_action_request_t *)evt->data;
        if (s_mock_mutex != NULL) xSemaphoreTake(s_mock_mutex, portMAX_DELAY);
        if (strcmp(act->target_device_id, "fan_relay_01") == 0) {
            s_mock_relay_coils[0] = (strcmp(act->action_command, "ON") == 0) ? 1 : 0;
            printf("[MOCK_RS485] Slave #2 Coil #1 (fan_relay_01) set to: %s\n",
                   s_mock_relay_coils[0] ? "CLOSED (ON)" : "OPEN (OFF)");
        } else if (strcmp(act->target_device_id, "heater_relay_01") == 0) {
            s_mock_relay_coils[1] = (strcmp(act->action_command, "ON") == 0) ? 1 : 0;
            printf("[MOCK_RS485] Slave #2 Coil #2 (heater_relay_01) set to: %s\n",
                   s_mock_relay_coils[1] ? "CLOSED (ON)" : "OPEN (OFF)");
        } else if (strcmp(act->target_device_id, "dehumidifier_01") == 0) {
            s_mock_relay_coils[2] = (strcmp(act->action_command, "START") == 0 || strcmp(act->action_command, "ON") == 0) ? 1 : 0;
            printf("[MOCK_RS485] Slave #2 Coil #3 (dehumidifier_01) set to: %s\n",
                   s_mock_relay_coils[2] ? "CLOSED (ON)" : "OPEN (OFF)");
        } else {
            s_mock_relay_coils[3] = 1;
            printf("[MOCK_RS485] Slave #2 Coil #4 (generic '%s') executed CMD: '%s'\n",
                   act->target_device_id, act->action_command);
        }
        if (s_mock_mutex != NULL) xSemaphoreGive(s_mock_mutex);
        fflush(stdout);
    }
}

void mock_peripherals_init(void)
{
    ESP_LOGI(TAG, "Initializing Mock Peripherals Subsystem...");
    if (s_mock_mutex == NULL) {
        s_mock_mutex = xSemaphoreCreateMutex();
    }
    event_bus_register_handler(EVT_RULE_ACTION_REQUEST, on_rule_action_mock);

    xTaskCreatePinnedToCore(
        mock_rs485_task,
        "mock_rs485_task",
        MOCK_TASK_STACK_SIZE,
        NULL,
        MOCK_TASK_PRIO,
        &s_mock_task_handle,
        MOCK_TASK_CORE
    );

    ESP_LOGI(TAG, "Mock Peripherals initialized with dedicated task (4KB stack)");
}

void mock_rs485_start(uint32_t interval_ms)
{
    if (interval_ms < 500) interval_ms = 500;
    s_mock_poll_interval_ms = interval_ms;
    s_mock_rs485_running = true;
    printf("Mock RS485 polling STARTED (Interval: %lu ms)\n", (unsigned long)interval_ms);
    fflush(stdout);
}

void mock_rs485_stop(void)
{
    s_mock_rs485_running = false;
    printf("Mock RS485 polling STOPPED.\n");
    fflush(stdout);
}

void mock_rs485_set_values(float temp, float hum, bool valid)
{
    s_manual_temp = temp;
    s_manual_hum = hum;
    s_manual_valid = valid;
    printf("Mock RS485 manual values set: Temp=%.1f C, Hum=%.1f %%, Valid=%s\n",
           temp, hum, valid ? "true" : "false");
    fflush(stdout);
}

void mock_rs485_inject_fault(const char *error_type)
{
    if (error_type == NULL) return;
    strncpy(s_fault_mode, error_type, sizeof(s_fault_mode) - 1);
    s_fault_mode[sizeof(s_fault_mode) - 1] = '\0';
    printf("Mock RS485 fault injection mode set to: '%s'\n", s_fault_mode);
    fflush(stdout);
}

bool mock_rs485_is_running(void)
{
    return s_mock_rs485_running;
}

bool mock_storage_write_sample_csv(void)
{
    time_t now = app_rtc_get_time();
    if (s_mock_mutex != NULL) xSemaphoreTake(s_mock_mutex, portMAX_DELAY);
    if (s_mock_csv_count < 10) {
        snprintf(s_mock_csv_log[s_mock_csv_count++], 96,
                 "%ld,manual_sample,25.0,C,OK", (long)now);
    }
    if (s_mock_mutex != NULL) xSemaphoreGive(s_mock_mutex);
    printf("[MOCK_STORAGE] Appended sample telemetry record to '/sdcard/data/telemetry/2026-09-16.csv'\n");
    fflush(stdout);
    return true;
}

void mock_peripherals_print_status(void)
{
    if (s_mock_mutex != NULL) xSemaphoreTake(s_mock_mutex, portMAX_DELAY);
    printf("\n=== MOCK PERIPHERALS SUBSYSTEM STATUS ===\n");
    printf("Mock RS485 Poller : %s\n", s_mock_rs485_running ? "RUNNING (Active)" : "STOPPED");
    printf("Mock Poll Count   : %lu frames\n", (unsigned long)s_mock_poll_count);
    printf("Mock Fault Mode   : %s\n", s_fault_mode);
    printf("Mock Relay Coils  : [1:%s] [2:%s] [3:%s] [4:%s]\n",
           s_mock_relay_coils[0] ? "ON" : "OFF",
           s_mock_relay_coils[1] ? "ON" : "OFF",
           s_mock_relay_coils[2] ? "ON" : "OFF",
           s_mock_relay_coils[3] ? "ON" : "OFF");
    printf("Mock CSV Records  : %d buffered lines\n", s_mock_csv_count);
    for (int i = 0; i < s_mock_csv_count && i < 5; i++) {
        printf("  CSV[%d]: %s\n", i, s_mock_csv_log[i]);
    }
    printf("Configuration Doc : MOCK_PERIPHERALS.md\n");
    printf("=========================================\n\n");
    if (s_mock_mutex != NULL) xSemaphoreGive(s_mock_mutex);
    fflush(stdout);
}

void mock_rs485_get_latest(float *temp, float *hum, bool *valid)
{
    if (s_mock_mutex != NULL) xSemaphoreTake(s_mock_mutex, portMAX_DELAY);
    if (temp) {
        *temp = (s_manual_temp > -100.0f) ? s_manual_temp : (28.0f + 5.0f * sinf((float)s_mock_poll_count * 0.3f));
    }
    if (hum) {
        *hum = (s_manual_hum > -100.0f) ? s_manual_hum : (65.0f + 15.0f * cosf((float)s_mock_poll_count * 0.3f));
    }
    if (valid) {
        *valid = s_manual_valid && (strcmp(s_fault_mode, "invalid") != 0);
    }
    if (s_mock_mutex != NULL) xSemaphoreGive(s_mock_mutex);
}

void mock_rs485_get_relays(uint8_t coils[4])
{
    if (s_mock_mutex != NULL) xSemaphoreTake(s_mock_mutex, portMAX_DELAY);
    if (coils) {
        for (int i = 0; i < 4; i++) {
            coils[i] = s_mock_relay_coils[i];
        }
    }
    if (s_mock_mutex != NULL) xSemaphoreGive(s_mock_mutex);
}

void mock_rs485_freeze(void)
{
    s_mock_frozen = true;
    printf("[TEST HOOK] Freezing rs485 task indefinitely (stopping heartbeat)... Watchdog timeout in 15s!\n");
    fflush(stdout);
}
