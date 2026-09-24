"""Host-tier tests for Store-and-Forward sync queue header binary packing."""

import struct
import zlib
import pytest

SYNC_QUEUE_MAGIC = 0x53594E43  # "SYNC"
HEADER_FORMAT = "<IIIIII"      # 6 x uint32: magic, write_offset, read_offset, total, unacked, crc32


def pack_sync_header(write_offset: int, read_offset: int, total: int, unacked: int) -> bytes:
    payload = struct.pack("<IIIII", SYNC_QUEUE_MAGIC, write_offset, read_offset, total, unacked)
    checksum = zlib.crc32(payload) & 0xFFFFFFFF
    return payload + struct.pack("<I", checksum)


def unpack_sync_header(data: bytes):
    assert len(data) == 24, "Header must be exactly 24 bytes"
    magic, write_off, read_off, total, unacked, checksum = struct.unpack(HEADER_FORMAT, data)
    assert magic == SYNC_QUEUE_MAGIC, "Invalid magic bytes"
    expected_crc = zlib.crc32(data[:20]) & 0xFFFFFFFF
    assert checksum == expected_crc, "Header checksum mismatch"
    return {
        "write_offset": write_off,
        "read_offset": read_off,
        "total_records": total,
        "unacked_count": unacked,
    }


def test_header_pack_and_unpack():
    header_bytes = pack_sync_header(write_offset=1024, read_offset=512, total=50, unacked=25)
    parsed = unpack_sync_header(header_bytes)
    assert parsed["write_offset"] == 1024
    assert parsed["read_offset"] == 512
    assert parsed["total_records"] == 50
    assert parsed["unacked_count"] == 25


RECORD_FORMAT = "<IBBH32s128sI"  # 172 bytes: ts(4), type(1), flags(1), len(2), dev_id(32), payload(128), crc32(4)


def pack_sync_record(timestamp: int, record_type: int, flags: int, device_id: str, payload_str: str) -> bytes:
    dev_bytes = device_id.encode("utf-8")[:32].ljust(32, b"\x00")
    payload_bytes = payload_str.encode("utf-8")[:128].ljust(128, b"\x00")
    payload_len = min(len(payload_str.encode("utf-8")), 128)
    data = struct.pack("<IBBH32s128s", timestamp, record_type, flags, payload_len, dev_bytes, payload_bytes)
    assert len(data) == 168
    checksum = zlib.crc32(data) & 0xFFFFFFFF
    return data + struct.pack("<I", checksum)


def unpack_sync_record(data: bytes):
    assert len(data) == 172, f"Record must be exactly 172 bytes, got {len(data)}"
    ts, rtype, flags, plen, dev_bytes, payload_bytes, checksum = struct.unpack(RECORD_FORMAT, data)
    expected_crc = zlib.crc32(data[:168]) & 0xFFFFFFFF
    assert checksum == expected_crc, "Record checksum mismatch"
    return {
        "timestamp": ts,
        "record_type": rtype,
        "flags": flags,
        "payload_len": plen,
        "device_id": dev_bytes.split(b"\x00", 1)[0].decode("utf-8"),
        "payload": payload_bytes[:plen].decode("utf-8", errors="replace"),
    }


def test_record_pack_and_unpack():
    rec_bytes = pack_sync_record(
        timestamp=1726800000,
        record_type=1,
        flags=0,
        device_id="ble_temp_01",
        payload_str='{"temp":28.5,"humidity":65.0}',
    )
    assert len(rec_bytes) == 172
    parsed = unpack_sync_record(rec_bytes)
    assert parsed["timestamp"] == 1726800000
    assert parsed["record_type"] == 1
    assert parsed["device_id"] == "ble_temp_01"
    assert parsed["payload"] == '{"temp":28.5,"humidity":65.0}'
    assert parsed["payload_len"] == len('{"temp":28.5,"humidity":65.0}')


def test_offline_store_and_forward_backfill_lifecycle():
    """Verify offline accumulation, batch draining, and pointer advancement (FR-SD-02 / TC-DEV-02)."""
    # 1. Simulate offline accumulation of 25 records
    records_buffer = []
    base_ts = 1726800000
    for i in range(25):
        payload_json = f'{{"val":{20.0 + i}}}'
        rec = pack_sync_record(
            timestamp=base_ts + i * 10,
            record_type=1,
            flags=0,
            device_id=f"sensor_{i % 3}",
            payload_str=payload_json
        )
        records_buffer.append(rec)

    header = pack_sync_header(
        write_offset=len(records_buffer) * 172,
        read_offset=0,
        total=len(records_buffer),
        unacked=len(records_buffer)
    )
    h_data = unpack_sync_header(header)
    assert h_data["total_records"] == 25
    assert h_data["unacked_count"] == 25

    # 2. Simulate Batch Drain upon Wi-Fi reconnect (Batch size = 20)
    batch_size = 20
    first_batch = records_buffer[:batch_size]
    batch_json = []
    for raw_rec in first_batch:
        p = unpack_sync_record(raw_rec)
        batch_json.append({
            "device_id": p["device_id"],
            "timestamp": p["timestamp"],
            "payload": p["payload"]
        })
    assert len(batch_json) == 20
    assert batch_json[0]["device_id"] == "sensor_0"
    assert batch_json[19]["device_id"] == "sensor_1"

    # 3. Simulate ACK of first batch -> advance read_offset by 20 * 172
    new_read_off = batch_size * 172
    new_unacked = 25 - batch_size
    header_after_ack = pack_sync_header(
        write_offset=h_data["write_offset"],
        read_offset=new_read_off,
        total=25,
        unacked=new_unacked
    )
    h_ack_data = unpack_sync_header(header_after_ack)
    assert h_ack_data["read_offset"] == 3440
    assert h_ack_data["unacked_count"] == 5

    # 4. Drain remaining 5 records
    second_batch = records_buffer[batch_size:]
    assert len(second_batch) == 5
    final_header = pack_sync_header(
        write_offset=h_data["write_offset"],
        read_offset=h_data["write_offset"],
        total=25,
        unacked=0
    )
    h_final = unpack_sync_header(final_header)
    assert h_final["read_offset"] == h_final["write_offset"]
    assert h_final["unacked_count"] == 0


def test_corrupted_record_crc_detection():
    """Verify bit-flip in sync record payload triggers CRC mismatch (FR-SD-01)."""
    valid_rec = pack_sync_record(
        timestamp=1726800000,
        record_type=1,
        flags=0,
        device_id="sensor_01",
        payload_str='{"temp":25.0}'
    )
    # Corrupt byte 50 (in payload region)
    corrupted = bytearray(valid_rec)
    corrupted[50] ^= 0xFF
    with pytest.raises(AssertionError, match="Record checksum mismatch"):
        unpack_sync_record(bytes(corrupted))


