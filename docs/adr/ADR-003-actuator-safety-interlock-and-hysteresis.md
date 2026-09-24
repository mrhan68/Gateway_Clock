# ADR-003: Cơ Chế Khóa Liên Động An Toàn và Dải Trễ Điều Khiển Cơ Cấu Chấp Hành (Actuator Safety Interlock, Emergency Cutoff & Hysteresis)

- **Trạng thái:** Chấp thuận (Accepted)
- **Ngày quyết định:** 2026-09-21
- **Người đề xuất:** System Architect
- **Phạm vi kỹ thuật:** 4 Kênh Relay (GPIO 15..18), [firmware/main/relay_controller.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/relay_controller.c), [firmware/main/rule_engine.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/rule_engine.c)
- **Tài liệu tham chiếu gốc:** [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md) (§4.7, §4.10, §11.2)
- **Truy vết yêu cầu kỹ thuật:** `Traces to: FR-RELAY-01 / FR-RULE-01 / M07 / S08`

---

## 1. Bối Cảnh và Vấn Đề (Context & Problem Statement)

Gateway quản lý 4 kênh rơ-le công suất kết nối với các tải điện gia dụng nguy cơ cao:
- **Kênh 1 (GPIO 15):** Quạt thông gió / làm mát (Fan).
- **Kênh 2 (GPIO 16):** Điện trở nhiệt / Máy sưởi (Heater).
- **Kênh 3 (GPIO 17):** Máy nén / Máy hút ẩm (Dehumidifier).
- **Kênh 4 (GPIO 18):** Đèn / Tải phụ trợ (Aux).

Các nguy cơ thảm họa tiềm ẩn:
1. **Nguy cơ hỏa hoạn do quá nhiệt:** Nếu bật máy sưởi mà quạt đối lưu không quay, nhiệt tích tụ trong buồng nhiệt có thể làm chảy nhựa và gây cháy.
2. **Hỏng tiếp điểm và cháy động cơ do đóng cắt liên tục (Chattering):** Nếu nhiệt độ môi trường dao động quanh ngưỡng cài đặt ($\pm 0.1^\circ\text{C}$ do nhiễu ADC/BLE), rơ-le sẽ đóng ngắt liên tục vài giây một lần, gây hồ quang điện làm hàn dính tiếp điểm rơ-le hoặc cháy động cơ máy nén.
3. **Mất an toàn khi rớt mạng hoặc lỗi logic Cloud:** Nếu Cloud gửi lệnh BẬT máy sưởi rồi bị mất mạng Internet, thanh nhiệt có thể bị cấp điện liên tục nhiều ngày.
4. **Mất tín hiệu cảm biến:** Cảm biến hết pin hoặc mất sóng khiến Gateway không nắm được nhiệt độ thực tế nhưng vẫn duy trì sưởi ấm.

---

### 2. Quyết Định Kiến Trúc (Decision)

Chúng tôi quyết định áp dụng **Kiến Trúc An Toàn Cơ Cấu Chấp Hành Đa Tầng Bất Đối Xứng (Asymmetric Multi-Layer Actuator Safety Architecture)**:

```
                                  Ngõ Vào Lệnh Điều Khiển
                                (Cloud MQTT / Local Rules)
                                            │
                                            ▼
           ┌─────────────────────────────────────────────────────────────────┐
           │ 1. Khóa Chu Kỳ Ngắn Có Chọn Lọc (Selective Anti-Short-Cycle)   │
           │    - CHỈ áp dụng Dwell Time 180s cho Relay 3 (Máy nén lạnh)    │
           │      [Chưa đủ 180s Monotonic] ──► Từ chối (ERR_RELAY_DWELL_LOCKED│
           │    - Relay 1 (Quạt) & Relay 4: Debounce <= 1s (Bỏ Dwell 180s)   │
           │    - Relay 2 (Sưởi): Bỏ Dwell 180s thường (chỉ Lockout 300s)    │
           └────────────────────────────────┬────────────────────────────────┘
                                            │ [Hợp lệ]
                                            ▼
           ┌─────────────────────────────────────────────────────────────────┐
           │ 2. Khóa Liên Động Bất Đối Xứng (Asymmetric Interlock Rule)      │
           │    - Quạt (Relay 1) và Sưởi (Relay 2) KHÔNG BAO GIỜ cùng ON    │
           │    - Lệnh BẬT Sưởi khi Quạt ON: TỪ CHỐI (ERR_INTERLOCK_CONFLICT)│
           │      (success = false, error_code = 3, KHÔNG tự ý tắt Quạt)     │
           │    - Lệnh BẬT Quạt khi Sưởi ON: ƯU TIÊN GIẢI NHIỆT (Override)  │
           │      (Ngắt Sưởi tức thì 0ms, lý do "COOLING_OVERRIDE", bật Quạt)│
           └────────────────────────────────┬────────────────────────────────┘
                                            │ [Hợp lệ]
                                            ▼
           ┌─────────────────────────────────────────────────────────────────┐
           │ 3. Giới Hạn Cưỡng Chế Monotonic (Emergency Cutoff & Lockout)    │
           │    - Relay 2 (Sưởi): Tối đa 30m -> Cutoff 0ms, Lockout 300s     │
           │    - Relay 1 (Quạt): Tối đa 4h; Relay 3 (Nén): Tối đa 60m       │
           │    - Cảm biến lỗi / Offline > 60s: Cutoff Sưởi 0ms, Lockout 300s│
           │    - Toàn bộ timer bảo vệ dùng esp_timer_get_time() (Monotonic) │
           └────────────────────────────────┬────────────────────────────────┘
                                            │
                                            ▼
                               Kích hoạt chân GPIO 15..18
```

1. **Phần cứng Fail-Safe mặc định và Tách biệt Hai Giai đoạn Khởi tạo:**
   - Mạch driver rơ-le được thiết kế theo logic **Active HIGH** với điện trở kéo xuống (Pull-down $10\,\text{k}\Omega$) cố định trên các chân GPIO 15..18.
   - **Giai đoạn 1 (Boot Step 2 — TRƯỚC Event Bus):** `relay_gpio_init()` được thực thi ngay lập tức để cấu hình GPIO 15..18 thành ngõ ra mức `LOW` với trở kéo 10kΩ, triệt tiêu 100% hiện tượng xung áp trôi nổi (floating voltage) có thể kích đóng nhầm thiết bị khi chip vừa cấp điện.
   - **Giai đoạn 2 (Boot Step 5 — SAU Event Bus):** `relay_controller_init()` chỉ được gọi sau khi Event Bus queue đã tạo xong, bảo đảm đăng ký an toàn các handler sự kiện (`EVT_DEVICE_COMMAND_REQUEST`, `EVT_RULE_ACTION_REQUEST`, `EVT_SENSOR_VALUE_INVALID`, `EVT_DEVICE_OFFLINE_ALERT`) và khởi tạo các monotonic timers.
2. **Quy tắc Khóa liên động Bất đối xứng (Asymmetric Fan-Heater Interlock):**
   - **Quy tắc bất biến:** Quạt làm mát (`relay_1`) và Máy sưởi nhiệt (`relay_2`) **tuyệt đối không bao giờ được phép BẬT đồng thời**.
   - **Lệnh BẬT Sưởi khi Quạt đang ON $\rightarrow$ TỪ CHỐI (REJECT):**
     Nếu `relay_1` đang BẬT: Từ chối thực thi lệnh bật `relay_2`. Bộ điều khiển trả về Command ACK lỗi: `success = false`, `error_code = 3` (`ERR_RELAY_INTERLOCK_CONFLICT`), `message = "Cannot turn ON Heater while Fan is running"`. Giữ nguyên trạng thái Quạt, tuyệt đối **không tự ý tắt quạt** của người dùng.
   - **Lệnh BẬT Quạt khi Sưởi đang ON $\rightarrow$ ƯU TIÊN GIẢI NHIỆT (Cooling Safety Override):**
     Làm mát và chống quá nhiệt là ưu tiên an toàn cao nhất trong nhà. Nếu `relay_2` đang BẬT mà có lệnh BẬT `relay_1`: Cưỡng chế ngắt ngay lập tức `relay_2 = OFF` (0ms trễ, lý do `"COOLING_OVERRIDE"`), phát `EVT_RELAY_STATE_CHANGED`, sau đó đóng rơ-le Quạt `relay_1 = ON`.
3. **Bỏ qua Cooldown khi TẮT (Immediate Shutoff):**
   - Mọi lệnh hoặc điều kiện bảo vệ yêu cầu TẮT rơ-le (đặc biệt là Máy sưởi `relay_2`) **bắt buộc thực thi ngay lập tức (0ms trễ, bypass toàn bộ cooldown)**. Cooldown chỉ áp dụng cho chiều BẬT để chống rung tiếp điểm cơ.
4. **Phân định phạm vi Khóa chu kỳ ngắn (Selective Anti-Short-Cycle Dwell Time):**
   - **CHỈ áp dụng Dwell Time 180s cho Relay 3 (Máy hút ẩm / Tải máy nén lạnh - Compressor):** Khi Relay 3 vừa ngắt về OFF, bộ điều khiển kích hoạt timer monotonic khóa tối thiểu **180 giây** trước khi cho phép đóng lại kênh đó. Lệnh kích hoạt trong thời gian này bị từ chối với mã lỗi `ERR_RELAY_DWELL_LOCKED` (code 4) nhằm chống kẹt áp suất khí gas máy nén.
   - **Relay 1 (Quạt) & Relay 4 (Aux):** Bỏ Dwell Time 180s; chỉ áp dụng thời gian chống rung cơ học (Mechanical Debounce $\le 1\text{s}$).
   - **Relay 2 (Sưởi điện trở):** Bỏ Dwell Time 180s của chu kỳ thường; chỉ kích hoạt **Safety Lockout 300 giây (5 phút)** khi có sự cố ngắt khẩn cấp (`SAFETY_CUTOFF`, `SENSOR_OFFLINE`, chạy quá 30 phút).
5. **Đồng bộ Monotonic Clock (`esp_timer_get_time()`) & Khóa An Toàn 300s (Safety Lockout):**
   - Toàn bộ thời gian bảo vệ (cooldown, lockout 300s, thời gian bật tối đa 30m/4h/60m, dwell 180s máy nén) được tính toán bằng Monotonic Clock (`esp_timer_get_time()`), loại trừ 100% rủi ro do điều chỉnh múi giờ hoặc bước nhảy NTP.
   - Khi `relay_2` bị cưỡng chế TẮT do vượt quá thời gian tối đa 30 phút hoặc do cảm biến mất tín hiệu (>60s) / báo lỗi:
     - `relay_controller` xuất mức LOW ra GPIO ngay lập tức và phát sự kiện `EVT_RELAY_STATE_CHANGED` (`state = false`, `reason = "SAFETY_CUTOFF"` hoặc `"SENSOR_OFFLINE"`, kèm `monotonic_time_us`).
     - `rule_engine` bắt sự kiện này và đồng bộ lại trạng thái nội bộ: gán `current_state = false`.
     - Thiết lập thời gian khóa bảo vệ (Safety Lockout = 300s / 5 phút): `lockout_until_mono_us = esp_timer_get_time() + 300000000ULL`. Rule Engine không cho phép bất kỳ quy tắc tự động nào kích hoạt bật lại Máy sưởi trong vòng 5 phút tiếp theo để thanh nhiệt hoàn toàn nguội.
6. **Dải trễ Hysteresis Deadband ($1.5^\circ\text{C}$):**
   - Rule Engine áp dụng dải trễ $1.5^\circ\text{C}$ đối xứng xung quanh ngưỡng cài đặt (bật khi $T \ge T_{\text{set}} + 0.75$, tắt khi $T \le T_{\text{set}} - 0.75$ đối với quạt), triệt tiêu hoàn toàn hiện tượng chattering tiếp điểm.

---

## 3. Hệ Quả và Đánh Giá Đánh Đổi (Consequences)

### Tích Cực (Positive):
- **Triệt tiêu nguy cơ hỏa hoạn và quá nhiệt:** Cơ chế khóa độc quyền loại trừ hoàn toàn tình huống Quạt và Sưởi cùng chạy hoặc Sưởi chạy quá 30 phút. Ưu tiên giải nhiệt (Cooling Override) bảo vệ an toàn tính mạng và tài sản.
- **Tôn trọng quyền chủ động của người dùng:** Lệnh bật sưởi khi quạt đang chạy không tự ý dập tắt quạt của người dùng mà từ chối rõ ràng kèm lỗi `ERR_RELAY_INTERLOCK_CONFLICT`.
- **Bảo vệ chuẩn xác từng loại tải phần cứng:** Máy nén (Relay 3) được bảo vệ chống kẹt áp gas bằng dwell 180s, trong khi Quạt (Relay 1) và Sưởi (Relay 2) đáp ứng nhanh không bị trễ 180s vô lý.
- **Miễn nhiễm với bước nhảy NTP:** Sử dụng `esp_timer_get_time()` bảo đảm các bộ đếm ngắt khẩn cấp và lockout 300s hoạt động tin cậy 100% ngay cả khi đồng hồ hệ thống bị chỉnh lùi hoặc tiến nhiều giờ.

### Tiêu Cực & Đánh Đổi (Negative & Neutral):
- **Từ chối lệnh điều khiển khi xung đột:** Người dùng phải chủ động tắt quạt trước khi muốn bật máy sưởi.
- **Thời gian chờ máy nén:** Sau khi tắt máy hút ẩm/máy nén, bắt buộc phải chờ đủ 180 giây trước khi có thể bật lại.

---

## 4. Tuân Thủ và Xác Nhận (Compliance & Verification)

- **Verification Contract:** Khớp chuẩn xác với `FR-RELAY-01` (Khóa liên động bất đối xứng, Dwell máy nén & Ngắt khẩn cấp an toàn) và `FR-RULE-01` (Hysteresis Deadband, Đồng bộ hai chiều, Monotonic Clock & Khóa 300s) tại FSD §4.10.6 & §4.7.7.
- **Kiểm thử tự động:** Bộ kiểm thử [tests/host/test_relay_controller.py](file:///home/chien/ai-workspace/qwen-test/tests/host/test_relay_controller.py) và [tests/host/test_rule_engine.py](file:///home/chien/ai-workspace/qwen-test/tests/host/test_rule_engine.py) xác nhận tính đúng đắn của toàn bộ các ca kiểm thử Interlock Asymmetry, Cooling Override, Selective Dwell Time 180s, Monotonic Safety Cutoff và Lockout 300s.

---

## 5. Thẩm Tra Ponytail và Đánh Đổi Tối Giản (Ponytail Audit & Trade-off)

- **Từ chối HAL Trừu Tượng Đa Cấu Hình (Flat Static Table):** Chỉ có 4 kênh rơ-le vật lý cố định trên bo mạch, hệ thống từ chối các lớp trừu tượng Device Tree hay Object-Oriented Actuator Classes. Một mảng cấu hình tĩnh phẳng 4 phần tử ánh xạ trực tiếp sang GPIO 15..18, truy xuất $O(1)$.
- **Không dùng FreeRTOS Software Timers cho Dwell/Lockout:** Sử dụng trực tiếp `esp_timer_get_time()` (monotonic microsecond clock) để kiểm tra Dwell Time (180s máy nén), Lockout (300s sưởi) và Cutoff (1800s sưởi) bằng phép trừ số nguyên $O(1)$. Không tạo timer threads chạy ngầm trong FreeRTOS timer service, tiết kiệm CPU cycles và stack heap, đồng thời miễn nhiễm với bước nhảy NTP.
- `/* ponytail: Quản lý 4 kênh rơ-le qua mảng tĩnh phẳng và so sánh Monotonic Clock (esp_timer_get_time()) trực tiếp; không tạo HAL trừu tượng, không cấp phát FreeRTOS timer động; giảm thiểu tối đa độ phức tạp và tiêu hao RAM. Tách biệt rõ rệt relay_gpio_init() ở Boot Step 2 và relay_controller_init() ở Boot Step 5. */`
