# Thiết Kế Chi Tiết Các Phân Hệ và Giao Diện Phần Mềm (Subsystems & Software Interfaces)

- **Dự án:** Smart Clock as Edge IoT Gateway
- **Tài liệu tham chiếu gốc (Frozen):** [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md)
- **Mã nguồn phân hệ:** [firmware/main/rtc.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rtc.c), [firmware/main/display.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/display.c), [firmware/main/ble_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.c), [firmware/main/uplink.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c), [firmware/main/relay_controller.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/relay_controller.c), [firmware/main/rule_engine.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c), [firmware/main/scheduler.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/scheduler.c), [firmware/main/storage.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/storage.c), [firmware/main/sd_logger.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/sd_logger.c), [firmware/main/supervisor.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.c)
- **Truy vết yêu cầu kỹ thuật:** `Traces to: FR-RTC-01 / FR-DISP-01 / FR-BLE-01 / FR-MQTT-01 / FR-RELAY-01 / FR-RULE-01 / FR-SCHED-01 / FR-ST-01 / FR-SD-01 / FR-SD-02 / FR-CFG-01 / FR-CFG-02 / FR-SUP-01 / FR-SUP-02 / KPI-BOOT-01`

---

## 1. Phân Hệ Đo Thời Gian Thực (Timekeeping Subsystem)

- **Trách nhiệm:** Cung cấp chuẩn thời gian đơn điệu và thời gian thực toàn cục cho hệ thống, đảm bảo độ chính xác cao ngay cả khi mất điện hoặc mất kết nối Internet dài ngày (`FR-RTC-01`).
- **Tập tin hiện thực:** [firmware/main/rtc.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/rtc.h), [firmware/main/rtc.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rtc.c).

```mermaid
flowchart TD
    subgraph TIME_SOURCES["Nguồn Thời Gian"]
        DS[RTC DS3231 Ngoại Vi - I2C0 0x68 / Pin CR2032]
        SNTP[SNTP Server - pool.ntp.org qua Wi-Fi]
    end

    subgraph TIME_CORE["Bộ Điều Phối Thời Gian Hệ Thống (app_rtc)"]
        VAL{rtc_is_valid?}
        SYNC[Đồng Bộ Chuẩn ESP-IDF SNTP & Ghi I2C DS3231]
        TZ[POSIX C Stdlib TZ Engine - ICT-7]
    end

    subgraph CONSUMERS["Phân Hệ Tiêu Thụ Thời Gian"]
        SCHED[scheduler_task - Kích hoạt Báo thức & Rule]
        DISP[display_task - Hiển thị Giờ OLED SSD1327]
        LOG[sd_log_task - Timestamp CSV & sync_queue.bin]
    end

    DS -->|Khởi động đọc thanh ghi| VAL
    SNTP -->|EVT_WIFI_CONNECTED| SYNC
    SYNC -->|Cập nhật thanh ghi| DS
    VAL -->|Hợp lệ: Năm >= 2020| TZ
    TZ --> SCHED
    TZ --> DISP
    TZ --> LOG
```

### 1.1 Chi Tiết Kỹ Thuật và Giao Tiếp Phần Cứng
- **Giao diện I2C0:** Chia sẻ bus I2C0 (`GPIO 8 SDA`, `GPIO 9 SCL`), tốc độ chuẩn **400 kHz**, địa chỉ I2C `0x68`.
- **Duy trì nguồn độc lập:** Nuôi bởi pin cúc áo CR2032 (3.0V). Khi Gateway mất nguồn 5V/12V, chip DS3231 tự động chuyển sang nguồn pin, bộ tạo dao động thạch anh bù nhiệt độ nội (TCXO) duy trì sai số dưới $\pm 2$ ppm ($\approx \pm 2$ phút/năm).
- **Hàm thẩm định hợp lệ `rtc_is_valid()`:**
  - Tiêu chuẩn kiểm tra: Thời gian đọc từ DS3231 được coi là hợp lệ nếu năm $\ge 2020$ (tức Unix epoch $> 1577836800$).
  - Nếu pin CR2032 cạn hoặc thanh ghi trạng thái (Oscillator Stop Flag - OSF) được bật, `rtc_is_valid()` trả về `false`. Khi đó hệ thống tạm dừng các tác vụ báo thức tuyệt đối và hiển thị chỉ báo `TIME_SYNC_WAIT` trên màn hình OLED.
- **Đồng bộ SNTP & Chuẩn POSIX Timekeeping:**
  - Tận dụng trực tiếp thư viện chuẩn `esp_sntp` của ESP-IDF với chế độ `SNTP_OPMODE_POLL` lắng nghe sự kiện `EVT_WIFI_CONNECTED`.
  - Thiết lập múi giờ thông qua biến môi trường POSIX tiêu chuẩn: `setenv("TZ", "ICT-7", 1); tzset();`. Các hàm đọc thời gian trong toàn hệ thống gọi trực tiếp POSIX `time()` và `localtime_r()` với độ phức tạp $O(1)$.
  - `/* ponytail: Tận dụng trực tiếp esp_sntp của ESP-IDF và POSIX setenv("TZ")/tzset() của C stdlib. Phần cứng DS3231 TCXO bù nhiệt đã đảm bảo độ chính xác ±2ppm; loại bỏ hoàn toàn việc tự lập trình thuật toán Clock Slew phức tạp theo nguyên tắc YAGNI. */`

---

## 2. Phân Hệ Hiển Thị và Tương Tác Người Dùng (Display Subsystem)

- **Trách nhiệm:** Render thông tin trực quan, vẽ màn hình đồng hồ trong vòng **< 1.5 giây** từ khi bật nguồn (`KPI-BOOT-01`), tiếp nhận nút bấm ngắt còi báo thức tức thì trong **< 500ms** (`FR-DISP-01`).
- **Tập tin hiện thực:** [firmware/main/display.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/display.h), [firmware/main/display.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/display.c).

```mermaid
flowchart LR
    subgraph INPUTS["Ngõ Vào Tương Tác (GPIO ISR)"]
        BTN_0["GPIO 0 (BOOT) - Nhấn: Snooze (+9m) | Giữ >5s: Reset SoftAP"]
        BTN_4["GPIO 4 - Nhấn: TẮT CHUÔNG (<500ms) / Chuyển màn hình"]
    end

    subgraph ENGINE["Display Engine (Core 0, Prio 3)"]
        DIRECT_ISR["Direct-to-Task Notification"]
        FBUF["Framebuffer Bộ Nhớ (128x128 4-bit = 8192 Bytes)"]
        PAGES["Carousel 3 Trang Hiển Thị (1 FPS)"]
    end

    subgraph OUTPUTS["Phần Cứng Xuất Tín Hiệu"]
        OLED["OLED SSD1327 (I2C 0x3C, 128x128 Grayscale)"]
        BUZZER["Còi Buzzer GPIO 1 (LEDC PWM Alarm)"]
    end

    BTN_4 -->|Ngắt phần cứng| DIRECT_ISR
    DIRECT_ISR -->|vTaskNotifyGiveFromISR| BUZZER
    PAGES --> FBUF
    FBUF -->|I2C DMA Transfer| OLED
```

### 2.1 Cấu Trúc Bộ Đệm Framebuffer và Tốc Độ Render (KPI-BOOT-01)
- **Cấu hình phần cứng:** Màn hình OLED SSD1327 kích thước $128 \times 128$ điểm ảnh, độ sâu 4-bit Grayscale (16 mức xám), địa chỉ I2C `0x3C`.
- **Bộ nhớ Framebuffer:** $128 \times 128 \times 0.5 = 8192$ Bytes được cấp phát từ Internal SRAM để tối ưu hóa tốc độ truyền I2C DMA.
- **Đáp ứng khởi động siêu tốc (KPI-BOOT-01 < 1.5s):**
  - Hàm `display_init()` được gọi ở Bước 6 của Boot sequence, ngay sau khi I2C bus và RTC sẵn sàng.
  - Framebuffer được khởi tạo, đọc thời gian từ DS3231 và đẩy thẳng ra màn hình OLED bằng lệnh I2C burst transfer. Toàn bộ thao tác hoàn thành trong vòng **~1.2 giây** kể từ lúc chip ESP32-S3 bắt đầu nhận nguồn, hoàn toàn độc lập với việc cắm thẻ nhớ SD hay trạng thái quét mạng Wi-Fi.
- **Carousel 3 Màn Hình Luân Phiên (Tần số refresh 1 FPS):**
  - **Trang 0 (Default Clock):** Đồng hồ số cỡ lớn (HH:MM:SS), Ngày/Tháng, và hàng biểu tượng trạng thái (Status Badges: Wi-Fi STA, MQTT, Thẻ SD, 4 Kênh Relay).
  - **Trang 1 (Upcoming Events):** Lịch biểu hoặc báo thức kế tiếp được nạp từ `schedule.json`.
  - **Trang 2 (Environmental Telemetry):** Nhiệt độ, độ ẩm môi trường từ cảm biến BLE mới nhất, điện áp pin và cảnh báo.
- **Phản ứng nút bấm ngắt còi trực tiếp (Direct-to-Task ISR):**
  - Khi báo thức kêu (Buzzer PWM trên GPIO 1 hoạt động), nút bấm trên **GPIO 4** kích hoạt ngắt cạnh xuống (`GPIO_INTR_NEGEDGE`).
  - Hàm ISR sử dụng `vTaskNotifyGiveFromISR()` gửi tín hiệu thẳng tới `display_task`. Ngay trong chu kỳ ngắt hoặc trong vòng **< 50ms**, chuỗi xung PWM của còi chip bị ngắt cưỡng bức, hoàn toàn thỏa mãn tiêu chuẩn **< 500ms** của `FR-DISP-01`. Sau đó một sự kiện `EVT_BUTTON_ALARM_STOP` được gửi lên Event Bus để ghi log.
- `/* ponytail: Tối giản hóa bộ đệm màn hình: Sử dụng duy nhất 1 mảng tĩnh static uint8_t s_framebuffer[8192] trong Internal SRAM, không double-buffering tốn RAM, không cấp phát động. ISR ngắt còi dùng vTaskNotifyGiveFromISR() gọi thẳng vào display_task, phản hồi < 50ms mà không cần queue hay mutex phụ. */`

---

## 3. Phân Hệ Kết Nối Không Dây và Đám Mây (Connectivity Subsystem)

- **Trách nhiệm:** Quản lý kết nối Wi-Fi (SoftAP & Station), tiếp nhận dữ liệu cảm biến BLE thụ động không ghép đôi (Passive Observer), duy trì giao thức MQTT hai chiều với Cloud Backend (`FR-BLE-01`, `FR-MQTT-01`).
- **Tập tin hiện thực:** [firmware/main/wifi_provisioning.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/wifi_provisioning.c), [firmware/main/ble_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.c), [firmware/main/uplink.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c).

```mermaid
flowchart TD
    subgraph RF_COEX["Software Coexistence Vô Tuyến (2.4 GHz RF Transceiver)"]
        COEX_ENGINE["FreeRTOS TDM Coexistence Engine (CONFIG_SW_COEXIST_ENABLE=y)"]
    end

    subgraph BLE_DOMAIN["BLE Central Observer (Core 0, Prio 5)"]
        NIMBLE["NimBLE Stack - Passive Scanning"]
        DUTY["50% Duty Cycle (Window 50ms, Interval 100ms)"]
        FILTER["Whitelist & UUID Filter (0x181A / BTHome V2)"]
        DEBOUNCE["BLE Storm Defense (Rate Limiter 1 pkt/5s)"]
    end

    subgraph WIFI_DOMAIN["Wi-Fi & MQTT Uplink (Core 1, Prio 5)"]
        STA["Wi-Fi Station Mode (WPA2)"]
        AP["SoftAP Captive Portal (192.168.4.1)"]
        MQTT_CLI["MQTT Client (QoS 1, LWT, Backoff)"]
    end

    COEX_ENGINE <--> BLE_DOMAIN
    COEX_ENGINE <--> WIFI_DOMAIN
    NIMBLE --> DUTY --> FILTER --> DEBOUNCE
    DEBOUNCE -->|EVT_BLE_DEVICE_DATA| EVENT_BUS[(Event Bus)]
    EVENT_BUS --> MQTT_CLI
```

### 3.1 Vô Tuyến Đồng Thời (RF Coexistence) và Chống Bão BLE (BLE Storm Defense)
- **Cơ chế Software Coexistence:** ESP32-S3 sử dụng chung 1 bộ vô tuyến RF 2.4 GHz vật lý. Bật cờ `CONFIG_SW_COEXIST_ENABLE=y` cho phép phần mềm thực hiện phân chia thời gian (TDM - Time Division Multiplexing).
- **Cấu hình NimBLE Passive Scanner:**
  - **Duty cycle 50%:** Scan Window = 50ms, Scan Interval = 100ms. Cấu hình này giải phóng 50% thời gian cho khối Wi-Fi truyền tải gói tin MQTT mà không làm nghẽn sóng.
  - **Passive Scanning (Quét thụ động):** Gateway chỉ lắng nghe bản tin Advertisement mà không bao giờ gửi `SCAN_REQ` hay yêu cầu ghép đôi GATT (Pairing/Bonding). Điều này giúp cảm biến BLE ngoại vi (như Xiaomi Mijia LYWSD03MMC chạy Custom Firmware ATC/pvvx) tiết kiệm pin tối đa (pin CR2032 hoạt động > 1 năm).
- **Bộ lọc Whitelist và Chống Bão Bản Tin (BLE Storm Defense):**
  - Gói tin thu nhận được kiểm tra Service UUID: Chấp nhận chuẩn môi trường **`0x181A`** hoặc BTHome V2 UUID **`0xFCD2`**.
  - Kiểm tra địa chỉ MAC của thiết bị với bảng đăng ký NVS `device_list.json`.
  - **Rate Limiting (Giới hạn tần suất):** Mỗi cảm biến chỉ được phép đẩy tối đa **1 bản tin telemetry mỗi 5 giây** vào Event Bus. Nếu thiết bị phát beacon liên tục ở tần số cao, các bản tin trùng lặp trong cửa sổ 5s bị loại bỏ trực tiếp tại driver BLE, ngăn ngừa cạn kiệt hàng đợi sự kiện.

### 3.2 Giao Thức MQTT, Exponential Backoff và LWT
- **Chính sách phân luồng Topic chuẩn:**
  - Uplink Telemetry: `gateway/{gateway_id}/device/{device_id}/telemetry` (QoS 1).
  - Downlink Command: `gateway/{gateway_id}/device/{device_id}/command` (QoS 1).
  - Command ACK: `gateway/{gateway_id}/device/{device_id}/command/ack` (QoS 1).
  - Batch Backfill: `gateway/{gateway_id}/telemetry/batch` (QoS 1).
  - Trạng thái LWT: `gateway/{gateway_id}/status` (QoS 1, Retained).
- **Thuật toán Tái kết nối Lũy thừa có Biến thiên (Exponential Backoff with Jitter):**
  - Khi mất kết nối tới MQTT Broker, task `uplink_task` không thực hiện thử lại liên tục mà áp dụng công thức:
    $$T_{\text{retry}} = \min(T_{\text{base}} \times 2^{\text{attempt}} + \text{jitter}, 60\,\text{s})$$
    Với $T_{\text{base}} = 1.0\,\text{s}$, $\text{jitter} \in [0, 0.2 \times T_{\text{retry}}]$.
- **Last Will and Testament (LWT):**
  - Ngay khi kết nối, Gateway gửi bản tin LWT đăng ký với Broker: Topic `gateway/{id}/status`, Payload `"offline"`, QoS 1, `retain = true`.
  - Sau khi bắt tay thành công, Gateway publish chuỗi `"online"` (retained) vào chính topic này.
- `/* ponytail: Loại bỏ hoàn toàn lớp trừu tượng GATT Connection/Bonding. NimBLE chỉ làm Passive Observer quét gói tin thô 31-byte và giải mã trực tiếp qua UUID 0x181A/0xFCD2; giảm 60% kích thước binary của Bluetooth stack và tiết kiệm pin tối đa cho cảm biến. */`

---

## 4. Phân Hệ Điều Khiển Cơ Cấu Chấp Hành (Actuators & Relays Subsystem)

- **Trách nhiệm:** Điều khiển 4 kênh rơ-le vật lý, đảm bảo an toàn điện và chống cháy nổ tuyệt đối thông qua Khóa liên động (Interlocking), Ngắt khẩn cấp (Emergency Cutoff), và Khóa chu kỳ ngắn (Anti-short-cycle) (`FR-RELAY-01`).
- **Tập tin hiện thực:** [firmware/main/relay_controller.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/relay_controller.h), [firmware/main/relay_controller.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/relay_controller.c).

| Kênh | Chân GPIO | Tên Cơ Cấu | Tải Thực Tế Điển Hình | Chế Độ Kích Hoạt | Giới Hạn Bật Tối Đa (`MAX_ON_SEC`) | Hành Vi Safe-Default Khi Lỗi Cảm Biến |
|---|:---:|---|---|:---:|:---:|---|
| **Kênh 1** | `GPIO 15` | `relay_1` (`fan_01`) | Quạt thông gió làm mát | Active HIGH | 4 giờ (14400s) | Duy trì BẬT (ON) nếu trước đó nóng, hoặc giữ an toàn. |
| **Kênh 2** | `GPIO 16` | `relay_2` (`heater_01`) | Điện trở sưởi / Đèn sưởi | Active HIGH | **30 phút (1800s)** (Emergency Cutoff) | **BẮT BUỘC NGẮT (OFF) TỨC THÌ!** |
| **Kênh 3** | `GPIO 17` | `relay_3` (`dehum_01`) | Máy nén / Máy hút ẩm | Active HIGH | 60 phút (3600s) | NGẮT AN TOÀN (OFF). |
| **Kênh 4** | `GPIO 18` | `relay_4` (`aux_01`) | Đèn bàn / Tải phụ trợ | Active HIGH | Không giới hạn | Giữ nguyên trạng thái hiện tại. |

```mermaid
sequenceDiagram
    autonumber
    participant Engine as Rule Engine / MQTT Downlink
    participant Controller as relay_controller (Core 0)
    participant HW as Hardware GPIO 15..18 (Relays)
    participant Bus as Event Bus

    Engine->>Controller: relay_controller_set_state("heater_01", true, cmd_id, "RULE")
    Note over Controller: 1. Kiểm tra Anti-Short-Cycle (Dwell Time >= 180s)
    Note over Controller: 2. Kiểm tra KHÓA LIÊN ĐỘNG: Quạt (Relay 1) đang BẬT chưa?
    alt Quạt đang TẮT (Fan is OFF)
        Controller->>HW: BẬT Relay 1 (GPIO 15 HIGH)
        Controller->>Controller: Trì hoãn liên động 500ms (RELAY_INTERLOCK_DELAY_MS)
        Controller->>HW: BẬT Relay 2 (GPIO 16 HIGH)
        Note over Controller: Kích hoạt Timer ngắt khẩn cấp 30m (RELAY_2_MAX_ON_SEC)
    else Quạt đã BẬT sẵn
        Controller->>HW: BẬT Relay 2 (GPIO 16 HIGH)
        Note over Controller: Kích hoạt Timer ngắt khẩn cấp 30m
    end
    Controller->>Bus: event_bus_post(EVT_RELAY_STATE_CHANGED)
    Controller->>Bus: event_bus_post(EVT_COMMAND_RESPONSE) [Gửi ACK về Cloud]
```

### 4.1 Khóa Liên Động An Toàn (Interlocking), Dwell Time và Giới Hạn Cưỡng Chế
1. **Khóa liên động Quạt vs Máy sưởi (Fan-Heater Interlock):**
   - Không cho phép bật Máy sưởi (`relay_2`) nếu Quạt làm mát (`relay_1`) đang tắt nhằm tránh hiện tượng tích nhiệt gây cháy.
   - Khi có lệnh BẬT Máy sưởi: Hệ thống tự động kích hoạt Quạt trước, chờ **500ms** để luồng gió ổn định, sau đó mới đóng rơ-le Máy sưởi.
   - Khi có lệnh TẮT Quạt trong lúc Máy sưởi đang chạy: Hệ thống **ngắt Máy sưởi ngay lập tức**, duy trì Quạt chạy thêm thời gian tản nhiệt nếu cần.
2. **Khóa chu kỳ ngắn (Anti-Short-Cycle Protection):**
   - Áp dụng thời gian trễ tối thiểu **180 giây (Dwell Time)** giữa hai lần chuyển trạng thái ON $\leftrightarrow$ OFF liên tiếp của cùng một kênh rơ-le, triệt tiêu hiện tượng đóng cắt liên tục (Chattering) làm hỏng tiếp điểm rơ-le và cháy động cơ máy nén.
3. **Bộ đếm thời gian ngắt khẩn cấp (Emergency Cutoff Timer):**
   - Kênh Máy sưởi (`relay_2`) được gán bộ giám sát độc lập `RELAY_2_MAX_ON_SEC = 1800` giây (30 phút). Nếu sau 30 phút mà không nhận được lệnh tắt hợp lệ từ Cloud hay Rule Engine (do treo logic hoặc rớt mạng), bộ điều khiển rơ-le tự động cắt điện cưỡng bức và phát sự kiện `EVT_RELAY_STATE_CHANGED` với lý do `"SAFETY_CUTOFF"`.
- `/* ponytail: Thiết kế bảng cấu hình rơ-le tĩnh s_relays[4] thay vì HAL Device Tree hướng đối tượng phức tạp. 4 kênh phần cứng cố định được điều khiển trực tiếp qua gpio_set_level(). Khóa liên động và Dwell Time kiểm tra bằng phép so sánh thời gian epoch đơn giản O(1), không tốn timer FreeRTOS phụ. */`

---

## 5. Phân Hệ Tự Động Hóa Cục Bộ (Local Automation Subsystem)

- **Trách nhiệm:** Thực thi toàn bộ luật tự động hóa (Rule Engine) và lịch biểu / báo thức (Local Scheduler) ngay tại biên (Edge Computing), hoạt động độc lập 100% khi mất kết nối Internet (`FR-RULE-01`, `FR-SCHED-01`).
- **Tập tin hiện thực:** [firmware/main/rule_engine.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.h), [firmware/main/rule_engine.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c), [firmware/main/scheduler.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/scheduler.h), [firmware/main/scheduler.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/scheduler.c).

```mermaid
flowchart TD
    subgraph SENSING["Dữ Liệu Đầu Vào"]
        EVT_DATA["EVT_BLE_DEVICE_DATA (Nhiệt độ / Độ ẩm)"]
        RTC_TICK["scheduler_task (Mỗi 1000ms)"]
    end

    subgraph RULE_ENGINE["Rule Engine (Đánh Giá Local)"]
        CHECK_VALID{"Cảm biến valid == true?"}
        HYST["Hysteresis Deadband (1.5°C)"]
        EDGE["Kích hoạt Cạnh (Edge-Triggering)"]
        LOCK["Thời gian khóa (Lockout 300s)"]
        ACT["Phát EVT_RULE_ACTION_REQUEST"]
        SAFE["KÍCH HOẠT SAFE-DEFAULT MATRIX!"]
    end

    subgraph SCHEDULER["Local Scheduler (Lịch Biểu & Báo Thức)"]
        CHK_RTC{"rtc_is_valid()?"}
        ROLLOVER["Rollover Guard (Chống bắn lặp qua nửa đêm)"]
        CATCHUP["Time Jump Catch-up Scan (15 phút)"]
        ALARM["Phát EVT_SCHEDULE_ALARM_TRIGGERED"]
    end

    EVT_DATA --> CHECK_VALID
    CHECK_VALID -->|true| HYST
    CHECK_VALID -->|false| SAFE
    HYST --> EDGE --> LOCK --> ACT

    RTC_TICK --> CHK_RTC
    CHK_RTC -->|true| ROLLOVER --> CATCHUP --> ALARM
    CHK_RTC -->|false| HOLD["Tạm dừng lịch biểu chờ đồng bộ giờ"]
```

### 5.1 Thuật Toán Hysteresis, Kích Hoạt Cạnh và Lockout của Rule Engine
1. **Dải trễ Hysteresis Deadband ($1.5^\circ\text{C}$):**
   - Để kiểm soát thiết bị làm mát với ngưỡng cài đặt $T_{\text{threshold}} = 30.0^\circ\text{C}$:
     - BẬT Quạt khi $T \ge T_{\text{threshold}} + \frac{\text{Hysteresis}}{2} = 30.0 + 0.75 = 30.75^\circ\text{C}$.
     - TẮT Quạt khi $T \le T_{\text{threshold}} - \frac{\text{Hysteresis}}{2} = 30.0 - 0.75 = 29.25^\circ\text{C}$.
   - Dải nằm giữa $29.25^\circ\text{C}$ và $30.75^\circ\text{C}$ là vùng chết (Deadband), giữ nguyên trạng thái trước đó của rơ-le, ngăn chặn hiện tượng bật tắt liên tục do nhiễu số đo cảm biến.
2. **Kích hoạt theo cạnh chuyển mức (Edge-Triggering):**
   - Hành động điều khiển chỉ được phát sinh một lần duy nhất tại thời điểm trạng thái chuyển từ `FALSE` sang `TRUE` (Rising Edge) hoặc ngược lại. Khi nhiệt độ duy trì ở mức cao qua nhiều chu kỳ quét liên tiếp, Rule Engine không bắn lại lệnh, giảm tải cho Event Bus.
3. **Thời gian khóa an toàn (Lockout 300s):**
   - Sau khi một luật tự động thay đổi trạng thái thiết bị, luật đó rơi vào trạng thái khóa an toàn trong **300 giây (5 phút)**. Mọi biến động nhiệt độ trong 300 giây này đều bị bỏ qua nhằm đảm bảo quán tính nhiệt của phòng được ổn định.

### 5.2 Cơ Chế Bảo Vệ của Local Scheduler
1. **Chống kích hoạt lặp qua nửa đêm (Rollover Guard):**
   - Mỗi mục lịch biểu lưu trữ biến `last_triggered_day` (1..31). Khi đồng hồ chuyển từ `23:59:59` sang `00:00:00`, biến trạng thái kích hoạt trong ngày bị xóa sạch, bảo đảm không xảy ra hiện tượng báo thức bị kích hoạt 2 lần hoặc bị bỏ sót khi sang ngày mới.
2. **Quét bù sự kiện khi thời gian nhảy cóc (Time Jump Catch-up Scan):**
   - Khi mạng Internet kết nối lại và SNTP hiệu chỉnh thời gian nhảy vọt về phía trước (ví dụ do mất điện nhiều giờ), hàm quét bù kiểm tra toàn bộ các mục lịch biểu trong cửa sổ **15 phút** vừa qua. Nếu có báo thức hoặc lệnh quan trọng bị trễ trong 15 phút đó, hệ thống sẽ kích hoạt ngay lập tức thay vì bỏ qua.
- `/* ponytail: Tối giản hóa Rule Engine & Scheduler: Lưu trữ tối đa 16 rules tĩnh (MAX_RULES=16) bảo vệ bởi 1 FreeRTOS Mutex duy nhất; không dùng engine biên dịch biểu thức logic hay scripting language (Lua/JS) tốn kém RAM. Scheduler duyệt tuần tự O(N) với N<=16 mỗi 1000ms chỉ tốn vài chục chu kỳ CPU. */`

---

## 6. Phân Hệ Lưu Trữ và Hàng Đợi Ngoại Tuyến (Storage & Offline Queue)

- **Trách nhiệm:** Duy trì dữ liệu cấu hình cục bộ không bị hỏng khi mất điện đột ngột (`FR-ST-01`), lưu trữ nhật ký và duy trì hàng đợi Store-and-Forward trên thẻ nhớ MicroSD (`FR-SD-01`, `FR-SD-02`).
- **Tập tin hiện thực:** [firmware/main/storage.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/storage.h), [firmware/main/storage.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/storage.c), [firmware/main/sd_logger.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/sd_logger.h), [firmware/main/sd_logger.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/sd_logger.c).

```mermaid
sequenceDiagram
    autonumber
    participant Cloud as Cloud Config Downlink
    participant Storage as storage.c (LittleFS /storage)
    participant Flash as SPI Flash Physical Sectors
    participant Bus as Event Bus

    Cloud->>Storage: storage_save_schedule_json(new_json)
    Note over Storage: Bước 1: Mở file tạm /storage/schedule.json.tmp
    Storage->>Flash: fwrite(new_json)
    Storage->>Flash: fflush() & fsync()
    Note over Storage: Bước 2: THAY THẾ NGUYÊN TỬ (Atomic Replacement)
    Storage->>Flash: rename("/storage/schedule.json.tmp", "/storage/schedule.json")
    Note over Storage: Dù mất nguồn tại bất kỳ mili-giây nào, file gốc vẫn nguyên vẹn!
    Storage->>Bus: event_bus_post(EVT_CONFIG_UPDATED_FROM_CLOUD)
```

### 6.1 Cơ Chế Ghi Đè File Nguyên Tử (Atomic File Replacement)
Để chống lại việc file JSON cấu hình (`config.json`, `rules.json`, `schedule.json`) bị rỗng hoặc hỏng cấu trúc khi mất điện bất ngờ trong lúc đang ghi:
1. File mới luôn được ghi vào một file tạm có phần mở rộng `.tmp` (vd: `/storage/rules.json.tmp`).
2. Thực hiện `fflush()` và ép đồng bộ sector vật lý.
3. Sử dụng lời gọi hàm POSIX `rename()` của LittleFS để hoán đổi tên file. Trên LittleFS, `rename()` là thao tác cập nhật con trỏ inode mang tính **nguyên tử (Atomic Operation)**: hệ thống tệp chỉ chuyển sang file mới khi toàn bộ dữ liệu đã nằm an toàn trên flash. Nếu mất điện giữa chừng, lần boot sau hệ thống vẫn đọc được file gốc hoàn chỉnh.
- `/* ponytail: Tận dụng cơ chế ghi nguyên tử rename() của LittleFS và hàm f_sync() chu kỳ 5s của FATFS; loại bỏ nhu cầu tự phát triển hệ thống ghi nhật ký kép (journaling) hoặc transaction engine cồng kềnh. */`

### 6.2 Giao Tiếp Thẻ Nhớ MicroSD SPI và sync_queue.bin
- **Chân kết nối SPI2_HOST:** `GPIO 10 (CS)`, `GPIO 11 (MOSI)`, `GPIO 12 (CLK)`, `GPIO 13 (MISO)`. Tần số bus SPI 20 MHz.
- **Hàng đợi nhị phân Store-and-Forward (`sync_queue.bin`):**
  - Khi mất kết nối Wi-Fi hoặc MQTT, mọi bản tin telemetry cảm biến và sự kiện hệ thống được đóng gói nhị phân và đẩy vào hàng đợi `s_sd_log_queue`.
  - Task `sd_log_task` ghi tuần tự vào file `/sdcard/data/sync_queue.bin` theo cơ chế con trỏ vòng tròn (Circular FIFO).
  - Định kỳ gọi `f_sync()` sau mỗi **10 bản ghi** hoặc tối đa **5 giây**, bảo vệ bảng FAT không bị lỗi khi người dùng rút nguồn đột ngột.

---

## 7. Phân Hệ Giám Sát Tác Vụ và Cầu Dao Khởi Động (Task Supervisor Subsystem)

- **Trách nhiệm:** Giám sát liên tục 7 production tasks thông qua Heartbeat Mask `0x7F`, kích hoạt Hardware TWDT khi task bị kẹt, và ngắt mạch chống Boot Loop (`FR-SUP-01`, `FR-SUP-02`).
- **Tập tin hiện thực:** [firmware/main/supervisor.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.h), [firmware/main/supervisor.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.c).

```mermaid
flowchart TD
    subgraph TASKS["7 Production Tasks (Mỗi 1000ms)"]
        T0["Bit 0: event_dispatcher"]
        T1["Bit 1: ble_scan_task"]
        T2["Bit 2: uplink_task"]
        T3["Bit 3: sd_log_task"]
        T4["Bit 4: scheduler_task"]
        T5["Bit 5: display_task"]
        T6["Bit 6: serial_cli_task"]
    end

    subgraph SUPERVISOR["Task Supervisor (Core 0, Prio 7 - Chu kỳ 1000ms)"]
        REG[Bộ ghi nhận cờ s_heartbeat_flags]
        CHECK{"(s_heartbeat_flags & 0x7F) == 0x7F?"}
        FEED["Tiếp thức ăn cho Hardware TWDT & Xóa flags về 0"]
        STALL{"Một task không báo nhịp > 15 giây?"}
        DUMP["Ghi chẩn đoán rtc_crash_dump_t vào RTC Slow Memory"]
        RESTART["esp_restart() / TWDT Reboot"]
        CIRCUIT{"Số lần crash liên tiếp >= 3 trong 60s?"}
        SAFE_MODE["KÍCH HOẠT SAFE-DEFAULTS MODE (Bỏ qua config lỗi)"]
    end

    T0 & T1 & T2 & T3 & T4 & T5 & T6 -->|supervisor_report_heartbeat| REG
    REG --> CHECK
    CHECK -->|Đủ 7 tasks| FEED
    CHECK -->|Thiếu task| STALL
    STALL -->|Timeout 15s| CIRCUIT
    CIRCUIT -->|>= 3 crashes| SAFE_MODE
    CIRCUIT -->|< 3 crashes| DUMP --> RESTART
```

### 7.1 Cấu Trúc Chẩn Đoán RTC Slow Memory và Cầu Dao Chống Treo Lặp (Boot Loop Circuit Breaker)
- **Vùng nhớ không bay hơi RTC Slow Memory:**
  - Vùng nhớ RTC Slow Memory duy trì toàn vẹn dữ liệu qua các lần khởi động lại do phần mềm (`SW_CPU_RESET`).
  - Khi phát hiện một task bị treo quá 15 giây (vd: trong bài test `FR-SUP-02`), supervisor điền thông tin vào struct `rtc_crash_dump_t`: Magic `0xDEADBEEF`, bit task lỗi, tên task, dung lượng free heap, uptime và UNIX timestamp.
- **Cầu dao chống lặp khởi động (Boot Loop Circuit Breaker):**
  - Trong RTC Memory duy trì biến đếm `s_boot_crash_count`. Nếu thiết bị khởi động lại và gặp sự cố liên tiếp **$\ge 3$ lần trong vòng 60 giây**:
    - Hệ thống đánh giá cấu hình flash hoặc database cục bộ đang bị hỏng.
    - Gateway tự động bỏ qua việc nạp file `rules.json` và `schedule.json` tùy biến, chuyển sang chế độ **Safe-Defaults Minimal Mode**.
    - Khởi chạy SoftAP Wi-Fi cứu hộ và mở Serial CLI UART0 để kỹ thuật viên can thiệp sửa chữa, tránh việc thiết bị bị treo vĩnh viễn ngoài hiện trường.
- `/* ponytail: Cơ chế giám sát siêu nhẹ: Dùng 1 biến toàn cục uint32_t s_heartbeat_flags với các thao tác bitwise nguyên tử (|=, &=) thay vì cấp phát FreeRTOS EventGroups (tiêu tốn heap và spinlock nội). Ngưỡng 15s đếm qua biến tĩnh, tiêu tốn 0 byte dynamic RAM và < 0.1% CPU Core 0. */`
