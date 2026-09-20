# Functional Specification Document (FSD)
# Smart Clock as Edge IoT Gateway

**Phiên bản:** 1.0  
**Ngày tạo:** 2026-08-14  
**Dự án:** Xây dựng đồng hồ thông minh tích hợp Edge IoT Gateway và hệ thống điều khiển tự động hóa  

---

## Mục lục

1. [Giới thiệu](#1-giới-thiệu)
2. [Phạm vi hệ thống](#2-phạm-vi-hệ-thống)
3. [Kiến trúc tổng thể](#3-kiến-trúc-tổng-thể)
4. [Đặc tả chức năng — Firmware Gateway](#4-đặc-tả-chức-năng--firmware-gateway)
5. [Đặc tả chức năng — Backend](#5-đặc-tả-chức-năng--backend)
6. [Đặc tả chức năng — Web App](#6-đặc-tả-chức-năng--web-app)
7. [Mô hình dữ liệu](#7-mô-hình-dữ-liệu)
8. [Đặc tả API Backend](#8-đặc-tả-api-backend)
9. [Đặc tả giao thức MQTT](#9-đặc-tả-giao-thức-mqtt)
10. [Use Cases](#10-use-cases)
11. [Xử lý lỗi & Failure Modes](#11-xử-lý-lỗi--failure-modes)
12. [Yêu cầu phi chức năng](#12-yêu-cầu-phi-chức-năng)
13. [Ràng buộc & Giả định](#13-ràng-buộc--giả-định)
14. [Tiêu chí nghiệm thu](#14-tiêu-chí-nghiệm-thu)
15. [Phụ lục](#15-phụ-lục)

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
│  - Bộ nhớ Flash / SD: Telemetry CSV log, Event log, Crash log, Offline Sync FIFO Queue │
│  - Flash NVS: Wi-Fi credentials, Broker URI, Gateway ID, Timezone (POSIX ICT-7)        │
│  - Task Supervisor: TWDT + Heartbeat bitmask (0xD7) giám sát 8 task FreeRTOS, hang 10s │
│  - Cấu hình tại chỗ: Web Dashboard (Port 80) + Serial CLI tương tác (UART0 115200 baud)│
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
| **GPIO 44** | `U0RXD` | USB-UART Console | Cổng UART0 RX nối chip nạp CH343 / Nhận lệnh CLI |
| **GPIO 8** | `I2C_SDA` | I2C Bus chung | Chia sẻ cho OLED SSD1327 (`0x3C`) & RTC DS3231 (`0x68`) |
| **GPIO 9** | `I2C_SCL` | I2C Bus chung | Trở kéo pull-up 4.7kΩ lên 3.3V |
| **GPIO 10** | `SPI_CS` | MicroSD Card (SPI) | Chip Select thẻ nhớ SD (tích cực mức LOW) |
| **GPIO 11** | `SPI_MOSI` | MicroSD Card (SPI) | Data In thẻ nhớ MicroSD (Master Out Slave In) |
| **GPIO 12** | `SPI_CLK` | MicroSD Card (SPI) | Clock thẻ nhớ MicroSD |
| **GPIO 13** | `SPI_MISO` | MicroSD Card (SPI) | Data Out thẻ nhớ MicroSD (Master In Slave Out) |
| **GPIO 1** | `PWM_BUZZER` | Còi chip Buzzer | Điều khiển âm lượng và tần số qua LEDC PWM (Tự ngắt sau 60s) |
| **GPIO 2** | `STATUS_LED` | LED trạng thái | Đèn nhấp nháy nhịp tim và hiển thị trạng thái kết nối |
| **GPIO 0** | `BTN_1` | Nút bấm 1 (BOOT) | Short press: Đổi màn hình / Snooze (+9m); Long press (≥5s): Reset Wi-Fi |
| **GPIO 4** | `BTN_2` | Nút bấm 2 | Tắt / Dừng chuông báo thức tức thì (Alarm Dismiss) |
| **Chân ảo / NVS** | `RELAY_1..4` | Relay Driver | Điều khiển 4 kênh Relay (`fan_relay_01`, `heater_01`, `dehum_01`, `aux_04`) |
| **SiP Package** | `Flash & PSRAM` | SPI nội bộ | 16MB Quad SPI Flash + 8MB Embedded PSRAM tích hợp sẵn trong chip |

### 2.4 Phân loại tính năng (MoSCoW)

#### Must Have (bắt buộc)

| ID | Tính năng | Thành phần |
|---|---|---|
| M01 | Hiển thị giờ, ngày, nhiệt độ trên OLED | Gateway |
| M02 | Kết nối BLE sensor, đọc telemetry | Gateway |
| M03 | Kết nối MQTT devices, nhận/gửi lệnh | Gateway |
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
┌──────────────────────────────────────── ESP32 ────────────────────────────────────────┐
│                                                                                       │
│   Core 0 (Real-Time, Wireless BLE & Giám sát hệ thống)                               │
│   ┌─────────────────────────────────────────────────────────────────────────┐         │
│   │  supervisor_task       (Priority 7) - Task Watchdog (TWDT) & Heartbeat  │         │
│   │  event_bus dispatcher  (Priority 6) - Điều phối Queue sự kiện trung tâm │         │
│   │  ble_scan_task         (Priority 5) - Quét & đọc BLE Sensor (GATT/Beacon)│        │
│   │  scheduler_task        (Priority 4) - Báo thức & Lịch tự động (RTC)     │         │
│   │  display_task          (Priority 3) - Cập nhật màn hình OLED SSD1327    │         │
│   │  rule_engine           (Chạy trong ngữ cảnh event_bus dispatcher)       │         │
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
│   │  - Event Bus Queue: 20 slots (phân phối sự kiện toàn hệ thống)          │         │
│   │  - BLE Command Queue: 10 slots (hàng đợi lệnh GATT Write / Điều khiển)  │         │
│   │  - SD Logger Queue: 30 slots (hàng đợi ghi file tránh block I/O)        │         │
│   │  - Uplink Outbox Queue: 20 slots (hàng đợi gửi MQTT khi online)         │         │
│   └─────────────────────────────────────────────────────────────────────────┘         │
│                                                                                       │
│   Phần cứng kết nối:                                                                 │
│   - I2C (GPIO 21, 22): OLED SSD1327 + RTC DS3231                                     │
│   - SPI (GPIO 14, 15, 2, 13): MicroSD Card Socket (FATFS)                            │
│   - UART0 (GPIO 1, 3): Debug Serial Console & Serial CLI cứu hộ                      │
│   - GPIO 18: Còi chip Buzzer (LEDC PWM)                                              │
│   - GPIO 19, 23: Nút bấm điều khiển tại chỗ                                         │
│   - Không dây: Wi-Fi 2.4 GHz + Bluetooth Low Energy (BLE 5.0) tích hợp               │
└───────────────────────────────────────────────────────────────────────────────────────┘
```

> **Ghi chú phân tách Task & Quản lý I/O:**
> - `ble_scan_task` chạy trên Core 0 với priority cao (5) để đảm bảo chu kỳ quét passive beacon và kết nối GATT ổn định. Task này **hoàn toàn độc lập** với Wi-Fi/Internet; dù mạng mất, BLE vẫn đọc cảm biến và cấp dữ liệu cho Rule Engine đều đặn.
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
             [EVT_COMMAND_RESPONSE] ──► uplink_task (phản hồi ACK về topic gateway/{id}/command/ack)
                                    ──► sd_log_task (ghi audit log)

  Luồng 3 — Khôi phục kết nối & Đồng bộ bù (Backfill):
  Internet khôi phục ──► uplink_task kết nối MQTT thành công
                               │
               [Bắt đầu tiến trình gửi bù]
                               │
                               ▼
  uplink_task ◄── Đọc từng batch (20 records) ─── sd_log_task (đọc từ /data/sync_queue.bin)
       │
  Gộp nhiều bản tin thành Batch (50 records/payload JSON) gửi qua MQTT QoS 1 (2 batch/giây = 100 msg/s), Backend dedupe theo (device_id, timestamp)
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
| Queue length | 20 slots | Đủ cho burst event bình thường |
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
| `EVT_MQTT_MESSAGE_RECEIVED` | mqtt_adapter | rule_engine, display | `mqtt_telemetry_t*` (xem §4.6.7) |
| `EVT_MQTT_CONNECTED` | mqtt_adapter | storage, display, sd_log_task | NULL |
| `EVT_MQTT_DISCONNECTED` | mqtt_adapter | display, sd_log_task | NULL |
| `EVT_REMOTE_COMMAND_REQUEST` (Legacy alias) | mqtt_adapter | ble_adapter, mqtt_adapter, display | `rule_action_request_t*` |
| `EVT_COMMAND_RESPONSE` | ble_adapter, mqtt_adapter | uplink_task, sd_log_task | `command_ack_t*` |
| `EVT_SCHEDULE_AUTOMATION_TRIGGERED` | scheduler | rule_engine | `automation_rule_t*` |
| `EVT_SCHEDULE_ALARM_TRIGGERED` | scheduler | display | `alarm_entry_t*` |
| `EVT_RULE_ACTION_REQUEST` | rule_engine | ble_adapter, mqtt_adapter, display | `rule_action_request_t*` |
| `EVT_SENSOR_VALUE_INVALID` | rule_engine | display, sd_log_task | NULL |
| `EVT_BUTTON_ALARM_STOP` | GPIO ISR | display | NULL |
| `EVT_BUTTON_SCREEN_NEXT` | GPIO ISR | display | NULL |
| `EVT_DISPLAY_UPDATE_REQUEST` | (bất kỳ module) | display | NULL |
| `EVT_CONFIG_UPDATED_FROM_CLOUD` | storage | rule_engine, scheduler | `config_updated_t*` |
| `EVT_EVENT_LOG_APPENDED` | storage, sd_logger | (cloud sync trigger) | NULL |
| `EVT_SYNC_COMPLETED` | storage, sd_logger | display | NULL |
| `EVT_BLE_DEVICE_DATA` | `ble_telemetry_t*` | Dữ liệu cảm biến không dây BLE giải mã thành công | free() |

> **Phân biệt `EVT_RULE_ACTION_REQUEST` vs `EVT_REMOTE_COMMAND_REQUEST`:**
> - `EVT_RULE_ACTION_REQUEST` — do **Rule Engine** sinh ra sau khi đánh giá điều kiện (sensor telemetry hoặc schedule automation). Đây là lệnh **tự động** từ logic nội bộ Gateway.
> - `EVT_REMOTE_COMMAND_REQUEST` — do **MQTT Adapter** sinh ra khi nhận lệnh từ Backend (Web App user bấm nút ON/OFF). Đây là lệnh **thủ công** từ bên ngoài.
> - Cả hai đều dùng cùng payload `rule_action_request_t` và cùng pattern `is_my_device()` tại adapter — chỉ khác **nguồn gốc** (local rule vs remote user).
> - Tách riêng event code giúp: (a) log/debug phân biệt lệnh tự động vs thủ công, (b) adapter có thể ưu tiên hoặc xử lý khác nếu cần.

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
   - Caller cấp phát động qua `malloc()` và truyền cờ `is_allocated=true`.
   - Dispatcher chịu trách nhiệm gọi `free(payload)` sau khi tất cả handler hoàn thành.
   - **Xử lý khi Queue đầy:** Nếu `event_bus_post()` thất bại (hàng đợi 32 slots bị đầy), chính hàm `event_bus_post()` có nghĩa vụ giải phóng `free(payload)` ngay lập tức để chống rò rỉ bộ nhớ (Memory Leak).
   - Sự kiện ưu tiên cao (như `EVT_BUTTON_ALARM_STOP`) được đẩy bằng `xQueueSendToFront()` để không bao giờ bị drop.
3. **An toàn đa luồng (Mutex Protection):**
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
// Input ví dụ: "Asia/Ho_Chi_Minh"
// Gọi khi: Nhận timezone từ captive portal form
```

#### 4.2.4 Hardware Interface

| Tham số | Giá trị |
|---|---|
| Chip | DS3231 |
| Giao tiếp | I2C |
| SDA pin | GPIO 21 (mặc định ESP32) |
| SCL pin | GPIO 22 (mặc định ESP32) |
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
// Hành vi: Ghi đè /littlefs/schedule.json
// Gọi bởi: cloud_sync_task sau khi pull từ Backend

char *storage_load_schedule_json(void);
// Hành vi: Đọc /littlefs/schedule.json
// Trả về: Chuỗi JSON (caller PHẢI free), hoặc "[]" nếu file rỗng/không tồn tại

bool storage_save_device_list_json(const char *json_str);
char *storage_load_device_list_json(void);
// Tương tự schedule_json

bool storage_append_event_log(const char *event_json_line);
// Hành vi: Append 1 dòng JSON vào /littlefs/event_log.json
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
/littlefs/
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
| `wifi_cfg` | `timezone` | string (max 64) | `"Asia/Ho_Chi_Minh"` |

---

### 4.5 Module: BLE Adapter (`ble_adapter.h/.c`)

#### 4.5.1 Mô tả
Protocol Adapter phía BLE — kết nối/đọc dữ liệu từ sensor/actuator BLE đã đăng ký qua Web. Sử dụng NimBLE stack.

#### 4.5.2 Chức năng

| ID | Chức năng | Mô tả |
|---|---|---|
| BA-01 | Quản lý danh sách device BLE | `s_known_devices[]`, nạp từ storage lúc boot |
| BA-02 | Scan và đọc GATT | `ble_scan_task` (Core 0, priority 5), poll mỗi 10s |
| BA-03 | Post telemetry lên bus | `EVT_BLE_DEVICE_DATA` với `ble_telemetry_t` |
| BA-04 | Nhận lệnh từ Rule Engine | Handler `EVT_RULE_ACTION_REQUEST` → `is_my_device()` → **enqueue** vào `s_ble_cmd_queue` |
| BA-05 | Xử lý lỗi đọc sensor | Đọc lỗi/timeout → `valid=false`, vẫn post event (không im lặng) |
| BA-06 | Thực thi lệnh BLE (cmd queue) | `ble_scan_task` đọc `s_ble_cmd_queue` giữa các vòng scan → GATT write |

#### 4.5.3 API

```c
void ble_adapter_init(void);
// Hành vi: Load device list từ storage
//          Tạo s_ble_cmd_queue (QueueHandle_t, depth=8)
//          Register handler EVT_RULE_ACTION_REQUEST
//          Register handler EVT_REMOTE_COMMAND_REQUEST
//          Tạo ble_scan_task (Core 0, priority 5, stack 4096)

void ble_adapter_register_device(const char *device_id, const char *mac_address);
// Hành vi: Thêm device vào s_known_devices[]
// Giới hạn: Tối đa 16 devices
```

#### 4.5.4 BLE Scan Flow

```
ble_scan_task (vòng lặp vô hạn)
    │
    ├── 1. Kiểm tra s_ble_cmd_queue (non-blocking, timeout=0)
    │     │── Có lệnh? ──► GATT connect + write (ĐƯỢC PHÉP block tại đây
    │     │                  vì đang trong ble_scan_task, KHÔNG phải dispatcher)
    │     │── Rỗng? ──► tiếp tục scan
    │
    ├── 2. Với mỗi device trong s_known_devices[]:
    │     │
    │     ├── Scan tìm MAC address
    │     │     │── Tìm thấy? ──► Kết nối GATT
    │     │     │                   │── Đọc characteristic OK?
    │     │     │                   │     ──► malloc(ble_telemetry_t)
    │     │     │                   │         valid = true, value = đọc được
    │     │     │                   │         post EVT_BLE_DEVICE_DATA
    │     │     │                   │
    │     │     │                   │── Đọc lỗi/timeout?
    │     │     │                         ──► malloc(ble_telemetry_t)
    │     │     │                             valid = false  ← QUAN TRỌNG
    │     │     │                             post EVT_BLE_DEVICE_DATA
    │     │     │
    │     │     │── Không tìm thấy? ──► valid = false, post event
    │     │
    │     vTaskDelay(10000ms)
```

> **Ghi chú BLE scan budget:** 10s là interval giữa các vòng poll. Mỗi vòng quét tuần tự N device, mỗi device mất 0.5–3s (connect + service discovery + read + disconnect). Với N > 3, tổng thời gian vòng poll sẽ > 10s — vòng tiếp theo bắt đầu ngay khi vòng trước kết thúc, không có gap. Demo thực tế với 1–2 device hoạt động tốt trong budget 10s.

#### 4.5.5 is_my_device() + Command Queue Pattern

```c
// Command queue cho lệnh BLE — tách khỏi dispatcher context
static QueueHandle_t s_ble_cmd_queue; // tạo trong ble_adapter_init(), depth=8

typedef struct {
    char device_id[32];
    char command[32];
} ble_cmd_t;

// Handler chạy trong DISPATCHER context — KHÔNG ĐƯỢC block:
static void on_rule_action_request(gateway_event_t *evt) {
    rule_action_request_t *req = (rule_action_request_t *)evt->data;
    if (!is_my_device(req->target_device_id)) {
        return;  // KHÔNG phải device BLE → bỏ qua im lặng
    }
    // CHỈ enqueue, KHÔNG gọi GATT trực tiếp:
    ble_cmd_t cmd;
    strncpy(cmd.device_id, req->target_device_id, 32);
    strncpy(cmd.command, req->command, 32);
    xQueueSend(s_ble_cmd_queue, &cmd, 0);  // non-blocking, trả về ngay
}

// ble_scan_task đọc queue và thực hiện GATT write (ĐƯỢC PHÉP block):
// → Xem bước 1 trong BLE Scan Flow (§4.5.4)
```

> **Tại sao không gọi GATT write trực tiếp trong handler?** Handler chạy trong `event_dispatcher_task` — nếu GATT connect/write bị timeout (3–10s khi device offline), **toàn bộ event bus bị đứng**, kể cả `EVT_BUTTON_ALARM_STOP`. Tách qua command queue đảm bảo dispatcher luôn trả về nhanh (< 1ms).

Rule Engine **không cần biết** giao thức nào xử lý device nào — mỗi adapter tự quyết định.

#### 4.5.6 Verification Contracts

**Verification Contract — FR-BLE-01: Quét Passive Beacon / Advertisement và Thu thập Telemetry BLE**

- **Tier:** target
- **Preconditions:** Board nạp firmware bật NimBLE stack; task `ble_scan_task` hoạt động trên Core 0; Rule Engine đã đăng ký nhận `EVT_BLE_DEVICE_DATA`.
- **Stimulus:** Thiết bị phát BLE (cảm biến thật, app điện thoại nRF Connect, hoặc lệnh CLI) phát quảng bá BLE Advertisement chứa Service Data Environmental Sensing (`0x181A`), BTHome (`0xFCD2`), hoặc thiết bị có Local Name / Device ID `sensor_temp_01`.
- **Expected:**
  1. BLE Adapter phân tích (parse) thành công gói tin advertisement, trích xuất `device_id` và giá trị cảm biến (`value`).
  2. Phục hồi cấu trúc `sensor_telemetry_t` với `valid=true` và post `EVT_BLE_DEVICE_DATA` lên Event Bus trong vòng ≤ 500ms.
  3. Serial console in log nhận dạng cảm biến: `[BLE] Discovered sensor ...` và Rule Engine kích hoạt quy tắc tự động hóa tương ứng (VD: Rule #1 nhiệt độ > 30.0°C bật quạt làm mát).
  4. Task `ble_scan_task` định kỳ báo danh `TASK_HEARTBEAT_BLE` tới Supervisor, không gây treo hệ thống.
- **Timing / tolerance:** Phát hiện và chuyển tiếp lên Event Bus trong vòng ≤ 1.0 giây từ khi nhận gói tin quảng bá.
- **Prohibited:**
  - Treo Event Bus Dispatcher do gọi hàm BLE blocking trong callback.
  - Rò rỉ bộ nhớ (memory leak) do không giải phóng packet mbuf hoặc payload event bus.
  - Watchdog timeout hoặc Guru Meditation crash.
- **Operator instructions:**
  1. Mở Serial Console ở tốc độ 115200 baud.
  2. Dùng điện thoại (app **nRF Connect for Mobile** hoặc tương đương), vào tab *Advertiser*, tạo gói tin quảng bá với Complete Local Name `sensor_temp_01` và Service Data UUID `0x181A` chứa giá trị hex `0x1A 0x18 0xB8 0x0C` (tương ứng 32.56°C).
  3. Bật phát quảng bá (Enable Advertising) gần mạch ESP32-S3.
  4. Quan sát console: xác nhận log Gateway nhận dạng được `sensor_temp_01`, giá trị `32.5°C`, và Rule Engine in thông báo: `[RULE_ENGINE] >>> MATCHED Rule #1! Device 'sensor_temp_01' val=... > thr=30.00 -> Target 'fan_relay_01' CMD='ON'`.
  5. Nếu không có sẵn điện thoại tại thời điểm test, sử dụng lệnh CLI `ble test` để bơm gói tin raw advertisement tương đương vào parser và kiểm chứng cùng chu trình.

---

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
| RE-01 | Nạp rule từ storage | Parse `schedule.json`, lọc `type="automation"` |
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
    RULE_CONDITION_SENSOR_GT,   // sensor_value > threshold
    RULE_CONDITION_SENSOR_LT,   // sensor_value < threshold
    RULE_CONDITION_TIME_MATCH,  // đúng giờ (thường đã được scheduler lọc sẵn)
} rule_condition_type_t;

typedef struct {
    int id;
    char device_id[32];              // sensor phát sinh điều kiện
    rule_condition_type_t condition_type;
    float threshold;
    char target_device_id[32];       // thiết bị bị tác động
    char action_command[32];         // "ON", "OFF", "SET:75"
} automation_rule_t;
```

#### 4.7.5 Luồng đánh giá BLE Telemetry (ví dụ đầy đủ)

```
on_ble_telemetry(evt)
    │
    ├── telem->valid == false?
    │     ├── YES → log WARN "sensor lỗi, giữ safe-default"
    │     │         post EVT_SENSOR_VALUE_INVALID
    │     │         return (KHÔNG đánh giá rule)
    │     │
    │     └── NO → tiếp tục đánh giá
    │
    ├── Duyệt s_rules[0..s_rule_count-1]:
    │     │
    │     ├── rule.device_id == telem->device_id?
    │     │     │── NO → skip, tiếp rule sau
    │     │     │── YES ↓
    │     │
    │     ├── Đánh giá điều kiện:
    │     │     SENSOR_GT: telem->value > rule.threshold?
    │     │     SENSOR_LT: telem->value < rule.threshold?
    │     │     │── NO → skip
    │     │     │── YES ↓
    │     │
    │     ├── MATCHED! → malloc(rule_action_request_t)
    │     │     req->target_device_id = rule.target_device_id
    │     │     req->command = rule.action_command
    │     │     post EVT_RULE_ACTION_REQUEST
    │     │     (dispatcher free(req) sau khi BLE+MQTT handler xong)
    │     │
    │     └── tiếp rule sau
    │
    └── KHÔNG free(telem) — dispatcher lo
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
//          Tạo scheduler_task (Core 0, priority 5, stack 4096)

void scheduler_reload(void);
// Hành vi: Đọc lại schedule.json, reset s_last_triggered_minute = -1
//          (trigger ngay nếu có schedule khớp phút hiện tại)
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

#### 4.9.4 Nút bấm GPIO

| Nút | GPIO | Chức năng | Xử lý |
|---|---|---|---|
| Next Screen | (cấu hình) | Chuyển màn hình | ISR → post `EVT_BUTTON_SCREEN_NEXT` |
| Alarm Stop | (cấu hình) | Tắt báo thức | ISR → post `EVT_BUTTON_ALARM_STOP` |

> **Quan trọng:** ISR chỉ `event_bus_post(..., from_isr=true)` rồi thoát ngay. KHÔNG vẽ, KHÔNG gọi hàm blocking trong ISR.

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

### 4.10 Phân hệ Mở Rộng: RS485 Modbus Master Adapter (Đã lược bỏ / Deprecated)

> [!NOTE]
> **Trạng thái phân hệ:** ĐÃ LƯỢC BỎ (OUT OF SCOPE / DEPRECATED)
> Để phục vụ mục tiêu hệ thống Smart Home 100% không dây và tinh giản phần cứng, toàn bộ việc kết nối dây qua RS485, chip transceiver SP3485/MAX485 và các bài test `FR-RS485-01`, `FR-RS485-02`, `FR-RS485-03` được gỡ bỏ khỏi phạm vi bắt buộc của dự án. Hệ thống ưu tiên tối đa cho 2 chuẩn không dây tích hợp sẵn trên chip là **Bluetooth Low Energy (BLE)** và **Wi-Fi 2.4 GHz**.
> Nội dung mục 4.10 dưới đây chỉ được lưu giữ dưới dạng tài liệu kỹ thuật tham khảo cho các kịch bản mở rộng công nghiệp tương lai.

#### 4.10.1 Mô tả (Tham khảo)
Chịu trách nhiệm quản lý toàn bộ giao tiếp vật lý RS485 (Half-Duplex) với các thiết bị cảm biến và cơ cấu chấp hành hiện trường theo chuẩn Modbus RTU Master.

#### 4.10.2 Cấu hình phần cứng & Thông số
| Tham số | Giá trị | Ghi chú |
|---|---|---|
| UART Peripheral | UART2 | Chân TX: GPIO 17, RX: GPIO 16 |
| Flow Control (DE/RE) | GPIO 4 | Mức HIGH: Truyền (TX), Mức LOW: Nhận (RX) |
| Baudrate mặc định | 9600 bps (hỗ trợ 19200, 115200) | Cấu hình qua Config Manager |
| Data Frame | 8 data bits, 1 stop bit, No parity (8-N-1) | |
| Task Priority / Core | Priority 5 / Core 0 | Chạy độc lập với mạng Wi-Fi |
| Command Queue Depth | 10 slots (`s_rs485_cmd_queue`) | Hàng đợi lệnh điều khiển ghi |
| Response Timeout | 300 ms (mặc định) | Timeout cho mỗi frame phản hồi từ Slave |
| Max Consecutive Retries | 3 lần | Sau 3 lần thất bại liên tiếp coi như Offline |
| Turnaround Delay (t3.5) | ~3.5 ký tự (~4ms ở 9600bps) | Khoảng lặng tối thiểu giữa các frame |

#### 4.10.3 Bảng cấu hình Polling Node (`rs485_node_cfg_t`)
```c
typedef struct {
    char     device_id[32];        // Mã định danh thiết bị (VD: "rs485_th_01")
    uint8_t  slave_id;             // Địa chỉ Slave Modbus (1 - 247)
    uint8_t  function_code;        // 0x03 (Read Holding) hoặc 0x04 (Read Input)
    uint16_t start_reg;            // Địa chỉ thanh ghi bắt đầu
    uint16_t reg_count;            // Số lượng thanh ghi cần đọc
    uint32_t poll_interval_ms;     // Chu kỳ polling (VD: 5000ms)
    uint32_t last_polled_ms;       // Timestamp lần poll gần nhất
    float    scale;                // Hệ số nhân (VD: 0.1 nếu giá trị trả về nhân 10)
    float    offset;               // Giá trị bù
    uint8_t  consecutive_failures; // Số lần timeout liên tiếp hiện tại
    bool     is_online;            // Trạng thái online của node
} rs485_node_cfg_t;
```

#### 4.10.4 State Machine & Circuit Breaker
```
  [IDLE] ◄────────────────────────────────────────────────────────┐
    │                                                             │
    ├── Có lệnh trong s_rs485_cmd_queue? ──► [XỬ LÝ GHI FC05/06]──┤
    │                                                             │
    └── Đến hạn poll node tiếp theo (is_online=true hoặc hết backoff)?
          │
          ▼
    [SEND_REQUEST] (Bật GPIO4 HIGH -> Gửi Modbus Frame -> Kéo GPIO4 LOW)
          │
          ▼
    [WAIT_RESPONSE] (Chờ byte UART2, Timeout 300ms)
          ├── Nhận đủ frame & CRC16 đúng ──► [PARSE_DATA] ──► Post EVT_RS485_DEVICE_DATA
          │                                                    (Reset failures = 0, online=true)
          └── Timeout hoặc CRC16 sai
                    │
                    ├── failures < 3 ──► Tăng failures, Retry ngay sau khoảng lặng t3.5
                    │
                    └── failures >= 3 ──► [CIRCUIT BREAKER TRIGGERED]
                                          - Đánh dấu is_online = false
                                          - Post EVT_DEVICE_OFFLINE_ALERT
                                          - Bỏ qua sang node kế tiếp (không nghẽn bus)
                                          - Áp dụng Backoff: Chỉ thử lại sau mỗi 60 giây
```

#### 4.10.5 Verification Contracts — RS485 Adapter (Mô phỏng hồi quy tại `mock_peripherals.c`)

**Verification Contract — FR-RS485-01: Polling Telemetry Modbus RTU (Mô phỏng phần mềm)**
- **Tier:** host / target (mock)
- **Preconditions:** Firmware kích hoạt tầng ngoại vi ảo `mock_peripherals.c` với Slave ID 0x01 (Reg 0x0001 = 250 -> 25.0°C).
- **Stimulus:** Khởi động Gateway, quan sát bus ảo trong chu kỳ 4 giây.
- **Expected:** Task `mock_rs485_task` sinh frame Modbus RTU chuẩn, giải mã thành công nhiệt độ 25.0°C, post `EVT_BLE_DEVICE_DATA` hoặc telemetry nội bộ.
- **Operator instructions:**
  1. Mở serial monitor 115200 baud.
  2. Gõ lệnh `mock rs485 start 4000`.
  3. Xác nhận trên log xuất hiện dòng `[MOCK_RS485] Telemetry generated: 25.0 C`.

**Verification Contract — FR-RS485-02: Timeout, Retry & Safe-Default (Mô phỏng phần mềm)**
- **Tier:** target (mock)
- **Preconditions:** Tầng ngoại vi ảo đang phát telemetry định kỳ.
- **Stimulus:** Bơm lỗi bus timeout qua Serial CLI: `mock rs485 fault timeout`.
- **Expected:** Hệ thống phát hiện Slave không phản hồi, không bị treo bus UART, giữ nguyên trạng thái an toàn hiện hành và post cảnh báo.
- **Operator instructions:**
  1. Gõ `mock rs485 fault timeout` trên Serial CLI.
  2. Quan sát log hệ thống: xác nhận xuất hiện cảnh báo timeout và Rule Engine duy trì trạng thái an toàn Safe-Default.
  3. Gõ `mock rs485 fault none` để khôi phục hoạt động bình thường.

**Verification Contract — FR-RS485-03: Downlink Command Execution (Mô phỏng phần mềm)**
- **Tier:** target (mock)
- **Preconditions:** Hệ thống nhận lệnh điều khiển thiết bị qua MQTT hoặc Serial CLI.
- **Stimulus:** Gõ lệnh `cmd send fan_relay_01 ON` hoặc gửi qua MQTT.
- **Expected:** Lệnh được tiếp nhận, chuyển đổi trạng thái relay đích, và gửi bản tin ACK về `gateway/{id}/command/ack` với `success=true`.
- **Operator instructions:**
  1. Gõ `cmd send fan_relay_01 ON` trên Serial CLI.
  2. Quan sát log: xác nhận relay chuyển trạng thái ON và Event Bus phát `EVT_COMMAND_ACK`.

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
- **Định dạng file:** File nhị phân có header cố định 64 bytes:
  ```c
  typedef struct {
      uint32_t magic;          // 0x53594E43 ("SYNC")
      uint32_t write_offset;   // Con trỏ vị trí byte ghi tiếp theo
      uint32_t read_offset;    // Con trỏ vị trí byte đã gửi thành công lên server
      uint32_t total_records;  // Tổng số bản ghi tích lũy
      uint32_t unacked_count;  // Số bản ghi đang chờ gửi
      uint32_t crc32;          // Checksum bảo vệ header
  } sync_queue_header_t;
  ```
- **Hành vi khi Offline:** Khi `uplink_task` mất mạng (Wi-Fi hoặc MQTT ngắt), mọi telemetry và event thay vì gửi mạng sẽ được gửi sang `s_sd_log_queue` để append vào cuối file `sync_queue.bin`.
- **Hành vi khi Online trở lại (Backfill):**
  1. `uplink_task` kết nối thành công tới MQTT Broker.
  2. Gửi tín hiệu kích hoạt tiến trình đồng bộ bù.
  3. `sd_log_task` đọc một lô (Batch) gồm **20 bản ghi** từ vị trí `read_offset`.
  4. `uplink_task` publish lên MQTT Broker với QoS 1 kèm khoảng giãn cách `200ms` giữa các message (Rate Limiting).
  5. Sau khi nhận đủ `PUBACK` cho cả batch, `sd_log_task` cập nhật `read_offset` mới và gọi `f_sync()` để lưu trạng thái.
  6. Lặp lại cho tới khi `read_offset == write_offset` (hàng đợi rỗng).

#### 4.11.4 Chống hỏng tệp khi mất điện đột ngột (Power Loss Safe)
- Không mở và đóng file liên tục trong vòng lặp nhỏ.
- Định kỳ gọi `f_sync()` sau mỗi **10 bản ghi** hoặc tối đa sau mỗi **5 giây** để ép flush cache từ RAM xuống sector vật lý của thẻ SD.
- Nếu Gateway mất điện đột ngột: Dữ liệu tối đa bị mất chỉ là trong 5 giây cuối cùng; cấu trúc bảng FAT và file `sync_queue.bin` không bị lỗi filesystem (corrupt).

#### 4.11.5 Verification Contracts — SD Card Logger

**Verification Contract — FR-SD-01: Ghi Telemetry CSV vào thẻ nhớ**
- **Tier:** target
- **Preconditions:** Thẻ nhớ MicroSD định dạng FAT32 được cắm vào khe thẻ (GPIO 14, 15, 2, 13).
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
    "broker_uri": "mqtt://broker.emqx.io:1883",
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
| `status` | `status` | In trạng thái hệ thống, Uptime, Heap, Active & Alive Heartbeat Mask (`0xD7`) |
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
Module đóng vai trò "người giám sát độc lập" (Watchdog Supervisor) chịu trách nhiệm đảm bảo tính khả dụng 24/7 của toàn bộ hệ thống. Sử dụng kết hợp **Hardware Watchdog (WDT)** của SoC ESP32 và cơ chế **Heartbeat Bitmask** trên FreeRTOS để theo dõi từng task nghiệp vụ riêng lẻ. Nếu bất kỳ task nào (dù là task mạng, task RS485, task thẻ nhớ hay task hiển thị) bị rơi vào trạng thái treo (Deadlock, Stuck ở hàm I/O blocking hoặc vòng lặp vô hạn), Supervisor sẽ phát hiện, ghi lại chẩn đoán lỗi vào thẻ nhớ SD và kích hoạt reset an toàn.

#### 4.13.2 Bảng phân bổ Heartbeat Bitmask các Task
```c
#define TASK_HEARTBEAT_EVENT_BUS   (1 << 0)  // event_dispatcher_task
#define TASK_HEARTBEAT_BLE         (1 << 1)  // ble_scan_task
#define TASK_HEARTBEAT_UPLINK      (1 << 2)  // uplink_task
#define TASK_HEARTBEAT_SD_LOGGER   (1 << 3)  // sd_log_task
#define TASK_HEARTBEAT_SCHEDULER   (1 << 4)  // scheduler_task
#define TASK_HEARTBEAT_DISPLAY     (1 << 5)  // display_task

#define TASK_ALL_HEARTBEATS_MASK   (0x3F)    // Tất cả 6 task bắt buộc
```

#### 4.13.3 Chu trình giám sát & Xử lý sự cố
1. `supervisor_task` chạy ở mức ưu tiên cao nhất (**Priority 7**, Core 0), thức dậy mỗi **1000ms**.
2. Mỗi task nghiệp vụ sau khi hoàn thành một chu kỳ làm việc (hoặc sau mỗi lượt xử lý queue) bắt buộc phải gọi hàm:
   ```c
   supervisor_report_heartbeat(uint32_t task_mask);
   ```
3. `supervisor_task` tích lũy các cờ đã báo danh vào biến `s_current_heartbeats`.
4. Nếu một task **không báo danh liên tục quá 15 giây** (`SUPERVISOR_MAX_STALL_TIME_MS`):
   - Supervisor xác định chính xác Task ID bị treo.
   - Ghi khẩn cấp thông tin chẩn đoán lỗi vào file `/sdcard/logs/crash.log`: Task Name, Thời gian treo, Bộ nhớ Free Heap khả dụng, Stack Watermark nhỏ nhất của task.
   - Ép `f_sync()` tức thời.
   - Gọi `esp_restart()` để đưa Gateway trở lại trạng thái vận hành ổn định.

#### 4.13.4 Verification Contracts — Task Supervisor

**Verification Contract — FR-SUP-01: Báo danh Heartbeat bình thường**
- **Tier:** target
- **Preconditions:** Gateway nạp firmware đang hoạt động đầy đủ 6 task.
- **Stimulus:** Giám sát thiết bị chạy liên tục trong 10 phút dưới tải bình thường.
- **Expected:** Supervisor task định kỳ kiểm tra và reset watchdog phần cứng. Không có thông báo lỗi hay reset bất thường; cờ của cả 6 task được set đều đặn mỗi chu kỳ ≤ 5s.
- **Prohibited:** Reset ngẫu nhiên khi hệ thống hoạt động bình thường.
- **Operator instructions:**
  1. Khởi động thiết bị, mở serial monitor.
  2. Quan sát log định kỳ: `[SUP] System healthy. Heap free: XXX KB, All tasks alive (mask=0x3F)`.

**Verification Contract — FR-SUP-02: Bắt lỗi Task bị treo & Tự khôi phục an toàn**
- **Tier:** target
- **Preconditions:** Nạp firmware kích hoạt chế độ test chẩn đoán (Test Hook: lệnh `test freeze event_bus`).
- **Stimulus:** Gửi lệnh qua Serial CLI: `test freeze event_bus` (khiến `event_dispatcher_task` rơi vào vòng lặp vô hạn mà không báo heartbeat).
- **Expected:**
  1. Đúng 15 giây sau khi task bị đóng băng, `supervisor_task` phát hiện thiếu cờ `TASK_HEARTBEAT_EVENT_BUS`.
  2. Ghi cảnh báo ra log Serial: `[SUP] CRITICAL: Task 'event_bus' stalled > 15s! Flushing crash log...`.
  3. File `/sdcard/logs/crash.log` được ghi thành công với nguyên nhân `TASK_STALL_EVENT_BUS`.
  4. Gateway thực hiện `esp_restart()` và khởi động lại hoàn tất trong < 3 giây.
- **Operator instructions:**
  1. Mở Serial terminal, gõ `test freeze event_bus`.
  2. Bấm đồng hồ đếm: Đúng 15 giây sau, xác nhận thấy log crash và gateway tự reboot.
  3. Sau khi reboot, kiểm tra file `/sdcard/logs/crash.log` ghi lại sự kiện đóng băng chính xác.

---

### 4.14 Boot Sequence tổng thể (Cập nhật)

```
app_main()
    │
    ├── 1. nvs_flash_init()                          ← Khởi tạo NVS Flash lưu trữ tham số
    ├── 2. event_bus_init()                          ← Khởi tạo Event Queue (32 slots)
    ├── 3. supervisor_init()                         ← Khởi tạo Task Supervisor (TWDT, Core 0, Prio 10)
    │       ├── Khởi tạo ESP Task WDT (TWDT)
    │       ├── Đăng ký mask giám sát 8 tasks (0xD7)
    │       └── Tạo supervisor_task (Core 0)
    │
    ├── 4. rtc_init()                                ← Nạp POSIX Timezone "ICT-7" từ NVS & RTC DS3231
    ├── 5. config_manager_init()                     ← Nạp cấu hình Wi-Fi, MQTT, danh sách BLE
    ├── 6. wifi_provisioning_init() & web_server()   ← Wi-Fi SoftAP Portal & Web Dashboard (Port 80)
    ├── 7. mqtt_adapter_init() & uplink_init()       ← Uplink Client & Telemetry Publisher (Core 1)
    ├── 8. rule_engine_init()                        ← Engine IF-THEN đánh giá cạnh & trễ hysteresis
    │       └── register EVT_BLE_DEVICE_DATA,
    │                    EVT_SCHEDULE_AUTOMATION_TRIGGERED
    │
    ├── 9. scheduler_init()                          ← Bộ lập lịch báo thức và tự động hóa (Core 0)
    │       └── Tạo scheduler_task (Core 0, priority 5)
    │
    ├── 10. mock_peripherals_init()                  ← Tầng ngoại vi ảo chạy ngầm phục vụ kiểm thử
    ├── 11. ble_adapter_init()                       ← Khởi tạo NimBLE stack & Central Passive Observer
    │       └── Tạo ble_scan_task (Core 0, priority 5)
    │
    ├── 12. serial_cli_init()                        ← Console tương tác UART0 (Core 1, priority 3)
    │       └── Tạo serial_cli_task (Core 1)
    │
    └── 13. event_bus_start_dispatcher()             ← BẮT ĐẦU ĐIỀU PHỐI (SAU CÙNG)
            ├── Tạo event_dispatcher_task (Core 0, priority 6)
            └── Phát tín hiệu khởi động [SYS] Alive marker 1
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
| timezone | VARCHAR(64) | | VD: "Asia/Ho_Chi_Minh" |
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

### 7.3 JSON Schema — schedule.json (trên Gateway LittleFS)

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

### 7.3.2 JSON Schema — rules.json (trên Gateway Storage)

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

### 7.4 JSON Schema — device_list.json (trên Gateway Storage)

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

### 7.5 JSON Schema — event_log.json (trên Gateway LittleFS)

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
  "timezone": "Asia/Ho_Chi_Minh"
}
```

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

#### POST /api/v1/gateways/claim

Người dùng gán Gateway mới vào tài khoản sau khi thiết bị lên mạng.

**Headers:** `Authorization: Bearer <user-jwt>`  
**Request Body:**
```json
{
  "claim_code": "GW-9821A0",
  "name": "Đồng Hồ Thông Minh Phòng Khách"
}
```
**Response 200:**
```json
{
  "success": true,
  "gateway_id": "gw_9821a0",
  "message": "Gateway claimed successfully"
}
```

### 8.6.2 Cơ Chế Cloud Push Thông Qua MQTT

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
    status                                       ← "online"/"offline" (retained, LWT)
    device/{device_id}/
        telemetry                                ← Gateway → Backend (QoS 0)
        command                                  ← Backend → Gateway (QoS 1)
    config/
        schedule                                 ← Backend → Gateway (QoS 1, retained)
    sync/
        event_log                                ← Gateway → Backend (QoS 1)
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

#### Command (Backend → Gateway)
```json
{
  "command": "ON",
  "source": "web_app",
  "timestamp": 1723621234
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

### 9.3 QoS

| Topic | QoS | Lý do |
|---|---|---|
| telemetry | 0 | Mất 1-2 data point không ảnh hưởng lớn |
| command | 1 | Lệnh điều khiển phải đảm bảo tới nơi |
| config/schedule | 1 | Config quan trọng, không được mất |
| status | 0 | Retained, LWT tự xử lý |
| sync/event_log | 1 | Log cần đảm bảo tới Backend |

---

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
| | 6. BLE adapter nhận (is_my_device=true) → GATT write → quạt bật |
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

### 11.2 Safe-default Pattern

```
Sensor đọc lỗi (valid=false)
    │
    ├── Rule Engine: KHÔNG đánh giá rule
    │                KHÔNG toggle thiết bị
    │                Giữ trạng thái cuối cùng (quạt đang bật → vẫn bật)
    │
    ├── Post EVT_SENSOR_VALUE_INVALID
    │
    └── Display: hiển thị cảnh báo sensor lỗi (nếu implement)
```

### 11.3 Task Watchdog & Supervisor Matrix (Chuẩn hóa 8 Task, Timeout 10s, Mask 0xD7)

| Task | Watchdog Mechanism | Timeout | Hành vi khi timeout |
|---|---|---|---|
| `supervisor_task` | Hardware WDT (ESP TWDT) | 3s | Reset hardware nếu supervisor bị kẹt |
| `event_dispatcher_task` | Heartbeat bitmask (bit 0) | 10s | Supervisor ghi chẩn đoán vào RTC Fast Memory & Console -> Gọi `esp_restart()` ngay lập tức |
| `mock_rs485_task` | Heartbeat bitmask (bit 1) | 10s | Supervisor ghi chẩn đoán vào RTC Fast Memory & Console -> Gọi `esp_restart()` ngay lập tức |
| `uplink_task` | Heartbeat bitmask (bit 2) | 10s | Tự restart Wi-Fi/MQTT; nếu quá 3 lần -> Reset an toàn |
| `main_task` | Heartbeat bitmask (bit 3) | 10s | Supervisor phát hiện stall -> Reset an toàn |
| `scheduler_task` | Heartbeat bitmask (bit 4) | 10s | Supervisor log cảnh báo, reset timer/RTC task |
| `display_task` | Heartbeat bitmask (bit 5) | 10s | Reinit màn hình I2C SSD1327 |
| `serial_cli_task` | Heartbeat bitmask (bit 6) | 10s | Flush UART0 buffer, reset CLI shell |
| `ble_scan_task` | Heartbeat bitmask (bit 7) | 10s | Restart NimBLE central scanner, khôi phục passive observation |

> **Quy chuẩn thời gian khôi phục (M17):** Thời gian phát hiện treo task là **10 giây** (chu kỳ kiểm tra 2 giây); thời gian khởi động lại chip ESP32-S3 là **~2.5 giây**. Tổng thời gian từ khi task chết đến khi hệ thống hồi phục hoàn toàn là **< 12.5 giây**, đáp ứng tuyệt đối chỉ tiêu **M17 (< 15 giây)**.

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
| Rule Engine latency | < 50ms từ lúc nhận telemetry tới lúc post action | Đo trên ESP32 240MHz |
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
| M02 | Kết nối BLE sensor, đọc telemetry | UC-02, UC-03 | Gateway scan, kết nối, đọc GATT characteristic từ ≥ 1 BLE sensor đã đăng ký. Giá trị hiển thị trên OLED và/hoặc Web App | Không thể kết nối BLE sau 3 chu kỳ scan (30s), hoặc giá trị `valid` luôn false |
| M03 | Kết nối MQTT devices, nhận/gửi lệnh | UC-02, UC-03 | Gateway kết nối MQTT broker, subscribe topic command, publish telemetry. Web App nhận được telemetry real-time | MQTT connection fail liên tục, hoặc lệnh command không tới device |
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
| **M17** | **Task Supervisor & Watchdog 24/7** | **UC-05** | **Heartbeat của tất cả task được giám sát liên tục; khi cố tình gây treo 1 task, supervisor ghi `crash.log` ra SD và reboot trong < 15s.** | **Hệ thống treo im lặng không tự reset; WDT không fire.** |

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

### 15.1 Sơ đồ phân bổ Flash Partition (4MB)

```
Offset     Size    Name        Type     Mô tả
──────     ────    ────        ────     ─────
0x9000     24 KB   nvs         data     Wi-Fi credentials, gateway config
0xF000     8  KB   otadata     data     OTA metadata
0x11000    4  KB   phy_init    data     RF calibration
0x20000    1  MB   factory     app      Firmware chính
0x120000   1  MB   ota_0       app      OTA slot (Should-have)
0x220000   1.5 MB  storage     data     LittleFS: schedule, device_list, event_log
```

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
