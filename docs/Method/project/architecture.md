# System Architecture & Build Standards

> **Target Platform**: Espressif ESP32-S3 (Dual-Core Xtensa LX7 @ 160MHz, 16MB Flash, 8MB PSRAM)  
> **Firmware Framework**: ESP-IDF v5.3.1 / FreeRTOS  
> **Design Pattern**: Three-tier Layered Architecture (L0 / L1 / L2) with Central Event Bus

---

## 1. System Layering (L0 → L1 → L2)

The firmware is structured with strict one-way dependency: higher layers consume and orchestrate lower layers, while lower layers communicate upward exclusively through asynchronous events dispatched over the Event Bus.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    L2 — Application & Automation Logic                      │
│   • Rule Engine (IF-THEN, Safe-Default)      • Local Scheduler / Alarms     │
│   • Task Supervisor (Watchdog & Hang Panic)  • Config Manager (NVS Storage) │
└──────────────────────────────────────▲──────────────────────────────────────┘
                                       │ Events / Action Requests
═══════════════════════════════════════╪═══════════════════════════════════════
                         FreeRTOS Central Event Bus
═══════════════════════════════════════╪═══════════════════════════════════════
                                       │ Raw Events & Commands
┌──────────────────────────────────────▼──────────────────────────────────────┐
│                    L1 — Protocol Adapters & Interfaces                      │
│   • BLE Adapter (NimBLE GAP Scanner)         • Web Server (SPA & REST API)  │
│   • Wi-Fi Provisioning (SoftAP + Scanner)    • MQTT Adapter & Uplink Task   │
│   • Serial CLI Console (UART0 115200)        • Mock Peripherals & Storage   │
│   • System RTC & SNTP Sync (ICT-7)           • (RS485 Modbus RTU Mock/Depr) │
└──────────────────────────────────────▲──────────────────────────────────────┘
                                       │ Driver APIs / SDK Calls
┌──────────────────────────────────────┴──────────────────────────────────────┐
│                    L0 — Platform Foundation & SDK                           │
│   • ESP-IDF v5.3.1 (FreeRTOS, NimBLE, lwIP, NVS, esp_http_server, esp_timer)│
│   • Hardware: ESP32-S3 SoC, USB-UART Bridge, Wi-Fi 2.4GHz & Bluetooth 5.0   │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 1.1. Layer Definitions

- **L0 — Foundation / Platform**: Shared infrastructure provided by ESP-IDF v5.3.1. Handled by configuration (`sdkconfig.defaults`) rather than custom implementation: FreeRTOS SMP scheduler, NimBLE Bluetooth stack, lwIP TCP/IP stack, Non-Volatile Storage (NVS), `esp_http_server`, and peripheral drivers.
- **L1 — Interfaces & Adapters**: Modules that interface directly with external protocols or physical/virtual channels:
  - [`ble_adapter.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.c): Central GAP passive scanner for wireless environmental sensors (GATT UUID `0x181A`, BTHome V2). Parses packets and maps them to device IDs.
  - [`web_server.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/web_server.c): HTTP server running on port 80 hosting the local Single Page Application (Dark Mode Dashboard) and REST endpoints (`/api/status`, `/api/telemetry`, `/api/relay`, `/api/rules`, `/api/wifi`).
  - [`wifi_provisioning.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/wifi_provisioning.c): SoftAP Captive Portal (`SmartClock-Setup`), DNS redirection, and Wi-Fi scan engine.
  - [`mqtt_adapter.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/mqtt_adapter.c) & [`uplink.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c): MQTT client managing broker connection, LWT status, topic routing, and telemetry uploads.
  - [`serial_cli.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/serial_cli.c): Command-line interpreter on UART0 providing interactive diagnostic, configuration, and fault-injection hooks.
  - [`mock_peripherals.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/mock_peripherals.c): Full-stack mock subsystem generating simulated telemetry and injecting bus errors for bare-board validation.
  - [`rtc.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/rtc.c): Real-Time Clock engine supporting internal timer fallback and SNTP Internet synchronization.
- **L2 — Application & Automation Logic**: Pure decision-making and control logic:
  - [`event_bus.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/event_bus.c): FreeRTOS event queue (20 slots) with asynchronous event dispatcher and decoupled memory ownership.
  - [`supervisor.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.c): Task Supervisor & Task Watchdog (TWDT) tracking 8 task heartbeats (`0xD7`), detecting stalled tasks, and triggering automatic panic restart (`FR-SUP-02`).
  - [`rule_engine.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c): IF-THEN rule evaluation (`GT`, `LT`, `EQ`) against telemetry with Safe-Default fail-safe protection.
  - [`scheduler.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/scheduler.c): Time-based alarm and automation scheduler with 5-second polling and minute-edge triggers.
  - [`config_manager.c`](file:///home/chien/ai-workspace/qwen-test/firmware/main/config_manager.c): Persistent configuration manager reading/writing settings to NVS Flash.

---

## 2. FreeRTOS Dual-Core & Task Allocation

The dual-core Xtensa LX7 processor is balanced to guarantee deterministic execution of real-time control tasks without interference from network and I/O tasks:

| Core | Task Name | Priority | Stack Size | Function & Heartbeat Bit |
|:---:|---|:---:|:---:|---|
| **Core 0** | `supervisor_task` | 10 | 4096 | Task Watchdog, checks 8-task bitmask every 3s |
| **Core 0** | `event_dispatcher` | 6 | 6144 | Central event queue dispatch (`TASK_HEARTBEAT_EVENT_BUS` - bit 0) |
| **Core 0** | `scheduler_task` | 5 | 4096 | Real-time alarms & schedules (`TASK_HEARTBEAT_SCHEDULER` - bit 4) |
| **Core 0** | `ble_scan_task` | 5 | 4096 | NimBLE GAP passive observer (`TASK_HEARTBEAT_BLE` - bit 7) |
| **Core 1** | `main_task` / App | 1 | 8192 | Bootstrap & loop (`TASK_HEARTBEAT_MAIN` - bit 3) |
| **Core 1** | `uplink_task` | 4 | 4096 | MQTT publishing & cloud sync (`TASK_HEARTBEAT_UPLINK` - bit 2) |
| **Core 1** | `serial_cli_task` | 3 | 8192 | Interactive UART0 console (`TASK_HEARTBEAT_CLI` - bit 6) |
| **Core 1** | `web_server` / Prov | 5 | 8192 | `esp_http_server` handling REST API and Dashboard |
| **Core 1** | `mock_rs485_task` | 4 | 4096 | Simulated peripheral telemetry (`TASK_HEARTBEAT_RS485` - bit 1) |

---

## 3. Repository Source Layout

```text
qwen-test/
├── firmware/                       # ESP-IDF Firmware Project
│   ├── CMakeLists.txt              # Top-level build configuration
│   ├── sdkconfig.defaults          # Persistent configs (16MB Flash, NimBLE, HTTPD)
│   ├── partitions.csv              # Flash partition map (factory, nvs, storage)
│   └── main/                       # Firmware source code (L1/L2 modules)
│       ├── CMakeLists.txt          # Component declaration & dependencies
│       ├── main.c                  # System bootstrap & composition root
│       ├── event_bus.c/.h          # Central FreeRTOS Event Bus
│       ├── supervisor.c/.h         # Task Supervisor & Watchdog Recovery
│       ├── config_manager.c/.h     # NVS Flash Configuration
│       ├── serial_cli.c/.h         # Serial CLI Console (UART0)
│       ├── ble_adapter.c/.h        # NimBLE Wireless Telemetry Scanner
│       ├── wifi_provisioning.c/.h  # Wi-Fi SoftAP Portal & Scanner
│       ├── web_server.c/.h         # Web Dashboard & REST API
│       ├── mqtt_adapter.c/.h       # MQTT Broker Client
│       ├── uplink.c                # Telemetry Publisher Task
│       ├── rule_engine.c/.h        # Automation Rule Engine
│       ├── scheduler.c/.h          # Alarm & Schedule Manager
│       ├── mock_peripherals.c/.h   # Mock Peripheral Subsystem
│       └── rtc.c/.h                # System Clock & SNTP Sync
├── tests/                          # Automated Host Unit Tests
│   └── host/                       # Pure logic pytest suite (17 tests)
│       ├── test_ble_parser.py      # BLE Advertisement & Service Data parsing
│       ├── test_rule_engine.py     # IF-THEN condition & Safe-Default logic
│       ├── test_modbus_crc.py      # Modbus RTU CRC-16 calculation
│       ├── test_sync_queue.py      # Binary FIFO storage packaging
│       └── test_mqtt_adapter.py    # MQTT topic routing & JSON telemetry
├── scripts/                        # Target Hardware Test & Monitoring Tools
│   ├── monitor.py                  # Serial monitor utility
│   ├── test_bareboard_suite.py     # 8-in-1 automated hardware test suite
│   └── test_ble_battery.py         # 6-in-1 BLE scanner target verification
├── website/                        # Cloud & Web Ecosystem
│   ├── backend/                    # Node.js / Express / Prisma / MQTT backend
│   └── web-app/                    # React 18 / Vite / TailwindCSS web UI
├── docs/                           # Three-Plane Documentation Model
│   ├── Functionality/FSD.md        # WHAT Plane (Authoritative FSD)
│   ├── Method/                     # HOW Plane (Architecture & Standards)
│   └── UserDocumentation/          # OPERATE Plane (User Manual)
├── testing/                        # AICLP Verification Assets
│   ├── test-plan.yaml              # Master test plan tracking verification contracts
│   └── debugging-agenda.md         # Hardware bringup burndown checklist
├── fsd-smart-clock-gateway.md      # Root FSD Mirror
├── PROGRESS.md                     # Engineering progress and test execution log
├── PROJECT_SUMMARY.md              # Executive summary and architectural snapshot
├── DEV_LOG.md                      # Chronological development journal
└── MOCK_PERIPHERALS.md             # Virtual peripherals configuration guide
```
