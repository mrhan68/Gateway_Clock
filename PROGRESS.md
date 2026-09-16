# 📊 Bảng Theo Dõi Tiến Độ Phát Triển Dự Án (PROGRESS.md)
### Smart Clock & Industrial Edge IoT Gateway (ESP32-S3)

> **Phương pháp**: AI Closed-Loop Programming (AICLP) — Chế độ Manual Testing  
> **Cập nhật lần cuối**: 13/09/2026  
> **Trạng thái phần cứng**: ESP32-S3 Bare-board (Cắm cáp qua cổng `/dev/ttyACM0` - UART0)  
> **Tài liệu đặc tả**: [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md)  
> **Kế hoạch kiểm thử**: [testing/test-plan.yaml](file:///home/chien/ai-workspace/qwen-test/testing/test-plan.yaml)  
> **Cấu hình giả lập ngoại vi**: [MOCK_PERIPHERALS.md](file:///home/chien/ai-workspace/qwen-test/MOCK_PERIPHERALS.md)

---

## 1. Tổng Quan Tiến Độ Hệ Thống (Snapshot)

```
[████████████████████████░░░░░░] 80% Hoàn thành Wave 1 (Toàn bộ logic Bare-board 100% Green)
```

| Hạng mục | Tiến độ | Trạng thái | Ghi chú |
|---|:---:|:---:|---|
| **Host Unit Tests** | 11/11 | 🟢 **100% PASSED** | Kiểm thử thuật toán Modbus CRC, Sync Queue, Rule Engine, MQTT Routing |
| **Core Infrastructure (Bare-board)** | 4/4 | 🟢 **100% PASSED** | Event Bus, Task Supervisor, NVS Config Manager, Serial CLI |
| **Network & Cloud Uplink** | 4/4 | 🟢 **100% PASSED** | SoftAP Captive Portal, Wi-Fi Scan, SNTP Real-Time Sync, MQTT Uplink |
| **Local Automation & Scheduler** | 2/2 | 🟢 **100% PASSED** | Rule Engine (IF-THEN, Safe-default), Local Scheduler (`TC-CLK-03`) |
| **Journey Tests (Critical Paths)** | 2/4 | 🟡 **50% PASSED** | `JRN-01` (Boot), `JRN-04` (Network/MQTT) Green; `JRN-02` (SD), `JRN-03` (RS485) chờ nối dây |
| **Peripheral Bringup (Ngoại vi vật lý)** | 0/3 | ⚪ **CHỜ PHẦN CỨNG** | MicroSD Card (SPI), RS485 Transceiver (UART2), I2C OLED/RTC |

---

## 2. Thông Số Phần Cứng Đã Xác Minh Thực Tế

* **SoC / Chip**: Espressif **ESP32-S3** (QFN56 revision v0.2, Dual-core Xtensa LX7 @ 160MHz).
* **Bộ nhớ Flash**: 16MB SPI Flash (Boya/XMC, chế độ DIO 40MHz).
* **Bộ nhớ PSRAM**: 8MB Octal/Quad PSRAM tích hợp.
* **Cổng giao tiếp nạp/debug**: Cổng MicroUSB/Type-C nối chip cầu nạp CH343 USB-to-UART bridge (tốc độ nạp `460800` baud, monitor `115200` baud) nhận diện tại `/dev/ttyACM0` (Windows COM7).
* **Bộ nhớ Heap khả dụng**: ~244,700 bytes free DRAM (cực kỳ dồi dào, ổn định không rò rỉ).

---

## 3. Ma Trận Module & Trạng Thái Triển Khai Chi Tiết

| Phân hệ / File mã nguồn | Chức năng chính | Điều khoản FSD | Kiểm thử liên quan | Trạng thái |
|---|---|---|---|:---:|
| [event_bus.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/event_bus.c) | Hàng đợi sự kiện trung tâm (Queue 20 slots, Core 0), điều phối bất đồng bộ giữa các tác vụ | §4.1.4 | `TC-SUP-01` | 🟢 **PASSED** |
| [supervisor.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.c) | Task Supervisor & Task Watchdog (TWDT), giám sát nhịp tim heartbeat bitmask, timeout 15s | §4.1.1, §4.1.5 | `TC-SUP-01` | 🟢 **PASSED** |
| [config_manager.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/config_manager.c) | Quản lý cấu hình đọc/ghi NVS Flash (Wi-Fi, MQTT Broker, RS485 params, Gateway ID) | §4.1.2 | `TC-CFG-02` | 🟢 **PASSED** |
| [serial_cli.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/serial_cli.c) | Console tương tác dòng lệnh qua USB UART0 (115200 baud): `help`, `status`, `wifi`, `config` | §4.1.3 | `TC-CFG-02` | 🟢 **PASSED** |
| [wifi_provisioning.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/wifi_provisioning.c) | SoftAP Captive Portal (`SmartClock-Setup`), DNS Redirection, HTTP Web Server cấu hình Dark mode. Chế độ Volatile RAM (tự xóa khi reset) | §4.2.1 | `TC-WIFI-01` | 🟢 **PASSED** |
| [wifi_provisioning.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/wifi_provisioning.c) | **Wi-Fi Scanner**: REST endpoint `/scan`, quét sóng Wi-Fi xung quanh và đổ động vào menu thả xuống | §4.2.1 | `TC-WIFI-02` | 🟢 **PASSED** |
| [rtc.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rtc.c) | Quản lý đồng hồ thời gian thực, đồng bộ SNTP Internet (`pool.ntp.org`), múi giờ `ICT-7` | §4.4.1 | `JRN-04` | 🟢 **PASSED** |
| [mqtt_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/mqtt_adapter.c) | MQTT Client: kết nối broker (`broker.emqx.io:1883`), LWT status, subscribe command & schedule | §4.6.1, §4.6.4 | `JRN-04` | 🟢 **PASSED** |
| [uplink.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c) | Tác vụ định kỳ đẩy bản tin telemetry (Uptime, Free Heap, IP) lên Cloud qua MQTT | §4.6.2 | `JRN-04` | 🟢 **PASSED** |
| [rule_engine.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c) | Bộ máy quy tắc tự động hóa nội bộ IF-THEN (GT/LT), Safe-Default, Action Executor | §4.7.1 | `TC-HOST-03` | 🟢 **PASSED** |
| [scheduler.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/scheduler.c) | Bộ lập lịch công việc và báo thức thời gian thực, quét chu kỳ 5s, chống trigger lặp | §4.8.1 | `TC-CLK-03` | 🟢 **PASSED** |
| [mock_peripherals.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/mock_peripherals.c) | Tầng ngoại vi ảo: mô phỏng Modbus RS485 (TH sensor, Relay board), Virtual Storage CSV, Fault Injection | §4.3, §4.5 | `TC-MOCK-01` | 🟢 **PASSED** |
| [web_server.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/web_server.c) | Web Dashboard SPA & REST API: `/api/rules` (GET/POST/DELETE), `/api/status`, `/api/telemetry`, `/api/relay` | §4.2, §4.7 | `TC-WEB-01` | 🟢 **PASSED** |
| `rs485_adapter.c` | Giao tiếp Modbus RTU Master (UART2, DE/RE flow control), đọc thanh ghi cảm biến FC03/04 | §4.3.1 | `TC-RS485-01`, `JRN-03` | ⏳ *Chờ nối dây* |
| `sd_logger.c` | Quản lý hệ thống file FATFS trên thẻ nhớ MicroSD qua SPI, ghi CSV nhật ký hàng ngày | §4.5.1 | `TC-SD-01`, `JRN-02` | ⏳ *Chờ nối dây* |
| `sync_queue.c` | Hàng đợi lưu trữ ngoại tuyến FIFO (`sync_queue.bin`) chống mất dữ liệu khi rớt mạng | §4.5.3 | `TC-HOST-02` | ⏳ *Chờ nối dây* |
| `display_ui.c` | Hiển thị màn hình OLED SSD1327 (I2C) đồng hồ kim/số, thời tiết, trạng thái cảm biến | §4.8.1 | `TC-CLK-01` | ⏳ *Chờ nối dây* |

---

## 4. Chi Tiết Kết Quả Kiểm Thử (Test Execution Records)

### 4.1. Automated Host Tests (`pytest tests/host/`)
* **Tổng số bài test**: 13 passed / 0 failed (Thời gian chạy: 0.04s).
* **Nội dung kiểm tra**:
  - `tests/host/test_modbus_crc.py`: Kiểm tra tính toán checksum CRC-16 Modbus (đúng từng byte theo chuẩn công nghiệp).
  - `tests/host/test_sync_queue.py`: Đóng gói binary header bản tin FIFO lưu thẻ nhớ.
  - `tests/host/test_rule_engine.py`: Đánh giá toán tử so sánh `>`, `<`, `==`, ngưỡng trễ hysteresis của Rule Engine.
  - `tests/host/test_mqtt_adapter.py`: Định tuyến topic MQTT và cấu trúc JSON telemetry.

### 4.2. Target Journey Tests (Kiểm thử thực địa trên board ESP32-S3)
* **`JRN-01` (System Boot and Alive Marker)**: ✅ **PASSED**
  - Bootloader nạp thành công phân vùng `factory` tại địa chỉ `0x20000`.
  - In thông báo khởi động và xuất hiện `[SYS] Alive marker 1` trong vòng 1.5s.
* **`JRN-04` (Wi-Fi & MQTT Uplink Connection)**: ✅ **PASSED**
  - Board kết nối Wi-Fi `TPLink_P1908` thành công, DHCP cấp IP `192.168.1.142`.
  - Tự động bắt sự kiện `EVT_WIFI_CONNECTED`, kích hoạt SNTP lấy giờ chuẩn `2026-09-13 16:10:01` (ICT-7).
  - Tự động kết nối MQTT broker `broker.emqx.io:1883`, chuyển trạng thái `ONLINE`, subscribe các topic điều khiển.
* **`TC-SUP-01` (Task Heartbeat Bitmask Tracking)**: ✅ **PASSED**
  - Giám sát đồng thời các tác vụ `EVENT_BUS` (bit 0), `CLI` (bit 6), `UPLINK` (bit 2) tạo thành mask `0x45`.
  - Supervisor duy trì trạng thái `HEALTHY`, uptime tăng liên tục.
* **`TC-CFG-02` (Serial CLI Interactive Shell)**: ✅ **PASSED**
  - Phản hồi tức thì các lệnh `help`, `status`, `wifi status`, `config show`.
* **`TC-WIFI-01` & `TC-WIFI-02` (Captive Portal & Wi-Fi Scanning)**: ✅ **PASSED**
  - SoftAP `SmartClock_Setup` phát sóng ổn định, DNS hijacking trỏ về `192.168.4.1`.
  - Quét sóng thời gian thực và cho phép chọn mạng trên giao diện web di động.
* **`TC-MOCK-01` (Mock Peripherals & Rule Engine Closed Loop)**: ✅ **PASSED**
  - Giả lập Modbus Slave 0x01 phát nhiệt độ/độ ẩm định kỳ qua Event Bus.
  - Rule Engine tự động phát hiện `temp > 30.0C` và `hum > 75.0%`, dispatch lệnh bật quạt làm mát (`fan_relay_01`) và máy hút ẩm (`dehumidifier_01`) đóng tiếp điểm Relay Slave 0x02.
  - Bơm giá trị thủ công `mock rs485 set 33.5 70.0 1` và ghi nhật ký CSV ảo hoạt động chính xác.
* **`TC-WEB-01` (Web Dashboard & REST API Engine)**: ✅ **PASSED**
  - Web Server duy trì liên tục (Always-on) trên cả mạng SoftAP và Station IP.
  - Single Page Dashboard Dark theme tích hợp: hiển thị giờ RTC, nhiệt độ, độ ẩm, nút bật/tắt 4 relay, quét sóng Wi-Fi.
  - Hệ thống REST API hoàn chỉnh: `GET /api/status`, `GET /api/telemetry`, `POST /api/relay`, `GET /api/rules`, `POST /api/rules`, `DELETE /api/rules`.
  - Hỗ trợ đầy đủ lệnh CLI xóa quy tắc `rule delete <id>`.

---

## 5. Các Sự Cố Kỹ Thuật Quan Trọng Đã Giải Quyết

```
┌──────────────────────────────┬──────────────────────────────────────────┬─────────────────────────────────────────┐
│ Sự Cố Gặp Phải               │ Nguyên Nhân Gốc (Root Cause)              │ Giải Pháp Kỹ Thuật Đã Áp Dụng           │
├──────────────────────────────┼──────────────────────────────────────────┼─────────────────────────────────────────┤
│ HTTP 431 Request Header Too  │ Header trình duyệt điện thoại vượt quá   │ Tăng CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048  │
│ Large khi truy cập Portal    │ buffer 512B mặc định của esp_http_server │ và tăng stack_size của server lên 8192  │
├──────────────────────────────┼──────────────────────────────────────────┼─────────────────────────────────────────┤
│ Tràn Stack task sys_evt      │ Task sys_evt mặc định chỉ có 2304B; chuỗi│ Tăng CONFIG_ESP_SYSTEM_EVENT_TASK_      │
│ (Guru Meditation Error)      │ ngắt nhận IP + printf + event bus tốn >3K│ STACK_SIZE=4096; dọn dẹp WebServer async│
├──────────────────────────────┼──────────────────────────────────────────┼─────────────────────────────────────────┤
│ ESP32 tự reset khi mở cổng   │ Tín hiệu DTR/RTS trên cầu nạp CH343 kéo  │ Cấu hình dtr=False, rts=False trong     │
│ serial monitor từ Linux      │ chân EN xuống đất qua mạch auto-reset    │ script giao tiếp pyserial               │
├──────────────────────────────┼──────────────────────────────────────────┼─────────────────────────────────────────┤
│ Reset TG1WDT khi chạy Mock   │ prvTimerTask chỉ có 2048B stack; hàm     │ Chuyển Mock RS485 sang dedicated task   │
│ RS485 poller                 │ printf format float + malloc làm tràn RAM│ mock_rs485_task (4096B stack, mutex)    │
└──────────────────────────────┴──────────────────────────────────────────┴─────────────────────────────────────────┘
```

---

## 6. Lộ Trình Kế Tiếp (Next Action Items)

Hiện tại, hệ thống đã hoàn thiện 100% phần **Core Infrastructure & Network Uplink** trên Bare-Board. Hai hướng đi tiếp theo:

### Hướng A: Tiếp tục hoàn thiện Logic phần mềm trên Bare-board (Khuyến nghị)
Không cần nối dây phần cứng bên ngoài, tiếp tục mở rộng các tính năng xử lý nội bộ:
1. **Rule Engine & Local Automation (`rule_engine.c`)**:
   - Viết module đánh giá điều kiện IF-THEN (Ví dụ: Nhiệt độ > 35°C -> Bật cảnh báo / Đóng relay).
   - Tích hợp vào Event Bus để nhận diện sự kiện và kích hoạt hành động.
2. **Local Scheduler & Alarm Clock (`scheduler.c`)**:
   - Quản lý lịch báo thức, lịch bật/tắt thiết bị theo giờ thực (RTC/SNTP).
   - Xử lý các lệnh lập lịch nhận từ MQTT qua topic `gateway/+/config/schedule`.
3. **Cloud Command Dispatcher**:
   - Xử lý các lệnh điều khiển nhận từ Cloud qua topic `gateway/+/device/+/command` và gửi phản hồi trạng thái.

### Hướng B: Bringup Ngoại Vi Phần Cứng (Sprint 2 - Peripheral Wiring)
Khi bạn sẵn sàng cắm dây hoặc hàn linh kiện:
1. **Module Thẻ nhớ MicroSD (SPI)**: Kết nối chân CS, MOSI, MISO, SCK -> Xác minh FATFS mount (`JRN-02`).
2. **Module Giao tiếp RS485 (UART2)**: Kết nối TXD, RXD, DE/RE -> Xác minh đọc cảm biến Modbus RTU (`JRN-03`).
3. **Màn hình OLED SSD1327 & RTC DS3231 (I2C)**: Kết nối SDA, SCL -> Xác minh hiển thị đồng hồ và cảm biến.
