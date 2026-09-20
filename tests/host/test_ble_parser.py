"""Host tests for BLE Advertisement and Environmental Sensing Packet Parsing.

Verifies: FR-BLE-01 / TC-HOST-05
"""

import struct


def parse_ble_adv_payload(ad_data: bytes) -> dict:
    """Parse raw BLE advertisement bytes into structured metadata and telemetry.

    Supports:
    - Complete/Shortened Local Name (AD type 0x08, 0x09)
    - Service Data 16-bit UUID (AD type 0x16):
        - Environmental Sensing (0x181A)
        - BTHome V2 (0xFCD2)
    - Manufacturer Specific Data (AD type 0xFF)
    """
    result = {
        "name": None,
        "temperature": None,
        "humidity": None,
        "device_id": None,
        "valid": False,
    }

    idx = 0
    length = len(ad_data)

    while idx < length:
        ad_len = ad_data[idx]
        if ad_len == 0:
            break
        if idx + 1 + ad_len > length:
            # Malformed AD structure
            break

        ad_type = ad_data[idx + 1]
        payload = ad_data[idx + 2 : idx + 1 + ad_len]

        if ad_type in (0x08, 0x09):  # Local Name
            try:
                result["name"] = payload.decode("utf-8", errors="ignore").strip("\x00")
            except Exception:
                pass

        elif ad_type == 0x16:  # Service Data - 16-bit UUID
            if len(payload) >= 2:
                uuid16 = struct.unpack("<H", payload[0:2])[0]
                svc_data = payload[2:]

                if uuid16 == 0x181A and len(svc_data) >= 2:
                    # Environmental Sensing Service (0x181A) - 16-bit signed integer (0.01 deg C)
                    raw_temp = struct.unpack("<h", svc_data[0:2])[0]
                    result["temperature"] = round(raw_temp * 0.01, 2)
                    result["valid"] = True

                    # If optional 2-byte humidity is also present (UUID 0x2A6F / 0.01%)
                    if len(svc_data) >= 4:
                        raw_hum = struct.unpack("<H", svc_data[2:4])[0]
                        result["humidity"] = round(raw_hum * 0.01, 2)

                elif uuid16 == 0xFCD2 and len(svc_data) >= 2:
                    # BTHome V2 format (UUID 0xFCD2)
                    # svc_data[0] is BTHome header/flags (e.g. 0x40)
                    bthome_idx = 1
                    while bthome_idx < len(svc_data):
                        obj_id = svc_data[bthome_idx]
                        bthome_idx += 1
                        if obj_id == 0x02 and bthome_idx + 2 <= len(svc_data):  # Temperature (0.01 C)
                            raw_t = struct.unpack("<h", svc_data[bthome_idx : bthome_idx + 2])[0]
                            result["temperature"] = round(raw_t * 0.01, 2)
                            result["valid"] = True
                            bthome_idx += 2
                        elif obj_id == 0x03 and bthome_idx + 2 <= len(svc_data):  # Humidity (0.01 %)
                            raw_h = struct.unpack("<H", svc_data[bthome_idx : bthome_idx + 2])[0]
                            result["humidity"] = round(raw_h * 0.01, 2)
                            result["valid"] = True
                            bthome_idx += 2
                        elif obj_id == 0x01 and bthome_idx + 1 <= len(svc_data):  # Battery (1 %)
                            bthome_idx += 1
                        else:
                            break

        elif ad_type == 0xFF:  # Manufacturer Specific Data
            # Check for custom text format, e.g., "TEMP:32.5"
            try:
                text = payload.decode("ascii", errors="ignore")
                if "TEMP:" in text:
                    parts = text.split("TEMP:")
                    val = float(parts[1].split()[0].split(";")[0])
                    result["temperature"] = val
                    result["valid"] = True
            except Exception:
                pass

        idx += 1 + ad_len

    # Device ID derivation: prefer name if present, else fallback
    if result["name"]:
        result["device_id"] = result["name"]

    return result


def test_parse_environmental_sensing_0x181a():
    """Verify parsing of standard Bluetooth SIG 0x181A Environmental Sensing."""
    # Complete Name "sensor_temp_01" (15 bytes)
    # Service Data 0x181A with temp 32.56 C (0x0CB8 = 3256)
    name_payload = b"sensor_temp_01"
    ad_name = bytes([len(name_payload) + 1, 0x09]) + name_payload

    # UUID 0x181A + 0x0CB8 (32.56 C) + 0x1770 (60.00 % hum)
    svc_data = struct.pack("<HhH", 0x181A, 3256, 6000)
    ad_svc = bytes([len(svc_data) + 1, 0x16]) + svc_data

    raw_adv = ad_name + ad_svc
    parsed = parse_ble_adv_payload(raw_adv)

    assert parsed["name"] == "sensor_temp_01"
    assert parsed["device_id"] == "sensor_temp_01"
    assert parsed["temperature"] == 32.56
    assert parsed["humidity"] == 60.0
    assert parsed["valid"] is True


def test_parse_negative_temperature():
    """Verify parsing negative temperature (sub-zero degrees)."""
    # -5.50 C = -550 -> 0xFDDD
    svc_data = struct.pack("<Hh", 0x181A, -550)
    ad_svc = bytes([len(svc_data) + 1, 0x16]) + svc_data

    parsed = parse_ble_adv_payload(ad_svc)
    assert parsed["temperature"] == -5.50
    assert parsed["valid"] is True


def test_parse_bthome_v2():
    """Verify parsing of open standard BTHome V2 packet (UUID 0xFCD2)."""
    # BTHome V2: UUID 0xFCD2, Header 0x40, Obj 0x02 (temp 24.15 C = 2415), Obj 0x03 (hum 55.40 % = 5540)
    bthome_payload = struct.pack("<HBBhBH", 0xFCD2, 0x40, 0x02, 2415, 0x03, 5540)
    ad_svc = bytes([len(bthome_payload) + 1, 0x16]) + bthome_payload

    name_payload = b"BTHome_Sensor"
    ad_name = bytes([len(name_payload) + 1, 0x09]) + name_payload

    raw_adv = ad_name + ad_svc
    parsed = parse_ble_adv_payload(raw_adv)

    assert parsed["name"] == "BTHome_Sensor"
    assert parsed["temperature"] == 24.15
    assert parsed["humidity"] == 55.40
    assert parsed["valid"] is True


def test_parse_empty_or_malformed_adv():
    """Verify robust handling of corrupt or empty advertisement buffers."""
    assert parse_ble_adv_payload(b"")["valid"] is False
    assert parse_ble_adv_payload(b"\x00\x00\x00")["valid"] is False
    # Length field claims 20 bytes but buffer is 5 bytes
    assert parse_ble_adv_payload(b"\x14\x09abc")["valid"] is False
