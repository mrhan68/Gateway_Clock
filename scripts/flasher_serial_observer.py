#!/usr/bin/env python3
"""
flasher_serial_observer.py
Standardized bareboard observer for ESP32-S3 over /dev/ttyACM0.
Captures clean poweron/boot sequence, verifies 16 steps, KPI-BOOT-01,
Task Supervisor 0x7F masks, alive markers, and executes CLI 'status'.
"""

import sys
import time
import os
import re
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200
LOG_FILE = "testing/boot_log.txt"

def main():
    os.makedirs("testing", exist_ok=True)
    full_log = []

    print(f"[SERIAL] Opening {PORT} at {BAUD} baud (DTR=False, RTS=False)...")
    ser = serial.Serial(PORT, BAUD, timeout=0.2)
    ser.dtr = False
    ser.rts = False

    # Reset input buffer to start clean
    ser.reset_input_buffer()
    print("[SERIAL] Listening for bootloader and 16-step boot sequence...")

    start_time = time.time()
    alive_markers_count = 0

    while time.time() - start_time < 12.0:
        data = ser.read(ser.in_waiting or 1)
        if data:
            text = data.decode("utf-8", errors="replace")
            sys.stdout.write(text)
            sys.stdout.flush()
            full_log.append(text)
            if "[SYS] Alive marker" in text:
                alive_markers_count += 1
                if alive_markers_count >= 2:
                    print("\n[INFO] Observed alive markers. Ready to execute CLI 'status'...")
                    break
        else:
            time.sleep(0.05)

    # Let runtime settle for 1 second
    time.sleep(1.0)
    ser.reset_input_buffer()

    # Execute CLI command 'status'
    print("\n>>> [CLI EXEC]: status")
    ser.write(b"status\r\n")
    ser.flush()

    status_start = time.time()
    while time.time() - status_start < 3.0:
        data = ser.read(ser.in_waiting or 1)
        if data:
            text = data.decode("utf-8", errors="replace")
            sys.stdout.write(text)
            sys.stdout.flush()
            full_log.append(text)
        else:
            time.sleep(0.05)

    ser.close()
    combined_log = "".join(full_log)

    with open(LOG_FILE, "w", encoding="utf-8") as f:
        f.write(combined_log)
    print(f"\n[OK] Complete boot and status log saved to {LOG_FILE}")

    # ==================== VERIFICATION ====================
    print("\n" + "="*60)
    print("        ESP32-S3 BOOT & RUNTIME VERIFICATION REPORT        ")
    print("="*60)

    # 1. 16 Boot Steps Check
    step_results = {}
    for s in range(1, 17):
        m = re.search(rf"\[BOOT {s}/16\] (.*)", combined_log)
        if m:
            step_results[s] = m.group(0).strip()
            print(f"  ✓ {m.group(0).strip()}")
        else:
            step_results[s] = None
            print(f"  ✗ [BOOT {s}/16] NOT FOUND")

    # 2. KPI-BOOT-01 Verification
    # Measure time from [BOOT 1/16] to [BOOT 6/16] / display render
    m1 = re.search(r"I \((\d+)\) MAIN: \[BOOT 1/16\]", combined_log)
    m6 = re.search(r"I \((\d+)\) MAIN: \[BOOT 6/16\]", combined_log)
    m_disp = re.search(r"I \((\d+)\) DISPLAY: Display Subsystem initialized", combined_log)

    kpi_boot_passed = False
    if m1 and m6:
        t1 = int(m1.group(1))
        t6 = int(m6.group(1))
        td = int(m_disp.group(1)) if m_disp else t6
        delta_boot1_to_display = td - t1
        print("\n[KPI-BOOT-01] Display initialization benchmark:")
        print(f"  - Boot Step 1 timestamp: {t1} ms")
        print(f"  - Boot Step 6 (Display Subsystem) timestamp: {t6} ms")
        if m_disp:
            print(f"  - Display Subsystem initialized timestamp: {td} ms")
        print(f"  - Execution latency (Step 1 -> Display Ready): {delta_boot1_to_display} ms")
        print("  - KPI Target: < 1500 ms (FSD §4.9 / KPI-BOOT-01)")
        if delta_boot1_to_display < 1500:
            print(f"  ✓ KPI-BOOT-01 PASSED ({delta_boot1_to_display} ms < 1500 ms)")
            kpi_boot_passed = True
        else:
            print(f"  ✗ KPI-BOOT-01 FAILED ({delta_boot1_to_display} ms >= 1500 ms)")

    # 3. Task Supervisor Active Mask 0x7F
    sup_active_match = re.search(r"Task Active Mask:\s*(0x[0-9A-Fa-f]+)", combined_log)
    sup_alive_match = re.search(r"Task Alive Mask\s*:\s*(0x[0-9A-Fa-f]+)\s*\((.*?)\)", combined_log)

    active_mask = sup_active_match.group(1) if sup_active_match else None
    alive_mask = sup_alive_match.group(1) if sup_alive_match else None
    health_status = sup_alive_match.group(2) if sup_alive_match else None

    print("\n[SUPERVISOR] Task Supervisor Health Status:")
    print(f"  - Active Task Mask: {active_mask} (Expected: 0x7F - 7 production tasks)")
    print(f"  - Alive Task Mask : {alive_mask} (Expected: 0x7F)")
    print(f"  - System Health   : {health_status} (Expected: HEALTHY)")

    supervisor_passed = (active_mask == "0x7F" and alive_mask == "0x7F" and "HEALTHY" in (health_status or ""))

    # 4. Alive Markers
    alive1 = "[SYS] Alive marker 1" in combined_log
    alive2 = "[SYS] Alive marker 2" in combined_log
    print("\n[ALIVE MARKERS] System Heartbeats:")
    print(f"  - Alive marker 1: {'✓ FOUND' if alive1 else '✗ MISSING'}")
    print(f"  - Alive marker 2: {'✓ FOUND' if alive2 else '✗ MISSING'}")

    all_steps_ok = all(step_results[s] is not None for s in range(1, 17))
    overall = all_steps_ok and kpi_boot_passed and supervisor_passed and alive1 and alive2

    print("\n" + "="*60)
    print(f"OVERALL EVALUATION: {'PASSED (ALL 5 CRITERIA MET)' if overall else 'FAILED'}")
    print("="*60)

    return 0 if overall else 1

if __name__ == "__main__":
    sys.exit(main())
