# Method Plane — Overview

> **Purpose**: This plane defines **HOW** the system is built, verified, and changed. It establishes the build contract, code layering rules, testing standards, and development workflows for the **Smart Clock & Edge IoT Gateway** project under the AICLP methodology.

---

## 1. Documentation Index

| File | Scope & Contents |
|---|---|
| [project/architecture.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/project/architecture.md) | Layering rules (L0/L1/L2), component isolation, FreeRTOS task assignment, source tree layout. |
| [project/testing-standard.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/project/testing-standard.md) | Multi-tier test taxonomy (Host, Target, Manual), evidence capture standards, and regression policies. |
| [AI-Workflow.md](file:///home/chien/ai-workspace/qwen-test/docs/Method/AI-Workflow.md) *(or [CLAUDE.md](file:///home/chien/ai-workspace/qwen-test/CLAUDE.md))* | AI Closed-Loop Programming operating rules and operator interaction protocol in manual mode. |

---

## 2. Build & Hardware Contracts

- **Target Microcontroller**: Espressif **ESP32-S3** (QFN56 revision v0.2, Dual-core Xtensa LX7 @ 160MHz).
- **Memory Configuration**: 16MB Quad SPI Flash (offset `0x0` bootloader, `0x8000` partition table, `0x20000` factory app), 8MB embedded PSRAM.
- **Toolchain**: **ESP-IDF v5.3.1** (pinned in CI container `espressif/idf:v5.3.1` and local builds).
- **USB Serial / Debug Port**: CH343 USB-to-UART Bridge mapped to `/dev/ttyACM0` (Windows host COM7 via USB/IP).
  - Serial Monitor: `115200` baud, 8-N-1.
  - Flash Baudrate: `460800` baud.

### Build Commands

```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

### Flashing Commands

Using `esptool.py` directly:
```bash
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 40m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x20000 build/smart_clock_gateway.bin
```

Or using `idf.py`:
```bash
idf.py -p /dev/ttyACM0 flash
```

### Serial Monitoring

```bash
python scripts/monitor.py
# Or using screen / minicom:
screen /dev/ttyACM0 115200
```

---

## 3. Continuous Integration Pipeline

The repository CI workflow is declared in [`.github/workflows/ci.yml`](file:///home/chien/ai-workspace/qwen-test/.github/workflows/ci.yml) and executes on every push and pull request to `main`:
1. **Host Tests Job**: Sets up Python 3.11+, installs test dependencies, and runs all 17 automated tests (`pytest tests/host/`).
2. **Firmware Build Job**: Runs inside the `espressif/idf:v5.3.1` container, compiles firmware for `esp32s3`, and generates packaged release artifacts (`smart_clock_gateway.bin`, `.elf`, `coldflash/` bundle).

---

## 4. AICLP Loop Discipline

```
Requirement Contract in FSD (docs/Functionality/FSD.md)
  └── Test Declared in Plan (testing/test-plan.yaml)
        └── Host Executable Test (pytest) / Target Test Script (scripts/)
              └── Code Implementation (firmware/main/)
                    └── Compilation & Build (idf.py / CI)
                          └── [Operator Flash & Execute / Automated Script]
                                └── Green Verification Recorded in test-plan.yaml
```
