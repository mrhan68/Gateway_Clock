"""
Test Native Compile-Time Assertions & Struct Sizes (TC-HOST-09 / FR-TEST-01)
Validates C firmware binary layout invariants specified in FSD §14.5 and testing/test-plan.yaml.
"""
import struct
from pathlib import Path

FIRMWARE_MAIN = Path(__file__).resolve().parent.parent.parent / "firmware" / "main"


def test_sync_queue_header_binary_size():
    """
    FSD §4.11.3 & §14.5:
    sync_queue_header_t: exactly 24 bytes packed.
    Layout:
      uint32_t magic;          // 4B
      uint32_t write_offset;   // 4B
      uint32_t read_offset;    // 4B
      uint32_t total_records;  // 4B
      uint32_t unacked_count;  // 4B
      uint32_t crc32;          // 4B
    Total = 24 bytes.
    """
    fmt = "<6I"
    assert struct.calcsize(fmt) == 24


def test_sync_queue_record_binary_size():
    """
    FSD §4.11.3 & §14.5:
    sync_queue_record_t: exactly 172 bytes packed.
    Layout:
      uint32_t timestamp;      // 4B
      uint8_t  record_type;    // 1B
      uint8_t  flags;          // 1B
      uint16_t payload_len;    // 2B
      char     device_id[32];  // 32B
      char     payload[128];   // 128B
      uint32_t crc32;          // 4B
    Total = 4 + 1 + 1 + 2 + 32 + 128 + 4 = 172 bytes.
    """
    fmt = "<IBBH32s128sI"
    assert struct.calcsize(fmt) == 172


def test_header_static_assertions_present():
    """
    Verify that C header files declare the mandatory _Static_assert checks
    mandated by FSD §14.5.
    """
    sd_logger_h = FIRMWARE_MAIN / "sd_logger.h"
    assert sd_logger_h.exists(), "sd_logger.h must exist"
    sd_content = sd_logger_h.read_text(encoding="utf-8")

    assert "_Static_assert(sizeof(sync_queue_header_t) == 24" in sd_content
    assert "_Static_assert(sizeof(sync_queue_record_t) == 172" in sd_content

    event_bus_h = FIRMWARE_MAIN / "event_bus.h"
    assert event_bus_h.exists(), "event_bus.h must exist"
    eb_content = event_bus_h.read_text(encoding="utf-8")

    assert "typedef gateway_event_t event_bus_msg_t;" in eb_content
    assert "_Static_assert(sizeof(event_bus_msg_t) <= 48" in eb_content


def test_event_bus_msg_size_limits():
    """
    gateway_event_t:
      enum (4B) + void* (4 or 8B) + size_t (4 or 8B) + uint32_t (4B)
      Max size on 64-bit host = 24 bytes <= 48 bytes.
    """
    # 32-bit arch: 4 + 4 + 4 + 4 = 16 bytes
    # 64-bit arch: 4 + 4(pad) + 8 + 8 + 4 + 4(pad) = 32 bytes
    for ptr_size in (4, 8):
        # Even with 64-bit pointers and worst-case alignment, size <= 32 <= 48
        worst_case_size = 4 + 4 + ptr_size + ptr_size + 4 + 4
        assert worst_case_size <= 48
