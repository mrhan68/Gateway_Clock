# Kế Hoạch Triển Khai: Đồng Bộ 2 Chiều Lịch Trình (`Schedules`) & Luật Tự Động Hóa (`Rules`) Giữa Cloud, Gateway ESP32-S3 và Thẻ Nhớ MicroSD

> **Trạng thái**: Đã chốt thiết kế với người dùng — Chờ triển khai (`/swarm`)
> **Ngày lập**: 2026-11-13
> **Mục tiêu**: Khắc phục đứt gãy đồng bộ `config/schedule` giữa Cloud Backend (Render + EMQX), Web Dashboard và Firmware ESP32-S3; bổ sung lưu trữ bền vững trên thẻ nhớ MicroSD (`/sdcard/schedule.json`, `/sdcard/rules.json`) để tự động nạp lại mỗi lần khởi động mạch.

---

## 1. Các Quyết Định Thiết Kế Đã Chốt

1. **Phạm vi đồng bộ (`A1`)**:
   - Đồng bộ đầy đủ cả **Schedules (Báo thức, Hẹn giờ tự động hóa, Sự kiện cá nhân)** và **Automation Rules** qua MQTT.
   - Tự động đồng bộ **3 Rule mặc định của FSD** (`Rule #1`: Quạt `> 30.0°C`, `Rule #2`: Sưởi `< 22.0°C`, `Rule #3`: Hút ẩm `> 75.0%`) và **2 Schedule mặc định** lên Cloud PostgreSQL nếu DB của Gateway đang trống, đảm bảo Web Dashboard và Mạch khớp nhau 100%.
2. **Lưu trữ bền vững trên Thẻ nhớ MicroSD (`A2`)**:
   - Lưu trực tiếp vào **`/sdcard/schedule.json`** và **`/sdcard/rules.json`** trên thẻ nhớ MicroSD (kèm bản sao dự phòng tại `/spiffs/schedule.json` và `/spiffs/rules.json`).
   - Trong chuỗi khởi động 17 bước của `main.c` (`Bước 9: sd_logger_init` chạy trước `Bước 13: rule_engine_init` và `Bước 14: scheduler_init`), mạch luôn ưu tiên đọc từ `/sdcard/schedule.json` và `/sdcard/rules.json` mỗi khi cấp nguồn hoặc reboot.
3. **Đồng bộ 2 chiều (`A3`)**:
   - **Chiều Cloud $\rightarrow$ Gateway**: Khi Thêm / Sửa / Bật-Tắt / Xóa lịch trình hoặc luật trên Web Dashboard, Backend xuất bản gói JSON retained (`qos: 1, retain: true`) xuống `gateway/<gw_id>/config/schedule`.
   - **Chiều Gateway $\rightarrow$ Cloud**:
     - Khi thêm/sửa/xóa lịch trình hoặc luật từ Serial CLI (`schedule add`, `rule add`...) hoặc trang Web nội bộ (`192.168.4.1` / `192.168.1.198`), Gateway lưu vào `/sdcard/` và publish lên `gateway/<gw_id>/schedule/sync` hoặc `gateway/<gw_id>/rules/sync` để Backend cập nhật vào PostgreSQL và làm mới bản tin retained.
     - Khi đến giờ báo thức kêu trên mạch (`EVT_SCHEDULE_ALARM_TRIGGERED`), Gateway publish lên `gateway/<gw_id>/alarm` và Backend ghi nhận vào bảng `event_logs` (`eventType: 'alarm_triggered'`) để hiển thị trên trang `/events`.

---

## 2. Chi Tiết Thay Đổi Theo Từng Thành Phần

### Phần A: Cloud Backend (`website/backend/`)

1. **`website/backend/src/mqtt/broker.js`**:
   - **Hoàn thiện `publishScheduleConfig(gatewayId)`** (hiện đang là hàm stub `return true`):
     - Kiểm tra nếu `prisma.automationRule.count({ where: { gatewayId } }) === 0`: tự động seed 3 luật mặc định (`sensor_temp_01 > 30 -> fan_01 ON`, `sensor_temp_01 < 22 -> heater_01 ON`, `sensor_hum_01 > 75 -> dehumidifier_01 ON`).
     - Kiểm tra nếu `prisma.schedule.count({ where: { gatewayId } }) === 0`: tự động seed 2 lịch mặc định (`07:00 Good Morning Alarm` và `18:00 Office Auto AC Off`).
     - Truy vấn toàn bộ `schedules` và `automationRules` của `gatewayId`.
     - Chuẩn hóa trường `repeat_days` (`"MON,TUE..."` $\rightarrow$ mảng chỉ số `[1,2,...]` tương thích `tm_wday` `0=Sun..6=Sat`) và ánh xạ `target_device_id` qua `relayAliases.js` (`fan_01` $\rightarrow$ `relay_1`, v.v.).
     - Xuất bản JSON `{ schedules, rules, updated_at }` tới topic `gateway/${gatewayId}/config/schedule` với `{ qos: 1, retain: true }`.
   - **Subscribe các topic Uplink từ Gateway trong `startBroker()`**:
     - `gateway/+/alarm` $\rightarrow$ `handleAlarmEvent(gatewayId, payload)`: Ghi `prisma.eventLog.create({ eventType: 'alarm_triggered', ... })`.
     - `gateway/+/schedule/sync` $\rightarrow$ `handleScheduleSyncFromGateway(gatewayId, payload)`: Đồng bộ danh sách lịch trình từ Gateway lên bảng `schedules` trong PostgreSQL và cập nhật lại retained topic `config/schedule`.
     - `gateway/+/rules/sync` $\rightarrow$ `handleRulesSyncFromGateway(gatewayId, payload)`: Đồng bộ danh sách luật từ Gateway lên bảng `automation_rules` trong PostgreSQL và cập nhật lại retained topic `config/schedule`.

2. **`website/backend/src/controllers/scheduleController.js` & `ruleController.js`**:
   - Trong `listSchedules` và `listRules`: nếu DB của `gateway_id` chưa có bản ghi nào lần đầu tiên, gọi hàm khởi tạo seed mặc định và kích hoạt `broker.publishScheduleConfig(gateway_id)` để đồng bộ ngay.

3. **`website/web-app/src/pages/Events.jsx`**:
   - Bổ sung cấu hình hiển thị badge & icon cho `alarm_triggered` (Báo thức kích hoạt).

---

### Phần B: Firmware Gateway ESP32-S3 (`firmware/main/`)

1. **`firmware/main/scheduler.h` & `firmware/main/scheduler.c`**:
   - Định nghĩa đường dẫn:
     - `#define SD_SCHEDULE_PATH "/sdcard/schedule.json"`
     - `#define SPIFFS_SCHEDULE_PATH "/spiffs/schedule.json"`
   - Viết hàm `save_schedules_to_storage()` và `load_schedules_from_storage()`:
     - Khi đọc (`load_schedules_from_storage()`): Kiểm tra nếu `sd_logger_is_mounted()` và `/sdcard/schedule.json` tồn tại hợp lệ thì đọc từ thẻ SD; nếu không thì đọc từ `/spiffs/schedule.json`; nếu chưa có cả hai thì gọi `load_default_schedules()` và ghi khởi tạo ra thẻ SD + SPIFFS.
     - Khi ghi (`save_schedules_to_storage()`): So sánh chuỗi JSON mới với file hiện hành để tránh ghi lặp (chống mòn thẻ SD/Flash). Thực hiện ghi nguyên tử (`.tmp` $\rightarrow$ `unlink` $\rightarrow$ `rename`) vào `/sdcard/schedule.json` (nếu thẻ SD đang gắn) và `/spiffs/schedule.json`.
   - Bổ sung hàm `scheduler_apply_cloud_json(cJSON *schedules_arr)` để cập nhật danh sách lịch trình nhận từ MQTT Downlink mà không kích hoạt vòng lặp gửi ngược lên Cloud.
   - Khi thêm/xóa lịch từ local (`scheduler_add_entry`, `scheduler_clear_entries`), tự động gọi `save_schedules_to_storage()` và đẩy bản tin đồng bộ lên `gateway/<gw_id>/schedule/sync` nếu thay đổi xuất phát từ CLI/Local.

2. **`firmware/main/rule_engine.h` & `firmware/main/rule_engine.c`**:
   - Mở rộng `load_rules_from_storage()` và `save_rules_to_storage()`:
     - Ưu tiên đọc/ghi `/sdcard/rules.json` khi thẻ nhớ MicroSD được mount (`sd_logger_is_mounted()`), đồng thời duy trì bản sao tại `/spiffs/rules.json`.
   - Bổ sung hàm `rule_engine_apply_cloud_json(cJSON *rules_arr)` (chuẩn hóa alias `fan_01..aux_01` $\rightarrow$ `relay_1..4`, giữ nguyên `hysteresis` và `cooldown_s` nếu bản tin Cloud không chỉ định).
   - Khi thêm/xóa rule từ CLI hoặc Local Web (`/api/rules`), lưu vào `/sdcard/rules.json` và publish lên `gateway/<gw_id>/rules/sync`.

3. **`firmware/main/mqtt_adapter.c`**:
   - Trong `MQTT_EVENT_DATA`:
     - Thay thế buffer tĩnh `char data[256]` bằng cấp phát động theo `event->data_len + 1` (giới hạn an toàn $\le 8192$ bytes) để không làm cụt gói JSON cấu hình.
     - Bổ sung nhánh xử lý khi `topic` kết thúc bằng `/config/schedule`:
       - Parse JSON gốc, lấy mảng `schedules` truyền vào `scheduler_apply_cloud_json(schedules)` và mảng `rules` truyền vào `rule_engine_apply_cloud_json(rules)`.

---

## 3. Kế Hoạch Kiểm Thử & Nghiệm Thu (Verification Plan)

1. **Automated Unit Tests**:
   - `cd website/backend && npm test`: Viết thêm unit test trong `mqtt_handlers.test.js` kiểm tra `publishScheduleConfig` (retained QoS 1), `handleAlarmEvent`, `handleScheduleSyncFromGateway`, `handleRulesSyncFromGateway`.
   - `pytest tests/host/`: Đảm bảo toàn bộ host tests xanh 100%.
2. **Biên dịch & Nạp Firmware (`/dev/ttyACM0`)**:
   - `cd firmware && idf.py build && idf.py -p /dev/ttyACM0 flash`.
   - Kiểm tra dung lượng phân vùng App (< 2MB) và SRAM.
3. **Kiểm thử Thực tế E2E (Hardware-in-the-Loop + Cloud)**:
   - **Kịch bản 1 (Cloud $\rightarrow$ Gateway $\rightarrow$ Thẻ nhớ SD)**: Tạo/sửa 1 lịch hẹn trên Cloud Web $\rightarrow$ xác nhận mạch nhận qua MQTT, lệnh `schedule list` trên Serial CLI hiện đúng lịch mới, và file `/sdcard/schedule.json` được ghi thành công.
   - **Kịch bản 2 (Khởi động lại mạch — Persistence trên Thẻ nhớ SD)**: Reboot mạch ESP32-S3 $\rightarrow$ xác nhận ngay khi khởi động (`[BOOT 13/17]` và `[BOOT 14/17]`), mạch tự nạp lại đúng danh sách từ `/sdcard/rules.json` và `/sdcard/schedule.json`.
   - **Kịch bản 3 (Gateway $\rightarrow$ Cloud)**: Thêm lịch hoặc kích hoạt báo thức từ Serial CLI (`schedule add` / `alarm test`) $\rightarrow$ xác nhận sự kiện báo thức và lịch mới đồng bộ ngược lên Cloud DB và trang `/events`.
