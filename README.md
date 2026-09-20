# Smart Clock & Wireless Smart Home IoT Gateway

> **Full-Stack IoT Platform**: ESP32-S3 Industrial Edge Gateway & Smart Clock, BLE Wireless Sensor Hub, On-Device Web Dashboard, Cloud MQTT Uplink, and Management Web App.

---

## 1. System Overview

```
┌─────────────────────────────────┐                 ┌─────────────────────────────────┐
│        Local Management         │                 │         Cloud Ecosystem         │
│                                 │                 │                                 │
│  ┌───────────────────────────┐  │                 │  ┌───────────────────────────┐  │
│  │   On-Device Web UI        │  │                 │  │   Cloud Web App (React)   │  │
│  │   http://<gateway-ip>/    │  │                 │  │   (Vite + TailwindCSS)    │  │
│  └─────────────┬─────────────┘  │                 │  └─────────────┬─────────────┘  │
│                │ HTTP REST      │                 │                │ REST API       │
│                ▼                │                 │                ▼                │
│  ┌───────────────────────────┐  │   MQTT Uplink   │  ┌───────────────────────────┐  │
│  │   ESP32-S3 Smart Clock    │◄─┼─────────────────┼─►│   Backend Server (Node)   │  │
│  │   Edge IoT Gateway        │  │ (broker.emqx.io)│  │   Embedded Aedes Broker   │  │
│  └─────────────┬─────────────┘  │                 │  └─────────────┬─────────────┘  │
│                │ BLE 5.0        │                 │                │ PostgreSQL     │
│                ▼                │                 │                ▼                │
│  ┌───────────────────────────┐  │                 │  ┌───────────────────────────┐  │
│  │   BLE Wireless Sensors    │  │                 │  │   Aiven Cloud Database    │  │
│  │   (Temp, Hum, Battery)    │  │                 │  │   (7 Prisma Data Models)  │  │
│  └───────────────────────────┘  │                 │  └───────────────────────────┘  │
└─────────────────────────────────┘                 └─────────────────────────────────┘
```

The **Smart Clock Gateway** serves as a dual-purpose device: an interactive ambient smart clock and an industrial-grade **Edge IoT Automation Controller**:
- **ESP32-S3 Microcontroller**: Dual-core Xtensa LX7 @ 160MHz with 16MB Flash and 8MB PSRAM running FreeRTOS.
- **BLE Wireless Environmental Hub**: NimBLE GAP passive observer continuously decoding standard GATT UUID `0x181A` (Temperature/Humidity) and BTHome V2 sensor broadcasts.
- **Offline-First Rule Engine**: Executes IF-THEN rules with hysteresis and Safe-Default protection directly on-chip without Internet dependency.
- **Local Scheduler & Alarms**: Manages recurring daily alarms and scheduled automation actions synchronized via SNTP Internet time (ICT-7).
- **On-Device Web Dashboard**: Built-in HTTP server on port 80 delivering a Dark Mode Single Page Application and full REST API (`/api/status`, `/api/telemetry`, `/api/relay`, `/api/rules`, `/api/wifi`).
- **High-Reliability Task Supervisor**: FreeRTOS Task Watchdog (TWDT) tracking an 8-task heartbeat mask (`0xD7`) with automatic 15-second hang detection panic restart (`FR-SUP-02`).
- **Cloud Ecosystem**: Node.js / Express backend with embedded Aedes MQTT broker and PostgreSQL database (Aiven), accompanied by a responsive React 18 / TailwindCSS management dashboard.

---

## 2. Project Directory Structure

```text
qwen-test/
├── firmware/                       # ESP32-S3 Firmware (ESP-IDF v5.3.1)
│   ├── CMakeLists.txt              # Build configuration
│   ├── sdkconfig.defaults          # Pinned configurations (16MB Flash, NimBLE, HTTPD)
│   ├── partitions.csv              # Flash memory map (bootloader, nvs, factory, storage)
│   └── main/                       # Firmware C source modules
│       ├── main.c                  # Bootstrap & module initialization
│       ├── ble_adapter.c/.h        # NimBLE GAP central scanner & packet parser
│       ├── web_server.c/.h         # On-device Web Dashboard & REST API
│       ├── wifi_provisioning.c/.h  # SoftAP Captive Portal & Wi-Fi scanner
│       ├── mqtt_adapter.c/.h       # MQTT client & topic router
│       ├── uplink.c                # Telemetry publisher task
│       ├── rule_engine.c/.h        # Offline-first automation engine
│       ├── scheduler.c/.h          # Real-time alarms & schedule executor
│       ├── supervisor.c/.h         # Task Supervisor & Watchdog panic recovery
│       ├── config_manager.c/.h     # NVS Flash parameter storage
│       ├── serial_cli.c/.h         # Interactive UART0 console interpreter
│       ├── mock_peripherals.c/.h   # Mock peripherals subsystem & fault injector
│       └── rtc.c/.h                # System clock & SNTP synchronization
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
├── website/                        # Cloud & Web Platform
│   ├── backend/                    # Node.js Express REST API + Aedes MQTT broker
│   │   ├── prisma/schema.prisma    # PostgreSQL schema (7 models)
│   │   ├── src/                    # Controllers, routes, and MQTT relay
│   │   └── Dockerfile              # Container deployment for Render
│   └── web-app/                    # React 18 + Vite + TailwindCSS frontend
│       └── src/                    # Pages (Dashboard, Devices, Schedules, Rules)
├── docs/                           # Three-Plane Documentation Model
│   ├── Functionality/FSD.md        # WHAT Plane (Authoritative FSD)
│   ├── Method/                     # HOW Plane (Architecture & Build Standards)
│   └── UserDocumentation/          # OPERATE Plane (Operator & User Manual)
├── testing/                        # AICLP Verification Assets
│   ├── test-plan.yaml              # Master test plan tracking verification contracts
│   └── debugging-agenda.md         # Hardware bringup burndown checklist
├── fsd-smart-clock-gateway.md      # Root FSD Specification Mirror
├── PROGRESS.md                     # Detailed engineering progress tracker
├── PROJECT_SUMMARY.md              # Executive architectural summary
├── DEV_LOG.md                      # Chronological development journal
└── MOCK_PERIPHERALS.md             # Virtual peripherals configuration guide
```

---

## 3. Quick Start

### 3.1. ESP32-S3 Firmware Build & Flash

```bash
# 1. Navigate to firmware directory and configure target
cd firmware
idf.py set-target esp32s3

# 2. Build firmware
idf.py build

# 3. Flash to board (e.g., /dev/ttyACM0 at 460800 baud)
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 40m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x20000 build/smart_clock_gateway.bin

# 4. Open Serial Monitor (115200 baud)
python ../scripts/monitor.py
```

### 3.2. Running Automated Verification Suites

```bash
# Run Host Automated Unit Tests (17 tests, ~0.05s)
pytest tests/host/ -v

# Run Bare-Board Hardware Test Suite (requires connected ESP32-S3 on /dev/ttyACM0)
python scripts/test_bareboard_suite.py

# Run BLE Scanner Target Battery (requires connected ESP32-S3 on /dev/ttyACM0)
python scripts/test_ble_battery.py
```

### 3.3. Cloud Backend Setup (Node.js & PostgreSQL)

```bash
cd website/backend
npm install
cp .env.example .env
# Configure DATABASE_URL in .env
npx prisma db push
npm run dev
# Backend runs at http://localhost:3000 (REST) and mqtt://localhost:1883
```

### 3.4. Management Web App (React & Vite)

```bash
cd website/web-app
npm install
npm run dev
# Web App opens at http://localhost:5173
```

---

## 4. Verification & Testing Status

| Test Tier | Suite | Scope | Status |
|---|---|---|:---:|
| **Host Unit** | `pytest tests/host/` | CRC-16, Sync Queue, Rule Engine, MQTT, BLE Parser | 🟢 **17/17 PASSED** |
| **Bare-Board Hardware** | `scripts/test_bareboard_suite.py` | CLI, Safe-Default, Rule CRUD, Scheduler, Storage, Hang Recovery | 🟢 **8/8 PASSED** |
| **Target BLE Scanner** | `scripts/test_ble_battery.py` | NimBLE Scanner, Device Registration, Telemetry Injection, Heartbeat | 🟢 **6/6 PASSED** |
| **Journey Critical Paths** | `testing/test-plan.yaml` | `JRN-01` (Boot), `JRN-04` (Wi-Fi/MQTT/SNTP), `TC-WEB-01` (Dashboard) | 🟢 **100% GREEN** |

---

## 5. Documentation Links

- **Functional Specification (FSD)**: [docs/Functionality/FSD.md](file:///home/chien/ai-workspace/qwen-test/docs/Functionality/FSD.md) *(or [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md))*
- **Operator & User Manual**: [docs/UserDocumentation/User-Manual.md](file:///home/chien/ai-workspace/qwen-test/docs/UserDocumentation/User-Manual.md)
- **System Architecture & Build Standards**: [docs/Method/project/architecture.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/project/architecture.md)
- **Testing Standard**: [docs/Method/project/testing-standard.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/project/testing-standard.md)
- **Cloud REST API Guide**: [docs/api/API-Guide.md](file:///home/chien/ai-workspace/qwen-test/docs/api/API-Guide.md)
- **Virtual Peripherals Guide**: [MOCK_PERIPHERALS.md](file:///home/chien/ai-workspace/qwen-test/MOCK_PERIPHERALS.md)
- **Engineering Progress Tracker**: [PROGRESS.md](file:///home/chien/ai-workspace/qwen-test/PROGRESS.md)
- **Development Journal**: [DEV_LOG.md](file:///home/chien/ai-workspace/qwen-test/DEV_LOG.md)

---

## 6. License & Methodology

This project is licensed under the MIT License and developed following the **AI Closed-Loop Programming (AICLP)** methodology. See [AGENTS.md](file:///home/chien/ai-workspace/qwen-test/AGENTS.md) and [CLAUDE.md](file:///home/chien/ai-workspace/qwen-test/CLAUDE.md) for agent operating guidelines.
