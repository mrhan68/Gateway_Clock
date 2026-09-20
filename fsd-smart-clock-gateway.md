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
- [Phụ lục B: Mô phỏng Thiết bị Modbus RS485 (Mock Test Harness)](#phụ-lục-b-mô-phỏng-thiết-bị-modbus-rs485-mock-test-harness)

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
         ┌─────────────┴─────────────┐                                 ┌─────────────┴─────────────┐
         ▼                           ▼                                 ▼                           ▼
┌──────────────────┐       ┌──────────────────┐               ┌──────────────────┐       ┌──────────────────┐
│ Wi-Fi Router LAN │       │ Thiết bị Wi-Fi   │               │ Cảm biến BLE T&H │       │ BLE Smart Plug   │
│ - SoftAP Portal  │       │ - Smart Socket   │               │ - Đọc Telemetry  │       │ - Điều khiển     │
│ - Web Dashboard  │       │ - Tasmota / Tuya │               │ - Beacon / Adv   │       │   bật/tắt quạt   │
│ - MQTT Broker    │       │   qua MQTT       │               │   (GATT Read)    │       │   (GATT Write)   │
└──────────────────┘       └──────────────────┘               └──────────────────┘       └──────────────────┘
```

#### Quy tắc mạng không dây:
- **Tập trung 100% không dây:** Toàn bộ liên kết giữa Gateway với các cảm biến và thiết bị chấp hành trong nhà đều thông qua sóng **Wi-Fi** hoặc **Bluetooth Low Energy (BLE)**, loại bỏ hoàn toàn nhu cầu đục tường kéo cáp vật lý.
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
| **M14** | *(Đã lược bỏ)* RS485 Polling: Không áp dụng cho phiên bản Smart Home không dây | *(Out of Scope)* |
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
│   │  uplink_task           (Priority 5) - Quản lý Wi-Fi, MQTT & Reconnect   │         │
│   │  sd_log_task           (Priority 4) - Ghi FATFS: Log, Telemetry, Queue  │         │
│   │  cloud_sync_task       (Priority 3) - Đồng bộ cấu hình & Batch Sync HTTP│         │
│   │  serial_cli_task       (Priority 2) - Giao diện lệnh cứu hộ UART0       │         │
│   │  wifi_provisioning     (Priority 2) - SoftAP + Web Captive Portal       │         │
│   └─────────────────────────────────────────────────────────────────────────┘         │
│                                                                                       │
│   ┌─────────────────────────────────────────────────────────────────────────┐         │
│   │                    HỆ THỐNG QUEUE ĐỆM & EVENT BUS                       │         │
│   │  - Event Bus Queue: 32 slots (phân phối sự kiện toàn hệ thống)          │         │
│   │  - BLE Command Queue: 10 slots (hàng đợi lệnh GATT Write / Điều khiển)  │         │
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
  Server ──(MQTT Topic: gateway/{id}/command)──► uplink_task
                                         │
                          [EVT_DEVICE_COMMAND_REQUEST]
                                         │
        ┌────────────────────────────────┼────────────────────────────────┐
        ▼                                ▼                                ▼
   local_relays                     ble_adapter                         display
 (Relay 1-4 GPIO)               (s_ble_cmd_queue)                  (cập nhật status)
        │                                │                                │
  Bật/tắt cuộn hút               Phát BLE Action                   Hiển thị màn hình
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
  Gộp nhiều bản tin thành Batch (50 records/payload JSON) gửi qua MQTT QoS 1 (2 batch/giây = 100 records/s), Backend dedupe theo (device_id, timestamp)
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

1. **Event bus cho xuyên module**: Mọi giao tiếp A → B (A ≠ B) phải đi qua event bus. Không gọi hàm trực tiếp giữa hai module khác nhau (ngoại lệ duy nhất: `display.c` → `storage_append_event_log()` cho ghi log alarm stop).

2. **Dispatcher owns free()**: Producer malloc, handler chỉ đọc, dispatcher free sau khi gọi hết handler. Không có ngoại lệ.

3. **Inversion of Control**: Module không biết module khác tồn tại. Ví dụ: `storage.c` post `EVT_CONFIG_UPDATED_FROM_CLOUD`, không gọi `rule_engine_reload_rules()`.

4. **Gọi trực tiếp cho nội bộ module**: `display_task` gọi `display_render()` trực tiếp, không post event lên bus.

5. **Handler không được block dispatcher**: Mọi handler chạy trong context của `event_dispatcher_task` (tuần tự, 1 task duy nhất). Nếu handler gọi API blocking (BLE connect, GATT write timeout, TCP write...), **toàn bộ event bus bị đứng** — kể cả `EVT_BUTTON_ALARM_STOP`. Do đó: handler chỉ được enqueue lệnh vào command queue riêng của adapter, task riêng của adapter thực hiện I/O thực tế. Xem §4.5.5 và §4.6.3 cho chi tiết.

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
| Queue length | 32 slots | Đủ cho burst event bình thường |
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
| `EVT_BLE_COMMAND_REQUEST` | rule_engine, mqtt_adapter | ble_adapter | `rule_action_request_t*` |
| `EVT_DEVICE_OFFLINE_ALERT` | ble_adapter, uplink_task | display, sd_log_task, uplink_task | `device_alert_t*` |
| `EVT_SD_STATUS_CHANGED` | sd_logger | display, supervisor | `sd_status_t*` |
| `EVT_CONFIG_RELOAD_REQUEST` | serial_cli, config_mgr | uplink_task, rule_engine, ble_adapter | NULL |
| `EVT_MQTT_MESSAGE_RECEIVED` | mqtt_adapter | rule_engine, display | `mqtt_telemetry_t*` |
| `EVT_MQTT_CONNECTED` | mqtt_adapter | storage, display, sd_log_task | NULL |
| `EVT_MQTT_DISCONNECTED` | mqtt_adapter | display, sd_log_task | NULL |
| `EVT_DEVICE_COMMAND_REQUEST` | mqtt_adapter, serial_cli | relay_controller, ble_adapter | `device_command_req_t*` |
| `EVT_RULE_ACTION_REQUEST` | rule_engine, scheduler | relay_controller, ble_adapter, display | `rule_action_request_t*` |
| `EVT_COMMAND_RESPONSE` | relay_controller, ble_adapter | uplink_task, sd_log_task | `command_ack_t*` |
| `EVT_RELAY_STATE_CHANGED` | relay_controller | rule_engine, display, sd_log_task | `relay_state_changed_t*` |
| `EVT_SCHEDULE_AUTOMATION_TRIGGERED` | scheduler | rule_engine, relay_controller | `automation_rule_t*` |
| `EVT_SCHEDULE_ALARM_TRIGGERED` | scheduler | display | `alarm_entry_t*` |
| `EVT_SENSOR_VALUE_INVALID` | rule_engine, ble_adapter | relay_controller, display, sd_log_task | NULL |
| `EVT_BUTTON_ALARM_STOP` | GPIO ISR / display_task | display, sd_log_task | NULL |
| `EVT_BUTTON_SCREEN_NEXT` | GPIO ISR / display_task | display | NULL |
| `EVT_DISPLAY_UPDATE_REQUEST` | (bất kỳ module) | display | NULL |
| `EVT_CONFIG_UPDATED_FROM_CLOUD` | storage | rule_engine, scheduler | `config_updated_t*` |
| `EVT_EVENT_LOG_APPENDED` | storage, sd_logger | (cloud sync trigger) | NULL |
| `EVT_SYNC_COMPLETED` | storage, sd_logger | display | NULL |

> **Phân biệt luồng lệnh điều khiển thiết bị:**
> - `EVT_RULE_ACTION_REQUEST`: Do **Rule Engine** hoặc **Scheduler** sinh ra tự động từ các quy tắc cục bộ.
> - `EVT_DEVICE_COMMAND_REQUEST`: Do **MQTT Adapter** (từ Web App) hoặc **Serial CLI** sinh ra từ thao tác thủ công của người dùng.
> - Cả hai luồng khi tác động lên 4 kênh rơ-le trên bo mạch đều do **`relay_controller`** tiếp nhận, kiểm tra khóa liên động an toàn (Interlock), xuất mức logic ra GPIO, và bắt buộc phát sinh **`EVT_COMMAND_RESPONSE`** để gửi ACK về Cloud.

#### 4.1.5 Shared Payload Structs

```c
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
    char     reason[32];           // "MANUAL_CMD", "RULE_TRIGGER", "SAFETY_CUTOFF", "SENSOR_OFFLINE"
    uint32_t timestamp;            // Epoch
} relay_state_changed_t;

typedef struct {
    char     command_id[36];       // UUID v4 định danh lệnh (chống trùng lặp & đối soát ACK)
    char     target_device_id[32]; // Thiết bị nhận lệnh
    char     action[32];           // "ON", "OFF", "SET:75", "TOGGLE"
    uint32_t ttl_seconds;          // Thời hạn hiệu lực của lệnh (mặc định 60s)
    uint32_t timestamp;            // Epoch thời điểm phát lệnh
} device_command_req_t;

typedef struct {
    char     command_id[36];       // Khớp command_id của yêu cầu gửi xuống
    char     target_device_id[32]; // Thiết bị thực thi
    bool     success;              // true nếu thực thi thành công
    int      error_code;           // 0: OK, 1: TIMEOUT, 2: INVALID_CMD, 3: EXPIRED
    char     message[64];          // Mô tả kết quả thực thi
    uint32_t executed_at;          // Thời điểm thực thi xong
} command_ack_t;

typedef struct {
    char     command_id[36];       // Tự sinh UUID cho action do Rule kích hoạt
    char     target_device_id[32]; // Adapter tự tra cứu danh sách thiết bị
    char     command[32];          // VD: "ON", "OFF", "SET:75"
    uint32_t timestamp;            // Epoch kích hoạt
} rule_action_request_t;

typedef struct {
    bool schedule_changed;         // schedule.json đã được cập nhật từ Cloud
    bool rules_changed;            // rules.json đã được cập nhật từ Cloud
    bool ble_devices_changed;      // device_list.json đã được cập nhật
} config_updated_t;
```

#### 4.1.6 Quy tắc bộ nhớ & Nguyên Tắc Không Gây Nghẽn Dispatcher (Non-Blocking Dispatcher)

1. **Nguyên tắc bất biến của Dispatcher:** Dispatcher handler **tuyệt đối không được thực hiện I/O blocking**, không gọi kết nối mạng (SNTP, HTTP), không ghi Flash/SD Card, và không thực hiện parse JSON cJSON phức tạp. Handler chỉ được phép sao chép dữ liệu nhỏ vào task queue của worker tương ứng hoặc bật cờ sự kiện.
2. **Quy tắc giải phóng bộ nhớ (Memory Ownership):**
   - **Cấp phát:** Module phát sự kiện (Producer) chịu trách nhiệm cấp phát động bộ nhớ payload qua `malloc()` nếu có dữ liệu gửi kèm (hoặc truyền `data = NULL` nếu không có payload).
   - **Giải phóng duy nhất:** Dispatcher task sau khi gọi lần lượt tất cả handler đã đăng ký cho event code đó sẽ gọi `free(evt.data)` một lần duy nhất tại điểm kết thúc vòng lặp. Các handler nhận event tuyệt đối **không được gọi free()** trên con trỏ nhận được.
   - **Xử lý chống tràn & rò rỉ khi Queue đầy:** Nếu hàng đợi Event Bus (32 slots) bị đầy dẫn đến `event_bus_post()` thất bại (trả về `pdFALSE`), chính hàm `event_bus_post()` có nghĩa vụ giải phóng `free(data)` ngay lập tức và ghi log cảnh báo `ESP_LOGW("Event queue full, payload freed")` để loại trừ hoàn toàn nguy cơ rò rỉ bộ nhớ (Memory Leak).
3. **Cơ chế xử lý nút bấm vật lý (Chống nghẽn Queue):**
   - Để tránh rủi ro mất sự kiện khi hàng đợi Event Bus bị đầy hoặc nghẽn, tín hiệu ngắt nút bấm vật lý (GPIO 0 `BTN_BOOT_ALARM` và GPIO 4 `BTN_SCREEN_NEXT`) sử dụng cơ chế **Direct-to-Task Notification** (`vTaskNotifyGiveFromISR`) gửi thẳng tới `display_task`.
   - `display_task` phản hồi ngay lập tức (Snooze +9m, Tắt chuông hoặc đổi trang màn hình OLED) mà không cần đợi luân chuyển qua Event Queue. Sau đó, `display_task` mới post sự kiện nhật ký lên Event Bus ở ngữ cảnh task thông thường (`from_isr = false`).
4. **An toàn đa luồng (Mutex Protection):**
   - Bảng lịch biểu `entries[]` của Scheduler được bảo vệ bởi `s_scheduler_mutex`.
   - Bảng đăng ký thiết bị `s_known_devices` được bảo vệ bởi `s_device_mutex`.
   - Các thao tác đọc/ghi file nhật ký sự kiện `event_log.json` và `sync_queue.bin` được bảo vệ bởi `s_storage_mutex`.

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
// Hành vi: esp_sntp đồng bộ → ghi giờ mới vào DS3231
// Trả về: true nếu đồng bộ thành công
// Gọi khi: EVT_WIFI_CONNECTED handler

void rtc_set_timezone(const char *tz_string);
// Hành vi: setenv("TZ", tz_string, 1); tzset();
// Input ví dụ: "ICT-7"
// Gọi khi: Nhận timezone từ captive portal form
```

#### 4.2.4 Hardware Interface

| Tham số | Giá trị |
|---|---|
| Chip | DS3231 |
| Giao tiếp | I2C |
| SDA pin | GPIO 8 (mặc định ESP32) |
| SCL pin | GPIO 9 (mặc định ESP32) |
| I2C address | 0x68 |
| Clock speed | 400 kHz |
| Pin backup | CR2032 (giữ giờ ~8 năm khi mất điện chính) |

#### 4.2.5 Ràng buộc

- **Mọi module** gọi `rtc_get_time()`, không bao giờ gọi `time(NULL)` trực tiếp
- NTP sync chỉ chạy 1 lần khi vừa có Wi-Fi, không poll liên tục
- Nếu chưa có NTP và DS3231 cũng chưa được set → giờ sẽ sai, scheduler vẫn chạy nhưng kết quả không đúng mốc thời gian thực

---

### 4.3 Module: Storage (`storage.h/.c`)

#### 4.3.1 Mô tả
Quản lý lưu trữ local (LittleFS trên flash) và đồng bộ cloud. Là nơi duy nhất giao tiếp với Backend để pull config và push event log.

#### 4.3.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| ST-01 | Mount LittleFS | `esp_vfs_littlefs_register()` cho partition "storage" |
| ST-02 | Lưu/đọc schedule.json | Backend là source of truth, Gateway chỉ đọc |
| ST-03 | Lưu/đọc device_list.json | Tương tự schedule.json |
| ST-04 | Append/đọc/xóa event_log.json | Gateway ghi, Backend chỉ đọc |
| ST-05 | Cloud sync task | Chạy Core 1, pull config → push log → post EVT_CONFIG_UPDATED |
| ST-06 | Đánh thức sync khi có mạng | Handler EVT_WIFI_CONNECTED/MQTT_CONNECTED → give semaphore |

#### 4.3.3 API

```c
void storage_init(void);
// Hành vi: Mount LittleFS, tạo semaphore, tạo cloud_sync_task (Core 1, priority 4)
//          Register handler EVT_WIFI_CONNECTED + EVT_MQTT_CONNECTED

bool storage_save_schedule_json(const char *json_str);
// Hành vi: Ghi đè /spiffs/schedule.json
// Gọi bởi: cloud_sync_task sau khi pull từ Backend

char *storage_load_schedule_json(void);
// Hành vi: Đọc /spiffs/schedule.json
// Trả về: Chuỗi JSON (caller PHẢI free), hoặc "[]" nếu file rỗng/không tồn tại

bool storage_save_device_list_json(const char *json_str);
char *storage_load_device_list_json(void);
// Tương tự schedule_json

bool storage_append_event_log(const char *event_json_line);
// Hành vi: Append 1 dòng JSON vào /spiffs/event_log.json
//          Post EVT_EVENT_LOG_APPENDED
// Gọi bởi: display.c khi user tắt alarm (ngoại lệ local duy nhất)

char *storage_get_pending_event_log(void);
// Hành vi: Đọc toàn bộ dòng chưa sync
// Trả về: Chuỗi JSON array (caller PHẢI free)

void storage_clear_synced_event_log(void);
// Hành vi: Xóa các dòng đã sync thành công

void storage_trigger_cloud_sync(void);
// Hành vi: (Chạy trong cloud_sync_task, Core 1, ĐƯỢC PHÉP block)
//   1. GET /gateway/{id}/schedule → so sánh, nếu khác → ghi đè local
//   2. GET /gateway/{id}/devices  → tương tự
//   3. POST /gateway/{id}/events  → upload event_log
//   4. Nếu có thay đổi config → post EVT_CONFIG_UPDATED_FROM_CLOUD
//   5. post EVT_SYNC_COMPLETED
```

#### 4.3.4 Cấu trúc file trên LittleFS

```
/spiffs/
├── schedule.json        ← Backend ghi đè, Gateway chỉ đọc
├── device_list.json     ← Backend ghi đè, Gateway chỉ đọc
└── event_log.json       ← Gateway ghi, Backend chỉ đọc rồi xóa entry đã sync
```

#### 4.3.5 Quy tắc xung đột đồng bộ

| Loại dữ liệu | Source of Truth | Gateway quyền gì | Backend quyền gì |
|---|---|---|---|
| `schedule.json` | **Backend** | Chỉ đọc | Đọc + Ghi |
| `device_list.json` | **Backend** | Chỉ đọc | Đọc + Ghi |
| `event_log.json` | **Gateway** | Đọc + Ghi | Chỉ đọc |

#### 4.3.6 Cloud Sync Flow

```
                    cloud_sync_task (Core 1)
                           │
    ┌──────────────────────┤
    ▼                      ▼
xSemaphoreTake()      ← đợi vô hạn ← on_network_available() give()
    │
    ▼
1. GET /gateway/{id}/schedule
    │── response OK? ──► so sánh với local
    │                     │── khác? ──► ghi đè schedule.json
    │                     │── giống? ──► bỏ qua
    │── request fail? ──► log ERROR, tiếp tục
    │
2. GET /gateway/{id}/devices (tương tự)
    │
3. POST /gateway/{id}/events (upload event_log tích lũy)
    │── 200 OK? ──► storage_clear_synced_event_log()
    │── fail? ──► giữ lại, thử lại lần sync sau
    │
4. Nếu bước 1 hoặc 2 có thay đổi:
    │── malloc(config_updated_t)
    │── post EVT_CONFIG_UPDATED_FROM_CLOUD
    │
5. post EVT_SYNC_COMPLETED
    │
    ▼
quay lại xSemaphoreTake() chờ lần có mạng tiếp
```

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
 │                         │                       │             │  → cloud_sync_task()
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
| BA-01 | Quản lý danh sách cảm biến BLE | Tra cứu `s_known_devices[]`, nạp từ `/spiffs/device_list.json` lúc boot |
| BA-02 | Passive Scan Advertisement | `ble_scan_task` (Core 0, priority 5), quét liên tục/phân kỳ (Software Coex) |
| BA-03 | Post telemetry lên bus | Đẩy `EVT_BLE_DEVICE_DATA` với `ble_telemetry_t*` (nhiệt độ, độ ẩm, pin, rssi) |
| BA-04 | Phát hiện cảm biến offline | Không nhận beacon sau **60 giây** → post `EVT_DEVICE_OFFLINE_ALERT` |
| BA-05 | Báo cáo nhịp tim Supervisor | Định kỳ 1000ms gọi `supervisor_report_heartbeat(TASK_HEARTBEAT_BLE)` |

#### 4.5.3 API

```c
void ble_adapter_init(void);
// Hành vi: Nạp danh sách thiết bị từ /spiffs/device_list.json
//          Khởi tạo ngăn xếp Apache NimBLE ở chế độ GAP Central / Observer
//          Tạo ble_scan_task (Core 0, Priority 5, Stack 4096)

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
    │     ├── Kiểm tra Service Data (UUID 0x181A hoặc BTHome):
    │     │     │── Sai format / CRC hỏng? ──► Hủy bỏ gói tin (free)
    │     │     │── Hợp lệ? ↓
    │     │
    │     ├── So khớp MAC Address với danh sách s_known_devices[]:
    │     │     │── Không khớp? ──► Bỏ qua
    │     │     │── Khớp! ↓
    │     │
    │     ├── Cập nhật last_seen_timestamp = now
    │     └── Cấp phát malloc(ble_telemetry_t), điền dữ liệu:
    │           valid = true, temp, hum, batt, rssi, ts
    │           event_bus_post(EVT_BLE_DEVICE_DATA, telem, len, false)
    │
    └── 3. Quét kiểm tra timeout thiết bị trong bảng:
          Với mỗi device: (now - last_seen > 60s)?
            ──► is_online = false, post EVT_DEVICE_OFFLINE_ALERT
```

> **Ghi chú vô tuyến Software Coexistence:** ESP32-S3 dùng chung 1 khối RF 2.4GHz. Khi Wi-Fi truyền tải nặng, ngăn xếp Software Coexistence tự động điều phối khe thời gian (TDM). BLE Adapter không block task; các gói beacon bị lỡ do xung đột sẽ tự động được nhận ở chu kỳ phát tiếp theo của cảm biến (chu kỳ phát thông thường 5–10s).

### 4.6 Module: MQTT Adapter (`mqtt_adapter.h/.c`)

#### 4.6.1 Mô tả
Protocol Adapter phía MQTT — giao tiếp với Backend và các thiết bị MQTT-native. Pattern đối xứng hoàn toàn với BLE Adapter.

#### 4.6.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| MA-01 | Kết nối MQTT broker | esp-mqtt, có LWT ("offline") |
| MA-02 | Subscribe topics | `gateway/{id}/device/+/command`, `gateway/{id}/config/schedule` |
| MA-03 | Nhận lệnh từ Rule Engine | Handler `EVT_RULE_ACTION_REQUEST` → `is_my_device()` → publish |
| MA-04 | Publish telemetry/status | `mqtt_adapter_publish()` |
| MA-05 | Post events lên bus | `EVT_MQTT_CONNECTED`, `EVT_MQTT_DISCONNECTED`, `EVT_MQTT_MESSAGE_RECEIVED` |
| MA-06 | Forward lệnh từ Web App | Nhận MQTT command → post `EVT_REMOTE_COMMAND_REQUEST` → adapter tự nhận (kể cả BLE) |

#### 4.6.3 API

```c
void mqtt_adapter_init(const char *broker_uri, const char *gateway_id);
// Hành vi: Init esp-mqtt client với LWT
//          Register handler EVT_RULE_ACTION_REQUEST
//          Register handler EVT_REMOTE_COMMAND_REQUEST
// Ví dụ: mqtt_adapter_init("mqtt://broker.local:1883", "gateway_001")

bool mqtt_adapter_publish(const char *topic_suffix, const char *payload_json);
// Hành vi: Publish tới "gateway/{id}/{topic_suffix}"
// Ví dụ: mqtt_adapter_publish("device/fan_01/telemetry", "{\"status\":\"ON\"}")
// QUAN TRỌNG: Khi gọi từ dispatcher handler (on_rule_action_request),
//   sử dụng timeout=0 (non-blocking — chỉ enqueue vào esp-mqtt outbox).
//   esp_mqtt_client_publish() với timeout=0 trả về ngay, không block dispatcher.
```

#### 4.6.4 MQTT Topics

| Topic | Direction | QoS | Mô tả |
|---|---|---|---|
| `gateway/{id}/status` | Publish | 0 | "online"/"offline" (retained, LWT) |
| `gateway/{id}/device/{did}/telemetry` | Publish | 0 | Telemetry từ device |
| `gateway/{id}/device/{did}/command` | Subscribe | 1 | Lệnh cho device từ Backend |
| `gateway/{id}/config/schedule` | Subscribe | 1 | Backend đẩy schedule mới |
| `gateway/{id}/sync/event_log` | Publish | 1 | Upload event log |

#### 4.6.5 LWT (Last Will and Testament)

```c
esp_mqtt_client_config_t cfg = {
    .broker.address.uri = broker_uri,
    .session.last_will = {
        .topic  = "gateway/{id}/status",
        .msg    = "offline",
        .retain = 1,  // giữ trên broker để Web App biết ngay
    }
};
```

Khi gateway mất kết nối đột ngột (không kịp gửi "offline"), broker tự publish LWT → Web App hiển thị đúng trạng thái.

#### 4.6.6 Remote Command Routing (Luồng Web App → BLE device)

Khi user bấm nút ON/OFF trên Web App cho một thiết bị BLE, lệnh đi theo đường:

```
Web App                Backend              MQTT Broker          Gateway (mqtt_adapter)
  │                      │                     │                      │
  ├─ POST /devices/      │                     │                      │
  │  fan_01/command ────►│                     │                      │
  │  {"command":"ON"}    │── publish ──────────►│                      │
  │                      │  gateway/001/device/ │                      │
  │                      │  fan_01/command      │── deliver ──────────►│
  │                      │                     │                      │
  │                      │                     │   on_mqtt_command_received():
  │                      │                     │   ├── parse device_id + command
  │                      │                     │   ├── malloc(rule_action_request_t)
  │                      │                     │   └── post EVT_REMOTE_COMMAND_REQUEST
  │                      │                     │                      │
  │                      │                     │   dispatcher gọi handler:
  │                      │                     │   ├── ble_adapter.on_remote_command()
  │                      │                     │   │   is_my_device("fan_01")? → YES
  │                      │                     │   │   → GATT write "ON" → quạt bật ✓
  │                      │                     │   │
  │                      │                     │   └── mqtt_adapter.on_remote_command()
  │                      │                     │       is_my_device("fan_01")? → NO → skip
```

> **Tại sao không tái sử dụng `EVT_RULE_ACTION_REQUEST`?** Tách riêng giúp phân biệt lệnh tự động (rule) và lệnh thủ công (user) trong log/debug. Nếu cần đơn giản hóa, có thể merge hai event code này — logic handler giống hệt nhau.

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


### 4.7 Module: Rule Engine (`rule_engine.h/.c`)

#### 4.7.1 Mô tả
Đánh giá "IF condition THEN action" hoàn toàn tại local. Chỉ xử lý **automation** (điều khiển thiết bị), KHÔNG xử lý alarm (hiển thị/kêu chuông — đó là việc của display.c).

#### 4.7.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| RE-01 | Nạp rule từ storage | Đọc và phân tích cú pháp tệp `/spiffs/rules.json` |
| RE-02 | Đánh giá telemetry BLE | Handler `EVT_BLE_DEVICE_DATA`: so khớp rule |
| RE-03 | Đánh giá telemetry MQTT | Handler `EVT_MQTT_MESSAGE_RECEIVED`: tương tự |
| RE-04 | Thực thi automation theo giờ | Handler `EVT_SCHEDULE_AUTOMATION_TRIGGERED` |
| RE-05 | Post action request | `EVT_RULE_ACTION_REQUEST` → adapter tự nhận |
| RE-06 | Safe-default | `valid=false` → KHÔNG đánh giá, giữ trạng thái cuối |
| RE-07 | Reload rules | Handler `EVT_CONFIG_UPDATED_FROM_CLOUD` → `rule_engine_reload_rules()` |

#### 4.7.3 API

```c
void rule_engine_init(void);
// Hành vi: Nạp rules từ storage
//          Register handler: EVT_BLE_DEVICE_DATA,
//                            EVT_SCHEDULE_AUTOMATION_TRIGGERED,
//                            EVT_CONFIG_UPDATED_FROM_CLOUD

void rule_engine_reload_rules(void);
// Hành vi: Đọc lại rules từ storage (sau khi sync config mới)
```

#### 4.7.4 Rule Data Model

```c
typedef enum {
    RULE_CONDITION_SENSOR_GT,   // sensor_value >= threshold (Ngưỡng trên, vd: Quạt làm mát)
    RULE_CONDITION_SENSOR_LT,   // sensor_value <= threshold (Ngưỡng dưới, vd: Máy sưởi nhiệt)
    RULE_CONDITION_TIME_MATCH,  // Đúng giờ hẹn (do scheduler lọc sẵn)
} rule_condition_type_t;

typedef struct {
    int id;
    char device_id[32];              // Cảm biến phát sinh dữ liệu (vd: "ble_temp_01")
    rule_condition_type_t condition_type;
    float threshold;                 // Ngưỡng nhiệt độ / độ ẩm kích hoạt
    float hysteresis;                // Biên trễ chống dao động tiếp điểm (mặc định 1.5°C)
    uint32_t cooldown_s;             // Thời gian làm nguội tối thiểu cho chiều BẬT (mặc định 60s)
    char target_device_id[32];       // Relay đích ("relay_1", "relay_2", ...)
    char action_command[32];         // "ON", "OFF", "SET:75"
    bool is_active;                  // Trạng thái bật/tắt của luật
    // Quản lý trạng thái nội bộ (Runtime State)
    bool current_state;              // true: đang trong vùng kích hoạt (Active Zone)
    uint32_t last_triggered_ts;      // Epoch thời điểm kích hoạt gần nhất để tính cooldown
    uint32_t lockout_until_ts;       // Epoch thời hạn khóa an toàn sau khi bị ngắt khẩn cấp
} automation_rule_t;
```

#### 4.7.5 Thuật toán Hysteresis, Kích hoạt cạnh (Edge-Triggering) & Đồng bộ an toàn

1. **Công thức Hysteresis chuẩn xác cho cả hai chiều:**
   - **Chiều Ngưỡng Trên (`SENSOR_GT` — Quạt làm mát `relay_1`, threshold = 30.0°C, hysteresis = 1.5°C):**
     - **Kích hoạt BẬT quạt (`relay_1 ON`):** Khi $T \ge 30.0^\circ\text{C}$ VÀ `current_state == false` VÀ $(t_{\text{now}} - t_{\text{last}} \ge 60\text{s})$ VÀ $(t_{\text{now}} \ge t_{\text{lockout}})$.
       - *Chuyển trạng thái:* `current_state = true`, gán $t_{\text{last}} = t_{\text{now}}$, post `EVT_RULE_ACTION_REQUEST` (`command = "ON"`).
     - **Giải phóng TẮT quạt (`relay_1 OFF`):** Khi $T \le (30.0 - 1.5) = 28.5^\circ\text{C}$ VÀ `current_state == true`.
       - *Chuyển trạng thái:* `current_state = false`, **ngay lập tức** post `EVT_RULE_ACTION_REQUEST` (`command = "OFF"`, bypass cooldown).
     - **Vùng đệm giữ nguyên (Deadband):** Khi $28.5^\circ\text{C} < T < 30.0^\circ\text{C}$, không phát sinh lệnh, giữ nguyên trạng thái.
   - **Chiều Ngưỡng Dưới (`SENSOR_LT` — Máy sưởi `relay_2`, threshold = 22.0°C, hysteresis = 1.5°C):**
     - **Kích hoạt BẬT sưởi (`relay_2 ON`):** Khi $T \le 22.0^\circ\text{C}$ VÀ `current_state == false` VÀ $(t_{\text{now}} - t_{\text{last}} \ge 60\text{s})$ VÀ $(t_{\text{now}} \ge t_{\text{lockout}})$.
       - *Chuyển trạng thái:* `current_state = true`, gán $t_{\text{last}} = t_{\text{now}}$, post `EVT_RULE_ACTION_REQUEST` (`command = "ON"`).
     - **Giải phóng TẮT sưởi (`relay_2 OFF`):** Khi $T \ge (22.0 + 1.5) = 23.5^\circ\text{C}$ VÀ `current_state == true`.
       - *Chuyển trạng thái:* `current_state = false`, **ngay lập tức** post `EVT_RULE_ACTION_REQUEST` (`command = "OFF"`, bypass cooldown).
     - **Vùng đệm giữ nguyên (Deadband):** Khi $22.0^\circ\text{C} < T < 23.5^\circ\text{C}$, không phát sinh lệnh.

2. **Kích hoạt cạnh (Edge-Triggering):** Lệnh chỉ được phát sinh tại thời điểm chuyển trạng thái logic (Cạnh lên `false → true` hoặc Cạnh xuống `true → false`). Khi sensor tiếp tục gửi telemetry nằm trong cùng vùng trạng thái, Rule Engine bỏ qua, triệt tiêu 100% hiện tượng chattering tiếp điểm hay bão tin nhắn MQTT.

3. **Cơ chế phản hồi ngắt an toàn & Khóa 5 phút (Lockout):**
   - Khi nhận sự kiện `EVT_RELAY_STATE_CHANGED` từ `relay_controller` thông báo một rơ-le bị ngắt an toàn (do chạy quá 30 phút hoặc do sensor offline >60s):
   - Rule Engine lập tức reset trạng thái: `r->current_state = false`.
   - Thiết lập thời gian khóa an toàn: `r->lockout_until_ts = now + 300` (khóa 5 phút). Trong 5 phút này, quy tắc sưởi không được phép tự động bật lại.

#### 4.7.6 Luồng đánh giá Telemetry tổng thể

```
on_ble_telemetry(evt)
    │
    ├── telem->valid == false?
    │     ├── YES → post EVT_SENSOR_VALUE_INVALID (relay_controller tự động ngắt relay_2 sưởi)
    │     │         return (KHÔNG đánh giá rule)
    │     └── NO → tiếp tục
    │
    ├── Duyệt từng rule trong /spiffs/rules.json:
    │     ├── rule.device_id != telem->device_id ──► Bỏ qua
    │     ├── now < rule.lockout_until_ts ──────────► Bỏ qua (Đang trong 5m lockout an toàn)
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

---

### 4.8 Module: Scheduler (`scheduler.h/.c`)

#### 4.8.1 Mô tả
Quét `schedule.json` mỗi phút, đối chiếu với giờ RTC. Xử lý CẢ HAI loại: automation (→ rule engine) và alarm (→ display). Chạy hoàn toàn offline.

#### 4.8.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| SC-01 | Poll mỗi 5s, detect đổi phút | Không dùng `vTaskDelay(60000)` — tránh drift |
| SC-02 | Chống bắn trùng | `s_last_triggered_minute` — mỗi phút chỉ trigger 1 lần |
| SC-03 | Trigger automation | type="automation" → post `EVT_SCHEDULE_AUTOMATION_TRIGGERED` |
| SC-04 | Trigger alarm | type="alarm"/"personal_event" → post `EVT_SCHEDULE_ALARM_TRIGGERED` |
| SC-05 | Reload schedule | Handler `EVT_CONFIG_UPDATED_FROM_CLOUD` → reset `last_triggered = -1` |

#### 4.8.3 API

```c
void scheduler_init(void);
// Hành vi: Register handler EVT_CONFIG_UPDATED_FROM_CLOUD
//          Tạo scheduler_task (Core 0, priority 4, stack 4096)

void scheduler_reload(void);
// Hành vi: Đọc lại schedule.json và nạp cấu hình mới vào RAM.
//          Minute Rollover Guard: Giữ nguyên trạng thái đã kích hoạt trong phút hiện tại,
//          tránh tình trạng bắn trùng (double-trigger) các tác vụ vừa chạy xong khi
//          Cloud đồng bộ file cấu hình mới vào giữa phút.
```

#### 4.8.4 Schedule Entry Data Model

```c
// Đề xuất đặt trong storage.h — cả scheduler.c lẫn rule_engine.c dùng chung
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
```

> **One-time vs Recurring:**
> - Nếu `one_time_date` khác rỗng (VD: "2026-08-20"): scheduler so khớp **ngày cụ thể** + giờ:phút. Sau khi trigger xong, Backend tự set `enabled=false` và push config mới về Gateway.
> - Nếu `one_time_date` rỗng: scheduler so khớp **thứ trong tuần** qua `repeat_days[]` như cũ.
> - Hai kiểu này **không cùng tồn tại** trong 1 entry — nếu có `one_time_date` thì `repeat_days` bị bỏ qua.

#### 4.8.5 Scheduler Task Flow

```
scheduler_task (vòng lặp vô hạn)
    │
    loop:
    │
    ├── now = rtc_get_time()
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
    │                 │     ├── Khớp ngày/thứ:
    │                 │     │     ├── strlen(entry.one_time_date) > 0? (sự kiện một lần)
    │                 │     │     │     │── date == "YYYY-MM-DD" hiện tại? ──► MATCH
    │                 │     │     │     └── khác ngày? ──────────────────────► skip
    │                 │     │     │
    │                 │     │     └── one_time_date rỗng (lịch lặp theo tuần):
    │                 │     │           │── entry.repeat_days[t.tm_wday] == true? ──► MATCH
    │                 │     │           └── false? ─────────────────────────────────► skip
    │                 │     │
    │                 │     ├── KHI MATCH:
    │                 │     │     ├── type == "automation"?
    │                 │     │     │     → malloc(automation_rule_t)
    │                 │     │     │     → post EVT_SCHEDULE_AUTOMATION_TRIGGERED
    │                 │     │     │
    │                 │     │     └── type == "alarm" / "personal_event"?
    │                 │     │           → malloc(alarm_entry_t)
    │                 │     │           → post EVT_SCHEDULE_ALARM_TRIGGERED
    │                 │     │
    │                 │     └── tiếp entry sau
    │
    ├── vTaskDelay(5000ms)  ← poll ngắn, không drift
    └── goto loop
```

---

### 4.9 Module: Display (`display.h/.c`)

#### 4.9.1 Mô tả
OLED SSD1327 + nút bấm + buzzer. Module "chỉ đọc" — hiển thị dữ liệu từ hệ thống, ngoại trừ nút tắt alarm (ngoại lệ local duy nhất).

#### 4.9.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| DI-01 | Hiển thị 3 màn hình luân phiên | Clock/Temp ↔ Upcoming Event ↔ Device Status |
| DI-02 | Auto-refresh clock mỗi giây | `display_task` gọi `display_render()` trực tiếp |
| DI-03 | Chuyển màn hình khi bấm nút | Handler `EVT_BUTTON_SCREEN_NEXT` |
| DI-04 | Hiển thị báo thức + bật buzzer | Handler `EVT_SCHEDULE_ALARM_TRIGGERED` |
| DI-05 | Tắt báo thức khi bấm nút | Handler `EVT_BUTTON_ALARM_STOP` → ghi event log |
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
| **Nút 2 (`BTN_SCREEN_NEXT`)** | **GPIO 4** | **Nhấn nhanh (< 500ms): TẮT CHUÔNG NGAY (DISMISS)**<br>Còi tắt ngay lập tức, tắt chuông triệt để, OLED trở về màn hình chính, ghi log `alarm_stopped`. | **Nhấn nhanh (< 500ms): Chuyển màn hình OLED**<br>Luân phiên: Đồng hồ ↔ Lịch biểu ↔ Trạng thái Rơ-le ↔ Thông tin hệ thống (Mã Claim). |

> **Ưu điểm thiết kế chống nhầm lẫn:**
> - Tắt chuông hoàn toàn chỉ cần 1 cú click nhanh (< 500ms) vào Nút 2 (`GPIO 4`), khớp tuyệt đối với tiêu chí nghiệm thu **M13** và **UC-04**.
> - Không yêu cầu giữ nút 2 giây để tắt chuông, triệt tiêu hoàn toàn rủi ro người dùng ngủ ngáy giữ quá 5 giây làm vô tình xóa cấu hình Wi-Fi!

#### 4.9.5 Ngoại lệ local — Tắt báo thức

Đây là hành động local duy nhất được phép ghi dữ liệu:

```c
static void on_button_alarm_stop(gateway_event_t *evt) {
    if (!s_alarm_active) return;
    s_alarm_active = false;
    // Tắt buzzer
    // Gọi TRỰC TIẾP storage (ngoại lệ duy nhất, không qua bus):
    storage_append_event_log("{\"type\":\"alarm_stopped\",\"ts\":1723621234}");
    // Event log sẽ được đồng bộ lên Backend khi có mạng
}
```

#### 4.9.6 Hardware Interface

| Thành phần | Chip/Type | Giao tiếp | Ghi chú |
|---|---|---|---|
| OLED | SSD1327 | I2C hoặc SPI | 128x128 grayscale, thư viện u8g2 |
| Buzzer | Passive buzzer | GPIO PWM (LEDC) | Cho báo thức |
| Nút bấm | Mechanical switch | GPIO + ngắt cạnh xuống | Có debounce phần cứng hoặc software |

---

### 4.10 Module: Bộ Điều Khiển Relay & Cơ Cấu Chấp Hành (`relay_controller.h/.c`)

#### 4.10.1 Mô tả
Quản lý trực tiếp 4 kênh Relay vật lý trên bo mạch (GPIO 15, 16, 17, 18). Tiếp nhận các yêu cầu điều khiển từ Rule Engine, Scheduler, Web Dashboard hoặc MQTT Cloud Downlink; bắt buộc thi hành cơ chế **Khóa liên động an toàn phần cứng (Interlocking)**, bảo vệ chống cháy nổ nhiệt và sinh phản hồi ACK (`command_ack_t`).

#### 4.10.2 Bảng phân bổ kênh, Định danh chuẩn & Ma trận an toàn Fail-Safe

| Kênh Relay | Chân GPIO | Tên định danh chuẩn | Tên định danh tương thích (Alias) | Tải điều khiển | Giới hạn thời gian bật liên tục tối đa | Hành vi khi cảm biến lỗi / Timeout (>60s) |
|---|:---:|---|---|---|:---:|---|
| **RELAY_1** | **GPIO 15** | `relay_1` | `fan_01`, `fan_relay_01` | Quạt làm mát (Fan) | 4 giờ (240 phút) | TẮT (OFF) hoặc Giữ BẬT tối đa 4h |
| **RELAY_2** | **GPIO 16** | `relay_2` | `heater_01`, `heater_relay_01` | Máy sưởi nhiệt (Heater) | **30 phút** (Emergency Cutoff) | **CƯỠNG CHẾ TẮT NGAY LẬP TỨC (OFF)** |
| **RELAY_3** | **GPIO 17** | `relay_3` | `dehum_01`, `dehumidifier_01` | Máy hút ẩm (Dehum) | 60 phút | TẮT (OFF) |
| **RELAY_4** | **GPIO 18** | `relay_4` | `aux_01`, `light_01` | Đèn / Thiết bị phụ trợ | Không giới hạn | Giữ nguyên trạng thái cuối |

#### 4.10.3 Khóa liên động an toàn (Interlocking) & Ưu tiên giải nhiệt
Để loại trừ triệt để nguy cơ cháy nổ do xung đột lệnh từ các nguồn khác nhau (Rule Engine, Scheduler, Web App, MQTT Cloud hay Serial CLI):
1. **Khóa liên động độc quyền:** Quạt làm mát (`relay_1`) và Máy sưởi nhiệt (`relay_2`) **tuyệt đối không bao giờ được phép BẬT đồng thời**.
2. **Quy tắc thực thi trong `relay_controller_set_state()`:**
   - Khi có lệnh BẬT Máy sưởi (`relay_2 = ON`): Bộ điều khiển kiểm tra `relay_1`. Nếu Quạt đang BẬT, bộ điều khiển tự động cưỡng chế ngắt Quạt (`relay_1 = OFF`), trễ 500ms để triệt tiêu dòng cảm ứng, sau đó mới đóng rơ-le Máy sưởi.
   - Khi có lệnh BẬT Quạt (`relay_1 = ON`): **Ưu tiên chống quá nhiệt (Cooling Safety Override)** luôn thắng! Bộ điều khiển ngắt ngay lập tức Máy sưởi (`relay_2 = OFF`), sau đó đóng rơ-le Quạt.
3. **Bỏ qua Cooldown khi TẮT (Immediate Shutoff):**
   - Mọi lệnh hoặc điều kiện bảo vệ yêu cầu TẮT rơ-le (đặc biệt là Máy sưởi `relay_2`) **bắt buộc thực thi ngay lập tức (0ms trễ, bypass toàn bộ cooldown)**. Cooldown chỉ áp dụng cho chiều BẬT để chống rung tiếp điểm cơ.

#### 4.10.4 Đồng bộ trạng thái Rule Engine & Thời gian khóa an toàn (Lockout)
- Khi một kênh Relay bị cưỡng chế TẮT do vượt quá thời gian tối đa (vd: Máy sưởi quá 30 phút) hoặc do cảm biến mất tín hiệu (>60s):
  - `relay_controller` xuất mức LOW ra GPIO và phát sự kiện `EVT_RELAY_STATE_CHANGED` (`state = false`, `reason = "SAFETY_CUTOFF"`).
  - `rule_engine` bắt sự kiện này và **đồng bộ lại trạng thái nội bộ**: gán `current_state = false`.
  - Thiết lập **thời gian khóa bảo vệ (Safety Lockout = 300s / 5 phút)**: Rule Engine không cho phép bất kỳ quy tắc tự động nào kích hoạt bật lại Máy sưởi trong vòng 5 phút tiếp theo để thanh nhiệt hoàn toàn nguội.

#### 4.10.5 C API (`relay_controller.h`)

```c
void relay_controller_init(void);
// Hành vi: Cấu hình GPIO 15..18 output LOW, xác nhận trở kéo 10kΩ GND
//          Đăng ký nhận EVT_DEVICE_COMMAND_REQUEST và EVT_RULE_ACTION_REQUEST

esp_err_t relay_controller_set_state(const char *target_id, bool state, const char *cmd_id, const char *source);
// Hành vi: Tra cứu alias (fan_01 -> relay_1, heater_01 -> relay_2)
//          Kiểm tra Interlock (Heater vs Fan)
//          Ghi GPIO vật lý, khởi tạo timer đếm thời gian bật liên tục
//          Phát EVT_COMMAND_RESPONSE gửi ACK về Cloud/MQTT

bool relay_controller_get_state(int relay_id);
// Hành vi: Đọc mức logic hiện tại của rơ-le (1..4)

void relay_controller_emergency_shutdown_all(void);
// Hành vi: Ngắt khẩn cấp toàn bộ 4 rơ-le về mức LOW (dùng trong panic/shutdown)
```

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

#### 4.11.3 Cơ chế Store-and-Forward Sync Queue (`sync_queue.bin`)
- **Định dạng file:** File nhị phân có header cố định 24 bytes (chuẩn `<IIIIII` khớp `test_sync_queue.py`):
  ```c
  typedef struct {
      uint32_t magic;          // 0x53594E43 ("SYNC")
      uint32_t write_offset;   // Con trỏ vị trí byte ghi tiếp theo
      uint32_t read_offset;    // Con trỏ vị trí byte đã gửi thành công lên server
      uint32_t total_records;  // Tổng số bản ghi tích lũy
      uint32_t unacked_count;  // Số bản ghi đang chờ gửi
      uint32_t crc32;          // Checksum bảo vệ header
  } sync_queue_header_t;       // Kích thước chuẩn cố định: đúng 24 bytes

  // Cấu trúc bản ghi nhị phân cố định 32 bytes (chuẩn `<IBBH16sfI` khớp test_sync_queue.py):
  typedef struct {
      uint32_t timestamp;      // 4 bytes: Unix epoch timestamp
      uint8_t  record_type;    // 1 byte: 0x01=telemetry, 0x02=event_log, 0x03=command_ack
      uint8_t  flags;          // 1 byte: cờ điều khiển (bit 0: QoS1, bit 1: encrypted)
      uint16_t reserved;       // 2 bytes: byte đệm căn gióng bộ nhớ (alignment padding)
      char     device_id[16];  // 16 bytes: Định danh thiết bị (null-terminated)
      float    value;          // 4 bytes: Giá trị đo telemetry (IEEE 754 float)
      uint32_t crc32;          // 4 bytes: Checksum bảo vệ toàn vẹn bản ghi
  } sync_queue_record_t;       // Kích thước chuẩn cố định: đúng 32 bytes
  ```
- **Hành vi khi Offline:** Khi `uplink_task` mất mạng (Wi-Fi hoặc MQTT ngắt), mọi telemetry và event thay vì gửi mạng sẽ được gửi sang `s_sd_log_queue` để append vào cuối file `sync_queue.bin`.
- **Hành vi khi Online trở lại (Backfill):**
  1. `uplink_task` kết nối thành công tới MQTT Broker.
  2. Gửi tín hiệu kích hoạt tiến trình đồng bộ bù.
  3. `sd_log_task` đọc một lô (Batch) gồm **20 đến 50 bản ghi** từ vị trí `read_offset`.
  4. `uplink_task` gộp nhiều bản ghi vào một payload gửi về Backend hoặc publish tuần tự với tốc độ đồng bộ bù tối đa đạt **100 records/giây**, Backend thực hiện deduplicate dựa trên `(device_id, timestamp)`.
  5. Sau khi nhận đủ `PUBACK` cho cả batch, `sd_log_task` cập nhật `read_offset` mới và gọi `f_sync()` để lưu trạng thái.
  6. Lặp lại cho tới khi `read_offset == write_offset` (hàng đợi rỗng).

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
- **Operator instructions:**
  1. Cắm thẻ nhớ MicroSD 8GB/16GB vào Gateway.
  2. Khởi động thiết bị, quan sát log serial có dòng `[SD] FATFS mounted successfully. Free: XXX MB`.
  3. Sau 1 phút, tháo thẻ nhớ cắm vào PC, mở file CSV kiểm tra các dòng dữ liệu nhiệt độ/độ ẩm.

**Verification Contract — FR-SD-02: Store-and-Forward Offline Queue & Backfill**
- **Tier:** manual
- **Preconditions:** Gateway đang online, đẩy telemetry lên MQTT Broker bình thường.
- **Stimulus:**
  1. Tắt router Wi-Fi (hoặc ngắt kết nối Internet) trong 3 phút.
  2. Bật lại router Wi-Fi.
- **Expected:**
  1. Khi mất mạng: Log xuất hiện `[UPLINK] MQTT lost, switching to SD Store-and-Forward`. File `sync_queue.bin` tăng kích thước.
  2. Khi có mạng lại: Gateway tự reconnect Wi-Fi và MQTT. Xuất hiện log `[UPLINK] Backfilling offline queue...`.
  3. Toàn bộ các gói tin phát sinh trong 3 phút ngắt mạng được publish bù lên Server, timestamp giữ nguyên thời điểm đo thực tế.
  4. Sau khi gửi hết, `unacked_count` trở về 0.
- **Prohibited:** Dữ liệu trong thời gian mất mạng bị ghi đè hoặc biến mất; Gateway bị tràn RAM (Out of Memory) trong lúc offline.
- **Operator instructions:**
  1. Chạy Backend và Web App, mở trang Events & Telemetry.
  2. Rút cáp mạng router Wi-Fi 3 phút.
  3. Cắm lại cáp mạng, theo dõi giao diện Web App: các điểm dữ liệu trong 3 phút vừa qua tự động lấp đầy biểu đồ.

---

### 4.12 Module: Config Manager & Cấu hình tại chỗ (`config_manager.h/.c`)

#### 4.12.1 Mô tả
Chịu trách nhiệm lưu trữ, quản lý và cập nhật toàn bộ tham số hoạt động của Gateway bao gồm: cấu hình mạng Wi-Fi, địa chỉ Server/Broker, múi giờ (POSIX ICT-7), danh sách ánh xạ cảm biến không dây BLE và thiết bị chấp hành, cùng các ngưỡng cảnh báo. Cung cấp cơ chế **cấu hình tại chỗ không phụ thuộc Cloud** thông qua 3 kênh: nạp tệp `config.json` từ thẻ nhớ Flash/SD, giao diện Dark Mode **Web Dashboard** (Port 80) và console **Serial CLI** tương tác qua cổng USB UART0 (115200 baud).

#### 4.12.2 Định dạng tệp `/spiffs/config.json`
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
  1. Tạo file `config.json` và lưu vào thư mục lưu trữ `/spiffs/config.json`.
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
| *(7)*| *(mock_harness)* | *(Chỉ bật trong môi trường test)* | *1000ms* | *1000ms* | *15 giây* |

- **Mặt nạ giám sát chuẩn Production (Active Mask):** `#define TASK_ACTIVE_MASK_PRODUCTION (0x7F)` (bật đủ 7 bit 0..6: `(1<<0)|...|(1<<6) = 127 = 0x7F`).
- **Ghi chẩn đoán sự cố vào RTC Slow Memory (Không phụ thuộc SD Card):**
  - Khi phát hiện bất kỳ task nào không báo nhịp tim quá **15 giây**:
  - `supervisor_task` đóng gói chẩn đoán vào vùng nhớ **RTC Slow SRAM (`RTC_NOINIT_ATTR`)** vốn được duy trì qua reset phần mềm:
    ```c
    typedef struct {
        uint32_t magic;           // 0xDEADBEEF
        uint8_t  failed_task_bit; // Bit task bị treo (0..6)
        char     task_name[16];   // Tên task (vd: "display_task")
        uint32_t free_heap;       // Dung lượng heap tại thời điểm treo
        uint32_t uptime_s;        // Thời gian hoạt động trước khi chết
        uint32_t timestamp;       // UNIX epoch
    } rtc_crash_dump_t;
    ```
  - In cảnh báo khẩn cấp ra cổng Serial Console UART0: `[SUP] CRITICAL: Task '%s' STALLED! Dumping to RTC Memory & restarting...`.
  - Gọi hàm khởi động lại an toàn: `esp_restart()`.
  - Khi boot lại: Hệ thống kiểm tra magic `0xDEADBEEF` trong RTC Memory, in báo cáo chẩn đoán sự cố ra terminal, post sự kiện ghi log và xóa cờ magic.

#### 4.13.3 Chu trình giám sát & Xử lý sự cố
1. `supervisor_task` chạy ở mức ưu tiên cao nhất (**Priority 7**, Core 0), thức dậy mỗi **1000ms**. Định kỳ gọi `esp_task_wdt_reset()` để nuôi phần cứng TWDT của chip ESP32-S3.
2. Tất cả 7 task định kỳ báo danh qua hàm `supervisor_report_heartbeat(task_mask)`.
3. `serial_cli_task` sử dụng hàm đọc `uart_read_bytes` kèm timeout `pdMS_TO_TICKS(1000)` để đảm bảo vòng lặp không bị kẹt vô hạn khi không có người gõ phím.
4. `ble_scan_task` sử dụng khoảng delay 1 giây và định kỳ báo nhịp tim, loại bỏ hoàn toàn hiện tượng delay 10 giây gây reset nhầm.

#### 4.13.4 Verification Contracts — Task Supervisor

**Verification Contract — FR-SUP-01: Báo danh Heartbeat bình thường**
- **Tier:** target
- **Preconditions:** Gateway nạp firmware đang hoạt động đầy đủ 7 task production (mask 0x7F).
- **Stimulus:** Giám sát thiết bị chạy liên tục trong 10 phút dưới tải bình thường.
- **Expected:** Supervisor task định kỳ kiểm tra và reset watchdog phần cứng. Không có thông báo lỗi hay reset bất thường; cờ của cả 7 task được set đều đặn mỗi chu kỳ ≤ 5s.
- **Prohibited:** Reset ngẫu nhiên khi hệ thống hoạt động bình thường.
- **Operator instructions:**
  1. Khởi động thiết bị, mở serial monitor.
  2. Quan sát log định kỳ: `[SUP] System healthy. Heap free: XXX KB, All tasks alive (mask=0x7F)`.

**Verification Contract — FR-SUP-02: Bắt lỗi Task bị treo & Tự khôi phục an toàn**
- **Tier:** target
- **Preconditions:** Nạp firmware kích hoạt chế độ test chẩn đoán (Test Hook: lệnh `test freeze event_bus`).
- **Stimulus:** Gửi lệnh qua Serial CLI: `test freeze event_bus` (khiến `event_dispatcher_task` rơi vào vòng lặp vô hạn mà không báo heartbeat).
- **Expected:**
  1. Đúng 15 giây sau khi task bị đóng băng, `supervisor_task` phát hiện thiếu cờ `TASK_HEARTBEAT_EVENT_BUS`.
  2. Ghi cảnh báo ra log Serial: `[SUP] CRITICAL: Task 'event_bus' stalled > 15s! Flushing crash log...`.
  3. Chẩn đoán sự cố được lưu thành công vào RTC Slow Memory với mã lỗi `TASK_STALL_EVENT_BUS` (và đồng bộ append vào `/sdcard/logs/crash.log` sau khi reboot).
  4. Gateway thực hiện `esp_restart()` và khởi động lại hoàn tất trong < 3 giây.
- **Operator instructions:**
  1. Mở Serial terminal, gõ `test freeze event_bus`.
  2. Bấm đồng hồ đếm: Đúng 15 giây sau, xác nhận thấy log crash và gateway tự reboot.
  3. Sau khi reboot, kiểm tra file `/sdcard/logs/crash.log` ghi lại sự kiện đóng băng chính xác.

---

### 4.14 Boot Sequence tổng thể (Hoàn chỉnh & Chuẩn hóa)

```
app_main()
    │
    ├── 1. nvs_flash_init()                          ← Khởi tạo NVS Flash lưu tham số Wi-Fi, System, Timezone
    ├── 2. relay_init()                              ← CẤP NGUỒN AN TOÀN: Kéo GPIO 15..18 LOW ngay lập tức (10kΩ pull-down)
    ├── 3. event_bus_init()                          ← Khởi tạo Event Queue trung tâm (32 slots) trước khi các module đăng ký
    ├── 4. supervisor_init()                         ← Khởi tạo Task Supervisor (Core 0, Priority 7, Mask 0x7F, Timeout 15s)
    │       ├── Khởi tạo phần cứng TWDT 15s
    │       ├── Kiểm tra & in chẩn đoán từ RTC Slow Memory (nếu có sau sự cố trước đó)
    │       └── Đăng ký Active Task Mask = 0x7F (7 tasks production)
    │
    ├── 5. rtc_init()                                ← Khởi tạo bus I2C0 (GPIO 8/9), đọc UTC từ DS3231, nạp POSIX TZ "ICT-7"
    ├── 6. display_init()                            ← Khởi tạo OLED SSD1327 (I2C0 0x3C), nút bấm (GPIO 0/4), buzzer (GPIO 1)
    │       └── VẼ MÀN HÌNH ĐỒNG HỒ NGAY LẬP TỨC! (Đạt KPI-BOOT-01 < 1.5s độc lập hoàn toàn với mạng & SD card)
    │
    ├── 7. storage_init()                            ← Gắn kết (Mount) phân vùng SPIFFS /spiffs (chứa config, schedule, rules)
    ├── 8. sd_logger_init()                          ← Khởi tạo SPI2 host & khe MicroSD (nếu chưa cắm thẻ -> fallback /spiffs)
    ├── 9. config_manager_init()                     ← Nạp cấu hình từ /spiffs/config.json
    ├── 10. wifi_provisioning_init() & web_server()  ← Khởi tạo Wi-Fi SoftAP Portal & Web Dashboard (Port 80)
    ├── 11. mqtt_adapter_init() & uplink_init()      ← Khởi tạo MQTT Client kết nối Broker (Core 1, QoS 1)
    ├── 12. rule_engine_init()                       ← Nạp /spiffs/rules.json, sẵn sàng đánh giá Hysteresis
    ├── 13. scheduler_init()                         ← Nạp /spiffs/schedule.json, tạo scheduler_task (Core 0, Priority 4)
    ├── 14. ble_adapter_init()                       ← Khởi tạo ngăn xếp NimBLE, bắt đầu passive scan cảm biến (Core 0, Prio 5)
    ├── 15. serial_cli_init()                        ← Giao diện dòng lệnh cứu hộ UART0 (Core 1, Priority 2, timeout 1s)
    │
    └── 16. event_bus_start_dispatcher()             ← BẮT ĐẦU ĐIỀU PHỐI EVENT BUS (SAU CÙNG)
            ├── Tạo event_dispatcher_task (Core 0, Priority 6)
            └── Bắt đầu chu trình xử lý sự kiện toàn hệ sinh thái
```

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
| BE-07 | Sync API | Cung cấp endpoint để Gateway pull config + push log |
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

### 7.3 JSON Schema — schedule.json (trên Gateway Flash /spiffs)

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

### 7.3.2 JSON Schema — rules.json (trên Gateway Flash /spiffs)

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

### 7.4 JSON Schema — device_list.json (trên Gateway Flash /spiffs)

```json
[
  {
    "device_id": "sensor_temp_01",
    "name": "Cảm biến nhiệt ẩm phòng ngủ",
    "protocol": "ble",
    "mac_address": "A4:C1:38:12:34:56",
    "parser_type": "sig_181a",
    "device_type": "sensor"
  },
  {
    "device_id": "fan_relay_01",
    "name": "Quạt làm mát",
    "protocol": "internal_gpio",
    "relay_channel": 1,
    "device_type": "actuator"
  }
]
```

### 7.5 JSON Schema — event_log.json (trên Gateway Flash /spiffs)

```json
[
  {
    "type": "rule_executed",
    "ts": 1723621234,
    "rule_id": 1,
    "target": "fan_01",
    "command": "ON"
  },
  {
    "type": "alarm_stopped",
    "ts": 1723621567,
    "schedule_id": 201
  },
  {
    "type": "automation_triggered",
    "ts": 1723621800,
    "schedule_id": 101,
    "target": "bedroom_light",
    "command": "ON"
  }
]
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


#### POST /gateways/claim
Gán một Gateway mới vào tài khoản người dùng thông qua mã xác thực cục bộ (Claim Code).

- **Quy trình bắt tay đăng ký Claim Code:**
  1. Khi xuất xưởng hoặc boot lần đầu tiên: Gateway tự động sinh một chuỗi ngẫu nhiên 6 ký tự chữ hoa và số (vd: `GW-9821A0`), lưu vào NVS.
  2. Khi Gateway kết nối Wi-Fi có Internet lần đầu, Gateway gửi gói tin đăng ký ban đầu lên Backend qua endpoint nội bộ:
     `POST /api/v1/gateways/register-claim`
     Body: `{ "mac_address": "AA:BB:CC:DD:EE:FF", "claim_code": "GW-9821A0", "firmware_version": "1.0.0" }`
     Backend lưu Gateway vào cơ sở dữ liệu với trạng thái `status: "UNCLAIMED"`, gán `claimCode: "GW-9821A0"` kèm thời hạn hiệu lực (TTL 24 giờ).
  3. Màn hình OLED SSD1327 của Gateway hiển thị mã `Claim: GW-9821A0` ở trang thông tin hệ thống (Screen 3).
  4. Người dùng đăng nhập Web App, vào mục *Thêm Gateway*, nhập mã `GW-9821A0` và đặt tên cho thiết bị:
- **Request (Web App → Backend):**
```json
{
  "claim_code": "GW-9821A0",
  "name": "Đồng hồ Phòng khách"
}
```
- **Response (200 OK):**
```json
{
  "gateway_id": "gw_a1b2c3d4",
  "name": "Đồng hồ Phòng khách",
  "message": "Gateway claimed successfully"
}
```
  5. Backend xác thực mã khớp, liên kết Gateway với tài khoản người dùng (`userId`), chuyển trạng thái sang `status: "ACTIVE"`, cấp phát `api_key` và thông tin MQTT client, đồng thời publish thông báo cấu hình xuống Gateway.
  6. Sau khi claim thành công, mã `claim_code` trên Gateway bị hủy bỏ/xóa khỏi NVS (mã chỉ dùng một lần duy nhất).

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
  "target_device_id": "fan_01",
  "action_command": "ON"
}
```


#### PUT /gateways/{gateway_id}/rules/{id}

Cập nhật rule.

#### DELETE /gateways/{gateway_id}/rules/{id}

Xóa rule.




### 8.6 Sync APIs (Gateway gọi)

#### GET /api/v1/gateways/{gateway_id}/sync/schedule

Gateway pull schedule mới nhất.

**Response 200:**
```json
{
  "schedules": [...],      // Toàn bộ schedule cho gateway này
  "rules": [...],          // Toàn bộ automation rule
  "last_updated": "2026-08-14T14:50:00Z"
}
```

#### GET /api/v1/gateways/{gateway_id}/sync/devices

Gateway pull device list mới nhất.

**Response 200:**
```json
{
  "devices": [...],
  "last_updated": "2026-08-14T14:50:00Z"
}
```

#### POST /api/v1/gateways/{gateway_id}/sync/events

Gateway push event log.

**Request Body:**
```json
{
  "events": [
    {"type": "rule_executed", "ts": 1723621234, "rule_id": 1, ...},
    {"type": "alarm_stopped", "ts": 1723621567, "schedule_id": 201}
  ]
}
```

**Response 200:**
```json
{
  "synced_count": 2,
  "message": "Events synced successfully"
}
```

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
| 404 | `GATEWAY_NOT_FOUND` | Không tìm thấy gateway | `gateway_id` không tồn tại trong DB |
| 404 | `DEVICE_NOT_FOUND` | Không tìm thấy thiết bị | `device_id` không tồn tại hoặc không thuộc gateway |
| 404 | `SCHEDULE_NOT_FOUND` | Không tìm thấy schedule | `schedule.id` không tồn tại |
| 404 | `RULE_NOT_FOUND` | Không tìm thấy rule | `rule.id` không tồn tại |
| 409 | `DEVICE_ID_CONFLICT` | `device_id` đã tồn tại | POST /devices với `device_id` trùng |
| 409 | `DEVICE_IN_USE` | Thiết bị đang được rule tham chiếu | Xóa sensor/actuator đang có rule (nếu không cascade) |
| 422 | `GATEWAY_OFFLINE` | Gateway không online | POST /command nhưng gateway đang offline |
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

| Topic | QoS | Lý do |
|---|---|---|
| `gateway/{id}/device/{dev}/telemetry` | 1 | Đảm bảo dữ liệu cảm biến đo lường không bị thất lạc |
| `gateway/{id}/telemetry/batch` | 1 | Dữ liệu lưu đệm ngoại tuyến (Store-and-Forward) cần đối soát đủ |
| `gateway/{id}/device/{dev}/command` | 1 | Lệnh điều khiển bắt buộc phải đến đích (Exactly/At-least once) |
| `gateway/{id}/device/{dev}/command/ack` | 1 | Báo cáo kết quả thực thi lệnh về Backend để cập nhật UI |
| `gateway/{id}/config/schedule` | 1 | Cấu hình lịch biểu và luật tự động hóa, retained |
| `gateway/{id}/status` | 1 | Retained Last Will & Testament (LWT) |
| `gateway/{id}/events` | 1 | Nhật ký sự kiện hệ thống |

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
| | 6. Gateway pull config từ Backend → sẵn sàng hoạt động |
| **Postcondition** | Gateway online, hiển thị đúng giờ, đã sync config |

### UC-02: Thêm thiết bị

| | |
|---|---|
| **Actor** | User |
| **Precondition** | Gateway đã online |
| **Main Flow** | 1. User mở Web App → Thiết bị → Thêm mới |
| | 2. Nhập: device_id, tên, protocol (BLE/MQTT), MAC/topic |
| | 3. Backend lưu DB → push device_list mới cho Gateway (MQTT hoặc chờ sync) |
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
| **Main Flow** | 1. Wi-Fi Provisioning post EVT_WIFI_LOST |
| | 2. cloud_sync_task chờ ở xSemaphoreTake() (không chiếm CPU) |
| | 3. RTC giữ giờ chính xác (DS3231 pin backup) |
| | 4. Scheduler vẫn chạy: poll 5s, trigger automation/alarm đúng giờ |
| | 5. Rule Engine vẫn đánh giá telemetry BLE (BLE không cần Internet) |
| | 6. Event log ghi vào LittleFS |
| **Postcondition** | Gateway hoạt động bình thường, log chờ sync |

### UC-06: Internet trở lại — đồng bộ

| | |
|---|---|
| **Actor** | Hệ thống (tự động) |
| **Precondition** | Gateway offline, có event log chưa sync |
| **Trigger** | Wi-Fi kết nối lại |
| **Main Flow** | 1. wifi_provisioning post EVT_WIFI_CONNECTED |
| | 2. rtc: sync NTP → ghi lại DS3231 |
| | 3. storage: on_network_available → give semaphore |
| | 4. cloud_sync_task đánh thức: |
| | &nbsp;&nbsp;&nbsp;&nbsp;a. GET /sync/schedule → so sánh → ghi đè nếu khác |
| | &nbsp;&nbsp;&nbsp;&nbsp;b. GET /sync/devices → tương tự |
| | &nbsp;&nbsp;&nbsp;&nbsp;c. POST /sync/events → upload log tích lũy |
| | &nbsp;&nbsp;&nbsp;&nbsp;d. post EVT_CONFIG_UPDATED_FROM_CLOUD (nếu có thay đổi) |
| | 5. rule_engine + scheduler tự reload |
| **Postcondition** | Config đồng bộ, event log đã upload, hệ thống up-to-date |

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
| MQTT broker mất kết nối | esp-mqtt stack | Post `EVT_MQTT_DISCONNECTED` | MQTT device không điều khiển được, BLE device vẫn OK |
| Wi-Fi mất | esp_wifi stack | Post `EVT_WIFI_LOST` | Chuyển sang offline mode |
| Event queue đầy | `event_bus_post()` | Log WARN, trả pdFALSE, event bị mất | Cần kiểm tra handler nào block dispatcher |
| malloc thất bại | Mỗi module tự kiểm tra | Log ERROR, skip lần này | Có thể mất 1 chu kỳ xử lý |
| LittleFS mount fail | `storage_init()` | Log ERROR, hệ thống chạy không có persistent data | Schedule/rule rỗng, chờ sync |
| DS3231 I2C fail | `rtc_init()` | Log ERROR, dùng epoch 0 | Giờ sai cho tới khi có NTP |
| NTP sync fail | `rtc_sync_from_ntp_and_save()` | Log WARN, giữ giờ DS3231 cũ | Giờ có thể lệch vài giây/phút |
| Backend không phản hồi | `cloud_sync_task` | Log ERROR, giữ config local, thử lại lần sau | Gateway chạy bình thường với data cũ |

### 11.2 Safe-default Pattern & Ma Trận Ngắt An Toàn Cơ Cấu Chấp Hành

```
Cảm biến mất tín hiệu / Hỏng (valid = false hoặc Timeout > 60s)
    │
    ├── Rule Engine: NGỪNG đánh giá các luật tự động liên quan
    │
    ├── RELAY_2 (Máy sưởi nhiệt / Hazard):
    │     └── BẮT BUỘC NGẮT CƯỠNG CHẾ (OFF) NGAY LẬP TỨC
    │         (Không cho phép giữ trạng thái cũ, triệt tiêu nguy cơ quá nhiệt gây hỏa hoạn)
    │
    ├── RELAY_1 (Quạt làm mát / Fan):
    │     └── Giữ trạng thái ON nếu trước đó nhiệt độ cao, hoặc duy trì tốc độ an toàn
    │
    ├── RELAY_3 (Máy hút ẩm): TẮT an toàn (OFF)
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
   - Không nhận được gói tin quảng bá (Advertisement Beacon) của cảm biến trong **60 giây liên tiếp**.
   - Gateway tự động đánh dấu `is_online = false`, phát `EVT_DEVICE_OFFLINE_ALERT`, và chuyển trạng thái telemetry sang `valid = false`.
   - Rule Engine ngay lập tức chuyển sang chế độ **Safe-Default**: chặn kích hoạt các cơ cấu chấp hành nguy hiểm và hiển thị cảnh báo lên Web Dashboard / OLED.
2. **Nhiễu sóng 2.4GHz / Xung đột Wi-Fi & Bluetooth (RF Coexistence):**
   - ESP32-S3 sử dụng chung 1 khối vô tuyến 2.4 GHz. Khi Wi-Fi truyền tải nặng (vd: upload sync queue), tính năng **Software Coexistence** (`CONFIG_SW_COEXIST_ENABLE=y`) tự động phân chia thời gian (TDM).
   - Nếu gói tin BLE bị mất do xung đột tần số, hệ thống không coi là lỗi; cảm biến sẽ tự động phát lại ở chu kỳ quảng bá kế tiếp.
3. **Gói tin hỏng / Sai định dạng (Corrupted Packet):**
   - NimBLE stack tự động loại bỏ các gói tin sai lệch CRC phần cứng cấp link-layer.
   - Hàm giải mã kiểm tra tính hợp lệ của Service UUID `0x181A` và chiều dài dữ liệu; nếu sai lệch, gói tin bị hủy bỏ ngay mà không gây rò rỉ bộ nhớ.

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
| FreeRTOS task stacks | ~24 KB | 6 tasks × ~4096 bytes |
| **Tổng ước tính** | **~110-150 KB** | ESP32 có ~520 KB SRAM |

### 12.3 Độ tin cậy

| Yêu cầu | Chỉ tiêu |
|---|---|
| Uptime | Chạy liên tục 24/7 (có task watchdog) |
| Data persistence | Schedule/device list/event log survive reboot (LittleFS + NVS) |
| Clock accuracy | ± 2 phút/tháng (DS3231), hiệu chỉnh mỗi lần có NTP |
| Offline operation | Vô hạn thời gian (giới hạn bởi flash capacity cho event log) |

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
│                   Authentication Flows                              │
│                                                                     │
│  Web App (Browser):                                                 │
│  ┌──────┐    POST /auth/login     ┌──────────┐                     │
│  │ User │ ──────────────────────► │ Backend  │                     │
│  │      │ ◄────────────────────── │          │                     │
│  │      │   { access_token,       │          │                     │
│  │      │     refresh_token }     │          │                     │
│  │      │                         │          │                     │
│  │      │  GET /api/v1/...        │          │                     │
│  │      │  Authorization: Bearer  │          │                     │
│  │      │  <access_token>         │          │                     │
│  └──────┘                         └──────────┘                     │
│                                                                     │
│  Gateway (ESP32):                                                   │
│  ┌──────────┐  GET /api/v1/sync/...  ┌──────────┐                  │
│  │ Gateway  │ ──────────────────────►│ Backend  │                  │
│  │          │  Header:               │          │                  │
│  │          │  X-Gateway-Key: <key>  │          │                  │
│  └──────────┘                        └──────────┘                  │
└─────────────────────────────────────────────────────────────────────┘
```

| Thành phần | Cơ chế | Chi tiết |
|---|---|---|
| Web App → Backend | JWT (access + refresh token) | Access token TTL: 15 phút. Refresh token TTL: 7 ngày. Lưu trong `httpOnly` cookie (chống XSS) |
| Gateway → Backend | API Key cố định | Key được burn vào NVS lúc provisioning hoặc cấu hình qua captive portal. Gửi trong header `X-Gateway-Key` |
| Backend verify | Middleware kiểm tra token/key | Route `/api/v1/*` yêu cầu auth. Route `/auth/*` public |

> **Lưu ý:** Authentication là **Should-have** — nếu chưa implement, hệ thống vẫn hoạt động nhưng API sẽ public. Khi implement, chỉ cần thêm middleware vào Express/FastAPI, không ảnh hưởng logic nghiệp vụ.

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

1. **Mạng Wi-Fi gia đình** — bandwidth đủ cho MQTT + HTTP sync, không cần optimize cho mạng yếu
2. **Số lượng thiết bị nhỏ** — tối đa 16 BLE + 16 MQTT device, tối đa 32 rule, tối đa 50 schedule entries
3. **Backend luôn sẵn sàng** khi có Internet — không xử lý trường hợp Backend down kéo dài
4. **Một gateway / một user** — không hỗ trợ multi-user cho cùng 1 gateway (trừ khi mở rộng)
5. **BLE Sensor Peripheral:**
   - *Khuyến nghị tối ưu:* Dùng 1 board ESP32/ESP32-C3 phụ nạp firmware BLE GATT Server + cảm biến nhiệt ẩm để khớp 100% với mô hình GATT Connect & Read của FSD.
   - *Nếu dùng Xiaomi Mijia LYWSD03MMC:* Cần flash open firmware (pvvx/ATC) và lưu ý firmware này phát broadcast BLE Advertisement — adapter cần điều chỉnh chế độ scan/parse advertisement payload thay vì GATT connect.
6. **Màn hình OLED:**
   - Thiết kế chuẩn dùng **OLED SSD1327 128x128 4-bit grayscale**.
   - Nếu thay bằng **OLED SSD1306 128x64 đơn sắc**, cần điều chỉnh lại layout 3 màn hình và đổi driver render tương ứng.
7. **Timezone không đổi** sau khi setup — nếu muốn đổi phải force reset
8. **BLE/WiFi đồng thời chấp nhận miss** — khi WiFi bận (cloud sync), BLE scan có thể miss 1–2 chu kỳ; hệ thống dùng safe-default, không gây lỗi logic
9. **Heap fragmentation không đáng kể** trong thời gian demo — với payload đồng nhất (~40–68 bytes) và tần suất malloc/free thấp (~8640 lần/ngày), fragmentation cần tuần/tháng mới thành vấn đề. Cho production chạy dài hạn, cân nhắc memory pool
10. **LittleFS thread-safe** — `esp_vfs_littlefs` trong ESP-IDF tích hợp mutex tại VFS layer; kết hợp event bus ordering (ghi xong → post event → handler đọc) đảm bảo an toàn đa luồng

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
| M08 | Cloud Sync: đồng bộ config + event log | UC-06 | Bật Internet → Gateway tự pull schedule/device_list mới từ Backend + push event log tích lũy. Backend nhận đủ event log | Sync không chạy khi có mạng, hoặc config mới không được apply, hoặc event log mất |
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

1. **Chuẩn bị:** Gateway đã flash firmware, Backend đang chạy, Web App truy cập được, có ≥ 1 BLE sensor + 1 MQTT actuator sẵn sàng
2. **Thực hiện:** Chạy tuần tự theo kịch bản demo (§15.2), đánh dấu pass/fail từng mục M01–M13
3. **Ghi nhận:** Screenshot Web App, chụp OLED, log serial Gateway cho mỗi test case
4. **Kết quả:** Tất cả M01–M13 pass = **Đạt yêu cầu tối thiểu**. Thêm S01–S06 pass = điểm cộng

---

## 15. Phụ lục

### 15.1 Sơ đồ phân bổ Flash Partition (16MB SPI Flash — ESP32-S3)

Khớp chuẩn xác với file cấu hình [`partitions.csv`]([partitions.csv](firmware/partitions.csv)) của dự án:

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

---

*— Hết tài liệu FSD —*


---

## Phụ lục B: Mô phỏng Thiết bị Modbus RS485 (Mock Test Harness)

> **Ghi chú phạm vi:** Chuẩn truyền thông vật lý RS485 đã được **lược bỏ hoàn toàn** khỏi sản phẩm thương mại để chuyển sang cảm biến không dây BLE. Phụ lục này lưu trữ tài liệu kỹ thuật của module giả lập [`mock_peripherals.c`]([mock_peripherals.c](firmware/main/mock_peripherals.c)), được sử dụng độc quyền trong môi trường kiểm thử tự động nội bộ để xác minh tính năng ngắt mạch (Circuit Breaker) và kịch bản đóng băng tác vụ (`test freeze rs485`) nhằm kích hoạt Task Supervisor.

1. **Khung truyền giả lập:** Hỗ trợ Modbus RTU Function Code 03 (Read Holding Registers) và Function Code 06/16 (Write Registers).
2. **Mặt nạ giám sát trong chế độ kiểm thử:**
   - Trong quá trình chạy kịch bản kiểm thử `test freeze rs485`, bộ test harness tự động gọi `supervisor_set_active_tasks(TASK_ACTIVE_MASK_PRODUCTION | TASK_HEARTBEAT_MOCK)` (tức mask `0xFF` = `0x7F | 0x80`), kích hoạt cờ giám sát bit 7 cho mock task.
   - Trong môi trường sản xuất thương mại (Production), mock task bị tắt hoàn toàn và mặt nạ giám sát chỉ duy trì 7 task chuẩn (`mask = 0x7F`), bảo đảm hệ thống không bao giờ bị reset vô cớ.

3. **Kịch bản kiểm thử Fault Injection qua Serial CLI:**
   - `mock rs485 start [interval_ms]`: Bật luồng giả lập cảm biến.
   - `mock rs485 stop`: Dừng luồng giả lập.
   - `mock rs485 fault <none|timeout|invalid>`: Tiêm lỗi đường truyền hoặc timeout.
   - `test freeze rs485`: Cố tình đóng băng vòng lặp để kích hoạt cơ chế phát hiện của Task Supervisor sau đúng 15 giây.
