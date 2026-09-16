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
#include "rule_engine.h"
#include "scheduler.h"
#include "mock_peripherals.h"

static const char *TAG __attribute__((unused)) = "CLI";

#define CLI_UART_NUM        UART_NUM_0
#define CLI_BUFFER_SIZE     256
#define CLI_TASK_STACK_SIZE 8192
#define CLI_TASK_PRIO       2
#define CLI_TASK_CORE       1

static automation_rule_t s_cli_rules[MAX_RULES];
static schedule_entry_t s_cli_entries[MAX_SCHEDULE_ENTRIES];

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
    printf("  rule list                            - List active automation rules\n");
    printf("  rule add <id> <sensor> <GT|LT> <thr> <target> <cmd> - Add or update rule\n");
    printf("  rule test <sensor_id> <val> [valid]  - Test telemetry against rules (valid=1/0)\n");
    printf("  rule clear                           - Clear all active rules\n");
    printf("  schedule list                        - List all active schedule entries\n");
    printf("  schedule add <alarm|auto> <HH:MM> <title_or_target> [cmd] - Add schedule\n");
    printf("  schedule test [title]                - Manually trigger an alarm notification\n");
    printf("  schedule clear                       - Clear all schedule entries\n");
    printf("  cmd send <target_id> <command>       - Dispatch command to device\n");
    printf("  mock status                          - Show mock peripherals status\n");
    printf("  mock rs485 start [interval_ms]       - Start simulated RS485 polling\n");
    printf("  mock rs485 stop                      - Stop simulated RS485 polling\n");
    printf("  mock rs485 set <temp> <hum>          - Set simulated sensor values\n");
    printf("  mock rs485 fault <none|timeout|invalid> - Inject simulated bus faults\n");
    printf("  mock storage test                    - Write sample telemetry to virtual storage\n");
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
    } else if (strcmp(cmd_line, "rule list") == 0) {
        int cnt = rule_engine_get_rule_count();
        printf("\n=== ACTIVE AUTOMATION RULES (%d) ===\n", cnt);
        int n = rule_engine_get_rules(s_cli_rules, MAX_RULES);
        for (int i = 0; i < n; i++) {
            printf("  [#%d] Sensor: '%s' %s %.1f -> Target: '%s' CMD: '%s' (Status: %s)\n",
                   s_cli_rules[i].id, s_cli_rules[i].device_id,
                   (s_cli_rules[i].condition_type == RULE_CONDITION_SENSOR_GT) ? ">" : "<",
                   s_cli_rules[i].threshold, s_cli_rules[i].target_device_id, s_cli_rules[i].action_command,
                   s_cli_rules[i].enabled ? "ENABLED" : "DISABLED");
        }
        printf("===================================\n\n");
    } else if (strncmp(cmd_line, "rule add", 8) == 0) {
        int id = 0;
        char sensor[32] = {0}, cond[8] = {0}, target[32] = {0}, cmd[32] = {0};
        float thresh = 0.0f;
        int parsed = sscanf(cmd_line + 8, "%d %31s %7s %f %31s %31s",
                            &id, sensor, cond, &thresh, target, cmd);
        if (parsed >= 6) {
            automation_rule_t r = {
                .id = id,
                .condition_type = (strcmp(cond, "LT") == 0 || strcmp(cond, "<") == 0) ? RULE_CONDITION_SENSOR_LT : RULE_CONDITION_SENSOR_GT,
                .threshold = thresh,
                .enabled = true,
            };
            strncpy(r.device_id, sensor, sizeof(r.device_id) - 1);
            strncpy(r.target_device_id, target, sizeof(r.target_device_id) - 1);
            strncpy(r.action_command, cmd, sizeof(r.action_command) - 1);
            rule_engine_add_rule(&r);
            printf("OK: Rule #%d added.\n", id);
        } else {
            printf("Usage: rule add <id> <sensor_id> <GT|LT> <threshold> <target_id> <action_cmd>\n");
        }
    } else if (strncmp(cmd_line, "rule test", 9) == 0) {
        char sensor[32] = {0};
        float val = 0.0f;
        int valid = 1;
        int parsed = sscanf(cmd_line + 9, "%31s %f %d", sensor, &val, &valid);
        if (parsed >= 2) {
            sensor_telemetry_t telem = {
                .value = val,
                .valid = (valid != 0),
            };
            strncpy(telem.device_id, sensor, sizeof(telem.device_id) - 1);
            printf("Testing telemetry: sensor='%s', value=%.2f, valid=%s...\n",
                   telem.device_id, telem.value, telem.valid ? "true" : "false");
            int matches = rule_engine_evaluate_telemetry(&telem);
            printf("Evaluation completed: %d rule(s) triggered.\n", matches);
        } else {
            printf("Usage: rule test <sensor_id> <value> [valid: 1 or 0]\n");
        }
    } else if (strcmp(cmd_line, "rule clear") == 0) {
        rule_engine_clear_rules();
        printf("OK: All rules cleared.\n");
    } else if (strcmp(cmd_line, "schedule list") == 0) {
        int cnt = scheduler_get_entry_count();
        printf("\n=== ACTIVE SCHEDULES (%d) ===\n", cnt);
        int n = scheduler_get_entries(s_cli_entries, MAX_SCHEDULE_ENTRIES);
        for (int i = 0; i < n; i++) {
            printf("  [#%d] [%s] %02d:%02d Title: '%s' (Buzzer: %s, Enabled: %s)\n",
                   s_cli_entries[i].id, s_cli_entries[i].type, s_cli_entries[i].hour, s_cli_entries[i].minute,
                   s_cli_entries[i].title, s_cli_entries[i].buzzer ? "YES" : "NO",
                   s_cli_entries[i].enabled ? "YES" : "NO");
            if (strcmp(s_cli_entries[i].type, "automation") == 0) {
                printf("       -> Target: '%s' Command: '%s'\n",
                       s_cli_entries[i].target_device_id, s_cli_entries[i].action_command);
            }
        }
        printf("==============================\n\n");
    } else if (strncmp(cmd_line, "schedule add", 12) == 0) {
        int id = 0;
        char type[16] = {0}, time_str[8] = {0}, title[64] = {0}, cmd[32] = {0};
        int parsed = sscanf(cmd_line + 12, "%d %15s %7s %63s %31s",
                            &id, type, time_str, title, cmd);
        if (parsed >= 4) {
            int h = 0, m = 0;
            sscanf(time_str, "%d:%d", &h, &m);
            schedule_entry_t e = {
                .id = id,
                .hour = h,
                .minute = m,
                .enabled = true,
                .buzzer = true,
            };
            strncpy(e.type, type, sizeof(e.type) - 1);
            strncpy(e.title, title, sizeof(e.title) - 1);
            for (int d = 0; d < 7; d++) e.repeat_days[d] = true;
            if (strcmp(type, "automation") == 0) {
                strncpy(e.target_device_id, title, sizeof(e.target_device_id) - 1);
                strncpy(e.action_command, cmd, sizeof(e.action_command) - 1);
            }
            scheduler_add_entry(&e);
            printf("OK: Schedule #%d added (%02d:%02d).\n", id, h, m);
        } else {
            printf("Usage: schedule add <id> <alarm|automation> <HH:MM> <title_or_target> [cmd]\n");
        }
    } else if (strncmp(cmd_line, "schedule test", 13) == 0) {
        char title[64] = "Manual Test Alarm";
        if (strlen(cmd_line) > 14) {
            strncpy(title, cmd_line + 14, sizeof(title) - 1);
        }
        printf("Triggering test alarm: '%s'...\n", title);
        scheduler_trigger_alarm(title, true);
    } else if (strcmp(cmd_line, "schedule clear") == 0) {
        scheduler_clear_entries();
        printf("OK: All schedules cleared.\n");
    } else if (strncmp(cmd_line, "cmd send", 8) == 0) {
        char target[32] = {0}, command[32] = {0};
        int parsed = sscanf(cmd_line + 8, "%31s %31s", target, command);
        if (parsed >= 2) {
            printf("Dispatching command '%s' to target device '%s'...\n", command, target);
            rule_action_request_t *req = (rule_action_request_t *)malloc(sizeof(rule_action_request_t));
            if (req != NULL) {
                strncpy(req->target_device_id, target, sizeof(req->target_device_id) - 1);
                req->target_device_id[sizeof(req->target_device_id) - 1] = '\0';
                strncpy(req->action_command, command, sizeof(req->action_command) - 1);
                req->action_command[sizeof(req->action_command) - 1] = '\0';
                req->source_rule_id = 0; // Manual command
                event_bus_post(EVT_RULE_ACTION_REQUEST, req, sizeof(rule_action_request_t), false);
            }
        } else {
            printf("Usage: cmd send <target_device_id> <command>\n");
        }
    } else if (strcmp(cmd_line, "mock status") == 0) {
        mock_peripherals_print_status();
    } else if (strncmp(cmd_line, "mock rs485 start", 16) == 0) {
        uint32_t interval = 3000;
        sscanf(cmd_line + 16, "%lu", (unsigned long*)&interval);
        mock_rs485_start(interval);
    } else if (strcmp(cmd_line, "mock rs485 stop") == 0) {
        mock_rs485_stop();
    } else if (strncmp(cmd_line, "mock rs485 set", 14) == 0) {
        float temp = 0.0f, hum = 0.0f;
        int valid = 1;
        int parsed = sscanf(cmd_line + 14, "%f %f %d", &temp, &hum, &valid);
        if (parsed >= 2) {
            mock_rs485_set_values(temp, hum, valid != 0);
        } else {
            printf("Usage: mock rs485 set <temperature> <humidity> [valid: 1/0]\n");
        }
    } else if (strncmp(cmd_line, "mock rs485 fault", 16) == 0) {
        char fault[16] = "none";
        sscanf(cmd_line + 16, "%15s", fault);
        mock_rs485_inject_fault(fault);
    } else if (strcmp(cmd_line, "mock storage test") == 0) {
        mock_storage_write_sample_csv();
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
                            CLI_TASK_STACK_SIZE,
                            NULL,
                            CLI_TASK_PRIO,
                            NULL,
                            CLI_TASK_CORE);
}
