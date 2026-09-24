# 🔌 Hướng Dẫn Nối Chân & Thiết Kế Mạch Ngoại Vi (Hardware Wiring & Component Guide)
### Dự án: Smart Clock & Industrial Edge IoT Gateway (ESP32-S3)
*Tài liệu chuẩn hóa kỹ thuật phần cứng — Đồng bộ 100% với Firmware v1.6.0 & FSD*

---

## 1. Bảng Phân Bổ Chân GPIO ESP32-S3 (Comprehensive Pinout)

Hệ thống sử dụng vi điều khiển **ESP32-S3** (Dual-Core Xtensa LX7 @ 160MHz/240MHz, 16MB Flash, 8MB PSRAM). Toàn bộ các chân GPIO đã được gán cố định và bảo vệ trong firmware:

| GPIO Pin | Hướng | Tín hiệu | Thiết bị kết nối | Mức tích cực | Điện trở & Tụ đệm khuyến nghị |
|:---:|:---:|---|---|:---:|---|
| **GPIO 0** | IN | `BTN_BOOT` | Nút BOOT / Snooze (+9m) / Reset SoftAP | LOW | Pull-up 10kΩ lên 3.3V, Tụ lọc dội 100nF xuống GND, Trở bảo vệ 220Ω |
| **GPIO 1** | OUT | `BUZZER` | Còi báo thức (Active/Passive Buzzer) | HIGH | Transistor NPN (S8050), Trở Base 1kΩ, Trở xả Base 10kΩ, Diode 1N4148 |
| **GPIO 4** | IN | `BTN_SCREEN` | Nút Next Screen / Dismiss Alarm (<500ms) | LOW | Pull-up 10kΩ lên 3.3V, Tụ lọc dội 100nF xuống GND, Trở bảo vệ 220Ω |
| **GPIO 8** | BIDIR | `I2C0_SDA` | Bus I2C0: OLED SSD1327 + RTC DS3231 | — | Pull-up 3.3kΩ – 4.7kΩ lên 3.3V (nếu module chưa tích hợp) |
| **GPIO 9** | OUT | `I2C0_SCL` | Bus I2C0: OLED SSD1327 + RTC DS3231 | — | Pull-up 3.3kΩ – 4.7kΩ lên 3.3V (nếu module chưa tích hợp) |
| **GPIO 10** | OUT | `SD_CS` | MicroSD SPI Chip Select | LOW | Pull-up 10kΩ – 47kΩ lên 3.3V, Trở đệm nối tiếp 22Ω – 33Ω |
| **GPIO 11** | OUT | `SD_MOSI` | MicroSD SPI Master Out Slave In | — | Trở nối tiếp triệt sóng phản xạ 22Ω – 33Ω |
| **GPIO 12** | OUT | `SD_CLK` | MicroSD SPI Clock (SPI2_HOST) | — | Trở nối tiếp triệt sóng phản xạ 22Ω – 33Ω |
| **GPIO 13** | IN | `SD_MISO` | MicroSD SPI Master In Slave Out | — | Pull-up nhẹ 10kΩ – 47kΩ lên 3.3V |
| **GPIO 15** | OUT | `RELAY_1` | Rơ-le 1: Quạt làm mát (Fan) | HIGH | Pull-down 10kΩ xuống GND, Optocoupler cách ly quang |
| **GPIO 16** | OUT | `RELAY_2` | Rơ-le 2: Máy sưởi ấm (Heater) | HIGH | Pull-down 10kΩ xuống GND, Optocoupler cách ly quang |
| **GPIO 17** | OUT | `RELAY_3` | Rơ-le 3: Hút ẩm / Máy nén (Dehumidifier) | HIGH | Pull-down 10kΩ xuống GND, Optocoupler cách ly quang |
| **GPIO 18** | OUT | `RELAY_4` | Rơ-le 4: Đèn / Tải phụ trợ (Aux) | HIGH | Pull-down 10kΩ xuống GND, Optocoupler cách ly quang |

> [!CAUTION]
> **Điện áp logic sống còn**: Toàn bộ GPIO của ESP32-S3 hoạt động ở chuẩn **3.3V Logic**.
> **Tuyệt đối KHÔNG cấp trực tiếp nguồn 5V** vào bất kỳ chân tín hiệu nào của ESP32-S3 (bao gồm I2C, SPI, Buttons). Nguồn 5V chỉ dùng cấp nuôi mạch công suất (cuộn hút rơ-le, chân VIN của kit).

---

## 2. Thiết Kế Mạch Nút Nhấn (Button Circuit & Hardware Debounce)

### 2.1. Vị trí & Công dụng Nút
1. **Nút 1 — `BTN_BOOT` (Nối vào GPIO 0)**:
   - *Công dụng*: Nhấn nhả khi chuông reo để hoãn báo thức (`Snooze +9 phút`). Nhấn giữ $> 5\text{s}$ để xóa Wi-Fi và phát lại mạng SoftAP `SmartClock-Setup`.
   - *Lưu ý kỹ thuật*: **GPIO 0 là chân Strapping Pin** của ESP32-S3 (nếu bị kéo LOW khi vừa cấp nguồn hoặc bấm EN/RESET, chip sẽ vào chế độ nạp ROM bootloader thay vì chạy firmware). Vì vậy: **BẮT BUỘC dùng cấu hình kéo lên 3.3V (Pull-up)**, không được đấu kéo xuống GND thường trực!
2. **Nút 2 — `BTN_SCREEN` (Nối vào GPIO 4)**:
   - *Công dụng*: Nhấn nhả $< 500\text{ms}$ khi báo thức đang reo để dập tắt chuông tức thì (`Dismiss Alarm`). Ở chế độ bình thường: Chuyển đổi qua lại giữa Màn hình 0 (Đồng hồ & Cảm biến), Màn hình 1 (Lịch trình sắp tới), và Màn hình 2 (Trạng thái mạng & Gateway).

### 2.2. Sơ Đồ Nguyên Lý Mạch Lọc Dội (RC Debounce Circuit)

Nút bấm cơ khí luôn sinh ra hiện tượng dội tiếp điểm (contact bounce) kéo dài từ 5ms đến 20ms, dễ làm firmware hiểu nhầm thành nhiều lần bấm liên tiếp. Mạch lọc phần cứng dưới đây triệt tiêu 100% hiện tượng này:

```
        +3.3V
          │
          [R_pull: 10kΩ] (Dải chọn: 4.7kΩ - 10kΩ)
          │
          ├─────────────────────────[R_protect: 220Ω]──────> Đến chân GPIO (GPIO 0 hoặc GPIO 4)
          │                                (Dải: 100Ω - 330Ω)
        ──o── (Tiếp điểm nút nhấn)
          │
          ├──────────────┐
          │              │
       [SW Tact]   [C_debounce: 100nF / 0.1µF] (Mã 104, Dải: 10nF - 100nF)
          │              │
         GND            GND
```

### 2.3. Hướng Dẫn Chọn Trở & Tụ Cho Nút Bấm
* **Điện trở kéo lên ($R_{\text{pull}}$)**:
  - **Dải giá trị**: **$4.7\text{k}\Omega - 10\text{k}\Omega$** (Chuẩn tốt nhất: **$10\text{k}\Omega$**).
  - *Lý do*: $10\text{k}\Omega$ tiêu thụ dòng cực nhỏ ($I = 3.3\text{V} / 10\text{k}\Omega = 0.33\text{mA}$) khi bấm nút, đồng thời giữ mức logic HIGH rất vững chắc, chống nhiễu bắt sóng điện từ từ ăng-ten Wi-Fi/BLE lân cận.
* **Tụ lọc dội ($C_{\text{debounce}}$)**:
  - **Dải giá trị**: **$10\text{nF} - 100\text{nF}$** (Tụ gốm nhiều lớp MLCC, mã ghi **104** $= 100\text{nF}$).
  - *Tính toán hằng số thời gian*: $\tau = R_{\text{pull}} \times C = 10\text{k}\Omega \times 100\text{nF} = 1\text{ms}$. Tín hiệu sẽ chuyển tiếp mượt mà, cắt sạch gai dội cơ khí mà tay người bấm không cảm nhận thấy bất kỳ độ trễ nào.
* **Điện trở bảo vệ nối tiếp ($R_{\text{protect}}$)**:
  - **Dải giá trị**: **$100\Omega - 330\Omega$** (Chuẩn: **$220\Omega$**).
  - *Lý do*: Khi bấm nút, tụ $C$ xả điện cực nhanh về GND. Trở này hạn chế xung dòng phóng điện bảo vệ tiếp điểm nút bấm và bảo vệ cổng bảo vệ ESD bên trong chip ESP32-S3.

---

## 3. Bus I2C: Màn Hình OLED SSD1327 & Module RTC DS3231

Cả 2 thiết bị chia sẻ chung bus `I2C0` trên **GPIO 8 (SDA)** và **GPIO 9 (SCL)** với tốc độ Fast Mode $400\text{kHz}$:

```
     ESP32-S3                   Màn Hình SSD1327 (0x3C)         Module DS3231 (0x68)
  ┌────────────┐               ┌───────────────────────┐       ┌────────────────────┐
  │   GPIO 8   ├───────┬───────┤ SDA                   ├───────┤ SDA                │
  │   (SDA)    │       │       │                       │       │                    │
  │   GPIO 9   ├─┬─────┼───────┤ SCL                   ├───────┤ SCL                │
  │   (SCL)    │ │     │       │                       │       │                    │
  │    3.3V    ├─┼──┐  │       │ VCC                   │   ┌───┤ VCC                │
  │    GND     ├─┼──┼──┼───────┤ GND                   │   │ ┌─┤ GND                │
  └────────────┘ │  │  │       └───────────────────────┘   │ │ └────────────────────┘
                 │  │  │                                   │ │
           [3.3kΩ] [3.3kΩ]                                [100nF]
                 │  │  │                                   │ │
                 └──┴──┘ (Kéo lên 3.3V)                   GND GND
```

### 3.1. Hướng Dẫn Chọn Trở Kéo Lên I2C (Pull-up Resistors)
* **Dải giá trị**: **$2.2\text{k}\Omega - 4.7\text{k}\Omega$** (Chuẩn khuyến nghị: **$3.3\text{k}\Omega$** khi chạy $400\text{kHz}$).
* **Kiểm tra thực tế linh kiện**:
  - Module đồng hồ RTC DS3231 (ZS-042) và bo mạch OLED SSD1327 thường đã hàn sẵn 2 con trở dán SMD $4.7\text{k}\Omega$ hoặc $10\text{k}\Omega$ trên đường SDA/SCL.
  - *Cách kiểm tra*: Dùng đồng hồ VOM đo thang ôm giữa chân SDA với chân VCC của module khi chưa cấp nguồn:
    - Nếu đo được khoảng $4.7\text{k}\Omega$: **Không cần hàn thêm trở ngoài**.
    - Nếu đo được $> 10\text{k}\Omega$ hoặc vô cùng: **Bắt buộc hàn 2 trở $3.3\text{k}\Omega$ hoặc $4.7\text{k}\Omega$** nối từ SDA lên 3.3V và SCL lên 3.3V.

### 3.2. Cảnh Báo An Toàn Module RTC DS3231 (ZS-042)
* Module ZS-042 trên thị trường thường có mạch diode + trở sạc pin dự phòng. Nếu bạn dùng pin cúc áo **CR2032 thông thường (không sạc được)**, mạch sạc này sẽ nạp dòng ngược gây phồng/chập nổ pin!
* *Giải pháp*: Dùng mỏ hàn nhấc bỏ con diode (thường có ký hiệu `D1` cạnh trở `R1` 200Ω) trên module DS3231, hoặc thay bằng pin sạc `LIR2032` 3.6V.

---

## 4. Giao Tiếp Thẻ Nhớ MicroSD (SPI2_HOST)

Giao tiếp SPI tần số cao ($20\text{MHz}$) rất nhạy cảm với sụt áp và suy hao đường truyền:

```
  ESP32-S3                    Khe Cắm MicroSD (SPI Mode)
┌───────────┐                 ┌──────────────────────────┐
│  GPIO 10  ├──[ 33Ω ]────────┤ CS   (Kéo lên 10kΩ 3.3V) │
│  GPIO 11  ├──[ 33Ω ]────────┤ MOSI (DI)                │
│  GPIO 12  ├──[ 33Ω ]────────┤ CLK  (SCLK)              │
│  GPIO 13  ├──[ 33Ω ]────────┤ MISO (DO)                │
│   3.3V    ├──┬──────────────┤ VDD                      │
│   GND     ├──┼──────────────┤ VSS                      │
└───────────┘  │              └──────────────────────────┘
             ┌─┴─────────┐
          [22µF]      [100nF]
             │           │
            GND         GND
```

### 4.1. Hướng Dẫn Chọn Tụ & Trở Cho Thẻ SD
* **Tụ bù sụt áp tức thời (Bypass/Bulk Capacitor)**:
  - **Bắt buộc**: Cần mắc song song **1 tụ Tantalum hoặc tụ Hóa $10\mu\text{F} - 47\mu\text{F}$** (khuyến nghị **$22\mu\text{F}$**) cùng **1 tụ gốm $100\text{nF}$** đặt **ngay sát chân VDD của khe thẻ nhớ** (càng gần càng tốt $< 1\text{cm}$).
  - *Giải thích*: Thẻ SD tiêu thụ dòng tĩnh chỉ $\approx 1\text{mA}$, nhưng khi chip kích hoạt ghi cụm dữ liệu (NAND write block), nó rút dòng xung đột ngột vọt lên $80\text{mA} - 120\text{mA}$ trong vài micro-giây. Nếu thiếu tụ đệm này, nguồn 3.3V sẽ sụt xuống dưới $2.7\text{V}$, làm ESP32-S3 bị Brownout Reset ngay lập tức!
* **Trở triệt sóng phản xạ (Damping Series Resistors)**:
  - Mắc nối tiếp trở **$22\Omega - 47\Omega$** (chuẩn: **$33\Omega$**) trên các đường `CLK`, `MOSI`, `CS` nếu dây nối dài $> 5\text{cm}$ để làm phẳng sườn xung vuông và giảm bức xạ EMI.
* **Trở kéo lên chân CS**:
  - Trở **$10\text{k}\Omega - 47\text{k}\Omega$** kéo chân `GPIO 10` lên 3.3V, đảm bảo thẻ nhớ luôn ở trạng thái Unselected (ngủ) trong suốt quá trình ESP32 nạp bootloader.

---

## 5. Mạch Điều Khiển Rơ-le 4 Kênh (Relay Isolation & Anti-Chatter)

Các chân GPIO 15, 16, 17, 18 điều khiển trực tiếp 4 rơ-le 220VAC. Đây là nơi phát sinh nhiễu cảm ứng điện từ lớn nhất hệ thống:

```
  ESP32-S3 (3.3V Logic)               Module Relay 4 Kênh Cách Ly Quang
┌───────────┐                         ┌──────────────────────────────────────┐
│  GPIO 15  ├──┬──────────────────────┤ IN1 (Quạt - Fan)                     │
│  GPIO 16  ├──┼─┬────────────────────┤ IN2 (Sưởi - Heater)                  │
│  GPIO 17  ├──┼─┼─┬──────────────────┤ IN3 (Hút ẩm - Dehumidifier)          │
│  GPIO 18  ├──┼─┼─┼─┬────────────────┤ IN4 (Đèn phụ - Aux)                  │
│   GND     ├──┼─┼─┼─┼──┬─────────────┤ GND                                  │
│           │  │ │ │ │  │             │ VCC (Tín hiệu logic: 3.3V hoặc 5V)   │
└───────────┘  │ │ │ │  │             │                                      │
               │ │ │ │  │             │ JD-VCC (Nguồn nuôi cuộn hút: 5V DC)  │
              [10kΩ x 4]│             └──────────────────────────────────────┘
               │ │ │ │  │                        │                 │
              GND GND GND GND                 +5V Adapter         GND Nguồn 5V
             (Pull-down an toàn)
```

### 5.1. Trở Kéo Xuống Chống Đóng Nhầm Khi Khởi Động (Anti-Chatter Pull-down)
* **Dải giá trị**: **$4.7\text{k}\Omega - 10\text{k}\Omega$** (Chuẩn tốt nhất: **$10\text{k}\Omega$**).
* **Vị trí**: Nối từ mỗi chân `GPIO 15`, `GPIO 16`, `GPIO 17`, `GPIO 18` xuống GND.
* **Tầm quan trọng sống còn**:
  - Khi ESP32-S3 vừa cấp điện hoặc reset, các chân GPIO ở trạng thái trở kháng cao (High-Z Float).
  - Tín hiệu trôi nổi có thể vô tình kích hoạt Optocoupler, làm cuộn hút nhảy đóng ngắt loạn xạ (relay chatter). Điều này **cực kỳ nguy hiểm** đối với Máy nén / Máy sưởi công suất lớn (FSD §4.10).
  - Trở $10\text{k}\Omega$ đảm bảo chân luôn ghì chặt ở mức `0V (LOW)` cho đến khi firmware làm chủ hoàn toàn phần cứng.

### 5.2. Cách Ly Nguồn Quang Học (Optocoupler Isolation)
* Trên module rơ-le 4 kênh chuẩn, luôn có 1 jumper nối giữa chân `VCC` và `JD-VCC`.
* **Khuyến nghị chuẩn công nghiệp**:
  1. **Rút bỏ Jumper JD-VCC**: Tách riêng hoàn toàn nguồn cuộn hút và nguồn vi điều khiển.
  2. Cấp nguồn **5V riêng biệt (Adapter 5V/2A)** vào chân `JD-VCC` và `GND` của rơ-le.
  3. Chân `VCC` của module rơ-le nối vào nguồn `3.3V` của ESP32 để cấp nguồn cho LED bên trong Optocoupler.
  4. Tiếp điểm đầu ra rơ-le đóng cắt tải 220VAC: Mắc thêm mạch **RC Snubber (100Ω 2W nối tiếp tụ 100nF 400V)** song song với 2 tiếp điểm rơ-le để dập sạch tia lửa điện hồ quang khi ngắt tải cảm ứng (quạt, máy nén).

---

## 6. Mạch Còi Báo Thức (Buzzer Driver Circuit)

Chân `GPIO 1` chỉ có thể cấp dòng tối đa $20\text{mA} - 40\text{mA}$, không đủ để nuôi còi báo động trực tiếp (còi thường ăn $30\text{mA} - 80\text{mA}$). Cần dùng transistor đệm:

```
                  +5V (hoặc +3.3V)
                     │
                    [+] Còi Báo Thức (Buzzer)
                    [-]
                     │
                     ├───────────[ Diode 1N4148 / 1N4007 ] (Dập xung ngược cuộn cảm)
                     │           (Catốt nối +5V, Anốt nối cực C)
                   C │
     GPIO 1 ──[R_B: 1kΩ]──┤ B  (Transistor NPN: S8050 hoặc 2N2222)
                     │    E
                    [R_BE: 10kΩ]
                     │    │
                    GND  GND
```

* **Điện trở phân cực cực Base ($R_B$)**: **$1\text{k}\Omega - 2.2\text{k}\Omega$** (Đảm bảo transistor bão hòa hoàn toàn khi GPIO ở mức HIGH).
* **Điện trở xả cực Base ($R_{BE}$)**: **$10\text{k}\Omega$** nối từ Base xuống GND để chống còi kêu rít rè rè lúc chip đang boot.
* **Diode dập xung ngược**: Mắc song song ngược cực tính với còi để triệt tiêu điện áp cảm ứng ngược đánh thủng transistor khi ngắt dòng.

---

## 7. Bảng Tổng Hợp Linh Kiện Cần Chuẩn Bị (Bill of Materials - BOM)

| Loại Linh Kiện | Trị Số / Mã Hiệu | Số Lượng | Dải Khả Dụng | Vị Trí Lắp Đặt |
|---|:---:|:---:|:---:|---|
| **Điện trở** | **$10\text{k}\Omega$** (1/4W) | **7 con** | $4.7\text{k}\Omega - 10\text{k}\Omega$ | Pull-up Button (2), Pull-down Relay (4), Trở xả Buzzer (1) |
| **Điện trở** | **$3.3\text{k}\Omega$** (1/4W) | **2 con** | $2.2\text{k}\Omega - 4.7\text{k}\Omega$ | Pull-up I2C0 SDA & SCL (GPIO 8, 9) |
| **Điện trở** | **$1\text{k}\Omega$** (1/4W) | **1 con** | $1\text{k}\Omega - 2.2\text{k}\Omega$ | Phân cực cực Base Transistor còi (GPIO 1) |
| **Điện trở** | **$220\Omega$** (1/4W) | **2 con** | $100\Omega - 330\Omega$ | Bảo vệ hạn dòng phóng tụ nút nhấn (GPIO 0, 4) |
| **Điện trở** | **$33\Omega$** (1/4W) | **3 con** | $22\Omega - 47\Omega$ | Trở đệm nối tiếp đường SPI SD (CS, MOSI, CLK) |
| **Tụ gốm MLCC** | **$100\text{nF}$** ($0.1\mu\text{F}$ / 104) | **6 con** | $10\text{nF} - 100\text{nF}$ | Lọc dội nút nhấn (2), Lọc nhiễu cao tần VCC OLED (1), RTC (1), SD (1), Nguồn ESP32 (1) |
| **Tụ hóa / Tantalum** | **$22\mu\text{F} - 47\mu\text{F}$** ($\ge 10\text{V}$) | **1 con** | $10\mu\text{F} - 100\mu\text{F}$ | Bù sụt áp tức thời ngay sát chân VDD khe thẻ nhớ MicroSD |
| **Tụ hóa Bulk** | **$220\mu\text{F} - 470\mu\text{F}$** ($\ge 16\text{V}$) | **1 con** | $100\mu\text{F} - 1000\mu\text{F}$ | Lọc nguồn chính 5V đầu vào adapter, gánh dòng đỉnh Wi-Fi/Relay |
| **Transistor NPN** | **S8050** hoặc **2N2222** | **1 con** | — | Kích công suất còi báo thức buzzer |
| **Diode** | **1N4148** hoặc **1N4007** | **1 con** | — | Dập xung ngược cảm ứng cho còi buzzer |
| **Nút bấm** | **Tactile Switch 6x6mm** | **2 cái** | 2 chân hoặc 4 chân | Nút BOOT (GPIO 0) và Nút SCREEN (GPIO 4) |

---

## 8. Hướng Dẫn Bố Trí Dây & Hàn Mạch (Assembly Best Practices)

1. **Quy tắc Tiếp Địa (Star Grounding)**:
   - Gom tất cả các điểm mass (GND) của tụ lọc, nút bấm và cảm biến về **1 điểm GND duy nhất trên bo ESP32-S3**. Tránh nối dây mass hình vòng tròn (Ground loop) gây nhiễu sóng ADC và I2C.
2. **Chiều Dài Dây Dẫn**:
   - Dây I2C (OLED, RTC): Chiều dài $\le 15\text{cm}$.
   - Dây SPI (Thẻ SD): Chiều dài $\le 10\text{cm}$ (càng ngắn càng tốt vì tần số quét lên tới $20\text{MHz}$).
3. **Nguồn Cấp Độc Lập**:
   - Khuyến nghị dùng củ sạc điện thoại **5V $\ge 2\text{A}$** cấp vào cổng USB hoặc chân 5V/VIN của bo mạch. Không nên cắm qua cổng USB máy tính cũ (chỉ cấp tối đa 500mA) khi vừa chạy Wi-Fi vừa bật cả 4 rơ-le.
