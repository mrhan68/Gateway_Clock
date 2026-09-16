# 🛠️ Tài Liệu Cấu Hình Ngoại Vi Giả Lập (MOCK_PERIPHERALS.md)
### Smart Clock & Industrial Edge IoT Gateway — ESP32-S3

> **Phiên bản**: v1.0  
> **Ngày khởi tạo**: 16/09/2026  
> **Mục đích**: Lưu trữ thông số, kịch bản dữ liệu và quy tắc hoạt động của tầng ngoại vi ảo (Mock Peripherals) trên Bare-board ESP32-S3. Tài liệu này là cơ sở để chỉnh sửa thông số mô phỏng trước khi chuyển giao sang phần cứng thật.

---

## 1. Tổng Quan Kiến Trúc Giả Lập (Simulation Architecture)

Để đảm bảo quy trình **AI Closed-Loop Programming (AICLP)** vẫn chạy khép kín mà không bị chặn bởi việc chưa nối dây vật lý, firmware hỗ trợ cơ chế ngoại vi ảo chạy song song với driver thật:

```
┌────────────────────────────────────────────────────────────────────────┐
│                   ESP32-S3 Bare-board Firmware                         │
│                                                                        │
│  ┌──────────────────────┐   ┌───────────────────┐   ┌───────────────┐  │
│  │    Rule Engine       │   │  Local Scheduler  │   │  MQTT Uplink  │  │
│  └──────────▲───────────┘   └─────────▲─────────┘   └───────▲───────┘  │
│             │                         │                     │          │
│  ═══════════╪═════════════════════════╪═════════════════════╪════════  │
│             └──────────── FreeRTOS Event Bus ───────────────┘          │
│                                       ▲                                │
│                     ┌─────────────────┴──────────────────┐             │
│                     │  TẦNG NGOẠI VI ẢO (MOCK SUBSYSTEM) │             │
│                     └─────────────────┬──────────────────┘             │
│         ┌─────────────────────────────┼────────────────────────────┐   │
│         ▼                             ▼                            ▼   │
│ ┌────────────────┐          ┌───────────────────┐        ┌───────────┐ │
│ │  Mock RS485    │          │  Mock VFS Storage │        │ Mock OLED │ │
│ │  Modbus Master │          │  (Internal Flash) │        │ & DS3231  │ │
│ └────────────────┘          └───────────────────┘        └───────────┘ │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Cấu Hình Giả Lập Bus RS485 Modbus RTU

### 2.1. Danh Sách Node Ngoại Vi (Slave Devices)

| Slave ID | Loại thiết bị | Thanh ghi (Registers) | Dải giá trị mô phỏng | Chu kỳ phát |
|:---:|---|---|---|:---:|
| **0x01** | Cảm biến Nhiệt độ & Độ ẩm công nghiệp (TH Sensor) | **Reg 0x0000**: Nhiệt độ (°C x 10)<br>**Reg 0x0001**: Độ ẩm (%RH x 10) | • Nhiệt độ: `26.0°C ~ 36.5°C` (bước nhảy `0.5°C`)<br>• Độ ẩm: `60.0% ~ 85.0%` (bước nhảy `1.0%`) | 2000 ms (2s) |
| **0x02** | Bo Relay 4 kênh công nghiệp (4-Ch Relay Board) | **Coil 0x0000**: Relay 1 (Quạt làm mát)<br>**Coil 0x0001**: Relay 2 (Máy sưởi)<br>**Coil 0x0002**: Relay 3 (Hút ẩm)<br>**Coil 0x0003**: Relay 4 (Đèn phụ) | `0x00` (OFF) hoặc `0x01` (ON) theo lệnh điều khiển | On-Demand (khi nhận lệnh) |

### 2.2. Kịch Bản Sinh Dữ Liệu Tự Động (Auto-Wave Generation)

```c
// Công thức mô phỏng dao động nhiệt độ / độ ẩm theo chu kỳ sin:
temp_val = 28.0f + 5.0f * sinf(tick_count * 0.1f);  // Dao động từ 23°C đến 33°C
hum_val  = 65.0f + 15.0f * cosf(tick_count * 0.1f); // Dao động từ 50% đến 80%
```

* **Điểm kích hoạt Rule Engine**:
  - Khi `temp_val > 30.0°C`: Rule Engine tự động phát lệnh bật Relay 1 (`fan_relay_01`).
  - Khi `temp_val < 22.0°C`: Rule Engine tự động phát lệnh bật Relay 2 (`heater_relay_01`).
  - Khi `hum_val > 75.0%`: Rule Engine tự động phát lệnh bật Relay 3 (`dehumidifier_01`).

### 2.3. Bơm Lỗi Mô Phỏng (Fault Injection Parameters)

Để kiểm thử Wave 3 (Negative tests & Circuit Breaker):
* **`mock_loss_rate`**: Tỷ lệ mất gói ngẫu nhiên (Mặc định: `0%`).
* **`mock_sensor_fault`**: Giả lập cảm biến hỏng (`valid = false`) để kích hoạt cơ chế Safe-Default.
* **`mock_bus_short`**: Giả lập chập bus RS485 (Timeout toàn bộ node).

---

## 3. Cấu Hình Giả Lập Bộ Nhớ Lưu Trữ (Virtual MicroSD Storage)

Thay vì cắm thẻ MicroSD ngoài, hệ thống dùng **Phân vùng Flash nội bộ `storage` (2MB tại offset `0x420000`)** với hệ thống tệp FATFS / LittleFS.

### 3.1. Cấu Trúc Thư Mục Trên Ổ Ảo (`/sdcard/` hoặc `/storage/`)

```
/storage/
├── config.json                     # File cấu hình Gateway (JSON)
├── data/
│   └── telemetry/
│       ├── 2026-09-15.csv          # Nhật ký telemetry theo ngày
│       └── 2026-09-16.csv
└── sync_queue.bin                  # Hàng đợi ngoại tuyến FIFO nhị phân (chống rớt mạng)
```

### 3.2. Định Dạng File Nhật Ký CSV (`telemetry/YYYY-MM-DD.csv`)
```csv
timestamp,device_id,sensor_type,value,unit,status
1726480000,sensor_temp_01,temperature,31.5,C,OK
1726480000,sensor_hum_01,humidity,72.0,%,OK
```

### 3.3. Kịch Bản Kiểm Thử Thẻ Nhớ
* **Hot-Unplug Emulation**: Lệnh CLI `mock sd eject` giả lập rút thẻ nhớ đột ngột.
* **Hot-Plug Emulation**: Lệnh CLI `mock sd insert` giả lập cắm lại thẻ, kích hoạt tự động đồng bộ NVS và backfill dữ liệu còn tồn đọng trong RAM.

---

## 4. Cấu Hình Giả Lập Màn Hình OLED SSD1327 & RTC DS3231

### 4.1. Màn Hình OLED Giả Lập (Virtual Display)
* **Kích thước ảo**: 128 x 128 pixels, 4-bit Grayscale (16 mức xám).
* **Buffer bộ nhớ**: Mảng RAM `uint8_t s_mock_display_buf[128 * 64]` (~8KB).
* **Chế độ hiển thị console**: Lệnh CLI `display show` sẽ in khung viền và nội dung text mô phỏng trực tiếp ra terminal UART0:
  ```text
  ┌──────────────────────────────┐
  │       16:45:02 (ICT)         │
  │    2026-09-16  Wednesday     │
  ├──────────────────────────────┤
  │ Temp: 31.5 °C   Hum: 72.0 %  │
  │ Wi-Fi: CONNECTED (192.168.1) │
  │ MQTT: ONLINE    Relay: [1:ON]│
  └──────────────────────────────┘
  ```

### 4.2. Đồng Hồ Thời Gian Thực RTC DS3231 Giả Lập
* **Cơ chế Fallback**: Khi I2C scanner không thấy địa chỉ `0x68`, hệ thống tự động fallback sang `esp_timer` và đồng bộ SNTP qua Wi-Fi.
* **Mô phỏng mất nguồn (Power-Loss Simulation)**: Lệnh `mock rtc reset` mô phỏng pin CMOS hết điện.

---

## 5. Bảng Ánh Xạ Chân Phần Cứng Thực Tế (Future Pinout Migration)

Khi bạn sẵn sàng cắm dây hoặc hàn linh kiện thật, chỉ cần đấu nối theo đúng sơ đồ bảng này và tắt cờ `#define USE_MOCK_PERIPHERALS`:

| Khối linh kiện | Tên chân module | Chân ESP32-S3 (Khuyến nghị) | Chức năng phần cứng |
|---|---|---|---|
| **RS485 Transceiver**<br>*(SP3485 / MAX485)* | **TXD / DI**<br>**RXD / RO**<br>**DE / RE**<br>**VCC / GND** | **GPIO 17**<br>**GPIO 16**<br>**GPIO 4**<br>3.3V / GND | UART2 TX<br>UART2 RX<br>Flow control thu/phát nửa song công<br>Nguồn nuôi module |
| **MicroSD Card Slot**<br>*(SPI Interface)* | **CS / DAT3**<br>**MOSI / CMD**<br>**MISO / DAT0**<br>**SCK / CLK**<br>**VCC / GND** | **GPIO 10**<br>**GPIO 11**<br>**GPIO 13**<br>**GPIO 12**<br>3.3V / GND | Chip Select (SPI)<br>Master Out Slave In<br>Master In Slave Out<br>SPI Clock<br>Nguồn 3.3V |
| **I2C Bus Chung**<br>*(OLED SSD1327 + RTC DS3231)* | **SDA**<br>**SCL**<br>**VCC / GND** | **GPIO 8**<br>**GPIO 9**<br>3.3V / GND | I2C Data (Trở kéo 4.7kΩ lên 3.3V)<br>I2C Clock (Trở kéo 4.7kΩ lên 3.3V)<br>Nguồn 3.3V |

---

## 6. Danh Mục Lệnh CLI Tương Tác Giả Lập (Mock CLI Commands)

Các lệnh này được tích hợp vào `serial_cli.c` để bạn có thể kiểm tra và thay đổi hành vi ngoại vi ảo trực tiếp qua cổng USB COM7:

```text
mock status                           - Hiển thị trạng thái các ngoại vi giả lập
mock rs485 start [interval_ms]        - Bắt đầu tự động phát telemetry Modbus ảo
mock rs485 stop                       - Dừng phát telemetry Modbus ảo
mock rs485 set <temp> <hum>           - Cài đặt thủ công giá trị cảm biến ảo
mock rs485 inject_error <none|crc|timeout|invalid> - Bơm lỗi giả lập vào bus RS485
mock storage write_test               - Ghi thử file nhật ký CSV vào phân vùng Flash
mock storage read_csv [lines]         - Đọc nhật ký CSV đã ghi
mock storage eject                    - Mô phỏng rút thẻ nhớ
mock storage insert                   - Mô phỏng cắm lại thẻ nhớ
display show                          - In giao diện màn hình OLED ảo ra màn hình terminal
```

---

## 7. Cách Chỉnh Sửa File Này

* Bạn có thể mở trực tiếp file [`MOCK_PERIPHERALS.md`](file:///home/chien/ai-workspace/qwen-test/MOCK_PERIPHERALS.md) để:
  1. Thêm hoặc thay đổi địa chỉ Slave ID Modbus RTU mong muốn.
  2. Thay đổi dải nhiệt độ / độ ẩm mô phỏng.
  3. Đổi số chân GPIO trong bảng Migration khi thiết kế bo mạch PCB thực tế.
