# Smart Clock & Edge IoT Gateway — Project Documentation Plane Map

This project adopts the **Three-Plane Model** for embedded systems documentation under the **AI Closed-Loop Programming (AICLP)** methodology. Each plane addresses a distinct question and maintains strict traceability between specification, implementation, and operational use.

---

## 1. Documentation Plane Overview

| Question | Plane | Canonical Document / Directory | Role & Responsibility |
|---|---|---|---|
| **What must be true?** | **WHAT** — Functional Specification | [docs/Functionality/FSD.md](file:///home/chien/ai-workspace/qwen-test/docs/Functionality/FSD.md)<br>*(Mirrored at [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md))* | Defines system goals, non-goals, architecture, and every atomic functional requirement (`FR-*`) with falsifiable verification contracts and operator test instructions. |
| **How is it built?** | **HOW** — Method & Architecture | [docs/Method/00-Overview.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/00-Overview.md)<br>[docs/Method/project/architecture.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/project/architecture.md)<br>[docs/Method/project/testing-standard.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/project/testing-standard.md) | Enforces L0/L1/L2 component layering, build toolchain contracts (ESP-IDF v5.3.1 for ESP32-S3), CI pipeline standards, and multi-tiered test execution rules. |
| **How do I run it?** | **OPERATE** — User & Operator Manual | [docs/UserDocumentation/User-Manual.md](file:///home/chien/ai-workspace/qwen-test/docs/UserDocumentation/User-Manual.md) | Guides the operator through hardware setup, flashing firmware, serial CLI console interaction, local Web Dashboard usage, automated test suites, and troubleshooting. |

---

## 2. Supporting Tracking & Reference Documents

| Document | Purpose |
|---|---|
| [README.md](file:///home/chien/ai-workspace/qwen-test/README.md) | Top-level repository overview, quickstart, full-stack architecture, and build guide. |
| [PROGRESS.md](file:///home/chien/ai-workspace/qwen-test/PROGRESS.md) | Comprehensive engineering progress tracker, test matrices, pass/fail records, and hardware status. |
| [PROJECT_SUMMARY.md](file:///home/chien/ai-workspace/qwen-test/PROJECT_SUMMARY.md) | High-level executive snapshot and system capability summary. |
| [DEV_LOG.md](file:///home/chien/ai-workspace/qwen-test/DEV_LOG.md) | Chronological engineering and hardware bringup journal detailing sprint accomplishments. |
| [MOCK_PERIPHERALS.md](file:///home/chien/ai-workspace/qwen-test/MOCK_PERIPHERALS.md) | Simulation specifications for virtual telemetry, fault injection, and bare-board regression. |
| [testing/test-plan.yaml](file:///home/chien/ai-workspace/qwen-test/testing/test-plan.yaml) | Executable test plan tracking verification contracts across host, target, and manual tiers. |
| [testing/debugging-agenda.md](file:///home/chien/ai-workspace/qwen-test/testing/debugging-agenda.md) | Checklist of hardware subsystems unproven on first physical bringup. |
| [docs/api/API-Guide.md](file:///home/chien/ai-workspace/qwen-test/docs/api/API-Guide.md) | REST API and MQTT topic convention reference for the Cloud Backend and Web App. |

---

## 3. Current System Status

- **Target Hardware**: ESP32-S3 (Dual-Core Xtensa LX7 @ 160MHz, 16MB Flash, 8MB PSRAM).
- **Core Firmware**: FreeRTOS dual-core dispatching, Task Supervisor with 8-task bitmask tracking (`0xD7`), Task Watchdog (TWDT), and automatic hang panic recovery (`FR-SUP-02`).
- **Wireless Smart Home**: Wi-Fi Station + SoftAP Captive Portal, SNTP RTC sync, always-on Web Dashboard SPA & REST API, NimBLE BLE GAP passive observer for environmental sensors (UUID `0x181A` / BTHome V2).
- **Automation & Edge Logic**: Offline-first Rule Engine (IF-THEN, hysteresis, Safe-Default), Local Scheduler (`TC-CLK-03`), and Mock Peripherals subsystem for bare-board regression testing.
- **Verification Status**:
  - Host Unit Tests (`pytest tests/host/`): **17 passed / 0 failed (100%)**
  - Bare-board Automated Suite (`scripts/test_bareboard_suite.py`): **8/8 passed (100%)**
  - Target BLE Battery Suite (`scripts/test_ble_battery.py`): **6/6 passed (100%)**
  - Critical Journey Paths: `JRN-01` (Boot) & `JRN-04` (Network/MQTT) **PASSED** on physical hardware.
