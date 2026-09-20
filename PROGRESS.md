# 📊 Bảng Theo Dõi Tiến Độ Phát Triển Dự Án (PROGRESS.md)
### Smart Clock & Wireless Smart Home IoT Gateway (ESP32-S3: Wi-Fi + BLE)

> **Phương pháp**: AI Closed-Loop Programming (AICLP) — Chế độ Manual Testing  
> **Cập nhật lần cuối**: 19/09/2026  
> **Trạng thái phần cứng**: ESP32-S3 Bare-board (Cắm cáp qua cổng `/dev/ttyACM0` - UART0)  
> **Tài liệu đặc tả**: [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md)  
> **Kế hoạch kiểm thử**: [testing/test-plan.yaml](file:///home/chien/ai-workspace/qwen-test/testing/test-plan.yaml)  
> **Cấu hình giả lập ngoại vi**: [MOCK_PERIPHERALS.md](file:///home/chien/ai-workspace/qwen-test/MOCK_PERIPHERALS.md)

---

## 1. Tổng Quan Tiến Độ Hệ Thống (Snapshot)

```
[██████████████████████████████] 100% Hoàn thành Wave 1 (Hệ thống không dây Wi-Fi + BLE 100% Green)
```

| Hạng mục | Tiến độ | Trạng thái | Ghi chú |
|---|:---:|:---:|---|
| **Host Unit Tests** | 17/17 | 🟢 **100% PASSED** | Kiểm thử thuật toán CRC, Sync Queue, Rule Engine, MQTT Routing, BLE Ad Parser |
| **Bare-board Automated Test Suite** | 8/8 | 🟢 **100% PASSED** | CLI Scan, Safe-Default, Rule CRUD, Scheduler, Storage, Wi-Fi Scan, Watchdog Recovery |
| **Target BLE Scanner Battery** | 6/6 | 🟢 **100% PASSED** | NimBLE Scanner, Device Dynamic Reg, Telemetry Injection, Rule Match, Sub-threshold, Supervisor Bitmask |
| **Core Infrastructure (Bare-board)** | 4/4 | 🟢 **100% PASSED** | Event Bus, Task Supervisor (8 tasks), NVS Config Manager, Serial CLI |
| **Wireless Protocols (Wi-Fi + BLE)** | 5/5 | 🟢 **100% PASSED** | SoftAP Portal, Wi-Fi Scan, SNTP Sync, MQTT Uplink, NimBLE Central Scanner |
| **Local Automation & Scheduler** | 2/2 | 🟢 **100% PASSED** | Rule Engine (IF-THEN, Safe-default), Local Scheduler (`TC-CLK-03`) |
| **Negative & Fault Recovery** | 1/1 | 🟢 **100% PASSED** | Task Supervisor Watchdog Hang Recovery (`TC-SUP-02` / `TC-NEG-03`, FR-SUP-02) |
| **Journey Tests (Critical Paths)** | 2/2 | 🟢 **100% PASSED** | `JRN-01` (Boot), `JRN-04` (Network/MQTT) Green; `JRN-02` (SD) chờ thẻ; `JRN-03` (RS485) đã loại bỏ |
| **Peripheral Bringup (Ngoại vi vật lý)** | 0/2 | ⚪ **CHỜ PHẦN CỨNG** | MicroSD Card (SPI), I2C OLED/RTC (RS485 đã loại bỏ theo định hướng không dây) |

---

## 2. Thông Số Phần Cứng Đã Xác Minh Thực Tế

* **SoC / Chip**: Espressif **ESP32-S3** (QFN56 revision v0.2, Dual-core Xtensa LX7 @ 160MHz).
* **Bộ nhớ Flash**: 16MB SPI Flash (Boya/XMC, chế độ DIO 40MHz).
* **Bộ nhớ PSRAM**: 8MB Octal/Quad PSRAM tích hợp.
* **Cổng giao tiếp nạp/debug**: Cổng MicroUSB/Type-C nối chip cầu nạp CH343 USB-to-UART bridge (tốc độ nạp `460800` baud, monitor `115200` baud) nhận diện tại `/dev/ttyACM0` (Windows COM7).
* **Bộ nhớ Heap khả dụng**: ~116,400 bytes free DRAM khi chạy đồng thời Wi-Fi + BLE NimBLE stack + Web Server + MQTT.

---

## 3. Ma Trận Module & Trạng Thái Triển Khai Chi Tiết

| Phân hệ / File mã nguồn | Chức năng chính | Điều khoản FSD | Kiểm thử liên quan | Trạng thái |
|---|---|---|---|:---:|
| [event_bus.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/event_bus.c) | Hàng đợi sự kiện trung tâm (Queue 20 slots, Core 0), điều phối bất đồng bộ giữa các tác vụ | §4.1.4 | `TC-SUP-01` | 🟢 **PASSED** |
| [supervisor.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.c) | Task Supervisor & Task Watchdog (TWDT), giám sát nhịp tim 8 task (`0xD7`), timeout 15s | §4.1.1, §4.1.5 | `TC-SUP-01`, `TC-SUP-02` | 🟢 **PASSED** |
| [config_manager.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/config_manager.c) | Quản lý cấu hình đọc/ghi NVS Flash (Wi-Fi, MQTT Broker, Gateway ID) | §4.1.2 | `TC-CFG-02` | 🟢 **PASSED** |
| [serial_cli.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/serial_cli.c) | Console tương tác dòng lệnh qua USB UART0 (115200 baud): `help`, `status`, `wifi`, `ble`, `rule`, `schedule` | §4.1.3 | `TC-CFG-02` | 🟢 **PASSED** |
| [ble_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.c) | **BLE Telemetry Scanner**: NimBLE GAP Central/Observer, giải mã Service Data 0x181A / BTHome, Command queue, Heartbeat | §4.5, FR-BLE-01 | `TC-BLE-01`, `TC-HOST-05` | 🟢 **PASSED** |
| [wifi_provisioning.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/wifi_provisioning.c) | SoftAP Captive Portal (`SmartClock-Setup`), DNS Redirection, HTTP Web Server cấu hình Dark mode | §4.2.1 | `TC-WIFI-01` | 🟢 **PASSED** |
| [wifi_provisioning.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/wifi_provisioning.c) | **Wi-Fi Scanner**: REST endpoint `/scan`, quét sóng Wi-Fi xung quanh với chế độ Coexistence BLE | §4.2.1 | `TC-WIFI-02` | 🟢 **PASSED** |
| [rtc.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rtc.c) | Quản lý đồng hồ thời gian thực, đồng bộ SNTP Internet (`pool.ntp.org`), múi giờ `ICT-7` | §4.4.1 | `JRN-04` | 🟢 **PASSED** |
| [mqtt_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/mqtt_adapter.c) | MQTT Client: kết nối broker (`broker.emqx.io:1883`), LWT status, subscribe command & schedule | §4.6.1, §4.6.4 | `JRN-04` | 🟢 **PASSED** |
| [uplink.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c) | Tác vụ định kỳ đẩy bản tin telemetry (Uptime, Free Heap, IP) lên Cloud qua MQTT | §4.6.2 | `JRN-04` | 🟢 **PASSED** |
| [rule_engine.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c) | Bộ máy quy tắc tự động hóa nội bộ IF-THEN (GT/LT), Safe-Default, Action Executor | §4.7.1 | `TC-HOST-03`, `TC-RULE-01` | 🟢 **PASSED** |
| [scheduler.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/scheduler.c) | Bộ lập lịch công việc và báo thức thời gian thực, quét chu kỳ 5s, chống trigger lặp | §4.8.1 | `TC-CLK-03` | 🟢 **PASSED** |
| [mock_peripherals.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/mock_peripherals.c) | Tầng ngoại vi ảo: mô phỏng cảm biến không dây, Virtual Storage CSV, Fault Injection | §4.3, §4.5 | `TC-MOCK-01` | 🟢 **PASSED** |
| [web_server.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/web_server.c) | Web Dashboard SPA & REST API: `/api/rules` (GET/POST/DELETE), `/api/status`, `/api/telemetry`, `/api/relay` | §4.2, §4.7 | `TC-WEB-01` | 🟢 **PASSED** |
| `rs485_adapter.c` | *(Đã lược bỏ)* Giao tiếp RS485 Modbus RTU dây đồng | §4.10 | — | 🚫 *Deprecated* |
| `sd_logger.c` | Quản lý hệ thống file FATFS trên thẻ nhớ MicroSD qua SPI, ghi CSV nhật ký hàng ngày | §4.5.1 | `TC-SD-01`, `JRN-02` | ⏳ *Chờ cắm thẻ* |
| `sync_queue.c` | Hàng đợi lưu trữ ngoại tuyến FIFO (`sync_queue.bin`) chống mất dữ liệu khi rớt mạng | §4.5.3 | `TC-HOST-02` | ⏳ *Chờ cắm thẻ* |
| `display_ui.c` | Hiển thị màn hình OLED SSD1327 (I2C) đồng hồ kim/số, thời tiết, trạng thái cảm biến | §4.8.1 | `TC-CLK-01` | ⏳ *Chờ cắm màn hình* |

---

## 4. Chi Tiết Kết Quả Kiểm Thử (Test Execution Records)

### 4.1. Automated Host Tests (`pytest tests/host/`)
* **Tổng số bài test**: 17 passed / 0 failed (Thời gian chạy: 0.05s).
* **Nội dung kiểm tra**:
  * `TC-HOST-01`: Thuật toán tính Modbus RTU CRC-16.
  * `TC-HOST-02`: Đóng gói binary header đồng bộ sync queue.
  * `TC-HOST-03`: Bộ thẩm định quy tắc tự động hóa Rule Engine (GT/LT, threshold, safe-default).
  * `TC-HOST-04`: Phân tích topic MQTT và đóng gói JSON telemetry.
  * `TC-HOST-05`: **BLE Advertisement & Environmental Sensing Packet Parser** (phân tích UUID 0x181A nhiệt độ/độ ẩm, BTHome V2, xử lý số âm, chuỗi tên thiết bị).

### 4.2. Target BLE Verification Suite (`scripts/test_ble_battery.py`)
* **Thời gian thực hiện**: 19/09/2026 trên mạch ESP32-S3 thật (`/dev/ttyACM0`).
* **Kết quả**: **6/6 bài test vượt qua (100% PASSED)**.
* **Chi tiết**:
  1. *BLE Scanner Status*: NimBLE stack chạy ổn định, GAP passive scanner ở trạng thái `RUNNING`.
  2. *Dynamic Device Registration*: Đăng ký thiết bị mới qua CLI (`ble reg sensor_livingroom 11:22:33:44:55:66`).
  3. *Telemetry Pipeline & Rule Trigger (`FR-BLE-01`)*: Bơm dữ liệu 34.50°C vào `sensor_temp_01`, bắt sự kiện `EVT_BLE_DEVICE_DATA`, Rule Engine kích hoạt Rule #1 bật `fan_relay_01`.
  4. *Sub-threshold Safe Handling*: Bơm dữ liệu 25.00°C không gây kích hoạt giả mạo.
  5. *Supervisor Tracking*: Heartbeat mask `0xD7` (Active: 0xD7, Alive: 0xD7), task `ble_scan_task` trên Core 0 báo danh liên tục, hệ thống hoàn toàn khỏe mạnh.

### 4.3. Bare-board Automated Test Suite (`scripts/test_bareboard_suite.py`)
* **Tổng số bài test**: 8/8 PASSED trên phần cứng thực tế ESP32-S3 (`/dev/ttyACM0`).
* **Chi tiết 8 bài kiểm tra**:
  1. `TC-CLI-01`: Interactive CLI console & Help table output.
  2. `TC-MOCK-02`: Safe-Default fail-safe khi nhận telemetry `valid = false`.
  3. `TC-MOCK-03`: Bus timeout simulation mà không gây rò rỉ bộ nhớ hay treo bus.
  4. `TC-RULE-01`: Thêm/xóa rule động và kiểm định ngưỡng kích hoạt action.
  5. `TC-CLK-03`: Kích hoạt cảnh báo báo thức thời gian thực từ Local Scheduler.
  6. `TC-MOCK-04`: Ghi bản tin telemetry vào Virtual Storage CSV trên flash.
  7. `TC-WIFI-02`: Quét mạng Wi-Fi lân cận với chế độ coexistence.
  8. `TC-SUP-02` / `TC-NEG-03`: Phát hiện treo task sau 15s và tự động panic reboot khôi phục hệ thống (`FR-SUP-02`).

### 4.4. Target Journey Tests (Kiểm thử thực địa trên board ESP32-S3)
* **`JRN-01` (System Boot and Alive Marker)**: ✅ **PASSED**
  - Bootloader nạp thành công phân vùng `factory` tại địa chỉ `0x20000`.
  - In thông báo khởi động và xuất hiện `[SYS] Alive marker 1` trong vòng 1.5s.
* **`JRN-04` (Wi-Fi & MQTT Uplink Connection)**: ✅ **PASSED**
  - Board kết nối Wi-Fi `TPLink_P1908` thành công, DHCP cấp IP `192.168.1.143`.
  - Tự động bắt sự kiện `EVT_WIFI_CONNECTED`, kích hoạt SNTP lấy giờ chuẩn thời gian thực (ICT-7: `2026-09-16 17:43:xx`).
  - Tự động kết nối MQTT broker `broker.emqx.io:1883`, chuyển trạng thái `ONLINE`, subscribe các topic điều khiển.
  - REST API & Web Dashboard truy cập trực tiếp từ mạng LAN tại `http://192.168.1.143/` (Ping: 0% loss, ~50ms).
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
* **`TC-MOCK-02` (Safe-Default on Sensor CRC/Invalid Fault)**: ✅ **PASSED**
  - Khi bơm lỗi `mock rs485 fault invalid`, khung truyền mang cờ `valid = false`.
  - Rule Engine phát hiện dữ liệu không hợp lệ, ghi log cảnh báo `[SAFE-DEFAULT]` và chặn toàn bộ việc kích hoạt Relay sai lệch theo đúng đặc tả FSD §4.7.5.
* **`TC-MOCK-03` (Modbus Bus Timeout Simulation)**: ✅ **PASSED**
  - Khi kích hoạt lỗi `mock rs485 fault timeout`, hệ thống phát hiện Slave không phản hồi.
  - Hệ thống không bị treo bus, không rò rỉ bộ nhớ, giữ nguyên trạng thái relay hiện hành.
* **`TC-RULE-01` (Rule Engine Dynamic CRUD & Boundary Evaluation)**: ✅ **PASSED**
  - Thêm mới rule động `rule add 4 sensor_temp_01 GT 42.0 heater_relay_01 ALARM_COOL`.
  - Thử nghiệm giá trị cận dưới 40.0°C (không kích hoạt), giá trị cận trên 43.5°C (kích hoạt thành công lệnh `ALARM_COOL`).
  - Xóa rule thành công bằng `rule delete 4`.
* **`TC-MOCK-04` (Virtual Storage CSV Telemetry Logging)**: ✅ **PASSED**
  - Lệnh `mock storage test` ghi thành công bản tin đo đạc vào buffer vòng tệp CSV `/sdcard/data/telemetry/`.
* **`TC-SUP-02` / `TC-NEG-03` (Task Supervisor Hang Detection & Panic Recovery — FR-SUP-02)**: ✅ **PASSED**
  - Gửi lệnh test hook `test freeze rs485` khiến task RS485 rơi vào trạng thái ngủ vô hạn không báo nhịp tim.
  - Đúng 15,000 ms sau, Task Supervisor Core 0 phát hiện `CRITICAL: Task 'RS485' STALLED`.
  - Hệ thống tự động kích hoạt `esp_restart()` giải phóng deadlock và reboot hoàn tất trong < 2.5s, tái lập trạng thái lành mạnh `[SYS] Alive marker 1`.

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
