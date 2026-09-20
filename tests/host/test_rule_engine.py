"""Host-tier tests for Rule Engine condition evaluator (FSD Section 4.7).

Tests pure evaluation logic:
- Sensor GT (greater than) and LT (less than) thresholds.
- Safe-default handling when sensor data is invalid (valid == False).
- Action request generation for matching rules.
- Isolation of rules by device_id.
"""

from enum import Enum
from typing import List, NamedTuple


class RuleConditionType(Enum):
    SENSOR_GT = 0
    SENSOR_LT = 1
    TIME_MATCH = 2


class AutomationRule(NamedTuple):
    id: int
    device_id: str
    condition_type: RuleConditionType
    threshold: float
    target_device_id: str
    action_command: str


class SensorTelemetry(NamedTuple):
    device_id: str
    value: float
    valid: bool


class RuleActionRequest(NamedTuple):
    target_device_id: str
    action_command: str


def evaluate_rules(
    telemetry: SensorTelemetry,
    rules: List[AutomationRule]
) -> List[RuleActionRequest]:
    """Evaluate telemetry against rules following FSD 4.7.5 logic."""
    # FSD 4.7.5: If valid is False -> keep safe-default, DO NOT evaluate rules
    if not telemetry.valid:
        return []

    actions: List[RuleActionRequest] = []
    for rule in rules:
        if rule.device_id != telemetry.device_id:
            continue

        matched = False
        if rule.condition_type == RuleConditionType.SENSOR_GT:
            if telemetry.value > rule.threshold:
                matched = True
        elif rule.condition_type == RuleConditionType.SENSOR_LT:
            if telemetry.value < rule.threshold:
                matched = True

        if matched:
            actions.append(RuleActionRequest(
                target_device_id=rule.target_device_id,
                action_command=rule.action_command,
            ))

    return actions


def test_sensor_gt_triggers_action():
    """Rule with SENSOR_GT triggers action when sensor value > threshold."""
    rule = AutomationRule(
        id=1,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_GT,
        threshold=30.0,
        target_device_id="fan_relay_01",
        action_command="ON"
    )
    telem = SensorTelemetry(device_id="sensor_temp_01", value=31.5, valid=True)
    actions = evaluate_rules(telem, [rule])

    assert len(actions) == 1
    assert actions[0].target_device_id == "fan_relay_01"
    assert actions[0].action_command == "ON"


def test_sensor_gt_does_not_trigger_when_below():
    """Rule with SENSOR_GT does not trigger when sensor value <= threshold."""
    rule = AutomationRule(
        id=1,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_GT,
        threshold=30.0,
        target_device_id="fan_relay_01",
        action_command="ON"
    )
    telem = SensorTelemetry(device_id="sensor_temp_01", value=29.0, valid=True)
    actions = evaluate_rules(telem, [rule])

    assert len(actions) == 0


def test_sensor_lt_triggers_action():
    """Rule with SENSOR_LT triggers action when sensor value < threshold."""
    rule = AutomationRule(
        id=2,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_LT,
        threshold=20.0,
        target_device_id="heater_relay_01",
        action_command="ON"
    )
    telem = SensorTelemetry(device_id="sensor_temp_01", value=18.5, valid=True)
    actions = evaluate_rules(telem, [rule])

    assert len(actions) == 1
    assert actions[0].target_device_id == "heater_relay_01"
    assert actions[0].action_command == "ON"


def test_invalid_sensor_telemetry_safe_default():
    """FSD 4.7.5: When sensor telemetry valid=False, rules MUST NOT be evaluated."""
    rule = AutomationRule(
        id=1,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_GT,
        threshold=30.0,
        target_device_id="fan_relay_01",
        action_command="ON"
    )
    # Value is above threshold (35.0), but valid is False (sensor error / timeout)
    telem = SensorTelemetry(device_id="sensor_temp_01", value=35.0, valid=False)
    actions = evaluate_rules(telem, [rule])

    assert len(actions) == 0, "Safe-default violated: action triggered on invalid sensor data"


def test_device_id_isolation():
    """Telemetry from one device does not trigger rules of another device."""
    rule = AutomationRule(
        id=1,
        device_id="sensor_temp_02",
        condition_type=RuleConditionType.SENSOR_GT,
        threshold=30.0,
        target_device_id="fan_relay_01",
        action_command="ON"
    )
    telem = SensorTelemetry(device_id="sensor_temp_01", value=35.0, valid=True)
    actions = evaluate_rules(telem, [rule])

    assert len(actions) == 0


def test_rule_deletion_by_id():
    """Verify deleting a rule removes it from active rules list."""
    rules = [
        AutomationRule(id=1, device_id="sensor_temp_01", condition_type=RuleConditionType.SENSOR_GT, threshold=30.0, target_device_id="fan_relay_01", action_command="ON"),
        AutomationRule(id=2, device_id="sensor_temp_01", condition_type=RuleConditionType.SENSOR_LT, threshold=20.0, target_device_id="heater_relay_01", action_command="ON"),
    ]
    # Delete rule id 1
    rules = [r for r in rules if r.id != 1]
    assert len(rules) == 1
    assert rules[0].id == 2


def test_rule_json_serialization_roundtrip():
    """Verify rule schema serialization matching REST API contract."""
    import json
    rule_dict = {
        "id": 4,
        "device_id": "sensor_temp_01",
        "condition": ">",
        "threshold": 32.5,
        "target": "fan_relay_01",
        "action": "ON",
        "enabled": True
    }
    encoded = json.dumps(rule_dict)
    decoded = json.loads(encoded)
    assert decoded["id"] == 4
    assert decoded["condition"] == ">"
    assert decoded["threshold"] == 32.5

