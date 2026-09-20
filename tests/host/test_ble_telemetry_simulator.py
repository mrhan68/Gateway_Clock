"""
Host Unit Tests for BLE Telemetry Simulator & Test Automation Battery.
Verifies packet synthesis, decoding, round-trip serialization, and mock simulation engine.
"""

import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), "../..")))

from testing.ble_telemetry_simulator import (
    build_sig_0x181a_adv,
    build_bthome_v2_adv,
    parse_ble_adv_payload,
    GatewaySerialClient,
    TestReport,
    run_scenario_1_fan_hysteresis,
    run_scenario_2_heater_interlock_mutex,
    run_scenario_3_safety_cutoff_sensor_fault,
    run_scenario_4_event_bus_stress,
)


def test_sig_0x181a_synthesis_and_parsing():
    """Verify Bluetooth SIG 0x181A builder and parser round-trip."""
    payload = build_sig_0x181a_adv(26.50, 72.30, "sensor_temp_01")
    parsed = parse_ble_adv_payload(payload)

    assert parsed["valid"] is True
    assert parsed["name"] == "sensor_temp_01"
    assert parsed["uuid16"] == "0x181A"
    assert parsed["temperature"] == 26.50
    assert parsed["humidity"] == 72.30
    assert parsed["format"] == "SIG_0x181A"


def test_sig_0x181a_negative_temperature():
    """Verify negative temperature handling in int16 x0.01 format."""
    payload = build_sig_0x181a_adv(-12.45, 45.00, "sensor_freezer")
    parsed = parse_ble_adv_payload(payload)

    assert parsed["valid"] is True
    assert parsed["temperature"] == -12.45
    assert parsed["humidity"] == 45.00


def test_bthome_v2_synthesis_and_parsing():
    """Verify BTHome V2 (UUID 0xFCD2) builder and parser round-trip."""
    payload = build_bthome_v2_adv(23.40, 58.10, 88, "sensor_living")
    parsed = parse_ble_adv_payload(payload)

    assert parsed["valid"] is True
    assert parsed["name"] == "sensor_living"
    assert parsed["uuid16"] == "0xFCD2"
    assert parsed["temperature"] == 23.40
    assert parsed["humidity"] == 58.10
    assert parsed["battery"] == 88
    assert parsed["format"] == "BTHome_V2"


def test_mock_gateway_scenarios_all_pass():
    """Verify all 4 test scenarios pass 100% in mock simulation mode."""
    client = GatewaySerialClient(mock_mode=True)
    assert client.connect() is True
    report = TestReport()

    s1 = run_scenario_1_fan_hysteresis(client, report)
    s2 = run_scenario_2_heater_interlock_mutex(client, report)
    s3 = run_scenario_3_safety_cutoff_sensor_fault(client, report)
    s4 = run_scenario_4_event_bus_stress(client, report)

    assert s1 is True
    assert s2 is True
    assert s3 is True
    assert s4 is True
    assert report.is_all_passed() is True
    assert len(report.results) == 12
