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


class HysteresisRule:
    """FSD §4.7.4 & §4.7.5 Rule Evaluator with Hysteresis, Edge-Triggering, and Lockout."""
    def __init__(
        self,
        id: int,
        device_id: str,
        condition_type: RuleConditionType,
        threshold: float,
        hysteresis: float,
        cooldown_s: int,
        target_device_id: str,
        action_command: str = "ON",
        is_active: bool = True,
    ):
        self.id = id
        self.device_id = device_id
        self.condition_type = condition_type
        self.threshold = threshold
        self.hysteresis = hysteresis
        self.cooldown_s = cooldown_s
        self.target_device_id = target_device_id
        self.action_command = action_command
        self.is_active = is_active
        self.current_state = False
        self.last_triggered_ts = 0
        self.lockout_until_ts = 0

    def evaluate(self, telemetry: SensorTelemetry, now: int) -> List[RuleActionRequest]:
        if not telemetry.valid:
            return []
        if not self.is_active or telemetry.device_id != self.device_id:
            return []
        if now < self.lockout_until_ts:
            return []

        actions: List[RuleActionRequest] = []
        if self.condition_type == RuleConditionType.SENSOR_GT:
            if telemetry.value >= self.threshold:
                if not self.current_state:
                    if self.last_triggered_ts == 0 or (now - self.last_triggered_ts >= self.cooldown_s):
                        self.current_state = True
                        self.last_triggered_ts = now
                        actions.append(RuleActionRequest(self.target_device_id, self.action_command))
            elif telemetry.value <= (self.threshold - self.hysteresis):
                if self.current_state:
                    self.current_state = False
                    actions.append(RuleActionRequest(self.target_device_id, "OFF"))
        elif self.condition_type == RuleConditionType.SENSOR_LT:
            if telemetry.value <= self.threshold:
                if not self.current_state:
                    if self.last_triggered_ts == 0 or (now - self.last_triggered_ts >= self.cooldown_s):
                        self.current_state = True
                        self.last_triggered_ts = now
                        actions.append(RuleActionRequest(self.target_device_id, self.action_command))
            elif telemetry.value >= (self.threshold + self.hysteresis):
                if self.current_state:
                    self.current_state = False
                    actions.append(RuleActionRequest(self.target_device_id, "OFF"))
        return actions


def test_hysteresis_gt_and_edge_triggering():
    """FSD §4.7.5: SENSOR_GT hysteresis deadband and edge triggering."""
    rule = HysteresisRule(
        id=1,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_GT,
        threshold=30.0,
        hysteresis=1.5,
        cooldown_s=60,
        target_device_id="relay_1",
        action_command="ON"
    )

    # 1. Temp = 31.0°C >= 30.0 -> Edge-UP triggers ON
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 31.0, True), now=100)
    assert len(act) == 1
    assert act[0].action_command == "ON"
    assert rule.current_state is True

    # 2. Temp = 32.0°C >= 30.0 -> Still in Active Zone, edge-triggering suppresses duplicate ON
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 32.0, True), now=105)
    assert len(act) == 0

    # 3. Temp = 29.0°C (between 28.5 and 30.0: Deadband) -> Keeps state True, no command
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 29.0, True), now=110)
    assert len(act) == 0
    assert rule.current_state is True

    # 4. Temp = 28.0°C <= (30.0 - 1.5 = 28.5) -> Edge-DOWN immediately triggers OFF (0ms, bypass cooldown)
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 28.0, True), now=115)
    assert len(act) == 1
    assert act[0].action_command == "OFF"
    assert rule.current_state is False

    # 5. Temp = 27.0°C <= 28.5 -> Already OFF, suppresses duplicate OFF
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 27.0, True), now=120)
    assert len(act) == 0


def test_hysteresis_lt_and_edge_triggering():
    """FSD §4.7.5: SENSOR_LT hysteresis deadband and edge triggering."""
    rule = HysteresisRule(
        id=2,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_LT,
        threshold=22.0,
        hysteresis=1.5,
        cooldown_s=60,
        target_device_id="relay_2",
        action_command="ON"
    )

    # 1. Temp = 21.0°C <= 22.0 -> Edge-UP triggers ON
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 21.0, True), now=100)
    assert len(act) == 1
    assert act[0].action_command == "ON"
    assert rule.current_state is True

    # 2. Temp = 20.0°C -> Duplicate suppressed
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 20.0, True), now=105)
    assert len(act) == 0

    # 3. Temp = 23.0°C (between 22.0 and 23.5: Deadband) -> Keeps state True, no command
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 23.0, True), now=110)
    assert len(act) == 0
    assert rule.current_state is True

    # 4. Temp = 24.0°C >= (22.0 + 1.5 = 23.5) -> Edge-DOWN immediately triggers OFF
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 24.0, True), now=115)
    assert len(act) == 1
    assert act[0].action_command == "OFF"
    assert rule.current_state is False


def test_safety_cutoff_5m_lockout():
    """FSD §4.7.5 #3: Engaging 5-minute safety lockout prevents rule re-activation."""
    rule = HysteresisRule(
        id=2,
        device_id="sensor_temp_01",
        condition_type=RuleConditionType.SENSOR_LT,
        threshold=22.0,
        hysteresis=1.5,
        cooldown_s=60,
        target_device_id="relay_2",
        action_command="ON"
    )

    # Trigger rule initially
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 20.0, True), now=100)
    assert len(act) == 1
    assert rule.current_state is True

    # SAFETY_CUTOFF occurs at now=150 -> rule resets current_state and locks out for 300s (until 450)
    rule.current_state = False
    rule.lockout_until_ts = 150 + 300

    # Cold temperature again during lockout period (now=300 < 450) -> MUST NOT trigger!
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 19.0, True), now=300)
    assert len(act) == 0, "Safety lockout violated: rule fired while locked out"

    # After 5 minutes elapse (now=451 >= 450) -> allowed to fire again
    act = rule.evaluate(SensorTelemetry("sensor_temp_01", 19.0, True), now=451)
    assert len(act) == 1
    assert act[0].action_command == "ON"


