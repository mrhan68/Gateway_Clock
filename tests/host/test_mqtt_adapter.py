"""
Test TC-HOST-04: MQTT Topic Routing & Telemetry Packaging
Traces to: FSD §4.6 (MQTT Adapter), Requirement [M03]
"""
import json

class MqttTopicManager:
    def __init__(self, gateway_id: str):
        self.gateway_id = gateway_id

    def get_status_topic(self) -> str:
        return f"gateway/{self.gateway_id}/status"

    def get_device_telemetry_topic(self, device_id: str) -> str:
        return f"gateway/{self.gateway_id}/device/{device_id}/telemetry"

    def get_command_subscription_topic(self) -> str:
        return f"gateway/{self.gateway_id}/device/+/command"

    def get_schedule_subscription_topic(self) -> str:
        return f"gateway/{self.gateway_id}/config/schedule"

    def parse_command_topic(self, topic: str):
        # Format: gateway/{gateway_id}/device/{device_id}/command
        parts = topic.split('/')
        if len(parts) == 5 and parts[0] == "gateway" and parts[1] == self.gateway_id and parts[2] == "device" and parts[4] == "command":
            return parts[3] # device_id
        return None

    def package_status_payload(self, is_online: bool, uptime_s: int, free_heap: int) -> str:
        if not is_online:
            return "offline"
        return json.dumps({
            "status": "online",
            "uptime_s": uptime_s,
            "free_heap": free_heap
        })

    def parse_command_payload(self, payload: str):
        try:
            data = json.loads(payload)
            if "command" in data:
                return {
                    "valid": True,
                    "command": str(data["command"]),
                    "value": data.get("value", 0.0)
                }
        except Exception:
            pass
        return {"valid": False, "command": "", "value": 0.0}


def test_mqtt_topic_generation():
    mgr = MqttTopicManager("gw_alpha_01")
    assert mgr.get_status_topic() == "gateway/gw_alpha_01/status"
    assert mgr.get_device_telemetry_topic("sensor_temp_01") == "gateway/gw_alpha_01/device/sensor_temp_01/telemetry"
    assert mgr.get_command_subscription_topic() == "gateway/gw_alpha_01/device/+/command"
    assert mgr.get_schedule_subscription_topic() == "gateway/gw_alpha_01/config/schedule"


def test_mqtt_command_topic_parsing():
    mgr = MqttTopicManager("gw_alpha_01")
    
    # Valid topic
    device_id = mgr.parse_command_topic("gateway/gw_alpha_01/device/relay_ch1/command")
    assert device_id == "relay_ch1"

    # Mismatched gateway ID
    assert mgr.parse_command_topic("gateway/other_gw/device/relay_ch1/command") is None

    # Invalid structure
    assert mgr.parse_command_topic("gateway/gw_alpha_01/status") is None
    assert mgr.parse_command_topic("some/random/topic") is None


def test_mqtt_status_and_command_payload():
    mgr = MqttTopicManager("gw_alpha_01")
    
    # LWT offline
    assert mgr.package_status_payload(False, 0, 0) == "offline"

    # Online JSON
    online_str = mgr.package_status_payload(True, 120, 150000)
    data = json.loads(online_str)
    assert data["status"] == "online"
    assert data["uptime_s"] == 120
    assert data["free_heap"] == 150000

    # Command parse valid
    cmd = mgr.parse_command_payload('{"command": "RELAY_ON", "value": 1.0}')
    assert cmd["valid"] is True
    assert cmd["command"] == "RELAY_ON"
    assert cmd["value"] == 1.0

    # Command parse invalid
    bad_cmd = mgr.parse_command_payload('invalid json')
    assert bad_cmd["valid"] is False
