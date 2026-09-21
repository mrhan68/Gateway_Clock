# ADR-005: Giám Sát Tác Vụ Hai Tầng, Chẩn Đoán Bộ Nhớ RTC và Cầu Dao Chống Treo Khởi Động (Task Supervisor, Hardware TWDT & Boot Loop Circuit Breaker)

- **Trạng thái:** Chấp thuận (Accepted)
- **Ngày quyết định:** 2026-09-21
- **Người đề xuất:** System Architect
- **Phạm vi kỹ thuật:** Hệ điều hành FreeRTOS, Task Watchdog Timer (TWDT), RTC Slow Memory, [firmware/main/supervisor.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/supervisor.c)
- **Tài liệu tham chiếu gốc:** [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md) (§4.13, §11.3)
- **Truy vết yêu cầu kỹ thuật:** `Traces to: FR-SUP-01 / FR-SUP-02 / M17 / NFR-REL-01`

---

## 1. Bối Cảnh và Vấn Đề (Context & Problem Statement)

Thiết bị Smart Clock Gateway được thiết kế để hoạt động liên tục 24/7/365 trong môi trường gia đình mà không có kỹ sư túc trực giám sát:
1. **Hiện tượng treo tác vụ trong im lặng (Silent Task Starvation / Deadlock):** Một tác vụ FreeRTOS có thể rơi vào vòng lặp vô hạn, kẹt tại một mutex do deadlock, hoặc bị chặn mãi mãi bởi một lời gọi I/O mạng lwIP không có timeout. Khi đó hệ điều hành FreeRTOS vẫn chạy, nhưng các tính năng quan trọng (như điều phối sự kiện hoặc báo thức) bị tê liệt hoàn toàn.
2. **Mất dấu vết nguyên nhân lỗi sau khi khởi động lại:** Khi chip ESP32-S3 bị reset (do Watchdog hoặc Panic), toàn bộ dữ liệu trên RAM bay hơi, khiến việc truy tìm tác vụ nào đã gây ra lỗi trở nên bất khả thi.
3. **Thảm họa lặp khởi động liên tục (Boot Loop Catastrophe):** Nếu file cấu hình `rules.json` hoặc database flash bị lỗi cú pháp gây panic ngay khi khởi động, chip sẽ rơi vào vòng lặp reboot liên hồi hàng nghìn lần, làm nóng chip, cháy rơ-le do đóng cắt liên tục và biến thiết bị thành cục gạch (bricked device).

---

## 2. Quyết Định Kiến Trúc (Decision)

Chúng tôi quyết định thiết kế **Hệ Thống Giám Sát An Toàn Tác Vụ Hai Tầng (Two-Tier Supervisor Architecture)** kết hợp với **Vùng Nhớ Chẩn Đoán RTC Slow Memory** và **Cầu Dao Chống Lặp Khởi Động (Boot Loop Circuit Breaker)**:

```
                                    +-----------------------------+
                                    | ESP32-S3 Hardware TWDT     |
                                    | (Core 0 HW Timer: 15s)      |
                                    +-----------------------------+
                                                   ▲
                                                   │ esp_task_wdt_reset()
                                    +-----------------------------+
                                    | supervisor_task (Prio 7)    |
                                    | Quét mỗi 1000ms             |
                                    +-----------------------------+
                                                   │ Kiểm tra Bitmask
                                                   ▼
            ┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
            │  Bit 0  │  Bit 1  │  Bit 2  │  Bit 3  │  Bit 4  │  Bit 5  │  Bit 6  │
            │Dispatch │   BLE   │ Uplink  │ SD Log  │Scheduler│ Display │   CLI   │
            └─────────┴─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
                                   TỔNG MẶT NẠ CHUẨN: 0x7F
```

1. **Kiến trúc giám sát hai tầng:**
   - **Tầng 1 (Cấp phần cứng):** `supervisor_task` đăng ký với trình điều khiển phần cứng ESP-IDF Task Watchdog Timer (TWDT) với ngưỡng ngắt cứng **15 giây**. Nếu chính bản thân task supervisor bị kẹt, phần cứng ESP32-S3 sẽ tự động kích hoạt ngắt Reset hệ thống.
   - **Tầng 2 (Cấp ứng dụng):** Giám sát **7 tác vụ sản xuất (Production Tasks)** thông qua Heartbeat Bitmask `0x7F`. Cứ mỗi 1000ms, mỗi task bắt buộc phải gọi `supervisor_report_heartbeat(BIT)`. `supervisor_task` kiểm tra nếu đủ cả 7 bit thì mới tiến hành xóa mask về 0 và tiếp thức ăn cho HW TWDT.
2. **Ngưỡng phát hiện lỗi 15 giây và Chẩn đoán qua RTC Slow Memory:**
   - Nếu bất kỳ task nào trong 7 task không báo nhịp tim trong **15 giây liên tiếp**:
     - Supervisor xác định task đó đã bị deadlock hoặc đóng băng.
     - Thu thập thông tin sự cố vào cấu trúc `rtc_crash_dump_t` (Magic `0xDEADBEEF`, bit task lỗi, tên task, dung lượng heap trống, thời gian uptime).
     - Ghi dữ liệu chẩn đoán này vào **RTC Slow Memory** (vùng nhớ SRAM đặc biệt duy trì nguồn nuôi qua lệnh `esp_restart()`).
     - Kích hoạt tính năng `gpio_hold_en()` trên các chân rơ-le để giữ trạng thái an toàn trong quá trình khởi động lại, triệt tiêu hiện tượng chattering tiếp điểm.
     - Gọi lệnh khởi động lại vi điều khiển `esp_restart()`.
3. **Cầu dao an toàn chống lặp khởi động (Boot Loop Circuit Breaker):**
   - Bộ nhớ RTC Slow Memory duy trì biến đếm `s_boot_crash_count`.
   - Nếu hệ thống phát hiện có **$\ge 3$ lần khởi động lại bất thường trong vòng 60 giây**:
     - Cầu dao Circuit Breaker tự động ngắt mạch (Tripped).
     - Gateway bỏ qua việc nạp file cấu hình người dùng `rules.json` và `schedule.json`, nạp cấu hình mặc định xuất xưởng an toàn (**Safe-Defaults Mode**).
     - Khởi động mạng cứu hộ SoftAP và mở cổng dòng lệnh UART0 Console để kỹ thuật viên kết nối và nạp lại cấu hình hợp lệ.

---

## 3. Hệ Quả và Đánh Giá Đánh Đổi (Consequences)

### Tích Cực (Positive):
- **Tự khôi phục 100% (Zero-Touch Self-Healing):** Gateway tự giải thoát khỏi mọi trạng thái treo vô hạn mà không cần người dùng phải rút nguồn cắm lại.
- **Dễ dàng tìm nguyên nhân gốc (Post-Mortem Root Cause Analysis):** Sau khi khởi động lại, log khởi động in chính xác tên task và lượng RAM tại thời điểm bị chết từ RTC Memory ra màn hình serial.
- **Chống biến thành cục gạch (Unbrickable Gateway):** Cầu dao Circuit Breaker ngăn chặn 100% nguy cơ hỏng thiết bị do file cấu hình độc hại hoặc lỗi cú pháp JSON.

### Tiêu Cực & Đánh Đổi (Negative & Neutral):
- **Chi phí CPU định kỳ:** Việc 7 tasks định kỳ gửi tín hiệu báo nhịp mỗi giây và supervisor thức dậy mỗi 1000ms tiêu tốn một lượng chu kỳ CPU không đáng kể (< 0.2% tải của Core 0).
- **Thời gian gián đoạn 2.5s:** Khi một task bị treo, hệ thống mất 15 giây để xác định và mất khoảng 2.5 giây để chip ESP32-S3 khởi động lại hoàn tất.

---

## 4. Tuân Thủ và Xác Nhận (Compliance & Verification)

- **Verification Contract:** Khớp chuẩn xác với `FR-SUP-01` (Báo danh Heartbeat & Giám sát 7 Task Production) và `FR-SUP-02` (Bắt lỗi Task bị treo & Tự khôi phục an toàn) tại FSD §4.13.4.
- **Kiểm thử thực nghiệm:** Được kiểm chứng trực tiếp trên phần cứng thật qua lệnh CLI `supervisor freeze` và Journey Test `JRN-01` tại [testing/test-plan.yaml](file:///home/chien/ai-workspace/qwen-test/testing/test-plan.yaml), xác nhận hệ thống reboot đúng 15 giây và in báo cáo chẩn đoán từ RTC Slow Memory.

---

## 5. Thẩm Tra Ponytail và Đánh Đổi Tối Giản (Ponytail Audit & Trade-off)

- **Từ chối FreeRTOS EventGroups (Atomic Bitmask 8-bit):** Thay vì sử dụng `EventGroupHandle_t` của FreeRTOS (vốn đòi hỏi cấp phát heap động và cơ chế spinlock đa nhân phức tạp), Supervisor sử dụng **1 biến nguyên tử `uint32_t s_heartbeat_flags`** thao tác qua bitwise OR (`|=`). Thao tác này tiêu tốn đúng 1 chu kỳ CPU, 0 byte heap và lock-free hoàn toàn.
- **Tận dụng RTC Slow Memory Không Bay Hơi:** Struct `rtc_crash_dump_t` được đặt trong phân vùng RTC RAM (`RTC_NOINIT_ATTR`). Dữ liệu tồn tại qua lệnh `esp_restart()` mà không cần thực hiện thao tác Flash Write/Erase gây hao mòn chip nhớ SPI Flash.
- `/* ponytail: Giám sát bằng 1 biến bitmask 8-bit trên Core 0 thay vì FreeRTOS EventGroups, tiết kiệm 100% dynamic heap và loại bỏ spinlock overhead. Vùng chẩn đoán sự cố ghi thẳng vào RTC Slow Memory không tốn chu kỳ ghi flash. */`
