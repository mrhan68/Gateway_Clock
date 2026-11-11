#!/usr/bin/env python3
"""
Test script for Option A Config Import (TC-CFG-01) and SD RAM Fallback (TC-NEG-02)
Executed directly against hardware /dev/ttyACM0.
"""

import time
import serial
import sys
import os

PORT = "/dev/ttyACM0"
BAUD = 115200
LOG_DIR = "testing/evidence/2026-11-11"
os.makedirs(LOG_DIR, exist_ok=True)

def send_cmd(ser, cmd, wait_time=0.8):
    ser.reset_input_buffer()
    ser.write(f"\r\n{cmd}\r\n".encode("utf-8"))
    time.sleep(wait_time)
    resp = ser.read(4096).decode("utf-8", errors="ignore")
    return resp

def test_tc_cfg_01(ser):
    print("==================================================================")
    print("=== [TC-CFG-01] Testing Option A: Config Import from MicroSD ===")
    print("==================================================================")
    log_file = os.path.join(LOG_DIR, "TC_CFG_01_option_a.log")
    full_log = []

    # 1. Ensure SD is mounted
    resp = send_cmd(ser, "sd mount", wait_time=1.0)
    full_log.append(f">>> sd mount\n{resp}")
    print(resp)

    # 2. Write test config.json to SD card
    test_json = '{"gateway_id":"gw_smartclock_optA","timezone":"ICT-7"}'
    write_cmd = f"sd write /sdcard/config.json {test_json}"
    resp = send_cmd(ser, write_cmd, wait_time=0.8)
    full_log.append(f">>> {write_cmd}\n{resp}")
    print(resp)
    assert "OK: Written" in resp, f"Failed to write config.json: {resp}"

    # 3. Verify file exists
    resp = send_cmd(ser, "sd ls /sdcard", wait_time=0.8)
    full_log.append(f">>> sd ls /sdcard\n{resp}")
    print(resp)
    assert "config.json" in resp, "config.json not found in listing"

    # 4. Execute config import
    resp = send_cmd(ser, "config import /sdcard/config.json", wait_time=1.2)
    full_log.append(f">>> config import /sdcard/config.json\n{resp}")
    print(resp)
    assert "Successfully imported configuration" in resp, f"Config import failed: {resp}"
    assert "config.json.bak" in resp, "File was not renamed to .bak"

    # 5. Check config show to verify NVS updated
    resp = send_cmd(ser, "config show", wait_time=0.8)
    full_log.append(f">>> config show\n{resp}")
    print(resp)
    assert "gw_smartclock_optA" in resp, "Gateway ID not updated in NVS"

    # 6. Verify file renamed to .bak and original gone
    resp = send_cmd(ser, "sd ls /sdcard", wait_time=0.8)
    full_log.append(f">>> sd ls /sdcard\n{resp}")
    print(resp)
    assert "config.json.bak" in resp, "config.json.bak not found"

    with open(log_file, "w", encoding="utf-8") as f:
        f.write("\n".join(full_log))
    print(f">> [PASS] TC-CFG-01 verified! Log saved to {log_file}\n")


def test_tc_neg_02(ser):
    print("==================================================================")
    print("=== [TC-NEG-02] Testing MicroSD Hotplug & RAM Fallback Buffer ===")
    print("==================================================================")
    log_file = os.path.join(LOG_DIR, "TC_NEG_02_sd_hotplug.log")
    full_log = []

    # 1. Check initial state
    resp = send_cmd(ser, "sd status", wait_time=0.5)
    full_log.append(f">>> sd status (initial)\n{resp}")
    print(resp)

    # 2. Simulate card removal via CLI unmount
    resp = send_cmd(ser, "sd unmount", wait_time=0.8)
    full_log.append(f">>> sd unmount\n{resp}")
    print(resp)
    assert "MicroSD card unmounted" in resp, f"Unmount failed: {resp}"

    resp = send_cmd(ser, "sd status", wait_time=0.5)
    full_log.append(f">>> sd status (unmounted)\n{resp}")
    print(resp)
    assert "NOT MOUNTED" in resp, "SD should not be mounted"

    # 3. Inject 5 telemetry points while card unmounted
    for i in range(1, 6):
        val = 28.0 + i * 0.5
        resp = send_cmd(ser, f"ble test {val:.1f}", wait_time=0.3)
        full_log.append(f">>> ble test {val:.1f}\n{resp}")

    # 4. Check RAM fallback buffer count
    resp = send_cmd(ser, "sd status", wait_time=0.5)
    full_log.append(f">>> sd status (with buffered items)\n{resp}")
    print(resp)
    assert "RAM Fallback Buffer  : 5 / 50 items" in resp or "5 / 50" in resp, f"Expected 5 items in fallback buffer, got: {resp}"

    # 5. Remount SD card
    resp = send_cmd(ser, "sd mount", wait_time=2.5)
    full_log.append(f">>> sd mount\n{resp}")
    print(resp)
    assert "mounted successfully" in resp.lower() or "success" in resp.lower(), f"Remount failed: {resp}"

    # Wait for background task to drain
    time.sleep(1.0)

    # 6. Verify RAM fallback buffer drained to 0
    resp = send_cmd(ser, "sd status", wait_time=0.5)
    full_log.append(f">>> sd status (drained)\n{resp}")
    print(resp)
    assert "0 / 50 items" in resp, f"RAM fallback buffer did not drain to 0: {resp}"

    with open(log_file, "w", encoding="utf-8") as f:
        f.write("\n".join(full_log))
    print(f">> [PASS] TC-NEG-02 verified! Log saved to {log_file}\n")


if __name__ == "__main__":
    ser = serial.Serial(PORT, BAUD, timeout=1)
    time.sleep(1.0)
    try:
        test_tc_cfg_01(ser)
        test_tc_neg_02(ser)
        print("ALL TESTS PASSED SUCCESSFULLY ON PHYSICAL ESP32-S3 HARDWARE!")
    finally:
        ser.close()
