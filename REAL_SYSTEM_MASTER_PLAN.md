# Kế Hoạch Thống Nhất Thiết Kế & Hoàn Thiện Hệ Thống Thật 100% (`REAL_SYSTEM_MASTER_PLAN.md`)

> **Mục tiêu tối thượng**: Triệt tiêu hoàn toàn mọi đoạn code giả (fake UI, hardcoded strings, mock fallbacks, luồng đồng bộ đứt gãy) giữa **Mạch 1 (Smart Clock Gateway)**, **Mạch 2 (Remote IoT Devices Simulator — Cảm biến BLE + Thiết bị chấp hành từ xa)**, **Cloud Backend (Node.js/Prisma/EMQX)** và **Web App (React SPA + Local ESP32 Web UI)**.

---

## 1. Thống Nhất Thiết Kế Toàn Hệ Thống (Confirmed Design Contract)

### 1.1. Vai trò của 2 Mạch Phần Cứng Khi Vận Hành & Demo Thực Tế
* **Mạch 1 — Smart Clock & Edge IoT Gateway (`qwen-test/firmware`)**:
  - Chạy độc lập (không cần cắm PC sau khi đã nạp code): gắn Màn hình OLED SSD1327 128x128 (I2C `0x3C`), RTC DS3231 có pin CR2032 (I2C `0x68`), Khe thẻ nhớ MicroSD (SPI2), Nút bấm và 4 ngõ ra điều khiển thiết bị (GPIO 15..18).
  - Thu thập dữ liệu cảm biến BLE từ xa, đánh giá Rule Engine tại biên (Edge), lưu thẻ nhớ MicroSD, hiển thị đồng hồ/lịch biểu, và phát lệnh điều khiển tới các thiết bị chấp hành từ xa (trên Mạch 2) cũng như đồng bộ 2 chiều với Cloud.
* **Mạch 2 — Thiết Bị IoT Kết Nối Từ Xa (`qwen-iot-simulator` — Luôn cắm USB vào PC khi Demo)**:
  - Đóng vai **2 nhóm thiết bị ngoại vi từ xa** giao tiếp với Mạch 1:
    1. **Cảm biến Nhiệt độ & Độ ẩm BLE từ xa**: Phát sóng quảng bá BLE (`0x181A` hoặc `BTHome 0xFCD2`) sang Mạch 1. Người vận hành gõ lệnh Serial trên PC (`set temp`, `set hum`, `send`) để thay đổi nhiệt độ/độ ẩm môi trường.
    2. **4 Thiết bị chấp hành từ xa (Remote Actuators)**:
       - Thiết bị 1 (`relay_1` / `fan_01`): **Quạt làm mát (Cooling Fan)**
       - Thiết bị 2 (`relay_2` / `heater_01`): **Máy sưởi nhiệt (Electric Heater)**
       - Thiết bị 3 (`relay_3` / `dehumidifier_01`): **Máy hút ẩm / Điều hòa (Dehumidifier / AC Compressor)**
       - Thiết bị 4 (`relay_4` / `aux_01`): **Thiết bị phụ trợ / Đèn (Auxiliary Load)**
  - **Quy tắc hiển thị trên màn hình Serial của Mạch 2**:
    - Màn hình Serial của Mạch 2 **CHỈ hiển thị đúng các lệnh điều khiển mà Mạch 1 yêu cầu các thiết bị từ xa trên Mạch 2 thực hiện** (ví dụ: Khi nhiệt độ > 30°C $\rightarrow$ Mạch 1 yêu cầu BẬT Quạt; khi nhiệt độ hạ xuống $\le$ 28.5°C $\rightarrow$ Mạch 1 yêu cầu TẮT Quạt; khi lịch hẹn giờ 18:00 điểm $\rightarrow$ Mạch 1 yêu cầu TẮT Thiết bị 4; hoặc khi người dùng bấm nút điều khiển từ Web $\rightarrow$ Mạch 1 yêu cầu Mạch 2 đóng/ngắt thiết bị).
    - **Không in** các log nội bộ của Mạch 1 không liên quan đến thiết bị trên Mạch 2 (như chuông báo thức cá nhân trên đồng hồ Mạch 1) để màn hình Serial của Mạch 2 phản ánh đúng vai trò của các thiết bị IoT từ xa.

---

### 1.2. Cơ Chế Bảo Vệ Thiết Bị Chống Bật/Tắt Liên Tục Quanh Ngưỡng (Anti-Chattering & Equipment Protection)
Khi nhiệt độ dao động liên tục quanh ngưỡng (ví dụ nhảy `29.9°C <-> 30.1°C` quanh mốc `30.0°C`), nếu chỉ so sánh `> 30°C` thông thường thì Quạt hay Điều hòa sẽ bật/tắt liên tục gây cháy động cơ hoặc hỏng máy nén. Hệ thống bắt buộc thực thi và hiển thị rõ **3 lớp bảo vệ vật lý**:

1. **Lớp 1 — Vùng trễ Hysteresis (Deadband) hai chiều trong Rule Engine (`rule_engine.c`)**:
   - **Với Luật Quá Nhiệt / Quá Ẩm (`condition: GT ">"`)**:
     - Ngưỡng BẬT (`Turn-ON Threshold`): $T \ge T_{\text{threshold}}$ (VD: **$\ge 30.0^\circ\text{C}$** thì Mạch 1 gửi lệnh **BẬT Quạt**).
     - Vùng giữ trạng thái (`Deadband`): Khi nhiệt độ giảm nhẹ xuống `29.8°C`, `29.5°C`, `28.8°C` ($T_{\text{threshold}} - H < T < T_{\text{threshold}}$, với độ trễ $H = 1.5^\circ\text{C}$), Mạch 1 **giữ nguyên trạng thái Quạt đang BẬT**, tuyệt đối không gửi lệnh tắt.
     - Ngưỡng TẮT (`Turn-OFF Release`): Chỉ khi nhiệt độ mát hẳn xuống **$T \le T_{\text{threshold}} - H$** (VD: **$\le 28.5^\circ\text{C}$**), Mạch 1 mới gửi lệnh **TẮT Quạt** sang Mạch 2.
   - **Với Luật Sưởi Ấm (`condition: LT "<"`)**:
     - Ngưỡng BẬT: $T \le 22.0^\circ\text{C}$ $\rightarrow$ BẬT Sưởi.
     - Vùng giữ trạng thái: $22.0^\circ\text{C} < T < 23.5^\circ\text{C}$ $\rightarrow$ Giữ nguyên Sưởi BẬT.
     - Ngưỡng TẮT: Chỉ khi ấm lên **$T \ge 23.5^\circ\text{C}$** ($22.0 + 1.5$) $\rightarrow$ Mới TẮT Sưởi.
   - **Với Luật Độ Ẩm (`condition: GT ">"` cho Máy hút ẩm / Điều hòa)**:
     - Ngưỡng BẬT: $RH \ge 75.0\%$ $\rightarrow$ BẬT Máy hút ẩm.
     - Ngưỡng TẮT: Chỉ khi độ ẩm hạ xuống **$RH \le 73.0\%$** ($75.0 - 2.0$, với $H = 2.0\%$) $\rightarrow$ Mới TẮT Máy hút ẩm.
   - **Hiển thị & Cấu hình tường minh trên Web App (`Rules.jsx`, Local Web UI & Backend)**:
     - Trên mỗi thẻ Quy tắc (Rule Card) và form Tạo/Sửa Quy tắc trên Web App cũng như Web Cục bộ (`192.168.1.198`), phải hiển thị rõ cả **Ngưỡng kích hoạt (`Threshold`)** lẫn **Độ trễ bảo vệ (`Hysteresis`, mặc định `1.5°C` cho nhiệt độ, `2.0%` cho độ ẩm)** kèm dòng giải thích tự động: *"Bật khi $\ge 30.0^\circ\text{C}$ • Tự tắt bảo vệ khi $\le 28.5^\circ\text{C}$"*.
2. **Lớp 2 — Thời gian nghỉ tối thiểu giữa 2 chu kỳ kích hoạt (`Cooldown = 60s`)**:
   - Sau khi một Rule vừa phát lệnh BẬT thiết bị, Rule đó phải chờ tối thiểu **60 giây** (tính bằng đồng hồ đơn điệu `esp_timer_get_time()`) mới được phép kích hoạt BẬT lại nếu thiết bị vừa bị tắt. (Chiều TẮT khi nhiệt độ đã đạt ngưỡng an toàn hoặc cắt khẩn cấp thì luôn thực thi tức thì `0ms`).
3. **Lớp 3 — Bảo vệ Lốc nén Điều hòa / Máy hút ẩm (`Anti-Short-Cycle Dwell Time = 180s` & `Asymmetric Interlock`)**:
   - **Chống đóng ngắt nhanh máy nén (Relay 3)**: Khi Máy hút ẩm / Điều hòa (`relay_3`) vừa TẮT, phần cứng khóa cứng kênh 3 trong **180 giây (3 phút)**. Mọi lệnh yêu cầu BẬT lại kênh 3 trong thời gian này (dù từ Rule hay bấm tay) đều bị Mạch 1 chặn lại để bảo vệ lốc nén.
   - **Khóa liên động bất đối xứng Quạt - Sưởi (Relay 1 & Relay 2)**: Không bao giờ cho phép Quạt làm mát và Máy sưởi chạy cùng lúc. Nếu Quạt đang chạy mà có lệnh bật Sưởi $\rightarrow$ Từ chối bật Sưởi. Nếu Sưởi đang chạy mà nhiệt độ vọt lên quá ngưỡng cần bật Quạt $\rightarrow$ Mạch 1 phát lệnh **TẮT Sưởi ngay lập tức (`0ms`, `COOLING_OVERRIDE`)** rồi mới **BẬT Quạt**, đồng thời khóa Máy sưởi trong **300 giây (5 phút)**.
4. **Lớp 4 — Ưu Tiên Quyền Điều Khiển Thủ Công Của Con Người (`Manual Override Priority = 30 phút`)**:
   - **Human Always Wins over Automation**: Khi người dùng chủ động BẬT một rơ-le bằng tay (`MANUAL_CMD`, ví dụ bật Quạt ở `20°C`), `rule_engine.c` kích hoạt cửa sổ `RULE_MANUAL_OVERRIDE_US` (**30 phút** tính theo `esp_timer_get_time()`) trên kênh rơ-le đó và giữ `r->current_state = false`, đảm bảo gói cảm biến `20°C` tiếp theo không tự ý tắt Quạt của người dùng. Nếu người dùng chủ động TẮT lại rơ-le vừa bật tay, quyền ưu tiên thủ công được xóa ngay (`0`) để rơ-le trở về chế độ Tự động (Auto). Ngược lại, nếu Rule tự động đang BẬT thiết bị (ví dụ Quạt đang chạy ở `31°C`) mà người dùng chủ động bấm TẮT (`MANUAL_CMD`), hệ thống hạ `r->current_state = false` và giữ quyền ưu tiên thủ công trong **30 phút** để Rule không tự bật lại sau 60s. Các lệnh cắt an toàn phần cứng (`SAFETY_CUTOFF`, `COOLING_OVERRIDE`, `SENSOR_OFFLINE`, `EMERGENCY_SHUTDOWN`) luôn xóa ngay quyền ưu tiên thủ công vì an toàn vật lý là tối thượng.

---

### 1.3. Các Luồng Dữ Liệu Thật Khác Đã Thống Nhất
1. **Cảm biến BLE Đa chỉ số (`ble_adapter.c` & `rule_engine.c`)**:
   - Một cảm biến BLE trên Mạch 2 (`FA:BB:CC:11:22:33`, ID `sensor_temp_01`) gửi cả Nhiệt độ và Độ ẩm trong cùng 1 gói tin.
   - Bộ lọc 2 giây trong `ble_adapter.c` cho phép gói tin đi qua nếu `|ΔT| >= 0.2°C` **HOẶC** `|ΔH| >= 0.5%RH`.
   - `rule_engine.c` đánh giá cả Luật Nhiệt độ lẫn Luật Độ ẩm trên cùng gói tin BLE đó.
   - Xóa hoàn toàn đoạn code backdoor tự bịa nhiệt độ `31.50°C` trong `ble_adapter.c`.
   - Khi thêm/sửa/xóa cảm biến BLE trên trang `Devices.jsx`, Backend đồng bộ danh sách `ble_sensors` xuống Mạch 1 qua MQTT.
2. **Màn hình OLED SSD1327 trên Mạch 1 (`display.c`)**:
   - **Screen 1 (`UPCOMING EVENTS`)**: Xóa 3 dòng chữ vẽ chết (`06:30 Wakeup`, `17:30 Fan Auto`, `21:00 Aux Off`). Đọc trực tiếp các lịch đang bật từ `scheduler_get_entries()`, sắp xếp theo giờ gần nhất tính từ giờ RTC hiện tại và vẽ tối đa 3 lịch thật lên OLED.
3. **Đồng bộ Múi giờ (`Settings.jsx`) & Lưu trữ Offline (`sd_logger.c`, `broker.js`)**:
   - Đổi Múi giờ trên `Settings.jsx` gửi MQTT xuống Mạch 1 áp dụng ngay cho RTC và OLED.
   - Thẻ nhớ MicroSD (`.csv` và `sync_queue.bin`) lưu đầy đủ cả Nhiệt độ + Độ ẩm; báo cáo dung lượng thẻ thật từ `esp_vfs_fat_info()`.
   - Khi có mạng lại, dữ liệu bù `telemetry/batch` được `sd_logger.c` gửi chắc chắn và được Backend `handleTelemetryBatch()` ghi thật vào bảng `TelemetryLog` để vẽ lên biểu đồ Web App.
   - Trang Web Cục Bộ trên Mạch 1 (`http://192.168.1.198/`) có thêm bảng quản lý **Schedules & Alarms** và hiển thị thông số **Hysteresis** của từng Rule.

---

## 2. Kết Quả Rà Soát Mạch 2 (`qwen-iot-simulator`) & Các Điểm Cần Đồng Nhất

| STT | Hiện trạng trên Mạch 2 (`qwen-iot-simulator`) | Vị trí file | Điều chỉnh để khớp 100% với vai trò Thiết bị Từ Xa trên PC |
|:---:|---|---|---|
| **1** | **Màn hình Serial đang in log theo kiểu "Trạm sniffer/debug MQTT" thay vì "Thiết bị từ xa nhận lệnh từ Mạch 1"**: Hiện tại `sim_rx_monitor.c` in cả các gói ACK/lỗi hoặc bị thiếu mất lệnh tắt cưỡng bức khi Mạch 1 thực hiện `COOLING_OVERRIDE` / `SAFETY_CUTOFF` (do Mạch 1 chỉ phát sự kiện `relay_state_changed` khi ngắt an toàn). | `sim_rx_monitor.c:157-330` | Chuẩn hóa đầu ra Serial của Mạch 2 chỉ đóng vai **4 Thiết bị chấp hành từ xa**: Mạch 1 gửi lệnh yêu cầu thực thi (quaRule Engine, Lịch hẹn giờ, Manual Web, hoặc Lệnh ngắt bảo vệ `COOLING_OVERRIDE`/`SAFETY_CUTOFF`), Mạch 2 nhận lệnh, đổi trạng thái thiết bị tương ứng (`ON`/`OFF`) và in thông báo hành động rõ ràng ra Serial PC (kèm lý do: do Luật nhiệt độ vượt ngưỡng, do Hạ nhiệt dưới ngưỡng trễ Hysteresis, do Ngắt an toàn, hay do Lệnh từ người dùng). |
| **2** | **Lệch địa chỉ MAC mặc định giữa Mạch 2 và Mạch 1**: Mạch 2 phát MAC Static Random `FA:BB:CC:11:22:33`, còn Mạch 1 đăng ký mặc định `AA:BB:CC:11:22:33`. | `sim_ble_tx.c:36` vs `ble_adapter.c:382` | Chuẩn hóa Mạch 1 đăng ký mặc định `FA:BB:CC:11:22:33` khớp hoàn toàn với Mạch 2. |
| **3** | **Thiếu kịch bản test trực quan cho cơ chế Hysteresis quanh ngưỡng 30°C & `scenario dwell` đang phải lách luật**: `sim_cli.c` chưa có kịch bản mô phỏng nhiệt độ nhảy quanh 30°C (`29.9 <-> 30.2 <-> 29.5 <-> 28.4°C`), và `scenario dwell` đang phải tự đổi tên sang `sensor_hum_01` + cộng `0.5°C`. | `sim_cli.c:109-211` | 1. Nâng cấp `scenario cooling` (hoặc `scenario hysteresis`) mô phỏng rõ nhiệt độ dao động quanh 30°C: tăng lên `30.2°C` (Mạch 1 yêu cầu BẬT Quạt) $\rightarrow$ giảm nhẹ xuống `29.6°C` trong vùng trễ Hysteresis (Quạt được giữ nguyên BẬT, không bị tắt/bật liên tục) $\rightarrow$ giảm xuống `28.3°C` dưới ngưỡng `30.0 - 1.5 = 28.5°C` (Mạch 1 yêu cầu TẮT Quạt).<br>2. Sửa `scenario dwell` dùng nguyên 1 cảm biến `sensor_temp_01`, chỉ đổi độ ẩm `80% -> 70% -> 82%`. |
| **4** | **Cấu hình Wi-Fi qua Serial trên Mạch 2 chưa lưu NVS & chưa có `wifi scan`**: Mỗi lần reset Mạch 2 lại mất Wi-Fi, và nếu tên Wi-Fi có khoảng trắng thì `sscanf` bị cắt chuỗi. | `sim_cli.c:320-329`, `sim_rx_monitor.c:776` | Thêm lệnh `wifi scan` trên Serial Mạch 2; sửa parser `wifi connect <ssid> <pass>` hỗ trợ SSID có khoảng trắng, tự lưu vào NVS Flash và tự động kết nối lại khi bật nguồn. |
| **5** | **File rác trong thư mục `qwen-iot-simulator`**: Còn 5 file `test_results_*.json` và `PLAN.md` cũ. | `/home/chien/ai-workspace/qwen-iot-simulator/` | Xóa sạch 5 file `test_results_*.json` và `PLAN.md` cũ. |

---

## 3. Kế Hoạch Chỉnh Sửa Chi Tiết Theo Từng Hạng Mục

### Hạng mục 1: Firmware Mạch 1 — Smart Clock Gateway (`qwen-test/firmware`)
1. **[firmware/main/ble_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.c) & [ble_adapter.h](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.h)**:
   - Đổi MAC mặc định của `sensor_temp_01` sang `FA:BB:CC:11:22:33` (và `sensor_hum_01` sang `FA:BB:CC:44:55:66`).
   - Lưu thêm `last_humidity` trong `ble_device_entry_t`; cho phép gói BLE vượt qua bộ lọc 2 giây khi `|ΔT| >= 0.2f` **hoặc** `(has_hum && |ΔH| >= 0.5f)`.
   - Xóa khối code backdoor tự bịa `temp_val = 31.50f` (dòng 235–245).
   - Bổ sung hàm `ble_adapter_sync_devices_from_json(const cJSON *arr)` để nhận danh sách cảm biến BLE từ Cloud.
2. **[firmware/main/rule_engine.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c)**:
   - Cho phép gói tin từ cảm biến BLE đa chỉ số đánh giá cả Rule Nhiệt độ lẫn Rule Độ ẩm.
   - Khi Rule kích hoạt BẬT (`EDGE-UP`) hoặc TẮT khi ra khỏi vùng trễ Hysteresis (`EDGE-DOWN`), truyền rõ lý do (`"RULE_THRESHOLD_TRIGGER"` vs `"RULE_HYSTERESIS_RELEASE"`) trong bản tin gửi đi để Mạch 2 hiển thị tường minh hành động được yêu cầu.
   - Bảo toàn và đồng bộ đầy đủ trường `hysteresis` và `cooldown_s` trong JSON 2 chiều (`rules/sync` và `config/schedule`).
3. **[firmware/main/display.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/display.c)**:
   - Tại `DISPLAY_SCREEN_UPCOMING_EVENT`: Gọi `scheduler_get_entries()`, lọc các lịch `enabled == true`, sắp xếp 3 lịch sắp tới gần nhất theo giờ RTC hiện tại và vẽ thật lên OLED SSD1327 + Serial log.
4. **[firmware/main/sd_logger.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/sd_logger.c)**:
   - Lưu đầy đủ cả `temperature` và `humidity` vào file CSV hàng ngày và `sync_queue.bin` (`{"device_id":"%s","temperature":%.2f,"humidity":%.2f,"ts":%lu}`).
   - Chỉ tăng `read_offset` / giảm `unacked_count` khi `mqtt_adapter_publish("telemetry/batch", ...)` trả về `true`.
   - Lấy `free_kb` và `total_kb` thật từ `esp_vfs_fat_info()`.
5. **[firmware/main/mqtt_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/mqtt_adapter.c) & [uplink.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c)**:
   - Khi nhận `config/schedule`, áp dụng luôn trường `timezone` (cập nhật RTC + lưu NVS + vẽ lại OLED) và mảng `ble_sensors`.
   - Đính kèm `"rtc_time"` vào bản tin telemetry định kỳ.
6. **[firmware/main/web_server.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/web_server.c)**:
   - Cập nhật `GET/POST /api/rules` và bảng Rules trên `DASHBOARD_HTML` (`192.168.1.198`) có thêm cột & ô nhập **Hysteresis (Độ trễ bảo vệ)**.
   - Bổ sung bảng **Schedules & Alarms** vào `DASHBOARD_HTML` để quản lý lịch hẹn giờ và báo thức ngay trên mạng LAN.
7. **[test/host/stubs/cJSON.h](file:///home/chien/ai-workspace/qwen-test/test/host/stubs/cJSON.h)**:
   - Bổ sung stub `cJSON_free`, `cJSON_AddItemToObject`, `cJSON_CreateIntArray` để `make -C test/host` biên dịch sạch và chạy PASS 9/9 bài test C Native (bao gồm bài test Hysteresis, Interlock và Dwell 180s).

### Hạng mục 2: Firmware Mạch 2 — Remote IoT Simulator (`qwen-iot-simulator/main`)
1. **[sim_cli.c](file:///home/chien/ai-workspace/qwen-iot-simulator/main/sim_cli.c)**:
   - Thêm lệnh `wifi scan` quét Wi-Fi qua Serial; nâng cấp `wifi connect <ssid> <pass>` hỗ trợ SSID có khoảng trắng và tự lưu vào NVS.
   - Rõ ràng hóa các lệnh `set temp <val>`, `set hum <val>`, `send <temp> <hum>` trên Serial.
   - Cập nhật `scenario cooling` để demo trực tiếp **cơ chế bảo vệ Hysteresis quanh mốc 30°C**:
     - Bước 1: `25.0°C` (Bình thường — Quạt TẮT)
     - Bước 2: `30.5°C` (Vượt ngưỡng `30.0°C` $\rightarrow$ Mạch 1 yêu cầu Mạch 2 **BẬT Quạt**)
     - Bước 3: `29.4°C` (Dao động dưới 30°C nhưng vẫn nằm trong vùng bảo vệ Hysteresis `28.5°C..30.0°C` $\rightarrow$ Mạch 1 **KHÔNG tắt quạt**, bảo vệ thiết bị khỏi bật/tắt liên tục)
     - Bước 4: `28.0°C` (Đã làm mát xuống dưới ngưỡng trễ `<= 28.5°C` $\rightarrow$ Mạch 1 yêu cầu Mạch 2 **TẮT Quạt**).
   - Cập nhật `scenario dwell` dùng 1 cảm biến `sensor_temp_01` duy nhất, thử thách khóa bảo vệ máy nén 180s.
2. **[sim_rx_monitor.c](file:///home/chien/ai-workspace/qwen-iot-simulator/main/sim_rx_monitor.c)**:
   - Tự động nạp Wi-Fi đã lưu từ NVS khi khởi động.
   - Chỉ lắng nghe và hiển thị ra màn hình Serial của PC **các hành động mà Mạch 1 yêu cầu 4 thiết bị từ xa trên Mạch 2 thực hiện** (BẬT / TẮT Quạt, Máy sưởi, Máy hút ẩm, Thiết bị phụ trợ, kèm nguyên nhân: do Rule vượt ngưỡng, do thoát vùng trễ Hysteresis, do Lịch tự động, do Ngắt an toàn `COOLING_OVERRIDE` / `SAFETY_CUTOFF`, hoặc khi lệnh bật bị Mạch 1 chặn lại để bảo vệ thiết bị do `INTERLOCK` / `DWELL 180s`).

### Hạng mục 3: Cloud Backend & Web App (`qwen-test/website`)
1. **Prisma Schema & Backend Controllers ([schema.prisma](file:///home/chien/ai-workspace/qwen-test/website/backend/prisma/schema.prisma), [ruleController.js](file:///home/chien/ai-workspace/qwen-test/website/backend/src/controllers/ruleController.js), [broker.js](file:///home/chien/ai-workspace/qwen-test/website/backend/src/mqtt/broker.js))**:
   - Hỗ trợ trường `hysteresis` trong API Rules và payload đồng bộ MQTT (mặc định `1.5` cho cảm biến nhiệt độ, `2.0` cho cảm biến độ ẩm).
   - Bọc `prisma.$transaction` cho `handleScheduleSyncFromGateway` và `handleRulesSyncFromGateway`.
   - Sửa `handleTelemetryBatch` ghi dữ liệu bù thật vào `prisma.telemetryLog` (chống trùng lặp theo timestamp).
   - Tự động gọi `publishScheduleConfig` (đẩy cả `timezone` và `ble_sensors`) khi cập nhật cài đặt ở `gatewayController.js` hoặc thiết bị BLE ở `deviceController.js`.
2. **Giao diện Web App ([Rules.jsx](file:///home/chien/ai-workspace/qwen-test/website/web-app/src/pages/Rules.jsx))**:
   - Trên mỗi Rule Card, hiển thị rõ huy hiệu **Vùng trễ bảo vệ (Hysteresis)** và ngưỡng tự động tắt (VD: *"NẾU Nhiệt độ > 30°C THÌ Quạt = ON • Bảo vệ chống bật/tắt liên tục: giữ ON cho tới khi $\le 28.5^\circ\text{C}$ (Độ trễ $1.5^\circ\text{C}$)"*).
   - Trong Modal Tạo/Sửa Rule, cho phép người dùng xem và tùy chỉnh độ trễ Hysteresis.

---

## 4. Bộ Tiêu Chí Đánh Giá Kết Quả Định Lượng & Chống Tự Thôi Miên (Strict Acceptance & Anti-Hypnosis Criteria)

### 4.0. Năm Quy Tắc Cấm Tuyệt Đối (Zero-Hypnosis Guardrails — Vi phạm 1 điều = FAIL toàn bộ)
1. **CẤM Mock / Giả lập phần cứng bằng script**: Không được dùng script Python tạo `VirtualGateway` hay bơm giả MQTT từ PC thay cho Mạch 1 và Mạch 2. Mọi gói tin cảm biến trong bài nghiệm thu phải phát qua sóng BLE từ Mạch 2 sang Mạch 1.
2. **CẤM Hardcode dữ liệu hiển thị**: Màn hình OLED (Screen 0, 1, 2), Web Cục bộ (`192.168.1.198`) và Web Cloud không được chứa bất kỳ chuỗi lịch hẹn, nhiệt độ (`31.50°C`) hay dung lượng thẻ nhớ (`1000000 KB`) cố định nào.
3. **CẤM "Nuốt" dữ liệu ở Backend**: Mọi dữ liệu cảm biến (kể cả dữ liệu đồng bộ bù `telemetry/batch` sau khi mất mạng) bắt buộc phải tạo bản ghi thực trong bảng `TelemetryLog` của PostgreSQL và vẽ lên đồ thị, không được chỉ ghi text vào `EventLog` để đối phó.
4. **CẤM Bỏ qua lỗi biên dịch hoặc xóa bài test khó**: Bộ test C Native (`make -C test/host`), Pytest (`pytest tests/host/`), Backend Jest (`npm test`), và Web Build (`npm run build`) phải đạt **100% PASS với 0 lỗi biên dịch**.
5. **CẤM In log rác trên Serial Mạch 2**: Màn hình Serial của Mạch 2 (`sim>`) chỉ được phép hiển thị trạng thái của chính Mạch 2 (Wi-Fi, BLE TX) và **các lệnh điều khiển thiết bị từ xa mà Mạch 1 yêu cầu Mạch 2 thực thi** (`relay_1..4` / `fan_01`, `heater_01`, `dehumidifier_01`, `aux_01`). Không in log báo thức nội bộ hay log debug thừa của Mạch 1.

---

### 4.1. Tiêu Chí Kỹ Thuật & Tài Nguyên Định Lượng Cho Từng Mạch

#### A. Mạch 1 — Smart Clock & Edge IoT Gateway (`qwen-test/firmware`)
| Mã tiêu chí | Hạng mục kiểm tra | Ngưỡng định lượng bắt buộc (Pass Threshold) | Cách đo kiểm thực tế | Điều kiện đánh FAIL ngay lập tức |
|:---:|---|---|---|---|
| **AC-GW-01** | **Biên dịch & Ngân sách Bộ nhớ** | - `make -C test/host`: **9/9 PASS**.<br>- `pytest tests/host/`: **4/4 PASS**.<br>- `idf.py build`: **0 error**, binary `< 1.60 MB` (`> 20%` free trên phân vùng 2MB).<br>- RAM `free_heap` khi chạy đủ Wi-Fi + BLE + MQTTS + HTTP + SD: **`>= 40,000 bytes`**. | Chạy lệnh build & kiểm tra trường `"free_heap"` từ `GET http://<gw_ip>/api/status`. | Lỗi compile `test/host`; binary vượt 1.65MB; hoặc `free_heap` tụt dưới 35KB / rò rỉ bộ nhớ sau 10 phút chạy. |
| **AC-GW-02** | **Màn hình OLED SSD1327 & Đồng hồ RTC DS3231** | - **Screen 0**: Hiển thị giờ lệch `< 2 giây` so với giờ chuẩn NTP; cập nhật `Temp` & `Humi` trong vòng **`<= 3 giây`** sau khi Mạch 2 phát BLE.<br>- **Screen 1 (`UPCOMING EVENTS`)**: Hiển thị tối đa **3 lịch `enabled=true` gần nhất** từ `scheduler.c`. Khi thêm/xóa lịch trên Web, OLED cập nhật khớp **`100%` trong `<= 3 giây`**.<br>- **Screen 2 (`RELAY STATUS`)**: Khớp **`100%`** với trạng thái 4 chân GPIO 15..18 (`ON`/`OFF`). | Quan sát trực tiếp màn hình OLED vật lý + đối chiếu `GET /api/schedules` và `GET /api/telemetry`. | Screen 1 vẫn hiện chữ cứng `06:30 Wakeup`; hoặc đổi Timezone trên Web mà giờ trên OLED không đổi theo. |
| **AC-GW-03** | **Cảm biến BLE Đa chỉ số (Nhiệt + Ẩm trên 1 MAC)** | - Nhận gói BLE (`0x181A` & `BTHome 0xFCD2`) từ MAC `FA:BB:CC:11:22:33` của Mạch 2.<br>- Bộ lọc 2s cho qua ngay khi `|ΔT| >= 0.2°C` **HOẶC** `|ΔH| >= 0.5%RH`.<br>- Cùng 1 gói BLE từ `sensor_temp_01` kích hoạt được **cả Rule Nhiệt độ lẫn Rule Độ ẩm**. | Trên Serial Mạch 2, giữ nguyên `temp=26.0`, chỉ gõ `set hum 80.0` $\rightarrow$ Mạch 1 phải nhận được `hum=80.0%` và kích hoạt Rule bật Máy hút ẩm (`relay_3`). | Mạch 1 bỏ qua gói BLE khi chỉ đổi độ ẩm; hoặc Rule độ ẩm không chạy vì khác tên `sensor_hum_01`. |
| **AC-GW-04** | **Bảo vệ Thiết bị: Hysteresis, Cooldown 60s, Dwell 180s, Interlock** | - **Hysteresis**: Bật Quạt ở `>= 30.0°C`; giữ BẬT khi giảm về `29.0°C`; chỉ TẮT khi `<= 28.5°C` (`H = 1.5°C`).<br>- **Cooldown**: Không kích hoạt BẬT lại cùng 1 Rule trong vòng **`60 giây`**.<br>- **Dwell Time**: Sau khi `relay_3` (Hút ẩm/Điều hòa) TẮT, khóa cứng không cho BẬT lại trong **`180 giây`** (trả mã lỗi `error_code = 4`).<br>- **Interlock**: Khi Quạt (`relay_1`) đang ON, từ chối bật Sưởi (`relay_2`) với `error_code = 3`. Khi Sưởi đang ON mà bật Quạt $\rightarrow$ ngắt Sưởi tức thì (`0ms`, `COOLING_OVERRIDE`) và khóa Sưởi **`300 giây`**. | Chạy kịch bản từ Serial Mạch 2 + kiểm tra phản hồi lệnh điều khiển trên Mạch 2 và trạng thái GPIO/Web. | Quạt bị tắt ngay khi nhiệt độ vừa tụt xuống `29.8°C`; hoặc Máy hút ẩm bật lại được ngay sau khi vừa tắt `< 180s`. |
| **AC-GW-05** | **Thẻ nhớ MicroSD, Hotplug & Đồng bộ bù (Backfill)** | - File `/sdcard/data/telemetry/YYYY-MM-DD.csv` và `sync_queue.bin` ghi **cả `temperature` và `humidity`**.<br>- Rút thẻ SD: đệm tối đa **50 bản tin** vào RAM (`ram_fallback_count` tăng). Cắm lại thẻ: tự động xả xuống SD.<br>- Mất MQTT: `sync_queue_unacked` tăng đúng theo số gói BLE. Có MQTT lại: đẩy `telemetry/batch` và đưa `sync_queue_unacked` về **`0`**. | Kiểm tra qua `GET /api/status` (`sd_mounted`, `sync_queue_total`, `sync_queue_unacked`, `ram_fallback_count`) và đọc thẻ nhớ. | File CSV/sync_queue thiếu cột độ ẩm; hoặc `unacked_count` giảm về 0 khi MQTT chưa gửi thành công. |
| **AC-GW-06** | **Web Cục Bộ LAN (`http://192.168.1.198/`)** | - Truy cập trực tiếp qua IP LAN (khi rút dây Internet router): Hiển thị và thao tác CRUD đầy đủ cho **4 Relays**, **Rules (có ô Hysteresis)**, **Schedules/Alarms (thêm/xóa lịch, tắt chuông)** và **Wi-Fi**. | Mở `http://192.168.1.198/` trên trình duyệt, kiểm tra `node --check` 0 lỗi cú pháp JS, thử thêm/xóa lịch và rule. | Lỗi JS `SyntaxError` trên trình duyệt; thiếu bảng Schedules hoặc thiếu thông số Hysteresis trên giao diện cục bộ. |

---

#### B. Mạch 2 — Thiết Bị IoT Kết Nối Từ Xa (`qwen-iot-simulator` — Cắm cố định vào PC)
| Mã tiêu chí | Hạng mục kiểm tra | Ngưỡng định lượng bắt buộc (Pass Threshold) | Cách đo kiểm thực tế | Điều kiện đánh FAIL ngay lập tức |
|:---:|---|---|---|---|
| **AC-SIM-01** | **Cấu hình Wi-Fi qua Serial & Tự động nhớ NVS** | - Lệnh `wifi scan` trả về danh sách mạng Wi-Fi trong **`<= 4 giây`**.<br>- Lệnh `wifi connect "<ssid>" <pass>` hỗ trợ SSID có dấu cách, lưu vào NVS Flash.<br>- Sau khi bấm nút RST hoặc rút/cắm lại USB, Mạch 2 tự động kết nối lại Wi-Fi và MQTTS EMQX trong **`<= 10 giây`** mà không cần gõ lại lệnh. | Gõ `wifi scan`, `wifi connect` trên Serial PC, sau đó gõ `reboot` và quan sát Mạch 2 tự lên `WIFI CONNECTED` & `MQTT CONNECTED`. | Reset mạch xong mất Wi-Fi phải gõ lại; hoặc SSID có khoảng trắng không kết nối được. |
| **AC-SIM-02** | **Điều khiển Cảm biến BLE từ xa qua Serial** | - Lệnh `set temp <val>`, `set hum <val>`, `send <temp> <hum>` cập nhật gói BLE (`FA:BB:CC:11:22:33`) ngay lập tức (`< 100ms`) và in xác nhận rõ ràng ra Serial.<br>- Kịch bản `scenario cooling`, `scenario interlock`, `scenario dwell` chạy tự động trơn tru trên cùng 1 cảm biến `sensor_temp_01`. | Gõ từng lệnh trên Serial Mạch 2 (`sim>`) và quan sát phản hồi. | `scenario dwell` vẫn phải đổi tên thiết bị sang `sensor_hum_01` hoặc phải cộng giả nhiệt độ mới chạy được. |
| **AC-SIM-03** | **Hiển thị Hành động Thiết bị Từ Xa trên Serial** | - Khi Mạch 1 ra lệnh BẬT/TẮT thiết bị (do Rule vượt ngưỡng, do thoát vùng trễ Hysteresis, do Lịch hẹn giờ, do Ngắt bảo vệ `COOLING_OVERRIDE`/`SAFETY_CUTOFF`, hoặc từ Web), màn hình Serial Mạch 2 phải in thông báo hành động trong **`<= 1.5 giây`**.<br>- Khi lệnh điều khiển bị Mạch 1 chặn để bảo vệ thiết bị (`INTERLOCK` hoặc `DWELL 180s`), Serial Mạch 2 in rõ thông báo bảo vệ. | Quan sát cửa sổ Serial Mạch 2 trong suốt quá trình test Rule, Lịch và Điều khiển Web. | Serial Mạch 2 không in gì khi Mạch 1 ngắt sưởi do `COOLING_OVERRIDE`; hoặc in lẫn lộn log báo thức nội bộ của Mạch 1. |

---

#### C. Cloud Backend & Web App (`qwen-test/website`)
| Mã tiêu chí | Hạng mục kiểm tra | Ngưỡng định lượng bắt buộc (Pass Threshold) | Cách đo kiểm thực tế | Điều kiện đánh FAIL ngay lập tức |
|:---:|---|---|---|---|
| **AC-CLOUD-01** | **Hiển thị & Cấu hình Vùng trễ Bảo vệ (Hysteresis)** | - Trang `Rules.jsx` hiển thị rõ cả **Ngưỡng kích hoạt (`Threshold`)**, **Độ trễ bảo vệ (`Hysteresis`)**, và **Ngưỡng tự động tắt** (VD: Bật `>= 30.0°C`, Tự tắt `<= 28.5°C`).<br>- Khi tạo/sửa Rule với Hysteresis tùy chỉnh trên Web, giá trị Hysteresis được lưu vào DB và đồng bộ xuống Mạch 1 trong **`<= 3 giây`**. | Tạo/sửa Rule trên Web App, sau đó gọi `GET http://<gw_ip>/api/rules` trên Mạch 1 kiểm tra trường `"hysteresis"`. | Giao diện Web không có ô Hysteresis; hoặc chỉnh trên Web nhưng xuống Mạch 1 vẫn bị reset về giá trị mặc định. |
| **AC-CLOUD-02** | **Đồng bộ Cài đặt Timezone & Danh sách Cảm biến BLE** | - Đổi Múi giờ ở `Settings.jsx` (VD: từ `Asia/Ho_Chi_Minh` sang `Asia/Tokyo` `JST-9`) $\rightarrow$ Đồng hồ trên Mạch 1 nhảy sang múi giờ mới trong **`<= 3 giây`**.<br>- Thêm/sửa cảm biến BLE (kèm MAC) ở `Devices.jsx` $\rightarrow$ Mạch 1 nhận được danh sách MAC mới qua MQTT. | Thao tác trên Web App và kiểm tra `GET http://<gw_ip>/api/status` (`time`) trên Mạch 1. | Bấm Lưu trên Web báo thành công nhưng Mạch 1 không nhận được bản tin MQTT nào. |
| **AC-CLOUD-03** | **Đồng bộ bù Dữ liệu Lịch sử (`TelemetryLog` Backfill)** | - Khi Mạch 1 đẩy `N` bản ghi qua `gateway/+/telemetry/batch`, bảng `TelemetryLog` trên PostgreSQL tăng thêm đúng `N` cặp bản ghi (`°C` và `%`) với đúng `recordedAt` là thời điểm đo lúc mất mạng, và biểu đồ trên `Dashboard.jsx` / `Devices.jsx` vẽ đầy đủ các điểm dữ liệu đó. | Gọi `GET /api/v1/gateways/gw_smartclock_01/devices/sensor_temp_01/telemetry` trước và sau khi backfill để đếm số bản ghi thực trong DB. | `handleTelemetryBatch` chỉ ghi vào `EventLog` mà số bản ghi trong `TelemetryLog` không tăng. |

---

## 5. Ma Trận 8 Kịch Bản Nghiệm Thu Liên Thông Thực Chiến (End-to-End Demo & Verification Matrix)

Đây là **8 kịch bản kiểm chứng thực tế** bạn sẽ chạy khi nghiệm thu code (và cũng là kịch bản trình diễn trước Hội đồng bảo vệ đồ án):

| Kịch bản | Thao tác thực hiện (Operator Action) | Kết quả bắt buộc trên Mạch 1 (Gateway + OLED) | Kết quả bắt buộc trên Serial Mạch 2 (PC Console) | Kết quả bắt buộc trên Web App / Cloud DB |
|:---:|---|---|---|---|
| **Gate 1: Khởi động & Kết nối** | Cấp nguồn Mạch 1; cắm Mạch 2 vào USB PC, gõ `status` trên Mạch 2. | OLED Screen 0 sáng `< 1.5s`, hiện đúng giờ DS3231, `WF MQ` sáng. `free_heap >= 40KB`. | Hiển thị `BLE TX: ACTIVE (MAC=FA:BB:CC:11:22:33)`, `Wi-Fi: CONNECTED`, `MQTT: CONNECTED`. | Gateway `gw_smartclock_01` hiện trạng thái **Online** màu xanh. |
| **Gate 2: Chống bật/tắt liên tục quanh 30°C (Hysteresis)** | Trên Serial Mạch 2 gõ lần lượt:<br>1. `set temp 30.5`<br>2. `set temp 29.5` *(dao động dưới 30°C)*<br>3. `set temp 28.2` *(mát dưới 28.5°C)* | 1. OLED hiện `30.5 C`; GPIO 15 (Quạt) lên `HIGH (ON)`.<br>2. OLED hiện `29.5 C`; GPIO 15 **GIỮ NGUYÊN `ON`**.<br>3. OLED hiện `28.2 C`; GPIO 15 xuống `LOW (OFF)`. | 1. In: `[THIẾT BỊ TỪ XA: QUẠT] Nhận lệnh từ Gateway -> BẬT (ON) (Rule #1: 30.5C >= 30.0C)`.<br>2. **Không nhận lệnh tắt** (Quạt vẫn `ON` nhờ vùng trễ Hysteresis).<br>3. In: `[THIẾT BỊ TỪ XA: QUẠT] Nhận lệnh từ Gateway -> TẮT (OFF) (Hạ dưới ngưỡng trễ <= 28.5C)`. | Đồ thị nhiệt độ vẽ 3 điểm `30.5 -> 29.5 -> 28.2°C`. Công tắc Quạt trên Dashboard chuyển `ON` $\rightarrow$ giữ `ON` $\rightarrow$ chuyển `OFF`. |
| **Gate 3: Đa chỉ số (Độ ẩm) & Khóa bảo vệ máy nén 180s (Dwell Time)** | Trên Serial Mạch 2 gõ:<br>1. `set hum 80.0`<br>2. `set hum 70.0`<br>3. Ngay lập tức (< 180s) gõ `set hum 82.0` hoặc bấm bật Máy hút ẩm trên Web. | 1. OLED hiện `Humi: 80.0 %`; GPIO 17 (Hút ẩm) `ON`.<br>2. GPIO 17 `OFF` (dưới ngưỡng `75 - 2 = 73%`).<br>3. GPIO 17 **BỊ KHÓA Giữ `OFF`** (chưa đủ 180s Dwell Time). | 1. In: `[THIẾT BỊ TỪ XA: MÁY HÚT ẨM] -> BẬT (ON) (Rule #3: 80.0% >= 75.0%)`.<br>2. In: `[THIẾT BỊ TỪ XA: MÁY HÚT ẨM] -> TẮT (OFF) (<= 73.0%)`.<br>3. In: `[BẢO VỆ THIẾT BỊ] Gateway chặn lệnh BẬT Máy hút ẩm (Khóa bảo vệ lốc nén 180s)`. | Trang Events ghi nhận sự kiện `compressor_dwell_lock` (`error_code: 4`). Công tắc Máy hút ẩm trên Web giữ trạng thái `OFF`. |
| **Gate 4: Khóa liên động bất đối xứng Quạt - Sưởi (Interlock)** | 1. Trên Serial Mạch 2 gõ `set temp 19.0` (Sưởi BẬT).<br>2. Gõ tiếp `set temp 32.0` (Quá nhiệt đột ngột).<br>3. Thử bấm bật lại Sưởi trên Web. | 1. GPIO 16 (Sưởi) `ON`.<br>2. GPIO 16 ngắt tức thì `0ms` (`COOLING_OVERRIDE`), GPIO 15 (Quạt) `ON`, khóa Sưởi `300s`.<br>3. Từ chối lệnh bật Sưởi (`error_code: 3`). | 1. In: `[THIẾT BỊ TỪ XA: MÁY SƯỞI] -> BẬT (ON)`.<br>2. In: `[BẢO VỆ KHẨN CẤP] Máy sưởi -> TẮT NGAY (COOLING_OVERRIDE)` & `[THIẾT BỊ TỪ XA: QUẠT] -> BẬT (ON)`.<br>3. In: `[BẢO VỆ THIẾT BỊ] Gateway từ chối bật Sưởi khi Quạt đang chạy`. | Trang Events hiển thị `COOLING_OVERRIDE` và `safety_interlock`. |
| **Gate 5: Lịch biểu thật trên OLED Screen 1 & Đồng bộ 2 chiều** | 1. Bấm nút BOOT trên Mạch 1 chuyển sang Screen 1 (`UPCOMING EVENTS`).<br>2. Trên Web App (`Schedules.jsx`), tạo lịch mới `20:30 - Tắt Đèn Phụ Trợ (aux_01 = OFF)`. | Screen 1 trên OLED lập tức cập nhật và hiển thị dòng lịch thật `20:30` vừa tạo (lưu xuống `/sdcard/schedule.json`). | Khi đến giờ hẹn của lịch `automation`, Serial Mạch 2 in: `[THIẾT BỊ TỪ XA: PHỤ TRỢ (aux_01)] Nhận lệnh hẹn giờ từ Gateway -> TẮT (OFF)`. | Lịch mới hiển thị đồng bộ trên cả Web Cloud (`Schedules.jsx`) và Web Cục bộ (`http://192.168.1.198/`). |
| **Gate 6: Đồng bộ Múi giờ & Cảm biến từ Web xuống Mạch 1** | Trên trang `Settings.jsx`, đổi Múi giờ sang `UTC` (hoặc `Asia/Tokyo`) và bấm Lưu. | Đồng hồ trên OLED Screen 0 của Mạch 1 lập tức đổi giờ theo múi giờ mới trong `<= 3 giây`. | Không ảnh hưởng/không in rác trên Serial Mạch 2. | `Settings.jsx` và `Dashboard.jsx` hiển thị múi giờ mới khớp với Mạch 1. |
| **Gate 7: Mất mạng & Đồng bộ bù Lịch sử (`sync_queue.bin` Backfill)** | 1. Ngắt MQTT trên Mạch 1 (`POST /api/mqtt/control {"action":"disconnect"}`).<br>2. Trên Mạch 2 gõ `send 27.5 64.0` và `send 28.1 66.0`.<br>3. Nối lại MQTT (`{"action":"reconnect"}`). | 1. Khi mất MQTT, `sync_queue_unacked` tăng lên `>= 2`.<br>2. Khi nối lại MQTT, Mạch 1 đẩy `telemetry/batch` và `sync_queue_unacked` về `0`. | Mạch 2 vẫn phát BLE bình thường. | Bảng `TelemetryLog` trên Cloud có thêm đúng các bản ghi `27.5°C / 64.0%` và `28.1°C / 66.0%` và vẽ lên biểu đồ. |
| **Gate 8: Vận hành Cục bộ Không cần Internet (Local Web UI)** | Mở trực tiếp `http://192.168.1.198/` trên trình duyệt LAN. Thử bật/tắt Relay, sửa Hysteresis của Rule, thêm/xóa Schedule. | Mạch 1 thực thi ngay lập tức, lưu xuống thẻ nhớ MicroSD `/sdcard/rules.json` & `/sdcard/schedule.json`. | Mạch 2 nhận và hiển thị ngay lệnh bật/tắt thiết bị từ Mạch 1. | Khi có Internet, thay đổi từ Web Cục bộ tự động đồng bộ ngược lên Web Cloud (`schedule/sync`, `rules/sync`). |
