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
