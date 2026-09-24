# Functional Specification Document (FSD)
# Smart Clock as Edge IoT Gateway

**Phiên bản:** 1.0  
**Ngày tạo:** 2026-08-14  
**Dự án:** Xây dựng đồng hồ thông minh tích hợp Edge IoT Gateway và hệ thống điều khiển tự động hóa  

---

## Mục lục

- [1. Giới thiệu](#1-giới-thiệu)
- [2. Phạm vi hệ thống](#2-phạm-vi-hệ-thống)
  - [2.1 Các thành phần](#21-các-thành-phần)
  - [2.2 Sơ đồ kết nối mạng không dây (Wi-Fi + BLE)](#22-sơ-đồ-kết-nối-mạng-không-dây-smart-home-wi-fi--ble--cấp-nguồn)
  - [2.3 Bảng phân bổ chân GPIO ESP32-S3](#23-bảng-phân-bổ-chân-gpio-esp32-s3-chống-xung-đột-phần-cứng)
  - [2.4 Phân loại tính năng (MoSCoW)](#24-phân-loại-tính-năng-moscow)
- [3. Kiến trúc tổng thể](#3-kiến-trúc-tổng-thể)
  - [3.1 Kiến trúc firmware Gateway (Phân chia Task & Core FreeRTOS)](#31-kiến-trúc-firmware-gateway-phân-chia-task--core-freertos)
  - [3.2 Sơ đồ luồng dữ liệu tổng thể](#32-sơ-đồ-luồng-dữ-liệu-tổng-thể-store-and-forward-data-flow)
- [4. Đặc tả chức năng — Firmware Gateway](#4-đặc-tả-chức-năng--firmware-gateway)
  - [4.1 Module: Event Bus (`event_bus.h/.c`)](#41-module-event-bus-event_bushc)
  - [4.2 Module: RTC (`rtc.h/.c`)](#42-module-rtc-rtchc)
  - [4.3 Module: Storage & Flash SPIFFS (`storage.h/.c`)](#43-module-storage-storagehc)
  - [4.4 Module: Wi-Fi Provisioning (`wifi_provisioning.h/.c`)](#44-module-wi-fi-provisioning-wifi_provisioninghc)
  - [4.5 Module: BLE Telemetry Adapter (`ble_adapter.h/.c`)](#45-module-ble-telemetry-adapter-ble_adapterhc)
  - [4.6 Module: MQTT Adapter (`mqtt_adapter.h/.c`)](#46-module-mqtt-adapter-mqtt_adapterhc)
  - [4.7 Module: Rule Engine (`rule_engine.h/.c`)](#47-module-rule-engine-rule_enginehc)
  - [4.8 Module: Scheduler (`scheduler.h/.c`)](#48-module-scheduler-schedulerhc)
  - [4.9 Module: Display (`display.h/.c`)](#49-module-display-displayhc)
  - [4.10 Module: Bộ Điều Khiển Relay & Cơ Cấu Chấp Hành (`relay_controller.h/.c`)](#410-module-bộ-điều-khiển-relay--cơ-cấu-chấp-hành-relay_controllerhc)
  - [4.11 Module: SD Card Storage & Store-and-Forward Queue (`sd_logger.h/.c`)](#411-module-sd-card-storage--store-and-forward-queue-sd_loggerhc)
  - [4.12 Module: Config Manager & Cấu hình tại chỗ (`config_manager.h/.c`)](#412-module-config-manager--cấu-hình-tại-chỗ-config_managerhc)
  - [4.13 Module: Task Supervisor & Watchdog (`supervisor.h/.c`)](#413-module-task-supervisor--watchdog-supervisorhc)
  - [4.14 Boot Sequence tổng thể](#414-boot-sequence-tổng-thể-hoàn-chỉnh--chuẩn-hóa)
- [5. Đặc tả chức năng — Backend](#5-đặc-tả-chức-năng--backend)
- [6. Đặc tả chức năng — Web App](#6-đặc-tả-chức-năng--web-app)
- [7. Mô hình dữ liệu](#7-mô-hình-dữ-liệu)
- [8. Đặc tả API Backend](#8-đặc-tả-api-backend)
- [9. Đặc tả giao thức MQTT](#9-đặc-tả-giao-thức-mqtt)
- [10. Use Cases](#10-use-cases)
- [11. Xử lý lỗi & Failure Modes](#11-xử-lý-lỗi--failure-modes)
- [12. Yêu cầu phi chức năng](#12-yêu-cầu-phi-chức-năng)
- [13. Ràng buộc & Giả định](#13-ràng-buộc--giả-định)
- [14. Tiêu chí nghiệm thu](#14-tiêu-chí-nghiệm-thu)
- [15. Phụ lục](#15-phụ-lục)

---

## 1. Giới thiệu

### 1.1 Mục đích tài liệu

Tài liệu này đặc tả đầy đủ các chức năng của hệ thống Smart Clock Gateway, bao gồm firmware ESP32, Backend server, và Web App. Tài liệu phục vụ làm cơ sở để implement, kiểm thử, và đánh giá hệ thống.

### 1.2 Đối tượng đọc

- Nhóm phát triển firmware (ESP-IDF/C)
- Nhóm phát triển Backend/Web App
- Giáo viên hướng dẫn / Hội đồng bảo vệ đồ án

### 1.3 Ý tưởng cốt lõi

Smart Clock không phải là một IoT device đơn lẻ, mà là một **Gateway + Automation Controller** đóng vai trò:

- Kết nối và quản lý nhiều thiết bị IoT qua nhiều giao thức (BLE, MQTT)
- Lưu trữ và thực thi automation/schedule **tại local** (edge computing)
- Chỉ đồng bộ với Cloud khi có Internet — **offline-first**
- Hiển thị thông tin như một đồng hồ thông minh

### 1.4 Thuật ngữ

| Thuật ngữ | Định nghĩa |
|---|---|
| Gateway | Thiết bị Smart Clock, đóng vai trò trung tâm điều khiển |
| Edge Computing | Xử lý logic tại thiết bị cục bộ, không qua cloud |
| Offline-first | Hệ thống hoạt động đầy đủ khi mất mạng, đồng bộ khi có mạng |
| Protocol Adapter | Module chuyển đổi giao thức (BLE ↔ Event Bus, MQTT ↔ Event Bus) |
| Rule Engine | Bộ đánh giá "IF condition THEN action" chạy tại local |
| Safe-default | Giữ trạng thái an toàn cuối cùng khi dữ liệu sensor lỗi |
| Source of Truth | Nguồn dữ liệu chính thức — Backend cho config, Gateway cho event log |
| Telemetry | Dữ liệu đo đạc từ sensor (nhiệt độ, độ ẩm, trạng thái on/off...) |
| LWT | Last Will and Testament — message MQTT tự gửi khi mất kết nối |
| Captive Portal | Trang web tự động mở khi kết nối vào Wi-Fi AP, dùng để cấu hình |

---

## 2. Phạm vi hệ thống

### 2.1 Các thành phần

```
┌────────────────────────────────────────────────────────────────────────────────────────┐
│                                   HỆ THỐNG TỔNG THỂ                                    │
│                                                                                        │
│  ┌──────────────┐          REST API / WS          ┌─────────────────────────────────┐  │
│  │   Web App    │◄───────────────────────────────►│             Backend             │  │
│  │ (React/Vite) │                                 │ (Node.js/Express + Aedes MQTT)  │  │
│  └──────────────┘                                 └────────────────┬────────────────┘  │
│                                                                    │                   │
│                                                       Wi-Fi / MQTT │ Uplink & Commands │
│                                                                    ▼                   │
│                                                   ┌─────────────────────────────────┐  │
│                                                   │    Smart Clock Edge Gateway     │  │
│                                                   │        (ESP32-S3 Firmware)      │  │
│                                                   └────┬───────────┬───────────┬────┘  │
│                                                        │           │           │       │
│                                                        ▼           ▼           ▼       │
│                                                      BLE         Local       Cloud     │
│                                                    Sensors      Relays       MQTT      │
│                                                  (0x181A/BT)   (Ch 1-4)    Actuators   │
│                                                                                        │
│  Lưu trữ & Giám sát tại chỗ:                                                           │
│  - Bộ nhớ Flash / SD: Telemetry CSV log, Event log, RTC Memory Crash Dump, Sync Queue  │
│  - Flash NVS: Wi-Fi credentials, Broker URI, Gateway ID, Timezone (POSIX ICT-7)        │
│  - Task Supervisor: TWDT + Heartbeat bitmask (0x7F) giám sát 7 task FreeRTOS, hang 15s │
│  - Cấu hình tại chỗ: Web Dashboard (Port 80) + Serial CLI tương tác (UART0 115200 baud)│
│  - Broker MQTT: Embedded Aedes (Backend port 1883) hoặc Mosquitto bảo mật TLS (8883)   │
└────────────────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Sơ đồ kết nối mạng không dây Smart Home (Wi-Fi + BLE) & Cấp nguồn

> **Nền tảng vi điều khiển**: Espressif **ESP32-S3** (Dual-Core Xtensa LX7 @ 160MHz, 16MB Quad SPI Flash, 8MB PSRAM).
> **Kiến trúc Vô tuyến (Radio)**: ESP32-S3 sử dụng **chính xác 1 khối Radio RF 2.4 GHz chung** cho cả Wi-Fi 802.11 b/g/n và Bluetooth 5.0 LE. Hoạt động song song được điều phối thông qua cơ chế phân chia thời gian **Software Coexistence** (`CONFIG_SW_COEXIST_ENABLE=y`).

```
                      ┌───────────────────────────────────────────────────────────────┐
                      │ Nguồn điện gia đình: Adapter USB 5V DC @ 2A (Type-C/MicroUSB) │
                      └───────────────────────────────┬───────────────────────────────┘
                                                      │ 5V DC
                                                      ▼
                                       ┌─────────────────────────────┐
                                       │  ESP32 Smart Clock Gateway  │
                                       │ ┌─────────────────────────┐ │
                                       │ │ ESP32-S3 (Wi-Fi + BLE)  │ │
                                       │ ├─────────────────────────┤ │
                                       │ │ SD Card Socket (SPI)    │ │
                                       │ │ OLED SSD1327 (I2C)      │ │
                                       │ │ RTC DS3231 (I2C)        │ │
                                       │ │ Buzzer + Nút bấm        │ │
                                       │ └─────────────────────────┘ │
                                       └──────────────┬──────────────┘
                                                      │
                       ┌──────────────────────────────┴──────────────────────────────┐
                       │ Không dây (Wireless RF 2.4 GHz - Ăng-ten tích hợp)          │
                       ▼                                                             ▼
        ┌─────────────────────────────┐                               ┌─────────────────────────────┐
        │       Kênh Wi-Fi 2.4 GHz    │                               │     Kênh Bluetooth BLE      │
        └──────────────┬──────────────┘                               └──────────────┬──────────────┘
                       │                                                             │
          ┌─────────────┴─────────────┐                                               │
          ▼                           ▼                                               ▼
┌──────────────────┐       ┌──────────────────┐                             ┌──────────────────┐
│ Wi-Fi Router LAN │       │ Thiết bị Wi-Fi   │                             │ Cảm biến BLE T&H │
│ - SoftAP Portal  │       │ - Smart Socket   │                             │ - Đọc Telemetry  │
│ - Web Dashboard  │       │ - Tasmota / Tuya │                             │ - Beacon / Adv   │
│ - MQTT Broker    │       │   qua MQTT       │                             │   (Passive Scan) │
└──────────────────┘       └──────────────────┘                             └──────────────────┘
```

#### Quy tắc mạng không dây:
- **Tập trung 100% không dây & Relay Onboard:** Liên kết giữa Gateway với các cảm biến môi trường qua **Bluetooth Low Energy (BLE)** ở chế độ **Passive Observer** (thu nhận beacon quảng bá, không kết nối GATT, không hỗ trợ BLE Actuator / GATT write). Thiết bị chấp hành ngoại vi được điều khiển qua sóng **Wi-Fi** (giao thức MQTT), trong khi các tải công suất cao chính (Quạt, Sưởi, Hút ẩm, Phụ trợ) được điều khiển trực tiếp qua **4 kênh Relay Onboard (GPIO 15..18)** của Gateway.
- **Không dùng RS485:** Chuẩn truyền thông dây đồng RS485 được lược bỏ để tối ưu chi phí linh kiện, đơn giản hóa mạch và tiết kiệm chân GPIO.
- **Sóng Zigbee (IEEE 802.15.4):** Được bảo lưu trong định hướng mở rộng tương lai (Could Have); giai đoạn hiện tại ưu tiên tối đa cho 2 sóng có sẵn trên chip là **Wi-Fi và BLE**.

### 2.3 Bảng phân bổ chân GPIO ESP32-S3 (Chống xung đột phần cứng)

| Chân GPIO | Chức năng gán | Giao tiếp / Module | Ghi chú thiết kế phần cứng ESP32-S3 |
|---|---|---|---|
| **GPIO 43** | `U0TXD` | USB-UART Console | Cổng UART0 TX nối chip nạp CH343 / Terminal 115200 baud |
| **GPIO 44** | `U0RXD` | USB-UART Console | Cổng UART0 RX nối chip nạp CH343 / Nhận lệnh CLI cứu hộ |
| **GPIO 8** | `I2C_SDA` | I2C0 Bus chung | Chia sẻ cho OLED SSD1327 (`0x3C`) & RTC DS3231 (`0x68`), pull-up 4.7kΩ |
| **GPIO 9** | `I2C_SCL` | I2C0 Bus chung | Clock I2C 400kHz |
| **GPIO 10** | `SPI_CS` | MicroSD Card (SPI) | Chip Select thẻ nhớ SD (tích cực mức LOW) trên `SPI2_HOST` |
| **GPIO 11** | `SPI_MOSI` | MicroSD Card (SPI) | Master Out Slave In trên bus `SPI2_HOST` |
| **GPIO 12** | `SPI_CLK` | MicroSD Card (SPI) | Clock SPI thẻ nhớ MicroSD (20MHz) |
| **GPIO 13** | `SPI_MISO` | MicroSD Card (SPI) | Master In Slave Out thẻ nhớ MicroSD |
| **GPIO 1** | `PWM_BUZZER` | Còi chip Buzzer | Điều khiển âm lượng và tần số qua LEDC PWM (Tự ngắt sau 60s) |
| **GPIO 2** | `STATUS_LED` | LED trạng thái | Đèn nhấp nháy nhịp tim và hiển thị trạng thái kết nối Wi-Fi/MQTT |
| **GPIO 0** | `BTN_BOOT` | Nút bấm 1 (BOOT) | Khi chuông reo: Nhấn nhanh = **SNOOZE (+9 phút)**. Khi bình thường: Giữ ≥5s = Reset Wi-Fi SoftAP |
| **GPIO 4** | `BTN_SCREEN_NEXT` | Nút bấm 2 | Khi chuông reo: Nhấn nhanh = **TẮT CHUÔNG NGAY (<500ms)**. Khi bình thường: Chuyển trang màn hình OLED |
| **GPIO 15** | `RELAY_1` | Relay Quạt (Fan) | Kênh Quạt làm mát (Active HIGH, trở pull-down cứng 10kΩ xuống GND, chạy tối đa 4h) |
| **GPIO 16** | `RELAY_2` | Relay Sưởi (Heater) | Kênh Máy sưởi nhiệt (Active HIGH, trở pull-down cứng 10kΩ, Fail-safe TẮT khi mất sensor, max 30m) |
| **GPIO 17** | `RELAY_3` | Relay Hút ẩm (Dehum) | Kênh Máy hút ẩm không khí (Active HIGH, trở pull-down cứng 10kΩ, chạy tối đa 60 phút) |
| **GPIO 18** | `RELAY_4` | Relay Tải phụ (Aux) | Kênh Đèn / Thiết bị phụ trợ (Active HIGH, trở pull-down cứng 10kΩ) |
| **SiP Package** | `Flash & PSRAM` | SPI nội bộ | 16MB Quad SPI Flash + 8MB Embedded PSRAM tích hợp sẵn trong chip |

> **Quy chuẩn phần cứng & Cơ chế an toàn bắt buộc:**
> - **Trở kéo xuống phần cứng (Hardware Pull-Down 10kΩ):** Mọi chân kích hoạt Relay (GPIO 15, 16, 17, 18) đều được thiết kế trở kéo xuống 10kΩ cố định về GND trên bo mạch điều khiển driver/opto. Khi ESP32-S3 ở trạng thái Reset, Bootloader, Panic hoặc Brownout, các chân GPIO ở trạng thái trở kháng cao (`HI-Z`), trở kéo 10kΩ kéo mức điện áp về GND, bảo đảm 100% rơ-le vật lý luôn ở trạng thái TẮT (OFF) an toàn.
> - **Tuyệt đối không dùng GPIO 22, 23, 24, 25:** Các chân này **không tồn tại** trên package chip ESP32-S3.
> - **Màn hình OLED:** Sử dụng giao tiếp I2C0 (chân GPIO 8/9, địa chỉ `0x3C`). Không dùng bus SPI cho màn hình, giải phóng hoàn toàn bus `SPI2_HOST` cho khe cắm thẻ nhớ MicroSD.

### 2.4 Phân loại tính năng (MoSCoW)

#### Must Have (bắt buộc)

| ID | Tính năng | Thành phần |
|---|---|---|
| M01 | Hiển thị giờ, ngày, nhiệt độ trên OLED | Gateway |
| M02 | Kết nối BLE sensor, đọc telemetry | Gateway |
| M03 | Điều khiển cơ cấu chấp hành Relay nội bộ (Quạt/Sưởi/Hút ẩm/Đèn) | Gateway |
| M04 | Rule Engine: IF condition THEN action | Gateway |
| M05 | Local Scheduler: chạy automation/alarm theo giờ | Gateway |
| M06 | Offline-first: hoạt động khi mất mạng | Gateway |
| M07 | RTC giữ giờ khi mất điện | Gateway |
| M08 | Cloud Sync: đồng bộ config + event log | Gateway + Backend |
| M09 | Wi-Fi Provisioning: SoftAP + Captive Portal | Gateway |
| M10 | REST API quản lý device, schedule, rule | Backend |
| M11 | Web App: CRUD device, schedule, xem trạng thái | Web App |
| M12 | Personal schedule/alarm: CRUD trên Web, kêu chuông tại Gateway | Gateway + Web App |
| M13 | Nút tắt/snooze báo thức tại đồng hồ | Gateway |
| **M14** | **BLE Environmental Sensing & Telemetry Ingestion (BTHome / 0x181A)** | **Gateway** |
| **M15** | **SD Card Logger: Lưu dữ liệu cục bộ, Store-and-Forward Sync Queue, crash log** | **Gateway** |
| **M16** | **Config Manager: Lưu cấu hình node/topic; hỗ trợ cấu hình tại chỗ qua SD/CLI** | **Gateway** |
| **M17** | **Task Supervisor: Task Watchdog (TWDT) giám sát timeout & sức khỏe từng task** | **Gateway** |

#### Should Have (nên có)

| ID | Tính năng | Thành phần |
|---|---|---|
| S01 | MQTT over TLS | Gateway |
| S02 | Authentication (JWT/API Key) | Backend + Web App |
| S03 | OTA firmware update | Gateway + Backend |
| S04 | BLE auto-discovery & Passive Beacon Scanning (Mijia/Tuya) | Gateway |
| S05 | Event logging + xem lịch sử trên Web | Backend + Web App |
| S06 | Luân phiên nhiều màn hình OLED | Gateway |
| **S07** | **BLE Sensor Dynamic Registration qua Serial CLI & Web** | **Gateway** |
| **S08** | **Cảnh báo mất kết nối cảm biến không dây BLE lên màn hình OLED & Buzzer** | **Gateway** |

#### Could Have (có thể có)

| ID | Tính năng | Thành phần |
|---|---|---|
| C01 | Matter / Google Home | Gateway |
| C02 | Zigbee protocol (Định hướng mở rộng tương lai qua module Co-processor) | Gateway |
| C03 | Multi-gateway | Backend |
| C04 | Voice control | Gateway |
| C05 | AI/ML tại Edge | Gateway |
| C06 | NVS Encryption (Wi-Fi password) | Gateway |

---

## 3. Kiến trúc tổng thể

### 3.1 Kiến trúc firmware Gateway (Phân chia Task & Core FreeRTOS)

```
┌─────────────────────────────────────── ESP32-S3 ──────────────────────────────────────┐
│                                                                                       │
│   Core 0 (Real-Time, Wireless BLE, Actuators & Giám sát hệ thống)                     │
│   ┌─────────────────────────────────────────────────────────────────────────┐         │
│   │  supervisor_task       (Priority 7) - Task Watchdog (TWDT 15s) & Heartbeat│        │
│   │  event_bus dispatcher  (Priority 6) - Điều phối Queue sự kiện trung tâm │         │
│   │  ble_scan_task         (Priority 5) - Quét thụ động BLE Sensor (0x181A) │         │
│   │  scheduler_task        (Priority 4) - Báo thức & Lịch tự động (RTC)     │         │
│   │  display_task          (Priority 3) - Cập nhật màn hình OLED SSD1327    │         │
│   │  relay_controller      (Điều khiển 4 Relay GPIO 15..18, Interlock, Timer)│        │
│   │  rule_engine           (Đánh giá Hysteresis, chạy trong ngữ cảnh Event) │         │
│   └─────────────────────────────────────────────────────────────────────────┘         │
│                                                                                       │
│   Core 1 (I/O Mạng Wi-Fi, Lưu trữ SD Card & Cấu hình tại chỗ)                        │
│   ┌─────────────────────────────────────────────────────────────────────────┐         │
│   │  uplink_task           (Priority 5) - Quản lý Wi-Fi, MQTT, Backfill & LWT│        │
│   │  sd_log_task           (Priority 4) - Ghi FATFS: Log, Telemetry, Queue  │         │
│   │  serial_cli_task       (Priority 2) - Giao diện lệnh cứu hộ UART0       │         │
│   │  wifi_provisioning / web_server (Priority 2) - SoftAP Portal & Dashboard│         │
│   └─────────────────────────────────────────────────────────────────────────┘         │
│                                                                                       │
│   ┌─────────────────────────────────────────────────────────────────────────┐         │
│   │                    HỆ THỐNG QUEUE ĐỆM & EVENT BUS                       │         │
│   │  - Event Bus Queue: 64 slots (phân phối sự kiện toàn hệ thống)          │         │
│   │  - Config Worker Queue: 10 slots (hàng đợi ghi Flash LittleFS phi nghẽn)│         │
│   │  - SD Logger Queue: 30 slots (hàng đợi ghi file tránh block I/O)        │         │
│   │  - Uplink Outbox Queue: 20 slots (hàng đợi gửi MQTT khi online)         │         │
│   └─────────────────────────────────────────────────────────────────────────┘         │
│                                                                                       │
│   Phần cứng kết nối ESP32-S3:                                                         │
│   - I2C0 (GPIO 8, 9): Màn hình OLED SSD1327 (0x3C) + RTC ngoại vi DS3231 (0x68)     │
│   - SPI2 (GPIO 11, 12, 13, 10): MicroSD Card Socket (SPI Mode, CS = GPIO 10, FATFS)  │
│   - UART0 (GPIO 43, 44): CH343 USB-UART Serial Console & CLI cứu hộ (115200 baud)    │
│   - GPIO 1: Còi chip Buzzer (LEDC PWM báo thức)                                      │
│   - GPIO 2: LED trạng thái kết nối Wi-Fi / MQTT                                       │
│   - GPIO 0: Nút bấm 1 (BOOT) - Nhấn: Snooze (+9m); Giữ ≥5s: Reset Wi-Fi SoftAP        │
│   - GPIO 4: Nút bấm 2 - Nhấn: TẮT CHUÔNG (<500ms) / Chuyển trang OLED                │
│   - GPIO 15, 16, 17, 18: 4 Kênh Relay (Fan, Heater, Dehumidifier, Aux)               │
│   - Không dây: Wi-Fi 2.4 GHz + BLE 5.0 chia sẻ chung vô tuyến qua Software Coexistence│
└───────────────────────────────────────────────────────────────────────────────────────┘
```

> **Ghi chú phân tách Task & Quản lý I/O:**
> - `ble_scan_task` chạy trên Core 0 với priority cao (5) để đảm bảo chu kỳ quét passive beacon và nhận diện cảm biến nhiệt ẩm môi trường ổn định. Task này **hoàn toàn độc lập** với Wi-Fi/Internet; dù mạng mất, BLE vẫn đọc cảm biến và cấp dữ liệu cho Rule Engine đều đặn.
> - Thao tác ghi thẻ nhớ SD (`sd_log_task`) được cô lập sang Core 1. Do tốc độ ghi thẻ SD có thể trễ đột ngột tới 100–300ms khi controller của thẻ bận làm Garbage Collection hoặc flush block, việc tách riêng task kèm `sd_log_queue` giúp các task real-time (BLE, Display, Rule Engine) không bao giờ bị nghẽn (non-blocking).

### 3.2 Sơ đồ luồng dữ liệu tổng thể (Store-and-Forward Data Flow)

```
                    event_bus.h
               (shared structs + API)
                        │
           ┌────────────┼────────────────────────┐
           ▼            ▼                        ▼
        main.c ──► Tất cả module             Shared payload:
                   đều depend vào              - ble_telemetry_t
                   event_bus.h                 - mqtt_telemetry_t
                                               - rule_action_request_t
                                               - config_updated_t

Luồng dữ liệu chính:

  Luồng 1 — Telemetry không dây (BLE & Wi-Fi MQTT) → Rule Engine, OLED, SD & Cloud:
  BLE Sensor ──► ble_adapter ─[EVT_BLE_DEVICE_DATA]──────┬──► rule_engine (đánh giá IF-THEN)
  Wi-Fi Device ─► mqtt_adapter [EVT_MQTT_MESSAGE_RECV]───┼──► display (hiển thị thông số OLED)
                                                         ├──► sd_log_task (ghi CSV thẻ nhớ)
                                                         └──► uplink_task (MQTT publish lên Cloud)
                                                                    │
                                                          (Nếu mất mạng / offline)
                                                                    ▼
                                                          sd_log_task (đẩy vào FIFO queue SD)

  Luồng 2 — Điều khiển từ xa (Downlink Command) & Local Rule Action:
  Server ──(MQTT Topic: gateway/{id}/device/{did}/command)──► uplink_task
                                         │
                          [EVT_DEVICE_COMMAND_REQUEST]
                                         │
        ┌────────────────────────────────┼────────────────────────────────┐
        ▼                                ▼                                ▼
   local_relays                     mqtt_adapter                        display
 (Relay 1-4 GPIO)               (forward Wi-Fi devices)             (cập nhật status)
        │                                │                                │
  Bật/tắt cuộn hút               MQTT Publish                      Hiển thị màn hình
        │                                │
        └──────────────┬─────────────────┘
                       ▼
             [EVT_COMMAND_RESPONSE] ──► uplink_task (phản hồi ACK về topic gateway/{gateway_id}/device/{device_id}/command/ack)
                                    ──► sd_log_task (ghi audit log)

  Luồng 3 — Khôi phục kết nối & Đồng bộ bù (Backfill):
  Internet khôi phục ──► uplink_task kết nối MQTT thành công
                               │
                [Bắt đầu tiến trình gửi bù]
                               │
                               ▼
  uplink_task ◄── Đọc từng batch (20 records) ─── sd_log_task (đọc từ /data/sync_queue.bin)
       │
  Gộp 20 bản ghi/batch gửi qua MQTT QoS 1 (giãn cách ≥ 100ms, duy trì 20–50 records/s), Backend dedupe theo (device_id, timestamp)
       │
  Nhận PUBACK từ Broker ──► sd_log_task cập nhật con trỏ read_ptr lên SD

  Luồng 4 — Giám sát & An toàn (Supervisor & Watchdog):
  Mỗi task FreeRTOS ────(Heartbeat định kỳ ≤ 5s)────► supervisor_task
                                                             │
                                                  (Nếu 1 task treo > 15s)
                                                             ▼
                                                    Ghi crash log ra SD
                                                    Kích hoạt ESP_RESTART
```

### 3.3 Nguyên tắc kiến trúc bắt buộc

1. **Event bus cho xuyên module**: Mọi giao tiếp A → B (A ≠ B) phải đi qua event bus. Không gọi hàm trực tiếp giữa hai module khác nhau. Toàn bộ thông điệp nhật ký sự kiện, trạng thái báo thức và kết quả thực thi lệnh đều phải đóng gói thành sự kiện tiêu chuẩn đưa lên Event Bus.

2. **Dispatcher owns free()**: Producer malloc, handler chỉ đọc, dispatcher free sau khi gọi hết handler. Không có ngoại lệ.

3. **Inversion of Control**: Module không biết module khác tồn tại. Ví dụ: `storage.c` post `EVT_CONFIG_UPDATED_FROM_CLOUD`, không gọi `rule_engine_reload_rules()`.

4. **Gọi trực tiếp cho nội bộ module**: `display_task` gọi `display_render()` trực tiếp, không post event lên bus.

5. **Handler không được block dispatcher**: Mọi handler chạy trong context của `event_dispatcher_task` (tuần tự, 1 task duy nhất). Nếu handler gọi API blocking (kết nối mạng, ghi Flash, I/O thẻ SD, TCP write...), **toàn bộ event bus bị đứng** — kể cả `EVT_BUTTON_ALARM_STOP`. Do đó: handler chỉ được sao chép dữ liệu nhỏ vào queue riêng của worker task (vd: SD log queue, Storage config queue), task riêng thực hiện I/O thực tế. Xem §4.1.6 và §4.3.5 cho chi tiết.

### 3.4 Ghi chú thiết kế: Message Router

Tài liệu phân tích (mục 5.2) liệt kê "Message Router" là thành phần riêng biệt trong Gateway. Trong thiết kế chi tiết, chức năng Message Router được **hiện thực hóa bằng tổ hợp hai cơ chế**:

1. **Event Bus** đóng vai trò routing backbone — phân phối event tới tất cả handler đã đăng ký.
2. **Pattern `is_my_device()`** tại mỗi Protocol Adapter — mỗi adapter tự quyết định nhận hay bỏ qua lệnh dựa trên danh sách thiết bị của mình.

Đây là quyết định thiết kế có chủ đích: thay vì tạo một module "router" trung tâm phải biết device nào thuộc giao thức nào (tạo coupling), kiến trúc hiện tại để mỗi adapter tự quyết — đúng nguyên tắc Inversion of Control. Kết quả là **không có file `message_router.c`** — chức năng routing được phân tán vào event bus + adapter, không mất đi.

> **Mapping:** Tài liệu phân tích §5.2 "Message Router" = FSD §3.2 "Event Bus" + §4.5/4.6 "`is_my_device()` pattern".

---

## 4. Đặc tả chức năng — Firmware Gateway

### 4.1 Module: Event Bus (`event_bus.h/.c`)

#### 4.1.1 Mô tả
Xương sống giao tiếp của toàn bộ Gateway. Cung cấp hàng đợi sự kiện trung tâm và cơ chế đăng ký handler theo event code.

#### 4.1.2 Cấu hình

| Tham số | Giá trị | Ghi chú |
|---|---|---|
| Queue length | 64 slots | Mở rộng đệm chống tràn khi có bão quảng bá BLE (BLE storm) |
| Max handlers/event | 4 | Đủ cho quy mô đồ án |
| Dispatcher priority | 6 | Cao nhất trong nhóm real-time |
| Dispatcher core | Core 0 | Nhóm real-time |
| Dispatcher stack | 4096 bytes | |

#### 4.1.3 API

```c
void event_bus_init(void);
// Precondition: Gọi 1 lần đầu tiên trong app_main()
// Postcondition: Queue được tạo, bảng handler rỗng

void event_bus_start_dispatcher(void);
// Precondition: Tất cả module đã register_handler xong
// Postcondition: Task dispatcher bắt đầu đọc queue
// QUAN TRỌNG: Phải gọi SAU CÙNG trong boot sequence

void event_bus_register_handler(gateway_event_code_t code,
                                gateway_event_handler_t handler);
// Precondition: event_bus_init() đã gọi
// Postcondition: handler được thêm vào slot trống cho event code
// Lỗi: Nếu đủ 4 handler → log WARN, bỏ qua

BaseType_t event_bus_post(gateway_event_code_t code, void *data,
                           size_t len, bool from_isr);
// Precondition: data đã malloc (hoặc NULL nếu không cần payload)
// Postcondition: Event được đẩy vào queue
// from_isr=true: sử dụng xQueueSendFromISR (cho GPIO ISR)
// Timeout: 100ms (non-ISR), nếu queue đầy → log WARN, trả pdFALSE
// QUAN TRỌNG: Sau khi post, producer KHÔNG được truy cập data nữa
```

#### 4.1.4 Event Codes đầy đủ

| Code | Producer | Consumer(s) | Payload |
|---|---|---|---|
| `EVT_WIFI_PROVISIONING_STARTED` | wifi_provisioning | display | NULL |
| `EVT_WIFI_CREDENTIALS_RECEIVED` | wifi_provisioning | rtc | NULL |
| `EVT_WIFI_CONNECTED` | wifi_provisioning | rtc, storage, display, uplink_task | NULL |
| `EVT_WIFI_LOST` | wifi_provisioning | display, uplink_task | NULL |
| `EVT_BLE_DEVICE_DATA` | ble_adapter | rule_engine, mqtt_adapter, display, sd_log_task | `ble_telemetry_t*` |
| `EVT_DEVICE_OFFLINE_ALERT` | ble_adapter, uplink_task | rule_engine, relay_controller, display, sd_log_task, uplink_task | `device_alert_t*` |
| `EVT_SD_STATUS_CHANGED` | sd_logger | display, supervisor | `sd_status_t*` |
| `EVT_CONFIG_RELOAD_REQUEST` | serial_cli, config_mgr | uplink_task, rule_engine, ble_adapter | NULL |
| `EVT_MQTT_MESSAGE_RECEIVED` | mqtt_adapter | rule_engine, display | `mqtt_telemetry_t*` |
| `EVT_MQTT_CONNECTED` | mqtt_adapter | storage, display, sd_log_task | NULL |
| `EVT_MQTT_DISCONNECTED` | mqtt_adapter | display, sd_log_task | NULL |
| `EVT_DEVICE_COMMAND_REQUEST` | mqtt_adapter, serial_cli | relay_controller | `device_command_req_t*` |
| `EVT_RULE_ACTION_REQUEST` | rule_engine, scheduler | relay_controller, display | `rule_action_request_t*` |
| `EVT_COMMAND_RESPONSE` | relay_controller | uplink_task, sd_log_task | `command_ack_t*` |
| `EVT_RELAY_STATE_CHANGED` | relay_controller | rule_engine, display, sd_log_task | `relay_state_changed_t*` |
| `EVT_SCHEDULE_AUTOMATION_TRIGGERED` | scheduler | rule_engine, relay_controller | `automation_rule_t*` |
| `EVT_SCHEDULE_ALARM_TRIGGERED` | scheduler | display | `alarm_entry_t*` |
| `EVT_SENSOR_VALUE_INVALID` | rule_engine, ble_adapter | relay_controller, display, sd_log_task | NULL |
| `EVT_BUTTON_ALARM_STOP` | GPIO ISR / display_task | display, sd_log_task | NULL |
| `EVT_BUTTON_SCREEN_NEXT` | GPIO ISR / display_task | display | NULL |
| `EVT_DISPLAY_UPDATE_REQUEST` | (bất kỳ module) | display | NULL |
| `EVT_CONFIG_UPDATED_FROM_CLOUD` | storage | rule_engine, scheduler, ble_adapter | `config_updated_t*` |
| `EVT_EVENT_LOG_APPENDED` | storage, sd_logger | (cloud sync trigger) | NULL |
| `EVT_SYNC_COMPLETED` | storage, sd_logger | display | NULL |

> **Phân biệt luồng lệnh điều khiển thiết bị & Fail-Safe Routing:**
> - `EVT_RULE_ACTION_REQUEST`: Do **Rule Engine** hoặc **Scheduler** sinh ra tự động từ các quy tắc cục bộ.
> - `EVT_DEVICE_COMMAND_REQUEST`: Do **MQTT Adapter** (từ Web App) hoặc **Serial CLI** sinh ra từ thao tác thủ công của người dùng.
> - Cả hai luồng điều khiển rơ-le onboard đều được điều hướng tập trung vào **`relay_controller`** để thẩm định khóa liên động an toàn (Interlock), xuất logic GPIO và phát sinh **`EVT_COMMAND_RESPONSE`** phản hồi ACK lên Cloud.
> - **Fail-Safe Alert:** Khi nhận `EVT_DEVICE_OFFLINE_ALERT`, cả `rule_engine` và `relay_controller` đều thẩm tra sensor ID liên quan; nếu gắn với Máy sưởi (`relay_2`), lệnh ngắt khẩn cấp (0ms trễ) được thực thi ngay lập tức.

#### 4.1.5 Shared Payload Structs

```c
typedef enum {
    ALERT_SENSOR_OFFLINE   = 1,
    ALERT_SENSOR_CORRUPTED = 2,
    ALERT_SAFETY_CUTOFF    = 3
} alert_type_t;

typedef struct {
    char         device_id[32];     // Cảm biến mất tín hiệu hoặc lỗi (VD: "sensor_temp_01")
    alert_type_t alert_type;        // ALERT_SENSOR_OFFLINE
    uint32_t     timestamp;         // Wall-clock UNIX epoch
    int64_t      monotonic_time_us; // esp_timer_get_time() tại thời điểm phát hiện
} device_alert_t;

typedef struct {
    char     device_id[32]; // Mã thiết bị (VD: "sensor_temp_01")
    float    temperature;   // Nhiệt độ (°C)
    float    humidity;      // Độ ẩm (%RH)
    float    battery_pct;   // Mức pin (0 - 100%)
    bool     valid;         // false -> rule engine PHẢI áp dụng safe-default
    uint32_t timestamp;     // Thời gian đọc (UNIX epoch từ RTC)
} ble_telemetry_t;

typedef struct {
    int      relay_id;             // 1: Fan, 2: Heater, 3: Dehumidifier, 4: Aux
    char     device_id[32];        // "relay_1", "relay_2", ...
    bool     state;                // true: ON, false: OFF
    char     reason[32];           // "MANUAL_CMD", "RULE_TRIGGER", "SAFETY_CUTOFF", "SENSOR_OFFLINE", "COOLING_OVERRIDE"
    uint32_t timestamp;            // Epoch
    int64_t  monotonic_time_us;    // esp_timer_get_time() thời điểm chuyển mạch
} relay_state_changed_t;

typedef struct {
    char     command_id[36];       // UUID v4 định danh lệnh (chống trùng lặp & đối soát ACK)
    char     target_device_id[32]; // Thiết bị nhận lệnh ("relay_1".."relay_4")
    char     action[32];           // "ON", "OFF", "SET:75", "TOGGLE"
    uint32_t ttl_seconds;          // Thời hạn hiệu lực của lệnh (mặc định 60s)
    uint32_t timestamp;            // Epoch thời điểm phát lệnh
} device_command_req_t;

typedef struct {
    char     command_id[36];       // Khớp command_id của yêu cầu gửi xuống
    char     target_device_id[32]; // Thiết bị thực thi
    bool     success;              // true nếu thực thi thành công
    int      error_code;           // 0: OK, 1: TIMEOUT, 2: INVALID_CMD, 3: INTERLOCK_CONFLICT, 4: DWELL_LOCKED, 5: LOCKOUT_ACTIVE
    char     message[64];          // Mô tả kết quả thực thi
    uint32_t executed_at;          // Thời điểm thực thi xong
} command_ack_t;

typedef struct {
    char     command_id[36];       // Tự sinh UUID cho action do Rule kích hoạt
    char     target_device_id[32]; // Relay đích ("relay_1", "relay_2", ...)
    char     command[32];          // VD: "ON", "OFF"
    uint32_t timestamp;            // Epoch kích hoạt
} rule_action_request_t;

typedef enum {
    CONFIG_TYPE_NONE     = 0,
    CONFIG_TYPE_SCHEDULE = 1 << 0, // schedule.json đã được cập nhật
    CONFIG_TYPE_RULES    = 1 << 1, // rules.json đã được cập nhật
    CONFIG_TYPE_DEVICES  = 1 << 2  // device_list.json đã được cập nhật (whitelist sensor BLE)
} config_type_flags_t;

typedef struct {
    config_type_flags_t flags;     // Mặt nạ loại cấu hình được cập nhật
    uint32_t            version;   // Version / checksum cấu hình mới
} config_updated_t;
```

#### 4.1.6 Quy tắc bộ nhớ & Nguyên Tắc Không Gây Nghẽn Dispatcher (Non-Blocking Dispatcher)

1. **Nguyên tắc bất biến của Dispatcher:** Dispatcher handler **tuyệt đối không được thực hiện I/O blocking**, không gọi kết nối mạng (SNTP, HTTP), không ghi Flash/SD Card, và không thực hiện parse JSON cJSON phức tạp. Handler chỉ được phép sao chép dữ liệu nhỏ vào task queue của worker tương ứng hoặc bật cờ sự kiện.
2. **Quy tắc giải phóng bộ nhớ (Memory Ownership):**
   - **Cấp phát:** Module phát sự kiện (Producer) chịu trách nhiệm cấp phát động bộ nhớ payload qua `malloc()` nếu có dữ liệu gửi kèm (hoặc truyền `data = NULL` nếu không có payload).
   - **Giải phóng duy nhất:** Dispatcher task sau khi gọi lần lượt tất cả handler đã đăng ký cho event code đó sẽ gọi `free(evt.data)` một lần duy nhất tại điểm kết thúc vòng lặp. Các handler nhận event tuyệt đối **không được gọi free()** trên con trỏ nhận được.
   - **Xử lý chống tràn & rò rỉ khi Queue đầy:** Nếu hàng đợi Event Bus (64 slots) bị đầy dẫn đến `event_bus_post()` thất bại (trả về `pdFALSE`), chính hàm `event_bus_post()` có nghĩa vụ giải phóng `free(data)` ngay lập tức và ghi log cảnh báo `ESP_LOGW("Event queue full, payload freed")` để loại trừ hoàn toàn nguy cơ rò rỉ bộ nhớ (Memory Leak).
3. **Cơ chế xử lý nút bấm vật lý (Chống nghẽn Queue):**
   - Để tránh rủi ro mất sự kiện khi hàng đợi Event Bus bị đầy hoặc nghẽn, tín hiệu ngắt nút bấm vật lý (GPIO 0 `BTN_BOOT_ALARM` và GPIO 4 `BTN_SCREEN_NEXT`) sử dụng cơ chế **Direct-to-Task Notification** (`vTaskNotifyGiveFromISR`) gửi thẳng tới `display_task`.
   - `display_task` phản hồi ngay lập tức (Snooze +9m, Tắt chuông hoặc đổi trang màn hình OLED) mà không cần đợi luân chuyển qua Event Queue. Sau đó, `display_task` mới post sự kiện nhật ký lên Event Bus ở ngữ cảnh task thông thường (`from_isr = false`).
4. **An toàn đa luồng (Mutex Protection):**
   - Bảng lịch biểu `entries[]` của Scheduler được bảo vệ bởi `s_scheduler_mutex`.
   - Bảng đăng ký thiết bị `s_known_devices` được bảo vệ bởi `s_device_mutex`.
   - Các thao tác đọc/ghi file nhật ký hàng đợi ngoại tuyến `sync_queue.bin` trên MicroSD được bảo vệ bởi `s_storage_mutex`.

```
Vòng đời payload:

  Producer              Dispatcher              Handler A    Handler B
  ────────              ──────────              ─────────    ─────────
  malloc(data)
  data->field = val
  post(data)  ────►     xQueueReceive(&evt)
                        call handler_A(&evt) ──► đọc data   (KHÔNG free)
                        call handler_B(&evt) ──►             đọc data (KHÔNG free)
                        free(evt.data)  ◄── Điểm free DUY NHẤT
```

#### 4.1.7 Verification Contract — Event Bus

**Verification Contract — FR-EVT-01: Chống nghẽn hàng đợi, Thu hồi bộ nhớ & Ràng buộc kiểu dữ liệu Host/Target**
- **Tier:** host
- **Preconditions:** Khởi tạo Event Bus với 64 slots; đăng ký 3 handler cho `EVT_BLE_DEVICE_DATA`; các header kích hoạt kiểm tra tĩnh `_Static_assert` căn gióng bộ nhớ.
- **Stimulus:**
  1. Biên dịch hệ thống kiểm thử trên host x86_64 GCC: Xác thực các ràng buộc `_Static_assert(sizeof(device_alert_t) > 0, ...)` và kích thước payload nhị phân.
  2. Bắn liên tục 128 sự kiện có payload `malloc()` vào Event Bus với tốc độ 1000 events/giây; trong đó 64 sự kiện đầu được nhận, 64 sự kiện sau bị tràn hàng đợi (queue full).
- **Expected:**
  1. Quá trình biên dịch thành công 100%, không có lỗi mismatch struct alignment/padding.
  2. 64 sự kiện đầu tiên được Dispatcher phân phối đầy đủ đến 3 handler và `free()` an toàn 100%.
  3. 64 sự kiện sau khi queue đầy bị từ chối (`pdFALSE`), hàm `event_bus_post()` tự động giải phóng payload ngay lập tức.
  4. Kiểm tra rò rỉ bộ nhớ (Memory Leak Check): Heap free trước và sau bài test chênh lệch 0 bytes.
- **Timing / tolerance:** Thời gian xử lý trung bình mỗi event của Dispatcher ≤ 0.5ms.
- **Prohibited:** Handler thực hiện blocking I/O gây kẹt Dispatcher; payload bị rò rỉ khi queue đầy; double-free; sai khác struct size giữa host và target.
- **Operator instructions:**
  1. Chạy test tự động: `pytest tests/host/test_event_bus.py -v`.
  2. Xác nhận 100% assertions PASS và không có log `Memory leak detected`.

---

### 4.2 Module: RTC (`rtc.h/.c`)

#### 4.2.1 Mô tả
Nguồn giờ duy nhất của hệ thống. Sử dụng chip DS3231 (I2C) có pin backup để giữ giờ khi mất điện — điều kiện tiên quyết cho offline-first.

#### 4.2.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| RTC-01 | Đọc giờ từ DS3231 khi boot | `rtc_init()` đọc I2C → `settimeofday()` |
| RTC-02 | Cung cấp API giờ thống nhất | `rtc_get_time()` — toàn hệ thống chỉ gọi hàm này |
| RTC-03 | Đồng bộ NTP khi có Wi-Fi | Handler `EVT_WIFI_CONNECTED` → `esp_sntp` → ghi lại DS3231 |
| RTC-04 | Set timezone từ captive portal | `rtc_set_timezone(tz)` → `setenv("TZ", tz, 1); tzset()` |

#### 4.2.2.1 Bảng Ánh Xạ Múi Giờ IANA (Web App / Browser) sang POSIX (ESP-IDF)

Trình duyệt Web và Captive Portal trích xuất múi giờ người dùng qua JavaScript `Intl.DateTimeFormat().resolvedOptions().timeZone` trả về định dạng chuỗi IANA (vd: `Asia/Ho_Chi_Minh`). Trong khi đó, thư viện C chuẩn Newlib trong ESP-IDF bắt buộc chuỗi định dạng POSIX chuẩn (vd: `ICT-7`). Gateway thực hiện ánh xạ trước khi lưu vào NVS:

| Chuỗi IANA từ Trình Duyệt / Portal | Chuỗi POSIX lưu NVS & nạp `setenv("TZ")` | Chênh lệch UTC |
|---|---|:---:|
| `Asia/Ho_Chi_Minh`, `Asia/Bangkok`, `Asia/Hanoi` | `"ICT-7"` | UTC+7 |
| `Asia/Jakarta` | `"WIB-7"` | UTC+7 |
| `Asia/Singapore`, `Asia/Kuala_Lumpur` | `"SGT-8"` | UTC+8 |
| `Asia/Tokyo` | `"JST-9"` | UTC+9 |
| `Europe/London` | `"GMT0BST,M3.5.0/1,M10.5.0"` | UTC+0 / DST |
| `America/New_York` | `"EST5EDT,M3.2.0,M11.1.0"` | UTC-5 / DST |
| `UTC` | `"UTC0"` | UTC+0 |

```c
void app_rtc_set_timezone(const char *tz_string) {
    if (!tz_string || strlen(tz_string) == 0) return;
    if (strstr(tz_string, "Ho_Chi_Minh") || strstr(tz_string, "Bangkok") || strstr(tz_string, "Hanoi")) {
        setenv("TZ", "ICT-7", 1);
    } else {
        setenv("TZ", tz_string, 1);
    }
    tzset();
    ESP_LOGI(TAG, "System Timezone set to '%s'", getenv("TZ"));
}
```

#### 4.2.3 API

```c
void rtc_init(void);
// Hành vi: Đọc DS3231 qua I2C → settimeofday()
//          Register handler EVT_WIFI_CONNECTED
// Lỗi: Nếu I2C fail → log ERROR, dùng epoch 0 (giờ sẽ sai cho tới khi có NTP)

time_t rtc_get_time(void);
// Hành vi: Trả về giờ hệ thống (đã được set từ DS3231 hoặc NTP)
// QUAN TRỌNG: Toàn bộ code PHẢI gọi hàm này, KHÔNG gọi time(NULL) trực tiếp

bool rtc_sync_from_ntp_and_save(void);
// Hành vi: esp_sntp đồng bộ (chế độ Smooth Slew) → ghi giờ mới vào DS3231
// Trả về: true nếu đồng bộ thành công
// Gọi khi: EVT_WIFI_CONNECTED handler

bool rtc_is_valid(void);
// Hành vi: Kiểm tra năm hiện tại có >= 2020 không (xác nhận giờ đã được set từ DS3231 hoặc NTP).
// Trả về: true nếu giờ hợp lệ; false nếu đang ở Epoch 1970 (chưa có giờ tin cậy).

void rtc_set_timezone(const char *tz_string);
// Hành vi: setenv("TZ", tz_string, 1); tzset();
// Input ví dụ: "ICT-7"
// Gọi khi: Nhận timezone từ captive portal form
```

#### 4.2.4 Hardware Interface

| Tham số | Giá trị |
|---|---|
| Chip | DS3231 |
| Giao tiếp | I2C0 |
| SDA pin | GPIO 8 (pull-up 4.7kΩ) |
| SCL pin | GPIO 9 (pull-up 4.7kΩ) |
| I2C address | 0x68 |
| Clock speed | 400 kHz |
| Pin backup | CR2032 (giữ giờ ~8 năm khi mất điện chính) |

#### 4.2.5 Ràng buộc & Cơ chế Đồng Bộ Thời Gian Hybrid (Step vs Smooth Slew)

- **Mọi module** đọc thời gian dân dụng (Wall-clock Epoch) bắt buộc gọi `rtc_get_time()`, tuyệt đối không bao giờ gọi trực tiếp `time(NULL)`.
- **Bảo vệ báo thức khi chưa có giờ (RTC Valid Guard):** Scheduler bắt buộc kiểm tra `rtc_is_valid()`. Nếu `false` (năm hệ thống < 2020) $\rightarrow$ tạm dừng kích hoạt các lịch hẹn/báo thức theo giờ để chống reo nhầm chuông lúc boot khi chưa có giờ mạng tin cậy. (Lưu ý: Các luật tự động của Rule Engine theo ngưỡng cảm biến nhiệt/ẩm vẫn hoạt động 100% độc lập với RTC).
- **Cơ chế Đồng bộ Giờ Hybrid (Step Adjustment vs Smooth Slew):**
  1. **Bước nhảy tức thời (Step Adjustment via `settimeofday()`):** Áp dụng khi độ lệch thời gian giữa RTC và NTP $|\Delta t| > 15\text{ phút}$ HOẶC khi hệ thống vừa khởi động mà `rtc_is_valid() == false`. Giúp hệ thống bắt kịp giờ chuẩn Internet ngay lập tức thay vì phải chờ hàng tuần nếu chỉ dùng slew.
  2. **Trượt thời gian êm dịu (Smooth Slew via `adjtime()` / `SNTP_SYNC_MODE_SMOOTH`):** Áp dụng khi độ lệch thời gian $|\Delta t| \le 15\text{ phút}$. Tốc độ bù trôi tối đa $\pm 0.5\text{ms/s}$, bảo đảm thời gian trôi tịnh tiến liên tục, triệt tiêu nguy cơ nhảy lùi thời gian (Time Jump Backwards) làm đảo lộn chu kỳ log hoặc kích hoạt lại báo thức đã tắt.
- **Chu kỳ đồng bộ NTP:** Thực hiện 1 lần ngay sau khi kết nối Wi-Fi thành công (`EVT_WIFI_CONNECTED`), sau đó định kỳ kiểm tra mỗi 6 giờ 1 lần. Mỗi lần đồng bộ thành công đều cập nhật lại thanh ghi phần cứng của chip ngoại vi DS3231.

#### 4.2.6 Verification Contract — Module RTC

**Verification Contract — FR-RTC-01: Giữ giờ RTC khi mất điện & Cơ chế Hybrid NTP Slew/Step**
- **Tier:** target
- **Preconditions:** DS3231 đã lắp pin CR2032 (điện áp ≥ 2.8V), đã đồng bộ giờ Internet chính xác (vd: 14:00:00).
- **Stimulus:**
  1. Ngắt nguồn điện chính của Gateway trong 30 phút.
  2. Cấp điện lại khi không có Wi-Fi (offline boot).
  3. Kết nối Wi-Fi với máy chủ NTP: Trường hợp A giả lập lệch giờ 5 phút; Trường hợp B giả lập lệch giờ 2 giờ.
- **Expected:**
  1. Lúc khởi động offline, `rtc_init()` đọc I2C từ DS3231 thành công, thời gian hệ thống hiển thị 14:30:xx (sai số trôi giờ ≤ 1 giây).
  2. `rtc_is_valid()` trả về `true`.
  3. Trường hợp A (lệch ≤ 15m): Hệ thống kích hoạt Smooth Slew, giờ trôi êm dịu không nhảy lùi.
  4. Trường hợp B (lệch > 15m): Hệ thống kích hoạt Step Adjustment, cập nhật ngay lập tức sang mốc giờ mới và ghi đè thanh ghi DS3231 thành công.
- **Timing / tolerance:** Độ trôi RTC khi mất nguồn ≤ 2 ppm (~0.17 giây/ngày). Thời gian đọc I2C lúc boot ≤ 20ms.
- **Prohibited:** Bị reset về Epoch 1970 khi mất điện; treo hệ thống khi NTP server không phản hồi; lạm dụng Step Adjustment khi độ lệch nhỏ (≤ 15m).
- **Operator instructions:**
  1. Nạp firmware, kiểm tra serial log xác nhận `[RTC] Loaded time from DS3231: 2026-xx-xx`.
  2. Rút cáp nguồn, đợi 5 phút, cắm lại cáp và đối chiếu giờ in ra terminal với đồng hồ điện thoại. Phải khớp từng phút.
  3. Chạy test giả lập NTP bù giờ nhỏ và lớn, kiểm tra log `[RTC] Smooth slew applied` và `[RTC] Step adjustment applied`.

---

### 4.3 Module: Storage (`storage.h/.c`)

#### 4.3.1 Mô tả
Quản lý lưu trữ tệp cục bộ trên Flash phân vùng `storage` sử dụng hệ thống tệp **LittleFS** an toàn với cơ chế **Ghi Nguyên Tử (Atomic File Replacement)** chống mất điện đột ngột. Đồng bộ cấu hình từ xa thông qua giao thức **MQTT Downlink** (được xử lý tập trung bởi MQTT Adapter), loại bỏ hoàn toàn các tác vụ polling HTTP dư thừa để tiết kiệm bộ nhớ RAM và ngăn chặn xung đột mạng.

#### 4.3.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| ST-01 | Mount LittleFS An Toàn | `esp_vfs_littlefs_register()` cho partition "storage", tự động format phục hồi nếu partition hỏng |
| ST-02 | Ghi Tệp Nguyên Tử (Atomic Write) | Ghi dữ liệu ra file tạm `.tmp`, gọi `fflush()`, sau đó dùng POSIX `rename()` tráo đổi tức thời |
| ST-03 | Quản lý `schedule.json` | Lưu trữ cấu hình lịch biểu, báo thức. Backend cập nhật qua MQTT Downlink |
| ST-04 | Quản lý `rules.json` | Lưu trữ các quy tắc tự động hóa nội bộ (Rule Engine) |
| ST-05 | Quản lý `device_list.json` | Lưu trữ danh sách cảm biến BLE và thiết bị ngoại vi đã đăng ký |
| ST-06 | Ringbuffer Offline Fallback | Lưu trữ 100 bản tin sự kiện gần nhất trong RAM/LittleFS khi không có thẻ nhớ MicroSD |

#### 4.3.3 API & Cơ chế Ghi Nguyên Tử

```c
void storage_init(void);
// Hành vi: Mount LittleFS trên phân vùng "storage" (offset 0x620000, size 2MB).
//          Nếu lỗi mount -> tự động format lại phân vùng và khởi tạo cấu trúc thư mục sạch.

bool storage_atomic_write_file(const char *path, const char *content);
// Hành vi: Ghi nguyên tử an toàn tuyệt đối trước sự cố mất nguồn:
//   1. Mở file tạm: snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);
//   2. fwrite() toàn bộ content vào tmp_path, gọi fflush(f) và fsync(fileno(f)).
//   3. fclose(f).
//   4. Gọi POSIX rename(tmp_path, path) để tráo đổi nguyên tử.
//   -> Nếu mất điện giữa chừng: File cũ vẫn nguyên vẹn 100%, không bao giờ bị 0 bytes.

char *storage_load_file_string(const char *path);
// Hành vi: Đọc toàn bộ nội dung file text.
// Trả về: Chuỗi heap malloc (caller PHẢI free), hoặc NULL nếu file không tồn tại.

bool storage_save_schedule_json(const char *json_str);
char *storage_load_schedule_json(void);

bool storage_save_rules_json(const char *json_str);
char *storage_load_rules_json(void);

bool storage_save_device_list_json(const char *json_str);
char *storage_load_device_list_json(void);
```

#### 4.3.4 Cấu trúc thư mục LittleFS (`/storage/`)

```
/storage/
├── schedule.json        ← Backend gửi qua MQTT Downlink, Gateway lưu và thực thi
├── rules.json           ← Backend / Web App gửi qua MQTT, Rule Engine nạp thực thi
├── device_list.json     ← Danh sách cảm biến BLE đã gán nhãn
└── fallback_events.bin  ← Vùng đệm sự kiện khẩn cấp khi không cắm thẻ nhớ SD
```

#### 4.3.5 Cơ chế Đồng bộ Cấu hình qua MQTT Downlink & Storage Worker Task

Nhằm tuân thủ tuyệt đối nguyên tắc **Non-Blocking Dispatcher (§4.1.6)** và **Inversion of Control (§3.3)**, toàn bộ thao tác I/O Flash, phân tích cú pháp JSON nặng và kiểm tra tính toàn vẹn được giao cho **Storage Worker Task** chạy bất đồng bộ trên **Core 1 (Priority 3)**:

```
MQTT Broker                  Gateway MQTT Adapter             Storage Worker Task (Core 1)    Event Bus / Subsystems
     │                                │                               │                               │
     │── publish(config/{type}) ─────►│                               │                               │
     │   (QoS 1, retained)            │── Enqueue job ───────────────►│                               │
     │                                │   (Non-blocking queue)        │── Check Hash/CRC (Skip if same)
     │                                │                               │── Atomic LittleFS Write (.tmp)│
     │                                │                               │── cJSON_Parse & Validation    │
     │                                │                               │── Rename .tmp -> config.json  │
     │                                │                               │── post(EVT_CONFIG_UPDATED) ──►│
     │                                │                               │   (FLAGS: RULES/SCHED/DEVICES)│
     │                                │                               │                               ├── rule_engine reloads rules
     │                                │                               │                               ├── scheduler reloads schedule
     │                                │                               │                               └── ble_adapter updates WHITELIST!
```

**Các bước xử lý chuẩn hóa:**
1. **MQTT Adapter tiếp nhận:** Nhận bản tin `gateway/{id}/config/{type}` (`type` $\in$ `{"schedule", "rules", "devices"}`), đóng gói con trỏ payload và sao chép vào `s_config_worker_queue` (10 slots) với timeout = 0ms. Không thực hiện I/O trên luồng mạng.
2. **Kiểm tra Chống Mòn Flash (Hash / CRC Check):** Storage Worker Task tính toán SHA-256 hoặc CRC32 của nội dung payload và so sánh với hash của tệp đang lưu trên Flash. Nếu nội dung hoàn toàn trùng khớp (thường gặp khi nhận lại retained message lúc reconnect), worker bỏ qua thao tác ghi Flash, bảo vệ tuổi thọ ô nhớ Flash LittleFS.
3. **Ghi Tệp Nguyên Tử (Atomic LittleFS Write):** Ghi nội dung vào tệp tạm `/storage/{type}.json.tmp`, gọi `fflush()` và `fsync()`.
4. **Xác thực Cú pháp (Validation):** Phân tích `cJSON_Parse()`. Nếu JSON không hợp lệ, worker hủy bỏ tệp `.tmp`, ghi cảnh báo `ESP_LOGE` và không ghi đè tệp chính thức.
5. **Hoán đổi Nguyên tử (Atomic Rename):** Sử dụng `rename("/storage/{type}.json.tmp", "/storage/{type}.json")`.
6. **Phát Sự Kiện Độc Lập (Inversion of Control):** Phát sự kiện `EVT_CONFIG_UPDATED_FROM_CLOUD` kèm cờ `config_type_flags_t` (`CONFIG_TYPE_SCHEDULE`, `CONFIG_TYPE_RULES`, hoặc `CONFIG_TYPE_DEVICES`).
   - `rule_engine` tự reload khi nhận cờ `CONFIG_TYPE_RULES`.
   - `scheduler` tự reload khi nhận cờ `CONFIG_TYPE_SCHEDULE`.
   - `ble_adapter` tự đọc lại `device_list.json` và cập nhật tức thời bảng Whitelist MAC Address (`s_known_devices[]`) khi nhận cờ `CONFIG_TYPE_DEVICES` mà không cần khởi động lại chip.

#### 4.3.6 Verification Contract — Module Storage

**Verification Contract — FR-ST-01: Ghi đè file JSON nguyên tử, chống mòn Flash & Nạp Whitelist**
- **Tier:** target
- **Preconditions:** Gateway đã mount LittleFS thành công, file `/storage/schedule.json` đang chứa dữ liệu hợp lệ (10 mục lịch hẹn); Storage Worker Task đang vận hành trên Core 1.
- **Stimulus:**
  1. Gửi bản tin cấu hình `gateway/{id}/config/schedule` giống hệt 100% nội dung hiện hành qua MQTT Broker.
  2. Gửi bản tin cấu hình `gateway/{id}/config/devices` mới bổ sung thêm cảm biến `sensor_livingroom` (MAC `11:22:33:44:55:66`).
  3. Gửi bản tin cấu hình `gateway/{id}/config/rules` mới dung lượng 4KB; đồng thời ngắt nguồn điện đột ngột trong vòng 20ms kể từ khi bắt đầu ghi.
- **Expected:**
  1. Khi nhận payload trùng: Log in `[STORAGE] Hash matches existing file, skipping flash write`, số chu kỳ ghi Flash không tăng.
  2. Khi nhận devices mới: Storage ghi thành công, phát `EVT_CONFIG_UPDATED_FROM_CLOUD` với cờ `CONFIG_TYPE_DEVICES`; `ble_adapter` nạp ngay MAC mới vào Whitelist mà không cần reboot.
  3. Sau sự cố ngắt nguồn: Mount lại LittleFS thành công không lỗi `DIRTY_FS`. Tệp `/storage/rules.json` còn nguyên vẹn 100% (file cũ hoặc file mới hoàn chỉnh), không bao giờ bị 0 bytes hoặc JSON cụt.
- **Timing / tolerance:** Thời gian phân tích và cập nhật cấu hình của Worker Task $\le 100\text{ms}$; thời gian mount LittleFS lúc boot $\le 150\text{ms}$.
- **Prohibited:** Dispatcher task thực hiện ghi Flash; file bị 0 bytes khi mất nguồn; ghi Flash liên tục khi nhận bản tin retained không thay đổi; `ble_adapter` không nhận diện được sensor mới sau cập nhật.
- **Operator instructions:**
  1. Nạp firmware, theo dõi terminal: `[STORAGE] LittleFS mounted successfully`.
  2. Publish lại cùng 1 cấu hình từ broker, xác nhận terminal báo skip write.
  3. Publish cấu hình devices mới, dùng lệnh CLI `ble status` kiểm tra cảm biến mới xuất hiện tức thì trong danh sách.
  4. Thực hiện bài test ngắt nguồn khi ghi và dùng lệnh `storage verify` kiểm tra tính toàn vẹn.

---

### 4.4 Module: Wi-Fi Provisioning (`wifi_provisioning.h/.c`)

#### 4.4.1 Mô tả
Setup Wi-Fi lần đầu qua SoftAP + Captive Portal. Không cần mobile app — hoạt động trên mọi trình duyệt.

#### 4.4.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| WP-01 | Kiểm tra NVS có Wi-Fi config | Boot → đọc NVS "wifi_cfg" |
| WP-02 | Bật SoftAP nếu chưa config | AP name: "SmartClock-Setup", open network |
| WP-03 | Captive Portal tự mở trên trình duyệt | DNS redirect tất cả domain → IP của ESP32 |
| WP-04 | Form nhập SSID + Password + Timezone | Timezone tự lấy từ JS: `Intl.DateTimeFormat().resolvedOptions().timeZone` |
| WP-05 | Lưu config vào NVS | Namespace "wifi_cfg", keys: ssid, password, timezone |
| WP-06 | Post events lên bus | `EVT_WIFI_CREDENTIALS_RECEIVED`, `EVT_WIFI_CONNECTED`, `EVT_WIFI_LOST` |
| WP-07 | Force reset | Xóa NVS wifi_cfg, quay lại SoftAP (khi user giữ nút reset > 5s) |

#### 4.4.3 API

```c
void wifi_provisioning_init(void);
// Hành vi:
//   Đọc NVS "wifi_cfg"
//   Nếu có → try_connect_saved_wifi()
//   Nếu không → start_softap_captive_portal()

void wifi_provisioning_force_reset(void);
// Hành vi: Xóa NVS, bật lại SoftAP

bool wifi_provisioning_is_configured(void);
// Trả về: true nếu đã có SSID/password trong NVS
```

#### 4.4.4 Captive Portal Flow

```
User                    ESP32 SoftAP              NVS        Event Bus
 │                         │                       │             │
 │── Kết nối WiFi ────────►│                       │             │
 │   "SmartClock-Setup"    │                       │             │
 │                         │── DNS redirect ──────►│             │
 │◄── Tự mở trình duyệt ──│   mọi domain          │             │
 │                         │   → 192.168.4.1       │             │
 │                         │                       │             │
 │── Nhập SSID, Pass ─────►│                       │             │
 │   (Timezone tự lấy      │                       │             │
 │    từ JS browser)       │                       │             │
 │                         │── Lưu NVS ───────────►│             │
 │                         │── Post ──────────────────────────► EVT_WIFI_CREDENTIALS_RECEIVED
 │                         │                       │             │  → rtc_set_timezone()
 │                         │── esp_wifi_connect()   │             │
 │                         │── Thành công ────────────────────► EVT_WIFI_CONNECTED
 │                         │                       │             │  → rtc_sync_ntp()
 │                         │                       │             │  → mqtt_connect()
```

#### 4.4.5 Captive Portal HTML Form

```html
<!-- Các field bắt buộc -->
<input type="text"     name="ssid"     placeholder="Tên Wi-Fi" required>
<input type="password" name="password" placeholder="Mật khẩu"  required>
<input type="hidden"   name="timezone" id="timezone">

<script>
  // Tự lấy timezone khi trang load — không cần API ngoài
  document.getElementById('timezone').value =
      Intl.DateTimeFormat().resolvedOptions().timeZone;
</script>
```

#### 4.4.6 NVS Storage

| Namespace | Key | Type | Ví dụ |
|---|---|---|---|
| `wifi_cfg` | `ssid` | string (max 33) | `"MyHomeWiFi"` |
| `wifi_cfg` | `password` | string (max 65) | `"secret123"` |
| `wifi_cfg` | `timezone` | string (max 64) | `"ICT-7"` |

---

### 4.5 Module: BLE Telemetry Adapter (`ble_adapter.h/.c`)

#### 4.5.1 Mô tả
Chịu trách nhiệm quét thụ động liên tục (Passive BLE Advertisement Observer) các gói tin quảng bá môi trường (Service UUID `0x181A`, định dạng BTHome / Xiaomi BLE) từ các cảm biến nhiệt ẩm không dây xung quanh, giải mã và đẩy vào hệ sinh thái Gateway.

#### 4.5.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| BA-01 | Quản lý danh sách cảm biến BLE | Tra cứu `s_known_devices[]`, nạp từ `/storage/device_list.json` lúc boot; tự động cập nhật lại whitelist khi nhận `EVT_CONFIG_UPDATED_FROM_CLOUD` (cờ `CONFIG_TYPE_DEVICES`) |
| BA-02 | Passive Scan Advertisement | `ble_scan_task` (Core 0, priority 5), quét thụ động liên tục với Software Coexistence (50% Duty Cycle) |
| BA-03 | Lọc trùng & Post Telemetry | Lọc trùng phần mềm (Software Dedupe Window); đẩy `EVT_BLE_DEVICE_DATA` với `ble_telemetry_t*` (temp, hum, batt, valid, ts) |
| BA-04 | Phát hiện cảm biến offline | Không nhận beacon sau **60 giây** → post `EVT_DEVICE_OFFLINE_ALERT`. Tuyệt đối cố định 60s cho cảm biến sưởi, không nới lỏng |
| BA-05 | Báo cáo nhịp tim Supervisor | Định kỳ 1000ms gọi `supervisor_report_heartbeat(TASK_HEARTBEAT_BLE)` |

#### 4.5.3 API

```c
void ble_adapter_init(void);
// Hành vi: Nạp danh sách thiết bị từ /storage/device_list.json
//          Khởi tạo ngăn xếp Apache NimBLE ở chế độ GAP Central / Observer
//          Tạo ble_scan_task (Core 0, Priority 5, Stack 4096)

void ble_adapter_reload_whitelist(void);
// Hành vi: Đọc lại /storage/device_list.json và cập nhật danh sách MAC hợp lệ tức thời vào s_known_devices[]

void ble_adapter_register_device(const char *device_id, const char *mac_address);
// Hành vi: Thêm cảm biến vào bảng theo dõi (tối đa 16 thiết bị)
```

#### 4.5.4 BLE Advertisement Processing Flow

```
ble_scan_task (vòng lặp chu kỳ 1000ms)
    │
    ├── 1. Báo cáo nhịp tim: supervisor_report_heartbeat(TASK_HEARTBEAT_BLE)
    │
    ├── 2. Bắt gói tin BLE Advertisement từ NimBLE GAP Event Callback:
    │     │
    │     ├── So khớp MAC Address với danh sách s_known_devices[] (Whitelist Link-layer check):
    │     │     │── Không khớp? ──► Bỏ qua ngay lập tức (0 bytes heap)
    │     │     │── Khớp! ↓
    │     │
    │     ├── Kiểm tra định dạng & Giải mã theo parser_type:
    │     │     ├── "sig_0x181a": Chuẩn Bluetooth SIG Environmental Sensing Service (UUID 0x181A)
    │     │     ├── "bthome_v2": BTHome V2 format (không mã hóa hoặc mã hóa AES-128 CCM với bindkey)
    │     │     └── "mijia_v2": Xiaomi MiBeacon format (giải mã AES-128 với bindkey 16 bytes)
    │     │     │── Payload hỏng / CRC lỗi? ──► Hủy bỏ gói tin
    │     │     │── Hợp lệ! ↓
    │     │
    │     ├── Cơ chế lọc trùng phần mềm (Software Dedupe Window):
    │     │     ├── Trùng 100% payload với gói trước VÀ trong vòng < 5 giây? ──► Bỏ qua (chống ngập bus)
    │     │     └── Có thay đổi giá trị (nhiệt độ/độ ẩm thay đổi) HOẶC định kỳ ≥ 10 giây:
    │     │           ├── Cập nhật last_seen_mono_us = esp_timer_get_time()
    │     │           ├── Cấp phát malloc(ble_telemetry_t), điền telemetry: valid = true, temp, hum, batt, ts
    │     │           └── event_bus_post(EVT_BLE_DEVICE_DATA, telem, len, false)
    │     │
    │     └── 3. Quét kiểm tra Timeout mất kết nối (Sensor Offline Scan):
    │           Với mỗi cảm biến trong bảng:
    │           ├── Nếu là Cảm biến gắn với Tải Sưởi Nhiệt (Heater / Relay 2):
    │           │     └── Ngưỡng timeout BẮT BUỘC CỐ ĐỊNH = 60 GIÂY (Không nới 180s trong bất kỳ tình huống nào!)
    │           │         Nếu (now_mono - last_seen_mono > 60s):
    │           │           is_online = false, post EVT_DEVICE_OFFLINE_ALERT (alert_type: ALERT_SENSOR_OFFLINE)
    │           └── Nếu là Cảm biến thông thường khác:
    │                 └── Ngưỡng thích ứng: 60s (Wi-Fi ổn định) hoặc 180s (Wi-Fi đang reconnecting)
```

#### 4.5.5 Cấu hình NimBLE Link-layer & Bộ lọc trùng phần mềm (Software Dedupe Window)

1. **Cấu hình NimBLE Link-layer:**
   - **Tắt lọc trùng phần cứng của Controller:** `disc_params.filter_duplicates = 0`.
     - *Lý do kỹ thuật bắt buộc:* Nếu bật `filter_duplicates = 1`, phần cứng BLE controller sẽ âm thầm vứt bỏ toàn bộ các gói tin phát lại định kỳ của cảm biến sau lần nhận đầu tiên, khiến hệ thống hiểu nhầm cảm biến bị chết sau 60 giây. Bắt buộc để `filter_duplicates = 0` để nhận các beacon lặp lại.
   - **Software Coexistence:** Tỷ lệ **50% Duty Cycle** (`scan_interval = 60ms`, `scan_window = 30ms`), bảo đảm không gây nghẽn kết nối Wi-Fi.
2. **Bộ lọc trùng phần mềm (Software Dedupe Window):**
   - Lưu trữ `last_payload_hash` (hoặc packet counter) và `last_processed_mono_us` cho từng MAC trong whitelist.
   - Nếu gói tin trùng lặp 100% trong vòng **5 giây** $\rightarrow$ drop ngay tại chỗ để chống bão Event Bus.
   - Chấp nhận gói tin và post `EVT_BLE_DEVICE_DATA` khi: (1) Giá trị đo thay đổi (nhiệt độ lệch $\ge 0.1^\circ\text{C}$ hoặc độ ẩm lệch $\ge 1\%$), HOẶC (2) Đã trôi qua chu kỳ định kỳ tối đa **10 giây**.
3. **Cơ chế An Toàn Bất Biến cho Tải Nhiệt (Strict 60s Timeout):**
   - Đối với cảm biến nhiệt độ gắn với quy tắc điều khiển máy sưởi (`relay_2`), thời gian phát hiện mất tín hiệu **bắt buộc cố định 60 giây**, tuyệt đối cấm nới lên 180s kể cả khi Wi-Fi đang reconnect. Nếu sau 60s không có beacon mới, bộ điều khiển phải kích hoạt fail-safe ngắt ngay máy sưởi để phòng chống hỏa hoạn.

#### 4.5.6 Verification Contract — BLE Telemetry Adapter

**Verification Contract — FR-BLE-01: Quét thụ động BLE, Lọc trùng 5s/10s & Timeout an toàn 60s cố định**
- **Tier:** target
- **Preconditions:** Gateway đã đăng ký cảm biến `sensor_temp_01` (MAC `A4:C1:38:11:22:33`, parser `bthome_v2`); `disc_params.filter_duplicates = 0`; tải nhiệt `relay_2` đang được cấu hình giám sát theo cảm biến này.
- **Stimulus:**
  1. Cảm biến phát liên tục 10 beacon giống hệt nhau trong 3 giây (nhiệt độ 28.5°C, độ ẩm 65.0%).
  2. Tại giây thứ 4, cảm biến phát beacon đổi giá trị lên 28.8°C.
  3. Duy trì giá trị 28.8°C trong 15 giây liên tục.
  4. Ngắt hoàn toàn cảm biến trong 65 giây (đồng thời kích hoạt router Wi-Fi reconnect để tạo tải RF).
- **Expected:**
  1. Trong 3 giây đầu: Chỉ có đúng 1 sự kiện `EVT_BLE_DEVICE_DATA` được đưa lên Event Bus (Software Dedupe 5s hoạt động).
  2. Tại giây thứ 4: Giá trị đổi sang 28.8°C được post ngay tức thì lên Event Bus (< 200ms).
  3. Trong 15 giây tiếp theo: Sau mỗi 10 giây định kỳ xuất hiện đúng 1 sự kiện refresh heartbeat.
  4. Khi ngắt cảm biến: Sau đúng 60 giây (dù Wi-Fi đang reconnect), Gateway post `EVT_DEVICE_OFFLINE_ALERT` mang `device_id = "sensor_temp_01"`; không bị kéo dài lên 180 giây.
- **Timing / tolerance:** Độ trễ nhận và phân tích gói tin $\le 500\text{ms}$; sai số phát hiện timeout 60s $\le 1\text{s}$.
- **Prohibited:** Bật `filter_duplicates = 1` làm mất beacon; để lọt thiết bị ngoài whitelist; nới timeout lên 180s cho cảm biến sưởi; rò rỉ heap khi lọc gói tin.
- **Operator instructions:**
  1. Mở Serial Terminal, gõ `ble scan`.
  2. Kích hoạt cảm biến phát tin, kiểm tra log xác nhận telemetry và dedupe window.
  3. Tắt nguồn cảm biến nhiệt, bấm đồng hồ đếm: đúng 60s sau terminal in `[BLE] CRITICAL: sensor_temp_01 OFFLINE! Triggering fail-safe alert`.

---

### 4.6 Module: MQTT Adapter (`mqtt_adapter.h/.c`)

#### 4.6.1 Mô tả
Protocol Adapter phía MQTT — giao tiếp với Backend và các thiết bị MQTT-native. Pattern đối xứng hoàn toàn với BLE Adapter.

#### 4.6.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| MA-01 | Kết nối MQTT broker | esp-mqtt client kết nối broker với Retained LWT ("offline", QoS 1) |
| MA-02 | Subscribe topics | Subscribe các topic: `gateway/{id}/device/+/command`, `gateway/{id}/config/#` |
| MA-03 | Enqueue Config Downlink | Khi nhận `gateway/{id}/config/{type}`, chuyển tiếp ngay vào Storage Worker Queue |
| MA-04 | Publish telemetry/status | `mqtt_adapter_publish()` gửi dữ liệu telemetry, status, events, command/ack với QoS 1 |
| MA-05 | Post events lên bus | `EVT_MQTT_CONNECTED`, `EVT_MQTT_DISCONNECTED`, `EVT_MQTT_MESSAGE_RECEIVED` |
| MA-06 | Forward lệnh từ Web App | Nhận MQTT command → post `EVT_DEVICE_COMMAND_REQUEST` → `relay_controller` tiếp nhận |

#### 4.6.3 API

```c
void mqtt_adapter_init(const char *broker_uri, const char *gateway_id);
// Hành vi: Init esp-mqtt client với LWT (QoS 1)
//          Register handler EVT_RULE_ACTION_REQUEST
//          Register handler EVT_COMMAND_RESPONSE

bool mqtt_adapter_publish(const char *topic_suffix, const char *payload_json, int qos);
// Hành vi: Publish tới "gateway/{id}/{topic_suffix}" với QoS chỉ định (mặc định QoS 1)
// QUAN TRỌNG: Khi gọi từ dispatcher handler, sử dụng timeout=0 (non-blocking outbox enqueue).
```

#### 4.6.4 MQTT Topics (Hợp nhất Chuẩn hóa Toàn Hệ Thống)

| Topic | Direction | QoS | Retain | Mô tả |
|---|---|:---:|:---:|---|
| `gateway/{id}/status` | Publish | 1 | Yes | "online" lúc kết nối / "offline" qua LWT |
| `gateway/{id}/device/{did}/telemetry` | Publish | 1 | No | Telemetry thời gian thực từ cảm biến (BLE / MQTT) |
| `gateway/{id}/device/{did}/command` | Subscribe | 1 | No | Lệnh điều khiển rơ-le/thiết bị từ Backend/Web App |
| `gateway/{id}/device/{did}/command/ack` | Publish | 1 | No | Phản hồi kết quả thực thi lệnh kèm command_id, mã lỗi |
| `gateway/{id}/config/#` | Subscribe | 1 | Yes | Backend đẩy cấu hình xuống (`schedule`, `rules`, `devices`) |
| `gateway/{id}/events` | Publish | 1 | No | Nhật ký sự kiện thời gian thực (báo thức, cutoff, rule trigger) |
| `gateway/{id}/telemetry/batch` | Publish | 1 | No | Đồng bộ bù (Backfill) dữ liệu từ thẻ nhớ SD khi có mạng lại |

#### 4.6.5 LWT (Last Will and Testament)

```c
esp_mqtt_client_config_t cfg = {
    .broker.address.uri = broker_uri,
    .session.last_will = {
        .topic  = "gateway/{id}/status",
        .msg    = "offline",
        .qos    = 1,
        .retain = 1,  // giữ trên broker để Web App và Backend cập nhật trạng thái
    }
};
```

Khi gateway mất kết nối đột ngột (không kịp gửi "offline"), broker tự publish LWT → Web App hiển thị đúng trạng thái.

#### 4.6.6 Điều khiển Thiết bị từ xa & Phản hồi ACK (Remote Command & ACK Flow)

Khi user bấm nút ON/OFF trên Web App cho một Relay onboard hoặc thiết bị chấp hành, lệnh được chuyển tiếp theo đường:

```
Web App                Backend              MQTT Broker          Gateway (mqtt_adapter)       Relay Controller
  │                      │                     │                      │                              │
  ├─ POST /devices/      │                     │                      │                              │
  │  relay_1/command ───►│                     │                      │                              │
  │  {"command":"ON"}    │── publish ──────────►│                      │                              │
  │                      │  gateway/{id}/device/                      │                              │
  │                      │  relay_1/command    │── deliver ──────────►│                              │
  │                      │                     │                      │                              │
  │                      │                     │   on_mqtt_command_recv():                           │
  │                      │                     │   ├── malloc(device_command_req_t)                  │
  │                      │                     │   └── post EVT_DEVICE_COMMAND_REQUEST ─────────────►│
  │                      │                     │                      │                              │
  │                      │                     │                      │   relay_controller_set():    │
  │                      │                     │                      │   ├── Thẩm định Interlock    │
  │                      │                     │                      │   ├── Ghi chân GPIO 15 HIGH  │
  │                      │                     │                      │   └── post EVT_COMMAND_RESP ─┤
  │                      │                     │                      │◄─────────────────────────────┘
  │                      │                     │   on_command_response():                            │
  │                      │                     │   └── publish(topic: .../command/ack, QoS 1)        │
  │                      │◄── deliver ─────────│◄─────────────────────┤                              │
  │                      │  (command/ack)      │                      │                              │
  │◄── SSE / WebSocket ──┤                     │                      │                              │
  │    (Cập nhật UI)     │                     │                      │                              │
```

> **Lưu ý kiến trúc:** Toàn bộ lệnh điều khiển phần cứng được thực thi bởi `relay_controller` trên 4 kênh GPIO onboard (`relay_1` đến `relay_4`) hoặc chuyển tiếp qua MQTT cho các thiết bị Wi-Fi phụ trợ. Không sử dụng BLE Actuator / GATT Write.


#### 4.6.7 MQTT Telemetry Payload Struct

```c
// Struct cho EVT_MQTT_MESSAGE_RECEIVED — tương đương ble_telemetry_t nhưng cho MQTT device
typedef struct {
    char  device_id[32]; // khớp device_id đăng ký trên Backend
    char  payload[128];  // raw JSON payload từ MQTT message
    float value;         // giá trị đã parse (nếu là telemetry số)
    bool  valid;         // false nếu parse lỗi hoặc message format sai
} mqtt_telemetry_t;
```

#### 4.6.8 Cơ chế Tái Kết Nối Thông Minh (Exponential Backoff) & Giới Hạn Hàng Đợi (Outbox Throttling)

1. **Exponential Backoff với Jitter:**
   - Khi mất kết nối MQTT Broker (hoặc Wi-Fi rớt), Gateway không được phép reconnect dồn dập làm cạn kiệt tài nguyên CPU và nghẽn vô tuyến RF.
   - Chu kỳ thử lại bắt đầu từ **1 giây**, nhân đôi sau mỗi lần thất bại: $1\text{s} \rightarrow 2\text{s} \rightarrow 4\text{s} \rightarrow 8\text{s} \rightarrow 16\text{s} \rightarrow 32\text{s} \rightarrow 60\text{s}$ (trần tối đa 60 giây).
   - Thêm nhiễu ngẫu nhiên (Jitter) $\pm 20\%$ thời gian chờ để triệt tiêu hiện tượng sấm sét đồng loạt (Thundering Herd) khi nhiều Gateway cùng kết nối lại broker sau sự cố điện/mạng diện rộng.
2. **Kiểm soát hàng đợi Outbox (Outbox Buffer Overflow Protection):**
   - Khi broker mất kết nối, các bản tin telemetry thời gian thực (QoS 0) **bị hủy bỏ ngay lập tức** (không đọng lại trong RAM).
   - Dữ liệu lịch sử quan trọng được chuyển tuyến sang thẻ nhớ MicroSD (`sd_logger`) qua cơ chế Store-and-Forward (`sync_queue.bin`), dung lượng RAM dành cho `esp-mqtt` outbox luôn duy trì cố định $\le 10\text{KB}$.

#### 4.6.9 Verification Contract — MQTT Adapter

**Verification Contract — FR-MQTT-01: Tái kết nối Exponential Backoff & Phục hồi phiên LWT**
- **Tier:** target
- **Preconditions:** Gateway đã kết nối Wi-Fi và MQTT Broker thành công; topic `gateway/{id}/status` đang có giá trị retained `"online"`.
- **Stimulus:**
  1. Tắt dịch vụ MQTT Broker trên máy chủ trong 45 giây (giữ nguyên Wi-Fi AP).
  2. Khởi động lại dịch vụ MQTT Broker.
- **Expected:**
  1. Khi Broker sập: Gateway phát hiện sự cố qua keepalive ping, post `EVT_MQTT_DISCONNECTED`. Broker publish LWT `"offline"`.
  2. Gateway kích hoạt thuật toán Exponential Backoff: Khoảng cách giữa các lần thử kết nối lại giãn cách đúng quy luật (1s, 2s, 4s, 8s, 16s...); không gây tràn RAM (Heap free dao động $\le 1\text{KB}$); không làm treo Task Supervisor.
  3. Khi Broker hoạt động trở lại: Gateway tự động kết nối lại thành công, subscribe lại đầy đủ các topic và publish lại trạng thái `"online"` (retained).
- **Timing / tolerance:** Thời gian phát hiện mất kết nối $\le 10\text{s}$; thời gian tái kết nối thành công $\le 5\text{s}$ sau khi broker sẵn sàng.
- **Prohibited:** Thử kết nối lại liên tục với chu kỳ ngắn (< 500ms); rò rỉ bộ nhớ (memory leak); task bị kẹt gây Task Supervisor panic reboot.
- **Operator instructions:**
  1. Chạy broker Mosquitto trên máy tính thử nghiệm: `mosquitto -v`.
  2. Nạp firmware Gateway, quan sát terminal: `[MQTT] Connected to broker: mqtt://...`.
  3. Nhấn `Ctrl+C` tắt Mosquitto: Terminal Gateway hiển thị `[MQTT] Lost connection, retry in 1s`, sau đó `retry in 2s`, `retry in 4s`...
  4. Chạy lại Mosquitto: Terminal Gateway in `[MQTT] Reconnected! Subscribed to config and command topics`.

---


### 4.7 Module: Rule Engine (`rule_engine.h/.c`)

#### 4.7.1 Mô tả
Đánh giá "IF condition THEN action" hoàn toàn tại local. Chỉ xử lý **automation** (điều khiển thiết bị), KHÔNG xử lý alarm (hiển thị/kêu chuông — đó là việc của display.c).

#### 4.7.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| RE-01 | Nạp rule từ storage | Đọc và phân tích cú pháp tệp `/storage/rules.json` |
| RE-02 | Đánh giá telemetry BLE | Handler `EVT_BLE_DEVICE_DATA`: so khớp rule qua Monotonic Clock |
| RE-03 | Đánh giá telemetry MQTT | Handler `EVT_MQTT_MESSAGE_RECEIVED`: tương tự |
| RE-04 | Thực thi automation theo giờ | Handler `EVT_SCHEDULE_AUTOMATION_TRIGGERED` |
| RE-05 | Post action request | `EVT_RULE_ACTION_REQUEST` → `relay_controller` tiếp nhận |
| RE-06 | Safe-default | `valid=false` → KHÔNG đánh giá, phát `EVT_SENSOR_VALUE_INVALID` ngắt tải nhiệt |
| RE-07 | Reload rules | Handler `EVT_CONFIG_UPDATED_FROM_CLOUD` (cờ `CONFIG_TYPE_RULES`) → `rule_engine_reload_rules()` |
| RE-08 | Xử lý Sensor Offline Fail-Safe | Handler `EVT_DEVICE_OFFLINE_ALERT`: quét rule liên quan, ngắt sưởi 0ms và khóa 300s |

#### 4.7.3 API

```c
void rule_engine_init(void);
// Hành vi: Nạp rules từ storage;
//          Đăng ký handlers: EVT_BLE_DEVICE_DATA,
//                            EVT_MQTT_MESSAGE_RECEIVED,
//                            EVT_SCHEDULE_AUTOMATION_TRIGGERED,
//                            EVT_RELAY_STATE_CHANGED,
//                            EVT_DEVICE_OFFLINE_ALERT,
//                            EVT_CONFIG_UPDATED_FROM_CLOUD

void rule_engine_reload_rules(void);
// Hành vi: Đọc lại /storage/rules.json và cập nhật danh sách quy tắc trong RAM
```

#### 4.7.4 Rule Data Model (Chuẩn hóa Monotonic Clock)

```c
typedef enum {
    RULE_CONDITION_SENSOR_GT,   // sensor_value >= threshold (Ngưỡng trên, vd: Quạt làm mát)
    RULE_CONDITION_SENSOR_LT,   // sensor_value <= threshold (Ngưỡng dưới, vd: Máy sưởi nhiệt)
    RULE_CONDITION_TIME_MATCH,  // Đúng giờ hẹn (do scheduler lọc sẵn)
} rule_condition_type_t;

typedef struct {
    int                   id;
    char                  device_id[32];              // Cảm biến phát sinh dữ liệu (vd: "sensor_temp_01")
    rule_condition_type_t condition_type;
    float                 threshold;                 // Ngưỡng nhiệt độ / độ ẩm kích hoạt
    float                 hysteresis;                // Biên trễ chống dao động tiếp điểm (mặc định 1.5°C)
    uint32_t              cooldown_s;                // Cooldown tối thiểu chiều BẬT (mặc định 60s)
    char                  target_device_id[32];      // Relay đích ("relay_1", "relay_2", ...)
    char                  action_command[32];        // "ON", "OFF"
    bool                  is_active;                 // Trạng thái bật/tắt của luật
    // Quản lý trạng thái nội bộ bằng Monotonic Clock (esp_timer_get_time())
    bool                  current_state;             // Đồng bộ 100% hai chiều từ EVT_RELAY_STATE_CHANGED
    int64_t               last_triggered_mono_us;    // Monotonic timestamp kích hoạt gần nhất (microsecond)
    int64_t               lockout_until_mono_us;     // Monotonic timestamp hết hạn khóa an toàn 300s sau cutoff
} automation_rule_t;
```

#### 4.7.5 Thuật toán Hysteresis, Kích hoạt cạnh (Edge-Triggering) & Đồng bộ Trạng thái Hai Chiều

1. **Công thức Hysteresis chuẩn xác qua Monotonic Clock:**
   Toàn bộ việc so sánh thời gian sử dụng $t_{\text{mono}} = \text{esp\_timer\_get\_time()}$ ($\mu\text{s}$), triệt tiêu 100% ảnh hưởng từ bước nhảy NTP hoặc thay đổi RTC:
   - **Chiều Ngưỡng Trên (`SENSOR_GT` — Quạt làm mát `relay_1`, threshold = 30.0°C, hysteresis = 1.5°C):**
     - **Kích hoạt BẬT quạt (`relay_1 ON`):** Khi $T \ge 30.0^\circ\text{C}$ VÀ `current_state == false` VÀ $(t_{\text{mono}} - t_{\text{last\_mono}} \ge \text{cooldown\_us})$ VÀ $(t_{\text{mono}} \ge t_{\text{lockout\_mono}})$.
       - *Chuyển trạng thái:* `current_state = true`, gán $t_{\text{last\_mono}} = t_{\text{mono}}$, post `EVT_RULE_ACTION_REQUEST` (`command = "ON"`). (Lưu ý: Quạt thường không áp dụng Dwell Time 180s của máy nén).
     - **Giải phóng TẮT quạt (`relay_1 OFF`):** Khi $T \le (30.0 - 1.5) = 28.5^\circ\text{C}$ VÀ `current_state == true`.
       - *Chuyển trạng thái:* `current_state = false`, **ngay lập tức** post `EVT_RULE_ACTION_REQUEST` (`command = "OFF"`, bypass cooldown).
     - **Vùng đệm giữ nguyên (Deadband):** Khi $28.5^\circ\text{C} < T < 30.0^\circ\text{C}$, không phát sinh lệnh.
   - **Chiều Ngưỡng Dưới (`SENSOR_LT` — Máy sưởi điện trở `relay_2`, threshold = 22.0°C, hysteresis = 1.5°C):**
     - **Kích hoạt BẬT sưởi (`relay_2 ON`):** Khi $T \le 22.0^\circ\text{C}$ VÀ `current_state == false` VÀ $(t_{\text{mono}} - t_{\text{last\_mono}} \ge \text{cooldown\_us})$ VÀ $(t_{\text{mono}} \ge t_{\text{lockout\_mono}})$.
       - *Chuyển trạng thái:* `current_state = true`, gán $t_{\text{last\_mono}} = t_{\text{mono}}$, post `EVT_RULE_ACTION_REQUEST` (`command = "ON"`). (Lưu ý: Thanh sưởi điện trở thuần không áp dụng Dwell Time 180s của chu kỳ thường).
     - **Giải phóng TẮT sưởi (`relay_2 OFF`):** Khi $T \ge (22.0 + 1.5) = 23.5^\circ\text{C}$ VÀ `current_state == true`.
       - *Chuyển trạng thái:* `current_state = false`, **ngay lập tức** post `EVT_RULE_ACTION_REQUEST` (`command = "OFF"`, 0ms trễ, bypass cooldown).
     - **Vùng đệm giữ nguyên (Deadband):** Khi $22.0^\circ\text{C} < T < 23.5^\circ\text{C}$, không phát sinh lệnh.

2. **Kích hoạt cạnh (Edge-Triggering):** Lệnh chỉ phát sinh tại thời điểm chuyển trạng thái logic (`false → true` hoặc `true → false`). Khi tiếp tục nhận telemetry trong cùng vùng, Rule Engine bỏ qua, triệt tiêu tiếp điểm chattering.

3. **Đồng bộ Trạng thái Hai Chiều (Bidirectional State Synchronization):**
   Khi nhận sự kiện `EVT_RELAY_STATE_CHANGED` (ACK từ Relay Controller khi relay được bật/tắt thủ công qua Web, nút bấm, hay MQTT):
   - Rule Engine duyệt toàn bộ quy tắc có `target_device_id == rst->device_id`:
     ```c
     for (int i = 0; i < s_rule_count; i++) {
         if (strcasecmp(s_rules[i].target_device_id, rst->device_id) == 0) {
             s_rules[i].current_state = rst->state; // Cập nhật 100% trạng thái thực (cả true và false)
             if (!rst->state && (strcmp(rst->reason, "SAFETY_CUTOFF") == 0 ||
                                 strcmp(rst->reason, "SENSOR_OFFLINE") == 0)) {
                 s_rules[i].lockout_until_mono_us = esp_timer_get_time() + 300000000ULL; // Khóa an toàn 300s
             }
         }
     }
     ```
   - Điều này triệt tiêu hoàn toàn lỗi lệch trạng thái khiến relay không tự tắt được khi người dùng đã bật thiết bị trước đó bằng tay.

4. **Fail-Safe Phản hồi Mất Cảm Biến (`EVT_DEVICE_OFFLINE_ALERT`):**
   - Khi nhận `EVT_DEVICE_OFFLINE_ALERT`, Rule Engine kiểm tra các quy tắc liên kết với `alert->device_id`.
   - Nếu quy tắc điều khiển tải nhiệt (Heater / `relay_2`): Lập tức gán `current_state = false`, thiết lập `lockout_until_mono_us = esp_timer_get_time() + 300000000ULL` (300s), đồng thời phát ngay `EVT_RULE_ACTION_REQUEST` (`target: "relay_2"`, `command: "OFF"`, `reason: "SENSOR_OFFLINE"`) và `EVT_SENSOR_VALUE_INVALID` để cưỡng chế ngắt thanh sưởi tức khắc (0ms trễ).

#### 4.7.6 Luồng đánh giá Telemetry tổng thể

```
on_ble_telemetry(evt)
    │
    ├── telem->valid == false?
    │     ├── YES → post EVT_SENSOR_VALUE_INVALID (relay_controller tự động ngắt relay_2 sưởi 0ms)
    │     │         return (KHÔNG đánh giá rule)
    │     └── NO → tiếp tục
    │
    ├── now_mono = esp_timer_get_time()
    ├── Duyệt từng rule trong /storage/rules.json:
    │     ├── rule.device_id != telem->device_id ──► Bỏ qua
    │     ├── now_mono < rule.lockout_until_mono_us ──► Bỏ qua (Đang trong 300s Safety Lockout)
    │     │
    │     ├── Kiểm tra điều kiện ngưỡng Hysteresis:
    │     │     ├── Thỏa mãn BẬT & current_state == false & cooldown OK?
    │     │     │     └── current_state = true, post EVT_RULE_ACTION_REQUEST ("ON")
    │     │     │
    │     │     ├── Thỏa mãn TẮT & current_state == true?
    │     │     │     └── current_state = false, post EVT_RULE_ACTION_REQUEST ("OFF") (bypass cooldown)
    │     │     │
    │     │     └── Nằm trong Deadband ─────────────► Giữ nguyên, không làm gì
```

#### 4.7.7 Verification Contract — Rule Engine

**Verification Contract — FR-RULE-01: Đánh giá Hysteresis Deadband, Đồng bộ Hai Chiều, Monotonic Clock & Khóa 300s**
- **Tier:** host
- **Preconditions:** Khởi tạo Rule Engine với 2 luật tự động:
  1. Luật làm mát: Cảm biến `temp_01` vượt ngưỡng `gt 30.0°C`, biên trễ `hysteresis = 1.5°C`, cooldown 60s, đích `relay_1` ("ON").
  2. Luật sưởi ấm: Cảm biến `temp_01` dưới ngưỡng `lt 22.0°C`, biên trễ `hysteresis = 1.5°C`, cooldown 60s, đích `relay_2` ("ON").
- **Stimulus:**
  1. Bơm chuỗi nhiệt độ: 29.5°C → 30.2°C → 30.1°C → 29.8°C → 28.4°C.
  2. Bơm chuỗi nhiệt độ: 23.0°C → 21.8°C → 22.1°C → 23.6°C.
  3. Phát `EVT_RELAY_STATE_CHANGED` (`relay_1`, `state = true`, `reason = "MANUAL_CMD"`); sau đó bơm nhiệt độ 28.0°C.
  4. Giả lập bước nhảy thời gian NTP lùi về 1 giờ; đồng thời bơm sự kiện `EVT_DEVICE_OFFLINE_ALERT` cho `temp_01` khi `relay_2` đang chạy.
  5. Bơm tiếp nhiệt độ 20.0°C trong vòng < 300 giây sau sự cố offline.
- **Expected:**
  1. Tại 30.2°C: Rule 1 kích hoạt BẬT (`relay_1` = "ON"); Deadband 30.1°C và 29.8°C không phát thêm event; tại 28.4°C kích hoạt TẮT ("OFF").
  2. Tại 21.8°C: Rule 2 kích hoạt BẬT (`relay_2` = "ON"); tại 23.6°C kích hoạt TẮT ("OFF").
  3. Đồng bộ hai chiều: Khi nhận lệnh bật thủ công, `current_state` của Rule 1 tự động chuyển sang `true`; khi nhiệt độ hạ xuống 28.0°C, Rule 1 phát hiện `current_state == true` và phát lệnh TẮT chính xác.
  4. Monotonic Clock & Offline Alert: Bước nhảy lùi giờ NTP không làm tê liệt logic timer; khi nhận `EVT_DEVICE_OFFLINE_ALERT`, Rule 2 phát lệnh TẮT khẩn cấp `relay_2` ngay lập tức và thiết lập khóa `lockout_until_mono_us` = 300s.
  5. Trong 300s khóa an toàn: Khi nhiệt độ 20.0°C, Rule 2 bị chặn hoàn toàn, không bật lại máy sưởi.
- **Timing / tolerance:** Thời gian đánh giá mỗi bản tin telemetry $\le 1\text{ms}$; thời gian phản hồi ngắt offline $\le 5\text{ms}$.
- **Prohibited:** Bị tê liệt timer khi NTP đổi giờ; không tắt được thiết bị sau khi bật thủ công; sưởi tự bật lại trong 300s lockout; áp dwell time 180s cho quạt hoặc thanh sưởi điện trở thường.
- **Operator instructions:**
  1. Chạy test tự động: `pytest tests/host/test_rule_engine.py -v`.
  2. Xác nhận 100% assertions kiểm tra Hysteresis, Bidirectional Sync, Monotonic Timing và Offline Fail-safe đều PASS.

---

### 4.8 Module: Scheduler (`scheduler.h/.c`)

#### 4.8.1 Mô tả
Quét `/storage/schedule.json` mỗi phút, đối chiếu với giờ RTC. Xử lý CẢ HAI loại: automation (→ rule engine) và alarm (→ display). Chạy hoàn toàn offline.

#### 4.8.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| SC-01 | Poll mỗi 5s, detect đổi phút | Không dùng `vTaskDelay(60000)` — tránh trôi thời gian (drift) |
| SC-02 | Chống bắn trùng (Rollover Guard) | `s_last_triggered_minute` — mỗi phút chỉ trigger đúng 1 lần |
| SC-03 | Trigger automation | type="automation" → post `EVT_SCHEDULE_AUTOMATION_TRIGGERED` |
| SC-04 | Trigger alarm | type="alarm"/"personal_event" → post `EVT_SCHEDULE_ALARM_TRIGGERED` |
| SC-05 | Reload schedule | Handler `EVT_CONFIG_UPDATED_FROM_CLOUD` (cờ `CONFIG_TYPE_SCHEDULE`) → nạp lại an toàn |
| SC-06 | Khóa giờ không hợp lệ (RTC Guard) | Kiểm tra `rtc_is_valid()`, tạm dừng kích hoạt khi chưa có giờ tin cậy (Epoch < 2020) |
| SC-07 | Bù lịch khi Time Jump | Quét bù (Catch-up Scan) tối đa 15 phút khi NTP nhảy thời gian về phía trước |
| SC-08 | Quản lý Snooze (+9 phút) | Hỗ trợ hoãn báo thức 9 phút; tự động reo lại sau 9 phút nếu chưa dismiss |
| SC-09 | Chống reo chuông quá khứ (No Retroactive Ring) | Khi khởi động, khởi tạo `last_checked_ts = rtc_get_time()`, không kích hoạt lịch của giờ đã qua |

#### 4.8.3 API

```c
void scheduler_init(void);
// Hành vi: Register handler EVT_CONFIG_UPDATED_FROM_CLOUD
//          Tạo scheduler_task (Core 0, priority 4, stack 4096)

void scheduler_reload(void);
// Hành vi: Đọc lại /storage/schedule.json và nạp cấu hình mới vào RAM.

void scheduler_snooze_alarm(int schedule_id, int minutes);
// Hành vi: Đặt trạng thái is_snoozed = true, tính snooze_until_epoch = now + minutes * 60 (mặc định 9m)
//          Tắt còi buzzer tạm thời, overlay OLED hiển thị "SNOOZE 9m"

void scheduler_dismiss_alarm(int schedule_id);
// Hành vi: Tắt báo thức triệt để trong ngày, reset trạng thái runtime alarm
```

#### 4.8.4 Schedule Entry & Active Alarm Data Model

```c
typedef struct {
    int  id;
    char type[16];           // "automation" | "alarm" | "personal_event"
    int  hour;               // 0-23
    int  minute;             // 0-59
    bool repeat_days[7];     // [CN, T2, T3, T4, T5, T6, T7] — cho lịch lặp
    char one_time_date[11];  // "YYYY-MM-DD" hoặc rỗng "" — cho sự kiện 1 lần
    bool enabled;

    // Cho automation:
    char target_device_id[32];
    char action_command[32];

    // Cho alarm/personal_event:
    char title[64];          // VD: "Dậy đi học"
    bool buzzer;             // bật buzzer khi trigger?
} schedule_entry_t;

// Runtime state cho báo thức đang hoạt động hoặc tạm hoãn (Snooze)
typedef struct {
    int      schedule_id;
    bool     is_active;
    bool     is_snoozed;
    uint32_t snooze_until_epoch; // Epoch thời điểm chuông reo lại sau Snooze
} active_alarm_runtime_t;
```

> **One-time vs Recurring:**
> - Nếu `one_time_date` khác rỗng (VD: "2026-08-20"): scheduler so khớp **ngày cụ thể** + giờ:phút. Sau khi trigger xong, Backend tự set `enabled=false` và push config mới về Gateway.
> - Nếu `one_time_date` rỗng: scheduler so khớp **thứ trong tuần** qua `repeat_days[]` như cũ.
> - Hai kiểu này **không cùng tồn tại** trong 1 entry — nếu có `one_time_date` thì `repeat_days` bị bỏ qua.

#### 4.8.5 Scheduler Task Flow & Các cơ chế bảo vệ

```
scheduler_task (vòng lặp vô hạn)
    │
    loop:
    │
    ├── 1. Kiểm tra rtc_is_valid():
    │     ├── false? ──► Log warning "[SCHED] RTC invalid, skipping trigger", vTaskDelay(5000ms), goto loop
    │     └── true  ──► Tiếp tục (Nếu vừa chuyển sang true lúc boot: last_checked_ts = rtc_get_time(), NO retroactive ring)
    │
    ├── 2. Kiểm tra Alarm đang Snooze:
    │     └── Nếu có alarm đang is_snoozed VÀ now >= snooze_until_epoch:
    │           ├── is_snoozed = false, is_active = true
    │           └── post EVT_SCHEDULE_ALARM_TRIGGERED (Reo chuông lại sau 9 phút)
    │
    ├── 3. Phát hiện bước nhảy thời gian (Time Jump Detection):
    │     ├── Nếu now > last_checked_ts + 65s (NTP nhảy giờ tới trước):
    │     │     └── Thực hiện Catch-up Scan (quét ngược tối đa 15 phút bỏ lỡ, chỉ trigger 1 lần gần nhất)
    │     └── last_checked_ts = now
    │
    ├── 4. now = rtc_get_time()
    ├── localtime_r(&now, &t)
    ├── cur_min = t.tm_hour * 60 + t.tm_min
    │
    ├── cur_min == s_last_triggered_minute?
    │     ├── YES → bỏ qua (đã xử lý phút này rồi)
    │     └── NO  → phút MỚI
    │           ├── s_last_triggered_minute = cur_min
    │           └── check_and_trigger(&t)
    │                 │
    │                 ├── Duyệt entries[]:
    │                 │     ├── entry.enabled == false? ──► skip
    │                 │     ├── entry.hour != t.tm_hour || entry.minute != t.tm_min? ──► skip
    │                 │     │
    │                 ├── Khớp ngày/thứ (one-time vs repeat):
    │                 │     ├── Khớp! ──► Phát EVT_SCHEDULE_AUTOMATION_TRIGGERED hoặc EVT_SCHEDULE_ALARM_TRIGGERED
    │                 │     └── Không khớp ──► skip
    │                 └── tiếp entry sau
    │
    ├── vTaskDelay(5000ms)  ← poll ngắn, không drift
    └── goto loop
```

#### 4.8.6 Verification Contract — Scheduler

**Verification Contract — FR-SCHED-01: Kiểm tra lịch biểu, Chống bắn trùng, Snooze +9m & RTC Guard**
- **Tier:** host
- **Preconditions:** Nạp lịch biểu gồm 1 automation lúc 07:00 (Bật đèn phòng) và 1 báo thức lúc 06:30 (Dậy đi học).
- **Stimulus:**
  1. Giả lập `rtc_is_valid() == false` lúc boot: Đồng hồ chạy qua 06:30 và 07:00.
  2. Bật `rtc_is_valid() == true` tại mốc 07:15: Kiểm tra xem các sự kiện 06:30 và 07:00 có bị reo bù giật lùi không.
  3. Cho đồng hồ chạy từ 06:29:50 đến 06:30:10 (kích hoạt báo thức 06:30).
  4. Bấm Snooze Nút 1 (+9 phút): Đồng hồ chạy tiếp từ 06:30 đến 06:39:10.
- **Expected:**
  1. Khi RTC chưa hợp lệ: Tuyệt đối không phát sinh sự kiện `EVT_SCHEDULE_*`.
  2. No Retroactive Ring: Khi boot tại 07:15, các sự kiện trong quá khứ không bị kích hoạt reo nhầm.
  3. Đúng mốc 06:30, phát sinh đúng 1 sự kiện `EVT_SCHEDULE_ALARM_TRIGGERED`. Trong suốt phút 06:30 không lặp lại (Rollover Guard).
  4. Khi Snooze: Còi dừng kêu; tại đúng mốc 06:39 (+9m), chuông báo thức tự động reo lại lần 2 chính xác.
- **Timing / tolerance:** Sai số kích hoạt trong vòng $\le 5\text{s}$ so với đầu phút (do chu kỳ poll 5s).
- **Prohibited:** Báo thức reo khi RTC chưa có giờ hợp lệ; reo bù sự kiện đã qua lúc boot; bắn trùng nhiều lần trong cùng một phút; quên chuông sau khi Snooze.
- **Operator instructions:**
  1. Chạy test tự động: `pytest tests/host/test_scheduler.py -v`.
  2. Xác nhận 100% assertions về RTC Guard, Rollover Guard, Catch-up Scan và Snooze +9m đều PASS.

---

### 4.9 Module: Display (`display.h/.c`)

#### 4.9.1 Mô tả
OLED SSD1327 + nút bấm + buzzer. Module hiển thị dữ liệu từ hệ thống và tiếp nhận ngắt nút bấm chuyển trang / tắt hoãn báo thức.

#### 4.9.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| DI-01 | Hiển thị 3 màn hình luân phiên | Clock/Temp ↔ Upcoming Event ↔ Device Status |
| DI-02 | Auto-refresh clock mỗi giây | `display_task` gọi `display_render()` trực tiếp |
| DI-03 | Chuyển màn hình khi bấm nút | Handler `EVT_BUTTON_SCREEN_NEXT` |
| DI-04 | Hiển thị báo thức + bật buzzer | Handler `EVT_SCHEDULE_ALARM_TRIGGERED` |
| DI-05 | Tắt / Snooze báo thức | Xử lý Direct-to-Task ISR, tắt buzzer và post `EVT_BUTTON_ALARM_STOP` |
| DI-06 | Cập nhật khi module khác yêu cầu | Handler `EVT_DISPLAY_UPDATE_REQUEST` |

#### 4.9.3 Màn hình OLED

```
Screen 0: CLOCK_TEMP               Screen 1: UPCOMING_EVENT
┌────────────────────┐             ┌────────────────────┐
│    14:52   28.5°C  │             │  Sắp tới:          │
│  Thứ 5, 14/08/2026 │             │  ⏰ 15:30 Bật đèn  │
│                    │             │  🔔 06:30 Dậy học   │
│  ● Wi-Fi  ● MQTT  │             │                    │
└────────────────────┘             └────────────────────┘

Screen 2: DEVICE_STATUS             Screen ALARM (overlay):
┌────────────────────┐             ┌────────────────────┐
│  Thiết bị:         │             │    ⚠️ BÁO THỨC     │
│  💡 Đèn phòng  ON  │             │                    │
│  🌡 Sensor    28.5°│             │    06:30           │
│  🔌 Quạt     OFF  │             │   Dậy đi học       │
└────────────────────┘             │  [Bấm nút tắt]    │
                                   └────────────────────┘
```

#### 4.9.4 Nút bấm GPIO & Trải nghiệm người dùng (UX)

Hệ thống trang bị 2 nút bấm vật lý với hành vi phân định rõ ràng giữa lúc bình thường và lúc chuông báo thức đang kêu:

| Nút bấm | Chân GPIO | Hành vi khi chuông báo thức đang kêu | Hành vi khi hoạt động bình thường |
|---|:---:|---|---|
| **Nút 1 (`BTN_BOOT`)** | **GPIO 0** | **Nhấn nhanh (< 2s): SNOOZE (+9 phút)**<br>Còi tắt ngay, OLED hiển thị `SNOOZE 9m`, Scheduler tự kích hoạt reo lại sau đúng 9 phút. | **Nhấn giữ ≥ 5s: Reset Wi-Fi SoftAP**<br>Xóa cấu hình Wi-Fi trong NVS và mở lại Captive Portal. (Chỉ cho phép khi chuông KHÔNG kêu). |
| **Nút 2 (`BTN_SCREEN_NEXT`)** | **GPIO 4** | **Nhấn nhanh (< 500ms): TẮT CHUÔNG NGAY (DISMISS)**<br>Còi tắt ngay lập tức, tắt chuông triệt để, OLED trở về màn hình chính, post `EVT_BUTTON_ALARM_STOP`. | **Nhấn nhanh (< 500ms): Chuyển màn hình OLED**<br>Luân phiên: Đồng hồ ↔ Lịch biểu ↔ Trạng thái Rơ-le ↔ Thông tin hệ thống (Mã Claim). |

> **Ưu điểm thiết kế chống nhầm lẫn:**
> - Tắt chuông hoàn toàn chỉ cần 1 cú click nhanh (< 500ms) vào Nút 2 (`GPIO 4`), khớp tuyệt đối với tiêu chí nghiệm thu **M13** và **UC-04**.
> - Không yêu cầu giữ nút 2 giây để tắt chuông, triệt tiêu hoàn toàn rủi ro người dùng ngủ ngáy giữ quá 5 giây làm vô tình xóa cấu hình Wi-Fi!

#### 4.9.5 Xử lý Tắt và Hoãn Báo Thức (Tuân thủ Event Bus Backbone)

Toàn bộ quy trình phản hồi nút bấm tuân thủ nghiêm ngặt kiến trúc bất đồng bộ không gây nghẽn:

```c
// display.c — Phản hồi ngắt nút bấm (Direct-to-Task Notification):
void display_handle_alarm_button_press(bool is_dismiss) {
    if (!s_alarm_active) return;
    
    // 1. Tắt ngay còi buzzer phần cứng (< 50ms)
    buzzer_stop();
    
    if (is_dismiss) {
        // Tắt chuông hoàn toàn (Nút 2)
        s_alarm_active = false;
        scheduler_dismiss_alarm(s_current_alarm_id);
    } else {
        // Hoãn báo thức +9 phút (Nút 1)
        s_alarm_active = false;
        scheduler_snooze_alarm(s_current_alarm_id, 9);
    }
    
    // 2. Post sự kiện lên Event Bus để sd_log_task ghi audit log và uplink_task gửi MQTT
    event_bus_post(EVT_BUTTON_ALARM_STOP, NULL, 0, false);
}
```
*Không gọi trực tiếp hàm I/O storage, đảm bảo phân tách rõ ràng trách nhiệm giữa các module.*

#### 4.9.6 Hardware Interface

| Thành phần | Chip/Type | Giao tiếp | Ghi chú |
|---|---|---|---|
| OLED | SSD1327 | I2C hoặc SPI | 128x128 grayscale, thư viện u8g2 |
| Buzzer | Passive buzzer | GPIO PWM (LEDC) | Cho báo thức |
| Nút bấm | Mechanical switch | GPIO + ngắt cạnh xuống | Có debounce phần cứng hoặc software |

#### 4.9.7 Verification Contract — Display & Buttons

**Verification Contract — FR-DISP-01: Phản hồi nút bấm ngắt còi báo thức < 500ms (Direct-to-Task ISR)**
- **Tier:** target
- **Preconditions:** Gateway đang trong trạng thái kích hoạt báo thức (`s_alarm_active == true`, còi buzzer kêu ngắt quãng 2.7kHz, màn hình OLED SSD1327 hiển thị overlay `ALARM`).
- **Stimulus:**
  1. Nhấn Nút 1 (`BTN_BOOT` / GPIO 0) nhanh < 2 giây.
  2. Kích hoạt báo thức lại, sau đó nhấn Nút 2 (`BTN_SCREEN_NEXT` / GPIO 4) nhanh < 500ms.
- **Expected:**
  1. Khi nhấn Nút 1 (GPIO 0): Còi buzzer ngừng kêu ngay lập tức; màn hình OLED hiển thị `SNOOZE 9m`; Scheduler tự động dời chuông sau 9 phút. Cấu hình Wi-Fi NVS không bị xóa.
  2. Khi nhấn Nút 2 (GPIO 4): Direct-to-Task Notification (`vTaskNotifyGiveFromISR`) gửi thẳng đến `display_task`, còi buzzer tắt hoàn toàn trong thời gian $\le 500\text{ms}$; OLED trở về màn hình chính; ghi bản tin nhật ký `alarm_stopped`.
- **Timing / tolerance:** Thời gian trễ từ thời điểm nhấn Nút 2 đến khi còi câm $\le 500\text{ms}$ (đáp ứng nghiệm thu M13 và UC-04).
- **Prohibited:** Nút bấm bị lag/đơ do Event Bus bị đầy; người dùng bấm tắt chuông nhưng vô tình xóa Wi-Fi; còi tiếp tục kêu sau khi bấm nút.
- **Operator instructions:**
  1. Dùng lệnh CLI đặt báo thức thử: `schedule add 99 alarm 12:00`.
  2. Đặt giờ giả lập hoặc đợi chuông reo: xác nhận còi kêu và OLED hiển thị overlay ALARM.
  3. Bấm Nút 2 (GPIO 4): Xác nhận còi tắt tức thì, màn hình hiển thị bình thường.

---

### 4.10 Module: Bộ Điều Khiển Relay & Cơ Cấu Chấp Hành (`relay_controller.h/.c`)

#### 4.10.1 Mô tả
Quản lý trực tiếp 4 kênh Relay vật lý trên bo mạch (GPIO 15, 16, 17, 18). Tiếp nhận các yêu cầu điều khiển từ Rule Engine, Scheduler, Web Dashboard hoặc MQTT Cloud Downlink; bắt buộc thi hành cơ chế **Khóa liên động an toàn phần cứng (Interlocking)**, bảo vệ chống cháy nổ nhiệt và sinh phản hồi ACK (`command_ack_t`).

#### 4.10.2 Bảng phân bổ kênh, Định danh chuẩn & Ma trận an toàn Fail-Safe (Monotonic Clock)

| Kênh Relay | Chân GPIO | Tên định danh chuẩn | Tên định danh tương thích (Alias) | Tải điều khiển | Giới hạn thời gian bật liên tục tối đa (Monotonic Clock) | Hành vi khi cảm biến lỗi / Timeout (>60s) |
|---|:---:|---|---|---|:---:|---|
| **RELAY_1** | **GPIO 15** | `relay_1` | `fan_01`, `fan_relay_01` | Quạt làm mát (Fan) | 4 giờ (240 phút) | TẮT (OFF) hoặc Giữ BẬT tối đa 4h |
| **RELAY_2** | **GPIO 16** | `relay_2` | `heater_01`, `heater_relay_01` | Máy sưởi nhiệt (Heater) | **30 phút** (Emergency Cutoff) | **CƯỠNG CHẾ TẮT NGAY LẬP TỨC (0ms trễ, OFF)** |
| **RELAY_3** | **GPIO 17** | `relay_3` | `dehum_01`, `dehumidifier_01` | Máy hút ẩm (Dehum - Compressor) | 60 phút | TẮT (OFF) |
| **RELAY_4** | **GPIO 18** | `relay_4` | `aux_01`, `light_01` | Đèn / Thiết bị phụ trợ | Không giới hạn | Giữ nguyên trạng thái cuối |

*Toàn bộ thời gian bảo vệ (cooldown, lockout 300s, thời gian bật tối đa 30m/4h, dwell 180s máy nén) được tính toán bằng Monotonic Clock (`esp_timer_get_time()`), loại trừ 100% rủi ro do điều chỉnh múi giờ hoặc bước nhảy NTP.*

#### 4.10.3 Khóa liên động an toàn (Interlocking), Ưu tiên giải nhiệt & Khóa chu kỳ ngắn (Anti-Short-Cycle)

Để loại trừ triệt để nguy cơ cháy nổ do xung đột lệnh từ các nguồn khác nhau:
1. **Khóa liên động độc quyền (Interlocking Rule):** Quạt làm mát (`relay_1`) và Máy sưởi nhiệt (`relay_2`) **tuyệt đối không bao giờ được phép BẬT đồng thời**.
2. **Quy tắc thực thi bất đối xứng trong `relay_controller_set_state()`:**
   - **Lệnh BẬT Sưởi khi Quạt đang ON $\rightarrow$ TỪ CHỐI (REJECT):**
     Nếu `relay_1` đang BẬT: Từ chối thực thi lệnh bật `relay_2`.
     Bộ điều khiển trả về Command ACK lỗi: `success = false`, `error_code = 3` (`ERR_RELAY_INTERLOCK_CONFLICT`), `message = "Cannot turn ON Heater while Fan is running"`.
     Giữ nguyên trạng thái Quạt, tuyệt đối **không tự ý tắt quạt** của người dùng.
   - **Lệnh BẬT Quạt khi Sưởi đang ON $\rightarrow$ ƯU TIÊN GIẢI NHIỆT (Cooling Safety Override):**
     Làm mát và chống quá nhiệt là ưu tiên an toàn cao nhất trong nhà.
     Nếu `relay_2` đang BẬT mà có lệnh BẬT `relay_1`: Cưỡng chế ngắt ngay lập tức `relay_2 = OFF` (0ms trễ, lý do `"COOLING_OVERRIDE"`), phát `EVT_RELAY_STATE_CHANGED`, sau đó đóng rơ-le Quạt `relay_1 = ON`.
3. **Bỏ qua Cooldown khi TẮT (Immediate Shutoff):**
   Mọi lệnh hoặc điều kiện bảo vệ yêu cầu TẮT rơ-le (đặc biệt là Máy sưởi `relay_2`) **bắt buộc thực thi ngay lập tức (0ms trễ, bypass toàn bộ cooldown)**. Cooldown chỉ áp dụng cho chiều BẬT để chống rung tiếp điểm cơ.
4. **Phân định phạm vi Khóa chu kỳ ngắn (Anti-Short-Cycle Dwell Time):**
   - **CHỈ áp dụng Dwell Time 180s cho Relay 3 (Máy hút ẩm / Tải máy nén lạnh - Compressor):** Khi Relay 3 vừa ngắt về OFF, bộ điều khiển kích hoạt timer monotonic khóa tối thiểu **180 giây** trước khi cho phép đóng lại kênh đó. Lệnh kích hoạt trong thời gian này bị từ chối với mã lỗi `ERR_RELAY_DWELL_LOCKED` (code 4) nhằm chống kẹt áp suất khí gas máy nén.
   - **Relay 1 (Quạt) & Relay 4 (Aux):** Bỏ Dwell Time 180s; chỉ áp dụng thời gian chống rung cơ học (Mechanical Debounce $\le 1\text{s}$).
   - **Relay 2 (Sưởi điện trở):** Bỏ Dwell Time 180s của chu kỳ thường; chỉ kích hoạt **Safety Lockout 300 giây (5 phút)** khi có sự cố ngắt khẩn cấp (`SAFETY_CUTOFF`, `SENSOR_OFFLINE`, chạy quá 30 phút).

#### 4.10.4 Đồng bộ trạng thái Rule Engine & Thời gian khóa an toàn (Lockout)

- Khi một kênh Relay bị cưỡng chế TẮT do vượt quá thời gian tối đa (Máy sưởi quá 30 phút) hoặc do cảm biến mất tín hiệu (>60s) / báo lỗi:
  - `relay_controller` xuất mức LOW ra GPIO ngay lập tức và phát sự kiện `EVT_RELAY_STATE_CHANGED` (`state = false`, `reason = "SAFETY_CUTOFF"` hoặc `"SENSOR_OFFLINE"`, kèm `monotonic_time_us`).
  - `rule_engine` bắt sự kiện này và **đồng bộ lại trạng thái nội bộ**: gán `current_state = false`.
  - Thiết lập **thời gian khóa bảo vệ (Safety Lockout = 300s / 5 phút)**: `lockout_until_mono_us = esp_timer_get_time() + 300000000ULL`. Rule Engine không cho phép bất kỳ quy tắc tự động nào kích hoạt bật lại Máy sưởi trong vòng 5 phút tiếp theo để thanh nhiệt hoàn toàn nguội.

#### 4.10.5 C API (`relay_controller.h`)

```c
void relay_gpio_init(void);
// GIAI ĐOẠN 1 (Boot Step 2 — TRƯỚC Event Bus):
// Cấu hình GPIO 15..18 output mức LOW an toàn, kích hoạt trở kéo xuống 10kΩ phần cứng.
// Triệt tiêu 100% xung nổi chân rơ-le lúc vừa cấp nguồn.

void relay_controller_init(void);
// GIAI ĐOẠN 2 (Boot Step 4b — SAU Event Bus):
// Đăng ký nhận EVT_DEVICE_COMMAND_REQUEST, EVT_RULE_ACTION_REQUEST,
// EVT_SENSOR_VALUE_INVALID, EVT_DEVICE_OFFLINE_ALERT. Khởi tạo monotonic timers.

esp_err_t relay_controller_set_state(const char *target_id, bool state, const char *cmd_id, const char *source);
// Hành vi: Tra cứu alias (fan_01 -> relay_1, heater_01 -> relay_2, dehum_01 -> relay_3)
//          Kiểm tra Interlock (Heater vs Fan) theo quy tắc bất đối xứng
//          Kiểm tra Dwell Time 180s (CHỈ áp dụng cho relay_3 máy nén)
//          Ghi GPIO vật lý, cập nhật monotonic timestamp đếm thời gian bật liên tục
//          Phát EVT_COMMAND_RESPONSE gửi ACK về Cloud/MQTT

bool relay_controller_get_state(int relay_id);
// Hành vi: Đọc mức logic hiện tại của rơ-le (1..4)

void relay_controller_emergency_shutdown_all(void);
// Hành vi: Ngắt khẩn cấp toàn bộ 4 rơ-le về mức LOW (dùng trong panic/shutdown)
```

#### 4.10.6 Verification Contract — Relay Controller

**Verification Contract — FR-RELAY-01: Khóa liên động bất đối xứng, Dwell máy nén & Ngắt khẩn cấp an toàn**
- **Tier:** host
- **Preconditions:** Khởi tạo `relay_controller`, toàn bộ 4 kênh GPIO 15..18 ở mức LOW.
- **Stimulus:**
  1. Gửi lệnh BẬT `fan_01` (`relay_1 = ON`). Khi `relay_1` đang ON, gửi lệnh BẬT `heater_01` (`relay_2 = ON`).
  2. Tắt `relay_1`, bật `relay_2 = ON`. Khi `relay_2` đang ON, gửi lệnh BẬT `fan_01` (`relay_1 = ON`).
  3. Giả lập duy trì `relay_2` BẬT liên tục trong 31 phút (dùng monotonic clock).
  4. Bơm sự kiện `EVT_SENSOR_VALUE_INVALID` hoặc `EVT_DEVICE_OFFLINE_ALERT` khi `relay_2` đang BẬT.
  5. Tắt `relay_3` (Máy hút ẩm), ngay sau đó (sau 5 giây) gửi lệnh BẬT lại `relay_3`.
  6. Tắt `relay_1` (Quạt), sau 2 giây gửi lệnh BẬT lại `relay_1`.
- **Expected:**
  1. Interlock Reject: Khi Quạt đang ON, lệnh bật Sưởi bị TỪ CHỐI (`success = false`, `error_code = 3`, `ERR_RELAY_INTERLOCK_CONFLICT`), Quạt vẫn tiếp tục chạy ON bình thường.
  2. Cooling Safety Override: Khi Sưởi đang ON, lệnh bật Quạt được CHẤP NHẬN; Sưởi bị cưỡng chế ngắt tức khắc (`relay_2 = OFF`, reason `"COOLING_OVERRIDE"`), sau đó Quạt bật ON.
  3. Emergency Cutoff: Sau đúng 30 phút monotonic, `relay_2` tự động ngắt về LOW, phát `EVT_RELAY_STATE_CHANGED` (`reason = "SAFETY_CUTOFF"`).
  4. Fail-Safe Offline: Khi cảm biến lỗi / offline, `relay_2` ngắt về LOW ngay lập tức (0ms trễ).
  5. Anti-Short-Cycle Máy nén: Lệnh BẬT lại `relay_3` sau 5s bị từ chối (`ERR_RELAY_DWELL_LOCKED`, error_code = 4) do chưa đủ 180s dwell time.
  6. Quạt không bị dwell: Lệnh BẬT lại `relay_1` sau 2s được chấp nhận ngay lập tức (vượt qua debounce 1s, không bị kẹt 180s).
- **Timing / tolerance:** Thời gian phản hồi ngắt an toàn $\le 50\text{ms}$; sai số timer monotonic $\le 1\text{s}$.
- **Prohibited:** Relay 1 và Relay 2 cùng ON tại bất kỳ thời điểm nào; tự ý tắt quạt của người dùng khi có lệnh bật sưởi; Relay 2 tiếp tục đóng khi mất cảm biến; áp dwell 180s cho quạt hoặc sưởi thường.
- **Operator instructions:**
  1. Chạy test tự động: `pytest tests/host/test_relay_controller.py -v`.
  2. Xác nhận 100% assertions về Interlock Asymmetry, Cooling Override, Monotonic Safety Cutoff và Selective Dwell Time đều PASS.

---

### 4.11 Module: SD Card Storage & Store-and-Forward Queue (`sd_logger.h/.c`)

#### 4.11.1 Mô tả
Chịu trách nhiệm toàn bộ thao tác I/O với thẻ nhớ MicroSD qua chuẩn SPI và hệ thống tệp FATFS (`esp_vfs_fat_sdspi_mount`). Module hoạt động hoàn toàn bất đồng bộ thông qua `sd_log_task` trên Core 1, tiếp nhận bản tin qua `s_sd_log_queue` (30 slots) để đảm bảo không làm nghẽn các tác vụ thời gian thực. Cung cấp chức năng ghi log lịch sử, log sự cố (crash log) và cơ chế bộ đệm ngoại tuyến FIFO (**Store-and-Forward**) giúp dữ liệu không bị thất thoát khi mất kết nối mạng dài ngày.

#### 4.11.2 Cấu trúc thư mục thẻ nhớ SD
```
/sdcard/
├── config.json                 # Cấu hình tại chỗ (Config Manager tự nạp khi boot)
├── data/
│   ├── telemetry/
│   │   └── YYYY-MM-DD.csv      # Telemetry cảm biến theo ngày: timestamp,device_id,val,valid
│   └── sync_queue.bin          # File nhị phân FIFO lưu trữ bản tin chưa gửi lên Cloud
└── logs/
    ├── system_YYYY-MM.log      # Log vận hành hệ thống (INFO, WARN, ERROR)
    └── crash.log               # Dump nhật ký sự cố, task treo, panic stacktrace
```

#### 4.11.3 Cơ chế Store-and-Forward Sync Queue (`sync_queue.bin`) & Circular Ring Buffer

- **Định dạng file nhị phân:** Cung cấp định dạng byte packing cố định đảm bảo đồng nhất 100% giữa ESP32 (Xtensa 32-bit) và Host test (x86_64 GCC):
  ```c
  typedef struct __attribute__((packed)) {
      uint32_t magic;          // 0x53594E43 ("SYNC")
      uint32_t write_offset;   // Con trỏ vị trí byte ghi tiếp theo
      uint32_t read_offset;    // Con trỏ vị trí byte đã gửi thành công lên server
      uint32_t total_records;  // Tổng số bản ghi tích lũy
      uint32_t unacked_count;  // Số bản ghi đang chờ gửi
      uint32_t crc32;          // Checksum bảo vệ header
  } sync_queue_header_t;       // Kích thước chuẩn cố định: đúng 24 bytes

  _Static_assert(sizeof(sync_queue_header_t) == 24, "sync_queue_header_t must be exactly 24 bytes");

  // Cấu trúc bản ghi nhị phân mở rộng cố định 172 bytes (chuẩn "<IBBH32s128sI" khớp test_sync_queue.py):
  typedef struct __attribute__((packed)) {
      uint32_t timestamp;      // 4 bytes: Unix epoch timestamp
      uint8_t  record_type;    // 1 byte: 0x01=telemetry, 0x02=event_log, 0x03=command_ack
      uint8_t  flags;          // 1 byte: bit 0: QoS1, bit 1: compressed
      uint16_t payload_len;    // 2 bytes: chiều dài dữ liệu chuỗi trong payload (1..128)
      char     device_id[32];  // 32 bytes: Device ID hoặc target device ID (null-terminated)
      char     payload[128];   // 128 bytes: Chuỗi JSON chứa telemetry, event log hoặc command ack (UUID 36 ký tự)
      uint32_t crc32;          // 4 bytes: CRC32 checksum toàn vẹn của 168 bytes đầu
  } sync_queue_record_t;       // Tổng kích thước chuẩn cố định: đúng 172 bytes

  _Static_assert(sizeof(sync_queue_record_t) == 172, "sync_queue_record_t must be exactly 172 bytes");
  ```
- **Hành vi khi Offline & Giới hạn trần dung lượng (Circular FIFO):**
  - Khi `uplink_task` mất mạng (Wi-Fi hoặc MQTT ngắt), mọi telemetry, event log và command ACK thay vì gửi mạng sẽ được đưa vào `s_sd_log_queue` để append vào cuối file `sync_queue.bin`.
  - **Giới hạn trần Circular FIFO Buffer:** Quy định `SYNC_QUEUE_MAX_RECORDS = 50000` bản ghi (~8.6MB dữ liệu). Nếu mất mạng dài ngày khiến queue chạm trần, con trỏ `read_offset` tự động tịnh tiến để ghi đè bản ghi cũ nhất, bảo đảm thẻ nhớ SD không bao giờ bị cạn kiệt dung lượng và không gây lỗi phân mảnh hệ thống tệp.
- **Hành vi khi Online trở lại & Điều tiết tốc độ bù (Backfill Rate Limiter):**
  1. `uplink_task` kết nối thành công tới MQTT Broker.
  2. Gửi tín hiệu kích hoạt tiến trình đồng bộ bù.
  3. `sd_log_task` đọc một lô (Batch) gồm **20 bản ghi** từ vị trí `read_offset`.
  4. **Điều tiết tốc độ (Rate Limiter):** Đóng gói batch thành JSON mảng và publish lên topic `gateway/{id}/telemetry/batch` với **QoS 1**, giãn cách tối thiểu **100ms giữa các batch** (duy trì tốc độ gửi bù từ **20 đến 50 records/giây**). Tốc độ này giúp không làm nghẽn băng thông Wi-Fi, không gây trễ các gói tin điều khiển tức thời của người dùng, và giữ cho bộ đệm outbox RAM của MQTT client luôn an toàn.
  5. Sau khi nhận đủ `PUBACK` cho cả batch từ Broker, `sd_log_task` cập nhật `read_offset` mới và gọi `f_sync()` để lưu con trỏ trạng thái xuống thẻ SD.
  6. Lặp lại cho tới khi `read_offset == write_offset` (hàng đợi rỗng hoàn toàn).

#### 4.11.4 Chống hỏng tệp khi mất điện đột ngột (Power Loss Safe)
- Không mở và đóng file liên tục trong vòng lặp nhỏ.
- Định kỳ gọi `f_sync()` sau mỗi **10 bản ghi** hoặc tối đa sau mỗi **5 giây** để ép flush cache từ RAM xuống sector vật lý của thẻ SD.
- Nếu Gateway mất điện đột ngột: Dữ liệu tối đa bị mất chỉ là trong 5 giây cuối cùng; cấu trúc bảng FAT và file `sync_queue.bin` không bị lỗi filesystem (corrupt).

#### 4.11.5 Verification Contracts — SD Card Logger

**Verification Contract — FR-SD-01: Ghi Telemetry CSV vào thẻ nhớ**
- **Tier:** target
- **Preconditions:** Thẻ nhớ MicroSD định dạng FAT32 được cắm vào khe thẻ (GPIO 11, 12, 13, 10).
- **Stimulus:** Khởi động hệ thống, để hệ thống thu thập dữ liệu cảm biến BLE và nội bộ trong 30 giây.
- **Expected:** Thẻ SD mount thành công, LED báo SD sáng (hoặc log xác nhận). Sau 30s, file `/sdcard/data/telemetry/YYYY-MM-DD.csv` được tạo ra chứa các dòng dữ liệu định dạng chuẩn: `timestamp,device_id,value,valid`.
- **Timing / tolerance:** Thời gian mount FATFS lúc boot $\le 300\text{ms}$; thời gian ghi mỗi bản ghi $\le 10\text{ms}$.
- **Prohibited:** Treo Core 0 khi thẻ nhớ trễ I/O; rò rỉ bộ đệm FATFS.
- **Operator instructions:**
  1. Cắm thẻ nhớ MicroSD 8GB/16GB vào Gateway.
  2. Khởi động thiết bị, quan sát log serial có dòng `[SD] FATFS mounted successfully. Free: XXX MB`.
  3. Sau 1 phút, tháo thẻ nhớ cắm vào PC, mở file CSV kiểm tra các dòng dữ liệu nhiệt độ/độ ẩm.

**Verification Contract — FR-SD-02: Store-and-Forward Offline Queue 172B & Điều tiết Backfill**
- **Tier:** host
- **Preconditions:** Thư viện struct `sync_queue_header_t` (24B) và `sync_queue_record_t` (172B) vượt qua `_Static_assert` compile-time; Broker MQTT đang hoạt động.
- **Stimulus:**
  1. Kiểm tra cấu trúc bộ nhớ packing `"<IBBH32s128sI"` qua test suite host.
  2. Tạo 100 bản ghi ngoại tuyến gồm cả 3 loại: telemetry (0x01), event_log (0x02) và command_ack (0x03 chứa UUID 36 bytes).
  3. Kích hoạt tiến trình gửi bù Backfill lên MQTT broker giả lập.
- **Expected:**
  1. Struct packing khớp chính xác 100% (Header 24B, Record 172B), không bị lỗi compiler padding.
  2. Tốc độ xuất bản bù được điều tiết chính xác trong khoảng **20 đến 50 records/giây** (batch size 20, interval $\ge 100\text{ms}$).
  3. Xuất bản lên đúng topic `gateway/{id}/telemetry/batch` với QoS 1.
  4. Con trỏ `read_offset` chỉ tịnh tiến khi nhận được `PUBACK`. Sau khi gửi hết 100 bản ghi, `unacked_count` trở về 0.
- **Timing / tolerance:** Tốc độ backfill: 20–50 records/s; sai số CRC32 = 0.
- **Prohibited:** Bản ghi bị mất thông tin command UUID do payload quá ngắn (< 128B); tràn RAM outbox; tốc độ backfill vượt quá 50 records/s gây nghẽn mạng; cập nhật `read_offset` trước khi có `PUBACK`.
- **Operator instructions:**
  1. Chạy test tự động: `pytest tests/host/test_sync_queue.py -v`.
  2. Xác nhận 100% assertions về Record Size 172B, Header 24B, CRC32, và Backfill Batching đều PASS.

---

### 4.12 Module: Config Manager & Cấu hình tại chỗ (`config_manager.h/.c`)

#### 4.12.1 Mô tả
Chịu trách nhiệm lưu trữ, quản lý và cập nhật toàn bộ tham số hoạt động của Gateway bao gồm: cấu hình mạng Wi-Fi, địa chỉ Server/Broker, múi giờ (POSIX ICT-7), danh sách ánh xạ cảm biến không dây BLE và thiết bị chấp hành, cùng các ngưỡng cảnh báo. Cung cấp cơ chế **cấu hình tại chỗ không phụ thuộc Cloud** thông qua 3 kênh: nạp tệp `config.json` từ thẻ nhớ Flash/SD, giao diện Dark Mode **Web Dashboard** (Port 80) và console **Serial CLI** tương tác qua cổng USB UART0 (115200 baud).

#### 4.12.2 Định dạng tệp `/storage/config.json`
```json
{
  "gateway_id": "gw_smartclock_01",
  "timezone": "ICT-7",
  "wifi": {
    "ssid": "Home_Net_2.4G",
    "password": "SecretPassword123"
  },
  "mqtt": {
    "broker_uri": "mqtt://backend.local:1883",
    "client_id": "gw_01",
    "telemetry_interval_s": 10
  },
  "ble_sensors": [
    {
      "device_id": "sensor_temp_01",
      "name": "Cảm biến nhiệt ẩm phòng ngủ",
      "mac_address": "A4:C1:38:12:34:56",
      "parser_type": "sig_181a"
    },
    {
      "device_id": "sensor_livingroom",
      "name": "Cảm biến phòng khách",
      "mac_address": "11:22:33:44:55:66",
      "parser_type": "bthome_v2"
    }
  ]
}
```

#### 4.12.3 Tập lệnh Serial CLI (UART0 Console, 115200 bps)
| Lệnh | Cú pháp | Chức năng |
|---|---|---|
| `help` | `help` | Hiển thị danh sách câu lệnh hỗ trợ |
| `status` | `status` | In trạng thái hệ thống, Uptime, Heap, Active & Alive Heartbeat Mask (`0x7F`) |
| `time` | `time` | In thời gian hiện hành (ICT-7) và epoch timestamp |
| `config show` | `config show` | In toàn bộ cấu hình đang chạy (Wi-Fi, MQTT Broker, Gateway ID, BLE Devices) |
| `config set wifi` | `config set wifi <ssid> <pass>` | Cập nhật SSID và mật khẩu Wi-Fi vào NVS |
| `config set mqtt` | `config set mqtt <uri>` | Cập nhật Broker URI lên Cloud |
| `ble status` | `ble status` | Hiển thị trạng thái NimBLE Scanner và danh sách cảm biến đã đăng ký |
| `ble reg` | `ble reg <device_id> [mac]` | Đăng ký cảm biến BLE mới và gán vào bảng ánh xạ |
| `ble test` | `ble test [val]` | Bơm dữ liệu telemetry BLE giả lập (mặc định 32.5°C) để kiểm tra Rule Engine |
| `rule list` / `rule add` | `rule add [id] <cond> <target> <cmd>` | Thêm/xem các quy tắc tự động hóa IF-THEN |
| `schedule list` / `add` | `schedule add [id] <alarm\|auto> <HH:MM>` | Thêm/xem lịch báo thức và lịch tự động hóa |
| `reboot` | `reboot` | Khởi động lại Gateway an toàn |

#### 4.12.4 Verification Contracts — Config Manager

**Verification Contract — FR-CFG-01: Nạp cấu hình tự động từ bộ nhớ lưu trữ**
- **Tier:** target
- **Preconditions:** Bộ nhớ Flash/SD chứa file `config.json` hợp lệ với 2 cảm biến BLE `sensor_temp_01` và `sensor_livingroom`.
- **Stimulus:** Khởi động lại Gateway.
- **Expected:** Gateway phát hiện `config.json`, nạp thành công vào NVS, cập nhật bảng ánh xạ cảm biến BLE. Log serial xuất hiện `[CFG] Loaded configuration successfully`.
- **Operator instructions:**
  1. Tạo file `config.json` và lưu vào thư mục lưu trữ `/storage/config.json`.
  2. Khởi động lại Gateway qua lệnh `reboot`.
  3. Gõ `config show` và `ble status` xác nhận danh sách cảm biến khớp hoàn toàn với cấu hình.

**Verification Contract — FR-CFG-02: Cấu hình qua Serial CLI và Đăng ký Cảm biến BLE**
- **Tier:** target
- **Preconditions:** Gateway kết nối với máy tính qua cổng USB Type-C (UART0).
- **Stimulus:** Mở terminal 115200 baud, gõ lệnh `ble reg sensor_balcony 22:33:44:55:66:77`.
- **Expected:** Gateway ghi nhận cảm biến mới vào bảng ánh xạ động trong RAM và NVS, in `OK: Registered BLE device 'sensor_balcony'`. Gõ `config show` in ra thông số hệ thống rõ ràng.
- **Operator instructions:**
  1. Mở terminal Serial Monitor ở baudrate 115200.
  2. Nhấn Enter thấy dấu nhắc lệnh `gateway>`.
  3. Gõ `ble reg sensor_balcony 22:33:44:55:66:77` và xác nhận kết quả thành công.
  4. Gõ `ble status` thấy cảm biến `sensor_balcony` xuất hiện trong danh sách giám sát.

---

### 4.13 Module: Task Supervisor & Watchdog (`supervisor.h/.c`)

#### 4.13.1 Mô tả
Module đóng vai trò "người giám sát độc lập" (Watchdog Supervisor) chịu trách nhiệm đảm bảo tính khả dụng 24/7 của toàn bộ hệ thống. Sử dụng kết hợp **Hardware Watchdog (WDT)** của SoC ESP32 và cơ chế **Heartbeat Bitmask** trên FreeRTOS để theo dõi từng task nghiệp vụ riêng lẻ. Nếu bất kỳ task nào (dù là task mạng, task BLE, task thẻ nhớ, task lịch biểu hay task hiển thị) bị rơi vào trạng thái treo (Deadlock, stuck ở hàm I/O blocking hoặc vòng lặp vô hạn), Supervisor sẽ phát hiện, ghi lại chẩn đoán lỗi vào vùng nhớ RTC Slow SRAM (`RTC_NOINIT_ATTR`) và kích hoạt reset an toàn.

#### 4.13.2 Bảng phân bổ Heartbeat Bitmask các Task (Chuẩn hóa 7 Task Production, Mask 0x7F)

| Bit | Tên Task | Cờ Heartbeat (`task_mask`) | Chu kỳ báo danh | Chu kỳ kiểm tra | Timeout tối đa |
|:---:|---|---|:---:|:---:|:---:|
| **0** | `event_dispatcher` | `TASK_HEARTBEAT_EVENT_BUS` (`1 << 0`) | Mỗi event / Timeout 1s | 1000ms | **15 giây** |
| **1** | `ble_scan_task` | `TASK_HEARTBEAT_BLE` (`1 << 1`) | 1000ms | 1000ms | **15 giây** |
| **2** | `uplink_task` | `TASK_HEARTBEAT_UPLINK` (`1 << 2`) | 1000ms | 1000ms | **15 giây** |
| **3** | `sd_log_task` | `TASK_HEARTBEAT_SD_LOGGER` (`1 << 3`) | Mỗi log ghi / Timeout 1s | 1000ms | **15 giây** |
| **4** | `scheduler_task` | `TASK_HEARTBEAT_SCHEDULER` (`1 << 4`) | 1000ms | 1000ms | **15 giây** |
| **5** | `display_task` | `TASK_HEARTBEAT_DISPLAY` (`1 << 5`) | 1000ms (1 FPS) | 1000ms | **15 giây** |
| **6** | `serial_cli_task` | `TASK_HEARTBEAT_CLI` (`1 << 6`) | UART read timeout 1s | 1000ms | **15 giây** |

- **Mặt nạ giám sát chuẩn Production (Active Mask):** `#define TASK_ACTIVE_MASK_PRODUCTION (0x7F)` (chính xác 7 task sản xuất, bits 0..6: `(1<<0)|...|(1<<6) = 127 = 0x7F`).
- **Cơ chế Mặt nạ Ngoại vi Động (Dynamic Peripheral Masking):**
  - Trong giai đoạn khởi động (`app_main`), hệ thống tự động kiểm tra phần cứng ngoại vi:
    - Nếu thẻ nhớ MicroSD không có hoặc mount thất bại $\rightarrow$ tự động bỏ cờ `TASK_HEARTBEAT_SD_LOGGER` (bit 3).
    - Nếu màn hình OLED I2C không phản hồi ACK $\rightarrow$ tự động bỏ cờ `TASK_HEARTBEAT_DISPLAY` (bit 5).
  - Hàm `supervisor_set_active_mask(mask)` cho phép hệ thống hoạt động tin cậy trên bo mạch trần (Bare-board bringup) mà không bị Task Watchdog reset nhầm. Khi ngoại vi được cắm lại, bit tương ứng tự động được bật lại.
- **Cơ chế Ngắt Vòng lặp Khởi động (Boot Loop Circuit Breaker):**
  - Lưu trữ chẩn đoán sự cố và đếm số lần crash liên tiếp trong vùng nhớ **RTC Slow SRAM (`RTC_NOINIT_ATTR`)**:
    ```c
    typedef struct {
        uint32_t magic;                // 0xDEADBEEF
        uint8_t  failed_task_bit;      // Bit task bị treo (0..6)
        char     task_name[16];        // Tên task (vd: "event_dispatcher")
        uint32_t free_heap;            // Dung lượng heap tại thời điểm treo
        uint32_t uptime_s;             // Thời gian hoạt động trước khi chết
        uint32_t consecutive_crashes; // Số lần crash liên tiếp
        uint32_t timestamp;            // UNIX epoch
    } rtc_crash_dump_t;
    ```
  - **Quy tắc Safe Mode:** Nếu `consecutive_crashes >= 3` trong vòng < 60s, hệ thống kích hoạt chế độ Cứu Hộ An Toàn:
    1. Bỏ qua việc nạp file cấu hình `config.json` và `rules.json` từ Flash (phòng trường hợp file hỏng gây crash).
    2. Nạp cấu hình mặc định (Hardcoded Safe-Defaults).
    3. Mở SoftAP cứu hộ `SmartClock-Recovery` và phát cảnh báo nhấp nháy LED trạng thái.
- **Ghi chẩn đoán sự cố vào RTC Slow Memory:**
  - Khi phát hiện bất kỳ task nào không báo nhịp tim quá **15 giây**:
  - In cảnh báo khẩn cấp ra cổng Serial Console UART0: `[SUP] CRITICAL: Task '%s' STALLED! Dumping to RTC Memory & restarting...`.
  - Gọi hàm khởi động lại an toàn: `esp_restart()`.
  - Khi boot lại: Kiểm tra magic `0xDEADBEEF`, in báo cáo chẩn đoán sự cố ra terminal, đồng bộ append vào `/sdcard/logs/crash.log` và xóa cờ magic.

#### 4.13.3 Chu trình giám sát & Xử lý sự cố
1. `supervisor_task` chạy ở mức ưu tiên cao nhất (**Priority 7**, Core 0), thức dậy mỗi **1000ms**. Định kỳ gọi `esp_task_wdt_reset()` để nuôi phần cứng TWDT của chip ESP32-S3.
2. Tất cả 7 task định kỳ báo danh qua hàm `supervisor_report_heartbeat(task_mask)`.
3. `serial_cli_task` sử dụng hàm đọc `uart_read_bytes` kèm timeout `pdMS_TO_TICKS(1000)` để đảm bảo vòng lặp không bị kẹt vô hạn khi không có người gõ phím.
4. `ble_scan_task` sử dụng khoảng delay 1 giây và định kỳ báo nhịp tim, loại bỏ hoàn toàn hiện tượng delay kéo dài gây reset nhầm.

#### 4.13.4 Verification Contracts — Task Supervisor

**Verification Contract — FR-SUP-01: Báo danh Heartbeat & Giám sát 7 Task Production**
- **Tier:** target
- **Preconditions:** Gateway nạp firmware đang hoạt động đầy đủ 7 task production (mask 0x7F).
- **Stimulus:** Giám sát thiết bị chạy liên tục trong 10 phút dưới tải thu nhận BLE và phát Wi-Fi.
- **Expected:** Supervisor task định kỳ kiểm tra và reset watchdog phần cứng. Không có thông báo lỗi hay reset bất thường; cờ của cả 7 task được set đều đặn mỗi chu kỳ ≤ 5s.
- **Timing / tolerance:** Kiểm tra nhịp tim mỗi 1000ms ± 50ms; timeout tối đa 15s.
- **Prohibited:** Reset ngẫu nhiên khi hệ thống đang xử lý tải bình thường; bỏ sót task không giám sát.
- **Operator instructions:**
  1. Khởi động thiết bị, mở serial monitor ở baudrate 115200.
  2. Quan sát log định kỳ: `[SUP] System healthy. Heap free: XXX KB, All tasks alive (mask=0x7F)`.
  3. Xác nhận sau 10 phút không có sự cố reboot.

**Verification Contract — FR-SUP-02: Bắt lỗi Task bị treo & Tự khôi phục an toàn**
- **Tier:** target
- **Preconditions:** Nạp firmware kích hoạt chế độ test chẩn đoán (Test Hook: lệnh `test freeze event_bus`).
- **Stimulus:** Gửi lệnh qua Serial CLI: `test freeze event_bus` (khiến `event_dispatcher_task` rơi vào vòng lặp vô hạn mà không báo heartbeat).
- **Expected:**
  1. Sau đúng 15 giây (± 1s), `supervisor_task` phát hiện thiếu cờ `TASK_HEARTBEAT_EVENT_BUS`.
  2. Ghi cảnh báo ra log Serial: `[SUP] CRITICAL: Task 'event_dispatcher' stalled > 15s! Flushing crash log...`.
  3. Chẩn đoán sự cố được lưu thành công vào RTC Slow Memory với mã lỗi `TASK_STALL_EVENT_BUS` (và đồng bộ append vào `/sdcard/logs/crash.log` sau khi reboot).
  4. Gateway thực hiện `esp_restart()` và khởi động lại hoàn tất trong < 3 giây.
- **Timing / tolerance:** Thời gian phát hiện treo: 15s ± 1s; thời gian reboot: 2.5s ± 0.5s.
- **Prohibited:** Hệ thống treo im lặng không tự reset; rơi vào vòng lặp crash bootloop vô hạn; mất dữ liệu NVS khi reboot.
- **Operator instructions:**
  1. Mở Serial terminal, gõ `test freeze event_bus`.
  2. Bấm đồng hồ đếm: Đúng 15 giây sau, xác nhận thấy log crash và gateway tự reboot.
  3. Sau khi reboot, kiểm tra terminal xác nhận dòng log: `[BOOT] Last reset caused by Task Supervisor Watchdog! Failed task: event_dispatcher`.

---

### 4.14 Boot Sequence tổng thể (Hoàn chỉnh & Chuẩn hóa)

```
app_main()
    │
    ├── 1. nvs_flash_init()                          ← Khởi tạo NVS Flash lưu tham số Wi-Fi, System, Timezone
    ├── 2. relay_gpio_init()                         ← PHẦN CỨNG AN TOÀN (GIAI ĐOẠN 1): Kéo GPIO 15..18 LOW ngay lập tức (10kΩ pull-down)
    ├── 3. event_bus_init()                          ← Khởi tạo Event Queue trung tâm (64 slots) trước khi các module đăng ký
    ├── 4. supervisor_init()                         ← Khởi tạo Task Supervisor (Core 0, Priority 7, Mask 0x7F, Timeout 15s)
    │       ├── Khởi tạo phần cứng TWDT 15s
    │       ├── Kiểm tra & in chẩn đoán từ RTC Slow Memory (nếu có sau sự cố trước đó)
    │       └── Đăng ký Active Task Mask = 0x7F (7 tasks production)
    │
    ├── 5. relay_controller_init()                   ← ĐĂNG KÝ HANDLER BUS (GIAI ĐOẠN 2): Đăng ký nhận command, rule action & fail-safe alert
    ├── 6. rtc_init()                                ← Khởi tạo bus I2C0 (GPIO 8/9), đọc UTC từ DS3231, nạp POSIX TZ "ICT-7"
    ├── 7. display_init()                            ← Khởi tạo OLED SSD1327 (I2C0 0x3C), nút bấm (GPIO 0/4), buzzer (GPIO 1)
    │       └── VẼ MÀN HÌNH ĐỒNG HỒ NGAY LẬP TỨC! (Đạt KPI-BOOT-01 < 1.5s độc lập hoàn toàn với mạng & SD card)
    │
    ├── 8. storage_init()                            ← Gắn kết (Mount) phân vùng LittleFS /storage (chứa config, schedule, rules)
    ├── 9. sd_logger_init()                          ← Khởi tạo SPI2 host & khe MicroSD (nếu chưa cắm thẻ -> fallback /storage)
    ├── 10. config_manager_init()                    ← Nạp cấu hình từ /storage/config.json
    ├── 11. wifi_provisioning_init() & web_server()  ← Khởi tạo Wi-Fi SoftAP Portal & Web Dashboard (Port 80)
    ├── 12. mqtt_adapter_init() & uplink_init()      ← Khởi tạo MQTT Client kết nối Broker (Core 1, QoS 1)
    ├── 13. rule_engine_init()                       ← Nạp /storage/rules.json, sẵn sàng đánh giá Hysteresis
    ├── 14. scheduler_init()                         ← Nạp /storage/schedule.json, tạo scheduler_task (Core 0, Priority 4)
    ├── 15. ble_adapter_init()                       ← Khởi tạo ngăn xếp NimBLE, bắt đầu passive scan cảm biến (Core 0, Prio 5)
    ├── 16. serial_cli_init()                        ← Giao diện dòng lệnh cứu hộ UART0 (Core 1, Priority 2, timeout 1s)
    │
    └── 17. event_bus_start_dispatcher()             ← BẮT ĐẦU ĐIỀU PHỐI EVENT BUS (SAU CÙNG)
            ├── Tạo event_dispatcher_task (Core 0, Priority 6)
            └── Bắt đầu chu trình xử lý sự kiện toàn hệ sinh thái
```

> **Tại sao tách relay_gpio_init và relay_controller_init?** 
> - `relay_gpio_init()` phải chạy **ngay ở Bước 2** trước mọi thao tác phần mềm khác để thiết lập cấu hình chân GPIO về output mức LOW với trở kéo xuống phần cứng 10kΩ, triệt tiêu 100% hiện tượng xung áp trôi nổi (floating voltage) có thể kích đóng nhầm rơ-le sưởi/quạt lúc chip vừa cấp nguồn.
> - `relay_controller_init()` chỉ được gọi **sau khi `event_bus_init()` đã tạo queue thành công** ở Bước 3, bảo đảm việc đăng ký các hàm event handler không bao giờ bị crash do hàng đợi rỗng.

> **Tại sao supervisor_init trước dispatcher?** Supervisor cần sẵn sàng giám sát ngay từ thời điểm dispatcher và các tác vụ bắt đầu nhận tải sự kiện đầu tiên. Dispatcher luôn là bước kích hoạt cuối cùng để mọi module đã đăng ký handler đầy đủ.

> **Tại sao dispatcher start sau cùng?** Nếu start sớm hơn và có event phát sinh trong lúc một module chưa kịp register handler, event đó sẽ bị xử lý thiếu handler hoặc bị mất.

---

## 5. Đặc tả chức năng — Backend

### 5.1 Mô tả tổng quan

Backend server làm trung gian giữa Web App và Gateway. Là **source of truth** cho config (schedule, device list), lưu trữ dữ liệu lịch sử (telemetry, event log), và cung cấp REST API + MQTT broker.

### 5.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| BE-01 | Quản lý Gateway | Đăng ký, xem trạng thái online/offline |
| BE-02 | Quản lý Device | CRUD thiết bị, lưu device_type, protocol, status |
| BE-03 | Quản lý Schedule | CRUD automation + alarm + personal event |
| BE-04 | Quản lý Automation Rule | CRUD rule "IF condition THEN action" |
| BE-05 | Nhận telemetry | Lưu trữ dữ liệu sensor từ Gateway |
| BE-06 | Nhận event log | Lưu log sự kiện từ Gateway (alarm stopped, rule executed...) |
| BE-07 | Sync & Config Topics | Cung cấp MQTT topics để Backend đẩy config (retained) + nhận log/telemetry |
| BE-08 | MQTT broker relay | Chuyển tiếp lệnh từ Web App → Gateway |
| BE-09 | Authentication | (Should-have) JWT/API Key |

### 5.3 Technology Stack đề xuất

| Thành phần | Đề xuất | Lý do |
|---|---|---|
| Language | Node.js (Express) hoặc Python (FastAPI) | Nhanh phát triển, nhiều tài liệu |
| Database | PostgreSQL hoặc MySQL | Quan hệ, phù hợp model dữ liệu |
| MQTT Broker | Mosquitto | Nhẹ, dễ cài, hỗ trợ LWT |
| ORM | Prisma (Node.js) hoặc SQLAlchemy (Python) | Giảm boilerplate SQL |

---

## 6. Đặc tả chức năng — Web App

### 6.1 Mô tả tổng quan

Giao diện duy nhất để user quản lý hệ thống (không có mobile app). Mọi thao tác CRUD — bao gồm cấu hình thiết bị, schedule, rule, lịch cá nhân — đều thực hiện trên Web.

### 6.2 Các trang chức năng

#### 6.2.1 Dashboard

| Thành phần | Mô tả |
|---|---|
| Gateway status | Online/Offline (dựa trên MQTT LWT) |
| Thiết bị overview | Số lượng thiết bị online/offline |
| Schedule sắp tới | 3–5 schedule gần nhất |
| Telemetry real-time | Nhiệt độ, trạng thái thiết bị mới nhất |

#### 6.2.2 Quản lý thiết bị

| Chức năng | Mô tả |
|---|---|
| Thêm thiết bị | Form: device_id, tên, protocol (BLE/MQTT), MAC/topic |
| Sửa thiết bị | Cập nhật tên, cấu hình |
| Xóa thiết bị | Xóa khỏi hệ thống + xóa rule liên quan |
| Xem trạng thái | Online/offline, last_seen, telemetry cuối cùng |
| Điều khiển | Nút ON/OFF cho actuator (gửi lệnh qua MQTT → Gateway) |

#### 6.2.3 Quản lý Schedule

| Chức năng | Mô tả |
|---|---|
| Danh sách schedule | Bảng: giờ, ngày lặp, loại, trạng thái, hành động |
| Thêm automation | Form: giờ, ngày lặp, thiết bị đích, lệnh (ON/OFF/SET) |
| Thêm alarm | Form: giờ, ngày lặp, tiêu đề, buzzer on/off |
| Thêm personal event | Form: giờ, ngày, tiêu đề (hiển thị trên OLED) |
| Sửa/Xóa | Inline edit hoặc modal |
| Bật/Tắt | Toggle enable/disable từng entry |

#### 6.2.4 Quản lý Automation Rule

| Chức năng | Mô tả |
|---|---|
| Danh sách rule | Bảng: sensor, điều kiện, threshold, target, action |
| Thêm rule | Form: IF [sensor] [>/<] [threshold] THEN [device] = [command] |
| Sửa/Xóa | |
| Bật/Tắt | Toggle enable/disable |

#### 6.2.5 Lịch sử sự kiện

| Chức năng | Mô tả |
|---|---|
| Timeline | Danh sách event theo thời gian |
| Lọc | Theo loại (automation, alarm, sensor, sync...) |
| Xem chi tiết | Thời gian, loại, payload |

#### 6.2.6 Cài đặt

| Chức năng | Mô tả |
|---|---|
| Gateway config | MQTT broker URL, Gateway ID |
| Wi-Fi reset | Trigger force reset provisioning |
| OTA update | (Should-have) Upload firmware mới |

---

## 7. Mô hình dữ liệu

### 7.1 ERD (Entity Relationship Diagram)

```
┌──────────┐     1:N     ┌──────────┐
│   User   │────────────►│  Gateway  │
└──────────┘             └────┬─────┘
                              │ 1:N
              ┌───────────────┼───────────────┐
              ▼               ▼               ▼
        ┌──────────┐   ┌──────────┐   ┌──────────────┐
        │  Device  │   │ Schedule │   │ AutomationRule│
        └────┬─────┘   └──────────┘   └──────────────┘
             │ 1:N
             ▼
        ┌──────────┐
        │ EventLog │
        └──────────┘
```

### 7.2 Bảng dữ liệu chi tiết

#### 7.2.1 User

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | INT | PK, AUTO_INCREMENT | |
| username | VARCHAR(64) | UNIQUE, NOT NULL | |
| email | VARCHAR(128) | UNIQUE | |
| password_hash | VARCHAR(256) | NOT NULL | bcrypt |
| created_at | TIMESTAMP | DEFAULT NOW() | |

#### 7.2.2 Gateway

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | VARCHAR(32) | PK | VD: "gateway_001" |
| user_id | INT | FK → User.id | |
| name | VARCHAR(64) | NOT NULL | VD: "Đồng hồ phòng khách" |
| status | ENUM | 'online','offline' | Cập nhật qua MQTT LWT |
| last_seen | TIMESTAMP | | Thời điểm cuối cùng nhận message |
| firmware_version | VARCHAR(16) | | VD: "1.0.0" |
| timezone | VARCHAR(64) | | VD: "ICT-7" |
| created_at | TIMESTAMP | DEFAULT NOW() | |

#### 7.2.3 Device

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | INT | PK, AUTO_INCREMENT | |
| device_id | VARCHAR(32) | UNIQUE | VD: "sensor_01" |
| gateway_id | VARCHAR(32) | FK → Gateway.id | |
| name | VARCHAR(64) | NOT NULL | VD: "Cảm biến nhiệt phòng ngủ" |
| device_type | ENUM | 'sensor','actuator' | |
| protocol | ENUM | 'ble','mqtt' | |
| mac_address | VARCHAR(18) | NULLABLE | Cho BLE: "AA:BB:CC:DD:EE:FF" |
| mqtt_topic | VARCHAR(128) | NULLABLE | Cho MQTT |
| capabilities | JSON | | VD: ["temperature","humidity"] |
| status | ENUM | 'online','offline','unknown' | |
| last_seen | TIMESTAMP | | |
| last_value | FLOAT | NULLABLE | Giá trị telemetry cuối cùng |
| created_at | TIMESTAMP | DEFAULT NOW() | |

#### 7.2.4 Schedule

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | INT | PK, AUTO_INCREMENT | |
| gateway_id | VARCHAR(32) | FK → Gateway.id | |
| type | ENUM | 'automation','alarm','personal_event' | |
| hour | TINYINT | 0-23, NOT NULL | |
| minute | TINYINT | 0-59, NOT NULL | |
| repeat_days | VARCHAR(32) | NULLABLE | VD: "MON,TUE,WED,THU,FRI" — cho lịch lặp |
| one_time_date | DATE | NULLABLE | VD: "2026-08-20" — cho sự kiện 1 lần |
| enabled | BOOLEAN | DEFAULT true | |
| target_device_id | VARCHAR(32) | NULLABLE, FK → Device.device_id | Cho automation |
| action_command | VARCHAR(32) | NULLABLE | Cho automation: "ON","OFF" |
| title | VARCHAR(64) | NULLABLE | Cho alarm/personal_event |
| buzzer | BOOLEAN | DEFAULT false | Bật buzzer khi trigger? |
| created_at | TIMESTAMP | DEFAULT NOW() | |
| updated_at | TIMESTAMP | ON UPDATE NOW() | |

> **Ghi chú:** `repeat_days` và `one_time_date` là **mutually exclusive** — mỗi entry chỉ dùng 1 trong 2. Validation tại Backend: nếu có `one_time_date` thì `repeat_days` phải NULL và ngược lại.

#### 7.2.5 AutomationRule

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | INT | PK, AUTO_INCREMENT | |
| gateway_id | VARCHAR(32) | FK → Gateway.id | |
| sensor_device_id | VARCHAR(32) | FK → Device.device_id | Sensor trigger |
| condition_type | ENUM | 'gt','lt','eq' | Greater than, less than, equal |
| threshold | FLOAT | NOT NULL | Ngưỡng so sánh |
| hysteresis | FLOAT | DEFAULT 1.5 | Biên trễ chống dao động tiếp điểm rơ-le (mặc định 1.5) |
| target_device_id | VARCHAR(32) | FK → Device.device_id | Actuator đích |
| action_command | VARCHAR(32) | NOT NULL | "ON","OFF","SET:75" |
| enabled | BOOLEAN | DEFAULT true | |
| created_at | TIMESTAMP | DEFAULT NOW() | |

#### 7.2.6 EventLog

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | BIGINT | PK, AUTO_INCREMENT | |
| gateway_id | VARCHAR(32) | FK → Gateway.id | |
| event_type | VARCHAR(32) | NOT NULL | VD: "rule_executed","alarm_stopped","sync_completed" |
| device_id | VARCHAR(32) | NULLABLE | Thiết bị liên quan |
| payload | JSON | NULLABLE | Chi tiết sự kiện |
| occurred_at | TIMESTAMP | NOT NULL | Thời điểm xảy ra (từ RTC gateway) |
| synced_at | TIMESTAMP | NULLABLE | Thời điểm đồng bộ thành công |

#### 7.2.7 TelemetryLog (Should-have)

| Field | Type | Constraint | Mô tả |
|---|---|---|---|
| id | BIGINT | PK, AUTO_INCREMENT | |
| device_id | VARCHAR(32) | FK → Device.device_id | |
| value | FLOAT | NOT NULL | |
| unit | VARCHAR(16) | | VD: "°C", "%", "lux" |
| recorded_at | TIMESTAMP | NOT NULL | |

### 7.3 JSON Schema — schedule.json (trên Gateway Flash /storage)

```json
[
  {
    "id": 101,
    "type": "automation",
    "hour": 7,
    "minute": 0,
    "repeat": [1, 2, 3, 4, 5],
    "date": null,
    "enabled": true,
    "target_device_id": "bedroom_light",
    "action_command": "ON"
  },
  {
    "id": 201,
    "type": "alarm",
    "hour": 6,
    "minute": 30,
    "repeat": [1, 2, 3, 4, 5],
    "date": null,
    "enabled": true,
    "title": "Dậy đi học",
    "buzzer": true
  },
  {
    "id": 301,
    "type": "personal_event",
    "hour": 14,
    "minute": 0,
    "repeat": null,
    "date": "2026-08-20",
    "enabled": true,
    "title": "Họp nhóm đồ án",
    "buzzer": false
  }
]
```

> **Ghi chú:**
> - `repeat` dùng mảng số (0=CN, 1=T2, ..., 6=T7) thay vì chuỗi "MON,TUE" để tiết kiệm parse trên ESP32.
> - `date` và `repeat` là **mutually exclusive**: nếu `date` khác null thì `repeat` phải null và ngược lại.
> - Entry có `date` sẽ được Backend tự set `enabled=false` sau khi Gateway đã trigger và báo lại qua event log.
> - **Mapping tên field:** Backend lưu DB là `one_time_date` (§7.2.4), khi serialize ra file JSON local cho Gateway thì rút gọn thành `date` để tiết kiệm kích thước file trên flash ESP32. Firmware parse JSON dùng key `date`, map vào C struct field `one_time_date`. Đây là quyết định thiết kế có chủ đích, không phải lỗi đặt tên.

### 7.3.2 JSON Schema — rules.json (trên Gateway Flash /storage)

Tệp lưu trữ danh sách các quy tắc tự động hóa IF-THEN cục bộ trên chip:

```json
[
  {
    "id": 1,
    "sensor_device_id": "sensor_temp_01",
    "condition_type": "gt",
    "threshold": 30.0,
    "hysteresis": 1.5,
    "target_device_id": "fan_relay_01",
    "action_command": "ON",
    "enabled": true
  },
  {
    "id": 2,
    "sensor_device_id": "sensor_temp_01",
    "condition_type": "lt",
    "threshold": 22.0,
    "hysteresis": 1.5,
    "target_device_id": "heater_relay_01",
    "action_command": "ON",
    "enabled": true
  }
]
```

### 7.4 JSON Schema — device_list.json (trên Gateway Flash /storage)

```json
[
  {
    "device_id": "sensor_temp_01",
    "name": "Cảm biến nhiệt ẩm phòng ngủ",
    "protocol": "ble",
    "mac_address": "A4:C1:38:12:34:56",
    "parser_type": "bthome_v2",
    "bindkey": "0123456789abcdef0123456789abcdef",
    "device_type": "sensor"
  },
  {
    "device_id": "sensor_livingroom",
    "name": "Cảm biến phòng khách",
    "protocol": "ble",
    "mac_address": "11:22:33:44:55:66",
    "parser_type": "sig_0x181a",
    "bindkey": null,
    "device_type": "sensor"
  },
  {
    "device_id": "smart_socket_01",
    "name": "Ổ cắm thông minh Wi-Fi",
    "protocol": "mqtt",
    "mqtt_topic": "device/socket_01",
    "device_type": "actuator"
  }
]
```

> **Quy chuẩn Định danh Tài nguyên & Phân loại Actuator:**
> - **4 Kênh Relay Onboard:** Là tài nguyên phần cứng tích hợp sẵn của Gateway (`relay_1` đến `relay_4`, tương ứng GPIO 15..18). Hệ thống quản lý tập trung qua module `relay_controller`, không đưa vào danh sách protocol ngoại vi với nhãn `"internal_gpio"`.
> - **Cảm biến BLE (Passive Observer):** Bắt buộc thuộc `protocol: "ble"`. Hỗ trợ 3 kiểu phân tích gói tin (`parser_type`):
>   1. `"sig_0x181a"`: Chuẩn Bluetooth SIG Environmental Sensing (UUID 0x181A).
>   2. `"bthome_v2"`: Chuẩn BTHome V2 mở (hỗ trợ cả dạng plaintext và ciphertext có bindkey).
>   3. `"mijia_v2"`: Chuẩn Xiaomi MiBeacon V2/V3/V4 (giải mã AES-128 CCM với `bindkey` 16 bytes).
> - **Thiết bị Chấp hành Ngoại vi:** Bắt buộc thuộc `protocol: "mqtt"` (kết nối qua Wi-Fi), điều khiển qua publish lệnh tới topic `gateway/{id}/device/{did}/command`. Không hỗ trợ BLE Actuator.

### 7.5 Đặc tả Cấu trúc Bản tin Sự kiện (Event Payload Schema — MQTT & Offline MicroSD)

> **Lưu ý kiến trúc:** Gateway không lưu trữ file JSON sự kiện tích lũy trên bộ nhớ Flash SPIFFS để triệt tiêu hao mòn chip nhớ (wear-leveling protection).
> - **Khi Online:** Sự kiện được publish trực tiếp lên MQTT topic `gateway/{id}/events` (QoS 1).
> - **Khi Offline:** Sự kiện được đóng gói thành bản ghi nhị phân 172 bytes (`sync_queue_record_t`, `record_type = 0x02`) lưu vào `sync_queue.bin` trên thẻ nhớ MicroSD, trong đó trường `payload` chứa chuỗi JSON dưới đây (tối đa 128 bytes).

*Ví dụ bản tin Rule Event:*
```json
{
  "type": "rule_executed",
  "ts": 1723621234,
  "rule_id": 1,
  "target": "fan_01",
  "command": "ON"
}
```

*Ví dụ bản tin Alarm Event:*
```json
{
  "type": "alarm_stopped",
  "ts": 1723621567,
  "schedule_id": 201
}
```

*Ví dụ bản tin Automation Event:*
```json
{
  "type": "automation_triggered",
  "ts": 1723621800,
  "schedule_id": 101,
  "target": "bedroom_light",
  "command": "ON"
}
```

---

## 8. Đặc tả API Backend

### 8.1 Base URL

```
http://{backend_host}:{port}/api/v1
```

### 8.2 Gateway APIs

#### GET /gateways/{gateway_id}

Lấy thông tin gateway.

**Response 200:**
```json
{
  "id": "gateway_001",
  "name": "Đồng hồ phòng khách",
  "status": "online",
  "last_seen": "2026-08-14T14:50:00Z",
  "firmware_version": "1.0.0",
  "timezone": "ICT-7"
}
```


#### POST /gateways/register-claim

Gateway gọi lên Backend khi vừa kết nối Internet lần đầu để đăng ký mã xác thực cục bộ (Claim Code).

- **Authentication:** Public / Internal Gateway Provisioning
- **Rate Limit:** Tối đa 5 request / MAC / giờ.
- **Request Body (Gateway → Backend):**
```json
{
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "claim_code": "GW-9821A0",
  "firmware_version": "1.0.0"
}
```
- **Response 200 OK:**
```json
{
  "status": "UNCLAIMED",
  "ttl_seconds": 86400,
  "message": "Gateway registered for claim"
}
```
- **Response 409 Conflict:** Gateway MAC đã được liên kết với một tài khoản khác hoặc đã ACTIVE.

---

#### POST /gateways/claim

Người dùng nhập Claim Code hiển thị trên màn hình OLED (Screen 3) vào Web App để nhận quyền sở hữu Gateway.

- **Authentication:** Bearer User JWT Token (Người dùng đã đăng nhập)
- **Rate Limit:** Tối đa 5 lần thử sai / IP / 15 phút. Nếu vượt quá, trả lỗi `429 CLAIM_RATE_LIMITED` để ngăn chặn tấn công Brute-Force mã 6 ký tự.
- **Thời hạn mã (TTL):** 24 giờ kể từ khi Gateway gọi `register-claim`. Nếu quá hạn, trả `410 CLAIM_CODE_EXPIRED`.
- **Request Body (Web App → Backend):**
```json
{
  "claim_code": "GW-9821A0",
  "name": "Đồng hồ Phòng khách"
}
```
- **Response 200 OK:**
```json
{
  "gateway_id": "gw_a1b2c3d4",
  "name": "Đồng hồ Phòng khách",
  "api_key": "gw_sec_e8b17a02c9124...",
  "mqtt_broker": {
    "host": "broker.example.com",
    "port": 8883,
    "ssl": true,
    "username": "gw_a1b2c3d4",
    "password": "gw_mqtt_token_88912..."
  },
  "message": "Gateway claimed successfully"
}
```
- **Hành vi sau khi Claim thành công:**
  1. Backend chuyển trạng thái Gateway từ `UNCLAIMED` sang `ACTIVE`.
  2. Mã `claim_code` bị xóa khỏi hệ thống (Single-use Token), vô hiệu hóa hoàn toàn mã cũ.
  3. Backend publish gói tin MQTT Downlink chứa credentials hoặc cấp phát cấu hình ban đầu.
  4. Gateway xóa mã `claim_code` trong NVS, chuyển màn hình OLED sang trạng thái vận hành chính thức.

##### Verification Contract — Gateway Claim Lifecycle (FR-CLAIM-01)
- **Preconditions:** Gateway boot lần đầu, sinh `claim_code` (6 ký tự alphanumeric), kết nối Wi-Fi thành công; Backend sẵn sàng tại `POST /api/v1/gateways/register-claim`.
- **Stimulus:** Gateway gọi `register-claim`; sau đó Web App gọi `claim` với mã đúng, hoặc kẻ tấn công gửi thử sai 6 lần liên tiếp.
- **Expected:**
  1. `register-claim` trả HTTP 200 kèm TTL 86400s; DB lưu bản ghi `UNCLAIMED`.
  2. Web App nhập đúng mã nhận HTTP 200, `gateway_id`, API Key và MQTT credentials; DB chuyển sang `ACTIVE`, `claimCode` bị null hóa.
  3. Thử sai lần thứ 6 trong 15 phút trả HTTP 429 `CLAIM_RATE_LIMITED`.
  4. Nhập mã sau 24h trả HTTP 410 `CLAIM_CODE_EXPIRED`.
- **Timing/Tolerance:** Backend phản hồi trong $\le 500\text{ms}$; thời gian vô hiệu hóa `claimCode` $\le 50\text{ms}$ sau khi claim thành công.
- **Prohibited:** Nghiêm cấm chấp nhận mã `claim_code` đã claim lần 2 (Replay Attack); cấm bỏ qua rate limiting brute-force.
- **Tier:** Integration / System Test (`test_gateway_claim_lifecycle.py`).
- **Operator Instructions:** Sử dụng Postman/curl gửi 6 request sai mã liên tiếp tới `/gateways/claim`, xác minh mã lỗi 429 và thông báo rate limit.

#### PUT /gateways/{gateway_id}

Cập nhật thông tin gateway.

**Request Body:**
```json
{
  "name": "Đồng hồ phòng ngủ"
}
```

### 8.3 Device APIs

#### GET /gateways/{gateway_id}/devices

Lấy danh sách thiết bị.

**Response 200:**
```json
[
  {
    "id": 1,
    "device_id": "sensor_01",
    "name": "Cảm biến nhiệt phòng ngủ",
    "device_type": "sensor",
    "protocol": "ble",
    "mac_address": "AA:BB:CC:DD:EE:FF",
    "status": "online",
    "last_seen": "2026-08-14T14:48:00Z",
    "last_value": 28.5
  }
]
```

#### POST /gateways/{gateway_id}/devices

Thêm thiết bị mới.

**Request Body:**
```json
{
  "device_id": "sensor_02",
  "name": "Cảm biến độ ẩm",
  "device_type": "sensor",
  "protocol": "ble",
  "mac_address": "11:22:33:44:55:66"
}
```

**Response 201:**
```json
{
  "id": 2,
  "device_id": "sensor_02",
  "message": "Device created successfully"
}
```


#### PUT /gateways/{gateway_id}/devices/{device_id}

Cập nhật thiết bị.

#### DELETE /gateways/{gateway_id}/devices/{device_id}

Xóa thiết bị (cascade xóa rule liên quan).

#### POST /gateways/{gateway_id}/devices/{device_id}/command

Gửi lệnh điều khiển thiết bị (qua MQTT → Gateway).

**Request Body:**
```json
{
  "command": "ON"
}
```

### 8.4 Schedule APIs

#### GET /gateways/{gateway_id}/schedules

Lấy tất cả schedule (automation + alarm + personal event).

**Query params:**
- `type` (optional): `automation` | `alarm` | `personal_event`

**Response 200:**
```json
[
  {
    "id": 101,
    "type": "automation",
    "hour": 7,
    "minute": 0,
    "repeat_days": "MON,TUE,WED,THU,FRI",
    "enabled": true,
    "target_device_id": "bedroom_light",
    "action_command": "ON",
    "title": null,
    "buzzer": false
  },
  {
    "id": 201,
    "type": "alarm",
    "hour": 6,
    "minute": 30,
    "repeat_days": "MON,TUE,WED,THU,FRI",
    "enabled": true,
    "target_device_id": null,
    "action_command": null,
    "title": "Dậy đi học",
    "buzzer": true
  }
]
```

#### POST /gateways/{gateway_id}/schedules

Tạo schedule mới.

**Request Body (automation):**
```json
{
  "type": "automation",
  "hour": 17,
  "minute": 30,
  "repeat_days": "MON,TUE,WED,THU,FRI,SAT,SUN",
  "target_device_id": "living_light",
  "action_command": "ON"
}
```

**Request Body (alarm):**
```json
{
  "type": "alarm",
  "hour": 6,
  "minute": 30,
  "repeat_days": "MON,TUE,WED,THU,FRI",
  "title": "Dậy đi học",
  "buzzer": true
}
```

**Request Body (personal_event — sự kiện một lần):**
```json
{
  "type": "personal_event",
  "hour": 14,
  "minute": 0,
  "one_time_date": "2026-08-20",
  "title": "Họp nhóm đồ án",
  "buzzer": false
}
```

> **Lưu ý:** `personal_event` dùng `one_time_date` thay vì `repeat_days`. Hai field này **mutually exclusive** — Backend validate và trả `400 VALIDATION_ERROR` nếu cả hai cùng có giá trị (xem §8.9).

**Response 201:**
```json
{
  "id": 102,
  "message": "Schedule created successfully"
}
```


#### PUT /gateways/{gateway_id}/schedules/{id}

Cập nhật schedule.

#### DELETE /gateways/{gateway_id}/schedules/{id}

Xóa schedule.

#### PATCH /gateways/{gateway_id}/schedules/{id}/toggle

Bật/tắt schedule.

**Request Body:**
```json
{
  "enabled": false
}
```

### 8.5 Automation Rule APIs

#### GET /gateways/{gateway_id}/rules

Lấy danh sách rule.

**Response 200:**
```json
[
  {
    "id": 1,
    "sensor_device_id": "sensor_01",
    "condition_type": "gt",
    "threshold": 30.0,
    "hysteresis": 1.5,
    "target_device_id": "fan_01",
    "action_command": "ON",
    "enabled": true
  }
]
```

#### POST /gateways/{gateway_id}/rules

Tạo rule mới.

**Request Body:**
```json
{
  "sensor_device_id": "sensor_01",
  "condition_type": "gt",
  "threshold": 30.0,
  "hysteresis": 1.5,
  "target_device_id": "fan_01",
  "action_command": "ON"
}
```


#### PUT /gateways/{gateway_id}/rules/{id}

Cập nhật rule.

#### DELETE /gateways/{gateway_id}/rules/{id}

Xóa rule.




### 8.6 Quy chuẩn Giao thức Đồng bộ Cấu hình và Dữ liệu (100% MQTT, Không dùng HTTP Sync)

> [!IMPORTANT]
> **Loại bỏ hoàn toàn kiến trúc HTTP Sync Polling:**
> Nhằm tối ưu tài nguyên vi điều khiển ESP32-S3, giảm độ trễ và tiết kiệm băng thông, Gateway **tuyệt đối không sử dụng** các endpoint HTTP để pull cấu hình (`/sync/schedule`, `/sync/devices`) hay push sự kiện (`/sync/events`). Toàn bộ hoạt động đồng bộ tuân thủ mô hình **MQTT Event-Driven**:
> 1. **Cấu hình (Downlink):** Backend publish cấu hình mới tới topic `gateway/{id}/config/schedule` và `gateway/{id}/config/devices` (QoS 1, retained). Gateway tiếp nhận qua `mqtt_adapter` và lưu atomic vào LittleFS qua `storage_task`.
> 2. **Dữ liệu & Sự kiện (Uplink):** Gateway đẩy bản tin telemetry thời gian thực qua `gateway/{id}/device/{did}/telemetry`, sự kiện qua `gateway/{id}/events`, và dữ liệu backfill ngoại tuyến từ thẻ nhớ SD qua `gateway/{id}/telemetry/batch` (tất cả đều dùng QoS 1).

### 8.7 Event Log APIs (Web App gọi)

#### GET /gateways/{gateway_id}/events

Lấy lịch sử sự kiện.

**Query params:**
- `type` (optional): lọc loại event
- `from` (optional): timestamp bắt đầu
- `to` (optional): timestamp kết thúc
- `limit` (optional, default 50): số lượng tối đa
- `offset` (optional, default 0): phân trang

### 8.8 Telemetry APIs (Should-have)

#### GET /gateways/{gateway_id}/devices/{device_id}/telemetry

Lấy lịch sử telemetry.

**Query params:**
- `from`, `to`, `limit`, `offset`

### 8.9 Error Response Format

Tất cả các endpoint trả về lỗi đều sử dụng chung format JSON sau:

```json
{
  "error": {
    "code": "DEVICE_NOT_FOUND",
    "message": "Device with id 'sensor_99' not found in gateway 'gateway_001'",
    "status": 404
  }
}
```

#### Bảng mã lỗi

| HTTP Status | Error Code | Mô tả | Ví dụ tình huống |
|---|---|---|---|
| 400 | `VALIDATION_ERROR` | Request body không hợp lệ | Thiếu field bắt buộc, `hour` ngoài [0-23], `repeat_days` và `one_time_date` cùng có giá trị |
| 400 | `INVALID_DEVICE_PROTOCOL` | Protocol không hợp lệ | `protocol` không phải "ble" hoặc "mqtt" |
| 400 | `ERR_RELAY_INTERLOCK_CONFLICT` | Xung đột khóa liên động an toàn | Nhận lệnh bật Máy sưởi (Relay 2) khi Quạt (Relay 1) đang chạy (lệnh sưởi bị từ chối) |
| 404 | `GATEWAY_NOT_FOUND` | Không tìm thấy gateway | `gateway_id` không tồn tại trong DB |
| 404 | `DEVICE_NOT_FOUND` | Không tìm thấy thiết bị | `device_id` không tồn tại hoặc không thuộc gateway |
| 404 | `SCHEDULE_NOT_FOUND` | Không tìm thấy schedule | `schedule.id` không tồn tại |
| 404 | `RULE_NOT_FOUND` | Không tìm thấy rule | `rule.id` không tồn tại |
| 409 | `DEVICE_ID_CONFLICT` | `device_id` đã tồn tại | POST /devices với `device_id` trùng |
| 409 | `DEVICE_IN_USE` | Thiết bị đang được rule tham chiếu | Xóa sensor/actuator đang có rule (nếu không cascade) |
| 409 | `ERR_RELAY_DWELL_LOCKED` | Relay đang trong thời gian trễ bảo vệ | Nhận lệnh đóng/ngắt Relay 3 (máy nén) khi chưa hết 180s Dwell Time |
| 409 | `ERR_RELAY_LOCKOUT_ACTIVE` | Khóa an toàn máy sưởi đang kích hoạt (300s) | Nhận lệnh bật Máy sưởi (Relay 2) khi đang trong 300s Lockout sau sự cố ngắt khẩn cấp |
| 410 | `CLAIM_CODE_EXPIRED` | Mã xác thực Claim Code đã hết hạn | Nhập mã sau 24 giờ kể từ khi Gateway phát sinh và gửi `register-claim` |
| 422 | `GATEWAY_OFFLINE` | Gateway không online | POST /command nhưng gateway đang offline |
| 429 | `CLAIM_RATE_LIMITED` | Quá giới hạn thử mã xác thực | Nhập sai Claim Code quá 5 lần trong 15 phút (chặn brute-force) |
| 500 | `INTERNAL_ERROR` | Lỗi server không xác định | DB connection fail, unhandled exception |

#### Ví dụ response lỗi theo endpoint

**POST /gateways/{gateway_id}/devices — 400:**
```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Field 'device_id' is required and must be 1-32 characters",
    "status": 400
  }
}
```

**POST /gateways/{gateway_id}/devices — 409:**
```json
{
  "error": {
    "code": "DEVICE_ID_CONFLICT",
    "message": "Device 'sensor_01' already exists in gateway 'gateway_001'",
    "status": 409
  }
}
```

**POST /gateways/{gateway_id}/devices/{device_id}/command — 422:**
```json
{
  "error": {
    "code": "GATEWAY_OFFLINE",
    "message": "Gateway 'gateway_001' is currently offline. Command queued.",
    "status": 422
  }
}
```

**POST /gateways/{gateway_id}/schedules — 400:**
```json
{
  "error": {
    "code": "VALIDATION_ERROR",
    "message": "Fields 'repeat_days' and 'one_time_date' are mutually exclusive. Provide only one.",
    "status": 400
  }
}
```

---

## 9. Đặc tả giao thức MQTT

### 9.1 Topic Convention

```
gateway/{gateway_id}/                           ← prefix chung
    status                                       ← "online"/"offline" (retained, LWT, QoS 1)
    device/{device_id}/
        telemetry                                ← Gateway → Backend (QoS 1)
        command                                  ← Backend → Gateway (QoS 1, có command_id & ttl)
        command/ack                              ← Gateway → Backend (QoS 1, phản hồi kết quả thực thi)
    telemetry/
        batch                                    ← Gateway → Backend (QoS 1, backfill khi mạng phục hồi)
    config/
        schedule                                 ← Backend → Gateway (QoS 1, retained)
        devices                                  ← Backend → Gateway (QoS 1, retained, kèm bindkey/parser)
    events                                       ← Gateway → Backend (QoS 1, luồng sự kiện thời gian thực)
```

### 9.2 Payload Format

Tất cả payload dùng JSON.

#### Telemetry (Gateway → Backend)
```json
{
  "device_id": "sensor_01",
  "value": 28.5,
  "unit": "°C",
  "valid": true,
  "timestamp": 1723621234
}
```

#### Command Downlink (Backend → Gateway)
```json
{
  "command_id": "550e8400-e29b-41d4-a716-446655440000",
  "target_device_id": "relay_1",
  "command": "ON",
  "ttl_seconds": 60,
  "source": "web_app",
  "timestamp": 1723621234
}
```

#### Command ACK Uplink (Gateway → Backend)
```json
{
  "command_id": "550e8400-e29b-41d4-a716-446655440000",
  "target_device_id": "relay_1",
  "success": true,
  "error_code": 0,
  "message": "Relay 1 turned ON successfully",
  "execution_time_ms": 45,
  "timestamp": 1723621235
}
```

#### Status (Gateway → Backend, retained)
```json
"online"
```
hoặc LWT:
```json
"offline"
```

### 9.3 QoS & Triển Khai MQTT Broker

| Topic | QoS | Hướng | Lý do |
|---|:---:|:---:|---|
| `gateway/{id}/device/{dev}/telemetry` | 1 | Uplink | Đảm bảo dữ liệu cảm biến đo lường không bị thất lạc |
| `gateway/{id}/telemetry/batch` | 1 | Uplink | Dữ liệu lưu đệm ngoại tuyến (Store-and-Forward) cần đối soát đủ |
| `gateway/{id}/device/{dev}/command` | 1 | Downlink | Lệnh điều khiển bắt buộc phải đến đích (At-least once) |
| `gateway/{id}/device/{dev}/command/ack` | 1 | Uplink | Báo cáo kết quả thực thi lệnh về Backend để cập nhật UI |
| `gateway/{id}/config/schedule` | 1 | Downlink | Cấu hình lịch biểu và luật tự động hóa, retained |
| `gateway/{id}/config/devices` | 1 | Downlink | Danh sách cảm biến, parser_type và bindkey, retained |
| `gateway/{id}/status` | 1 | Uplink | Retained Last Will & Testament (LWT) |
| `gateway/{id}/events` | 1 | Uplink | Nhật ký sự kiện hệ thống |

##### Verification Contract — MQTT Topic Routing & QoS Assurance (FR-MQTT-01)
- **Preconditions:** Gateway đã kết nối Wi-Fi và thiết lập phiên MQTT thành công tới Broker với `CleanSession = 0`.
- **Stimulus:**
  1. Backend gửi lệnh xuống `gateway/{id}/device/{dev}/command` (QoS 1).
  2. Gateway gửi telemetry, batch backfill, command ACK và events lên Broker.
  3. Mạng bị ngắt đột ngột trong khi Gateway đang gửi hoặc nhận bản tin QoS 1.
- **Expected:**
  1. Toàn bộ bản tin truyền thông qua Broker đều nhận PUBACK xác nhận theo chuẩn QoS 1.
  2. Gateway phản hồi `command/ack` trong $\le 100\text{ms}$ kể từ khi relay chấp hành xong.
  3. Khi mất kết nối giữa chừng, bản tin chưa nhận PUBACK được gửi lại (retransmitted) ngay sau khi kết nối phục hồi mà không làm mất dữ liệu.
  4. Topic LWT tự động chuyển sang `offline` trên Broker khi phát hiện kết nối TCP đứt gãy sau khoảng thời gian KeepAlive (30s).
- **Timing/Tolerance:** Thời gian xử lý lệnh từ Downlink đến ACK Uplink $\le 200\text{ms}$; backfill rate kiểm soát từ 20 đến 50 bản tin/giây.
- **Prohibited:** Tuyệt đối không dùng QoS 0 cho telemetry/command/batch/events; không sử dụng HTTP polling thay thế MQTT topic.
- **Tier:** Integration / Protocol Conformance Test (`test_mqtt_protocol.py`).
- **Operator Instructions:** Sử dụng Wireshark hoặc Mosquitto Sub giám sát toàn bộ luồng topic `gateway/#`, kiểm tra trường QoS flag trong header gói tin MQTT bắt buộc phải bằng 1.

> **Khuyến nghị môi trường Broker (Bảo mật & Ổn định):**
> - **Triển khai Production:** Sử dụng **Embedded Aedes MQTT Broker** tích hợp sẵn trên Cloud Backend (cổng 1883 nội bộ Docker) hoặc cài đặt cụm **Eclipse Mosquitto** độc lập có xác thực chứng chỉ TLS/SSL (cổng 8883) và bảo vệ bằng mật khẩu (Username/Password per-gateway).
> - **Cảnh báo Broker công cộng:** Tùy chọn `mqtt://broker.emqx.io:1883` chỉ được dùng làm môi trường thử nghiệm nhanh (Sandbox/Dev Test). Tuyệt đối không dùng cho vận hành thực tế vì bất kỳ ai cũng có thể đọc/ghi đè lệnh điều khiển thiết bị trong nhà.

## 10. Use Cases

### UC-01: Setup lần đầu

| | |
|---|---|
| **Actor** | User |
| **Precondition** | Gateway mới, chưa có Wi-Fi config |
| **Main Flow** | 1. User bật Gateway → OLED hiển thị "Setup Mode" |
| | 2. User dùng điện thoại/laptop kết nối Wi-Fi "SmartClock-Setup" |
| | 3. Trình duyệt tự mở captive portal |
| | 4. User nhập SSID + Password (timezone tự lấy từ JS) |
| | 5. Gateway lưu NVS → kết nối Wi-Fi → set timezone → sync NTP |
| | 6. Gateway nhận config từ Backend qua MQTT topics → sẵn sàng hoạt động |
| **Postcondition** | Gateway online, hiển thị đúng giờ, đã sync config |

### UC-02: Thêm thiết bị

| | |
|---|---|
| **Actor** | User |
| **Precondition** | Gateway đã online |
| **Main Flow** | 1. User mở Web App → Thiết bị → Thêm mới |
| | 2. Nhập: device_id, tên, protocol (BLE/MQTT), MAC/topic |
| | 3. Backend lưu DB → publish device_list mới cho Gateway qua MQTT |
| | 4. Gateway nhận → cập nhật `device_list.json` → `ble_adapter` hoặc `mqtt_adapter` nạp device mới |
| **Postcondition** | Thiết bị xuất hiện trên Gateway, bắt đầu đọc telemetry |

### UC-03: Tạo automation rule

| | |
|---|---|
| **Actor** | User |
| **Precondition** | Có ít nhất 1 sensor + 1 actuator đã đăng ký |
| **Main Flow** | 1. User mở Web App → Automation → Thêm rule |
| | 2. Nhập: IF sensor_01 > 30°C THEN fan_01 = ON |
| | 3. Backend lưu DB → push config mới cho Gateway |
| | 4. Gateway nhận → `rule_engine_reload_rules()` |
| | 5. Sensor báo 31°C → Rule Engine match → post EVT_RULE_ACTION_REQUEST |
| | 6. Relay Controller nhận EVT_RULE_ACTION_REQUEST → kiểm tra khóa liên động an toàn (Interlock) → kích hoạt chân GPIO 15 (relay_1 = ON) → quạt bật, post EVT_COMMAND_RESPONSE gửi ACK |
| **Postcondition** | Quạt bật tự động khi nhiệt độ > 30°C |

### UC-04: Tạo báo thức

| | |
|---|---|
| **Actor** | User |
| **Precondition** | Gateway đã online |
| **Main Flow** | 1. User mở Web App → Schedule → Thêm alarm |
| | 2. Nhập: 06:30, T2-T6, "Dậy đi học", buzzer=ON |
| | 3. Backend lưu DB → push schedule mới cho Gateway |
| | 4. Gateway nhận → `scheduler_reload()` |
| | 5. Đến 06:30 T2 → Scheduler post EVT_SCHEDULE_ALARM_TRIGGERED |
| | 6. Display hiển thị alarm overlay + bật buzzer |
| | 7. User bấm nút tắt → buzzer tắt, ghi event log |
| **Postcondition** | Alarm tắt, event log chờ sync lên Backend |

### UC-05: Hoạt động offline

| | |
|---|---|
| **Actor** | Hệ thống (tự động) |
| **Precondition** | Gateway đang online, có schedule + rule đã sync |
| **Trigger** | Mất Internet |
| **Main Flow** | 1. Wi-Fi Provisioning post EVT_WIFI_LOST, MQTT Adapter post EVT_MQTT_DISCONNECTED |
| | 2. uplink_task chuyển sang cơ chế Store-and-Forward sang thẻ nhớ SD |
| | 3. RTC giữ giờ chính xác (DS3231 pin backup) |
| | 4. Scheduler vẫn chạy: poll 5s, trigger automation/alarm đúng giờ |
| | 5. Rule Engine vẫn đánh giá telemetry BLE (BLE không cần Internet) |
| | 6. Dữ liệu sự kiện và telemetry ghi đệm vào thẻ nhớ SD (`sync_queue.bin`) |
| **Postcondition** | Gateway hoạt động bình thường, log chờ backfill khi có mạng |

### UC-06: Internet trở lại — đồng bộ

| | |
|---|---|
| **Actor** | Hệ thống (tự động) |
| **Precondition** | Gateway offline, có event log và telemetry trong `sync_queue.bin` |
| **Trigger** | Wi-Fi kết nối lại |
| **Main Flow** | 1. wifi_provisioning post EVT_WIFI_CONNECTED |
| | 2. rtc: sync NTP (Smooth Slew) → ghi lại DS3231 |
| | 3. mqtt_adapter kết nối lại Broker (Exponential Backoff), post EVT_MQTT_CONNECTED |
| | 4. Tiến trình đồng bộ bù (Backfill): |
| | &nbsp;&nbsp;&nbsp;&nbsp;a. Backend publish cấu hình mới qua MQTT Downlink (`gateway/{id}/config/#`) |
| | &nbsp;&nbsp;&nbsp;&nbsp;b. Gateway nạp cấu hình mới qua storage LittleFS (Atomic Write) |
| | &nbsp;&nbsp;&nbsp;&nbsp;c. uplink_task đọc `sync_queue.bin` publish bù telemetry/events theo batch (tối đa 50 rec/s) |
| | &nbsp;&nbsp;&nbsp;&nbsp;d. Nhận PUBACK từ Broker → cập nhật read_offset |
| | 5. rule_engine + scheduler tự reload khi có config mới |
| **Postcondition** | Config đồng bộ, queue offline đã gửi bù sạch, hệ thống up-to-date |

### UC-07: Force reset Wi-Fi

| | |
|---|---|
| **Actor** | User |
| **Precondition** | Gateway đã cấu hình Wi-Fi |
| **Trigger** | User giữ nút reset > 5 giây |
| **Main Flow** | 1. GPIO ISR detect giữ nút > 5s |
| | 2. `wifi_provisioning_force_reset()` |
| | 3. Xóa NVS "wifi_cfg" |
| | 4. Bật lại SoftAP "SmartClock-Setup" |
| | 5. User cấu hình lại (UC-01) |
| **Postcondition** | Gateway ở trạng thái setup |

---

## 11. Xử lý lỗi & Failure Modes

### 11.1 Ma trận lỗi firmware

| Lỗi | Phát hiện bởi | Xử lý | Hậu quả |
|---|---|---|---|
| BLE sensor mất kết nối / đọc timeout | `ble_scan_task` | Post event với `valid=false` | Rule engine giữ safe-default |
| MQTT broker mất kết nối | esp-mqtt stack | Post `EVT_MQTT_DISCONNECTED`, Exponential Backoff | MQTT device không điều khiển được, BLE device vẫn OK |
| Wi-Fi mất | esp_wifi stack | Post `EVT_WIFI_LOST` | Chuyển sang offline mode, telemetry vào SD queue |
| Event queue đầy | `event_bus_post()` | Log WARN, trả pdFALSE, tự do giải phóng payload | Không rò rỉ RAM; cần rà soát non-blocking handler |
| malloc thất bại | Mỗi module tự kiểm tra | Log ERROR, skip lần này | Có thể mất 1 chu kỳ xử lý |
| LittleFS mount fail | `storage_init()` | Log ERROR, tự format phục hồi partition | Schedule/rule nạp mặc định safe-default |
| DS3231 I2C fail | `rtc_init()` | Log ERROR, rtc_is_valid() = false | Tạm dừng scheduler theo giờ tới khi có NTP |
| NTP sync fail | `rtc_sync_from_ntp_and_save()` | Log WARN, giữ giờ DS3231 cũ | Giờ có thể lệch vài giây |
| Broker / Backend không phản hồi | `uplink_task` | Chuyển dữ liệu vào Store-and-Forward queue trên SD | Gateway chạy bình thường, không mất dữ liệu |

### 11.2 Safe-default Pattern & Ma Trận Ngắt An Toàn Cơ Cấu Chấp Hành

```
Cảm biến mất tín hiệu / Hỏng (valid = false hoặc Timeout > 60s)
    │
    ├── Rule Engine: NGỪNG đánh giá các luật tự động liên quan
    │
    ├── RELAY_2 (Máy sưởi nhiệt / Hazard):
    │     └── BẮT BUỘC NGẮT CƯỠNG CHẾ (OFF) NGAY LẬP TỨC (0ms)
    │         (Không cho phép duy trì trạng thái cũ, cố định timeout 60s, TUYỆT ĐỐI KHÔNG nới lỏng 180s cho tải nhiệt)
    │
    ├── RELAY_1 (Quạt làm mát / Fan):
    │     └── Giữ trạng thái ON nếu trước đó nhiệt độ cao, hoặc duy trì tốc độ an toàn
    │
    ├── RELAY_3 (Máy hút ẩm / Compressor): TẮT an toàn (OFF) và khóa Dwell Time 180s
    │
    └── Hiển thị cảnh báo lên màn hình OLED & Báo động Web Dashboard
```

### 11.3 Task Watchdog & Supervisor Matrix (Chuẩn hóa 7 Task Production, Timeout 15s, Mask 0x7F)

| Task | Watchdog Mechanism | Chu kỳ nhịp tim | Timeout | Hành vi xử lý khi phát hiện Task bị treo |
|---|---|:---:|:---:|---|
| `supervisor_task` | Hardware TWDT (Core 0) | 1000ms | 15s | Phần cứng ESP32-S3 tự động reboot khi supervisor bị kẹt |
| `event_dispatcher_task` | Heartbeat bit 0 | ≤ 1000ms | 15s | Ghi chẩn đoán vào RTC Slow Memory → `esp_restart()` |
| `ble_scan_task` | Heartbeat bit 1 | 1000ms | 15s | Ghi chẩn đoán vào RTC Slow Memory → `esp_restart()` |
| `uplink_task` | Heartbeat bit 2 | 1000ms | 15s | Tự kết nối lại Wi-Fi/MQTT; nếu task kẹt loop → Reset an toàn |
| `sd_log_task` | Heartbeat bit 3 | ≤ 1000ms | 15s | Flush buffer, ghi RTC Slow Memory → `esp_restart()` |
| `scheduler_task` | Heartbeat bit 4 | 1000ms | 15s | Ghi chẩn đoán vào RTC Slow Memory → `esp_restart()` |
| `display_task` | Heartbeat bit 5 | 1000ms | 15s | Ghi chẩn đoán vào RTC Slow Memory → `esp_restart()` |
| `serial_cli_task` | Heartbeat bit 6 | 1000ms (UART read timeout 1s) | 15s | Ghi chẩn đoán vào RTC Slow Memory → `esp_restart()` |

> **Quy chuẩn thời gian khôi phục an toàn (M17):**
> - Ngưỡng phát hiện task treo: **15 giây** (Chu kỳ kiểm tra 1000ms của `supervisor_task`).
> - Khi phát hiện treo, thông tin chẩn đoán (Task ID, Tên task, Free Heap, Uptime) được lưu vào **RTC Slow Memory** (không bị mất qua software reset).
> - Chân relay kích hoạt `gpio_hold_en()` để không chattering tải.
> - Thời gian khởi động lại chip ESP32-S3 đến khi nạp lại trạng thái: **~2.5 giây**.
> - Sau khi boot, firmware in báo cáo sự cố từ RTC Slow Memory ra serial console.

### 11.4 Xử lý Lỗi & Nhiễu Sóng Cảm Biến Không Dây BLE
1. **Cảm biến mất kết nối / Hết pin (Device Offline):**
   - Không nhận được gói tin quảng bá (Advertisement Beacon) của cảm biến trong **60 giây liên tiếp**. Đối với cảm biến gán cho tải nhiệt (Máy sưởi Relay 2), ngưỡng timeout **cố định 60 giây**, tuyệt đối không nới lỏng lên 180s.
   - Gateway tự động đánh dấu `is_online = false`, phát `EVT_DEVICE_OFFLINE_ALERT` mang payload `device_alert_t` (chứa `device_id` và `last_seen_monotonic_us`), đồng thời chuyển trạng thái telemetry sang `valid = false`.
   - Cả `relay_controller` và `rule_engine` đồng thời xử lý sự kiện offline:
     + `relay_controller`: Cưỡng chế ngắt tức thì (0ms) Relay 2 (Máy sưởi) về mức LOW để triệt tiêu nguy cơ hỏa hoạn do quá nhiệt cục bộ.
     + `rule_engine`: Khóa luật điều khiển liên quan (Lockout) trong 300 giây cho đến khi cảm biến hồi phục telemetry hợp lệ.
2. **Nhiễu sóng 2.4GHz / Xung đột Wi-Fi & Bluetooth (RF Coexistence):**
   - ESP32-S3 sử dụng chung 1 khối vô tuyến 2.4 GHz. Khi Wi-Fi truyền tải nặng (vd: upload sync queue), tính năng **Software Coexistence** (`CONFIG_SW_COEXIST_ENABLE=y`) tự động phân chia thời gian (TDM).
   - Nếu gói tin BLE bị mất do xung đột tần số, hệ thống không coi là lỗi; cảm biến sẽ tự động phát lại ở chu kỳ quảng bá kế tiếp.
3. **Gói tin hỏng / Sai định dạng (Corrupted Packet):**
   - NimBLE stack tự động loại bỏ các gói tin sai lệch CRC phần cứng cấp link-layer.
   - Hàm giải mã kiểm tra tính hợp lệ của Service UUID `0x181A` / BTHome V2 header và chiều dài dữ liệu; nếu sai lệch, gói tin bị hủy bỏ ngay mà không gây rò rỉ bộ nhớ.

### 11.5 Xử lý lỗi thẻ nhớ SD & Điều kiện xấu
1. **Thẻ nhớ bị rút nóng hoặc chưa cắm khi boot:**
   - Hệ thống tự động chuyển sang chế độ **RAM Buffer Fallback** (lưu tối đa 50 bản tin telemetry mới nhất).
   - Định kỳ 10 giây một lần, `sd_log_task` thử gọi `esp_vfs_fat_sdspi_mount` để tự nhận lại thẻ khi người dùng cắm vào mà không cần reboot.
2. **Thẻ nhớ đầy bộ nhớ (Disk Full):**
   - Trước mỗi lần ghi file, kiểm tra dung lượng trống qua `f_getfree`.
   - Nếu dung lượng trống < 50MB: Kích hoạt chính sách tự động xóa (FIFO purge) các file telemetry cũ nhất trong `/sdcard/data/telemetry/`, giữ lại tối thiểu 10% dung lượng trống.
3. **Mất nguồn điện đột ngột (Power Loss Protection):**
   - Mọi thao tác ghi telemetry và sync queue đều gọi `f_sync()` theo batch (mỗi 10 bản tin hoặc 5s), đảm bảo bảng FAT luôn toàn vẹn sau khi có điện trở lại.

---

## 12. Yêu cầu phi chức năng

### 12.1 Hiệu năng

| Yêu cầu | Chỉ tiêu | Ghi chú |
|---|---|---|
| Rule Engine latency | < 50ms từ lúc nhận telemetry tới lúc post action | Đo trên ESP32-S3 240MHz (160MHz nominal) |
| Scheduler accuracy | ± 5 giây so với RTC | Poll mỗi 5s |
| BLE scan interval | 10 giây (giữa các vòng poll) | Mỗi device mất 0.5–3s. Với > 3 device, vòng poll > 10s — xem §4.5.4 |
| OLED refresh rate | 1 FPS (clock screen) | Đủ cho hiển thị đồng hồ |
| Cloud sync time | < 5 giây/lần (trên mạng ổn định) | Timeout 10s nếu Backend chậm |
| Boot time | < 5 giây từ power-on tới operational | Không bao gồm Wi-Fi connect |

### 12.2 Tài nguyên

| Module | RAM ước tính | Ghi chú |
|---|---|---|
| Wi-Fi + MQTT stack | ~40-60 KB | Dùng chung buffer |
| BLE NimBLE stack | ~30-40 KB | Có thể tắt khi không scan |
| OLED buffer (u8g2) | ~4-8 KB | Tùy độ phân giải |
| Rule Engine + Scheduler | ~5-10 KB | Tùy số lượng rule |
| Event bus queue | ~1 KB | 20 slot × ~48 bytes |
| FreeRTOS task stacks | ~28 KB | 7 tasks × ~4096 bytes (watchdog mask 0x7F) |
| **Tổng ước tính** | **~115-155 KB** | ESP32-S3 có ~512 KB SRAM nội bộ |

### 12.3 Độ tin cậy

| Yêu cầu | Chỉ tiêu |
|---|---|
| Uptime | Chạy liên tục 24/7 (có task watchdog) |
| Data persistence | Schedule/device list survive reboot (SPIFFS /storage), offline sync queue (MicroSD), Wi-Fi creds (NVS) |
| Clock accuracy | ± 2 phút/tháng (DS3231), hiệu chỉnh mỗi lần có NTP |
| Offline operation | Vô hạn thời gian (giới hạn bởi MicroSD dung lượng 50.000 records cho sync_queue.bin) |

### 12.4 Bảo mật (Should-have)

| Yêu cầu | Mô tả | Ghi chú |
|---|---|---|
| MQTT TLS | Encrypt traffic gateway ↔ broker | Cấu hình `esp_mqtt_client_config_t.broker.verification` |
| API Authentication | JWT hoặc API Key cho REST API | Xem §12.4.1 |
| Wi-Fi WPA2 | STA mode dùng WPA2 (không WEP/open) | ESP-IDF mặc định hỗ trợ |
| NVS encryption | Encrypt Wi-Fi password trong NVS | ⚠️ Xem §12.4.2 — rủi ro cao |

#### 12.4.1 Phác thảo luồng Authentication

Vì hệ thống có hai loại client gọi API (Web App và Gateway), mỗi loại có cơ chế xác thực riêng:

```
┌─────────────────────────────────────────────────────────────────────┐
│                   Authentication & Transport Flows                  │
│                                                                     │
│  Web App (Browser) ──► REST API:                                   │
│  ┌──────┐    POST /auth/login     ┌──────────┐                     │
│  │ User │ ──────────────────────► │ Backend  │                     │
│  │      │ ◄────────────────────── │          │                     │
│  │      │   { access_token,       │          │                     │
│  │      │     refresh_token }     │          │                     │
│  │      │                         │          │                     │
│  │      │  GET/POST /api/v1/...   │          │                     │
│  │      │  Authorization: Bearer  │          │                     │
│  │      │  <access_token>         │          │                     │
│  └──────┘                         └──────────┘                     │
│                                                                     │
│  Gateway (ESP32-S3) ──► MQTT over TLS (Port 8883):                 │
│  ┌──────────┐  CONNECT (MQTTS)        ┌──────────┐                  │
│  │ Gateway  │ ──────────────────────► │  Broker  │                  │
│  │          │  Client ID: gw_{id}     │          │                  │
│  │          │  Username:  gw_{id}     │          │                  │
│  │          │  Password:  <token>     │          │                  │
│  └──────────┘  (Cấp từ Claim Code)   └──────────┘                  │
└─────────────────────────────────────────────────────────────────────┘
```

| Thành phần | Cơ chế | Chi tiết |
|---|---|---|
| Web App → Backend | JWT (access + refresh token) | Access token TTL: 15 phút. Refresh token TTL: 7 ngày. Lưu trong `httpOnly` cookie (chống XSS) |
| Gateway → Broker | MQTTS (TLS 1.2/1.3, port 8883) | Xác thực danh tính bằng Client ID, Username và Password/Token cấp phát qua quy trình Claim Code (§8.2) |
| Backend verify | Middleware kiểm tra JWT / Broker Auth Plugin | Route `/api/v1/*` yêu cầu Bearer JWT; Broker xác thực client qua webhook hoặc backend DB |

> **Lưu ý:** Gateway chỉ gọi duy nhất endpoint REST là `POST /api/v1/gateways/register-claim` khi xuất xưởng/chưa claim. Mọi giao tiếp vận hành sau đó diễn ra 100% qua MQTTS broker.

#### 12.4.2 NVS Encryption — Cảnh báo độ phức tạp

> **⚠️ Cảnh báo:** NVS Encryption trên ESP32 đòi hỏi kích hoạt **Flash Encryption** toàn chip, bao gồm:
>
> 1. **Burn eFuse** — thao tác **không thể revert** (OTP — one-time programmable)
> 2. **Secure Boot v2** — khuyến nghị kích hoạt cùng Flash Encryption
> 3. **Ký firmware** — mỗi lần OTA phải ký bằng private key
>
> **Rủi ro:** Burn eFuse sai cấu hình sẽ **brick thiết bị vĩnh viễn** (không thể flash lại). Quá trình debug cũng phức tạp hơn đáng kể khi Flash Encryption đang bật.
>
> **Đề xuất:** Hạ xuống **Could-have** (C06). Trong phạm vi đồ án, Wi-Fi password trong NVS plaintext là chấp nhận được vì:
> - Kẻ tấn công cần truy cập vật lý vào chip ESP32
> - Wi-Fi password gia đình không phải tài sản giá trị cao
> - Thay thế bằng cách **không log password** ra serial console là đủ cho demo
>
> Nếu triển khai production, nên thực hiện trên board riêng (không phải board duy nhất) và tham khảo [ESP-IDF Flash Encryption Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/flash-encryption.html).

---

## 13. Ràng buộc & Giả định

### 13.1 Ràng buộc phần cứng

| Ràng buộc | Mô tả chuẩn hóa thực tế |
|---|---|
| **MCU** | Espressif **ESP32-S3** (Dual-Core Xtensa LX7 @ 160MHz) |
| **Flash** | **16 MB Quad/Octal SPI Flash** (Bố cục phân vùng theo `partitions.csv`) |
| **SRAM / PSRAM** | 512 KB internal SRAM + **8 MB Embedded PSRAM** |
| **OLED** | SSD1327 128×128 (Giao tiếp I2C địa chỉ `0x3C`, 4-bit Grayscale) |
| **RTC** | DS3231 (I2C địa chỉ `0x68`, pin CR2032 backup, lưu UTC chuẩn) |
| **BLE** | **Bluetooth 5.0 LE** (Apache NimBLE stack, chế độ Passive Central Observer) |
| **Wi-Fi** | 802.11 b/g/n 2.4 GHz (Station + SoftAP Captive Portal) |
| **Radio RF** | **Dùng chung 1 transceiver RF 2.4 GHz** cho cả Wi-Fi và BLE (Software Coexistence) |
### 13.2 Ràng buộc phần mềm

| Ràng buộc | Mô tả |
|---|---|
| Framework | ESP-IDF v5.x (C thuần, không Arduino) |
| RTOS | FreeRTOS (đi kèm ESP-IDF) |
| BLE stack | NimBLE (nhẹ hơn Bluedroid ~100KB RAM) |
| JSON parser | cJSON (component "json" có sẵn ESP-IDF) |
| Filesystem | LittleFS (tương thích SPIFFS partition) |

### 13.3 Giả định & Lưu ý phần cứng

1. **Mạng Wi-Fi gia đình** — băng thông đủ cho MQTT (QoS 1) và NTP, không sử dụng HTTP polling sync.
2. **Số lượng thiết bị nhỏ** — tối đa 16 BLE sensor + 16 MQTT device, tối đa 32 rule, tối đa 50 schedule entries.
3. **Backend luôn sẵn sàng** khi có Internet — khi backend mất kết nối, hệ thống chuyển sang Store-and-Forward sang thẻ SD.
4. **Một gateway / một user** — không hỗ trợ multi-user cho cùng 1 gateway (trừ khi mở rộng).
5. **BLE Sensor Peripheral (Passive Central Observer):**
   - Gateway thu thập dữ liệu hoàn toàn bằng hình thức **Passive Advertisement Scanning**, không thực hiện kết nối BLE GATT (GATT Connection-less).
   - Hỗ trợ giải mã các chuẩn Beacon phổ biến: **BTHome V2** (UUID `0xFCD2`), **Xiaomi Mijia V2** (UUID `0xFE95`, hỗ trợ giải mã AES-128 CCM qua bindkey 16 bytes) và **Bluetooth SIG Environmental Sensing** (UUID `0x181A`).
   - Tuyệt đối không gửi lệnh ghi GATT (GATT write) tới thiết bị BLE.
6. **Màn hình OLED:**
   - Thiết kế chuẩn dùng **OLED SSD1327 128x128 4-bit grayscale**.
   - Nếu thay bằng **OLED SSD1306 128x64 đơn sắc**, cần điều chỉnh lại layout 3 màn hình và đổi driver render tương ứng.
7. **Timezone không đổi** sau khi setup — nếu muốn đổi phải force reset.
8. **BLE/WiFi đồng thời chấp nhận miss** — khi WiFi bận (gửi bù MQTT batch), BLE scan có thể miss 1–2 chu kỳ quảng bá; hệ thống dùng safe-default, không gây lỗi logic.
9. **Heap fragmentation không đáng kể** trong thời gian demo — với payload đồng nhất (~40–68 bytes) và tần suất malloc/free thấp (~8640 lần/ngày), fragmentation cần tuần/tháng mới thành vấn đề.
10. **LittleFS thread-safe** — `esp_vfs_littlefs` trong ESP-IDF tích hợp mutex tại VFS layer; kết hợp event bus ordering (ghi xong → post event → handler đọc) đảm bảo an toàn đa luồng.

---

## 14. Tiêu chí nghiệm thu

### 14.1 Mục đích

Bảng tiêu chí nghiệm thu dưới đây map từng tính năng Must-have (M01–M13) với Use Case và điều kiện pass/fail cụ thể, phục vụ làm cơ sở kiểm thử và chấm điểm.

### 14.2 Bảng nghiệm thu Must-have

| ID | Tính năng | UC liên quan | Tiêu chí Pass | Tiêu chí Fail |
|---|---|---|---|---|
| M01 | Hiển thị giờ, ngày, nhiệt độ trên OLED | UC-01 | OLED hiển thị đúng giờ hiện tại (± 5s so với NTP), ngày, nhiệt độ đọc từ sensor. Tự cập nhật mỗi giây | Giờ sai > 1 phút, hoặc OLED trống/không phản hồi sau boot |
| M02 | Quét & Đọc Telemetry BLE Sensor | UC-02, UC-03 | Gateway quét thụ động (Passive Advertisement Scan), bóc tách dữ liệu nhiệt ẩm Service 0x181A từ ≥ 1 BLE sensor đã đăng ký. Hiển thị thông số lên màn hình OLED & Web App | Không nhận được beacon BLE sau 60 giây, hoặc giá trị `valid` luôn false |
| M03 | Điều khiển cơ cấu chấp hành Relay nội bộ & Phản hồi ACK | UC-02, UC-03 | Gateway nhận lệnh từ Web App/MQTT hoặc Rule Engine, điều khiển bật/tắt đúng kênh Relay vật lý (GPIO 15..18), tuân thủ khóa liên động an toàn và gửi phản hồi ACK thành công | Lệnh điều khiển không làm đổi trạng thái Relay, hoặc vi phạm khóa liên động giữa Quạt và Sưởi |
| M04 | Rule Engine: IF condition THEN action | UC-03 | Tạo rule "IF sensor_01 > 30 THEN fan_01 = ON" trên Web. Khi sensor báo 31°C → quạt bật trong < 5s. Khi sensor báo 29°C → quạt không bật (hoặc tắt nếu có rule ngược) | Rule không trigger khi điều kiện thỏa, hoặc trigger sai (false positive) |
| M05 | Local Scheduler: automation/alarm theo giờ | UC-04 | Tạo schedule "17:30 bật đèn" trên Web. Đến 17:30 → đèn bật tự động (± 5s). Tạo alarm "06:30" → đến 06:30 buzzer kêu | Schedule không trigger đúng giờ (sai > 10s), hoặc trigger trùng (bắn 2 lần cùng phút) |
| M06 | Offline-first: hoạt động khi mất mạng | UC-05 | Ngắt Internet → scheduler vẫn trigger đúng giờ, rule engine vẫn đánh giá BLE telemetry, event log và telemetry ghi vào thẻ nhớ SD | Hệ thống treo/reboot khi mất mạng, hoặc scheduler ngừng hoạt động |
| M07 | RTC giữ giờ khi mất điện | UC-05 | Rút điện 5 phút → cắm lại → OLED hiển thị đúng giờ (± 2 phút, không cần NTP) | Giờ reset về epoch 0 hoặc sai > 5 phút sau khi mất điện |
| M08 | Cloud Sync: đồng bộ config + event log | UC-06 | Bật Internet → Gateway tự nhận schedule/device_list mới từ MQTT retained config + push dữ liệu bù từ MicroSD qua `gateway/{id}/telemetry/batch`. Backend nhận đủ dữ liệu | Sync không chạy khi có mạng, hoặc config mới không được apply, hoặc dữ liệu bị mất |
| M09 | Wi-Fi Provisioning: SoftAP + Captive Portal | UC-01 | Bật Gateway mới → xuất hiện AP "SmartClock-Setup" → kết nối → captive portal tự mở → nhập SSID/password → Gateway kết nối Wi-Fi thành công | AP không xuất hiện, captive portal không mở, hoặc Wi-Fi connect fail sau khi nhập đúng credentials |
| M10 | REST API quản lý device, schedule, rule | UC-02, UC-03, UC-04 | Tất cả endpoint CRUD (GET/POST/PUT/DELETE) cho device, schedule, rule hoạt động đúng. Response 200/201 cho success, 4xx cho lỗi client | Endpoint trả 500 cho request hợp lệ, hoặc không trả response |
| M11 | Web App: CRUD device, schedule, xem trạng thái | UC-02, UC-03, UC-04 | Web App hiển thị danh sách device/schedule/rule. Thêm/sửa/xóa hoạt động. Trạng thái device (online/offline) cập nhật | UI không load, CRUD fail, hoặc trạng thái không đồng bộ với thực tế |
| M12 | Personal schedule/alarm: CRUD + kêu chuông | UC-04 | Tạo personal_event "14:00 Họp nhóm" với `date: 2026-08-20` trên Web. Đến ngày giờ đó → OLED hiển thị + buzzer (nếu bật). Tạo alarm lặp T2-T6 → trigger đúng các ngày | Personal event không hiển thị trên OLED, hoặc alarm không trigger đúng ngày lặp |
| M13 | Nút tắt/snooze báo thức tại đồng hồ | UC-04 | Alarm đang kêu → bấm nút → buzzer tắt ngay (< 500ms). Event log ghi nhận "alarm_stopped" | Nút không phản hồi, buzzer không tắt, hoặc event log không ghi |
| **M14** | **BLE Environmental Sensing & Telemetry Ingestion** | **UC-02, UC-03** | **Gateway quét và giải mã thành công gói tin quảng bá BLE (UUID 0x181A / BTHome V2), trích xuất nhiệt độ/độ ẩm và đưa vào Event Bus trong < 500ms.** | **Không quét được cảm biến; sai lệch dữ liệu phân tích.** |
| **M15** | **SD Card Logger & Offline Store-and-Forward** | **UC-05, UC-06** | **Dữ liệu telemetry ghi vào `/sdcard/data/telemetry/*.csv`. Khi mất mạng, bản tin lưu vào `sync_queue.bin`; khi có mạng lại, tự động gửi bù không sót gói nào.** | **Mất dữ liệu khi offline; crash thẻ nhớ khi mất nguồn đột ngột.** |
| **M16** | **Config Manager & Dynamic Sensor Registration** | **UC-01** | **Quản lý cấu hình NVS bền vững; lệnh CLI `ble reg`, `wifi scan`, `config show` và Web Dashboard cập nhật thông số hệ thống chính xác.** | **Mất cấu hình khi reset; CLI/Web không lưu được thông số.** |
| **M17** | **Task Supervisor & Watchdog 24/7** | **UC-05** | **Heartbeat của 7 task production (mask 0x7F) được giám sát liên tục; khi cố tình đóng băng 1 task (vd: test freeze), supervisor ghi chẩn đoán vào RTC Slow Memory và reboot an toàn đúng 15 giây.** | **Hệ thống treo im lặng không tự reset; TWDT không kích hoạt sau 15 giây.** |

### 14.3 Bảng nghiệm thu Should-have

| ID | Tính năng | Tiêu chí Pass | Ghi chú |
|---|---|---|---|
| S01 | MQTT over TLS | Gateway kết nối broker qua `mqtts://` port 8883. Wireshark xác nhận traffic encrypted | Cần broker hỗ trợ TLS |
| S02 | Authentication | Web App login → nhận JWT → gọi API có Bearer token → 200. Gọi không có token → 401 | Xem §12.4.1 |
| S03 | OTA firmware update | Upload firmware qua Web → Backend gửi → Gateway flash OTA partition → reboot với firmware mới | Kiểm tra rollback nếu firmware lỗi |
| S04 | BLE auto-discovery | Gateway scan BLE → liệt kê device chưa đăng ký → Web App hiển thị danh sách để user chọn thêm | |
| S05 | Event logging + lịch sử | Web App hiển thị timeline event, lọc theo loại, phân trang hoạt động | |
| S06 | Luân phiên màn hình OLED | OLED tự chuyển qua 3 màn hình (Clock → Upcoming → Device Status) mỗi 5-10s. Bấm nút chuyển ngay | |
| **S07** | **BLE Sensor Dynamic Registration qua Serial CLI & Web** | **Lệnh `ble reg <id> [mac]` đăng ký và gán ID thành công thiết bị cảm biến mới mà không cần biên dịch lại firmware.** | |
| **S08** | **Cảnh báo mất tín hiệu cảm biến BLE & Kích hoạt Safe-Default** | **Khi cảm biến mất sóng hoặc gửi cờ lỗi (`valid=false`), Rule Engine kích hoạt Safe-Default bảo vệ relay và hiển thị cảnh báo.** | |

### 14.4 Quy trình nghiệm thu

1. **Chuẩn bị:** Gateway đã flash firmware, Backend đang chạy, Web App truy cập được, có ≥ 1 BLE sensor (0x181A / BTHome) + 4 kênh Relay sẵn sàng.
2. **Thực hiện:** Chạy tuần tự theo ma trận kiểm thử tại `testing/test-plan.yaml` và kịch bản demo (§15.2), đánh dấu pass/fail từng mục **M01–M17** và **S01–S08**.
3. **Ghi nhận:** Screenshot Web App, log serial console Gateway UART0 (115200 baud), trạng thái màn hình OLED và trạng thái relay vật lý cho mỗi test case.
4. **Kết quả:**
   - Tất cả **M01–M17** pass = **Đạt chuẩn sản phẩm cốt lõi (Must-have Acceptance Gate)**.
   - Thêm **S01–S08** pass = **Đạt chuẩn sản phẩm thương mại hoàn chỉnh (Should-have Full Feature)**.

### 14.5 Kiểm thử Firmware Logic trên Native Host & Compile-Time Assertions

Nhằm đảm bảo tính đúng đắn tuyệt đối của các thuật toán an toàn cốt lõi mà không phụ thuộc vào phần cứng vật lý, dự án thiết lập bộ kiểm thử C Native Host chạy trực tiếp trên môi trường máy chủ Linux/CI (sử dụng Unity Test Harness):

1. **Compile-Time Static Assertions:**
   Bắt buộc khai báo tại các header file tương ứng để chặn đứng lỗi padding hoặc lệch kích thước struct trước khi liên kết nhị phân:
   ```c
   _Static_assert(sizeof(sync_queue_record_t) == 172, "sync_queue_record_t size must be exactly 172 bytes");
   _Static_assert(sizeof(sync_queue_header_t) == 24, "sync_queue_header_t size must be exactly 24 bytes");
   _Static_assert(sizeof(event_bus_msg_t) <= 48, "event_bus_msg_t size must not exceed 48 bytes");
   ```

2. **Native Host Logic Test Suites (`test/host/`):**
   - `test_relay_interlock.c`: Kiểm tra ma trận khóa liên động an toàn bất đối xứng:
     + Khi Quạt đang ON $\rightarrow$ Gửi lệnh bật Sưởi $\rightarrow$ Lệnh bị từ chối (`ERR_RELAY_INTERLOCK_CONFLICT`), quạt tiếp tục chạy.
     + Khi Sưởi đang ON $\rightarrow$ Gửi lệnh bật Quạt $\rightarrow$ Sưởi bị ngắt tức thì trong 0ms, Quạt bật thành công (Cooling Override).
     + Kiểm tra Dwell Time 180s cho Relay 3 (Máy nén lạnh); đảm bảo Relay 1 và Relay 2 đóng cắt tức thì 0ms (không bị dwell time).
   - `test_monotonic_timing.c`: Giả lập bước nhảy thời gian NTP; kiểm tra toàn bộ timer an toàn (cooldown rule, lockout heater, dwell time compressor) duy trì tính đơn điệu không bị ảnh hưởng bởi NTP step.
   - `test_ble_parsers.c`: Bơm dữ liệu thô (raw advertisement packets) kiểm thử thuật toán giải mã BTHome V2, Xiaomi Mijia V2 (giải mã AES-128 CCM với mock bindkey), và Bluetooth SIG 0x181A.
   - `test_scheduler_snooze.c`: Kiểm tra logic báo thức, chức năng Snooze chính xác +9 phút, và xác nhận không kích hoạt chuông báo ngược (no retroactive alarm) khi boot lại lúc quá giờ.

##### Verification Contract — Native Host Logic Verification & Static Assertions (FR-TEST-01)
- **Preconditions:** Mã nguồn firmware được cấu hình build native target (`make -C test/host` hoặc `ctest`).
- **Stimulus:** Chạy script thực thi toàn bộ test harness native trên Linux host mà không gắn board ESP32-S3.
- **Expected:**
  1. Toàn bộ `_Static_assert` pass 100% khi compile.
  2. 100% các ca kiểm thử liên quan đến Interlock Logic, Monotonic Timer, BLE Parser và Snooze 9 phút pass mà không có memory leak (kiểm tra qua Valgrind).
- **Timing/Tolerance:** Toàn bộ test suite hoàn thành thực thi trong $\le 5$ giây trên CI/CD runner.
- **Prohibited:** Nghiêm cấm bỏ qua static assertion hoặc cho phép bỏ qua test case interlock an toàn.
- **Tier:** Unit / Host Logic Test (`test/host/test_runner.c`).
- **Operator Instructions:** Chạy lệnh `gcc -DTEST_HOST test/host/*.c -o host_tests && ./host_tests` và xác nhận kết quả `ALL TESTS PASSED`.

---

## 15. Phụ lục

### 15.1 Sơ đồ phân bổ Flash Partition (16MB SPI Flash — ESP32-S3)

Khớp chuẩn xác với file cấu hình [`partitions.csv`](firmware/partitions.csv) của dự án:

| Tên Partition | Phân loại | Kiểu con | Offset | Kích thước | Mục đích sử dụng |
|---|---|---|---|---|---|
| `nvs` | data | nvs | `0x9000` | 24 KB (`0x6000`) | Lưu trữ tham số hệ thống, Wi-Fi credentials, Broker URI, Timezone |
| `otadata` | data | ota | `0xF000` | 8 KB (`0x2000`) | Quản lý trạng thái chuyển đổi slot nạp OTA và Rollback |
| `phy_init` | data | phy | `0x11000` | 4 KB (`0x1000`) | Dữ liệu cân chỉnh tần số sóng RF Wi-Fi/Bluetooth |
| `factory` | app | factory | `0x20000` | 2 MB (`0x200000`) | Firmware xuất xưởng ban đầu |
| `ota_0` | app | ota_0 | `0x220000` | 2 MB (`0x200000`) | Khe nạp ứng dụng OTA chính |
| `ota_1` | app | ota_1 | `0x420000` | 2 MB (`0x200000`) | Khe nạp ứng dụng OTA phụ (đối xứng phục vụ Rollback) |
| `storage` | data | spiffs | `0x620000` | 2 MB (`0x200000`) | Phân vùng SPIFFS chứa `schedule.json`, `rules.json`, ringbuffer offline |
| *(reserved)* | data | - | `0x820000` | ~8 MB | Vùng nhớ mở rộng dành cho coredump, font OLED u8g2 và asset |

---

### 15.2 Kịch bản demo

1. **Setup lần đầu:** bật đồng hồ → kết nối AP → nhập Wi-Fi → đồng hồ hiển thị đúng giờ
2. **Thêm thiết bị:** Web App → thêm sensor BLE + actuator MQTT
3. **Tạo automation:** Web App → IF sensor_01 > 30°C THEN fan_01 = ON
4. **Tạo schedule:** Web App → 15:30 bật đèn phòng ngủ
5. **Tạo alarm:** Web App → 06:30 T2-T6 "Dậy đi học" + buzzer
6. **Ngắt Internet**
7. **Chờ 15:30** → đèn vẫn bật (offline execution)
8. **Giả lập alarm** → chuông kêu → bấm nút tắt (thao tác local)
9. **Bật Internet** → Backend nhận event log, đồng bộ trạng thái
10. (Tuỳ chọn) Mô phỏng conflict: sửa schedule trên Web lúc offline, xem sync

### 15.3 Tham khảo

| Nguồn | Mục đích |
|---|---|
| tonyp7/esp32-wifi-manager | SoftAP + Captive Portal (dns_server.c, http_app.c) |
| alvarowolfx/wasm-ble-edge-filter | BLE scan task pattern, main.cpp cấu trúc |
| ESP-IDF Programming Guide | API reference cho NVS, Wi-Fi, MQTT, NimBLE, LittleFS |
| u8g2 library | OLED driver cho SSD1327 |

*— Hết tài liệu FSD —*

