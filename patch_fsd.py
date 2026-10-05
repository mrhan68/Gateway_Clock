import re

with open('fsd-smart-clock-gateway.md', 'r') as f:
    content = f.read()

# 1. Update 9.3
q93_old = r"### 9.3 QoS & Triển Khai MQTT Broker\n\n\| Topic \| QoS \| Hướng \| Lý do \|\n\|---\|:---:\|:---:\|---\|"
q93_new = r"""### 9.3 QoS & Triển Khai MQTT Broker

> **Triển khai Broker:** Sử dụng Broker công cộng có TLS (ví dụ: HiveMQ Cloud, port 8883) thay thế cho broker.emqx.io. Gateway bắt buộc kết nối qua `mqtts://` sử dụng xác thực: **Client ID**, **Username**, và **Password**.

| Topic | QoS | Hướng | Lý do |
|---|:---:|:---:|---|"""

content = re.sub(q93_old, q93_new, content, flags=re.MULTILINE)

# 2. Update 12.4
sec124_old = r"""### 12.4 Bảo mật \(Should-have\)

\| Yêu cầu \| Mô tả \| Ghi chú \|
\|---\|---\|---\|
\| MQTT TLS \| Encrypt traffic gateway ↔ broker \| Cấu hình `esp_mqtt_client_config_t\.broker\.verification` \|
\| API Authentication \| JWT hoặc API Key cho REST API \| Xem §12\.4\.1 \|
\| Wi-Fi WPA2 \| STA mode dùng WPA2 \(không WEP/open\) \| ESP-IDF mặc định hỗ trợ \|
\| NVS encryption \| Encrypt Wi-Fi password trong NVS \| ⚠️ Xem §12\.4\.2 — rủi ro cao \|

#### 12.4.1 Phác thảo luồng Authentication

Vì hệ thống có hai loại client gọi API \(Web App và Gateway\), mỗi loại có cơ chế xác thực riêng:

```
┌─────────────────────────────────────────────────────────────────────┐
│                   Authentication & Transport Flows                  │
│                                                                     │
│  Web App \(Browser\) ──► REST API:                                   │
│  ┌──────┐    POST /auth/login     ┌──────────┐                     │
│  │ User │ ──────────────────────► │ Backend  │                     │
│  │      │ ◄────────────────────── │          │                     │
│  │      │   \{ access_token,       │          │                     │
│  │      │     refresh_token \}     │          │                     │
│  │      │                         │          │                     │
│  │      │  GET/POST /api/v1/...   │          │                     │
│  │      │  Authorization: Bearer  │          │                     │
│  │      │  <access_token>         │          │                     │
│  └──────┘                         └──────────┘                     │
│                                                                     │
│  Gateway \(ESP32-S3\) ──► MQTT over TLS \(Port 8883\):                 │
│  ┌──────────┐  CONNECT \(MQTTS\)        ┌──────────┐                  │
│  │ Gateway  │ ──────────────────────► │  Broker  │                  │
│  │          │  Client ID: gw_\{id\}     │          │                  │
│  │          │  Username:  gw_\{id\}     │          │                  │
│  │          │  Password:  <token>     │          │                  │
│  └──────────┘  \(Cấp từ Claim Code\)   └──────────┘                  │
└─────────────────────────────────────────────────────────────────────┘
```

\| Thành phần \| Cơ chế \| Chi tiết \|
\|---\|---\|---\|
\| Web App → Backend \| JWT \(access \+ refresh token\) \| Access token TTL: 15 phút\. Refresh token TTL: 7 ngày\. Lưu trong `httpOnly` cookie \(chống XSS\) \|
\| Gateway → Broker \| MQTTS \(TLS 1\.2/1\.3, port 8883\) \| Xác thực danh tính bằng Client ID, Username và Password/Token cấp phát qua quy trình Claim Code \(§8\.2\) \|
\| Backend verify \| Middleware kiểm tra JWT / Broker Auth Plugin \| Route `/api/v1/\*` yêu cầu Bearer JWT; Broker xác thực client qua webhook hoặc backend DB \|

> \*\*Lưu ý:\*\* Gateway chỉ gọi duy nhất endpoint REST là `POST /api/v1/gateways/register-claim` khi xuất xưởng/chưa claim\. Mọi giao tiếp vận hành sau đó diễn ra 100% qua MQTTS broker\."""

sec124_new = r"""### 12.4 Bảo mật (Should-have)

| Yêu cầu | Mô tả | Ghi chú |
|---|---|---|
| MQTT TLS & Auth | Encrypt traffic gateway ↔ broker (HiveMQ Cloud port 8883) | Cấu hình `esp_mqtt_client_config_t`, kết nối bằng Client ID + Username/Password. Xem FR-SEC-01 |
| API Authentication | Backend Express dùng JWT (cookie/memory), CORS siết chặt | Xem §12.4.1 |
| Local REST Auth | Gateway API (rơ-le qua LAN) bảo vệ bằng API Key / Basic Auth | Xem FR-SEC-02 |
| Wi-Fi WPA2 | STA mode dùng WPA2 (không WEP/open) | ESP-IDF mặc định hỗ trợ |
| NVS encryption | Encrypt Wi-Fi password trong NVS | ⚠️ Xem §12.4.2 — rủi ro cao |

#### 12.4.1 Phác thảo luồng Authentication & Verification Contracts

Hệ thống bảo mật ở 3 cấp độ: Backend API, MQTT Broker, và ESP32 Local API.

**1. Backend Express / Web App (JWT & CORS):**
- **JWT:** Web App login thành công sẽ nhận JWT (access token). Token này được lưu trữ an toàn trong `httpOnly` cookie hoặc bộ nhớ (memory) của trình duyệt để chống XSS.
- **CORS:** Middleware Backend chỉ allow (chấp nhận) các request từ origins hợp lệ (VD: domain hoặc IP cụ thể của Web App). Tuyệt đối loại bỏ `*`.
- **Authorization:** Tất cả các endpoint `/api/v1/...` (ngoại trừ login/public) đều yêu cầu header `Authorization: Bearer <token>`.

**2. MQTT Gateway (MQTTS & Credentials):**
- **Broker:** Sử dụng Public Broker có hỗ trợ TLS 1.2/1.3 (ví dụ HiveMQ Cloud) qua port 8883. Tuyệt đối không dùng broker unencrypted qua port 1883.
- **Authentication:** Gateway bắt buộc cung cấp **Client ID**, **Username** và **Password** khi gửi gói `CONNECT`. Không cho phép Anonymous connection.

**3. Local REST trên ESP32 (LAN Protection):**
- Ngăn chặn kẻ tấn công trên cùng LAN tự ý gửi request điều khiển rơ-le (Local Device Control).
- Bổ sung kiểm tra **API Key** (truyền qua Header như `X-API-Key`) hoặc **Basic Auth** cho các request HTTP POST/PUT gọi tới server Web cục bộ của ESP32. Nếu sai hoặc không có header này, trả về mã 401 Unauthorized.

**Verification Contract — FR-SEC-01: MQTTS TLS & Authenticated Connection**
- **Tier:** target / manual
- **Preconditions:** Gateway đã có Wi-Fi internet. Broker HiveMQ Cloud TLS đã cấp phát credentials (Client ID, User, Pass).
- **Stimulus:** Khởi động Gateway.
- **Expected:** Gói tin TLS Handshake thành công. Nhận CONNACK. Sau Handshake, các gói MQTT Control Packet đều được mã hóa.
- **Operator Instructions:**
  1. Mở Wireshark, cấu hình filter `tcp.port == 8883`.
  2. Cấp nguồn (hoặc khởi động lại) ESP32 Gateway.
  3. Quan sát bản tin Client Hello / Server Hello trên Wireshark.
  4. Xác nhận trong Serial Terminal log có dòng `MQTT_EVENT_CONNECTED`.
  5. Cố gắng kết nối bằng MQTT client khác (VD: MQTTX) tới cùng port 8883 nhưng nhập sai Password, xác nhận không thể kết nối.

**Verification Contract — FR-SEC-02: ESP32 Local REST API Authorization**
- **Tier:** target / manual
- **Preconditions:** Gateway đang hoạt động và cung cấp API nội bộ điều khiển rơ-le tại LAN (ví dụ POST `/local/relay`). Đã cấu hình Local API Key hợp lệ.
- **Stimulus:** Từ PC/điện thoại trong cùng LAN, gửi lệnh điều khiển rơ-le (1) không có Header Auth hợp lệ và (2) kèm Header Auth hợp lệ.
- **Expected:** Trường hợp (1) trả về mã lỗi HTTP 401, rơ-le không chuyển trạng thái. Trường hợp (2) trả về HTTP 200 OK và rơ-le chuyển trạng thái.
- **Operator Instructions:**
  1. Kết nối PC vào cùng mạng Wi-Fi với Gateway. Lấy IP Gateway (VD: `192.168.1.50`).
  2. Gửi lệnh cURL: `curl -X POST http://192.168.1.50/local/relay -d '{"state":"ON"}'`. Xác nhận HTTP status code 401. Rơ-le không kêu.
  3. Gửi lệnh cURL: `curl -X POST http://192.168.1.50/local/relay -H 'X-API-Key: <SECRET>' -d '{"state":"ON"}'`. Xác nhận HTTP 200 OK. Rơ-le kêu tách và đèn báo đổi trạng thái."""

content = re.sub(sec124_old, sec124_new, content, flags=re.DOTALL)

with open('fsd-smart-clock-gateway.md', 'w') as f:
    f.write(content)
