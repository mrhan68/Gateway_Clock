"""Host-tier verification for Relay Controller safety mechanisms (FSD §4.10 / FR-RELAY-01).

Verification Contract — FR-RELAY-01:
- Preconditions: All 4 relay channels (GPIO 15..18) LOW.
- Stimulus & Assertions:
  1. Interlock Test:
     Turn ON heater_01 (relay_2). Then turn ON fan_01 (relay_1).
     Expected: heater_01 is immediately forced OFF. Relay 1 and 2 never stay ON together.
  2. Emergency Cutoff:
     relay_2 is kept ON for > 30 minutes (1800s).
     Expected: Safety cutoff triggers after 1800s, turning relay_2 OFF with reason "SAFETY_CUTOFF".
  3. Sensor Invalid Fallback:
     When EVT_SENSOR_VALUE_INVALID is received while heater_01 is ON, heater turns OFF immediately.
  4. Anti-Short-Cycle:
     Turning OFF a compressor/heater enforces a cooldown period before it can turn ON again.
"""

from typing import List


class RelayControllerHostSimulator:
    MAX_ON_SECONDS = {
        1: 4 * 3600,   # Fan: 4 hours
        2: 30 * 60,    # Heater: 30 minutes (Emergency cutoff)
        3: 60 * 60,    # Dehumidifier: 60 minutes
        4: 0,          # Aux: unlimited
    }
    COOLDOWN_SECONDS = {
        1: 0,
        2: 0,          # Resistor heater has no 180s dwell
        3: 180,        # Compressor anti-short-cycle 180s for Relay 3 only
        4: 0,
    }

    def __init__(self):
        self.states = {1: False, 2: False, 3: False, 4: False}
        self.on_start_time = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}
        self.off_time = {1: -1000.0, 2: -1000.0, 3: -1000.0, 4: -1000.0}
        self.heater_lockout_until = 0.0
        self.events: List[dict] = []

    def set_state(self, relay_id: int, state: bool, current_time: float, source: str = "CMD") -> bool:
        if not state:
            self.states[relay_id] = False
            self.off_time[relay_id] = current_time
            if relay_id == 2 and source in ("SAFETY_CUTOFF", "SENSOR_OFFLINE"):
                self.heater_lockout_until = current_time + 300.0
            self.events.append({
                "relay_id": relay_id,
                "event": "STATE_CHANGED",
                "state": False,
                "source": source,
                "time": current_time,
            })
            return True

        # Turn ON
        if state == self.states[relay_id]:
            return True

        # Check Compressor Dwell (Relay 3 only)
        cooldown = self.COOLDOWN_SECONDS.get(relay_id, 0)
        if cooldown > 0 and (current_time - self.off_time[relay_id]) < cooldown:
            self.events.append({
                "relay_id": relay_id,
                "event": "REJECTED_COOLDOWN",
                "time": current_time,
            })
            return False

        # Check Heater Lockout (Relay 2 only)
        if relay_id == 2 and current_time < self.heater_lockout_until:
            self.events.append({
                "relay_id": 2,
                "event": "REJECTED_LOCKOUT",
                "time": current_time,
            })
            return False

        # FSD §4.10.3 / FR-RELAY-01: Asymmetric Hardware Interlock
        if relay_id == 2 and self.states[1]:
            # Fan is running -> REJECT Heater ON command! Do NOT stop fan!
            self.events.append({
                "relay_id": 2,
                "event": "REJECTED_INTERLOCK",
                "reason": "FAN_ALREADY_ON",
                "time": current_time,
            })
            return False
        elif relay_id == 1 and self.states[2]:
            # Heater is running -> Cooling Safety Override: Force Heater OFF immediately (0ms delay)
            self.states[2] = False
            self.off_time[2] = current_time
            self.events.append({
                "relay_id": 2,
                "event": "INTERLOCK_CUTOFF",
                "reason": "COOLING_OVERRIDE",
                "time": current_time,
            })

        self.states[relay_id] = True
        self.on_start_time[relay_id] = current_time
        self.events.append({
            "relay_id": relay_id,
            "event": "STATE_CHANGED",
            "state": True,
            "source": source,
            "time": current_time,
        })
        return True

    def check_safety_timers(self, current_time: float):
        """Periodic safety tick (1s)."""
        for r_id in (1, 2, 3):
            if self.states[r_id]:
                limit = self.MAX_ON_SECONDS[r_id]
                duration = current_time - self.on_start_time[r_id]
                if limit > 0 and duration >= limit:
                    self.set_state(r_id, False, current_time, source="SAFETY_CUTOFF")

    def handle_sensor_invalid(self, current_time: float):
        """Safe-default pattern: cut critical heater on sensor invalid."""
        if self.states[2]:
            self.set_state(2, False, current_time, source="SAFETY_CUTOFF")


def test_relay_hardware_interlock_asymmetric():
    sim = RelayControllerHostSimulator()

    # Scenario A: Heater ON -> Fan ON (Cooling Override)
    sim.set_state(relay_id=2, state=True, current_time=100.0)
    assert sim.states[2] is True
    assert sim.states[1] is False

    # Fan turns ON -> Heater cut immediately (Cooling Override)
    ok = sim.set_state(relay_id=1, state=True, current_time=105.0)
    assert ok is True
    assert sim.states[1] is True
    assert sim.states[2] is False, "Cooling Override failure: Heater was not cut!"
    assert not (sim.states[1] and sim.states[2])

    # Scenario B: Fan ON -> Heater ON (Rejected with error)
    # Fan is currently ON. Attempt to turn ON Heater:
    ok_heater = sim.set_state(relay_id=2, state=True, current_time=110.0)
    assert ok_heater is False, "Interlock failure: Heater command was not rejected while Fan was ON!"
    assert sim.states[1] is True, "Fan was wrongly turned OFF when Heater requested!"
    assert sim.states[2] is False, "Heater was turned ON despite interlock conflict!"
    assert not (sim.states[1] and sim.states[2])


def test_relay_emergency_cutoff_30m():
    sim = RelayControllerHostSimulator()

    # Step 1: Turn ON Heater at t = 0s
    sim.set_state(relay_id=2, state=True, current_time=0.0)
    assert sim.states[2] is True

    # Step 2: Simulate 29 minutes (1740s) -> still ON
    sim.check_safety_timers(current_time=1740.0)
    assert sim.states[2] is True

    # Step 3: Simulate 30.5 minutes (1830s) -> Must be cutoff
    sim.check_safety_timers(current_time=1830.0)
    assert sim.states[2] is False, "Safety cutoff failure: Heater remained ON past 30m!"
    cutoff_events = [e for e in sim.events if e["event"] == "STATE_CHANGED" and e["source"] == "SAFETY_CUTOFF" and e["relay_id"] == 2]
    assert len(cutoff_events) >= 1


def test_relay_sensor_invalid_safe_default():
    sim = RelayControllerHostSimulator()

    # Turn ON Heater
    sim.set_state(relay_id=2, state=True, current_time=10.0)
    assert sim.states[2] is True

    # Stimulus: Sensor reading fails (EVT_SENSOR_VALUE_INVALID)
    sim.handle_sensor_invalid(current_time=15.0)
    assert sim.states[2] is False, "Heater failed to shut down on invalid sensor reading!"


def test_relay_compressor_anti_short_cycle():
    sim = RelayControllerHostSimulator()

    # Relay 3 (Dehumidifier compressor)
    # Turn ON compressor at t = 1000s, turn OFF at t = 1010s
    sim.set_state(relay_id=3, state=True, current_time=1000.0)
    sim.set_state(relay_id=3, state=False, current_time=1010.0)

    # Immediately try to turn ON 5s later (cooldown is 180s)
    accepted = sim.set_state(relay_id=3, state=True, current_time=1015.0)
    assert accepted is False
    assert sim.states[3] is False

    # After 185s, try turning ON -> should succeed
    accepted_after = sim.set_state(relay_id=3, state=True, current_time=1195.0)
    assert accepted_after is True
    assert sim.states[3] is True
