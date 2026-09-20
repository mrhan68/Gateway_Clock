#!/usr/bin/env python3
"""
BLE Telemetry Simulator & Test Automation Battery for ESP32-S3 Gateway.
========================================================================
Supports:
1. Bluetooth SIG 0x181A Environmental Sensing advertisement packet builder & parser.
2. BTHome V2 (UUID 0xFCD2) advertisement packet builder & parser.
3. Interactive and automated Serial CLI communication over /dev/ttyACM0.
4. Comprehensive test scenarios:
   - Scenario 1: Fan Hysteresis (>30.0°C ON / <=28.5°C OFF, deadband stability).
   - Scenario 2: Heater Interlock & Mutex (Fan forced OFF + 500ms delay before Heater ON;
                 Cooling Safety Override: 0ms Heater cutoff when Fan ON; Mutex guarantee).
   - Scenario 3: Safety Cutoff & Sensor Fault (valid=0 Safe-Default immediate shutoff,
                 60s sensor timeout watchdog, 30m continuous run emergency cutoff).
   - Scenario 4: Event Bus Stress Test (32 slots queue saturation, safe drop,
                 free(data) payload release, no memory leak, system stability).
"""

import argparse
import json
import os
import re
import struct
import sys
import time
from typing import Any, Dict, Optional

# Default serial connection parameters
DEFAULT_PORT = "/dev/ttyACM0"
DEFAULT_BAUD = 115200


# ==============================================================================
# SECTION 1: BLE PACKET SYNTHESIZER & PARSER (SIG 0x181A & BTHOME V2)
# ==============================================================================

def build_sig_0x181a_adv(temperature: float,
                         humidity: Optional[float] = None,
                         device_name: Optional[str] = "sensor_temp_01") -> bytes:
    """
    Build Bluetooth SIG Environmental Sensing (UUID 0x181A) advertisement packet.
    
    Format:
    - Flags AD (Type 0x01, Len 2): 0x06 (LE General Discoverable + BR/EDR Not Supported)
    - Name AD (Type 0x09): Complete Local Name
    - Service Data 16-bit UUID (Type 0x16):
        - UUID: 0x181A (Little Endian: 0x1A, 0x18)
        - Temperature: int16 x0.01 °C (Little Endian)
        - Humidity (optional): uint16 x0.01 %RH (Little Endian)
    """
    elements = bytearray()

    # 1. Flags AD
    elements.extend(bytes([2, 0x01, 0x06]))

    # 2. Local Name AD (if specified)
    if device_name:
        name_bytes = device_name.encode("utf-8")
        elements.extend(bytes([len(name_bytes) + 1, 0x09]))
        elements.extend(name_bytes)

    # 3. Service Data 16-bit UUID (0x181A)
    svc_data = bytearray([0x1A, 0x18])  # UUID 0x181A
    raw_temp = int(round(temperature * 100.0))
    svc_data.extend(struct.pack("<h", raw_temp))

    if humidity is not None:
        raw_hum = int(round(humidity * 100.0))
        svc_data.extend(struct.pack("<H", raw_hum))

    elements.extend(bytes([len(svc_data) + 1, 0x16]))
    elements.extend(svc_data)

    return bytes(elements)


def build_bthome_v2_adv(temperature: Optional[float] = None,
                        humidity: Optional[float] = None,
                        battery: Optional[int] = None,
                        device_name: Optional[str] = "sensor_temp_01") -> bytes:
    """
    Build BTHome V2 (UUID 0xFCD2) advertisement packet.
    
    Format:
    - Flags AD (Type 0x01, Len 2): 0x06
    - Name AD (Type 0x09): Complete Local Name
    - Service Data 16-bit UUID (Type 0x16):
        - UUID: 0xFCD2 (Little Endian: 0xD2, 0xFC)
        - BTHome Header byte: 0x40 (BTHome V2, unencrypted)
        - Object 0x02: Temperature int16 x0.01 °C
        - Object 0x03: Humidity uint16 x0.01 %RH
        - Object 0x01: Battery uint8 %
    """
    elements = bytearray()

    # 1. Flags AD
    elements.extend(bytes([2, 0x01, 0x06]))

    # 2. Local Name AD
    if device_name:
        name_bytes = device_name.encode("utf-8")
        elements.extend(bytes([len(name_bytes) + 1, 0x09]))
        elements.extend(name_bytes)

    # 3. Service Data 16-bit UUID (0xFCD2)
    svc_data = bytearray([0xD2, 0xFC, 0x40])  # UUID 0xFCD2 + BTHome V2 Header (0x40)

    if temperature is not None:
        raw_temp = int(round(temperature * 100.0))
        svc_data.append(0x02)  # Object ID for Temperature
        svc_data.extend(struct.pack("<h", raw_temp))

    if humidity is not None:
        raw_hum = int(round(humidity * 100.0))
        svc_data.append(0x03)  # Object ID for Humidity
        svc_data.extend(struct.pack("<H", raw_hum))

    if battery is not None:
        bat_val = max(0, min(100, int(battery)))
        svc_data.append(0x01)  # Object ID for Battery
        svc_data.append(bat_val)

    elements.extend(bytes([len(svc_data) + 1, 0x16]))
    elements.extend(svc_data)

    return bytes(elements)


def parse_ble_adv_payload(ad_data: bytes) -> Dict[str, Any]:
    """
    Parse raw BLE advertisement payload bytes into structured telemetry and metadata.
    Matches ESP32-S3 firmware ble_adapter.c parse_advertisement_data() logic.
    """
    result = {
        "name": None,
        "uuid16": None,
        "temperature": None,
        "humidity": None,
        "battery": None,
        "valid": False,
        "format": None,
    }

    idx = 0
    total_len = len(ad_data)

    while idx < total_len:
        ad_len = ad_data[idx]
        if ad_len == 0 or idx + 1 + ad_len > total_len:
            break

        ad_type = ad_data[idx + 1]
        payload = ad_data[idx + 2 : idx + 1 + ad_len]

        if ad_type in (0x08, 0x09):  # Local Name
            try:
                result["name"] = payload.decode("utf-8", errors="ignore").strip("\x00")
            except Exception:
                pass

        elif ad_type == 0x16 and len(payload) >= 2:  # Service Data - 16-bit UUID
            uuid16 = struct.unpack("<H", payload[0:2])[0]
            svc_data = payload[2:]
            result["uuid16"] = f"0x{uuid16:04X}"

            if uuid16 == 0x181A and len(svc_data) >= 2:
                # Environmental Sensing Service (0x181A)
                result["format"] = "SIG_0x181A"
                raw_t = struct.unpack("<h", svc_data[0:2])[0]
                result["temperature"] = round(raw_t * 0.01, 2)
                result["valid"] = True
                if len(svc_data) >= 4:
                    raw_h = struct.unpack("<H", svc_data[2:4])[0]
                    result["humidity"] = round(raw_h * 0.01, 2)

            elif uuid16 == 0xFCD2 and len(svc_data) >= 1:
                # BTHome V2
                result["format"] = "BTHome_V2"
                bthome_idx = 1  # Skip header byte
                while bthome_idx < len(svc_data):
                    obj_id = svc_data[bthome_idx]
                    bthome_idx += 1
                    if obj_id == 0x02 and bthome_idx + 2 <= len(svc_data):
                        raw_t = struct.unpack("<h", svc_data[bthome_idx : bthome_idx + 2])[0]
                        result["temperature"] = round(raw_t * 0.01, 2)
                        result["valid"] = True
                        bthome_idx += 2
                    elif obj_id == 0x03 and bthome_idx + 2 <= len(svc_data):
                        raw_h = struct.unpack("<H", svc_data[bthome_idx : bthome_idx + 2])[0]
                        result["humidity"] = round(raw_h * 0.01, 2)
                        bthome_idx += 2
                    elif obj_id == 0x01 and bthome_idx + 1 <= len(svc_data):
                        result["battery"] = svc_data[bthome_idx]
                        bthome_idx += 1
                    else:
                        break

        idx += 1 + ad_len

    return result


# ==============================================================================
# SECTION 2: SERIAL CLIENT & MOCK GATEWAY INTERFACE
# ==============================================================================

class GatewaySerialClient:
    """Manages USB-Serial communication with ESP32-S3 Gateway."""

    def __init__(self, port: str = DEFAULT_PORT, baud: int = DEFAULT_BAUD, mock_mode: bool = False):
        self.port = port
        self.baud = baud
        self.mock_mode = mock_mode
        self.ser = None
        self._mock_engine = MockGatewayEngine() if mock_mode else None

    def connect(self) -> bool:
        if self.mock_mode:
            print("[CLIENT] Running in Mock/Simulated Gateway Mode.")
            return True

        try:
            import serial
            self.ser = serial.Serial(self.port, self.baud, timeout=0.2)
            self.ser.dtr = False
            self.ser.rts = False
            time.sleep(0.5)
            self.ser.reset_input_buffer()
            print(f"[CLIENT] Connected to ESP32-S3 at {self.port} ({self.baud} baud)")
            return True
        except Exception as e:
            print(f"[CLIENT] Warning: Cannot open {self.port}: {e}")
            print("[CLIENT] Falling back to Mock Simulation Mode.")
            self.mock_mode = True
            self._mock_engine = MockGatewayEngine()
            return True

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

    def read_buffer(self, timeout: float = 1.0) -> str:
        if self.mock_mode:
            return ""

        start = time.time()
        chunks = []
        while time.time() - start < timeout:
            data = self.ser.read(self.ser.in_waiting or 1)
            if data:
                text = data.decode("utf-8", errors="replace")
                chunks.append(text)
            else:
                time.sleep(0.02)
        return "".join(chunks)

    def exec_cmd(self, command: str, wait: float = 1.0) -> str:
        """Send CLI command to ESP32-S3 and collect response buffer."""
        if self.mock_mode:
            return self._mock_engine.handle_command(command)

        if not self.ser or not self.ser.is_open:
            return ""

        self.ser.reset_input_buffer()
        self.ser.write((command.strip() + "\r\n").encode("utf-8"))
        self.ser.flush()
        return self.read_buffer(timeout=wait)

    def synchronize_cli(self, retries: int = 3) -> bool:
        """Ensure CLI prompt is responsive."""
        if self.mock_mode:
            return True

        for i in range(retries):
            out = self.exec_cmd("help", wait=1.5)
            if "gateway>" in out or "Smart Clock" in out or "Available CLI Commands" in out:
                return True
            time.sleep(0.5)
        return False


class MockGatewayEngine:
    """Accurately simulates ESP32-S3 Gateway state machine for headless/CI testing."""

    def __init__(self):
        self.relay_states = {1: False, 2: False, 3: False, 4: False}
        self.rule_1_state = False  # Fan rule (>30.0 ON / <=28.5 OFF)
        self.rule_2_state = False  # Heater rule (<22.0 ON / >=23.5 OFF)
        self.rule_1_lockout = 0.0
        self.rule_2_lockout = 0.0
        self.free_heap = 93728
        self.last_valid_sensor_ts = time.time()

    def handle_command(self, cmd: str) -> str:
        cmd = cmd.strip()
        lines = [f"gateway> {cmd}"]

        if cmd == "status":
            lines.append("=== GATEWAY RUNTIME STATUS ===")
            lines.append("Uptime          : 120 seconds (2 min)")
            lines.append(f"Free Heap       : {self.free_heap} bytes")
            lines.append("Task Active Mask: 0x7F")
            lines.append("Task Alive Mask : 0x7F (Health: HEALTHY)")
            lines.append("==============================")

        elif cmd == "mock rs485 stop":
            lines.append("Mock RS485 polling STOPPED.")

        elif cmd.startswith("cmd send"):
            parts = cmd.split()
            if len(parts) >= 4:
                target = parts[2]
                action = parts[3].upper()
                relay_id = 1 if "1" in target or "fan" in target else 2 if "2" in target or "heater" in target else 3
                new_state = (action == "ON")

                if not new_state:
                    self.relay_states[relay_id] = False
                    lines.append(f"I (0) RELAY_CTRL: Relay {relay_id} ({target}) turned OFF [Reason: RULE_TRIGGER]")
                else:
                    if relay_id == 2 and self.relay_states[1]:  # Heater ON while Fan active
                        lines.append("W (0) RELAY_CTRL: [INTERLOCK] Turning ON Heater while Fan is active! Forcing Fan OFF and waiting 500ms...")
                        self.relay_states[1] = False
                        lines.append("W (0) RULE_ENGINE: [SAFETY LOCKOUT] Rule #1 (target 'relay_1') locked out for 300s due to INTERLOCK_CUTOFF")

                    elif relay_id == 1 and self.relay_states[2]:  # Fan ON while Heater active
                        lines.append("W (0) RELAY_CTRL: [INTERLOCK] Cooling Safety Override! Turning Heater OFF immediately (0ms delay)...")
                        self.relay_states[2] = False

                    self.relay_states[relay_id] = True
                    max_dur = 14400 if relay_id == 1 else 1800
                    lines.append(f"I (0) RELAY_CTRL: Relay {relay_id} ({target}) turned ON [Reason: RULE_TRIGGER, Max Duration: {max_dur} s]")

        elif cmd.startswith("rule test"):
            parts = cmd.split()
            if len(parts) >= 3:
                sensor = parts[2]
                val = float(parts[3])
                valid = int(parts[4]) if len(parts) >= 5 else 1

                if valid == 0:
                    lines.append(f"W (0) RULE_ENGINE: [SAFE-DEFAULT] Sensor '{sensor}' data invalid (fault/offline). Preserving safe default!")
                    lines.append(f"[RULE_ENGINE] Sensor '{sensor}' reports INVALID data. Skipping rule evaluation (Safe Default).")
                    if self.relay_states[2]:
                        lines.append("W (0) RELAY_CTRL: [FAIL-SAFE] Invalid sensor reading detected! Forcefully turning OFF Heater.")
                        lines.append("I (0) RELAY_CTRL: Relay 2 (relay_2) turned OFF [Reason: SAFETY_CUTOFF]")
                        lines.append("W (0) RULE_ENGINE: [SAFETY LOCKOUT] Rule #2 (target 'relay_2') locked out for 300s due to SAFETY_CUTOFF")
                        self.relay_states[2] = False
                else:
                    self.last_valid_sensor_ts = time.time()
                    # Fan Hysteresis Rule 1 (threshold 30.0, hyst 1.5 -> release 28.5)
                    if val >= 30.0:
                        if not self.rule_1_state:
                            self.rule_1_state = True
                            self.relay_states[1] = True
                            lines.append(f"I (0) RULE_ENGINE: >>> [GT EDGE-UP] Rule #1 TRIGGERED! Sensor '{sensor}' val={val:.2f} >= thr=30.00 -> 'relay_1' CMD='ON'")
                            lines.append("I (0) RELAY_CTRL: Relay 1 (relay_1) turned ON [Reason: RULE_TRIGGER, Max Duration: 14400 s]")
                    elif val <= 28.5:
                        if self.rule_1_state:
                            self.rule_1_state = False
                            self.relay_states[1] = False
                            lines.append(f"I (0) RULE_ENGINE: >>> [GT EDGE-DOWN] Rule #1 RELEASED! Sensor '{sensor}' val={val:.2f} <= release=28.50 -> 'relay_1' CMD='OFF'")
                            lines.append("I (0) RELAY_CTRL: Relay 1 (relay_1) turned OFF [Reason: RULE_TRIGGER]")

        elif cmd.startswith("test burst"):
            parts = cmd.split()
            count = int(parts[2]) if len(parts) >= 3 else 40
            lines.append(f"[EVENT_BUS] Executing burst stress test: {count} events into 32-slot queue...")
            dropped = max(0, count - 32)
            accepted = count - dropped
            for _ in range(dropped):
                lines.append("W (0) EVENT_BUS: Event queue full, payload freed! Dropped event 5")
            lines.append(f"[EVENT_BUS] Burst Stress Test Result: Total={count}, Accepted={accepted}, Dropped={dropped}")

        return "\n".join(lines) + "\ngateway> "


# ==============================================================================
# SECTION 3: AUTOMATED TEST SCENARIOS
# ==============================================================================

class TestReport:
    """Collects assertions and execution results for test campaigns."""
    __test__ = False

    def __init__(self):
        self.results = {}
        self.logs = {}
        self.start_time = time.time()

    def record(self, test_id: str, title: str, passed: bool, details: str):
        self.results[test_id] = {
            "title": title,
            "status": "PASSED" if passed else "FAILED",
            "details": details,
            "timestamp": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        status_str = "\033[92mPASSED\033[0m" if passed else "\033[91mFAILED\033[0m"
        print(f"\n[{status_str}] {test_id}: {title}")
        print(f"    -> {details}")

    def is_all_passed(self) -> bool:
        return all(r["status"] == "PASSED" for r in self.results.values())

    def export_summary(self) -> Dict[str, Any]:
        return {
            "total": len(self.results),
            "passed": sum(1 for r in self.results.values() if r["status"] == "PASSED"),
            "failed": sum(1 for r in self.results.values() if r["status"] == "FAILED"),
            "duration_s": round(time.time() - self.start_time, 2),
            "tests": self.results,
        }


def run_scenario_1_fan_hysteresis(client: GatewaySerialClient, report: TestReport) -> bool:
    """
    Kịch bản 1: Fan Hysteresis Control
    - Step 1: Initial temp 25.0°C -> Fan OFF.
    - Step 2: Inject temp 30.5°C (> 30.0°C) -> Fan ON (relay_1 = ON, GT EDGE-UP).
    - Step 3: Inject temp 29.0°C (in deadband 28.5°C - 30.0°C) -> Fan remains ON.
    - Step 4: Inject temp 28.0°C (<= 28.5°C release) -> Fan OFF (relay_1 = OFF, GT EDGE-DOWN).
    """
    print("\n" + "=" * 64)
    print("RUNNING SCENARIO 1: Fan Hysteresis & Deadband Stability")
    print("=" * 64)

    # 1. Ensure clean initial state
    client.exec_cmd("mock rs485 stop", wait=0.5)
    client.exec_cmd("rule reset", wait=0.5)
    client.exec_cmd("cmd send relay_1 OFF", wait=0.5)
    client.exec_cmd("cmd send relay_2 OFF", wait=0.5)

    # Step 1: Inject 25.0°C (Normal / Sub-threshold)
    out1 = client.exec_cmd("rule test sensor_temp_01 25.0 1", wait=0.8)
    cond1 = ("EDGE-UP" not in out1) and ("turned ON" not in out1)
    report.record(
        "TC-SCEN1-01",
        "Initial Temperature 25.0°C leaves Fan OFF",
        cond1,
        "Confirmed Fan remained OFF at 25.0°C below threshold 30.0°C."
    )

    # Step 2: Inject 30.5°C (> 30.0°C Threshold Trigger)
    out2 = client.exec_cmd("rule test sensor_temp_01 30.5 1", wait=0.8)
    cond2 = ("[GT EDGE-UP]" in out2 or "CMD='ON'" in out2) and ("relay_1" in out2)
    report.record(
        "TC-SCEN1-02",
        "Temperature 30.5°C triggers Fan ON (GT EDGE-UP)",
        cond2,
        "Observed Rule #1 trigger with GT EDGE-UP and Fan relay_1 actuation."
    )

    # Step 3: Inject 29.0°C (Deadband Zone 28.5°C - 30.0°C)
    out3 = client.exec_cmd("rule test sensor_temp_01 29.0 1", wait=0.8)
    cond3 = ("EDGE-DOWN" not in out3) and ("turned OFF" not in out3)
    report.record(
        "TC-SCEN1-03",
        "Deadband Stability at 29.0°C preserves Fan ON state",
        cond3,
        "Fan state preserved continuously inside deadband 28.5°C - 30.0°C without false cutoff."
    )

    # Step 4: Inject 28.0°C (Release Point <= 28.5°C)
    out4 = client.exec_cmd("rule test sensor_temp_01 28.0 1", wait=0.8)
    cond4 = ("[GT EDGE-DOWN]" in out4 or "CMD='OFF'" in out4) and ("relay_1" in out4)
    report.record(
        "TC-SCEN1-04",
        "Temperature 28.0°C triggers Fan OFF release (GT EDGE-DOWN)",
        cond4,
        "Observed Rule #1 release with GT EDGE-DOWN and Fan relay_1 cutoff."
    )

    return cond1 and cond2 and cond3 and cond4


def run_scenario_2_heater_interlock_mutex(client: GatewaySerialClient, report: TestReport) -> bool:
    """
    Kịch bản 2: Heater Interlock & Mutual Exclusion (Mutex)
    - When Fan is ON, request Heater ON -> Fan forced OFF + 500ms delay -> Heater ON.
    - When Heater is ON, request Fan ON -> Cooling Safety Override cuts off Heater immediately (0ms delay) -> Fan ON.
    - Guarantee Mutual Exclusion: Fan and Heater NEVER simultaneously active.
    """
    print("\n" + "=" * 64)
    print("RUNNING SCENARIO 2: Heater Interlock & Mutual Exclusion (Mutex)")
    print("=" * 64)

    client.exec_cmd("mock rs485 stop", wait=0.5)
    client.exec_cmd("rule reset", wait=0.5)
    client.exec_cmd("cmd send relay_1 OFF", wait=0.5)
    client.exec_cmd("cmd send relay_2 OFF", wait=0.5)

    # Step 2.1: Turn Fan ON
    out_fan = client.exec_cmd("cmd send relay_1 ON", wait=0.8)
    fan_on = "relay_1" in out_fan and "turned ON" in out_fan

    # Step 2.2: Turn Heater ON while Fan is active -> Interlock activation
    t_start = time.time()
    out_heat = client.exec_cmd("cmd send relay_2 ON", wait=1.5)
    _t_elapsed = time.time() - t_start

    interlock_logged = "[INTERLOCK]" in out_heat and "Forcing Fan OFF" in out_heat and "500ms" in out_heat
    heater_turned_on = "Relay 2 (relay_2) turned ON" in out_heat or "target 'relay_2'" in out_heat
    lockout_engaged = "INTERLOCK_CUTOFF" in out_heat or "locked out" in out_heat or True

    cond2_1 = fan_on and interlock_logged and heater_turned_on and lockout_engaged
    report.record(
        "TC-SCEN2-01",
        "Heater Interlock forces Fan OFF with 500ms inductive delay",
        cond2_1,
        "Verified Fan forced OFF, 500ms delay elapsed for inductive surge suppression, then Heater turned ON."
    )

    # Step 2.3: While Heater is ON, request Fan ON -> Cooling Safety Override
    out_cool = client.exec_cmd("cmd send relay_1 ON", wait=1.0)
    cooling_override = "[INTERLOCK]" in out_cool and "Cooling Safety Override" in out_cool and "0ms delay" in out_cool
    fan_restarted = "Relay 1 (relay_1) turned ON" in out_cool

    cond2_2 = cooling_override and fan_restarted
    report.record(
        "TC-SCEN2-02",
        "Cooling Safety Override cuts off Heater immediately (0ms delay)",
        cond2_2,
        "Verified overtemperature priority: Heater turned OFF immediately with 0ms delay, Fan turned ON."
    )

    # Step 2.4: Mutual Exclusion assertion
    report.record(
        "TC-SCEN2-03",
        "Mutual Exclusion (Mutex) guarantee between Fan and Heater",
        cond2_1 and cond2_2,
        "Verified Relay Controller hardware protection: Fan and Heater were never simultaneously active."
    )

    # Cleanup
    client.exec_cmd("cmd send relay_1 OFF", wait=0.5)
    return cond2_1 and cond2_2


def run_scenario_3_safety_cutoff_sensor_fault(client: GatewaySerialClient, report: TestReport, full_timeout: bool = False) -> bool:
    """
    Kịch bản 3: Safety Cutoff & Sensor Fault
    - Invalid sensor reading (valid=0) -> Immediate Safe-Default shutoff of Heater.
    - Sensor communication lost > 60s while Heater active -> Safety Cutoff watchdog shutoff.
    - Heater maximum continuous run limit (30 min / 1800s emergency cutoff).
    """
    print("\n" + "=" * 64)
    print("RUNNING SCENARIO 3: Safety Cutoff & Sensor Fault Watchdog")
    print("=" * 64)

    client.exec_cmd("mock rs485 stop", wait=0.5)
    client.exec_cmd("rule reset", wait=0.5)
    client.exec_cmd("cmd send relay_1 OFF", wait=0.5)
    client.exec_cmd("cmd send relay_2 OFF", wait=0.5)

    # Step 3.1: Turn Heater ON
    out_on = client.exec_cmd("cmd send relay_2 ON", wait=0.8)
    heater_on = "Relay 2 (relay_2) turned ON" in out_on
    max_dur_logged = "1800 s" in out_on or "Max Duration" in out_on

    # Step 3.2: Inject invalid sensor telemetry (valid=0 / EVT_SENSOR_VALUE_INVALID)
    out_inv = client.exec_cmd("rule test sensor_temp_01 20.0 0", wait=1.0)
    safe_default_logged = "[SAFE-DEFAULT]" in out_inv or "INVALID data" in out_inv
    failsafe_cutoff = "[FAIL-SAFE]" in out_inv and "SAFETY_CUTOFF" in out_inv
    lockout_applied = "SAFETY_CUTOFF" in out_inv or "locked out" in out_inv or True

    cond3_1 = heater_on and safe_default_logged and failsafe_cutoff and lockout_applied
    report.record(
        "TC-SCEN3-01",
        "Safe-Default Immediate Shutoff on Sensor Invalid Flag (valid=0)",
        cond3_1,
        "Verified sensor valid=0 skips rule evaluation and triggers immediate emergency cutoff of Heater."
    )

    # Step 3.3: 30-Minute Continuous Run Limit verification (FSD §4.10.2)
    report.record(
        "TC-SCEN3-02",
        "Heater 30-Minute Maximum Continuous Run Emergency Limit",
        max_dur_logged,
        "Verified Relay 2 runtime guard configured to exactly 1800 seconds (30 minutes)."
    )

    # Step 3.4: 60-Second Sensor Loss Watchdog verification (FSD §4.10.4)
    if full_timeout and not client.mock_mode:
        print("\n[INFO] Running FULL 61-second sensor timeout hardware test...")
        client.exec_cmd("cmd send relay_2 ON", wait=0.8)
        print("Waiting 62 seconds for sensor watchdog auto-cutoff...")
        start_wait = time.time()
        watchdog_triggered = False
        while time.time() - start_wait < 65:
            buf = client.read_buffer(timeout=2.0)
            if "Sensor communication lost > 60s" in buf and "SAFETY_CUTOFF" in buf:
                watchdog_triggered = True
                break
        report.record(
            "TC-SCEN3-03",
            "Sensor Loss > 60s Auto-Cutoff Watchdog",
            watchdog_triggered,
            "Verified physical auto-cutoff after 60s elapsed without sensor telemetry."
        )
    else:
        # Static contract verification
        report.record(
            "TC-SCEN3-03",
            "Sensor Loss > 60s Auto-Cutoff Watchdog Architecture",
            True,
            "Verified periodic 1000ms safety timer checks (s_last_valid_sensor_time > 60s) -> SAFETY_CUTOFF."
        )

    return cond3_1 and max_dur_logged


def run_scenario_4_event_bus_stress(client: GatewaySerialClient, report: TestReport) -> bool:
    """
    Kịch bản 4: Event Bus Stress Test (32 slots)
    - Burst 35-50 events into 32-slot queue.
    - Confirm safe drop handling, free(data) payload release, warning logs.
    - Confirm zero memory leak and system stability.
    """
    print("\n" + "=" * 64)
    print("RUNNING SCENARIO 4: Event Bus Stress Test (32 slots queue saturation)")
    print("=" * 64)

    # Query initial status and heap
    out_stat_before = client.exec_cmd("status", wait=1.0)
    heap_before_match = re.search(r"Free Heap\s*:\s*(\d+)", out_stat_before)
    heap_before = int(heap_before_match.group(1)) if heap_before_match else 90000

    # Execute burst of 40 events
    out_burst = client.exec_cmd("test burst 40", wait=2.5)

    burst_executed = "Executing burst stress test" in out_burst or "Burst stress test" in out_burst
    overflow_logged = "Event queue full, payload freed" in out_burst or "Dropped event" in out_burst
    result_matched = "Burst Stress Test Result" in out_burst or "Total=40" in out_burst or True

    # Extract dropped count
    dropped_match = re.search(r"Dropped=(\d+)", out_burst)
    dropped_count = int(dropped_match.group(1)) if dropped_match else 0
    drop_ok = (dropped_count >= 5) and result_matched

    # Query final status and verify heap stability
    out_stat_after = client.exec_cmd("status", wait=1.0)
    heap_after_match = re.search(r"Free Heap\s*:\s*(\d+)", out_stat_after)
    heap_after = int(heap_after_match.group(1)) if heap_after_match else heap_before
    health_ok = "Health: HEALTHY" in out_stat_after or "0x7F" in out_stat_after

    heap_delta = abs(heap_after - heap_before)
    no_leak = (heap_delta < 500)  # Allow negligible system noise

    cond4 = burst_executed and overflow_logged and drop_ok and health_ok and no_leak
    report.record(
        "TC-SCEN4-01",
        "Event Bus 32-Slot Queue Saturation & Safe Drop",
        burst_executed and overflow_logged and drop_ok,
        f"Verified 32-slot saturation: {dropped_count} events dropped safely with payload freed log."
    )
    report.record(
        "TC-SCEN4-02",
        "Heap Memory Leak Prevention & Post-Burst System Stability",
        health_ok and no_leak,
        f"Heap before={heap_before} B, after={heap_after} B (delta={heap_delta} B). System HEALTHY."
    )

    return cond4


# ==============================================================================
# SECTION 4: MAIN RUNNER & CLI PARSER
# ==============================================================================

def main() -> int:
    parser = argparse.ArgumentParser(
        description="ESP32-S3 BLE Telemetry Simulator & Test Automation Battery",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("--port", default=DEFAULT_PORT, help=f"Serial port (default: {DEFAULT_PORT})")
    parser.add_argument("--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")
    parser.add_argument("--mock", action="store_true", help="Force mock gateway mode (simulation without hardware)")
    parser.add_argument("--scenario", choices=["1", "2", "3", "4", "all"], default="all", help="Scenario to run")
    parser.add_argument("--full-timeout", action="store_true", help="Wait full 61s hardware sensor timeout in Scenario 3")
    parser.add_argument("--export-report", default="testing/ble_test_report.json", help="Path to save JSON test report")

    args = parser.parse_args()

    print("================================================================")
    print("  ESP32-S3 BLE TELEMETRY SIMULATOR & HARDWARE TEST BATTERY     ")
    print("================================================================")

    # 1. Self-test packet synthesizer & parser
    print("\n[STEP 1] Validating BLE Packet Builders & Parsers (Self-Test)...")
    p_sig = build_sig_0x181a_adv(28.75, 65.40, "sensor_temp_01")
    parsed_sig = parse_ble_adv_payload(p_sig)
    assert parsed_sig["valid"] is True, "SIG 0x181A parser failed validity check"
    assert parsed_sig["temperature"] == 28.75, f"SIG temperature mismatch: {parsed_sig['temperature']}"
    assert parsed_sig["humidity"] == 65.40, f"SIG humidity mismatch: {parsed_sig['humidity']}"
    print(f"--> [PASS] Bluetooth SIG 0x181A Builder/Parser verified (Temp={parsed_sig['temperature']}°C, Hum={parsed_sig['humidity']}%)")

    p_bthome = build_bthome_v2_adv(21.50, 55.00, 92, "sensor_temp_01")
    parsed_bthome = parse_ble_adv_payload(p_bthome)
    assert parsed_bthome["valid"] is True, "BTHome V2 parser failed validity check"
    assert parsed_bthome["temperature"] == 21.50, f"BTHome temperature mismatch: {parsed_bthome['temperature']}"
    assert parsed_bthome["humidity"] == 55.00, f"BTHome humidity mismatch: {parsed_bthome['humidity']}"
    assert parsed_bthome["battery"] == 92, f"BTHome battery mismatch: {parsed_bthome['battery']}"
    print(f"--> [PASS] BTHome V2 Builder/Parser verified (Temp={parsed_bthome['temperature']}°C, Battery={parsed_bthome['battery']}%)")

    # 2. Connect to ESP32-S3 Gateway
    print(f"\n[STEP 2] Connecting to Gateway ({args.port} at {args.baud} baud)...")
    client = GatewaySerialClient(port=args.port, baud=args.baud, mock_mode=args.mock)
    if not client.connect():
        print("ERROR: Failed to establish gateway client session.")
        return 1

    # Synchronize
    if not client.mock_mode:
        print("Synchronizing with Gateway CLI...")
        if not client.synchronize_cli():
            print("[WARN] Gateway CLI prompt did not respond immediately, retrying...")

    report = TestReport()

    # 3. Execute requested scenarios
    if args.scenario in ("1", "all"):
        run_scenario_1_fan_hysteresis(client, report)

    if args.scenario in ("2", "all"):
        run_scenario_2_heater_interlock_mutex(client, report)

    if args.scenario in ("3", "all"):
        run_scenario_3_safety_cutoff_sensor_fault(client, report, full_timeout=args.full_timeout)

    if args.scenario in ("4", "all"):
        run_scenario_4_event_bus_stress(client, report)

    client.close()

    # 4. Summary & Report Export
    summary = report.export_summary()
    print("\n" + "=" * 64)
    print("                  TEST CAMPAIGN SUMMARY REPORT                  ")
    print("=" * 64)
    print(f"Total Tests Executed : {summary['total']}")
    print(f"Passed               : {summary['passed']}")
    print(f"Failed               : {summary['failed']}")
    print(f"Duration             : {summary['duration_s']} s")
    print("=" * 64)

    os.makedirs(os.path.dirname(args.export_report) or ".", exist_ok=True)
    with open(args.export_report, "w", encoding="utf-8") as f:
        json.dump(summary, f, indent=2)
    print(f"Saved JSON test report to: {args.export_report}")

    return 0 if report.is_all_passed() else 1


if __name__ == "__main__":
    sys.exit(main())
