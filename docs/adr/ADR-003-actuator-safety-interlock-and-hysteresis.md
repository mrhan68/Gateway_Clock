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

## 2. Quyết Định Kiến Trúc (Decision)

Chúng tôi quyết định áp dụng **Kiến Trúc An Toàn Cơ Cấu Chấp Hành Đa Tầng (Multi-Layer Actuator Safety Architecture)**:

```
                               Ngõ Vào Lệnh Điều Khiển
                             (Cloud MQTT / Local Rules)
                                         │
                                         ▼
                            ┌─────────────────────────┐
                            │ 1. Dwell Time Lockout   │──[Chưa đủ 180s]──► Từ chối lệnh / Hoãn
                            │ (Anti-Short-Cycle >=180s│
                            └─────────────────────────┘
                                         │ [Hợp lệ]
                                         ▼
                            ┌─────────────────────────┐
                            │ 2. Khóa Liên Động       │──[Bật Sưởi khi Quạt tắt]──► Tự động BẬT Quạt
                            │ (Fan-Heater Interlock)  │                             Chờ trễ 500ms
                            └─────────────────────────┘                             Bật Sưởi sau
                                         │
                                         ▼
                            ┌─────────────────────────┐
                            │ 3. Giới Hạn Cưỡng Chế   │──► Kích hoạt Timer 30m (Emergency Cutoff)
                            │ (RELAY_2_MAX_ON_SEC=30m)│
                            └─────────────────────────┘
                                         │
                                         ▼
                            Kích hoạt chân GPIO 15..18
```

1. **Phần cứng Fail-Safe mặc định (Hardware Fail-Safe):**
   - Mạch driver rơ-le được thiết kế theo logic **Active HIGH** với điện trở kéo xuống (Pull-down $10\,\text{k}\Omega$) cố định trên các chân GPIO 15..18.
   - Ngay tại Bước 3 của Boot sequence (trước khi bất kỳ tác vụ mạng hay scheduler nào khởi chạy), hàm `relay_controller_init()` kéo toàn bộ các chân này về mức `LOW`, bảo đảm tải luôn bị ngắt điện khi khởi động hoặc chập chờn nguồn.
2. **Khóa liên động vật lý và logic (Interlocking Logic):**
   - Quy tắc bất biến: **Máy sưởi (`relay_2`) chỉ được phép hoạt động khi Quạt (`relay_1`) đang BẬT.**
   - Nếu nhận lệnh bật Sưởi khi Quạt đang tắt: Controller tự động bật Quạt trước, kích hoạt bộ đếm trễ `RELAY_INTERLOCK_DELAY_MS = 500ms` để luồng không khí lưu thông ổn định, sau đó mới đóng tiếp điểm Máy sưởi.
   - Nếu người dùng hoặc Cloud yêu cầu tắt Quạt khi Máy sưởi đang chạy: Controller **cưỡng chế ngắt Máy sưởi ngay lập tức**, sau đó mới cho phép tắt Quạt.
3. **Cắt nguồn khẩn cấp (Emergency Cutoff Timer):**
   - Kênh Máy sưởi được gắn bộ giám sát thời gian tối đa `RELAY_2_MAX_ON_SEC = 1800` giây (30 phút). Hết 30 phút, rơ-le tự động nhả tiếp điểm và phát sự kiện `SAFETY_CUTOFF` bất kể trạng thái Cloud.
4. **Bảo vệ chống chu kỳ ngắn (Anti-Short-Cycle Dwell Time):**
   - Áp dụng thời gian trễ tối thiểu **180 giây** giữa hai lần đảo trạng thái ON $\leftrightarrow$ OFF liên tiếp của cùng một kênh rơ-le.
5. **Dải trễ Hysteresis Deadband ($1.5^\circ\text{C}$) và Lockout (300s):**
   - Rule Engine áp dụng dải trễ $1.5^\circ\text{C}$ đối xứng xung quanh ngưỡng cài đặt và áp dụng thời gian khóa an toàn 300s sau mỗi lần chuyển mạch, triệt tiêu hoàn toàn hiện tượng chattering.
6. **Chính sách Safe-Default khi mất tín hiệu cảm biến:**
   - Khi không nhận được beacon từ cảm biến sau 60 giây hoặc cờ `valid == false`: **Lập tức ngắt cưỡng chế Relay 2 (Máy sưởi)**, duy trì Relay 1 (Quạt) ở mức giải nhiệt an toàn.

---

## 3. Hệ Quả và Đánh Giá Đánh Đổi (Consequences)

### Tích Cực (Positive):
- **Triệt tiêu nguy cơ hỏa hoạn:** Cơ chế liên động và bộ đếm 30 phút bảo đảm thiết bị sưởi không bao giờ bị quá nhiệt không kiểm soát.
- **Bảo vệ tuổi thọ phần cứng:** Dwell time 180s và Hysteresis $1.5^\circ\text{C}$ bảo vệ tiếp điểm rơ-le và động cơ khỏi hao mòn hồ quang.
- **Tự trị an toàn (Autonomous Edge Safety):** Toàn bộ logic bảo vệ chạy nội bộ trên ESP32-S3 Core 0, không phụ thuộc vào kết nối Wi-Fi hay Cloud.

### Tiêu Cực & Đánh Đổi (Negative & Neutral):
- **Trễ thực thi lệnh:** Khi người dùng cố tình bật/tắt liên tục trong vòng 180s, lệnh sau sẽ bị từ chối hoặc hoãn thực thi để bảo vệ thiết bị.
- **Phụ thuộc logic quạt:** Nếu quạt hỏng cơ học (kẹt cánh), hệ thống cảm biến gián tiếp chưa phát hiện được dòng tải trừ khi trang bị thêm cảm biến dòng (Current Sensor) ở các phiên bản sau.

---

## 4. Tuân Thủ và Xác Nhận (Compliance & Verification)

- **Verification Contract:** Khớp chuẩn xác với `FR-RELAY-01` (Khóa liên động phần cứng, Khóa chu kỳ ngắn & Ngắt khẩn cấp) và `FR-RULE-01` tại FSD §4.10.6 & §4.7.7.
- **Kiểm thử tự động:** Bộ kiểm thử [tests/host/test_relay_controller.py](file:///home/chien/ai-workspace/qwen-test/tests/host/test_relay_controller.py) và [tests/host/test_rule_engine.py](file:///home/chien/ai-workspace/qwen-test/tests/host/test_rule_engine.py) xác nhận tính đúng đắn của toàn bộ các ca kiểm thử Interlock, Dwell Time, Hysteresis và Safe-Default.

---

## 5. Thẩm Tra Ponytail và Đánh Đổi Tối Giản (Ponytail Audit & Trade-off)

- **Từ chối HAL Trừu Tượng Đa Cấu Hình (Flat Static Table):** Chỉ có 4 kênh rơ-le vật lý cố định trên bo mạch, hệ thống từ chối các lớp trừu tượng Device Tree hay Object-Oriented Actuator Classes. Một mảng cấu hình tĩnh phẳng 4 phần tử ánh xạ trực tiếp sang GPIO 15..18, truy xuất $O(1)$.
- **Không dùng FreeRTOS Software Timers cho Dwell Time:** Kiểm tra Dwell Time (180s) và Cutoff (1800s) bằng phép trừ số nguyên epoch timestamp đơn giản (`now - last_state_change >= 180`). Không tạo 4 timer threads chạy ngầm trong FreeRTOS timer service, tiết kiệm CPU cycles và stack heap.
- `/* ponytail: Quản lý 4 kênh rơ-le qua mảng tĩnh phẳng và so sánh epoch trực tiếp; không tạo HAL trừu tượng, không cấp phát FreeRTOS timer động; giảm thiểu tối đa độ phức tạp và tiêu hao RAM. */`
