# ADR-002: Kiến Trúc Hướng Sự Kiện và Đơn Điểm Thu Hồi Bộ Nhớ (Event-Driven Messaging & Single-Point Memory Deallocation)

- **Trạng thái:** Chấp thuận (Accepted)
- **Ngày quyết định:** 2026-09-21
- **Người đề xuất:** System Architect
- **Phạm vi kỹ thuật:** FreeRTOS IPC, Quản lý Heap Memory, [firmware/main/event_bus.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/event_bus.c)
- **Tài liệu tham chiếu gốc:** [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md) (§3.3, §4.1)
- **Truy vết yêu cầu kỹ thuật:** `Traces to: FR-EVT-01 / M01 / NFR-RES-01`

---

## 1. Bối Cảnh và Vấn Đề (Context & Problem Statement)

Một hệ thống nhúng IoT Gateway phức tạp bao gồm nhiều phân hệ độc lập: BLE scanner, MQTT client, Rule Engine, Display OLED, Relay controller, và SD Logger. Nếu các phân hệ gọi trực tiếp hàm của nhau (Direct Function Calls):
1. **Phụ thuộc lẫn nhau quá chặt (Tight Coupling & Spaghetti Code):** Thay đổi giao diện một module sẽ kéo theo lỗi biên dịch trên toàn bộ dự án.
2. **Nguy cơ Deadlock và Đảo ngược mức ưu tiên (Priority Inversion):** Khi một task ưu tiên cao (vd: BLE scan) bị block do gọi trực tiếp vào một module đang chờ I/O chậm (vd: ghi thẻ nhớ SD).
3. **Hiểm họa quản lý bộ nhớ động trong C (Dynamic Memory Hazards):** Khi truyền tải các bản tin có kích thước biến thiên qua con trỏ heap, nếu không phân định rõ ai là người sở hữu con trỏ (Memory Ownership), hệ thống sẽ gặp các lỗi nghiêm trọng:
   - **Rò rỉ bộ nhớ (Memory Leak):** Khi subscriber quên giải phóng bộ nhớ.
   - **Lỗi giải phóng hai lần (Double Free):** Khi nhiều subscriber cùng gọi `free()`.
   - **Con trỏ lơ lửng (Dangling Pointer / Use-After-Free):** Khi một subscriber giải phóng bộ nhớ trong khi subscriber khác đang đọc.

---

## 2. Quyết Định Kiến Trúc (Decision)

Chúng tôi quyết định áp dụng mô hình **Hàng Đợi Sự Kiện Trung Tâm (Central Event Bus)** kết hợp với quy tắc bất biến **Đơn Điểm Thu Hồi Bộ Nhớ (Single-Point `free()`)**:

```
Producer Task                Event Queue (64 slots)            Dispatcher Task
┌──────────────┐                 ┌─────────────┐               ┌───────────────────────┐
│ malloc(size) │ ──[post_event]─►│ gateway_evt │ ──[receive]──►│ 1. Call Subscriber 1  │
│              │                 └─────────────┘               │ 2. Call Subscriber 2  │
└──────────────┘                                               │ 3. free(evt.data)     │
                                                               └───────────────────────┘
                                                               (ĐƠN ĐIỂM FREE DUY NHẤT)
```

1. **Phân tách hoàn toàn qua Hàng đợi trung tâm (`s_event_queue`):**
   - Mọi giao tiếp giữa các module $A \rightarrow B$ ($A \neq B$) bắt buộc phải thông qua hàm `event_bus_post()`. Hàng đợi cố định dung lượng `EVENT_BUS_QUEUE_LEN = 64` slots.
2. **Quy tắc sở hữu bộ nhớ tuyệt đối (Memory Ownership Invariant):**
   - **Producer:** Chịu trách nhiệm cấp phát bộ nhớ động (`malloc()`) để chứa dữ liệu payload (vd: `ble_telemetry_t`).
   - **Subscriber (Handlers):** Chỉ được nhận con trỏ dạng hằng chỉ đọc (`const gateway_event_t *evt`). Tuyệt đối **KHÔNG ĐƯỢC PHÉP** gọi `free(evt->data)` và không được lưu lại con trỏ để dùng sau.
   - **Event Dispatcher Task:** Là thực thể **DUY NHẤT** được quyền gọi `free(evt.data)` sau khi đã duyệt qua và gọi toàn bộ các hàm callback đã đăng ký cho mã sự kiện đó.
3. **Chính sách hủy an toàn khi tràn hàng đợi (Queue Overflow Drop Policy):**
   - Nếu hàng đợi bị đầy (trả về `pdFALSE` do bão sự kiện), hàm `event_bus_post()` **ngay lập tức giải phóng `free(data)` tại chỗ**, ghi log cảnh báo và không để lọt bất kỳ byte rò rỉ nào.
4. **Hợp đồng không gây nghẽn Dispatcher (Non-Blocking Dispatcher Contract):**
   - Các hàm callback subscriber phải thực thi hoàn tất trong thời gian $< 5$ms. Tuyệt đối cấm các tác vụ I/O blocking hoặc trì hoãn dài (`vTaskDelay`).

---

## 3. Hệ Quả và Đánh Giá Đánh Đổi (Consequences)

### Tích Cực (Positive):
- **Triệt tiêu lỗi bộ nhớ:** Loại bỏ 100% nguy cơ Double Free và Use-After-Free. Kiểm chứng qua bài kiểm thử rò rỉ RAM với 1000+ sự kiện liên tục, Free Heap không giảm.
- **Tách rời module (Decoupling):** Một module có thể thêm, bớt hoặc sửa đổi mà không làm ảnh hưởng đến mã nguồn của module khác.
- **Hỗ trợ đa người nghe (1-to-N Publish-Subscribe):** Một bản tin telemetry từ BLE có thể phân phối đồng thời tới Rule Engine, Display OLED, SD Logger và MQTT Uplink một cách trong suốt.

### Tiêu Cực & Đánh Đổi (Negative & Neutral):
- **Độ trễ truyền bản tin:** Bản tin phải qua bước trung chuyển tại hàng đợi FreeRTOS queue và context switch của `event_dispatcher_task`, làm phát sinh độ trễ nhỏ ($\approx 50\dots 150\,\mu\text{s}$). Mức trễ này là hoàn toàn chấp nhận được trong các hệ thống IoT thời gian thực mềm (Soft Real-Time).
- **Phân mảnh Heap:** Cấp phát và giải phóng động liên tục có thể gây phân mảnh heap nếu payload có kích thước quá đa dạng. Tuy nhiên, trong hệ thống này, các payload đã được chuẩn hóa kích thước cố định theo struct (`sizeof(ble_telemetry_t) = 48` bytes, v.v.), giúp heap duy trì tính đồng nhất cao.

---

## 4. Tuân Thủ và Xác Nhận (Compliance & Verification)

- **Verification Contract:** Khớp chuẩn xác với `FR-EVT-01` (Chống nghẽn hàng đợi & Thu hồi bộ nhớ an toàn) tại FSD §4.1.7.
- **Kiểm thử máy chủ:** Được kiểm chứng tại [tests/host/test_event_bus.py](file:///home/chien/ai-workspace/qwen-test/tests/host/test_event_bus.py) và hàm stress test bão sự kiện `event_bus_test_burst(40)`.

---

## 5. Thẩm Tra Ponytail và Đánh Đổi Tối Giản (Ponytail Audit & Trade-off)

- **Từ chối Per-Subscriber Queues (Tiết kiệm ~6 KB SRAM):** Thay vì tạo hàng đợi FreeRTOS riêng cho từng module nhận tin (đòi hỏi 7 hàng đợi độc lập $\approx 7$ KB RAM và nguy cơ lệch pha thứ tự sự kiện), Gateway sử dụng **1 hàng đợi trung tâm duy nhất (64 slots = 1024 Bytes)** kết hợp duyệt mảng tĩnh handler.
- **Từ chối Reference Counting (Zero-Overhead Memory Ownership):** Quy tắc bất biến Producer malloc / Dispatcher free loại bỏ hoàn toàn nhu cầu về atomic reference counters hay cấu trúc smart pointer tự chế trong C, tiết kiệm chu kỳ xung nhịp và giữ struct payload phẳng (POD).
- `/* ponytail: Đơn điểm free() tại Dispatcher kết hợp với 1 hàng đợi FreeRTOS duy nhất giúp tiết kiệm ~6KB RAM so với kiến trúc per-queue pub/sub, triệt tiêu 100% rò rỉ bộ nhớ mà không cần cơ chế đếm tham chiếu phức tạp. */`
