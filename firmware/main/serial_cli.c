#include "serial_cli.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "config_manager.h"
#include "supervisor.h"
#include "wifi_provisioning.h"
#include "mqtt_adapter.h"
#include "rtc.h"
#include "rule_engine.h"
#include "scheduler.h"
#include "event_bus.h"
#include "ble_adapter.h"
#include "relay_controller.h"
#include "sd_logger.h"
#include "board_led.h"

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
    printf("   SMART CLOCK EDGE IOT GATEWAY CONSOLE             \n");
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
    printf("  time set <YYYY-MM-DD> <HH:MM:SS>     - Manually set RTC hardware time\n");
    printf("  sd status                            - Show MicroSD card mount status\n");
    printf("  sd mount                             - Attempt to mount / remount MicroSD card\n");
    printf("  ble status                           - Show BLE scanner status and discovered devices\n");
    printf("  ble start                            - Start BLE GAP scanner\n");
    printf("  ble stop                             - Stop BLE GAP scanner\n");
    printf("  ble reg <device_id> [mac]            - Register a BLE sensor mapping\n");
    printf("  ble test [val]                       - Inject synthetic BLE telemetry (default 32.5 C)\n");
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
    printf("  rule list                            - List active automation rules\n");
    printf("  rule add <id> <sensor> <GT|LT> <thr> <target> <cmd> - Add or update rule\n");
    printf("  rule test <sensor_id> <val> [valid]  - Test telemetry against rules (valid=1/0)\n");
    printf("  rule delete <id>                     - Delete a rule by ID\n");
    printf("  rule clear                           - Clear all active rules\n");
    printf("  schedule list                        - List all active schedule entries\n");
    printf("  schedule add [id] <alarm|auto> <HH:MM> <title_or_target> [cmd] - Add schedule\n");
    printf("  schedule test [title]                - Manually trigger an alarm notification\n");
    printf("  schedule clear                       - Clear all schedule entries\n");
    printf("  relay status                         - Show state of all 4 relays (GPIO 15..18)\n");
    printf("  relay on <target_id>                 - Turn on relay (fan, heater, dehum, aux)\n");
    printf("  relay off <target_id>                - Turn off relay\n");
    printf("  relay emergency                      - Emergency shutdown all relays (Active LOW)\n");
    printf("  rgb off                              - Turn off on-board RGB LED (GPIO 48, 38, 21)\n");
    printf("  rgb set <gpio> <r> <g> <b>           - Set color on WS2812 pin (e.g. rgb set 48 0 0 0)\n");
    printf("  rgb test                             - Test blink candidate pins to locate RGB LED\n");
    printf("  cmd send <target_id> <command>       - Dispatch command to device\n");
    printf("  test freeze event_bus                - Test hook: simulate frozen event_bus task (FR-SUP-02)\n");
    printf("  test burst [count]                   - Test hook: burst events into event bus queue (FSD §4.1.6)\n");
    printf("  system factory-reset                 - Wipe Wi-Fi NVS credentials & restore SoftAP pass to empty (OPEN)\n");
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
    } else if (strncmp(cmd_line, "time set", 8) == 0) {
        int y, m, d, h, min, s;
        if (sscanf(cmd_line + 8, "%d-%d-%d %d:%d:%d", &y, &m, &d, &h, &min, &s) == 6) {
            struct tm ti = {
                .tm_year = y - 1900,
                .tm_mon  = m - 1,
                .tm_mday = d,
                .tm_hour = h,
                .tm_min  = min,
                .tm_sec  = s,
                .tm_isdst = 0
            };
            app_rtc_set_time(&ti);
            printf("OK: Time set and saved to DS3231: %04d-%02d-%02d %02d:%02d:%02d\n", y, m, d, h, min, s);
        } else {
            printf("Usage: time set YYYY-MM-DD HH:MM:SS (e.g. time set 2026-09-26 19:00:00)\n");
        }
    } else if (strcmp(cmd_line, "time sync") == 0) {
        app_rtc_sync_from_ntp_and_save();
        printf("OK: Triggered NTP network sync.\n");
    } else if (strcmp(cmd_line, "time") == 0) {
        char time_buf[64];
        app_rtc_get_formatted_time(time_buf, sizeof(time_buf));
        printf("Current System Time: %s (Epoch: %ld)\n", time_buf, (long)app_rtc_get_time());
    } else if (strcmp(cmd_line, "ble status") == 0) {
        ble_adapter_print_status();
    } else if (strcmp(cmd_line, "ble start") == 0) {
        ble_adapter_start_scan();
        printf("OK: BLE GAP passive scanner started.\n");
    } else if (strcmp(cmd_line, "ble stop") == 0) {
        ble_adapter_stop_scan();
        printf("OK: BLE GAP passive scanner stopped.\n");
    } else if (strncmp(cmd_line, "ble reg", 7) == 0) {
        char dev_id[32] = {0}, mac[32] = {0};
        int parsed = sscanf(cmd_line + 7, "%31s %31s", dev_id, mac);
        if (parsed >= 1) {
            ble_adapter_register_device(dev_id, parsed >= 2 ? mac : NULL);
            printf("OK: Registered BLE device '%s' (MAC: %s)\n", dev_id, parsed >= 2 ? mac : "Auto");
        } else {
            printf("Usage: ble reg <device_id> [mac_address]\n");
        }
    } else if (strncmp(cmd_line, "ble test", 8) == 0) {
        char arg1[32] = {0};
        float val = 32.50f;
        char dev[32] = "sensor_temp_01";
        int parsed = sscanf(cmd_line + 8, "%31s %f", arg1, &val);
        if (parsed == 1) {
            if ((arg1[0] >= '0' && arg1[0] <= '9') || arg1[0] == '-') {
                val = strtof(arg1, NULL);
                strcpy(dev, "sensor_temp_01");
            } else {
                strncpy(dev, arg1, sizeof(dev) - 1);
                val = 32.50f;
            }
        } else if (parsed >= 2) {
            strncpy(dev, arg1, sizeof(dev) - 1);
        }
        ble_adapter_inject_test_packet(dev, val);
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
    } else if (strncmp(cmd_line, "rule delete", 11) == 0) {
        int id = 0;
        if (sscanf(cmd_line + 11, "%d", &id) == 1) {
            if (rule_engine_delete_rule(id)) {
                printf("OK: Rule #%d deleted.\n", id);
            } else {
                printf("Error: Rule #%d not found.\n", id);
            }
        } else {
            printf("Usage: rule delete <id>\n");
        }
    } else if (strcmp(cmd_line, "rule clear") == 0) {
        rule_engine_clear_rules();
        printf("OK: All rules cleared.\n");
    } else if (strcmp(cmd_line, "rule reset") == 0 || strcmp(cmd_line, "rule reload") == 0) {
        rule_engine_reload_rules();
        rule_engine_reset_lockouts();
        printf("OK: Rules reloaded and lockouts reset.\n");
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
        char token1[16] = {0}, token2[64] = {0}, token3[64] = {0}, token4[64] = {0}, token5[32] = {0};
        int parsed = sscanf(cmd_line + 12, "%15s %63s %63s %63s %31s",
                            token1, token2, token3, token4, token5);
        char type[16] = {0}, time_str[8] = {0}, title[64] = {0}, cmd[32] = {0};

        if (parsed >= 3 && (strcmp(token1, "alarm") == 0 || strcmp(token1, "automation") == 0 || strcmp(token1, "auto") == 0)) {
            // Format without ID: schedule add <type> <HH:MM> <title> [cmd]
            id = scheduler_get_entry_count() + 1;
            strncpy(type, token1, sizeof(type) - 1);
            strncpy(time_str, token2, sizeof(time_str) - 1);
            strncpy(title, token3, sizeof(title) - 1);
            strncpy(cmd, token4, sizeof(cmd) - 1);
            parsed = 4;
        } else if (parsed >= 4) {
            // Format with ID: schedule add <id> <type> <HH:MM> <title> [cmd]
            id = atoi(token1);
            strncpy(type, token2, sizeof(type) - 1);
            strncpy(time_str, token3, sizeof(time_str) - 1);
            strncpy(title, token4, sizeof(title) - 1);
            strncpy(cmd, token5, sizeof(cmd) - 1);
        } else {
            parsed = 0;
        }

        if (parsed >= 4 && id > 0) {
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
            if (strcmp(type, "automation") == 0 || strcmp(type, "auto") == 0) {
                strncpy(e.type, "automation", sizeof(e.type) - 1);
                strncpy(e.target_device_id, title, sizeof(e.target_device_id) - 1);
                strncpy(e.action_command, cmd, sizeof(e.action_command) - 1);
            }
            scheduler_add_entry(&e);
            printf("OK: Schedule #%d added (%02d:%02d).\n", id, h, m);
        } else {
            printf("Usage: schedule add [id] <alarm|automation> <HH:MM> <title_or_target> [cmd]\n");
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
                memset(req, 0, sizeof(rule_action_request_t));
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
    } else if (strcmp(cmd_line, "relay status") == 0) {
        printf("\n=== 4-CHANNEL RELAY STATUS (FSD §4.10) ===\n");
        for (int i = 1; i <= 4; i++) {
            bool st = relay_controller_get_state(i);
            uint32_t dur = relay_controller_get_on_duration(i);
            printf("  Relay %d [GPIO %d]: %s (ON for %lu s)\n",
                   i, (i == 1) ? 15 : (i == 2) ? 16 : (i == 3) ? 17 : 18,
                   st ? "ON" : "OFF", (unsigned long)dur);
        }
        printf("==========================================\n\n");
    } else if (strncmp(cmd_line, "relay on", 8) == 0) {
        char target[32] = {0};
        if (sscanf(cmd_line + 8, "%31s", target) >= 1) {
            esp_err_t err = relay_controller_set_state(target, true, "CLI_CMD", "MANUAL_CLI");
            printf("Relay ON '%s': %s\n", target, (err == ESP_OK) ? "SUCCESS" : "REJECTED (Interlock/Lockout)");
        } else {
            printf("Usage: relay on <fan|heater|dehum|aux|relay_1..4>\n");
        }
    } else if (strncmp(cmd_line, "relay off", 9) == 0) {
        char target[32] = {0};
        if (sscanf(cmd_line + 9, "%31s", target) >= 1) {
            esp_err_t err = relay_controller_set_state(target, false, "CLI_CMD", "MANUAL_CLI");
            printf("Relay OFF '%s': %s\n", target, (err == ESP_OK) ? "SUCCESS" : "FAILED");
        } else {
            printf("Usage: relay off <fan|heater|dehum|aux|relay_1..4>\n");
        }
    } else if (strcmp(cmd_line, "relay emergency") == 0) {
        relay_controller_emergency_shutdown_all();
        printf("EMERGENCY SHUTDOWN: All 4 relays forced LOW.\n");
    } else if (strcmp(cmd_line, "rgb off") == 0) {
        board_led_all_off();
        printf("OK: WS2812 0s sent and pulled LOW on all candidate pins.\n");
    } else if (strcmp(cmd_line, "rgb float") == 0) {
        static const gpio_num_t candidate_pins[] = {
            48, 38, 21, 47, 14, 2, 3, 5, 6, 7, 35, 36, 37, 39, 40, 41, 42, 45, 46
        };
        for (size_t i = 0; i < sizeof(candidate_pins) / sizeof(candidate_pins[0]); i++) {
            gpio_reset_pin(candidate_pins[i]);
            gpio_set_direction(candidate_pins[i], GPIO_MODE_INPUT);
        }
        printf("OK: All candidate pins set to INPUT (Floating / High-Z). Any discrete LED forced OFF.\n");
    } else if (strncmp(cmd_line, "rgb set", 7) == 0) {
        int gpio = 48, r = 0, g = 0, b = 0;
        int parsed = sscanf(cmd_line + 7, "%d %d %d %d", &gpio, &r, &g, &b);
        if (parsed >= 4) {
            board_led_set_rgb((gpio_num_t)gpio, (uint8_t)r, (uint8_t)g, (uint8_t)b);
            printf("OK: Set RGB on GPIO %d: R=%d, G=%d, B=%d\n", gpio, r, g, b);
        } else {
            printf("Usage: rgb set <gpio> <r> <g> <b> (e.g. rgb set 48 0 0 0)\n");
        }
    } else if (strcmp(cmd_line, "rgb test") == 0) {
        printf("Testing on-board RGB LED pins...\n");
        printf("1. Flashing GPIO 48 (Red)...\n");
        board_led_set_rgb(BOARD_LED_PIN_DEVKITC1, 40, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        board_led_set_rgb(BOARD_LED_PIN_DEVKITC1, 0, 0, 0);

        printf("2. Flashing GPIO 38 (Green)...\n");
        board_led_set_rgb(BOARD_LED_PIN_ALT1, 0, 40, 0);
        vTaskDelay(pdMS_TO_TICKS(1000));
        board_led_set_rgb(BOARD_LED_PIN_ALT1, 0, 0, 0);

        printf("3. Flashing GPIO 21 (Blue)...\n");
        board_led_set_rgb(BOARD_LED_PIN_ZERO, 0, 0, 40);
        vTaskDelay(pdMS_TO_TICKS(1000));
        board_led_set_rgb(BOARD_LED_PIN_ZERO, 0, 0, 0);

        board_led_all_off();
        printf("Test complete. All LEDs turned OFF.\n");
    } else if (strcmp(cmd_line, "rgb scan") == 0) {
        static const gpio_num_t test_pins[] = {
            48, 38, 21, 47, 14, 2, 3, 5, 6, 7, 35, 36, 37, 39, 40, 41, 42, 45, 46
        };
        printf("=== Scanning all GPIO pins with WS2812 OFF signal ===\n");
        for (size_t i = 0; i < sizeof(test_pins) / sizeof(test_pins[0]); i++) {
            gpio_num_t p = test_pins[i];
            printf("Testing Pin GPIO %d... ", p);
            board_led_set_rgb(p, 0, 0, 0);
            vTaskDelay(pdMS_TO_TICKS(100));
            printf("done\n");
        }
        printf("Scan complete. If your LED was a WS2812, it should now be OFF!\n");

    } else if (strcmp(cmd_line, "test freeze event_bus") == 0) {
        printf("TEST HOOK: Simulating EVENT_BUS task stall (FSD §4.13.4 FR-SUP-02)...\n");
        printf("Task will stop heartbeat. Supervisor should reboot in 15 seconds!\n");
        event_bus_freeze();
    } else if (strncmp(cmd_line, "test burst", 10) == 0) {
        int count = 40;
        sscanf(cmd_line + 10, "%d", &count);
        if (count < 1) count = 40;
        event_bus_test_burst(count);
    } else if (strncmp(cmd_line, "sd", 2) == 0) {
        if (strcmp(cmd_line, "sd status") == 0) {
            printf("MicroSD Status: %s (Mount point: %s)\n",
                   sd_logger_is_mounted() ? "MOUNTED (FAT32)" : "NOT MOUNTED (SPIFFS Fallback)",
                   SDCARD_MOUNT_POINT);
        } else if (strcmp(cmd_line, "sd mount") == 0) {
            printf("Retrying MicroSD mount on SPI2 (CS=10, MOSI=11, CLK=12, MISO=13)...\n");
            esp_err_t err = sd_logger_remount();
            if (err == ESP_OK) {
                printf("SUCCESS: MicroSD card mounted at %s!\n", SDCARD_MOUNT_POINT);
            } else {
                printf("FAILED: MicroSD mount returned %s (0x%x).\n", esp_err_to_name(err), err);
            }
        } else if (strcmp(cmd_line, "sd diag") == 0) {
            printf("--- MicroSD Hardware Pin Diagnostics ---\n");
            gpio_set_direction(SD_PIN_CS, GPIO_MODE_INPUT);
            gpio_set_direction(SD_PIN_MOSI, GPIO_MODE_INPUT);
            gpio_set_direction(SD_PIN_CLK, GPIO_MODE_INPUT);
            gpio_set_direction(SD_PIN_MISO, GPIO_MODE_INPUT);

            gpio_set_pull_mode(SD_PIN_CS, GPIO_FLOATING);
            gpio_set_pull_mode(SD_PIN_MOSI, GPIO_FLOATING);
            gpio_set_pull_mode(SD_PIN_CLK, GPIO_FLOATING);
            gpio_set_pull_mode(SD_PIN_MISO, GPIO_FLOATING);
            esp_rom_delay_us(100);
            printf("[Floating] CS=%d, MOSI=%d, CLK=%d, MISO=%d\n",
                   gpio_get_level(SD_PIN_CS), gpio_get_level(SD_PIN_MOSI),
                   gpio_get_level(SD_PIN_CLK), gpio_get_level(SD_PIN_MISO));

            gpio_set_pull_mode(SD_PIN_CS, GPIO_PULLDOWN_ONLY);
            gpio_set_pull_mode(SD_PIN_MOSI, GPIO_PULLDOWN_ONLY);
            gpio_set_pull_mode(SD_PIN_CLK, GPIO_PULLDOWN_ONLY);
            gpio_set_pull_mode(SD_PIN_MISO, GPIO_PULLDOWN_ONLY);
            esp_rom_delay_us(100);
            printf("[PullDown] CS=%d, MOSI=%d, CLK=%d, MISO=%d (1 = powered/ext pullup)\n",
                   gpio_get_level(SD_PIN_CS), gpio_get_level(SD_PIN_MOSI),
                   gpio_get_level(SD_PIN_CLK), gpio_get_level(SD_PIN_MISO));

            gpio_set_pull_mode(SD_PIN_CS, GPIO_PULLUP_ONLY);
            gpio_set_pull_mode(SD_PIN_MOSI, GPIO_PULLUP_ONLY);
            gpio_set_pull_mode(SD_PIN_CLK, GPIO_PULLUP_ONLY);
            gpio_set_pull_mode(SD_PIN_MISO, GPIO_PULLUP_ONLY);
            esp_rom_delay_us(100);
            printf("[PullUp  ] CS=%d, MOSI=%d, CLK=%d, MISO=%d (0 = shorted to GND)\n",
                   gpio_get_level(SD_PIN_CS), gpio_get_level(SD_PIN_MOSI),
                   gpio_get_level(SD_PIN_CLK), gpio_get_level(SD_PIN_MISO));
            printf("----------------------------------------\n");
        } else if (strncmp(cmd_line, "sd test", 7) == 0) {
            int cs_pin = SD_PIN_CS;
            int mosi_pin = SD_PIN_MOSI;
            int clk_pin = SD_PIN_CLK;
            int miso_pin = SD_PIN_MISO;
            sscanf(cmd_line + 7, "%d %d %d %d", &cs_pin, &mosi_pin, &clk_pin, &miso_pin);
            printf("=== Raw MicroSD SPI Hardware Ping (CMD0) ===\n");
            printf("Testing Pins: CS=GPIO %d, MOSI=GPIO %d, CLK=GPIO %d, MISO=GPIO %d\n",
                   cs_pin, mosi_pin, clk_pin, miso_pin);
            gpio_set_direction(cs_pin, GPIO_MODE_OUTPUT);
            gpio_set_direction(mosi_pin, GPIO_MODE_OUTPUT);
            gpio_set_direction(clk_pin, GPIO_MODE_OUTPUT);
            gpio_set_direction(miso_pin, GPIO_MODE_INPUT);
            gpio_set_pull_mode(miso_pin, GPIO_PULLUP_ONLY);

            gpio_set_level(cs_pin, 1);
            gpio_set_level(mosi_pin, 1);
            for (int i = 0; i < 80; i++) {
                gpio_set_level(clk_pin, 0);
                esp_rom_delay_us(10);
                gpio_set_level(clk_pin, 1);
                esp_rom_delay_us(10);
            }

            gpio_set_level(cs_pin, 0);
            esp_rom_delay_us(20);

            uint8_t cmd0[] = {0x40, 0x00, 0x00, 0x00, 0x00, 0x95};
            for (int b = 0; b < 6; b++) {
                uint8_t byte = cmd0[b];
                for (int bit = 7; bit >= 0; bit--) {
                    gpio_set_level(mosi_pin, (byte >> bit) & 1);
                    gpio_set_level(clk_pin, 0);
                    esp_rom_delay_us(5);
                    gpio_set_level(clk_pin, 1);
                    esp_rom_delay_us(5);
                }
            }

            gpio_set_level(mosi_pin, 1);

            printf("Bytes received from MISO: ");
            uint8_t resp[16] = {0};
            for (int b = 0; b < 16; b++) {
                uint8_t rx = 0;
                for (int bit = 7; bit >= 0; bit--) {
                    gpio_set_level(SD_PIN_CLK, 0);
                    esp_rom_delay_us(5);
                    if (gpio_get_level(SD_PIN_MISO)) {
                        rx |= (1 << bit);
                    }
                    gpio_set_level(SD_PIN_CLK, 1);
                    esp_rom_delay_us(5);
                }
                resp[b] = rx;
                printf("0x%02X ", rx);
            }
            printf("\n");

            gpio_set_level(SD_PIN_CS, 1);

            bool found_01 = false;
            for (int b = 0; b < 16; b++) {
                if (resp[b] == 0x01) {
                    found_01 = true;
                    break;
                }
            }

            if (found_01) {
                printf(">> [SUCCESS] MicroSD card is ALIVE! Responded 0x01 (In Idle State).\n");
                printf(">> Hardware SPI wiring is 100%% working!\n");
            } else if (resp[0] == 0xFF && resp[1] == 0xFF && resp[2] == 0xFF && resp[3] == 0xFF) {
                printf(">> [SILENT / 0xFF] Card did not reply. Common causes:\n"
                       "   1. Card not inserted completely into socket.\n"
                       "   2. Pin order reversed or swapped (check CS vs MISO, MOSI vs CLK).\n"
                       "   3. 3.3V power contact loose on breadboard.\n");
            } else if (resp[0] == 0x00 && resp[1] == 0x00 && resp[2] == 0x00 && resp[3] == 0x00) {
                printf(">> [ALL ZEROS / 0x00] MISO pin is shorted to GND or pulled down!\n");
            } else {
                printf(">> [UNEXPECTED BYTES] Received mixed signal (possible contact bounce or noise).\n");
            }
            printf("=============================================\n");
        } else if (strncmp(cmd_line, "sd ls", 5) == 0 || strncmp(cmd_line, "sd list", 7) == 0) {
            char path[128] = "/sdcard";
            if (strncmp(cmd_line, "sd ls", 5) == 0 && strlen(cmd_line) > 6) {
                sscanf(cmd_line + 6, "%127s", path);
            } else if (strncmp(cmd_line, "sd list", 7) == 0 && strlen(cmd_line) > 8) {
                sscanf(cmd_line + 8, "%127s", path);
            }
            DIR *dir = opendir(path);
            if (!dir) {
                printf("Error opening directory '%s' (errno=%d: %s)\n", path, errno, strerror(errno));
            } else {
                printf("\nDirectory listing for '%s':\n", path);
                struct dirent *entry;
                int count = 0;
                while ((entry = readdir(dir)) != NULL) {
                    char full_item[512];
                    snprintf(full_item, sizeof(full_item), "%s/%s", path, entry->d_name);
                    struct stat st;
                    long fsize = 0;
                    if (stat(full_item, &st) == 0) {
                        fsize = (long)st.st_size;
                    }
                    printf("  - %-24s [%s] %ld bytes\n",
                           entry->d_name, (entry->d_type == DT_DIR) ? "DIR " : "FILE", fsize);
                    count++;
                }
                closedir(dir);
                printf("Total items: %d\n\n", count);
            }
        } else if (strncmp(cmd_line, "sd cat", 6) == 0) {
            char path[128] = {0};
            if (sscanf(cmd_line + 6, "%127s", path) == 1) {
                FILE *f = fopen(path, "r");
                if (!f) {
                    printf("Error opening file '%s' (errno=%d: %s)\n", path, errno, strerror(errno));
                } else {
                    printf("\n=== File Content: '%s' ===\n", path);
                    char buf[128];
                    size_t n;
                    while ((n = fread(buf, 1, sizeof(buf) - 1, f)) > 0) {
                        buf[n] = '\0';
                        printf("%s", buf);
                    }
                    fclose(f);
                    printf("\n==================================\n\n");
                }
            } else {
                printf("Usage: sd cat <filepath> (e.g. sd cat /sdcard/data/telemetry/2026-09-26.csv)\n");
            }
        } else if (strcmp(cmd_line, "sd queue") == 0) {
            FILE *f = fopen("/sdcard/data/sync_queue.bin", "rb");
            if (!f) {
                printf("Error opening /sdcard/data/sync_queue.bin (errno=%d: %s)\n", errno, strerror(errno));
            } else {
                sync_queue_header_t hdr;
                if (fread(&hdr, 1, sizeof(hdr), f) == sizeof(hdr)) {
                    printf("\n=== MicroSD Store-and-Forward Header (24 Bytes, FSD §4.11.3) ===\n");
                    printf("Magic           : 0x%08lX (%s)\n", (unsigned long)hdr.magic, (hdr.magic == SYNC_QUEUE_MAGIC) ? "VALID" : "INVALID");
                    printf("Write Offset    : %lu bytes\n", (unsigned long)hdr.write_offset);
                    printf("Read Offset     : %lu bytes\n", (unsigned long)hdr.read_offset);
                    printf("Total Records   : %lu\n", (unsigned long)hdr.total_records);
                    printf("Unacked Count   : %lu\n", (unsigned long)hdr.unacked_count);
                    printf("Header CRC32    : 0x%08lX\n", (unsigned long)hdr.crc32);

                    printf("\n=== Records Stored in sync_queue.bin (172 Bytes each) ===\n");
                    sync_queue_record_t rec;
                    int idx = 0;
                    while (fread(&rec, 1, sizeof(rec), f) == sizeof(rec)) {
                        printf("  [#%d] TS: %lu | Type: 0x%02X | Dev: '%s' | Payload: '%s' | CRC: 0x%08lX\n",
                               idx++, (unsigned long)rec.timestamp, rec.record_type, rec.device_id, rec.payload, (unsigned long)rec.crc32);
                    }
                    printf("=================================================================\n\n");
                }
                fclose(f);
            }
        } else {
            printf("Usage: sd status | sd mount | sd ls [path] | sd cat <file> | sd queue | sd diag | sd test\n");
        }
    } else if (strncmp(cmd_line, "pin", 3) == 0) {
        int gpio = 0, level = 0;
        if (sscanf(cmd_line + 3, "%d %d", &gpio, &level) == 2) {
            gpio_reset_pin((gpio_num_t)gpio);
            gpio_set_direction((gpio_num_t)gpio, GPIO_MODE_OUTPUT);
            gpio_set_level((gpio_num_t)gpio, level ? 1 : 0);
            printf("OK: GPIO %d set to %d\n", gpio, level ? 1 : 0);
        } else {
            printf("Usage: pin <gpio_num> <0|1> (e.g. pin 2 1 or pin 48 0)\n");
        }
    } else if (strcmp(cmd_line, "system factory-reset") == 0 || strcmp(cmd_line, "factory-reset") == 0) {
        printf("[FACTORY_RESET] Erasing NVS credentials and restoring SoftAP password to empty (OPEN)...\n");
        config_manager_factory_reset();
        vTaskDelay(pdMS_TO_TICKS(500));
        esp_restart();
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
