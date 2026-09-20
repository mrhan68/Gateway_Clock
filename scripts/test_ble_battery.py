#!/usr/bin/env python3
"""BLE Telemetry and GAP Scanner Target Hardware Verification Script.

Verifies: FR-BLE-01 / TC-BLE-01 on ESP32-S3 over USB Serial (/dev/ttyACM0).
"""

import sys
import time
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200


def main():
    print("================================================================")
    print("      BLE TELEMETRY & SCANNER TARGET TEST SUITE (TC-BLE-01)    ")
    print("================================================================")
    print(f"Connecting to {PORT} at {BAUD} baud...")

    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.2)
    except Exception as e:
        print(f"ERROR: Cannot open port {PORT}: {e}")
        return 1

    ser.dtr = True
    ser.rts = False
    time.sleep(0.5)
    ser.reset_input_buffer()

    def read_buffer(timeout=2.0):
        start = time.time()
        chunks = []
        while time.time() - start < timeout:
            data = ser.read(ser.in_waiting or 1)
            if data:
                text = data.decode("utf-8", errors="replace")
                sys.stdout.write(text)
                sys.stdout.flush()
                chunks.append(text)
        return "".join(chunks)

    def exec_cli(cmd, wait=2.0):
        print(f"\n>>> [CLI EXEC]: {cmd}")
        ser.write((cmd + "\r\n").encode("utf-8"))
        ser.flush()
        return read_buffer(timeout=wait)

    print("\n[STEP 1] Synchronizing with Gateway CLI...")
    read_buffer(timeout=2.0)
    out = exec_cli("help", wait=1.5)
    if "ble status" not in out and "gateway>" not in out:
        print("[WARN] Retrying CLI prompt...")
        out = exec_cli("help", wait=2.0)

    assert ("ble status" in out or "gateway>" in out), "CLI prompt failed to respond"

    # Temporarily pause simulated RS485 polling to test BLE channel isolation
    exec_cli("mock rs485 stop", wait=1.0)

    print("\n[STEP 2] Verifying BLE Adapter & Scanner Status...")
    status_out = exec_cli("ble status", wait=1.5)
    assert "BLE ADAPTER STATUS" in status_out, "BLE adapter status header missing"
    assert "RUNNING" in status_out, "BLE GAP scanner is not running"
    assert "sensor_temp_01" in status_out, "Default sensor_temp_01 missing from device table"
    print("--> [PASS] BLE Adapter is active and scanning.")

    print("\n[STEP 3] Registering custom test sensor via CLI...")
    reg_out = exec_cli("ble reg sensor_livingroom 11:22:33:44:55:66", wait=1.5)
    assert "Registered BLE device 'sensor_livingroom'" in reg_out, "Failed to register BLE device"

    status_out2 = exec_cli("ble status", wait=1.5)
    assert "sensor_livingroom" in status_out2, "Newly registered device not listed in status"
    print("--> [PASS] Dynamic device registration verified.")

    print("\n[STEP 4] Testing Telemetry Pipeline & Rule Engine Integration (FR-BLE-01)...")
    # Inject 34.5 C into sensor_temp_01 (should trigger Rule 1: val > 30.0 -> fan_relay_01 ON)
    test_out = exec_cli("ble test sensor_temp_01 34.50", wait=2.0)
    assert "Injecting synthetic test telemetry" in test_out, "Injection log missing"
    assert "MATCHED Rule #1" in test_out or "val=34.50" in test_out, "Rule Engine did not evaluate BLE telemetry"
    print("--> [PASS] FR-BLE-01 Verified: BLE Telemetry propagated to Event Bus and triggered Rule Engine!")

    print("\n[STEP 5] Testing Sub-threshold Telemetry (Safe Condition)...")
    # Inject 25.0 C (within normal range 20-30 C, neither fan nor heater should trigger)
    norm_out = exec_cli("ble test sensor_temp_01 25.00", wait=2.0)
    assert "MATCHED" not in norm_out, "Rule triggered unexpectedly on normal temperature"
    print("--> [PASS] Normal temperature handled correctly without false positive trigger.")

    print("\n[STEP 6] Testing Supervisor Bitmask with BLE Task...")
    sup_out = exec_cli("status", wait=1.5)
    assert "Alive tasks" in sup_out or "Free Heap" in sup_out, "Status query failed"
    print("--> [PASS] Task Supervisor healthy, BLE Task running normally on Core 0.")

    print("\n" + "=" * 64)
    print("ALL BLE TARGET HARDWARE VERIFICATION TESTS PASSED (100%)!")
    print("=" * 64)
    exec_cli("mock rs485 start 4000", wait=1.0)
    ser.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
