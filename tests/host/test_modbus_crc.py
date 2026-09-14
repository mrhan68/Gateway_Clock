"""Host-tier tests for Modbus RTU CRC-16 calculation.

Tests pure logic algorithm with known Modbus test vectors without requiring hardware.
"""

def calculate_modbus_crc16(data: bytes) -> int:
    """Standard Modbus RTU CRC-16 (polynomial 0xA001, initial value 0xFFFF)."""
    crc = 0xFFFF
    for pos in data:
        crc ^= pos
        for _ in range(8):
            if (crc & 0x0001) != 0:
                crc >>= 1
                crc ^= 0xA001
            else:
                crc >>= 1
    return crc


def test_modbus_crc_read_holding_registers():
    """Test vector: Read Holding Registers slave 0x01, reg 0x0001, count 0x0001."""
    # Request frame without CRC: [0x01, 0x03, 0x00, 0x01, 0x00, 0x01]
    # Expected CRC: 0xD5CA -> Low byte 0xD5, High byte 0xCA
    frame = bytes([0x01, 0x03, 0x00, 0x01, 0x00, 0x01])
    crc = calculate_modbus_crc16(frame)
    assert crc == 0xCAD5  # Low-byte first in RTU transmission is 0xD5 then 0xCA


def test_modbus_crc_write_single_coil():
    """Test vector: Write Single Coil slave 0x02, coil 0x0000, value 0xFF00 (ON)."""
    frame = bytes([0x02, 0x05, 0x00, 0x00, 0xFF, 0x00])
    crc = calculate_modbus_crc16(frame)
    assert crc == 0x098C  # Transmitted as 0x8C, 0x09
