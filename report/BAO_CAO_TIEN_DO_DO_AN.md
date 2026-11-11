# BÁO CÁO TIẾN ĐỘ THỰC HIỆN ĐỒ ÁN TỐT NGHIỆP
### Hệ Cử nhân Kỹ thuật / Công nghệ Thông tin — Trường CNTT&TT (Đại học Bách Khoa Hà Nội)

---

* **Tên đề tài**: Xây dựng hệ thống đồng hồ thông minh tích hợp Edge IoT Gateway
* **Lĩnh vực đề tài**: Internet of Things (IoT), Hệ thống nhúng (Embedded Systems), Kỹ thuật phần mềm (Software Engineering)
* **Sinh viên thực hiện**: [Họ và tên sinh viên] — MSSV: [Mã số sinh viên] — Lớp: [Tên lớp]
* **Giáo viên hướng dẫn (GVHD)**: [Họ và tên GVHD]
* **Thời điểm báo cáo**: Tháng 11/2026 (Tuần thứ 15–16 theo tiến độ 17 tuần)

---

## I. TỔNG QUAN TIẾN ĐỘ ĐỐI SOÁT THEO PHIẾU GIAO NHIỆM VỤ (PGNV)

Căn cứ theo 5 nội dung trọng tâm được giao trong Phiếu Giao Nhiệm Vụ ĐATN, tiến độ triển khai thực tế của đồ án đạt được như sau:

| STT | Nội dung công việc theo PGNV | Thời gian kế hoạch | Tiến độ thực tế | Trạng thái nghiệm thu |
|:---:|---|:---:|:---:|:---:|
| **1** | **Khảo sát bài toán và thiết kế tổng thể hệ thống**<br>- Khảo sát hạn chế của hệ thống Cloud-only.<br>- Nghiên cứu chuẩn BLE (0x181A, BTHome V2), lưu trữ ngoại tuyến.<br>- Lập tài liệu đặc tả FSD và thiết kế kiến trúc phân tầng. | Tuần 1 – 3 | **100%** | 🟢 **ĐÃ HOÀN TẤT**<br>(FSD v2.1 chính thức đóng băng) |
| **2** | **Nghiên cứu phần cứng và xây dựng firmware thiết bị biên (ESP32-S3)**<br>- Đấu nối ngoại vi (OLED, RTC DS3231, MicroSD, nút bấm, buzzer).<br>- Lập trình FreeRTOS đa lõi, Event Bus, RTC/SNTP.<br>- Quét sóng thụ động BLE NimBLE, giải mã cảm biến.<br>- Ghi nhật ký FATFS thẻ nhớ, Task Watchdog TWDT. | Tuần 4 – 8 | **100%** | 🟢 **ĐÃ HOÀN TẤT & ĐO KIỂM THỰC TẾ**<br>(Nghiệm thu trên phần cứng thật) |
| **3** | **Xây dựng máy chủ Cloud Backend và cơ sở dữ liệu**<br>- REST API Node.js/Express, MQTT Broker (hỗ trợ MQTTS TLS 8883).<br>- Cơ sở dữ liệu quan hệ (Prisma ORM, PostgreSQL/SQLite).<br>- Xác thực JWT, scrypt, chống trùng lặp dữ liệu gửi bù (Backfill Dedup).<br>- Kiểm thử luồng truyền nhận 2 chiều với ESP32-S3. | Tuần 9 – 11 | **100%** | 🟢 **ĐÃ HOÀN TẤT**<br>(20/20 Jest tests PASSED) |
| **4** | **Xây dựng ứng dụng web quản trị và tích hợp hệ thống**<br>- Web Dashboard React 18, Vite, TailwindCSS (chuẩn Impeccable).<br>- Giám sát thời gian thực, vẽ biểu đồ, quản lý cảm biến, đặt lịch.<br>- Tích hợp liên thông 3 tầng: Cảm biến ↔ ESP32-S3 ↔ Backend ↔ Web.<br>- SoftAP Captive Portal, kiểm thử gửi bù Store-and-Forward. | Tuần 12 – 14 | **100%** | 🟢 **ĐÃ HOÀN TẤT**<br>(Build Vite sạch 100%, 0 lỗi) |
| **5** | **Thử nghiệm thực tế, đánh giá hiệu năng và hoàn thiện báo cáo**<br>- Đo kiểm ổn định liên tục, thời gian boot màn hình, rò rỉ RAM.<br>- Thử nghiệm kịch bản lỗi: mất Wi-Fi, rút nóng thẻ nhớ, ngắt nguồn.<br>- Kiểm toán Zero-Trust trên phần cứng thật qua cổng Serial.<br>- Viết quyển báo cáo Đồ án tốt nghiệp và đóng gói sản phẩm. | Tuần 15 – 17 | **90%** | 🟡 **ĐANG HOÀN THIỆN QUYỂN BÁO CÁO**<br>(Đã xong đo kiểm phần cứng thực tế) |

---

## II. CHI TIẾT KẾT QUẢ ĐÃ ĐẠT ĐƯỢC

### 1. Về Mặt Phần Cứng & Thiết Bị Biên (ESP32-S3 Edge Gateway)

Thiết bị phần cứng đã được lắp ráp mạch hoàn chỉnh, nạp firmware và đo kiểm trực tiếp thông qua cổng giao tiếp `/dev/ttyACM0` (UART0 115200 baud).

* **Bộ xử lý trung tâm**: ESP32-S3 (Dual-core Xtensa LX7 @ 160MHz, 16MB SPI Flash, 8MB PSRAM). Phân chia 2 nhân CPU hợp lý:
  - *Core 0*: Tác vụ mạng (Wi-Fi, MQTTS TLS, Web Server cục bộ, Event Bus Dispatcher, Task Supervisor).
  - *Core 1*: Tác vụ thời gian thực (Display OLED, Đọc RTC, Lưu trữ thẻ nhớ MicroSD, Quét sóng BLE NimBLE).
* **Màn hình hiển thị OLED SSD1327 (1.5" 128x128 I2C `0x3C`)**:
  - Tốc độ khởi động hiển thị cực nhanh (< 0.25 giây).
  - Giao diện đồ họa phân chia 3 trang: Màn hình 0 (Đồng hồ số lớn + Nhiệt/ẩm môi trường + Trạng thái mạng), Màn hình 1 (Danh sách cảm biến BLE), Màn hình 2 (Trạng thái rơ-le & hệ thống).
  - Tự động bật trang *Provisioning* hướng dẫn cấu hình khi thiết bị chưa có Wi-Fi.
* **Mạch thời gian thực RTC DS3231 (I2C `0x68`) kèm pin CR2032**:
  - Tích hợp IC bù nhiệt TCXO độ chính xác cao.
  - Cơ chế đồng bộ: Khi có Wi-Fi, tự động lấy giờ Internet qua SNTP (`pool.ntp.org`) rồi nạp thẳng vào thanh ghi phần cứng của DS3231.
  - **Kiểm chứng mất điện**: Đã thử nghiệm ngắt hoàn toàn nguồn điện 5V của thiết bị; pin CR2032 duy trì xung nhịp chính xác từng giây, khi cắm điện lại thiết bị hiển thị giờ thực ngay lập tức mà không cần mạng.
* **Lưu trữ ngoại tuyến MicroSD FAT32 (giao tiếp SPI2: CS=10, MOSI=11, CLK=12, MISO=13)**:
  - Tự động khởi tạo cấu trúc thư mục `/DATA` và `/LOGS`.
  - Ghi nhật ký CSV đo đạc môi trường theo từng ngày.
  - Triển khai hàng đợi nhị phân 172-byte `sync_queue.bin` (FSD §4.11.3) hỗ trợ cơ chế tự hành **Offline-First**: khi mất mạng toàn bộ dữ liệu cảm biến được đóng gói lưu vào thẻ; khi có mạng trở lại, hệ thống tự động đẩy bù (backfill) lên Cloud Backend mà không làm nghẽn băng thông.
  - **Cơ chế Hotplug & RAM Fallback Buffer**: Đã kiểm chứng rút thẻ nhớ trong lúc hệ thống đang vận hành, bộ nhớ RAM tự động kích hoạt vòng đệm 50 bản tin để chống mất dữ liệu; khi cắm thẻ lại, hệ thống tự động nạp (drain) toàn bộ bản tin tồn đọng vào thẻ an toàn.
  - Hỗ trợ nạp file cấu hình từ thẻ nhớ (`/sdcard/config.json`) nạp vào bộ nhớ Flash NVS và tự đổi đuôi thành `.bak`.
* **Giao tiếp người dùng & Cảnh báo**:
  - Nút BOOT (GPIO 0): Tích hợp đa chức năng (nhấn nhanh <500ms để chuyển đổi trang màn hình OLED; nhấn giữ >=2s để tắt chuông báo thức; nhấn giữ >=5s để khôi phục cài đặt gốc).
  - Bộ lập lịch báo thức nội bộ độc lập Internet, còi buzzer cảnh báo, hỗ trợ hoãn chuông (Snooze +9 phút).
* **Độ tin cậy & Tự phục hồi (Fault-Tolerance)**:
  - Tích hợp Task Watchdog (TWDT 15s) giám sát nhịp tim toàn bộ 7 tác vụ qua bitmask `0x7F`.
  - Tự động ghi lại nhật ký crash dump vào vùng nhớ không mất dữ liệu RTC Memory / NVS khi phát hiện task bị treo để hỗ trợ chẩn đoán nguyên nhân gốc rễ.

---

### 2. Về Mặt Máy Chủ Đám Mây (Cloud Backend & Broker)

* **Kiến trúc dịch vụ**: Node.js v20 kết hợp Express REST API và Cloud MQTT Broker (EMQX Serverless) giao tiếp qua cổng an toàn **MQTTS TLS 8883** sử dụng chứng chỉ số X.509.
* **Cơ sở dữ liệu**: PostgreSQL / SQLite quản lý qua Prisma ORM, chuẩn hóa các bảng thực thể: `Gateway`, `Device`, `Telemetry`, `Rule`, `Schedule`, `EventLog`, `User`.
* **Bảo mật & Phân quyền**:
  - Xác thực người dùng bằng JSON Web Token (JWT), mật khẩu mã hóa bằng thuật toán `scrypt` có muối chống tấn công từ điển.
  - Tích hợp middleware chống tấn công Brute-force (`express-rate-limit`: tối đa 5 lần thử claim gateway trong 15 phút).
  - Chống lỗ hổng phân quyền Broken Object-Level Authorization (BOLA/IDOR).
* **Đồng bộ hai chiều & Gửi bù (Store-and-Forward)**:
  - Luồng Uplink: Nhận dữ liệu cảm biến từ ESP32-S3 qua MQTT QoS 1. Cơ chế lọc trùng lặp bản tin gửi bù dựa trên cặp khóa `(device_id, timestamp)`.
  - Luồng Downlink: Điều khiển thiết bị từ xa sinh mã UUID `command_id` duy nhất, yêu cầu thiết bị biên phản hồi gói tin `command_ack` xác nhận kết quả thực thi.
* **Kiểm thử tự động**: Đạt **20/20 bài kiểm thử tự động (PASS 100%)** bằng Jest (`npm test`).

---

### 3. Về Mặt Ứng Dụng Quản Trị Web (Web Dashboard Frontend)

* **Công nghệ phát triển**: React 18, Single-Page App (SPA), đóng gói bằng Vite và tạo kiểu với TailwindCSS.
* **Tiêu chuẩn thiết kế Impeccable (Chống giao diện AI rập khuôn)**:
  - Thiết kế bảng màu kỹ thuật tối Industrial IoT (`cyan`, `sky`, `amber`, `zinc.750`), tối ưu độ tương phản và bảo vệ thị giác.
  - Sử dụng Native System Font Stack nhẹ, tải tức thì, loại bỏ phụ thuộc font ngoài.
* **Chức năng đã hoàn thiện**:
  - *Màn hình Đăng nhập / Đăng ký*: Tích hợp Auth Guard (`ProtectedRoute.jsx`) khóa an toàn toàn bộ trang nội bộ khi chưa đăng nhập.
  - *Dashboard Giám sát*: Hiển thị trạng thái kết nối Gateway thời gian thực, các chỉ số nhiệt độ, độ ẩm đo được từ các cảm biến.
  - *Biểu đồ xu hướng (Telemetry Chart)*: Trực quan hóa biến thiên môi trường theo thời gian thực.
  - *Quản lý Thiết bị & Lập lịch*: Giao diện thêm/xóa cảm biến BLE, thiết lập ngưỡng kích hoạt rơ-le tự động và cài đặt giờ báo thức từ xa.
* **Kiểm thử giao diện**: Biên dịch sạch 100% trong 2.99 giây, 0 lỗi cảnh báo, kiểm toán đạt 0 anti-patterns bằng công cụ phân tích Impeccable.

---

### 4. Kết Quả Kiểm Toán Thực Nghiệm Zero-Trust (Ngày 11/11/2026)

Để đảm bảo tính trung thực học thuật và độ tin cậy tuyệt đối của đồ án, sinh viên đã chủ động tiến hành đợt **kiểm toán Zero-Trust trên phần cứng thật**:

1. **Thanh lọc dữ liệu ảo**: Đã rà soát và xóa bỏ toàn bộ 10 file kiểm thử Python mock-only ở các phiên làm việc trước vốn chỉ giả lập trong RAM mà không liên kết mã nguồn C.
2. **Đo kiểm trực tiếp trên thiết bị thật (`/dev/ttyACM0`)**:
   - Chạy bộ kiểm thử 8 bài test chức năng lõi bo mạch: 8/8 test PASS (`TC-BLE-01`, `TC-RELAY-01`, `TC-DEV-01`, `TC-RULE-01`, `TC-CLK-03`, `TC-ST-01`, `TC-WIFI-02`, `TC-SUP-02`).
   - Kiểm thử xác thực REST API cục bộ với tiêu đề `X-API-Key`: 4/4 kịch bản đạt chuẩn an ninh.
   - Thử nghiệm cơ chế rút nóng thẻ nhớ và gửi bù: đạt 100%.
   - Khắc phục thành công lỗi tranh chấp khóa Deadlock đệ quy trong thư viện FatFS của FreeRTOS, giúp firmware đạt độ ổn định tuyệt đối (uptime chạy liên tục không crash).
   - Toàn bộ bằng chứng log kiểm tra thực nghiệm độc lập đã được lưu giữ có hệ thống tại thư mục `testing/evidence/2026-11-11/`.

---

## III. SO SÁNH CÁC CHỈ SỐ KỸ THUẬT VỚI MỤC TIÊU BAN ĐẦU

| Chỉ số kỹ thuật | Yêu cầu thiết kế (PGNV / FSD) | Kết quả thực tế đo đạc | Đánh giá |
|---|:---:|:---:|:---:|
| **Thời gian khởi động hiển thị OLED** | < 1.0 giây | **~0.25 giây** (Hiển thị ngay tại boot segment 7) | 🟢 **Vượt chỉ tiêu** |
| **Duy trì giờ khi mất điện** | Lưu trữ độc lập bằng RTC | Giữ giờ chính xác 100% nhờ RTC DS3231 + Pin CR2032 | 🟢 **Đạt chuẩn** |
| **Khả năng tự hành khi mất mạng** | 100% (Hiển thị, báo thức, lưu thẻ) | Hoạt động trơn tru, đệm RAM 50 items + thẻ SD FAT32 | 🟢 **Đạt chuẩn** |
| **Cơ chế gửi bù dữ liệu (Backfill)** | Tự động khi có lại Internet | Backfill sạch gói tin nhị phân lên MQTTS Broker | 🟢 **Đạt chuẩn** |
| **Bộ nhớ RAM khả dụng (Free Heap)** | > 35,000 bytes | **~41,600 – 70,000 bytes** (Ổn định, không rò rỉ RAM) | 🟢 **Đạt chuẩn** |
| **Độ tin cậy hệ thống (Watchdog)** | Tự phục hồi khi treo tác vụ | TWDT 15s phát hiện chính xác và dump crash an toàn | 🟢 **Đạt chuẩn** |
| **Bảo mật kênh truyền** | MQTTS TLS cổng 8883 | MbedTLS với root CA bundle, mã hóa toàn bộ dữ liệu | 🟢 **Đạt chuẩn** |

---

## IV. CÁC CÔNG VIỆC CÒN LẠI VÀ KẾ HOẠCH HOÀN TẤT (TUẦN 16 – 17)

Để hoàn tất trọn vẹn đồ án trước ngày bảo vệ chính thức, sinh viên đang tập trung thực hiện các nội dung cuối cùng:

1. **Hoàn thiện quyển Báo cáo Đồ án Tốt nghiệp**:
   - Viết chi tiết các chương: Tổng quan lý thuyết, Thiết kế kiến trúc hệ thống, Hiện thực hóa chi tiết (Firmware, Backend, Frontend) và Đánh giá kết quả thực nghiệm.
   - Trích xuất đồ thị tri thức từ công cụ Graphify (`graphify-out/graph.json` với 856 nodes, 1983 liên kết kiến trúc) để minh họa mối quan hệ giữa các phân hệ trong báo cáo.
2. **Lắp ráp hoàn chỉnh mô hình sản phẩm mẫu**:
   - Cố định bo mạch ESP32-S3, màn hình OLED 1.5", mạch RTC DS3231 và khe cắm thẻ MicroSD vào hộp bảo vệ hoàn thiện.
   - (Tùy chọn) Đấu nối thêm cụm rơ-le 4 kênh vật lý để thực hiện demo đóng cắt đèn/quạt thực tế tại buổi bảo vệ.
3. **Quay video demo sản phẩm và chuẩn bị slide thuyết trình**:
   - Ghi lại các kịch bản minh chứng: Hoạt động bình thường, kịch bản rút dây mạng (chứng minh tính năng Offline-First) và kịch bản cắm lại mạng (chứng minh tính năng gửi bù Store-and-Forward).

---

## V. KẾT LUẬN & KIẾN NGHỊ

* **Kết luận**: Đồ án đã bám sát 100% các nội dung giao trong Phiếu Giao Nhiệm Vụ, hoàn thành đúng tiến độ các sản phẩm kỳ vọng (Firmware ESP32-S3, Cloud Backend, Web App Dashboard). Sản phẩm đã được chứng thực bằng đo kiểm thực tế trên bo mạch thật với độ ổn định cao.
* **Ý kiến / Đề xuất của sinh viên**: Kính mong Thầy/Cô Giáo viên hướng dẫn xem xét bản báo cáo tiến độ, đóng góp ý kiến hoàn thiện cho cấu trúc quyển báo cáo và định hướng các nội dung cần nhấn mạnh trong buổi bảo vệ Đồ án tốt nghiệp sắp tới.

*Hà Nội, ngày 11 tháng 11 năm 2026*  
**Sinh viên thực hiện**  
*(Ký và ghi rõ họ tên)*
