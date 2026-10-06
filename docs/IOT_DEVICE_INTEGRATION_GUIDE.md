# Hướng Dẫn Kết Nối Thiết Bị IoT Vào Smart Clock Gateway

Tài liệu hướng dẫn kỹ thuật dành cho nhà phát triển thiết bị ngoại vi (cảm biến, cơ cấu chấp hành) kết nối và truyền nhận dữ liệu với **ESP32-S3 Smart Clock & IoT Gateway**.

---

## 1. Tổng Quan Các Phương Thức Kết Nối

Gateway hỗ trợ 2 giao thức chính để thiết bị IoT giao tiếp:

```
┌─────────────────────────────────┐           ┌─────────────────────────────────┐
│     BLE Cảm Biến Môi Trường     │           │      Thiết Bị LAN / Wi-Fi       │
│  (BTHome v2, ESS 0x181A, Name)  │           │   (Microcontroller, RPi, ESP)   │
└────────────────┬────────────────┘           └────────────────┬────────────────┘
                 │ Sóng BLE Broadcast                          │ HTTP REST + X-API-Key
                 ▼                                             ▼
┌───────────────────────────────────────────────────────────────────────────────┐
│                    ESP32-S3 SMART CLOCK & IOT GATEWAY                         │
│                    (IP: 192.168.1.199 / SoftAP: 192.168.4.1)                  │
└──────────────────────────────────────┬────────────────────────────────────────┘
                                       │ MQTTS / TLS 8883
                                       ▼
                       ┌───────────────────────────────┐
                       │  Cloud Backend / MQTT Broker  │
                       └───────────────────────────────┘
```

| Giao thức | Loại thiết bị | Cơ chế kết nối | Phạm vi ứng dụng |
|---|---|---|---|
| **BLE (Passive Scan)** | Cảm biến pin (Nhiệt/Ẩm, Nút bấm) | Không ghép đôi (Non-connectable Advertising) | Tiết kiệm pin tối đa, chu kỳ phát 2s - 60s |
| **Local REST API** | Vi điều khiển Wi-Fi trong cùng mạng LAN | HTTP POST / GET + Header `X-API-Key` | Bật/tắt rơ-le, đọc trạng thái thời gian thực |
| **MQTT Gateway Bridge** | Thiết bị thông minh qua Broker | Publish / Subscribe topic phân cấp | Điều khiển hai chiều qua Cloud / Backend |

---

## 2. Kết Nối Cảm Biến Qua Sóng BLE (Không Dây)

Gateway chạy **NimBLE GAP Passive Scanner** ở chế độ nền. Cảm biến chỉ cần **quảng bá gói tin định kỳ (BLE Advertisement)**, Gateway tự động bắt gói tin, trích xuất dữ liệu, định tuyến vào Event Bus và hiển thị lên màn hình OLED.

### 2.1. Đặt Tên Thiết Bị (Device Name Convention)
Gateway tự động lọc và đăng ký các thiết bị có tên bắt đầu bằng `sensor_` (hoặc thiết bị đã đăng ký thủ công qua lệnh CLI `ble reg <device_id> [MAC]`).

* **Quy ước đặt tên**: `sensor_<loại>_<số_thứ_tự>` (Ví dụ: `sensor_temp_01`, `sensor_livingroom`).

---

### 2.2. Các Định Dạng Gói Tin BLE Được Hỗ Trợ

#### Định dạng A: Chuẩn Mở BTHome V2 (Khuyên Dùng)
BTHome v2 là chuẩn mở quốc tế cho các cảm biến nhà thông minh, truyền trong trường **Service Data 16-bit UUID `0xFCD2`**.

* **Cấu trúc Payload**:
  ```
  Byte 0..1 : 0xD2, 0xFC (UUID 0xFCD2 - Little Endian)
  Byte 2    : 0x40 (BTHome V2 Header: BTHome V2, Unencrypted)
  Byte 3    : 0x02 (Object ID: Nhiệt độ)
  Byte 4..5 : Raw Temp (int16_t, đơn vị 0.01 °C, Little Endian)
  Byte 6    : 0x03 (Object ID: Độ ẩm)
  Byte 7..8 : Raw Humidity (uint16_t, đơn vị 0.01 %RH, Little Endian)
  Byte 9    : 0x01 (Object ID: Pin - Tùy chọn)
  Byte 10   : Pin % (uint8_t: 0 - 100)
  ```

* **Ví dụ mẫu**: Nhiệt độ = `28.50 °C` (raw `2850` = `0x0B22`), Độ ẩm = `65.00 %` (raw `6500` = `0x1964`):
  ```hex
  D2 FC 40 02 22 0B 03 64 19
  ```

---

#### Định dạng B: Bluetooth SIG Environmental Sensing (UUID `0x181A`)
Chuẩn Bluetooth SIG chính thống, truyền trong trường **Service Data 16-bit UUID `0x181A`**.

* **Cấu trúc Payload**:
  ```
  Byte 0..1 : 0x1A, 0x18 (UUID 0x181A - Little Endian)
  Byte 2..3 : Raw Temp (int16_t, đơn vị 0.01 °C, Little Endian)
  Byte 4..5 : Raw Humidity (uint16_t, đơn vị 0.01 %, Little Endian - Tùy chọn)
  ```

* **Ví dụ mẫu**: Nhiệt độ = `31.25 °C` (raw `3125` = `0x0C35`):
  ```hex
  1A 18 35 0C
  ```

---

#### Định dạng C: Tên Kèm Giá Trị (Simple Name Fallback)
Dành cho người dùng phát tin nhanh từ ứng dụng điện thoại (như **nRF Connect** cho Android/iOS) hoặc board Arduino chưa cấu hình BTHome:

* Đặt Complete Local Name (AD Type `0x09`):
  * Cú pháp: `sensor_temp_01:<giá_trị>` (Ví dụ: `sensor_temp_01:32.5`)
  * Nếu chỉ đặt tên `sensor_temp_01` không kèm số, Gateway tự gán mặc định `31.50 °C` (ngưỡng kích hoạt bật quạt làm mát).

---

### 2.3. Ví Dụ Code Arduino / ESP32 Phát Tin BTHome v2

Dành cho board phụ (ESP32 / ESP32-C3 / ESP32-C6) làm cảm biến:

```cpp
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEAdvertising.h>

void setup() {
    Serial.begin(115200);
    BLEDevice::init("sensor_temp_01");

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();

    // Dữ liệu đo đạc giả định: 29.50 °C và 60.00 %RH
    int16_t temp_raw = 2950;
    uint16_t hum_raw = 6000;

    // Đóng gói khung BTHome v2
    uint8_t bthome_data[9] = {
        0xD2, 0xFC,       // UUID 0xFCD2
        0x40,             // BTHome V2 header (unencrypted)
        0x02,             // ID: Temperature
        (uint8_t)(temp_raw & 0xFF), (uint8_t)((temp_raw >> 8) & 0xFF),
        0x03,             // ID: Humidity
        (uint8_t)(hum_raw & 0xFF), (uint8_t)((hum_raw >> 8) & 0xFF)
    };

    BLEAdvertisementData advData;
    advData.setName("sensor_temp_01");
    advData.addData(std::string((char*)bthome_data, sizeof(bthome_data)));

    pAdvertising->setAdvertisementData(advData);
    pAdvertising->setScanResponse(false);
    pAdvertising->setMinInterval(1600); // 1000ms
    pAdvertising->setMaxInterval(3200); // 2000ms
    pAdvertising->start();

    Serial.println("BLE Sensor broadcast started!");
}

void loop() {
    delay(10000);
}
```

---

## 3. Kết Nối Điều Khiển Qua Local REST API (LAN)

Nếu thiết bị IoT của bạn kết nối cùng mạng Wi-Fi với Gateway (hoặc cắm Ethernet qua cùng Router), thiết bị có thể gửi lệnh điều khiển trực tiếp mà không cần internet.

### 3.1. Xác Thực (Authentication Contract FR-SEC-02)
Mọi yêu cầu gửi đến các endpoint điều khiển phải kèm theo header:
```http
X-API-Key: <LOCAL_API_KEY>
```
* **Lấy API Key**:
  * Xem trên màn hình console UART qua lệnh: `config show`.
  * Hoặc đặt khóa riêng bằng lệnh: `config set apikey <khoa_bi_mat_8_den_32_ky_tu>`.

---

### 3.2. Bảng Endpoint Điều Khiển

#### A. Điều Khiển Rơ-Le Phần Cứng (Relay Control)
* **Endpoint**: `POST http://<GATEWAY_IP>/api/relay`
* **Headers**:
  * `Content-Type: application/json`
  * `X-API-Key: <LOCAL_API_KEY>`
* **Payload JSON**:
  ```json
  {
    "target": "relay_1",
    "command": "ON"
  }
  ```
  * `target`: `relay_1` (Quạt - GPIO 15), `relay_2` (Sưởi - GPIO 16), `relay_3` (Hút ẩm - GPIO 17), `relay_4` (Aux - GPIO 18).
  * `command`: `ON` hoặc `OFF`.
* **Phản hồi**:
  * Thành công: `200 OK`
  * Sai hoặc thiếu key: `401 Unauthorized`

---

#### B. Đọc Trạng Thái Sức Khỏe Gateway
* **Endpoint**: `GET http://<GATEWAY_IP>/api/status`
* **Xác thực**: Không yêu cầu (Public LAN).
* **Phản hồi JSON mẫu**:
  ```json
  {
    "uptime_s": 3600,
    "free_heap": 48200,
    "ip": "192.168.1.199",
    "wifi_connected": true,
    "mqtt_connected": true,
    "active_mask": 127,
    "alive_mask": 127,
    "time": "2026-10-06 17:30:00"
  }
  ```

---

#### C. Đọc Dữ Liệu Đo Đạc Hiện Tại
* **Endpoint**: `GET http://<GATEWAY_IP>/api/telemetry`
* **Xác thực**: Không yêu cầu.
* **Phản hồi JSON mẫu**:
  ```json
  {
    "temp": 31.2,
    "hum": 62.0,
    "valid": true,
    "relays": [1, 0, 0, 0]
  }
  ```

---

### 3.3. Ví Dụ Lệnh cURL & Python Giao Tiếp LAN

#### Lệnh cURL từ Terminal:
```bash
# 1. Bật Quạt (Relay 1)
curl -X POST http://192.168.1.199/api/relay \
     -H "Content-Type: application/json" \
     -H "X-API-Key: 9304fc1f753ef509" \
     -d '{"target": "relay_1", "command": "ON"}'

# 2. Tắt Quạt (Relay 1)
curl -X POST http://192.168.1.199/api/relay \
     -H "Content-Type: application/json" \
     -H "X-API-Key: 9304fc1f753ef509" \
     -d '{"target": "relay_1", "command": "OFF"}'
```

#### Code Python Tự Động Hóa:
```python
import requests

GATEWAY_IP = "192.168.1.199"
API_KEY = "9304fc1f753ef509"

def set_relay(channel: int, state: bool):
    url = f"http://{GATEWAY_IP}/api/relay"
    headers = {
        "Content-Type": "application/json",
        "X-API-Key": API_KEY
    }
    payload = {
        "target": f"relay_{channel}",
        "command": "ON" if state else "OFF"
    }
    res = requests.post(url, json=payload, headers=headers, timeout=3)
    if res.status_code == 200:
        print(f"Relay {channel} -> {'ON' if state else 'OFF'} SUCCESS")
    else:
        print(f"Lỗi: {res.status_code} - {res.text}")

# Thử nghiệm bật Relay 1
set_relay(1, True)
```

---

## 4. Cấu Trúc Topic MQTT (Giao Tiếp Qua Broker)

Khi điều khiển phân tán qua MQTT Broker (HiveMQ Cloud / EMQX):

| Hướng dữ liệu | Topic | Định dạng Payload |
|---|---|---|
| **Gateway ➔ Cloud** | `gateway/{gateway_id}/telemetry` | `{"uptime_s": 120, "free_heap": 48900, "ip": "192.168.1.199"}` |
| **Cloud ➔ Gateway** | `gateway/{gateway_id}/device/{device_id}/command` | `{"command_id": "uuid", "target_device_id": "relay_1", "command": "ON"}` |
| **Gateway ➔ Cloud** | `gateway/{gateway_id}/device/{device_id}/command_ack` | `{"command_id": "uuid", "status": "SUCCESS"}` |

---

## 5. Hướng Dẫn Debug & Kiểm Tra Kết Nối

1. **Kiểm tra Gateway có nhận được sóng BLE không**:
   * Mở cổng nối tiếp Serial (Baudrate `115200`):
   * Gõ lệnh: `ble status` -> Kiểm tra số lượng thiết bị BLE đang được theo dõi và đếm gói tin.
2. **Bơm thử bản tin kiểm tra không cần phần cứng**:
   * Gõ lệnh CLI: `ble test 33.5` -> Bơm gói tin nhiệt độ 33.5 °C vào hệ thống để kiểm tra xem rơ-le có tự động nhảy theo Rule hay không.
3. **Kiểm tra trạng thái Wi-Fi & IP**:
   * Gõ lệnh CLI: `wifi status` để lấy địa chỉ IP hiện tại của Gateway.
