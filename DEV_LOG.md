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



