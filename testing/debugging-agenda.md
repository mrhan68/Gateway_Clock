# Debugging Agenda — Smart Clock Industrial Edge Gateway

> **Purpose**: Lists every project-side hardware component, interface, and wiring path that has **never been observed working** in this specific setup.
> **Role in AICLP**: Phase 2 (`/commission`) systematically executes and burns this agenda down before functional software testing begins.

---

## 1. Hardware Bringup Checklist

| # | Part / Subsystem | Unproven Because | How It Was / Will Be Proven | Status |
|---|---|---|---|:---:|
| **DA-01** | **ESP32-S3 DevKit Board** | First flash on physical hardware | Serial monitor displays boot message, chip revision v0.2, and `[SYS] Alive marker 1` | 🟢 `PROVEN` |
| **DA-02** | **USB-UART Console (115200 baud)** | First capture on workstation port (`/dev/ttyACM0`) | Terminal communicates at 115200 baud, executes CLI commands, and receives formatted logs | 🟢 `PROVEN` |
| **DA-03** | **MicroSD Card SPI Slot** | Physical wiring pending hardware assembly | Driver mounts FATFS volume, reads capacity, writes test CSV file (Mock Storage verified) | ⏳ `PENDING` |
| **DA-04** | **RS485 Transceiver & DE/RE Pin** | Wired Modbus RTU interface | Deprecated per FSD §4.10 in favor of wireless BLE; mock RS485 engine preserved for simulation | 🚫 `DEPRECATED` |
| **DA-05** | **I2C Bus (OLED & RTC DS3231)** | Physical screen wiring pending | I2C address scanner detects `0x68` (RTC) and `0x3C` (OLED) (SNTP fallback active) | ⏳ `PENDING` |
| **DA-06** | **Power Supply (5V USB Type-C)** | Power rail stability under radio load | Steady 5.0V operation maintained under concurrent Wi-Fi Station + BLE NimBLE scanning | 🟢 `PROVEN` |
| **DA-07** | **BLE Wireless Subsystem (NimBLE)** | New wireless protocol bringup on ESP32-S3 | NimBLE stack initialized, GAP central passive observer running, 6/6 battery tests passed | 🟢 `PROVEN` |

---

## 2. Commissioning Verification Records

### DA-01 & DA-02: ESP32-S3 Boot & Serial Console (PROVEN)
- **Action**: Plug USB Type-C cable, flash firmware via `esptool.py` at 460800 baud.
- **Result**: ✅ Verified. Serial console output at 115200 baud displays:
  ```text
  [BOOT] Starting Smart Clock Edge Gateway (Target: ESP32-S3)...
  [SYS] Alive marker 1
  [SYS] System fully operational in wireless Smart Home mode (Wi-Fi + BLE).
  ```

### DA-04: RS485 Transceiver Status (DEPRECATED per FSD §4.10)
- **Decision**: Wired Modbus RTU RS485 dropped in favor of wireless BLE Environmental Sensing for smart home deployment.
- **Alternative**: Tầng ngoại vi ảo `mock_peripherals.c` retains full RS485 simulation and fault injection capabilities for automated regression.

### DA-06: Power Supply Stability (PROVEN)
- **Result**: ✅ Verified. Continuous uptime observed over multi-hour runs with concurrent Wi-Fi station connectivity, SNTP synchronization, and continuous BLE passive scanning without brownout resets.

### DA-07: BLE Wireless Subsystem (PROVEN)
- **Action**: Run `python scripts/test_ble_battery.py` over `/dev/ttyACM0`.
- **Result**: ✅ 6/6 tests passed. GAP passive observer actively scans for GATT UUID `0x181A` and BTHome V2 advertisement frames.

### DA-03 & DA-05: MicroSD Card & I2C Display (PENDING Physical Assembly)
- **Status**: The core firmware operates in bare-board mode with full mock drivers (`mock_peripherals.c`) and internal timer/SNTP fallback. Physical bringup will execute when breadboard/PCB hardware is connected.
