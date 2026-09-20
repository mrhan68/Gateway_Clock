# Smart Clock Gateway System — Project Summary

> **Document Status**: Complete reference snapshot  
> **Last Updated**: 2026-09-20  
> **Target Hardware**: Espressif ESP32-S3 (16MB Flash, 8MB PSRAM, Dual-Core Xtensa LX7 @ 160MHz)  
> **Firmware Toolchain**: ESP-IDF v5.3.1 / FreeRTOS  
> **Cloud Stack**: Node.js / Express / Prisma (PostgreSQL on Aiven) / Embedded Aedes MQTT / React 18 (Vite)

---

## 1. Executive Summary

The **Smart Clock Gateway System** is an IoT platform centered around an ESP32-S3 microcontroller acting as an **Ambient Smart Clock & Industrial Edge IoT Gateway**:

- **Wireless Smart Home & Sensor Hub**: Interfaces with Bluetooth Low Energy (BLE) environmental sensors (temperature, humidity, battery) using standard GATT UUID `0x181A` and BTHome V2 protocols via a central NimBLE GAP passive observer. Per FSD §4.10, wired RS485 Modbus RTU has been deprecated in favor of wireless BLE, while a comprehensive Mock Peripherals subsystem remains active in firmware for simulation and regression testing.
- **Offline-First Automation & Alarms**: Runs dynamic IF-THEN rules (with hysteresis and Safe-Default fail-safe handling) and real-time daily schedules/alarms directly on the device with zero cloud dependency.
- **Local Web Dashboard & REST API**: Features an on-device HTTP server running on port 80 hosting a responsive Dark Mode Single Page Application and REST API (`/api/status`, `/api/telemetry`, `/api/relay`, `/api/rules`, `/api/wifi`) accessible directly on the local LAN.
- **High-Reliability Task Supervisor**: FreeRTOS Task Supervisor & Task Watchdog (TWDT) continuously monitors an 8-task heartbeat mask (`0xD7`) and executes automatic panic restarts within 15 seconds if any critical task stalls (`FR-SUP-02`).
- **Store-and-Forward Offline Protection**: Logs telemetry to flash storage (`/data/telemetry/*.csv`) and enqueues unsent data in a persistent FIFO queue (`sync_queue.bin`) to backfill cloud telemetry upon reconnection.
- **Interactive Serial CLI**: Provides a full command console on UART0 (115200 baud) for diagnostics, Wi-Fi configuration, BLE management, and fault injection.
- **Cloud Ecosystem**: Synchronizes telemetry and device state with an MQTT broker (`broker.emqx.io:1883`) connected to a Node.js backend and React management dashboard.

---

## 2. System Architecture

```text
┌──────────────────────────────┐
│           Web App            │ (React 18 + Vite + TailwindCSS)
│  (Management UI on GH Pages) │
└──────────────┬───────────────┘
               │ REST API (JSON / JWT)
               ▼
┌──────────────────────────────┐
│        Backend Server        │ (Node.js + Express + Prisma)
│     (Hosted on Render)       │
│  ┌────────────────────────┐  │
│  │ Embedded MQTT (Aedes)  │  │ (Broker on port 1883 + WS)
│  └────────────────────────┘  │
└──────────────┬───────────────┘
               │               ▲
               │ PostgreSQL    │ MQTT (Telemetry & Commands)
               ▼               │
┌──────────────────────────────┐ │
│      Database (Aiven)        │ │
│   PostgreSQL 7 models        │ │
└──────────────────────────────┘ │
                                 │
               ┌─────────────────┴─────────────────────────────────────────┐
               │    Smart Clock Edge Gateway & Controller (ESP32-S3)       │
               │   • Core 0: supervisor_task (TWDT), event_bus dispatcher, │
               │             scheduler_task, ble_scan_task (NimBLE)        │
               │   • Core 1: uplink_task (MQTT), serial_cli_task (UART0),  │
               │             web_server (SPA & REST API), mock_rs485_task  │
               │   • Memory: 16MB SPI Flash + 8MB Embedded PSRAM           │
               │   • Storage: Virtual CSV partition + NVS configuration    │
               │   • Clock: SNTP Internet Sync (ICT-7) + Internal Timer    │
               └───────────┬─────────────────────┬───────────────────┬─────┘
                           │ BLE 5.0 (NimBLE)    │ HTTP Port 80      │ Local MQTT
                           ▼                     ▼                   ▼
                 ┌───────────────────┐ ┌───────────────────┐ ┌─────────────────┐
                 │    BLE Sensors    │ │  LAN Web Browser  │ │  MQTT Actuators │
                 │ (Temp, Hum, Batt) │ │ (Dark Dashboard)  │ │ (Relays, Plugs) │
                 │ (GATT / BTHome V2)│ │ (192.168.1.x)     │ └─────────────────┘
                 └───────────────────┘ └───────────────────┘
```

---

## 3. Workspace Structure & Key Files

```text
qwen-test/
├── firmware/                       # ESP-IDF v5.3.1 Firmware Project
│   ├── CMakeLists.txt              # Build configuration
│   ├── sdkconfig.defaults          # Pinned configurations (16MB, NimBLE, HTTPD)
│   ├── partitions.csv              # Flash memory map
│   └── main/                       # Firmware C modules
│       ├── main.c                  # System bootstrap & task initialization
│       ├── ble_adapter.c/.h        # NimBLE GAP central observer & packet parser
│       ├── web_server.c/.h         # Web Dashboard & REST API
│       ├── wifi_provisioning.c/.h  # SoftAP Captive Portal & Wi-Fi scanner
│       ├── mqtt_adapter.c/.h       # MQTT client & topic router
│       ├── uplink.c                # Telemetry publisher task
│       ├── rule_engine.c/.h        # Automation Rule Engine (IF-THEN, Safe-Default)
│       ├── scheduler.c/.h          # Alarm & Schedule Manager
│       ├── supervisor.c/.h         # Task Supervisor & Watchdog Recovery (0xD7)
│       ├── config_manager.c/.h     # NVS Flash Configuration
│       ├── serial_cli.c/.h         # Interactive UART0 Console
│       ├── mock_peripherals.c/.h   # Mock Peripherals & Fault Injection
│       └── rtc.c/.h                # System Clock & SNTP Sync
├── tests/                          # Automated Testing Suites
│   └── host/                       # Host-tier pure logic tests (pytest)
│       ├── test_ble_parser.py      # BLE Advertisement & Service Data parsing
│       ├── test_rule_engine.py     # Rule condition evaluation & Safe-Default
│       ├── test_modbus_crc.py      # Modbus RTU CRC-16 calculation
│       ├── test_sync_queue.py      # Binary FIFO storage packaging
│       └── test_mqtt_adapter.py    # MQTT topic routing & JSON telemetry
├── scripts/                        # Target Hardware Test & Monitoring Tools
│   ├── monitor.py                  # Serial terminal monitor (115200 baud)
│   ├── test_bareboard_suite.py     # 8-in-1 automated hardware test suite
│   └── test_ble_battery.py         # 6-in-1 BLE scanner target verification
├── website/                        # Cloud & Web Ecosystem
│   ├── backend/                    # Node.js Express REST API + Aedes MQTT
│   └── web-app/                    # React 18 + Vite + TailwindCSS frontend
├── docs/                           # Three-Plane Documentation Model
│   ├── Functionality/FSD.md        # Authoritative FSD
│   ├── Method/                     # Architecture & Testing Standards
│   └── UserDocumentation/          # Operator & User Manual
├── testing/                        # AICLP Verification Assets
│   ├── test-plan.yaml              # Master test plan tracking verification contracts
│   └── debugging-agenda.md         # Hardware bringup burndown checklist
├── fsd-smart-clock-gateway.md      # Root FSD Specification Mirror
├── PROGRESS.md                     # Engineering progress and test execution log
├── DEV_LOG.md                      # Chronological development journal
└── MOCK_PERIPHERALS.md             # Virtual peripherals configuration guide
```

---

## 4. Verification & Testing Snapshot

- **Host Unit Tests (`pytest tests/host/`)**: **17 passed / 0 failed (100%)** in 0.05 seconds.
- **Bare-board Automated Suite (`scripts/test_bareboard_suite.py`)**: **8/8 passed (100%)** on physical ESP32-S3 hardware (`/dev/ttyACM0`).
- **BLE Scanner Battery (`scripts/test_ble_battery.py`)**: **6/6 passed (100%)** on physical ESP32-S3 hardware (`/dev/ttyACM0`).
- **Task Supervisor Hang Recovery (`TC-SUP-02` / `FR-SUP-02`)**: **PASSED** — detects task stalls within 15 seconds and automatically recovers via panic restart.
- **Critical Journey Tests (`JRN-01`, `JRN-04`, `TC-WEB-01`)**: **100% GREEN**.
