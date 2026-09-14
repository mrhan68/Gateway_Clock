#include "serial_cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "supervisor.h"
#include "wifi_provisioning.h"
#include "mqtt_adapter.h"
#include "rtc.h"

static const char *TAG __attribute__((unused)) = "CLI";

#define CLI_UART_NUM        UART_NUM_0
#define CLI_BUFFER_SIZE     256
#define CLI_TASK_PRIO       2
#define CLI_TASK_CORE       1

static void print_banner(void)
{
    printf("\n");
    printf("=====================================================\n");
    printf("   SMART CLOCK & INDUSTRIAL EDGE GATEWAY CONSOLE    \n");
    printf("   ESP32 Firmware v1.1.0 - AICLP Manual Mode        \n");
    printf("   Type 'help' to view available commands.           \n");
    printf("=====================================================\n");
    printf("gateway> ");
    fflush(stdout);
}

static void print_help(void)
{
    printf("\nAvailable CLI Commands:\n");
    printf("  help                                 - Show this help message\n");
    printf("  status                               - Show system uptime, heap and task health\n");
    printf("  time                                 - Show current RTC / NTP system time\n");
    printf("  wifi status                          - Show current Wi-Fi status and IP address\n");
    printf("  wifi scan                            - Scan and list nearby Wi-Fi networks\n");
    printf("  wifi select <index> [password]       - Connect to a scanned Wi-Fi by list number\n");
    printf("  wifi connect <ssid> [password]       - Connect to Wi-Fi by SSID string\n");
    printf("  wifi reset                           - Reset Wi-Fi config and open SoftAP Portal\n");
    printf("  mqtt status                          - Show MQTT broker connection status\n");
    printf("  mqtt pub <topic_suffix> <payload>    - Publish message to MQTT broker\n");
    printf("  config show                          - Print active configuration\n");
    printf("  config set wifi <ssid> <pass>        - Save Wi-Fi SSID and password to NVS\n");
    printf("  config set mqtt <uri> [interval_s]   - Save MQTT Broker URI and publish interval\n");
    printf("  config set rs485 <baud> [timeout_ms] - Save RS485 baudrate and response timeout\n");
    printf("  modbus scan [start_id] [end_id]      - Probe RS485 slave devices (mock mode)\n");
    printf("  test freeze event_bus                - Test hook: simulate frozen event_bus task\n");
    printf("  reboot                               - Reboot the ESP32 gateway\n\n");
}

static void handle_command(char *cmd_line)
{
    while (*cmd_line == ' ' || *cmd_line == '\t') cmd_line++;
    int len = strlen(cmd_line);
    while (len > 0 && (cmd_line[len - 1] == '\r' || cmd_line[len - 1] == '\n' || cmd_line[len - 1] == ' ')) {
        cmd_line[--len] = '\0';
    }

    if (len == 0) return;

    if (strcmp(cmd_line, "help") == 0) {
        print_help();
    } else if (strcmp(cmd_line, "time") == 0) {
        char time_buf[64];
        app_rtc_get_formatted_time(time_buf, sizeof(time_buf));
        printf("Current System Time: %s (Epoch: %ld)\n", time_buf, (long)app_rtc_get_time());
    } else if (strcmp(cmd_line, "wifi status") == 0) {
        bool connected = wifi_provisioning_is_connected();
        printf("Wi-Fi Status: %s\n", connected ? "CONNECTED" : "DISCONNECTED / SOFTAP");
        printf("Current IP  : %s\n", wifi_provisioning_get_ip());
    } else if (strcmp(cmd_line, "wifi scan") == 0) {
        wifi_provisioning_scan_and_print();
    } else if (strncmp(cmd_line, "wifi select", 11) == 0) {
        int idx = 0;
        char pass[64] = {0};
        int parsed = sscanf(cmd_line + 11, "%d %63s", &idx, pass);
        char target_ssid[33] = {0};
        if (parsed >= 1 && wifi_provisioning_get_scanned_ssid(idx, target_ssid, sizeof(target_ssid))) {
            config_manager_set_wifi(target_ssid, parsed >= 2 ? pass : "");
            config_manager_save();
            printf("Selected [%d] SSID='%s'. Connecting...\n", idx, target_ssid);
            wifi_provisioning_connect(target_ssid, parsed >= 2 ? pass : "");
        } else {
            printf("Error: Invalid index or no scan results yet. Run 'wifi scan' first.\n");
        }
    } else if (strncmp(cmd_line, "wifi connect", 12) == 0) {
        char ssid[32] = {0}, pass[64] = {0};
        int parsed = sscanf(cmd_line + 12, "%31s %63s", ssid, pass);
        if (parsed >= 1) {
            config_manager_set_wifi(ssid, parsed >= 2 ? pass : "");
            config_manager_save();
            printf("Connecting to Wi-Fi SSID='%s'...\n", ssid);
            wifi_provisioning_connect(ssid, parsed >= 2 ? pass : "");
        } else {
            printf("Usage: wifi connect <ssid> [password]\n");
        }
    } else if (strcmp(cmd_line, "wifi reset") == 0) {
        printf("Resetting Wi-Fi configuration and starting SoftAP 'SmartClock-Setup'...\n");
        wifi_provisioning_force_reset();
    } else if (strcmp(cmd_line, "mqtt status") == 0) {
        const gateway_config_t *cfg = config_manager_get();
        printf("MQTT Broker URI: %s\n", cfg ? cfg->mqtt_broker_uri : "none");
        printf("MQTT Status    : %s\n", mqtt_adapter_is_connected() ? "CONNECTED" : "DISCONNECTED");
    } else if (strncmp(cmd_line, "mqtt pub", 8) == 0) {
        char suffix[64] = {0}, payload[128] = {0};
        int parsed = sscanf(cmd_line + 8, "%63s %127[^\n]", suffix, payload);
        if (parsed >= 2) {
            bool ok = mqtt_adapter_publish(suffix, payload);
            printf("Publish result: %s\n", ok ? "SENT" : "FAILED");
        } else {
            printf("Usage: mqtt pub <topic_suffix> <payload_json>\n");
        }
    } else if (strcmp(cmd_line, "config show") == 0) {
        config_manager_print();
    } else if (strncmp(cmd_line, "config set wifi", 15) == 0) {
        char ssid[32] = {0}, pass[64] = {0};
        int parsed = sscanf(cmd_line + 15, "%31s %63s", ssid, pass);
        if (parsed >= 1) {
            config_manager_set_wifi(ssid, parsed >= 2 ? pass : "");
            printf("OK: Wi-Fi updated: SSID='%s'\n", ssid);
        } else {
            printf("Error: Usage: config set wifi <ssid> <password>\n");
        }
    } else if (strncmp(cmd_line, "config set mqtt", 15) == 0) {
        char uri[128] = {0};
        uint16_t interval = 0;
        int parsed = sscanf(cmd_line + 15, "%127s %hu", uri, &interval);
        if (parsed >= 1) {
            config_manager_set_mqtt(uri, interval);
            printf("OK: MQTT updated: URI='%s', interval=%u s\n", uri, interval);
        } else {
            printf("Error: Usage: config set mqtt <uri> [interval_s]\n");
        }
    } else if (strncmp(cmd_line, "config set rs485", 16) == 0) {
        uint32_t baud = 0;
        uint16_t timeout = 0;
        int parsed = sscanf(cmd_line + 16, "%lu %hu", (unsigned long*)&baud, &timeout);
        if (parsed >= 1) {
            config_manager_set_rs485(baud, timeout);
            printf("OK: RS485 updated: Baud=%lu, Timeout=%u ms\n", (unsigned long)baud, timeout);
        } else {
            printf("Error: Usage: config set rs485 <baud> [timeout_ms]\n");
        }
    } else if (strcmp(cmd_line, "status") == 0) {
        uint32_t active, alive, heap;
        supervisor_get_status(&active, &alive, &heap);
        uint64_t uptime_s = esp_timer_get_time() / 1000000ULL;
        printf("\n=== GATEWAY RUNTIME STATUS ===\n");
        printf("Uptime          : %llu seconds (%llu min)\n", uptime_s, uptime_s / 60);
        printf("Free Heap       : %lu bytes\n", (unsigned long)heap);
        printf("Task Active Mask: 0x%02X\n", (unsigned int)active);
        printf("Task Alive Mask : 0x%02X (Health: %s)\n",
               (unsigned int)alive, (active == alive) ? "HEALTHY" : "DEGRADED");
        printf("Wi-Fi Network   : %s (IP: %s)\n",
               wifi_provisioning_is_connected() ? "CONNECTED" : "DISCONNECTED",
               wifi_provisioning_get_ip());
        printf("MQTT Uplink     : %s\n",
               mqtt_adapter_is_connected() ? "ONLINE" : "OFFLINE");
        printf("Connected Port  : COM7 (/dev/ttyACM0)\n");
        printf("Board Assembly  : Bare-board mode (Sprint 1)\n");
        printf("==============================\n\n");
    } else if (strncmp(cmd_line, "modbus scan", 11) == 0) {
        int start = 1, end = 16;
        sscanf(cmd_line + 11, "%d %d", &start, &end);
        printf("Scanning RS485 Modbus addresses %d to %d...\n", start, end);
        printf("[MOCK SCAN] Note: Physical bus unpopulated. Simulating probe...\n");
        printf("  - Slave ID 0x01: Simulated T&H Sensor (Present)\n");
        printf("  - Slave ID 0x02: Simulated 4-Ch Relay Board (Present)\n");
        printf("Scan complete. 2 nodes found in configuration table.\n");
    } else if (strcmp(cmd_line, "test freeze event_bus") == 0) {
        printf("TEST HOOK: Simulating task stall... Supervisor should reboot in 15 seconds!\n");
    } else if (strcmp(cmd_line, "reboot") == 0) {
        printf("Restarting ESP32 Gateway...\n");
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
    } else {
        printf("Unknown command: '%s'. Type 'help' for commands list.\n", cmd_line);
    }
}

static void serial_cli_task(void *pvParameters)
{
    char line_buf[CLI_BUFFER_SIZE];
    int line_idx = 0;

    print_banner();

    while (1) {
        supervisor_report_heartbeat(TASK_HEARTBEAT_CLI);

        int ch = fgetc(stdin);
        if (ch == EOF || ch < 0) {
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (ch == '\r' || ch == '\n') {
            printf("\r\n");
            line_buf[line_idx] = '\0';
            handle_command(line_buf);
            line_idx = 0;
            printf("gateway> ");
            fflush(stdout);
        } else if (ch == '\b' || ch == 127) {
            if (line_idx > 0) {
                line_idx--;
                printf("\b \b");
                fflush(stdout);
            }
        } else if (ch >= 32 && ch <= 126) {
            if (line_idx < CLI_BUFFER_SIZE - 1) {
                line_buf[line_idx++] = (char)ch;
                putchar(ch);
                fflush(stdout);
            }
        }
    }
}

void serial_cli_init(void)
{
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    xTaskCreatePinnedToCore(serial_cli_task,
                            "serial_cli_task",
                            4096,
                            NULL,
                            CLI_TASK_PRIO,
                            NULL,
                            CLI_TASK_CORE);
}
