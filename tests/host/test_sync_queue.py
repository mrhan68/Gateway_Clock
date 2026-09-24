"""Host-tier tests for Store-and-Forward sync queue header binary packing."""

import struct
import zlib

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

