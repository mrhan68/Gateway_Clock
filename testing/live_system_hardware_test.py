#!/usr/bin/env python3
"""
Simultaneous All-Component Live Test Suite with Real Hardware Firmware
=====================================================================
Orchestrates and verifies ALL components of the ecosystem SIMULTANEOUSLY:
1. Real ESP32-S3 Hardware running live firmware on COM7
2. Backend REST API Server (port 3000)
3. Embedded Aedes MQTT Broker (port 1883)
4. PostgreSQL In-Memory Database Store
5. Web Client / Actuator & Telemetry Consumers

Conforms to: fsd-smart-clock-gateway.md (FSD v1.1)
"""

import argparse
import json
import logging
import os
import re
import socket
import subprocess
import sys
import time

if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8')
from typing import List, Optional, Tuple

import paho.mqtt.client as mqtt
import requests
import serial

# ── Terminal Colors ─────────────────────────────────────────────────────────
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"
COLOR_GREEN = "\033[92m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_MAGENTA = "\033[95m"

logging.basicConfig(
    level=logging.INFO,
    format=f"{COLOR_CYAN}[%(asctime)s]{COLOR_RESET} %(levelname)s: %(message)s",
    datefmt="%H:%M:%S"
)
logger = logging.getLogger("LIVE_HARDWARE_E2E")


def banner(text: str):
    sep = "=" * 80
    print(f"\n{COLOR_BOLD}{COLOR_MAGENTA}{sep}\n {text}\n{sep}{COLOR_RESET}\n")


def check_port_open(host: str, port: int) -> bool:
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(1.0)
    try:
        s.connect((host, port))
        s.close()
        return True
    except Exception:
        return False


class LiveHardwareSystemTester:
    def __init__(self, com_port: str = "COM7", baud: int = 115200, api_url: str = "http://localhost:3000", mqtt_host: str = "localhost", mqtt_port: int = 1883):
        if sys.platform.startswith("linux") and (com_port == "COM7" or not os.path.exists(com_port)):
            if os.path.exists("/dev/ttyACM0"):
                com_port = "/dev/ttyACM0"
        elif sys.platform.startswith("win"):
            if not check_port_open(mqtt_host, mqtt_port):
                try:
                    out = subprocess.check_output(["wsl", "hostname", "-I"], text=True).strip()
                    wsl_ip = out.split()[0]
                    if check_port_open(wsl_ip, mqtt_port):
                        mqtt_host = wsl_ip
                        if "localhost" in api_url or "127.0.0.1" in api_url:
                            api_url = api_url.replace("localhost", wsl_ip).replace("127.0.0.1", wsl_ip)
                except Exception:
                    pass
        self.com_port = com_port
        self.baud = baud
        self.api_url = api_url.rstrip("/")
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port

        self.ser: Optional[serial.Serial] = None
        self.mqtt_client: Optional[mqtt.Client] = None
        self.backend_process: Optional[subprocess.Popen] = None

        # Real Hardware Identity
        self.mac_address = "28:84:85:50:69:E0"
        self.gateway_id = "gw_esp32_s3_2884855069e0"
        self.sensor_id = "sensor_temp_01"
        self.actuator_id = "relay_1"

        self.test_results: List[Tuple[str, bool, str]] = []

    def log_result(self, name: str, passed: bool, detail: str):
        self.test_results.append((name, passed, detail))
        mark = f"{COLOR_GREEN}[PASS]{COLOR_RESET}" if passed else f"{COLOR_RED}[FAIL]{COLOR_RESET}"
        print(f"  {mark} {name}: {detail}")

    # ── Serial Communication Helpers ────────────────────────────────────────
    def send_cli_command(self, cmd: str, wait_sec: float = 0.8) -> str:
        """Send command to ESP32-S3 over COM7 and collect response buffer."""
        if not self.ser or not self.ser.is_open:
            return ""
        self.ser.reset_input_buffer()
        cmd_bytes = (cmd.strip() + "\r\n").encode("utf-8")
        self.ser.write(cmd_bytes)
        time.sleep(wait_sec)

        output = ""
        start = time.time()
        while time.time() - start < 2.0:
            line = self.ser.readline()
            if line:
                output += line.decode("utf-8", errors="replace")
            else:
                break
        return output

    # ── Test Execution Pipeline ─────────────────────────────────────────────
    def run_all(self):
        banner("SIMULTANEOUS ALL-COMPONENT LIVE TEST: REAL FIRMWARE + BACKEND + MQTT + WEB")
        print(f"  Target Hardware:  ESP32-S3 on {self.com_port} ({self.baud} baud)")
        print(f"  Target Backend:   {self.api_url}")
        print(f"  Target MQTT:      {self.mqtt_host}:{self.mqtt_port}")
        print(f"  Gateway MAC:      {self.mac_address}")
        print(f"  Gateway ID:       {self.gateway_id}\n")

        try:
            self.step_1_ensure_services()
            self.step_2_connect_hardware()
            self.step_3_hardware_diagnostics()
            self.step_4_backend_handshake_and_provisioning()
            self.step_5_hardware_relay_actuation()
            self.step_6_edge_rule_trigger_and_telemetry()
            self.step_7_event_bus_burst_and_supervisor_health()
        finally:
            self.cleanup()

        self.print_summary()

    # ── Step 1: Ensure Backend Services ─────────────────────────────────────
    def step_1_ensure_services(self):
        banner("STEP 1: Verifying Backend REST API & Embedded MQTT Broker Services")
        
        backend_ok = False
        try:
            r = requests.get(f"{self.api_url}/health", timeout=2.0)
            backend_ok = (r.status_code == 200)
        except Exception:
            backend_ok = False

        mqtt_ok = check_port_open(self.mqtt_host, self.mqtt_port)

        self.log_result("STEP-1.1-REST-API-READY", backend_ok, "REST API listening on port 3000")
        self.log_result("STEP-1.2-MQTT-BROKER-READY", mqtt_ok, "Aedes MQTT Broker listening on port 1883")
        if not (backend_ok and mqtt_ok):
            raise RuntimeError("Backend services could not be reached!")

    # ── Step 2: Connect Hardware Serial ─────────────────────────────────────
    def step_2_connect_hardware(self):
        banner("STEP 2: Connecting to Physical ESP32-S3 Board on COM7")
        try:
            self.ser = serial.Serial(self.com_port, self.baud, timeout=1.0)
            time.sleep(0.5)
            self.log_result("STEP-2.1-SERIAL-CONNECTED", True, f"Serial port {self.com_port} opened successfully")
        except Exception as e:
            self.log_result("STEP-2.1-SERIAL-CONNECTED", False, f"Failed to open {self.com_port}: {e}")
            raise

    # ── Step 3: Hardware Diagnostics & Health ───────────────────────────────
    def step_3_hardware_diagnostics(self):
        banner("STEP 3: Real Firmware Hardware Health & Diagnostics")

        # Check 'status' command
        out_status = self.send_cli_command("status")
        has_alive = "Alive Mask : 0x7F" in out_status or "HEALTHY" in out_status
        has_mask = "Active Mask: 0x7F" in out_status

        # Extract Free Heap
        heap_match = re.search(r"Free Heap\s*:\s*(\d+)\s*bytes", out_status)
        free_heap = int(heap_match.group(1)) if heap_match else 0
        heap_ok = free_heap > 50000

        self.log_result(
            "STEP-3.1-TASK-SUPERVISOR-HEALTHY",
            has_alive and has_mask,
            "Active Mask: 0x7F, Alive Mask: 0x7F (All 7 FreeRTOS tasks ALIVE)"
        )
        self.log_result(
            "STEP-3.2-FREE-HEAP-STABILITY",
            heap_ok,
            f"Free Heap on chip: {free_heap} bytes (> 50,000 bytes required)"
        )

        # Check 'ble status' command
        out_ble = self.send_cli_command("ble status")
        ble_running = "Scanner state: RUNNING" in out_ble
        self.log_result("STEP-3.3-NIMBLE-SCANNER-RUNNING", ble_running, "NimBLE BLE Scanner active on Core 0")

    # ── Step 4: Backend Handshake & Provisioning ────────────────────────────
    def step_4_backend_handshake_and_provisioning(self):
        banner("STEP 4: Real Hardware Registration & Claiming Handshake (FSD §8.2)")

        # 1. Register Hardware Claim
        res_reg = requests.post(
            f"{self.api_url}/api/v1/gateways/register-claim",
            json={"mac_address": self.mac_address, "gateway_id": self.gateway_id}
        )
        reg_json = res_reg.json() if res_reg.status_code == 200 else {}
        claim_code = reg_json.get("claim_code")
        if reg_json.get("gateway_id"):
            self.gateway_id = reg_json.get("gateway_id")

        is_already_active = reg_json.get("status") in ("ACTIVE", "ONLINE")
        reg_ok = res_reg.status_code == 200 and (claim_code is not None or is_already_active)
        self.log_result("STEP-4.1-REGISTER-CLAIM", reg_ok, f"Claim code: '{claim_code}' (Status: {reg_json.get('status')}) for MAC {self.mac_address}")

        # 2. User Claim from Web App
        if claim_code:
            res_claim = requests.post(
                f"{self.api_url}/api/v1/gateways/claim",
                json={"claim_code": claim_code, "name": "Living Room ESP32-S3 Clock"}
            )
            claim_ok = res_claim.status_code == 200 and res_claim.json().get("gateway_id") == self.gateway_id
        else:
            claim_ok = is_already_active
        self.log_result("STEP-4.2-USER-CLAIM", claim_ok, f"Gateway '{self.gateway_id}' paired and set to ACTIVE")

        # 3. Register Devices in Backend
        res_dev1 = requests.post(
            f"{self.api_url}/api/v1/gateways/{self.gateway_id}/devices",
            json={
                "device_id": self.sensor_id,
                "name": "BLE Thermometer",
                "protocol": "ble",
                "device_type": "sensor",
                "mac_address": "AA:BB:CC:11:22:33"
            }
        )
        res_dev2 = requests.post(
            f"{self.api_url}/api/v1/gateways/{self.gateway_id}/devices",
            json={
                "device_id": self.actuator_id,
                "name": "Fan Relay 1",
                "protocol": "mqtt",
                "device_type": "actuator",
                "mqtt_topic": f"gateway/{self.gateway_id}/device/{self.actuator_id}",
                "capabilities": json.dumps(["ON", "OFF"])
            }
        )
        devices_ok = res_dev1.status_code in (200, 201, 409) and res_dev2.status_code in (200, 201, 409)
        self.log_result("STEP-4.3-REGISTER-PERIPHERALS", devices_ok, f"Registered sensor '{self.sensor_id}' & actuator '{self.actuator_id}'")

        # 4. Create Automation Rule
        res_rule = requests.post(
            f"{self.api_url}/api/v1/gateways/{self.gateway_id}/rules",
            json={
                "sensor_device_id": self.sensor_id,
                "condition_type": "gt",
                "threshold": 30.0,
                "target_device_id": self.actuator_id,
                "action_command": "ON",
                "enabled": True
            }
        )
        rule_ok = res_rule.status_code in (200, 201, 409)
        self.log_result("STEP-4.4-CREATE-RULE-ON-BACKEND", rule_ok, f"Rule: IF {self.sensor_id} > 30.0 -> {self.actuator_id} ON")

    # ── Step 5: Hardware Relay Actuation & Remote Command ───────────────────
    def step_5_hardware_relay_actuation(self):
        banner("STEP 5: Physical Hardware Relay Actuation (GPIO 15) & Web Command Dispatch")

        # 1. Dispatch manual command ON directly to hardware
        out_on = self.send_cli_command(f"cmd send {self.actuator_id} ON")
        relay_turned_on = ("turned ON" in out_on) and ("RELAY_CTRL" in out_on)
        self.log_result("STEP-5.1-HARDWARE-RELAY-ON", relay_turned_on, "Relay 1 (GPIO 15) switched ON physically")

        # 2. Dispatch manual command OFF directly to hardware
        out_off = self.send_cli_command(f"cmd send {self.actuator_id} OFF")
        relay_turned_off = ("turned OFF" in out_off) and ("RELAY_CTRL" in out_off)
        self.log_result("STEP-5.2-HARDWARE-RELAY-OFF", relay_turned_off, "Relay 1 (GPIO 15) switched OFF physically")

        # 3. Dispatch Cloud Command from Web App via REST API
        res_cmd = requests.post(
            f"{self.api_url}/api/v1/gateways/{self.gateway_id}/devices/{self.actuator_id}/command",
            json={"command": "ON", "ttl_seconds": 60}
        )
        cmd_202 = res_cmd.status_code == 202 and ("command_id" in res_cmd.json())
        cmd_id = res_cmd.json().get("command_id") if cmd_202 else None
        self.log_result("STEP-5.3-WEB-COMMAND-ACCEPTED-202", cmd_202, f"REST 202 Accepted returned with UUID '{cmd_id}'")

    # ── Step 6: Edge Rule Trigger & Telemetry Ingestion ─────────────────────
    def step_6_edge_rule_trigger_and_telemetry(self):
        banner("STEP 6: Real Hardware Rule Engine Edge Automation & Telemetry Uplink")

        # 1. Update rule on chip
        self.send_cli_command(f"rule add 1 {self.sensor_id} GT 30.0 {self.actuator_id} ON")

        # 2. Inject telemetry into real ESP32-S3 firmware: 33.5°C (> 30.0°C)
        out_rule_test = self.send_cli_command(f"rule test {self.sensor_id} 33.5 1")
        rule_fired = ("[GT EDGE-UP]" in out_rule_test) and ("Rule #1 TRIGGERED" in out_rule_test)
        relay_actuated = ("turned ON" in out_rule_test) and ("RELAY_CTRL" in out_rule_test)
        self.log_result(
            "STEP-6.1-HARDWARE-RULE-ENGINE-TRIGGER",
            rule_fired and relay_actuated,
            "Rule Engine on chip triggered! 33.5°C >= 30.0°C -> Relay 1 ON executed"
        )

        # 3. Publish Telemetry to MQTT Broker (simulating NimBLE uplink)
        try:
            mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        except Exception:
            mqttc = mqtt.Client()
        mqtt_target = self.mqtt_host
        mqttc.connect(mqtt_target, self.mqtt_port, keepalive=60)
        mqttc.loop_start()
        telem_topic = f"gateway/{self.gateway_id}/device/{self.sensor_id}/telemetry"
        inf = mqttc.publish(telem_topic, json.dumps({
            "device_id": self.sensor_id,
            "value": 33.5,
            "unit": "°C",
            "valid": True,
            "timestamp": int(time.time())
        }), qos=1)
        inf.wait_for_publish(timeout=3.0)
        time.sleep(0.5)
        mqttc.loop_stop()
        mqttc.disconnect()

        # 4. Verify Backend stored telemetry in DB
        res_telem = requests.get(f"{self.api_url}/api/v1/gateways/{self.gateway_id}/devices/{self.sensor_id}/telemetry?limit=1")
        telem_records = res_telem.json() if res_telem.status_code == 200 else []
        telem_val = telem_records[0].get("value") if len(telem_records) > 0 else None
        telem_ok = telem_val is not None and abs(telem_val - 33.5) < 0.1
        self.log_result("STEP-6.2-DB-TELEMETRY-VERIFIED", telem_ok, f"Telemetry value in DB: {telem_val}°C")

    # ── Step 7: Event Bus Burst & Supervisor Health ─────────────────────────
    def step_7_event_bus_burst_and_supervisor_health(self):
        banner("STEP 7: Real Hardware Event Bus Burst & Supervisor Non-blocking Health")

        out_burst = self.send_cli_command("test burst 5")
        burst_ok = ("Accepted=5" in out_burst) and ("Dropped=0" in out_burst)
        self.log_result("STEP-7.1-EVENT-BUS-BURST-PASS", burst_ok, "5 burst events accepted into 32-slot queue without drops")

        # Final health verify
        out_health = self.send_cli_command("status")
        healthy = "Alive Mask : 0x7F" in out_health
        self.log_result("STEP-7.2-POST-TEST-HEALTH", healthy, "Supervisor confirms 100% HEALTHY (Mask 0x7F) post-test")

    # ── Cleanup ─────────────────────────────────────────────────────────────
    def cleanup(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
            logger.info(f"Closed serial connection to {self.com_port}")
        if self.backend_process:
            self.backend_process.terminate()
            logger.info("Terminated local backend process")

    # ── Summary ─────────────────────────────────────────────────────────────
    def print_summary(self):
        banner("SIMULTANEOUS ALL-COMPONENT LIVE HARDWARE TEST SUMMARY")
        total = len(self.test_results)
        passed = sum(1 for _, ok, _ in self.test_results if ok)
        failed = total - passed

        print(f"Total Test Steps: {total}")
        print(f"Passed:           {COLOR_GREEN}{passed}{COLOR_RESET}")
        print(f"Failed:           {COLOR_RED}{failed}{COLOR_RESET}")
        print("-" * 80)

        for name, ok, detail in self.test_results:
            status = f"{COLOR_GREEN}PASS{COLOR_RESET}" if ok else f"{COLOR_RED}FAIL{COLOR_RESET}"
            print(f"[{status}] {name:<35} : {detail}")

        print("=" * 80)
        if failed == 0:
            print(f"\n{COLOR_BOLD}{COLOR_GREEN}🏆 100% SUCCESS: ALL COMPONENTS OPERATING SIMULTANEOUSLY WITH REAL FIRMWARE!{COLOR_RESET}\n")
        else:
            print(f"\n{COLOR_BOLD}{COLOR_RED}⚠ SOME TEST STEPS FAILED. CHECK HARDWARE / NETWORK LOGS.{COLOR_RESET}\n")


def main():
    parser = argparse.ArgumentParser(description="Live All-Component Hardware System Test")
    parser.add_argument("--port", default="COM7", help="ESP32-S3 Serial Port (default: COM7)")
    parser.add_argument("--baud", type=int, default=115200, help="Baud rate (default: 115200)")
    parser.add_argument("--api-url", default="http://localhost:3000", help="Backend REST API URL")
    parser.add_argument("--mqtt-host", default="localhost", help="MQTT Broker host")
    parser.add_argument("--mqtt-port", type=int, default=1883, help="MQTT Broker port")

    args = parser.parse_args()

    tester = LiveHardwareSystemTester(
        com_port=args.port,
        baud=args.baud,
        api_url=args.api_url,
        mqtt_host=args.mqtt_host,
        mqtt_port=args.mqtt_port
    )
    tester.run_all()


if __name__ == "__main__":
    main()
