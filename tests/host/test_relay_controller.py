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
        1: 10,
        2: 180,        # Anti-short-cycle 180s for Heater
        3: 180,        # Anti-short-cycle 180s for Dehumidifier
        4: 0,
    }

    def __init__(self):
        self.states = {1: False, 2: False, 3: False, 4: False}
        self.on_start_time = {1: 0.0, 2: 0.0, 3: 0.0, 4: 0.0}
        self.off_time = {1: -1000.0, 2: -1000.0, 3: -1000.0, 4: -1000.0}
        self.events: List[dict] = []

    def set_state(self, relay_id: int, state: bool, current_time: float, source: str = "CMD") -> bool:
        if state == self.states[relay_id]:
            return True

        if state:
            # Check Anti-Short-Cycle
            cooldown = self.COOLDOWN_SECONDS.get(relay_id, 0)
            if (current_time - self.off_time[relay_id]) < cooldown:
                self.events.append({
                    "relay_id": relay_id,
                    "event": "REJECTED_COOLDOWN",
                    "time": current_time,
                })
                return False

            # FSD §4.10.3: Hardware Interlock (Fan vs Heater)
            if relay_id == 1 and self.states[2]:
                # Forcibly cut heater immediately
                self.states[2] = False
                self.off_time[2] = current_time
                self.events.append({
                    "relay_id": 2,
                    "event": "INTERLOCK_CUTOFF",
                    "reason": "FAN_ACTIVATED",
                    "time": current_time,
                })
            elif relay_id == 2 and self.states[1]:
                # Forcibly cut fan immediately
                self.states[1] = False
                self.off_time[1] = current_time
                self.events.append({
                    "relay_id": 1,
                    "event": "INTERLOCK_CUTOFF",
                    "reason": "HEATER_ACTIVATED",
                    "time": current_time,
                })

            self.states[relay_id] = True
            self.on_start_time[relay_id] = current_time
        else:
            self.states[relay_id] = False
            self.off_time[relay_id] = current_time

        self.events.append({
            "relay_id": relay_id,
            "event": "STATE_CHANGED",
            "state": self.states[relay_id],
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
                    self.states[r_id] = False
                    self.off_time[r_id] = current_time
                    self.events.append({
                        "relay_id": r_id,
                        "event": "SAFETY_CUTOFF",
                        "duration": duration,
                        "time": current_time,
                    })

    def handle_sensor_invalid(self, current_time: float):
        """Safe-default pattern: cut critical heater on sensor invalid."""
        if self.states[2]:
            self.states[2] = False
            self.off_time[2] = current_time
            self.events.append({
                "relay_id": 2,
                "event": "SAFETY_CUTOFF",
                "reason": "SENSOR_VALUE_INVALID",
                "time": current_time,
            })


def test_relay_hardware_interlock():
    sim = RelayControllerHostSimulator()

    # Step 1: Turn ON Heater (Relay 2)
    sim.set_state(relay_id=2, state=True, current_time=100.0)
    assert sim.states[2] is True
    assert sim.states[1] is False

    # Step 2: Turn ON Fan (Relay 1) -> Must force Relay 2 OFF immediately
    sim.set_state(relay_id=1, state=True, current_time=105.0)
    assert sim.states[1] is True
    assert sim.states[2] is False, "Interlock failure: Heater was not cut when Fan turned ON!"

    # Ensure both are never concurrently ON
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
    cutoff_events = [e for e in sim.events if e["event"] == "SAFETY_CUTOFF" and e["relay_id"] == 2]
    assert len(cutoff_events) >= 1


def test_relay_sensor_invalid_safe_default():
    sim = RelayControllerHostSimulator()

    # Turn ON Heater
    sim.set_state(relay_id=2, state=True, current_time=10.0)
    assert sim.states[2] is True

    # Stimulus: Sensor reading fails (EVT_SENSOR_VALUE_INVALID)
    sim.handle_sensor_invalid(current_time=15.0)
    assert sim.states[2] is False, "Heater failed to shut down on invalid sensor reading!"


def test_relay_anti_short_cycle():
    sim = RelayControllerHostSimulator()

    # Turn ON heater at t = 1000s, turn OFF at t = 1010s
    sim.set_state(relay_id=2, state=True, current_time=1000.0)
    sim.set_state(relay_id=2, state=False, current_time=1010.0)

    # Immediately try to turn ON 5s later (cooldown is 180s)
    accepted = sim.set_state(relay_id=2, state=True, current_time=1015.0)
    assert accepted is False
    assert sim.states[2] is False

    # After 185s, try turning ON -> should succeed
    accepted_after = sim.set_state(relay_id=2, state=True, current_time=1195.0)
    assert accepted_after is True
    assert sim.states[2] is True
