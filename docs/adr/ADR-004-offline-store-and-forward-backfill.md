# ADR-004: Hàng Đợi Lưu Trữ Ngoại Tuyến và Điều Tiết Tốc Độ Gửi Bù (Offline Store-and-Forward Ring Buffer & Throttled MQTT Backfill)

- **Trạng thái:** Chấp thuận (Accepted)
- **Ngày quyết định:** 2026-09-21
- **Người đề xuất:** System Architect
- **Phạm vi kỹ thuật:** Thẻ nhớ MicroSD FATFS, [firmware/main/sd_logger.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/sd_logger.c), [firmware/main/uplink.c](file:///home/chien/ai-workspace/qwen-test/firmware/main/uplink.c)
- **Tài liệu tham chiếu gốc:** [fsd-smart-clock-gateway.md](file:///home/chien/ai-workspace/qwen-test/fsd-smart-clock-gateway.md) (§4.11, §11.5)
- **Truy vết yêu cầu kỹ thuật:** `Traces to: FR-SD-01 / FR-SD-02 / FR-MQTT-01 / M15 / NFR-REL-04`

---

## 1. Bối Cảnh và Vấn Đề (Context & Problem Statement)

Trong môi trường triển khai thực tế của Smart Home:
1. **Kết nối mạng không ổn định (Intermittent Connectivity):** Mạng Wi-Fi gia đình hoặc đường truyền Internet có thể bị ngắt quãng vài phút đến nhiều ngày (mất mạng viễn thông, tắt router Wi-Fi vào ban đêm).
2. **Nguy cơ mất mát dữ liệu đo lường:** Các bản tin nhiệt độ, độ ẩm và nhật ký sự kiện nếu chỉ giữ trong bộ nhớ RAM sẽ bị tràn hàng đợi hoặc mất sạch khi mất điện.
3. **Cơn bão dữ liệu khi mạng phục hồi (Reconnection Thundering Herd / Buffer Overflow):** Khi Internet có lại sau nhiều ngày mất mạng, nếu Gateway đẩy ồ ạt hàng chục nghìn bản tin tích lũy lên Broker cùng một lúc:
   - Sẽ làm nghẽn băng thông Wi-Fi.
   - Tràn bộ đệm Outbox của MQTT client trên ESP32 dẫn đến Crash Heap.
   - Làm chậm hoặc nghẽn hoàn toàn các lệnh điều khiển thời gian thực (Real-time Downlink Commands) do người dùng gửi từ ứng dụng.

---

## 2. Quyết Định Kiến Trúc (Decision)

Chúng tôi quyết định áp dụng cơ chế **Lưu Trữ và Chuyển Tiếp Ngoại Tuyến (Store-and-Forward)** kết hợp với **Hàng Đợi Vòng Tròn Nhị Phân (Binary Circular FIFO)** và **Bộ Điều Tiết Tốc Độ Gửi Bù (Throttled Backfill Rate Limiter)**:

```
                            Bản Tin Telemetry / Sự Kiện
                                         │
                         ┌───────────────┴───────────────┐
                         ▼                               ▼
                 [Có Mạng Online]                [Mất Mạng Offline]
                         │                               │
                Publish MQTT QoS 1               Đẩy vào s_sd_log_queue
                         │                               │
                         │                               ▼
                         │               Ghi nhị phân vào sync_queue.bin (MicroSD)
                         │               (f_sync mỗi 10 bản tin hoặc 5s)
                         │                               │
                         │               ┌───────────────┴───────────────┐
                         │               │ Mạng Internet Phục Hồi        │
                         │               └───────────────┬───────────────┘
                         │                               ▼
                         │               Tiến Trình Gửi Bù (Throttled Backfill):
                         │               - Đọc từng Lô 20 records từ read_offset
                         │               - Nghỉ 100ms giữa các Batch
                         │               - Tốc độ khống chế: 20..50 records/s
                         │               - Nhận PUBACK -> Cập nhật read_offset
                         │                               │
                         └───────────────────────────────┘
```

1. **Định dạng nhị phân phẳng chuẩn hóa (Header 24B & Record 172B) và kiểm định CRC32:**
   - Cung cấp định dạng byte packing cố định đảm bảo đồng nhất 100% giữa ESP32 (Xtensa 32-bit) và Host test (x86_64 GCC):
     - **Header 24 Bytes (`sync_queue_header_t`):** Gồm `magic` (0x53594E43 "SYNC"), `write_offset` (4B), `read_offset` (4B), `total_records` (4B), `unacked_count` (4B), `crc32` (4B bảo vệ 20 bytes đầu).
     - **Record chuẩn cố định 172 Bytes (`sync_queue_record_t`, định dạng `<IBBH32s128sI`):**
       - `timestamp` (4B): Unix epoch timestamp.
       - `record_type` (1B): `0x01` (telemetry), `0x02` (event_log), `0x03` (command_ack).
       - `flags` (1B): bit 0: QoS1, bit 1: compressed.
       - `payload_len` (2B): chiều dài chuỗi trong payload (1..128).
       - `device_id[32]` (32B): Device ID hoặc target device ID (null-terminated).
       - `payload[128]` (128B): Chuỗi JSON chứa telemetry, event log hoặc command ACK (chứa đầy đủ UUID 36 ký tự).
       - `crc32` (4B): Checksum bảo vệ toàn vẹn 168 bytes đầu.
     - Kiểm tra tĩnh tại compile time: `_Static_assert(sizeof(sync_queue_record_t) == 172, "sync_queue_record_t must be exactly 172 bytes");`
2. **Cơ chế con trỏ vòng tròn (Circular FIFO) và Trần dung lượng an toàn:**
   - Header lưu hai con trỏ: `write_offset` (vị trí byte ghi mới) và `read_offset` (vị trí đã được Backend xác nhận ACK).
   - Giới hạn tối đa `SYNC_QUEUE_MAX_RECORDS = 50000` bản ghi (~8.6 MB). Khi hàng đợi chạm trần do offline dài ngày, con trỏ `read_offset` tự động tịnh tiến để ghi đè bản ghi cũ nhất, bảo vệ thẻ nhớ SD không bao giờ bị cạn kiệt dung lượng và tránh phân mảnh hệ thống tệp.
3. **Bảo vệ chống mất điện đột ngột (Power-Loss Safe Flush):**
   - File nhị phân không mở và đóng liên tục trong vòng lặp nhỏ.
   - Hệ điều hành FATFS chỉ thực hiện ép đồng bộ `f_sync()` từ RAM xuống flash vật lý của thẻ SD **sau mỗi 10 bản ghi hoặc tối đa 5 giây một lần**. Nhờ vậy cấu trúc bảng FAT không bao giờ bị phân mảnh hay lỗi filesystem khi mất điện.
4. **Bộ điều tiết tốc độ gửi bù (Backfill Rate Limiter) & MQTT QoS 1:**
   - Khi mạng phục hồi, `sd_log_task` đọc từng lô (Batch) **20 bản ghi** từ `read_offset`.
   - Đóng gói batch thành JSON mảng và publish lên topic `gateway/{gateway_id}/telemetry/batch` với **MQTT QoS 1**, giãn cách tối thiểu **100ms giữa các batch** (duy trì tốc độ gửi bù ổn định từ **20 đến 50 bản ghi/giây**).
   - Tốc độ này khống chế để không làm nghẽn băng thông Wi-Fi, không gây trễ các gói tin điều khiển tức thời của người dùng, và giữ bộ đệm outbox RAM của MQTT client luôn an toàn.
   - Chỉ khi nhận được `PUBACK` cho toàn bộ batch từ MQTT Broker, con trỏ `read_offset` mới được tịnh tiến và gọi `f_sync()` lưu xuống thẻ SD. Nếu xảy ra lỗi mạng giữa chừng, lần sau Gateway gửi lại từ vị trí `read_offset` cũ mà không bị mất dữ liệu.

---

## 3. Hệ Quả và Đánh Giá Đánh Đổi (Consequences)

### Tích Cực (Positive):
- **Không mất mát dữ liệu (Zero Data Loss):** Đảm bảo toàn vẹn dữ liệu đo lường môi trường, nhật ký thiết bị và phản hồi lệnh command ACK (kèm UUID) dù Gateway mất mạng nhiều ngày.
- **Bảo vệ ổn định mạng:** Tốc độ gửi bù được điều tiết 20-50 records/s mượt mà, không làm nghẽn đường truyền Wi-Fi và không gây gián đoạn các thao tác điều khiển thiết bị của người dùng.
- **Tuổi thọ thẻ nhớ cao:** Gom nhóm các thao tác ghi và `f_sync()` theo chu kỳ 10 bản ghi / 5s giúp giảm đáng kể số chu kỳ ghi/xóa (P/E Cycles) của chip flash trên thẻ nhớ MicroSD.

### Tiêu Cực & Đánh Đổi (Negative & Neutral):
- **Thời gian hoàn tất gửi bù kéo dài:** Với 10.000 bản ghi tồn đọng (khoảng 14 giờ offline với tần suất 5s/bản tin), hệ thống sẽ mất từ **3 đến 8 phút** để hoàn tất việc gửi bù toàn bộ dữ liệu lên Backend.
- **Trùng lặp tiềm tàng (At-Least-Once Delivery):** Nếu mất mạng đúng lúc Broker vừa nhận bản tin nhưng chưa kịp phản hồi PUBACK về Gateway, bản tin đó sẽ được gửi lại ở lần kết nối sau. Backend được yêu cầu phải có cơ chế Deduplication dựa trên cặp khóa `(device_id, timestamp)`.

---

## 4. Tuân Thủ và Xác Nhận (Compliance & Verification)

- **Verification Contract:** Khớp chuẩn xác với `FR-SD-01` (Ghi Telemetry CSV vào thẻ nhớ) và `FR-SD-02` (Store-and-Forward Offline Queue 172B & Điều tiết Backfill) tại FSD §4.11.5.
- **Kiểm thử tự động:** Được kiểm chứng toàn diện qua bộ test case nhị phân [tests/host/test_sync_queue.py](file:///home/chien/ai-workspace/qwen-test/tests/host/test_sync_queue.py) (`_Static_assert` 172B, Header 24B, CRC32, và Backfill Batching).

---

## 5. Thẩm Tra Ponytail và Đánh Đổi Tối Giản (Ponytail Audit & Trade-off)

- **Lý do chuẩn hóa bản ghi phẳng 172-Byte (POD Struct):** Bản ghi 32-byte cũ không thể chứa vừa chuỗi UUID 36 ký tự của command ACK hay các thông điệp event/telemetry đa thuộc tính. Bản ghi phẳng 172-byte với mảng cố định `device_id[32]` và `payload[128]` giải quyết trọn vẹn yêu cầu này mà không cần cấp phát heap động hay con trỏ lồng nhau.
- **Bộ đệm gửi bù trên Stack (Zero Dynamic Heap):** Tiến trình gửi bù (Backfill) đọc từng lô 20 records trực tiếp vào mảng stack tĩnh `sync_queue_record_t batch[20]` (20 $\times$ 172 = 3.440 Bytes), tiêu tốn 0 byte heap động và triệt tiêu nguy cơ phân mảnh RAM.
- `/* ponytail: Chuẩn hóa 100% vào bản ghi phẳng 172-byte (Header 24B, device_id[32], payload[128] chứa telemetry/events/command ACK UUID, crc32); loại bỏ cấp phát con trỏ động lồng nhau. Quá trình gửi bù đọc theo lô 20 records vào mảng stack 3440 bytes, điều tiết tốc độ 20..50 records/s qua QoS 1. */`
