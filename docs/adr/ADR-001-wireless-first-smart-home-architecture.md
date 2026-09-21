# ADR-001: Kiến Trúc Mạng Không Dây Ưu Tiên (Wireless-First Smart Home Architecture)

- **Trạng thái:** Chấp thuận (Accepted)
- **Ngày quyết định:** 2026-09-21
- **Người đề xuất:** System Architect
- **Phạm vi kỹ thuật:** ESP32-S3 Vô tuyến 2.4 GHz (Wi-Fi 802.11 b/g/n & BLE 5.0 NimBLE)
- **Tài liệu tham chiếu gốc:** [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md) (§2.2, §4.5, §11.4)
- **Truy vết yêu cầu kỹ thuật:** `Traces to: FR-BLE-01 / M14 / S04 / S07 / NFR-PERF-03`

---

## 1. Bối Cảnh và Vấn Đề (Context & Problem Statement)

Trong hệ thống Smart Home hiện đại, việc thu thập dữ liệu từ các cảm biến môi trường (nhiệt độ, độ ẩm) đặt ra các thách thức lớn:
1. **Tiêu hao năng lượng của thiết bị ngoại vi:** Cảm biến chạy pin cúc áo (CR2032) không thể duy trì kết nối Wi-Fi liên tục do dòng tiêu thụ cao (80–200mA). Kết nối BLE theo dạng GATT Connection (Ghép đôi / Pair / Bond) cũng làm cạn kiệt pin sau vài tháng do phải duy trì Connection Interval định kỳ và thủ tục bắt tay mã hóa phức tạp.
2. **Trải nghiệm người dùng:** Người dùng gia đình gặp khó khăn khi phải thao tác ghép đôi Bluetooth từng thiết bị bằng mã PIN.
3. **Xung đột phần cứng trên vi điều khiển:** ESP32-S3 chỉ tích hợp **duy nhất 1 bộ thu phát vô tuyến (RF Transceiver) 2.4 GHz** dùng chung cho cả Wi-Fi Station và Bluetooth LE. Nếu không có cơ chế điều phối thời gian hợp lý, việc truyền dữ liệu MQTT tốc độ cao sẽ bóp nghẹt quá trình quét gói tin BLE, dẫn đến mất mát dữ liệu cảm biến hoặc ngược lại.

---

## 2. Quyết Định Kiến Trúc (Decision)

Chúng tôi quyết định áp dụng mô hình **Kiến Trúc Không Dây Ưu Tiên (Wireless-First Architecture)** với các giải pháp kỹ thuật sau:

1. **Chế độ quan sát thụ động không kết nối (BLE Passive Central Observer):**
   - Phân hệ [firmware/main/ble_adapter.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/ble_adapter.c) chỉ thực hiện quét thụ động (Passive Scanning) các gói tin quảng bá (Advertisement Beacons) theo chuẩn **`0x181A` (Environmental Sensing Service)** và **BTHome V2 (`0xFCD2`)**.
   - Gateway tuyệt đối không gửi gói tin yêu cầu quét `SCAN_REQ` và không thiết lập kết nối GATT Connection tới cảm biến.
   - Nhờ vậy, cảm biến ngoại vi duy trì tuổi thọ pin trên 12–18 tháng chỉ với một viên pin CR2032.

2. **Cơ chế phân chia thời gian vô tuyến (Software RF Coexistence):**
   - Kích hoạt cấu hình `CONFIG_SW_COEXIST_ENABLE=y` của ESP-IDF, phân chia thời gian (TDM - Time Division Multiplexing) giữa driver Wi-Fi và NimBLE.
   - Cấu hình chu kỳ quét BLE cân bằng: **Scan Window = 50ms, Scan Interval = 100ms (Duty Cycle 50%)**. Khoảng thời gian trống 50% còn lại được nhường trọn vẹn cho việc truyền nhận khung truyền Wi-Fi và MQTT socket trên Core 1.

3. **Hàng rào lọc Whitelist và Chống bão bản tin (BLE Storm Defense):**
   - Chỉ giải mã và tiếp nhận các cảm biến có địa chỉ MAC nằm trong danh bạ quản lý hoặc khớp đúng Service Data UUID môi trường.
   - Giới hạn tần suất nạp sự kiện: Tối đa 1 bản tin telemetry trên mỗi cảm biến trong vòng 5 giây, bảo vệ hàng đợi Event Bus khỏi hiện tượng nghẽn dòng.

---

## 3. Hệ Quả và Đánh Giá Đánh Đổi (Consequences)

### Tích Cực (Positive):
- **Tiết kiệm năng lượng triệt để:** Cảm biến nhiệt ẩm hoạt động liên tục nhiều năm mà không cần sạc hoặc thay pin thường xuyên.
- **Trải nghiệm Zero-Pairing:** Người dùng chỉ cần bật cảm biến hoặc nạp MAC vào Web Dashboard/CLI là Gateway tự động nhận diện và đọc thông số.
- **Tương thích cao:** Hỗ trợ ngay lập tức các dòng cảm biến thương mại phổ biến (như Xiaomi Mijia LYWSD03MMC chạy firmware ATC/pvvx) cũng như các cảm biến tự chế dùng chip ESP32-C3/nRF52.
- **Không xảy ra nghẽn vô tuyến:** Tính năng Coexistence giúp MQTT duy trì kết nối ổn định ngay cả khi BLE quét liên tục.

### Tiêu Cực & Đánh Đổi (Negative & Neutral):
- **Truyền một chiều (Unidirectional):** Gateway không thể gửi lệnh cấu hình ngược lại cảm biến (như đổi tần số phát beacon) khi cảm biến ở chế độ phát quảng bá thuần túy.
- **Chấp nhận mất gói ngẫu nhiên:** Trong các thời điểm Wi-Fi truyền tải nặng (vd: đẩy Batch dữ liệu bù), một vài gói tin quảng bá BLE có thể bị bỏ qua. Hệ thống chấp nhận đánh đổi này vì nhiệt độ phòng có quán tính nhiệt lớn và cảm biến sẽ phát lại ở chu kỳ kế tiếp (sau 1..5 giây).

---

## 4. Tuân Thủ và Xác Nhận (Compliance & Verification)

- **Verification Contract:** Khớp chuẩn xác với `FR-BLE-01` (Quét và giải mã gói tin BLE & Chống bão bản tin) tại FSD §4.5.6.
- **Kiểm thử tự động:** Bộ test case `tests/host/test_ble_parser.py` và `tests/host/test_ble_telemetry_simulator.py` mô phỏng và xác nhận giải mã chính xác 100% các khung truyền mẫu 0x181A và BTHome V2.

---

## 5. Thẩm Tra Ponytail và Đánh Đổi Tối Giản (Ponytail Audit & Trade-off)

- **Loại bỏ trừu tượng thừa (No GATT Abstraction Layer):** Không xây dựng lớp quản lý GATT Client kết nối hay state machine ghép đôi; tiết kiệm ~40 KB Flash ROM và ~12 KB SRAM heap so với mô hình BLE Connection truyền thống.
- **Xử lý gói tin Zero-Copy thô:** Trình điều khiển NimBLE GAP xử lý trực tiếp mảng byte thô trong sự kiện quảng bá (`BLE_GAP_EVENT_DISC`), giải mã số nguyên 16-bit và đóng gói trực tiếp vào struct `ble_telemetry_t`.
- `/* ponytail: YAGNI - Loại bỏ hoàn toàn GATT Client connection engine. Chế độ Passive GAP Observer đáp ứng 100% nhu cầu thu thập dữ liệu đo lường, zero-pairing cho người dùng, và kéo dài tuổi thọ pin cảm biến lên > 1 năm mà không tốn tài nguyên vi điều khiển. */`
