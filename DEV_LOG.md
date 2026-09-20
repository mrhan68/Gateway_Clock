# Development & Testing Journal (DEV_LOG)

> **Project**: Smart Clock & Industrial Edge IoT Gateway (ESP32)  
> **Methodology**: AICLP (AI Closed-Loop Programming)  
> **Connected Port**: COM7 (`/dev/ttyS7`)  
> **Hardware Status**: Board trần (chưa hàn chân cắm, cắm cáp MicroUSB/Type-C)

---

## 📌 Phase Overview & Strategy

* **Chiến lược hiện tại**: Triển khai toàn bộ các module phần mềm nội bộ (on-chip core logic) trên board trần trước, chưa yêu cầu ngoại vi (RS485, SD Card, I2C Display) phải cắm dây.
* **Sprint 1 — Core Infrastructure**:
  1. `event_bus`: Hàng đợi sự kiện trung tâm và bộ điều phối dispatcher (FreeRTOS Queue).
  2. `supervisor`: Task Supervisor & Task Watchdog (TWDT) giám sát heartbeat toàn hệ thống.
  3. `config_manager`: Quản lý cấu hình hệ thống trên Flash NVS nội bộ.
  4. `serial_cli`: Trình thông dịch dòng lệnh tương tác trên cổng USB Console UART0 (115200 baud).

---

## 🕒 Nhật ký chi tiết (Chronological Log)

### [2026-09-13] Khởi động Sprint 1 — On-Board Core Infrastructure
* **Mục tiêu**: Lập trình bộ khung lõi chạy trực tiếp trên SoC ESP32 không cần ngoại vi.
* **Cổng kết nối**: Xác nhận COM7 tương ứng với `/dev/ttyS7` trên môi trường WSL/Linux, người dùng thuộc nhóm `dialout`.
* **Kế hoạch thực hiện**:
  * [x] Cập nhật FSD v1.1 với RS485, SD Card, Config Manager và Task Supervisor.
  * [x] Tạo `test-plan.yaml` và khai báo ma trận kiểm thử, journey tests.
  * [x] Viết unit tests trên Host cho Modbus CRC-16 và Sync Queue Header (`tests/host/`).
  * [x] Cập nhật FSD v1.1 với RS485, SD Card, Config Manager và Task Supervisor.
  * [x] Tạo `test-plan.yaml` và khai báo ma trận kiểm thử, journey tests.
  * [x] Viết unit tests trên Host cho Modbus CRC-16 và Sync Queue Header (`tests/host/`).
  * [x] Triển khai mã nguồn `event_bus.h` và `event_bus.c`.
  * [x] Triển khai mã nguồn `supervisor.h` và `supervisor.c`.
  * [x] Triển khai mã nguồn `config_manager.h` và `config_manager.c`.
  * [x] Triển khai mã nguồn `serial_cli.h` và `serial_cli.c`.
  * [x] Ghép nối vào `main.c` và cấu hình `firmware/main/CMakeLists.txt`.

### [2026-09-13] Kết quả triển khai Sprint 1
* **Các file đã hoàn thiện**:
  1. `firmware/main/event_bus.h` & `.c`: Triển khai Event Bus theo quy chuẩn FSD (Dispatcher owns `free()`, queue 20 slots, tối đa 4 handlers/code, Core 0 Prio 6).
  2. `firmware/main/supervisor.h` & `.c`: Triển khai Task Supervisor giám sát nhịp tim Heartbeat bitmask (timeout 15s), tự động in định kỳ log sức khỏe và khởi động lại nếu có task treo.
  3. `firmware/main/config_manager.h` & `.c`: Quản lý đọc/ghi cấu hình hệ thống (Wi-Fi, MQTT, RS485) an toàn trên NVS Flash.
  4. `firmware/main/serial_cli.h` & `.c`: Giao diện dòng lệnh tương tác trên cổng USB UART0 (115200 baud) hỗ trợ `help`, `status`, `config show`, `config set`, `modbus scan`, `reboot`.
  5. `firmware/main/main.c`: Khởi động tuần tự theo thứ tự bắt buộc của FSD (NVS -> Event Bus -> Supervisor -> Serial CLI -> Dispatcher).
* **Trạng thái**: Đã biên dịch thành công firmware với ESP-IDF v5.3.1 cục bộ.

### [2026-09-13] Hardware Bringup & Xác minh Kiểm thử Trực tiếp thành công
* **Phần cứng thực tế**:
  - Chip: **ESP32-S3** (QFN56 revision v0.2)
  - Bộ nhớ Flash: 16MB SPI Flash (XMC/Boya 0x684018, DIO 40MHz)
  - Bộ nhớ PSRAM: 8MB Embedded PSRAM (AP_3v3)
  - Cổng kết nối: CH343 USB Single Serial (VID 1a86, PID 55d3) kết nối UART0, thông qua USB/IP (`/dev/ttyACM0`).
* **Nạp Firmware**: Tự động biên dịch `esp32s3` và nạp trực tiếp qua `esptool.py` ở tốc độ 460800 baud thành công 100%.
* **Kết quả Kiểm thử Trực tiếp**:
  1. `JRN-01` (**System Boot & Alive Marker**): ✅ **PASSED**.
     - Bootloader v5.3.1 nạp thành công phân vùng `factory` tại `0x20000`.
     - In chuẩn xác `[BOOT] Starting Smart Clock Edge Gateway`.
     - Xuất hiện `[SYS] Alive marker 1` và chuỗi alive marker định kỳ.
  2. `TC-SUP-01` (**All Tasks Heartbeat Bitmask Tracking**): ✅ **PASSED**.
     - Mask giám sát ban đầu: `0x41` (`EVENT_BUS` bit 0 + `CLI` bit 6).
     - Supervisor báo cáo trạng thái `HEALTHY`, uptime tăng đều, không có task bị timeout.
  3. `TC-CFG-02` (**Serial CLI Query**): ✅ **PASSED**.
     - Phản hồi lệnh `status`: Uptime, Free Heap ~372KB, Active & Alive Mask `0x41`.
     - Phản hồi lệnh `config show`: Hiển thị đầy đủ Gateway ID, MQTT URI, RS485 params.
* **Trạng thái**: Sprint 1 Bare-board Core Infrastructure đã hoàn thành xuất sắc và đóng vòng lặp kiểm thử (Green).

---

### [2026-09-13] Triển khai & Xác minh Network Stack (Wi-Fi Station, Captive Portal, SNTP RTC, MQTT Uplink)
* **Mục tiêu**: Kích hoạt toàn bộ khả năng kết nối mạng và Cloud Uplink trên board trần mà chưa cần ngoại vi.
* **Các module hoàn thiện & tối ưu**:
  1. `firmware/main/wifi_provisioning.h` & `.c`:
     - Chế độ SoftAP phát mạng `SmartClock_Setup` (IP `192.168.4.1`).
     - DNS Server chuyển hướng mọi truy vấn (Captive Portal Hijacking).
     - HTTP Web Server phục vụ giao diện cấu hình tối ưu di động (Dark mode).
     - **Wi-Fi Scanner**: REST endpoint `/scan` quét mạng xung quanh và tự động đổ dữ liệu vào danh sách thả xuống (`<select>`), kèm tùy chọn nhập mạng ẩn.
  2. `firmware/main/rtc.h` & `.c`:
     - Quản lý đồng hồ hệ thống theo múi giờ `ICT-7` (`Asia/Ho_Chi_Minh`).
     - Tự động bắt sự kiện `EVT_WIFI_CONNECTED` từ Event Bus để đồng bộ giờ chuẩn Internet qua SNTP (`pool.ntp.org`, `time.google.com`).
     - Cơ chế fallback thông minh: Khi chưa có module ngoại vi I2C DS3231, hệ thống giữ giờ bằng timer nội bộ của ESP32.
  3. `firmware/main/mqtt_adapter.h` & `.c` & `uplink.c`:
     - Tự động kết nối MQTT broker khi có Wi-Fi và ngắt khi mất kết nối.
     - Đăng ký nhận lệnh từ Cloud: `gateway/<ID>/device/+/command` và `gateway/<ID>/config/schedule`.
     - Tự động gửi nhịp tim LWT `gateway/<ID>/status` (`online` / `offline`).
     - Định kỳ đẩy bản tin telemetry (Uptime, Free Heap, IP) lên Cloud.
* **Các sự cố kỹ thuật đã phân tích & xử lý triệt để**:
  1. **Lỗi HTTP 431 (Request Header Fields Too Large)**:
     - *Nguyên nhân*: Trình duyệt di động gửi User-Agent và Cookie vượt quá buffer 512 bytes mặc định của `esp_http_server`.
     - *Khắc phục*: Bổ sung `CONFIG_HTTPD_MAX_REQ_HDR_LEN=2048` và `CONFIG_HTTPD_MAX_URI_LEN=1024` trong `sdkconfig.defaults`, nâng stack HTTP server lên 8192.
  2. **Lỗi Tràn Stack `task sys_evt` (Guru Meditation Error)**:
     - *Nguyên nhân*: Tác vụ điều phối sự kiện hệ thống `sys_evt` mặc định chỉ có 2304 bytes. Chuỗi xử lý ngắt khi nhận IP kèm `printf` và FreeRTOS queue dispatch vượt quá 3KB. Ngoài ra, việc dừng HTTP server trực tiếp trong callback gây nghẽn task.
     - *Khắc phục*: Tăng `CONFIG_ESP_SYSTEM_EVENT_TASK_STACK_SIZE=4096` trong `sdkconfig.defaults`, chuyển việc dọn dẹp Web Server sang task bất đồng bộ riêng biệt, và loại bỏ delay trong callback ngắt.
* **Kết quả Kiểm thử Trực tiếp trên Phần cứng (`/dev/ttyACM0`)**:
  1. `TC-WIFI-01` (**SoftAP Captive Portal Bringup**): ✅ **PASSED**.
     - Board phát SoftAP `SmartClock_Setup`, cấp IP `192.168.4.1`, chuyển hướng HTTP tự động.
  2. `TC-WIFI-02` (**Nearby Wi-Fi Scan & Dynamic Dropdown**): ✅ **PASSED**.
     - Quét tìm thấy mạng lân cận, người dùng chọn mạng `TPLink_P1908`, nhập mật khẩu và lưu thành công vào NVS.
  3. `JRN-04` (**Wi-Fi & MQTT Uplink Connection**): ✅ **PASSED**.
     - Trạng thái Wi-Fi: `CONNECTED` (IP: `192.168.1.142`).
     - SNTP RTC: Đồng bộ thành công thời gian thực (`2026-09-13 16:10:01`).
     - MQTT Uplink: `ONLINE` (Kết nối broker công cộng `mqtt://broker.emqx.io:1883`, subscribe thành công các topic điều khiển).
     - Supervisor Heartbeat Bitmask: `0x45` (tất cả các task `EVENT_BUS`, `CLI`, `UPLINK` đều `HEALTHY`).
     - Bộ nhớ khả dụng: Free Heap đạt ~244KB (cực kỳ dồi dào cho các tác vụ tiếp theo).
* **Trạng thái hiện tại**: Hoàn thành toàn bộ mục tiêu Wave 1 ở chế độ Bare-Board. Đã sẵn sàng kết nối ngoại vi phần cứng hoặc lập trình tiếp các tính năng logic cao cấp (Rule Engine, Local Scheduler, Offline FIFO Queue).

---

### [2026-09-13] Chuyển đổi Cơ chế Wi-Fi sang Volatile RAM Mode (Mất khi Reset)
* **Mục tiêu**: Cấu hình Wi-Fi chỉ được lưu tạm thời trong RAM cho phiên chạy hiện tại, tự động biến mất khi board bị reset (bấm nút RESET, ngắt nguồn, hoặc lệnh `reboot`) thay vì lưu bền vững vào Flash NVS.
* **Các thay đổi mã nguồn**:
  1. `firmware/main/config_manager.c`:
     - Xóa vĩnh viễn các khóa `wifi_ssid` và `wifi_pass` trong NVS Flash.
     - `config_manager_set_wifi()` chỉ cập nhật RAM `s_active_config.wifi_ssid`.
     - `config_manager_save()` không ghi SSID/mật khẩu Wi-Fi vào NVS.
  2. `firmware/main/wifi_provisioning.c`:
     - Tắt cơ chế NVS ngầm của ESP-IDF Wi-Fi driver (`cfg.nvs_enable = false;`).
     - Gọi `esp_wifi_restore()` khi khởi động để dọn dẹp sạch mọi cấu hình cũ mà driver đã lưu trước đây.
     - Trong `wifi_event_handler`, chỉ tự động kết nối khi cấu hình Wi-Fi trong RAM hợp lệ (`wifi_provisioning_is_configured()`).
* **Kết quả Kiểm thử Trực tiếp (`/dev/ttyACM0`)**:
  - Khi khởi động lại board:
    - Log khởi động: `[WIFI_PROV] No Wi-Fi credentials (Volatile RAM mode). Starting SoftAP 'SmartClock-Setup' (IP: 192.168.4.1)...`
    - Lệnh CLI `wifi status`: `Wi-Fi Status: DISCONNECTED / SOFTAP (IP: 0.0.0.0)`
    - Lệnh CLI `config show`: `Wi-Fi SSID: (not configured)`, `Wi-Fi Password: (none)`
  - Mỗi lần reset, board luôn sẵn sàng phát SoftAP `SmartClock-Setup` để người dùng kết nối cấu hình lại từ đầu.

---

### [2026-09-16] Triển khai & Xác minh Phần mềm Hướng 1: Rule Engine & Local Scheduler trên Bare-Board
* **Mục tiêu**: Hoàn thiện toàn bộ logic xử lý tự động hóa cục bộ (Offline-First) và bộ lập lịch báo thức thời gian thực trên board trần ESP32-S3. Nạp và kiểm thử tự động qua cổng `/dev/ttyACM0` (COM7 đính kèm qua USB/IP).
* **Kết nối phần cứng qua USB/IP**:
  - Tự động gắn thiết bị CH343 USB-to-UART Bridge (`busid 1-1`) từ Windows Host vào WSL qua `usbipd.exe attach --wsl --busid 1-1`.
  - Nhận diện thiết bị CDC ACM tại `/dev/ttyACM0`, tốc độ giao tiếp 115200 baud, nạp firmware 460800 baud.
* **Các module triển khai**:
  1. `firmware/main/rule_engine.h` & `.c` (FSD §4.7):
     - Khung đánh giá điều kiện toán học: `RULE_CONDITION_SENSOR_GT` (>), `RULE_CONDITION_SENSOR_LT` (<).
     - Cơ chế Safe-Default: Khi bản tin telemetry mang cờ `valid=false` (sensor lỗi/offline), Rule Engine lập tức ngắt đánh giá, giữ nguyên trạng thái an toàn và phát `EVT_SENSOR_VALUE_INVALID`.
     - Điều phối hành động: Khi khớp điều kiện, sinh payload `rule_action_request_t` đẩy lên Event Bus qua `EVT_RULE_ACTION_REQUEST`.
     - Tích hợp MQTT: Tự động publish bản tin thông báo hành động tới topic `gateway/<ID>/device/<target_id>/action`.
  2. `firmware/main/scheduler.h` & `.c` (FSD §4.8):
     - Tác vụ `scheduler_task` (Core 0, Priority 5, Stack 4096).
     - Chu kỳ quét 5 giây, cơ chế phát hiện chuyển phút kèm chốt chống kích hoạt lặp `s_last_triggered_minute`.
     - Hỗ trợ lịch tuần lặp lại (`repeat_days[]`) và sự kiện một lần (`one_time_date`).
     - Tích hợp nhịp tim giám sát `TASK_HEARTBEAT_SCHEDULER` vào Task Supervisor (Bitmask `0x55`).
     - Kích hoạt sự kiện `EVT_SCHEDULE_AUTOMATION_TRIGGERED` và `EVT_SCHEDULE_ALARM_TRIGGERED`.
  3. Mở rộng `serial_cli.h` & `.c`:
     - Bổ sung nhóm lệnh tương tác: `rule list`, `rule add`, `rule test`, `rule clear`.
     - Bổ sung nhóm lệnh lập lịch: `schedule list`, `schedule add`, `schedule test`, `schedule clear`.
     - Bổ sung lệnh dispatch thiết bị: `cmd send <target> <command>`.
* **Sự cố kỹ thuật phát hiện qua kiểm thử trực tiếp & giải pháp**:
  - *Sự cố*: Tràn ngăn xếp `task serial_cli_task` (`***ERROR*** A stack overflow in task serial_cli_task has been detected`).
  - *Nguyên nhân*: Khai báo cục bộ `rules[16]` và `entries[16]` (>4.5KB) trên stack 4KB của `serial_cli_task`.
  - *Giải pháp*: Chuyển vùng đệm CLI sang `static s_cli_rules` và `static s_cli_entries`, đồng thời nâng stack size lên 8192 bytes; nâng `event_dispatcher` lên 6144 bytes.
* **Kết quả Kiểm thử Trực tiếp trên Phần cứng (`/dev/ttyACM0`)**:
  - `status`: Active Mask = `0x55`, Alive Mask = `0x55` (`EVENT_BUS`, `CLI`, `UPLINK`, `SCHEDULER` đều `HEALTHY`), Free Heap ~211KB.
  - `rule list`: Hiển thị 3 luật mặc định (`sensor_temp_01 > 30.0` -> `fan_relay_01 ON`, v.v.).
  - `rule test sensor_temp_01 32.5 1`: Khớp Rule #1, Action Executor thực thi lệnh bật quạt `ON` thành công.
  - `rule test sensor_temp_01 35.0 0`: Kích hoạt Safe-Default chính xác, bỏ qua đánh giá luật.
  - `schedule list`: Quản lý 2 lịch biểu hoạt động.
  - `schedule test Siren Check`: Kích hoạt banner cảnh báo báo thức thời gian thực `🔔 [ALARM NOTIFICATION TRIGGERED] 🔔`.
  - `cmd send relay_ch3 TOGGLE`: Dispatcher xử lý điều khiển thiết bị đích thành công.
* **Trạng thái**: Toàn bộ logic phần mềm nội bộ trên Bare-board đã hoàn thành 100% và vượt qua kiểm thử thực tế.

---

### [2026-09-16] Triển khai Web Dashboard SPA & REST API trên ESP32-S3 (`web_server.c`)
* **Mục tiêu**: Tích hợp máy chủ HTTP phục vụ giao diện Web Dashboard nội bộ và REST API hoàn chỉnh, truy cập trực tiếp từ mạng LAN qua IP của ESP32 (`http://<device-ip>/`).
* **Các tính năng triển khai**:
  1. `web_server.h` & `web_server.c` (FSD §4.2, §4.7):
     - Giao diện Single Page Application (SPA) chủ đề Dark Mode tinh gọn, tích hợp sẵn trong flash firmware.
     - Đồng hồ thời gian thực RTC, hiển thị thông số nhiệt độ, độ ẩm môi trường, thời gian hoạt động uptime.
     - 4 công tắc điều khiển trạng thái 4 relay (`fan_relay_01`, `heater_relay_01`, `dehumidifier_01`, `aux_relay_04`).
     - REST API endpoints:
       - `GET /api/status`: Lấy thông tin hệ thống (uptime, heap, trạng thái Wi-Fi/MQTT).
       - `GET /api/telemetry`: Lấy giá trị đo đạc mới nhất của cảm biến.
       - `POST /api/relay`: Bật/tắt relay theo kênh (`{"relay_id": 1, "state": 1}`).
       - `GET /api/rules`, `POST /api/rules`, `DELETE /api/rules`: CRUD các quy tắc tự động hóa IF-THEN.
       - `GET /api/wifi/scan`: Kích hoạt quét sóng Wi-Fi xung quanh và trả về JSON danh sách AP.
* **Kết quả Kiểm thử Trực tiếp**:
  - `TC-WEB-01`: Đạt 100% — Truy cập web dashboard từ trình duyệt LAN tại `http://192.168.1.143/`, thao tác bật/tắt relay phản hồi tức thì (<100ms), API REST trả về JSON chuẩn xác.

---

### [2026-09-17] Triển khai Tầng Ngoại Vi Ảo & Bơm Lỗi Kiểm Thử (`mock_peripherals.c`)
* **Mục tiêu**: Xây dựng hệ thống giả lập ngoại vi hoàn chỉnh chạy ngầm trên chip để phục vụ kiểm thử hồi quy (regression) và kiểm thử nhánh phủ định (Wave 3 Negative tests) mà không phụ thuộc vào dây nối phần cứng.
* **Các tính năng triển khai**:
  1. `mock_peripherals.h` & `mock_peripherals.c` (FSD §4.3, §4.5):
     - Tác vụ `mock_rs485_task` phát dữ liệu telemetry định kỳ theo chu kỳ sóng hình sin.
     - Bộ tạo lỗi (Fault Injection): Lỗi mất kết nối (timeout), lỗi dữ liệu sai lệch (invalid checksum/data).
     - Giả lập bộ nhớ Virtual Storage CSV: Ghi dữ liệu cảm biến vào phân vùng flash và đọc lại kiểm tra tính toàn vẹn.
  2. Tài liệu cấu hình: Ban hành tài liệu [`MOCK_PERIPHERALS.md`](file:///home/chien/ai-workspace/qwen-test/MOCK_PERIPHERALS.md).
* **Kết quả Kiểm thử Trực tiếp**:
  - `TC-MOCK-02`: Bơm lỗi `mock rs485 fault invalid` kích hoạt thành công cờ `valid = false`, Rule Engine lập tức bật chế độ Safe-Default, bảo vệ hệ thống không kích hoạt sai relay.
  - `TC-MOCK-03`: Bơm lỗi `mock rs485 fault timeout` mô phỏng mất kết nối thiết bị ngoại vi, hệ thống không bị crash hoặc treo bus.
  - `TC-MOCK-04`: Lệnh `mock storage test` ghi thành công bản tin CSV vào bộ nhớ ảo.

---

### [2026-09-18] Định Hướng Không Dây: Chuyển Đổi Sang BLE Environmental Sensing (§4.10)
* **Mục tiêu**: Theo đặc tả FSD v1.3 §4.10, chính thức lược bỏ kết nối Modbus RS485 có dây phức tạp cho kịch bản Smart Home và chuyển sang thu thập dữ liệu không dây qua Bluetooth Low Energy (BLE).
* **Kiến trúc BLE tích hợp**:
  - Sử dụng stack mã nguồn mở **Apache NimBLE** tích hợp trong ESP-IDF v5.3.1 (tiết kiệm hơn 100KB DRAM so với Bluedroid).
  - Khởi tạo chế độ GAP Central / Passive Observer: Lắng nghe liên tục các gói tin quảng bá (Advertisement / Scan Response) của các cảm biến môi trường lân cận.
  - Hỗ trợ chuẩn GATT Environmental Sensing (Service UUID `0x181A` - Temperature & Humidity) và định dạng BTHome V2 phổ biến.

---

### [2026-09-19 - 2026-09-20] Triển khai BLE Adapter, Phát Hiện Treo Task & Đóng Vòng Lặp Wave 1
* **Mục tiêu**: Hoàn thiện module BLE, triển khai cơ chế phát hiện treo task theo FR-SUP-02, viết các bộ kiểm thử tự động, và hoàn thành 100% mục tiêu Wave 1.
* **Các module triển khai & kiểm thử**:
  1. `firmware/main/ble_adapter.h` & `ble_adapter.c` (FSD §4.5, FR-BLE-01):
     - Tác vụ `ble_scan_task` trên Core 0, stack 4096 bytes, đăng ký nhịp tim `TASK_HEARTBEAT_BLE` vào Task Supervisor.
     - Bảng đăng ký thiết bị động `ble_device_map_t` hỗ trợ gán ID logic (`sensor_temp_01`) với địa chỉ MAC thực tế.
     - Hàm test hook `ble_adapter_inject_test_packet()` phục vụ kiểm thử telemetry.
     - Bổ sung các lệnh CLI: `ble status`, `ble start`, `ble stop`, `ble reg <id> [mac]`, `ble test [val]`.
  2. Bộ giám sát treo tác vụ `supervisor.c` (FSD §4.13.4, FR-SUP-02):
     - Nâng tổng số task giám sát lên 8 (`NUM_TRACKED_TASKS 8`), mask đầy đủ `0xD7`.
     - Bộ đếm thời gian kiểm tra mỗi 3 giây. Nếu bất kỳ task nào không cập nhật heartbeat quá 15 giây (15,000 ms), supervisor in log `CRITICAL: Task 'X' STALLED` và gọi `esp_restart()` để khôi phục hệ thống tự động.
     - Bổ sung lệnh test hook: `test freeze rs485` và `test freeze event_bus`.
  3. Mở rộng bộ kiểm thử tự động:
     - `tests/host/test_ble_parser.py`: Kiểm thử phân tích gói tin BLE (UUID 0x181A, BTHome V2, xử lý số âm, tên thiết bị). Nâng tổng số host tests lên **17 passed / 0 failed**.
     - `scripts/test_bareboard_suite.py`: Kịch bản kiểm thử tự động 8-in-1 trên phần cứng thực tế qua cổng `/dev/ttyACM0`. Đạt **8/8 PASSED**.
     - `scripts/test_ble_battery.py`: Kịch bản kiểm thử toàn diện phân hệ BLE 6-in-1 trên phần cứng thực tế qua cổng `/dev/ttyACM0`. Đạt **6/6 PASSED**.
* **Kết quả Kiểm thử Trực tiếp trên Phần cứng (`/dev/ttyACM0`)**:
  - `TC-SUP-02` / `TC-NEG-03`: Kích hoạt `test freeze rs485`, sau đúng 15s Supervisor phát hiện task bị treo, tự động kích hoạt reboot giải phóng deadlock và khôi phục trạng thái hoạt động bình thường trong < 2.5s.
  - Phân hệ BLE hoạt động ổn định song song với Wi-Fi (Wi-Fi + BLE Coexistence kích hoạt qua `CONFIG_SW_COEXIST_ENABLE=y`).
  - Heap DRAM khả dụng: ~116KB, hệ thống vận hành trơn tru và bền bỉ.
* **Trạng thái hiện tại**: Đóng vòng lặp Wave 1 (100% Green). Tất cả các hợp đồng xác minh và bộ kiểm thử đã vượt qua kiểm định thành công.
