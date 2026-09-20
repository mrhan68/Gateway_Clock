#!/usr/bin/env python3
"""
Comprehensive End-to-End (E2E) System Test Battery
==================================================
Project: Smart Clock & BLE/MQTT Gateway
Architecture Chain:
    IoT Device (BLE Sensor) 
      ──► IoT Gateway (ESP32-S3 / Virtual Gateway) 
      ──► Backend Server (Aedes MQTT Broker + Express REST API) 
      ──► Database (PostgreSQL) & Web Application

Conforms to: fsd-smart-clock-gateway.md (FSD v1.1)

Test Scenarios / Phases:
1. Phase 1: Gateway Provisioning & Claim Handshake (FSD §8.2)
2. Phase 2: Peripherals & Rule Configuration Sync (FSD §4.7, §4.8, §7.3, §8.3)
3. Phase 3: BLE Telemetry Ingestion & Hysteresis Rule Trigger (FSD §4.1, §4.7, §9.1, §9.2)
4. Phase 4: Bi-directional Command Dispatch & Asynchronous ACK Uplink (FSD §8.3, §9.1, §9.2)
5. Phase 5: Store-and-Forward Offline Queue & Deduplicated Backfill (FSD §4.11.3, §9.1)
"""

import argparse
import json
import logging
import os
import random
import struct
import sys
import threading
import time
import uuid
from datetime import datetime, timezone
from typing import Any, Dict, List, Optional, Tuple

import paho.mqtt.client as mqtt
import requests

# ─── Terminal Colors ────────────────────────────────────────────────────────
COLOR_RESET = "\033[0m"
COLOR_BOLD = "\033[1m"
COLOR_GREEN = "\033[92m"
COLOR_RED = "\033[91m"
COLOR_YELLOW = "\033[93m"
COLOR_CYAN = "\033[96m"
COLOR_MAGENTA = "\033[95m"

# ─── Logging Setup ──────────────────────────────────────────────────────────
logging.basicConfig(
    level=logging.INFO,
    format=f"{COLOR_CYAN}[%(asctime)s]{COLOR_RESET} %(levelname)s: %(message)s",
    datefmt="%H:%M:%S"
)
logger = logging.getLogger("E2E_TEST")


def banner(text: str) -> None:
    sep = "=" * 80
    print(f"\n{COLOR_BOLD}{COLOR_MAGENTA}{sep}\n {text}\n{sep}{COLOR_RESET}\n")


def check_assertion(condition: bool, description: str) -> bool:
    if condition:
        print(f"  {COLOR_GREEN}✔ PASS:{COLOR_RESET} {description}")
        return True
    else:
        print(f"  {COLOR_RED}✘ FAIL:{COLOR_RESET} {description}")
        return False


# ─── Virtual BLE Packet Builder ─────────────────────────────────────────────
def build_sig_0x181a_adv(temperature: float, humidity: Optional[float] = None) -> bytes:
    """Build Bluetooth SIG 0x181A Environmental Sensing advertisement packet."""
    elements = bytearray()
    elements.extend(bytes([2, 0x01, 0x06]))  # Flags AD
    svc_data = bytearray([0x1A, 0x18])      # UUID 0x181A
    raw_temp = int(round(temperature * 100.0))
    svc_data.extend(struct.pack("<h", raw_temp))
    if humidity is not None:
        raw_hum = int(round(humidity * 100.0))
        svc_data.extend(struct.pack("<H", raw_hum))
    elements.extend(bytes([len(svc_data) + 1, 0x16]))
    elements.extend(svc_data)
    return bytes(elements)


def parse_sig_0x181a_adv(payload: bytes) -> Optional[Dict[str, Any]]:
    """Parse Bluetooth SIG 0x181A advertisement packet."""
    idx = 0
    temp_c = None
    hum_pct = None
    while idx < len(payload):
        length = payload[idx]
        if length == 0 or idx + length >= len(payload):
            break
        ad_type = payload[idx + 1]
        ad_data = payload[idx + 2: idx + 1 + length]
        if ad_type == 0x16 and len(ad_data) >= 4:
            uuid16 = struct.unpack("<H", ad_data[0:2])[0]
            if uuid16 == 0x181A:
                raw_temp = struct.unpack("<h", ad_data[2:4])[0]
                temp_c = raw_temp / 100.0
                if len(ad_data) >= 6:
                    raw_hum = struct.unpack("<H", ad_data[4:6])[0]
                    hum_pct = raw_hum / 100.0
        idx += 1 + length
    if temp_c is not None:
        return {"temperature": temp_c, "humidity": hum_pct}
    return None


# ─── Mock Virtual Gateway ───────────────────────────────────────────────────
class VirtualESP32Gateway:
    """
    Virtual ESP32-S3 Gateway conforming strictly to FSD v1.1.
    Simulates NimBLE scanner, Event Bus, Rule Engine, and MQTT Uplink/Downlink.
    """
    def __init__(self, gateway_id: str, broker_host: str, broker_port: int):
        self.gateway_id = gateway_id
        self.broker_host = broker_host
        self.broker_port = broker_port
        self.client: Optional[mqtt.Client] = None
        self.is_connected = False
        
        # State & Configuration
        self.relay_state: Dict[str, bool] = {"relay_1": False, "relay_2": False}
        self.schedules: List[Dict[str, Any]] = []
        self.rules: List[Dict[str, Any]] = []
        self.received_commands: List[Dict[str, Any]] = []
        self.received_configs: List[Dict[str, Any]] = []
        self.lock = threading.Lock()

    def start(self):
        self.client = mqtt.Client(
            client_id=f"esp32_{self.gateway_id}_{random.randint(1000, 9999)}",
            protocol=mqtt.MQTTv311
        )
        # Configure LWT per FSD §4.6.4 & §9.2
        lwt_topic = f"gateway/{self.gateway_id}/status"
        self.client.will_set(lwt_topic, payload="offline", qos=1, retain=True)

        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.connect(self.broker_host, self.broker_port, keepalive=60)
        self.client.loop_start()

        # Wait for connection
        timeout = time.time() + 5.0
        while not self.is_connected and time.time() < timeout:
            time.sleep(0.1)

    def stop(self):
        if self.client:
            # Publish offline before disconnecting
            self.client.publish(f"gateway/{self.gateway_id}/status", "offline", qos=1, retain=True)
            self.client.loop_stop()
            self.client.disconnect()

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.is_connected = True
            logger.info(f"[VirtualGateway] Connected to MQTT broker at {self.broker_host}:{self.broker_port}")
            # Retained status "online" (FSD §4.6.4)
            client.publish(f"gateway/{self.gateway_id}/status", "online", qos=1, retain=True)
            
            # Subscribe to commands and config (FSD §4.6.4, §9.1)
            cmd_topic = f"gateway/{self.gateway_id}/device/+/command"
            cfg_topic = f"gateway/{self.gateway_id}/config/schedule"
            client.subscribe([(cmd_topic, 1), (cfg_topic, 1)])
            logger.info(f"[VirtualGateway] Subscribed to {cmd_topic} and {cfg_topic}")

    def _on_message(self, client, userdata, msg):
        topic = msg.topic
        payload_str = msg.payload.decode("utf-8", errors="replace")
        logger.info(f"[VirtualGateway] Received on {topic}: {payload_str}")

        try:
            data = json.loads(payload_str)
        except Exception:
            return

        with self.lock:
            # 1. Handle Config Push (FSD §4.8.3, §9.1)
            if topic == f"gateway/{self.gateway_id}/config/schedule":
                self.received_configs.append(data)
                self.schedules = data.get("schedules", [])
                self.rules = data.get("rules", [])
                logger.info(f"[VirtualGateway] Synchronized {len(self.schedules)} schedules, {len(self.rules)} rules")
                return

            # 2. Handle Device Command Downlink (FSD §8.3, §9.2)
            if "/command" in topic and not topic.endswith("/ack"):
                self.received_commands.append(data)
                command_id = data.get("command_id")
                target = data.get("target_device_id")
                cmd = data.get("command", "").upper()
                ttl = data.get("ttl_seconds", 60)
                cmd_ts = data.get("timestamp", time.time())

                now = time.time()
                is_expired = (now - cmd_ts) > ttl if ttl > 0 else False

                if is_expired:
                    ack = {
                        "command_id": command_id,
                        "target_device_id": target,
                        "success": False,
                        "error_code": 408,  # Timeout / TTL expired
                        "message": "Command dropped: TTL expired",
                        "execution_time_ms": 2,
                        "timestamp": int(now)
                    }
                else:
                    # Execute command on simulated relay
                    new_state = (cmd == "ON")
                    self.relay_state[target] = new_state
                    ack = {
                        "command_id": command_id,
                        "target_device_id": target,
                        "success": True,
                        "error_code": 0,
                        "message": f"{target} turned {cmd} successfully",
                        "execution_time_ms": 35,
                        "timestamp": int(now)
                    }

                # Publish Command ACK Uplink (FSD §9.2)
                ack_topic = f"gateway/{self.gateway_id}/device/{target}/command/ack"
                self.client.publish(ack_topic, json.dumps(ack), qos=1)
                logger.info(f"[VirtualGateway] Uplinked ACK to {ack_topic}: success={ack['success']}")

    def ingest_ble_advertisement(self, device_id: str, raw_packet: bytes):
        """Simulate NimBLE passive scan reception & internal rule evaluation."""
        parsed = parse_sig_0x181a_adv(raw_packet)
        if not parsed:
            return

        temp = parsed["temperature"]
        now = int(time.time())

        # 1. Uplink Telemetry (FSD §9.2)
        telem_payload = {
            "device_id": device_id,
            "value": temp,
            "unit": "°C",
            "valid": True,
            "timestamp": now
        }
        telem_topic = f"gateway/{self.gateway_id}/device/{device_id}/telemetry"
        self.client.publish(telem_topic, json.dumps(telem_payload), qos=1)
        logger.info(f"[VirtualGateway] Uplinked telemetry to {telem_topic}: {temp}°C")

        # 2. Local Rule Engine Evaluation (FSD §4.7.4)
        with self.lock:
            for rule in self.rules:
                if not rule.get("enabled", True):
                    continue
                if rule.get("sensor_device_id") == device_id:
                    cond = rule.get("condition_type")
                    thresh = rule.get("threshold", 0.0)
                    target = rule.get("target_device_id")
                    action = rule.get("action_command", "").upper()

                    triggered = False
                    if cond in ("gt", "SENSOR_GT") and temp > thresh:
                        triggered = True
                    elif cond in ("lt", "SENSOR_LT") and temp < thresh:
                        triggered = True
                    elif cond in ("eq", "SENSOR_EQ") and abs(temp - thresh) < 0.1:
                        triggered = True

                    if triggered:
                        logger.info(f"[VirtualGateway] Rule Engine TRIGGERED: {device_id} {temp}°C {cond} {thresh} -> {target} {action}")
                        self.relay_state[target] = (action == "ON")
                        # Post state change event log (FSD §9.1)
                        evt_payload = {
                            "type": "rule_triggered",
                            "rule_id": rule.get("id"),
                            "sensor": device_id,
                            "value": temp,
                            "target": target,
                            "action": action,
                            "ts": now
                        }
                        self.client.publish(f"gateway/{self.gateway_id}/events", json.dumps(evt_payload), qos=1)

    def publish_batch_backfill(self, records: List[Dict[str, Any]]):
        """Simulate Store-and-Forward offline backfill upload (FSD §4.11.3 & §9.1)."""
        topic = f"gateway/{self.gateway_id}/telemetry/batch"
        payload = json.dumps({"batch": records})
        self.client.publish(topic, payload, qos=1)
        logger.info(f"[VirtualGateway] Uplinked batch backfill to {topic} ({len(records)} records)")


# ─── Comprehensive E2E System Test Runner ──────────────────────────────────
class E2ESystemTestSuite:
    def __init__(self, api_base: str, mqtt_host: str, mqtt_port: int):
        self.api_base = api_base.rstrip("/")
        self.mqtt_host = mqtt_host
        self.mqtt_port = mqtt_port
        
        self.gateway_id = f"gw_test_{uuid.uuid4().hex[:6]}"
        self.mac_address = "30:30:F9:55:01:A2"
        self.sensor_id = "sensor_temp_01"
        self.actuator_id = "relay_1"
        
        self.v_gateway: Optional[VirtualESP32Gateway] = None
        self.test_results: List[Tuple[str, bool, str]] = []

    def log_result(self, name: str, passed: bool, detail: str):
        self.test_results.append((name, passed, detail))
        check_assertion(passed, f"{name}: {detail}")

    def run_all(self):
        banner("STARTING FULL END-TO-END SYSTEM TEST (IoT -> Gateway -> Backend -> DB)")
        print(f"  Target REST API:  {self.api_base}")
        print(f"  Target MQTT:      {self.mqtt_host}:{self.mqtt_port}")
        print(f"  Test Gateway ID:  {self.gateway_id}")
        print(f"  Test Sensor ID:   {self.sensor_id}")
        print(f"  Test Actuator ID: {self.actuator_id}\n")

        try:
            self.test_phase_1_provisioning_and_claim()
            self.test_phase_2_peripherals_and_config_sync()
            self.test_phase_3_telemetry_and_automation_rule()
            self.test_phase_4_command_downlink_and_ack_uplink()
            self.test_phase_5_offline_backfill_and_deduplication()
        finally:
            if self.v_gateway:
                self.v_gateway.stop()

        self.print_summary()

    # ── Phase 1: Gateway Claim Handshake ────────────────────────────────────
    def test_phase_1_provisioning_and_claim(self):
        banner("PHASE 1: Gateway Provisioning & Claim Handshake (FSD §8.2)")

        # Step 1: Gateway registers claim code
        url_reg = f"{self.api_base}/api/v1/gateways/register-claim"
        body_reg = {
            "gateway_id": self.gateway_id,
            "mac_address": self.mac_address
        }
        res_reg = requests.post(url_reg, json=body_reg)
        reg_ok = res_reg.status_code == 200 and "claim_code" in res_reg.json()
        claim_code = res_reg.json().get("claim_code") if reg_ok else ""
        self.log_result(
            "PHASE-1.1-REGISTER-CLAIM",
            reg_ok,
            f"Status {res_reg.status_code}, Claim Code: '{claim_code}'"
        )
        if not reg_ok:
            raise RuntimeError(f"Claim registration failed: {res_reg.text}")

        # Step 2: User claims gateway from Web App
        url_claim = f"{self.api_base}/api/v1/gateways/claim"
        body_claim = {
            "claim_code": claim_code,
            "name": "Living Room Clock Gateway"
        }
        res_claim = requests.post(url_claim, json=body_claim)
        claim_ok = res_claim.status_code == 200 and res_claim.json().get("gateway", {}).get("status") == "active"
        self.log_result(
            "PHASE-1.2-USER-CLAIM",
            claim_ok,
            f"Status {res_claim.status_code}, Gateway State: '{res_claim.json().get('gateway', {}).get('status')}'"
        )

        # Step 3: Start Virtual Gateway & Connect MQTT
        self.v_gateway = VirtualESP32Gateway(self.gateway_id, self.mqtt_host, self.mqtt_port)
        self.v_gateway.start()
        time.sleep(1.0)
        self.log_result(
            "PHASE-1.3-GATEWAY-ONLINE",
            self.v_gateway.is_connected,
            f"Gateway {self.gateway_id} connected to MQTT broker"
        )

    # ── Phase 2: Peripherals & Rule Config Sync ─────────────────────────────
    def test_phase_2_peripherals_and_config_sync(self):
        banner("PHASE 2: Peripherals & Rule Configuration Sync (FSD §4.7, §4.8, §7.3, §8.3)")

        # Step 1: Register Sensor Device
        url_sensor = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/devices"
        body_sensor = {
            "device_id": self.sensor_id,
            "name": "Bedroom BLE Thermometer",
            "protocol": "BLE",
            "device_type": "sensor_temp",
            "mac_address": "AA:BB:CC:DD:EE:FF"
        }
        res_sensor = requests.post(url_sensor, json=body_sensor)
        sensor_ok = res_sensor.status_code in (200, 201)
        self.log_result("PHASE-2.1-REG-SENSOR", sensor_ok, f"Sensor '{self.sensor_id}' registered")

        # Step 2: Register Actuator Relay Device
        body_actuator = {
            "device_id": self.actuator_id,
            "name": "Cooling Fan Relay",
            "protocol": "INTERNAL",
            "device_type": "relay",
            "capabilities": ["ON", "OFF"]
        }
        res_actuator = requests.post(url_sensor, json=body_actuator)
        actuator_ok = res_actuator.status_code in (200, 201)
        self.log_result("PHASE-2.2-REG-ACTUATOR", actuator_ok, f"Actuator '{self.actuator_id}' registered")

        # Step 3: Create Automation Rule: IF Temp > 30.0°C THEN relay_1 ON
        url_rule = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/rules"
        body_rule = {
            "sensor_device_id": self.sensor_id,
            "condition_type": "gt",
            "threshold": 30.0,
            "target_device_id": self.actuator_id,
            "action_command": "ON",
            "enabled": True
        }
        res_rule = requests.post(url_rule, json=body_rule)
        rule_ok = res_rule.status_code in (200, 201)
        self.log_result("PHASE-2.3-CREATE-RULE", rule_ok, f"Rule created: IF {self.sensor_id} > 30.0°C -> {self.actuator_id} ON")

        # Step 4: Create Schedule: Daily 07:30 -> relay_1 ON
        url_sched = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/schedules"
        body_sched = {
            "type": "automation",
            "hour": 7,
            "minute": 30,
            "repeat_days": "MON,TUE,WED,THU,FRI",
            "target_device_id": self.actuator_id,
            "action_command": "ON",
            "enabled": True
        }
        res_sched = requests.post(url_sched, json=body_sched)
        sched_ok = res_sched.status_code in (200, 201)
        self.log_result("PHASE-2.4-CREATE-SCHEDULE", sched_ok, "Schedule created: 07:30 Mon-Fri -> relay_1 ON")

        # Step 5: Verify Gateway received retained config push over MQTT
        time.sleep(1.0)
        has_cfg = len(self.v_gateway.received_configs) > 0
        rule_in_cfg = any(
            r.get("sensor_device_id") == self.sensor_id
            for cfg in self.v_gateway.received_configs
            for r in cfg.get("rules", [])
        )
        self.log_result(
            "PHASE-2.5-MQTT-CONFIG-RETAINED",
            has_cfg and rule_in_cfg,
            f"Gateway received config push ({len(self.v_gateway.rules)} rules, {len(self.v_gateway.schedules)} schedules)"
        )

        # Step 6: Verify REST Sync API (Gateway boot pull fallback)
        url_sync = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/sync/schedule"
        res_sync = requests.get(url_sync)
        sync_ok = res_sync.status_code == 200 and len(res_sync.json().get("rules", [])) >= 1
        self.log_result("PHASE-2.6-REST-SYNC-FALLBACK", sync_ok, f"REST sync schedule endpoint verified")

    # ── Phase 3: BLE Telemetry Ingestion & Automation Loop ──────────────────
    def test_phase_3_telemetry_and_automation_rule(self):
        banner("PHASE 3: BLE Telemetry Ingestion & Hysteresis Rule Trigger (FSD §4.1, §4.7, §9.1, §9.2)")

        # Step 1: Synthesize BLE SIG 0x181A packet with Temp = 32.5°C (> 30°C threshold)
        ble_packet = build_sig_0x181a_adv(temperature=32.5, humidity=65.0)
        self.log_result("PHASE-3.1-BLE-SYNTHESIS", len(ble_packet) > 0, "BLE SIG 0x181A advertisement built (32.5°C)")

        # Step 2: Ingest into Virtual Gateway
        self.v_gateway.ingest_ble_advertisement(self.sensor_id, ble_packet)
        time.sleep(1.0)

        # Step 3: Verify Virtual Gateway Rule Engine fired and turned relay_1 ON
        relay_state = self.v_gateway.relay_state.get(self.actuator_id)
        self.log_result(
            "PHASE-3.2-RULE-TRIGGER-LOCAL",
            relay_state is True,
            f"Relay '{self.actuator_id}' switched ON locally by Gateway Rule Engine"
        )

        # Step 4: Verify Backend Database ingested telemetry record via MQTT
        url_telem = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/devices/{self.sensor_id}/telemetry?limit=5"
        res_telem = requests.get(url_telem)
        telem_data = res_telem.json() if res_telem.status_code == 200 else []
        latest_val = telem_data[0].get("value") if len(telem_data) > 0 else None
        telem_ok = (latest_val is not None) and (abs(latest_val - 32.5) < 0.1)
        self.log_result(
            "PHASE-3.3-DB-TELEMETRY-INGESTED",
            telem_ok,
            f"Database telemetry for {self.sensor_id}: latest value = {latest_val}°C"
        )

    # ── Phase 4: Bi-directional Command & Asynchronous ACK ──────────────────
    def test_phase_4_command_downlink_and_ack_uplink(self):
        banner("PHASE 4: Bi-directional Command Dispatch & Asynchronous ACK Uplink (FSD §8.3, §9.1, §9.2)")

        # Step 1: Web App / User sends manual command: relay_1 OFF
        url_cmd = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/devices/{self.actuator_id}/command"
        body_cmd = {
            "command": "OFF",
            "ttl_seconds": 60
        }
        res_cmd = requests.post(url_cmd, json=body_cmd)
        
        cmd_accepted = res_cmd.status_code == 202
        command_id = res_cmd.json().get("command_id") if cmd_accepted else None
        self.log_result(
            "PHASE-4.1-COMMAND-ACCEPTED-202",
            cmd_accepted and (command_id is not None),
            f"REST 202 Accepted returned with command_id: '{command_id}'"
        )

        # Step 2: Verify Gateway executed the command
        time.sleep(1.0)
        relay_state = self.v_gateway.relay_state.get(self.actuator_id)
        self.log_result(
            "PHASE-4.2-GATEWAY-EXECUTED-COMMAND",
            relay_state is False,
            f"Gateway executed command: {self.actuator_id} is now OFF"
        )

        # Step 3: Verify Backend recorded the ACK uplink in EventLog (FSD §9.1)
        url_events = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/events?type=command_ack"
        res_events = requests.get(url_events)
        events_list = res_events.json() if res_events.status_code == 200 else []
        
        ack_found = False
        for ev in events_list:
            payload = ev.get("payload", {})
            if isinstance(payload, str):
                try:
                    payload = json.loads(payload)
                except Exception:
                    payload = {}
            if payload.get("command_id") == command_id:
                ack_found = True
                break

        self.log_result(
            "PHASE-4.3-ACK-RECORDED-IN-DB",
            ack_found,
            f"Command ACK matched and recorded in EventLog for command_id '{command_id}'"
        )

    # ── Phase 5: Store-and-Forward & Batch Backfill Deduplication ────────────
    def test_phase_5_offline_backfill_and_deduplication(self):
        banner("PHASE 5: Store-and-Forward Offline Queue & Deduplicated Backfill (FSD §4.11.3, §9.1)")

        # Step 1: Prepare 5 offline buffered telemetry records
        base_ts = int(time.time()) - 300  # 5 minutes ago
        batch_records = []
        for i in range(5):
            batch_records.append({
                "device_id": self.sensor_id,
                "value": round(24.0 + i * 0.5, 2),
                "unit": "°C",
                "timestamp": base_ts + (i * 30)
            })

        # Step 2: Uplink Batch via MQTT (FSD §9.1 gateway/{id}/telemetry/batch)
        self.v_gateway.publish_batch_backfill(batch_records)
        time.sleep(1.5)

        # Step 3: Uplink the EXACT SAME batch again (Simulating duplicate retransmit)
        self.v_gateway.publish_batch_backfill(batch_records)
        time.sleep(1.5)

        # Step 4: Verify Database Telemetry Count (Must have exactly 5, not 10 due to deduplication)
        url_telem = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/devices/{self.sensor_id}/telemetry?limit=50"
        res_telem = requests.get(url_telem)
        records_in_db = res_telem.json() if res_telem.status_code == 200 else []
        
        # Count timestamps matching the batch
        batch_ts_set = {r["timestamp"] for r in batch_records}
        matched_in_db = [
            r for r in records_in_db
            if int(datetime.fromisoformat(r["recorded_at"].replace("Z", "+00:00")).timestamp()) in batch_ts_set
        ]

        dedup_ok = len(matched_in_db) == len(batch_records)
        self.log_result(
            "PHASE-5.1-DEDUPLICATED-BATCH-BACKFILL",
            dedup_ok,
            f"Backfill batch deduplicated: expected {len(batch_records)} unique records, found {len(matched_in_db)}"
        )

        # Step 5: Test REST Backfill endpoint fallback
        url_backfill = f"{self.api_base}/api/v1/gateways/{self.gateway_id}/telemetry/backfill"
        res_rest_bf = requests.post(url_backfill, json={"records": batch_records})
        rest_bf_ok = res_rest_bf.status_code == 200 and res_rest_bf.json().get("deduplicated_count") == len(batch_records)
        self.log_result(
            "PHASE-5.2-REST-BACKFILL-ENDPOINT",
            rest_bf_ok,
            f"REST backfill API correctly identified {res_rest_bf.json().get('deduplicated_count', 0)} duplicates"
        )

    # ── Summary ─────────────────────────────────────────────────────────────
    def print_summary(self):
        banner("END-TO-END SYSTEM TEST RESULTS SUMMARY")
        total = len(self.test_results)
        passed = sum(1 for _, ok, _ in self.test_results if ok)
        failed = total - passed

        print(f"Total Tests Executed: {total}")
        print(f"Passed:               {COLOR_GREEN}{passed}{COLOR_RESET}")
        print(f"Failed:               {COLOR_RED}{failed}{COLOR_RESET}")
        print("-" * 80)

        for name, ok, detail in self.test_results:
            status = f"{COLOR_GREEN}PASS{COLOR_RESET}" if ok else f"{COLOR_RED}FAIL{COLOR_RESET}"
            print(f"[{status}] {name:<35} : {detail}")

        print("=" * 80)
        if failed == 0:
            print(f"\n{COLOR_BOLD}{COLOR_GREEN}🏆 ALL END-TO-END SYSTEM INTEGRATION TESTS PASSED!{COLOR_RESET}\n")
        else:
            print(f"\n{COLOR_BOLD}{COLOR_RED}⚠ SOME SYSTEM TESTS FAILED. PLEASE REVIEW ABOVE LOGS.{COLOR_RESET}\n")


# ── Main CLI Entrypoint ──────────────────────────────────────────────────────
def main():
    parser = argparse.ArgumentParser(
        description="Comprehensive E2E Integration Test Suite for Smart Clock Gateway"
    )
    parser.add_argument(
        "--api-url",
        default="http://localhost:3000",
        help="Backend REST API base URL (default: http://localhost:3000)"
    )
    parser.add_argument(
        "--mqtt-host",
        default="localhost",
        help="Backend MQTT Broker hostname/IP (default: localhost)"
    )
    parser.add_argument(
        "--mqtt-port",
        type=int,
        default=1883,
        help="Backend MQTT Broker port (default: 1883)"
    )

    args = parser.parse_args()

    suite = E2ESystemTestSuite(
        api_base=args.api_url,
        mqtt_host=args.mqtt_host,
        mqtt_port=args.mqtt_port
    )
    suite.run_all()


if __name__ == "__main__":
    main()
