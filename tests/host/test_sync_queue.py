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


RECORD_FORMAT = "<IBBH16sfI"  # 32 bytes: timestamp(4), type(1), flags(1), reserved(2), device_id(16), value(4), crc32(4)


def pack_sync_record(timestamp: int, record_type: int, flags: int, device_id: str, value: float) -> bytes:
    dev_bytes = device_id.encode("utf-8")[:16].ljust(16, b"\x00")
    payload = struct.pack("<IBBH16sf", timestamp, record_type, flags, 0, dev_bytes, value)
    checksum = zlib.crc32(payload) & 0xFFFFFFFF
    return payload + struct.pack("<I", checksum)


def unpack_sync_record(data: bytes):
    assert len(data) == 32, "Record must be exactly 32 bytes"
    ts, rtype, flags, _res, dev_bytes, val, checksum = struct.unpack(RECORD_FORMAT, data)
    expected_crc = zlib.crc32(data[:28]) & 0xFFFFFFFF
    assert checksum == expected_crc, "Record checksum mismatch"
    return {
        "timestamp": ts,
        "record_type": rtype,
        "flags": flags,
        "device_id": dev_bytes.split(b"\x00", 1)[0].decode("utf-8"),
        "value": round(val, 2),
    }


def test_record_pack_and_unpack():
    rec_bytes = pack_sync_record(
        timestamp=1726800000,
        record_type=1,
        flags=0,
        device_id="ble_temp_01",
        value=28.5,
    )
    assert len(rec_bytes) == 32
    parsed = unpack_sync_record(rec_bytes)
    assert parsed["timestamp"] == 1726800000
    assert parsed["record_type"] == 1
    assert parsed["device_id"] == "ble_temp_01"
    assert parsed["value"] == 28.5

