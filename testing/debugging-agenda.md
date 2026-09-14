# Debugging Agenda — Smart Clock Industrial Edge Gateway

> **Purpose**: Lists every project-side hardware component, interface, and wiring path that has **never been observed working** in this specific setup.
> **Role in AICLP**: Phase 2 (`/commission`) systematically executes and burns this agenda down to empty before functional software testing begins.

---

## 1. Hardware Bringup Checklist

| # | Part / Subsystem | Unproven Because | How It Will Be Proven | Status |
|---|---|---|---|---|
| **DA-01** | **ESP32 DevKit Board** | First flash on this physical hardware | Serial monitor displays boot message and chip revision without panic | `PENDING` |
| **DA-02** | **USB-UART Console (115200 baud)** | Never captured serial data on this workstation port | Terminal opens, receives legible ASCII text, responds to keypress | `PENDING` |
| **DA-03** | **MicroSD Card SPI Slot** | Wiring (GPIO 14, 15, 2, 13) and pull-ups unverified | Driver mounts FATFS volume, reads capacity, writes test file | `PENDING` |
| **DA-04** | **RS485 Transceiver & DE/RE Pin** | UART2 (GPIO 16, 17) + GPIO 4 flow control unverified | Scope / logic analyzer or Modbus slave detects TX frame; RX byte received | `PENDING` |
| **DA-05** | **I2C Bus (OLED & RTC DS3231)** | Shared I2C lines (GPIO 21 SDA, GPIO 22 SCL) unverified | I2C address scanner detects `0x68` (RTC) and `0x3C`/`0x3D` (OLED) | `PENDING` |
| **DA-06** | **Power Supply (12V/24V to 5V Step-Down)** | Buck converter ripple and current capability unproven | Multimeter confirms steady 5.0V ± 0.2V under full Wi-Fi TX load | `PENDING` |

---

## 2. Commissioning Verification Steps

### DA-01 & DA-02: ESP32 Boot & Serial Console
- **Action**: Plug USB cable, flash initial marker firmware.
- **Pass Criteria**: Serial output at 115200 baud displays:
  ```
  [BOOT] Starting Smart Clock Edge Gateway
  [SYS] Alive marker 1
  ```

### DA-03: MicroSD Card Interface
- **Action**: Insert FAT32-formatted MicroSD card containing `config.json`.
- **Pass Criteria**: Serial output displays:
  ```
  [SD] FATFS mounted successfully. Volume size: XXXX MB
  ```

### DA-04: RS485 Transceiver & Flow Control
- **Action**: Attach Modbus RTU Slave sensor to A/B terminal with 120Ω resistor.
- **Pass Criteria**: Polling request sent with GPIO 4 toggle; Slave responds with valid CRC.

### DA-05: I2C Bus Arbitration
- **Action**: Run I2C bus scanner hook on boot.
- **Pass Criteria**: Both address 0x68 (DS3231) and address 0x3C (SSD1327) acknowledged on bus.
