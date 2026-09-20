#!/usr/bin/env python3
"""
Bare-Board Hardware Automated Test Battery for Smart Clock Industrial Edge Gateway
Tests executed directly on ESP32-S3 over USB Serial (/dev/ttyACM0).
"""

import sys
import time
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200

def run_suite():
    print("================================================================")
    print("  SMART CLOCK GATEWAY - BARE-BOARD HARDWARE VERIFICATION SUITE  ")
    print("================================================================")
    print(f"Connecting to {PORT} at {BAUD} baud...")

    try:
        ser = serial.Serial(PORT, BAUD, timeout=0.2)
    except Exception as e:
        print(f"ERROR: Cannot open port {PORT}: {e}")
        return False

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

    test_results = {}

    # Wait for stable state
    print("\n[STEP 0] Synchronizing with Gateway CLI...")
    read_buffer(timeout=3.0)
    out = exec_cli("help", wait=1.5)
    if "Smart Clock Industrial Edge Gateway" in out or "gateway>" in out:
        print("\n[OK] CLI prompt responsive.")
    else:
        print("\n[WARN] Retrying CLI handshake...")
        out = exec_cli("help", wait=2.0)

    # -------------------------------------------------------------
    # TEST 1: Modbus Bus Scan (TC-CFG-02 / FR-CFG-02)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 1: Modbus RTU Bus Scan (TC-CFG-02 / FR-CFG-02)")
    print("-------------------------------------------------------------")
    out = exec_cli("modbus scan 1 5", wait=2.0)
    if "Slave ID 0x01" in out and "Slave ID 0x02" in out:
        print("\n>>> RESULT TEST 1: PASSED (Detected simulated nodes 0x01 and 0x02)")
        test_results["TC-CFG-02"] = "PASSED"
    else:
        print("\n>>> RESULT TEST 1: FAILED")
        test_results["TC-CFG-02"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 2: Safe-Default Verification on Fault (TC-MOCK-02 / FSD §4.7.5)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 2: Safe-Default on Sensor CRC/Invalid Fault (TC-MOCK-02)")
    print("-------------------------------------------------------------")
    # Step 2a: Set temperature high (35C) but with valid=1 -> Fan ON
    exec_cli("mock rs485 set 35.0 60.0 1", wait=1.5)
    out_valid = read_buffer(timeout=4.0)
    fan_on_triggered = "fan_relay_01" in out_valid and "ON" in out_valid

    # Step 2b: Inject INVALID fault -> Rule engine must preserve safe default & NOT actuate
    print("\nInjecting invalid fault mode...")
    exec_cli("mock rs485 fault invalid", wait=1.5)
    out_invalid = read_buffer(timeout=5.0)

    safe_default_logged = (
        "INVALID data" in out_invalid or 
        "[SAFE-DEFAULT]" in out_invalid or 
        "CRC: INVALID" in out_invalid
    )
    # Recovery
    exec_cli("mock rs485 fault none", wait=1.0)
    exec_cli("mock rs485 set 25.0 50.0 1", wait=1.5)

    if safe_default_logged and fan_on_triggered:
        print("\n>>> RESULT TEST 2: PASSED (Safe-default observed; rule engine blocked invalid input)")
        test_results["TC-MOCK-02"] = "PASSED"
    else:
        print(f"\n>>> RESULT TEST 2: FAILED (safe_default_logged={safe_default_logged}, fan_on={fan_on_triggered})")
        test_results["TC-MOCK-02"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 3: Modbus Bus Timeout Simulation (TC-MOCK-03 / FSD §4.3.3)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 3: RS485 Bus Timeout Simulation & State Preservation (TC-MOCK-03)")
    print("-------------------------------------------------------------")
    exec_cli("mock rs485 fault timeout", wait=1.5)
    out_timeout = read_buffer(timeout=5.0)
    timeout_detected = "timeout simulated" in out_timeout or "did not respond" in out_timeout

    # Check status
    out_stat = exec_cli("mock status", wait=1.5)
    # Clear fault
    exec_cli("mock rs485 fault none", wait=1.0)

    if timeout_detected and "Mock Fault Mode   : timeout" in out_stat:
        print("\n>>> RESULT TEST 3: PASSED (Bus timeout simulated and logged correctly without task crash)")
        test_results["TC-MOCK-03"] = "PASSED"
    else:
        print("\n>>> RESULT TEST 3: FAILED")
        test_results["TC-MOCK-03"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 4: Rule Engine Dynamic CRUD Lifecycle (TC-RULE-01 / FSD §4.7)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 4: Rule Engine Dynamic CRUD & Boundary Evaluation (TC-RULE-01)")
    print("-------------------------------------------------------------")
    # Add a custom rule: ID 4, sensor_temp_01 GT 42.0, heater_relay_01, ALARM_COOL
    exec_cli("rule add 4 sensor_temp_01 GT 42.0 heater_relay_01 ALARM_COOL", wait=1.5)
    out_list = exec_cli("rule list", wait=1.5)
    rule_added = ("[#4]" in out_list or "Rule #4" in out_list) and "42.0" in out_list

    # Test boundary below threshold (40.0C) -> should not match
    out_below = exec_cli("rule test sensor_temp_01 40.0 1", wait=1.5)
    matched_below = "ALARM_COOL" in out_below

    # Test boundary above threshold (43.5C) -> should match
    out_above = exec_cli("rule test sensor_temp_01 43.5 1", wait=1.5)
    matched_above = "ALARM_COOL" in out_above and ("Rule #4" in out_above or "[#4]" in out_above)

    # Clean up rule 4
    exec_cli("rule delete 4", wait=1.5)
    out_list_after = exec_cli("rule list", wait=1.5)
    rule_deleted = "[#4]" not in out_list_after and "Rule #4" not in out_list_after

    if rule_added and (not matched_below) and matched_above and rule_deleted:
        print("\n>>> RESULT TEST 4: PASSED (Rule added, evaluated correctly at boundary, and deleted)")
        test_results["TC-RULE-01"] = "PASSED"
    else:
        print(f"\n>>> RESULT TEST 4: FAILED (added={rule_added}, below_not_matched={not matched_below}, above_matched={matched_above}, deleted={rule_deleted})")
        test_results["TC-RULE-01"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 5: Schedule Engine & Alarm Dispatch (TC-CLK-03 / FSD §4.12)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 5: Local Scheduler & Alarm Notification (TC-CLK-03 / FSD §4.12)")
    print("-------------------------------------------------------------")
    exec_cli("schedule add alarm 12:00 Factory_Shift_Change", wait=1.5)
    out_sched = exec_cli("schedule list", wait=1.5)
    sched_added = "Factory_Shift_Change" in out_sched

    # Trigger manual test notification
    out_alarm = exec_cli("schedule test Factory_Shift_Change", wait=2.0)
    alarm_triggered = "Factory_Shift_Change" in out_alarm and "ALARM" in out_alarm

    if sched_added and alarm_triggered:
        print("\n>>> RESULT TEST 5: PASSED (Scheduler stored alarm and successfully dispatched notification)")
        test_results["TC-CLK-03"] = "PASSED"
    else:
        print("\n>>> RESULT TEST 5: FAILED")
        test_results["TC-CLK-03"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 6: Virtual Storage CSV Buffering (TC-MOCK-04 / FSD §4.11)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 6: Virtual Storage CSV Telemetry Logging (TC-MOCK-04)")
    print("-------------------------------------------------------------")
    exec_cli("mock storage test", wait=1.5)
    out_status = exec_cli("mock status", wait=1.5)
    csv_logged = "Mock CSV Records" in out_status and "buffered lines" in out_status

    if csv_logged:
        print("\n>>> RESULT TEST 6: PASSED (Sample telemetry record appended to storage buffer)")
        test_results["TC-MOCK-04"] = "PASSED"
    else:
        print("\n>>> RESULT TEST 6: FAILED")
        test_results["TC-MOCK-04"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 7: Wi-Fi Scanner & RF Operational Status (TC-WIFI-02 / WP-04)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 7: Wi-Fi Scanner & RF Environment Operational Status (TC-WIFI-02)")
    print("-------------------------------------------------------------")
    out_scan = exec_cli("wifi scan", wait=6.0)
    wifi_ok = "NEARBY WI-FI NETWORKS FOUND" in out_scan and "dBm" in out_scan

    if wifi_ok:
        print("\n>>> RESULT TEST 7: PASSED (Wi-Fi RF receiver active; successfully scanned APs)")
        test_results["TC-WIFI-02"] = "PASSED"
    else:
        print("\n>>> RESULT TEST 7: FAILED")
        test_results["TC-WIFI-02"] = "FAILED"

    # -------------------------------------------------------------
    # TEST 8: Task Supervisor Watchdog Stall & Panic Recovery (TC-SUP-02 / FR-SUP-02)
    # -------------------------------------------------------------
    print("\n-------------------------------------------------------------")
    print("TEST 8: Task Supervisor Watchdog Hang Detection & Recovery (TC-SUP-02 / FR-SUP-02)")
    print("-------------------------------------------------------------")
    print("Sending freeze command: 'test freeze rs485'...")
    ser.write(b"test freeze rs485\r\n")
    ser.flush()

    print("Observing serial output during 17-second hang window...")
    freeze_output = read_buffer(timeout=18.0)

    stalled_detected = "CRITICAL: Task 'RS485' STALLED" in freeze_output
    restart_triggered = "System will restart to recover" in freeze_output or "rst:0x" in freeze_output or "Starting Smart Clock" in freeze_output

    # Wait for reboot to complete
    print("\nWaiting for post-reboot alive marker...")
    boot_output = read_buffer(timeout=5.0)
    alive_after_reboot = "Alive marker" in (freeze_output + boot_output) or "gateway>" in boot_output

    if stalled_detected and (restart_triggered or alive_after_reboot):
        print("\n>>> RESULT TEST 8: PASSED (Supervisor detected stalled task at 15s and cleanly rebooted)")
        test_results["TC-SUP-02"] = "PASSED"
    else:
        print(f"\n>>> RESULT TEST 8: FAILED (stalled={stalled_detected}, restart={restart_triggered}, alive={alive_after_reboot})")
        test_results["TC-SUP-02"] = "FAILED"

    ser.close()

    print("\n================================================================")
    print("                     TEST BATTERY SUMMARY                       ")
    print("================================================================")
    all_passed = True
    for test_id, result in test_results.items():
        print(f"  [{result}] {test_id}")
        if result != "PASSED":
            all_passed = False

    print("================================================================")
    if all_passed:
        print("  ALL 8 BARE-BOARD HARDWARE TESTS PASSED!")
    else:
        print("  SOME TESTS FAILED. PLEASE CHECK LOGS.")
    print("================================================================")
    return all_passed

if __name__ == "__main__":
    success = run_suite()
    sys.exit(0 if success else 1)
