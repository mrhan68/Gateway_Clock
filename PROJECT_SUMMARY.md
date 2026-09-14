# Smart Clock Gateway System — Project Summary

> **Document Status**: Complete reference snapshot  
> **Last Updated**: 2026-09-12  
> **Target Hardware**: ESP32 (Smart Clock & Edge IoT Gateway)  
> **Cloud Stack**: Node.js / Express / Prisma (PostgreSQL) / Aedes MQTT / React (Vite)

---

## 1. Executive Summary

The **Smart Clock Gateway System** is an IoT platform centered around an ESP32-based Smart Clock acting as an **Edge IoT Gateway & Automation Controller**. 

Unlike simple IoT display clocks, this device:
- **Acts as an Industrial/Field IoT Hub**: Manages peripheral RS485 Modbus RTU devices (sensors & actuators), BLE peripherals, and local MQTT devices.
- **Executes automations locally (Offline-First)**: Runs schedules, alarms, and IF-THEN rules on-device even if internet connectivity is lost.
- **Store-and-Forward Offline Protection**: Logs telemetry to MicroSD card (`/data/telemetry/*.csv`) and enqueues unsent data in a persistent FIFO queue (`sync_queue.bin`), automatically backfilling to the cloud upon reconnect without memory overflow.
- **On-Site Configuration (No Cloud Dependency)**: Configurable on-site via `/sdcard/config.json` or interactive USB Serial CLI (UART0).
- **24/7 Reliability**: Monitored by a dedicated FreeRTOS Task Supervisor & Hardware Watchdog (TWDT) with heartbeat bitmasks and panic crash dumps.
- **Web Dashboard & Cloud Sync**: Synchronizes configuration, alarms, telemetry, and event history with the Node.js backend and React dashboard.

---

## 2. System Architecture

```
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
               │         Smart Clock Industrial Edge Gateway (ESP32)       │
               │   - Core 0: supervisor_task (TWDT), event_bus,            │
               │             rs485_task (Modbus RTU), scheduler, display   │
               │   - Core 1: uplink_task (MQTT/WiFi), sd_log_task (FATFS), │
               │             serial_cli_task (UART0), wifi_provisioning    │
               │   - Storage: SD Card (CSV logs, FIFO queue) + NVS (params)│
               │   - Display: OLED SSD1327 + RTC DS3231 + Buzzer           │
               └───────────┬─────────────────────┬───────────────────┬─────┘
                           │ RS485 (UART2)       │ BLE (NimBLE)      │ Local MQTT
                           ▼ (Half-Duplex)       ▼                   ▼
                 ┌───────────────────┐ ┌───────────────────┐ ┌─────────────────┐
                 │ Modbus RTU Slaves │ │    BLE Sensors    │ │  MQTT Actuators │
                 │ (Sensors/Relays)  │ │ (Temp, Hum, etc.) │ │ (Relays, Plugs) │
                 │ (Daisy-chain 120Ω)│ └───────────────────┘ └─────────────────┘
                 └───────────────────┘
```

---

## 3. Workspace Structure & Components

```
qwen-test/
├── fsd-smart-clock-gateway.md  # Complete Functional Specification (2,250+ lines)
├── CLAUDE.md                   # AICLP manual hardware testing methodology
├── README.md                   # Top-level quickstart & deployment guide
├── docs/
│   └── api/
│       └── SmartClockAPI.yaml  # OpenAPI 3.0 REST specification
├── website/                    # Web stack (Backend + Frontend)
│   ├── backend/                # Node.js Express API + Embedded MQTT Broker
│   │   ├── prisma/
│   │   │   └── schema.prisma   # 7 models (Gateway, Device, Schedule, Rule, etc.)
│   │   ├── src/
│   │   │   ├── app.js          # Express app & server bootstrap
│   │   │   ├── config/         # Environment configurations
│   │   │   ├── controllers/    # 7 controllers (devices, gateways, rules, etc.)
│   │   │   ├── middleware/     # Auth, validation, and error handlers
│   │   │   ├── mqtt/           # Aedes MQTT broker + message relay
│   │   │   ├── routes/         # 7 Express route files
│   │   │   └── utils/          # Helper utilities
│   │   ├── Dockerfile          # Container deployment for Render
│   │   └── docker-compose.yml  # Local Postgres & Redis container setup
│   └── web-app/                # React frontend application
│       ├── src/
│       │   ├── api/            # Axios client connecting to backend REST API
│       │   ├── components/     # Shared UI components (Navbar, Modals, Cards)
│       │   ├── pages/          # 6 views: Dashboard, Devices, Schedules,
│       │   │                   # Rules, Events, Settings
│       │   ├── App.jsx         # Router & layout
│       │   └── main.jsx        # Entrypoint
│       ├── dist/               # Production build ready for GitHub Pages
│       └── vite.config.js      # Vite configuration with proxy to :3000
├── firmware/                   # ESP32 ESP-IDF firmware codebase (pending)
├── testing/                    # Hardware verification test plans
└── tests/
    └── host/
        └── test_layout.py      # Host-tier pure logic pytest tests
```

---

## 4. Current Implementation Status

| Component | Status | Details |
|---|---|---|
| **FSD & Architecture** | ✅ Complete | Full 2,250-line specification ([`fsd-smart-clock-gateway.md`](fsd-smart-clock-gateway.md)) defining requirements, state machines, and protocols. |
| **API Spec** | ✅ Complete | OpenAPI 3.0 specification ([`docs/api/SmartClockAPI.yaml`](docs/api/SmartClockAPI.yaml)). |
| **Database Schema** | ✅ Complete | Prisma schema with 7 models: `User`, `Gateway`, `Device`, `Schedule`, `AutomationRule`, `EventLog`, `TelemetryLog`. |
| **Backend API** | ✅ Complete | 7 controllers & routes in `website/backend` covering gateway pairing, device management, rule CRUD, schedule triggers, event audit logs, and gateway sync endpoints. |
| **MQTT Broker** | ✅ Complete | Embedded Aedes MQTT broker running inside the backend process with authentication & topic routing. |
| **Web Frontend** | ✅ Complete | Modern React UI in `website/web-app` with Tailwind CSS, 6 functional views, modal dialogs, and built distribution in `dist/`. |
| **Firmware (ESP32)** | ⏳ Pending | `firmware/` directory is prepared for ESP-IDF / C implementation of the local rule engine, BLE scanner, and clock displays. |
| **Testing Harness** | 🟡 Scaffolding | Test plans declared in `testing/`, host-tier tests setup in `tests/host/`. |

---

## 5. Key Database Models

- **`Gateway`**: Tracks hardware ID, online/offline status, firmware version, last seen timestamp, and assigned user.
- **`Device`**: Peripheral IoT devices connected to a gateway via `ble` or `mqtt`. Tracks capabilities, current status, and latest telemetry value.
- **`Schedule`**: Automated timers, alarms (with buzzer support), or personal calendar events with day-of-week recurrence.
- **`AutomationRule`**: Local edge triggers (`IF sensor_value [gt|lt|eq] threshold THEN target_device action`).
- **`EventLog`**: Auditable event records synchronized from the edge gateway (rule triggered, device offline, manual override).
- **`TelemetryLog`**: Time-series sensor data history (temperature, humidity, lux, etc.).

---

## 6. Local Development Quickstart

### Running the Backend
```bash
cd website/backend
npm install
cp .env.example .env     # Configure DATABASE_URL (PostgreSQL)
npx prisma generate
npx prisma db push       # Sync schema to DB
npm run dev              # Starts Express on :3000 & MQTT on :1883
```

### Running the Web App
```bash
cd website/web-app
npm install
npm run dev              # Starts Vite on http://localhost:5173
```

---

## 7. Next Recommended Steps

1. **Firmware Implementation (`firmware/`)**:
   - Initialize ESP-IDF project structure.
   - Implement Wi-Fi provisioning and SNTP clock synchronization.
   - Implement BLE Central scanner and characteristic parsing for sensor nodes.
   - Implement local rule evaluation engine and offline event buffer.
2. **End-to-End Integration Testing**:
   - Connect virtual or physical ESP32 gateway to backend MQTT broker.
   - Verify bidirectional command execution and telemetry ingestion.
