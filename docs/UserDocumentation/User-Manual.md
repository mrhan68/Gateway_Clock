# User & Operator Manual — Smart Clock & Edge IoT Gateway

> **Product**: Smart Clock & Wireless Smart Home IoT Gateway  
> **Platform**: Espressif ESP32-S3 (Dual-Core Xtensa LX7 @ 160MHz, 16MB Flash, 8MB PSRAM)  
> **Firmware Version**: v1.3.0  
> **Status**: Production-ready Wave 1 (Wi-Fi + BLE Wireless Stack Fully Verified)

---

## 1. System Overview

The **Smart Clock Edge IoT Gateway** is an offline-first smart home hub and automation controller housed within an IoT digital clock device. Key capabilities include:

- **Wireless Peripheral Hub**: Collects telemetry from Bluetooth Low Energy (BLE) environmental sensors (temperature, humidity, battery) using standard GATT UUID `0x181A` and BTHome V2 protocols.
- **Local Automation Engine (Offline-First)**: Evaluates user-configured IF-THEN rules and executes real-time schedules/alarms directly on the device with no cloud dependency.
- **Local Web Dashboard & REST API**: Hosts an always-on Dark Mode Web Dashboard on port 80 (`http://<device-ip>/`) for relay control, telemetry display, Wi-Fi scanning, and rule configuration.
- **Cloud Uplink**: Synchronizes device status and telemetry to an MQTT broker (`broker.emqx.io:1883`) with Last Will and Testament (LWT) monitoring.
- **Industrial Reliability**: Protected by an active FreeRTOS Task Supervisor tracking an 8-task heartbeat mask (`0xD7`) and a Task Watchdog (TWDT) with 15-second hang detection panic restart (`FR-SUP-02`).

---

## 2. Hardware Specifications

| Component | Specification | Description |
|---|---|---|
| **SoC** | ESP32-S3-WROOM-1 | Dual-Core 32-bit Xtensa LX7 running at 160MHz |
| **Flash Memory** | 16MB Quad SPI Flash | Partitioned for bootloader (`0x0`), partitions (`0x8000`), factory app (`0x20000`), NVS (`0x9000`), and storage (`0x420000`) |
| **RAM** | 512KB SRAM + 8MB PSRAM | ~116KB free internal DRAM under full wireless load |
| **Wi-Fi** | 802.11 b/g/n (2.4 GHz) | Station mode + SoftAP Captive Portal (`SmartClock-Setup`) |
| **Bluetooth** | BLE 5.0 (NimBLE stack) | Central GAP passive observer for sensor beacon broadcasts |
| **USB Interface** | CH343 USB-UART Bridge | USB Type-C port mapped to UART0 console (115200 baud, flash at 460800 baud) |
| **Power Input** | 5V DC via USB Type-C | Current consumption: 120mA – 280mA under active Wi-Fi/BLE transmission |

---

## 3. Quick Start & Initial Setup

### 3.1. Connecting the Device
1. Connect the ESP32-S3 board to your computer using a high-quality USB Type-C data cable.
2. The computer will enumerate a serial device:
   - **Linux / WSL**: `/dev/ttyACM0` or `/dev/ttyUSB0`
   - **Windows**: `COMx` (e.g., `COM7`)
3. Open a terminal at **115200 baud** (8 data bits, no parity, 1 stop bit):
   ```bash
   python scripts/monitor.py
   # Or using screen:
   screen /dev/ttyACM0 115200
   ```
4. Press the **RESET (RST)** button. You will observe the boot sequence:
   ```text
   [BOOT] Starting Smart Clock Edge Gateway (Target: ESP32-S3)...
   [SYS] Alive marker 1
   [SYS] System fully operational in wireless Smart Home mode (Wi-Fi + BLE).
   ```

### 3.2. Wi-Fi Provisioning via Captive Portal
If no Wi-Fi credentials are saved, the gateway automatically launches an access point:
1. Connect your phone or laptop to the Wi-Fi network: **`SmartClock-Setup`** (no password).
2. The browser automatically opens the Captive Portal at `http://192.168.4.1/`.
3. Select your local 2.4GHz Wi-Fi network from the scanned dropdown list, enter the password, and click **Connect**.
4. The device connects to your Wi-Fi, synchronizes its clock with SNTP (`pool.ntp.org`), establishes an MQTT connection to `broker.emqx.io`, and prints its assigned LAN IP address (e.g., `192.168.1.143`).

---

## 4. Local Web Dashboard

Once connected to your local network, access the built-in Web Dashboard by entering the gateway's IP address in any modern web browser:

```text
http://192.168.1.143/
```

### Dashboard Features
- **Real-Time Clock & Telemetry Card**: Displays synchronized local time (ICT-7 / Asia/Ho_Chi_Minh), current ambient temperature, humidity, and uptime.
- **Relay Control Board**: Four toggle switches to directly control Relay 1 through Relay 4 (`fan_relay_01`, `heater_relay_01`, `dehumidifier_01`, `aux_relay_04`).
- **Rule Engine Manager**: View active automation rules, delete rules, or submit new IF-THEN rules directly from the browser.
- **Wi-Fi Scanner**: Scan and view nearby wireless access points with RSSI signal strength.

---

## 5. Serial CLI Reference (UART0 @ 115200 Baud)

The interactive CLI provides comprehensive control, diagnostics, and testing commands:

### System & Diagnostics
| Command | Description | Example Output / Usage |
|---|---|---|
| `help` | Display list of all available commands | Prints command syntax and parameters |
| `status` | Display system uptime, free heap, and task heartbeat mask | `Uptime: 340s, Free Heap: 118420 bytes, Active: 0xD7, Alive: 0xD7` |
| `time` | Display current RTC time and epoch timestamp | `Current System Time: 2026-09-20 09:30:15 (Epoch: 1789896615)` |
| `reboot` | Perform a clean software restart of the gateway | `Restarting ESP32 Gateway...` |

### Wi-Fi Commands
| Command | Description | Example Output / Usage |
|---|---|---|
| `wifi status` | Display current connection state and IP address | `Wi-Fi Status: CONNECTED, Current IP: 192.168.1.143` |
| `wifi scan` | Scan and display nearby 2.4GHz Wi-Fi access points | Lists SSID, RSSI, and security type |
| `wifi select <idx> [pass]` | Connect to a scanned network by its list index | `wifi select 1 mySecretPassword` |

### BLE Wireless Telemetry Commands
| Command | Description | Example Output / Usage |
|---|---|---|
| `ble status` | Show NimBLE stack state and registered sensor list | `BLE Scanner: RUNNING, Registered Sensors: 2` |
| `ble start` | Start the passive GAP central observer scanner | `OK: BLE GAP passive scanner started.` |
| `ble stop` | Stop the passive GAP central observer scanner | `OK: BLE GAP passive scanner stopped.` |
| `ble reg <dev_id> [mac]` | Register a BLE sensor device ID and MAC address | `ble reg sensor_bedroom 11:22:33:44:55:66` |
| `ble test [val]` | Inject synthetic BLE advertisement packet | `ble test 33.5` (Injects 33.50°C into sensor) |

### Automation Rule Engine Commands
| Command | Description | Example Output / Usage |
|---|---|---|
| `rule list` | List all active IF-THEN rules | `Rule #1: sensor_temp_01 GT 30.0 -> fan_relay_01 ON` |
| `rule add [id] <cond> <target> <cmd>` | Add a new rule (`GT` or `LT`) | `rule add 4 sensor_temp_01 GT 40.0 fan_relay_01 ON` |
| `rule test <sensor> <val> <valid>` | Test rule evaluation against arbitrary telemetry | `rule test sensor_temp_01 32.5 1` |
| `rule delete <id>` | Delete an existing rule by its numerical ID | `rule delete 4` |
| `rule clear` | Remove all rules and reset to empty table | `OK: All rules cleared.` |

### Local Scheduler Commands
| Command | Description | Example Output / Usage |
|---|---|---|
| `schedule list` | List all configured alarms and automation schedules | `Schedule #1: alarm at 07:00 (Daily Morning Alarm)` |
| `schedule add [id] <alarm\|auto> <HH:MM> <title> [cmd]` | Add a new scheduled event | `schedule add 2 alarm 06:30 "Wake Up"` |
| `schedule test [title]` | Trigger an immediate alarm notification | `schedule test "Fire Drill"` |
| `schedule clear` | Remove all scheduled entries | `OK: All schedule entries cleared.` |

### Device Control & Fault Injection
| Command | Description | Example Output / Usage |
|---|---|---|
| `cmd send <target> <cmd>` | Dispatch a command to a local or virtual device | `cmd send fan_relay_01 TOGGLE` |
| `mock rs485 fault <none\|timeout\|invalid>` | Inject simulated bus faults to test Safe-Default | `mock rs485 fault invalid` |
| `mock storage test` | Write a sample telemetry row to virtual flash storage | `mock storage test` |
| `test freeze rs485` | Simulate frozen RS485 task to test Watchdog panic restart | Reboots device automatically after 15s (`FR-SUP-02`) |
| `test freeze event_bus` | Simulate frozen Event Bus to test Watchdog panic restart | Reboots device automatically after 15s (`FR-SUP-02`) |

---

## 6. Building & Flashing Firmware

### Prerequisites
- Python 3.10+ with `pytest` installed.
- ESP-IDF v5.3.1 configured in your environment (`source $IDF_PATH/export.sh`).

### Build Firmware
```bash
cd firmware
idf.py set-target esp32s3
idf.py build
```

### Flash Firmware
```bash
# Automatic flashing with idf.py:
idf.py -p /dev/ttyACM0 flash

# Or using esptool.py directly at high speed (460800 baud):
esptool.py --chip esp32s3 -p /dev/ttyACM0 -b 460800 --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 40m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x20000 build/smart_clock_gateway.bin
```

---

## 7. Running Verification Test Suites

### 7.1. Automated Host Logic Tests (pytest)
Runs pure algorithmic unit tests instantly on the host PC without hardware:
```bash
pytest tests/host/ -v
```
*Expected Result*: **17 passed in 0.05s** (covers Modbus CRC, Sync Queue packaging, Rule Engine evaluations, MQTT routing, and BLE packet parsing).

### 7.2. Automated Hardware Bare-Board Suite
Runs an automated 8-in-1 battery test suite directly communicating with the physical ESP32-S3 over USB:
```bash
python scripts/test_bareboard_suite.py
```
*Expected Result*: **8/8 PASSED (100%)** — verifies CLI responsiveness, Safe-Default handling, dynamic rule CRUD, local scheduler triggering, virtual storage logging, Wi-Fi scanning, and Task Supervisor hang recovery.

### 7.3. BLE Scanner Target Verification Suite
Runs a 6-in-1 test battery exercising the NimBLE stack and BLE telemetry pipeline:
```bash
python scripts/test_ble_battery.py
```
*Expected Result*: **6/6 PASSED (100%)** — verifies GAP passive scanner state, dynamic sensor registration, telemetry injection, rule triggering, sub-threshold rejection, and Task Supervisor bitmask health (`0xD7`).

---

## 8. Troubleshooting Guide

| Symptom | Likely Cause | Solution |
|---|---|---|
| No serial output after flashing | Incorrect port or baudrate | Verify port using `ls /dev/ttyACM*` or Device Manager; set baudrate to `115200`. |
| Flashing fails with "A fatal error occurred" | Chip not in download mode | Hold the **BOOT** button, press and release **RESET**, then release **BOOT**, and re-run flash command. |
| Board keeps rebooting every 15 seconds | Task hang detection triggered (`FR-SUP-02`) | A critical FreeRTOS task failed to report heartbeat. Check boot log for `CRITICAL: Task 'X' STALLED`. |
| Cannot connect to SoftAP `SmartClock-Setup` | Wi-Fi Station already connected to a known router | The SoftAP only activates when Wi-Fi is unconfigured or disconnected. Run `wifi select` or reset Wi-Fi config. |
| Browser cannot open Web Dashboard | Computer and ESP32 are on different subnets / Wi-Fi networks | Ensure your computer is connected to the same Wi-Fi router as the ESP32 (e.g., `192.168.1.x`). |
| Rule Engine does not activate relay | Value does not cross threshold or telemetry is invalid | Use `rule test sensor_temp_01 <val> 1` to verify threshold. If sensor data is marked `valid=false`, Safe-Default intentionally blocks activation. |
