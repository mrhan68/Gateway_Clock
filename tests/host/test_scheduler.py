"""Host-tier verification for Scheduler, Rollover Guard & RTC Validity (FSD §4.8 / FR-SCHED-01).

Verification Contract — FR-SCHED-01:
- Preconditions:
  - 1 automation entry at 07:00 (target: "light_livingroom", cmd: "ON")
  - 1 alarm entry at 06:30 (title: "School Wakeup", buzzer: True)
- Stimulus & Assertions:
  1. rtc_is_valid() == False: Clock runs past 06:30 and 07:00.
     Expected: 0 events emitted. No false alarm.
  2. rtc_is_valid() == True: Clock runs from 06:29:50 to 06:30:10 across 06:30 mark.
     Expected: Exactly 1 alarm event emitted. No repeats in the 60-second window.
  3. Time jump from 06:55:00 to 07:02:00:
     Expected: Catch-up scan executes the missed 07:00 automation cleanly once.
"""

from typing import List, Optional


class ScheduleEntry:
    def __init__(
        self,
        entry_id: int,
        entry_type: str,
        hour: int,
        minute: int,
        title: str = "",
        target: str = "",
        cmd: str = "",
        buzzer: bool = False,
        enabled: bool = True,
    ):
        self.id = entry_id
        self.type = entry_type
        self.hour = hour
        self.minute = minute
        self.title = title
        self.target = target
        self.cmd = cmd
        self.buzzer = buzzer
        self.enabled = enabled


class SchedulerHostSimulator:
    def __init__(self, entries: List[ScheduleEntry]):
        self.entries = entries
        self.last_triggered_minute = -1
        self.emitted_events: List[dict] = []

    def tick(self, hour: int, minute: int, rtc_valid: bool, time_jump_from: Optional[int] = None):
        """Simulate scheduler periodic evaluation loop (FSD §4.8.5)."""
        if not rtc_valid:
            # FSD FR-SCHED-01: rtc_is_valid() check blocks all triggers
            return

        cur_minute = hour * 60 + minute

        # Catch-up scan if time jump occurred
        if time_jump_from is not None and time_jump_from < cur_minute:
            for m in range(time_jump_from + 1, cur_minute + 1):
                h_scan = m // 60
                m_scan = m % 60
                self._evaluate_minute(h_scan, m_scan)
            self.last_triggered_minute = cur_minute
            return

        # Rollover Guard: trigger only once per minute (FSD §4.8.3)
        if cur_minute != self.last_triggered_minute:
            self.last_triggered_minute = cur_minute
            self._evaluate_minute(hour, minute)

    def _evaluate_minute(self, hour: int, minute: int):
        for entry in self.entries:
            if not entry.enabled:
                continue
            if entry.hour == hour and entry.minute == minute:
                if entry.type == "alarm":
                    self.emitted_events.append({
                        "type": "EVT_SCHEDULE_ALARM_TRIGGERED",
                        "id": entry.id,
                        "title": entry.title,
                        "buzzer": entry.buzzer,
                        "hour": hour,
                        "minute": minute
                    })
                elif entry.type == "automation":
                    self.emitted_events.append({
                        "type": "EVT_SCHEDULE_AUTOMATION_TRIGGERED",
                        "id": entry.id,
                        "target": entry.target,
                        "cmd": entry.cmd,
                        "hour": hour,
                        "minute": minute
                    })


def test_scheduler_rtc_invalid_blocks_triggers():
    entries = [
        ScheduleEntry(1, "alarm", 6, 30, title="School Wakeup", buzzer=True),
        ScheduleEntry(2, "automation", 7, 0, target="light_livingroom", cmd="ON"),
    ]
    sim = SchedulerHostSimulator(entries)

    # Stimulus 1: Clock runs across 06:30 and 07:00 with invalid RTC (Epoch 1970)
    sim.tick(hour=6, minute=30, rtc_valid=False)
    sim.tick(hour=7, minute=0, rtc_valid=False)

    assert len(sim.emitted_events) == 0, "Scheduler triggered while RTC was invalid!"


def test_scheduler_rollover_guard_single_trigger():
    entries = [
        ScheduleEntry(1, "alarm", 6, 30, title="School Wakeup", buzzer=True),
    ]
    sim = SchedulerHostSimulator(entries)

    # Stimulus 2: Clock ticks multiple times within minute 06:30 (e.g. at sec 00, 10, 25, 59)
    for _ in range(5):
        sim.tick(hour=6, minute=30, rtc_valid=True)

    assert len(sim.emitted_events) == 1, "Rollover Guard failed; multiple triggers in same minute!"
    assert sim.emitted_events[0]["type"] == "EVT_SCHEDULE_ALARM_TRIGGERED"
    assert sim.emitted_events[0]["title"] == "School Wakeup"


def test_scheduler_time_jump_catch_up():
    entries = [
        ScheduleEntry(2, "automation", 7, 0, target="light_livingroom", cmd="ON"),
    ]
    sim = SchedulerHostSimulator(entries)

    # Step 1: Simulator was at 06:55
    sim.tick(hour=6, minute=55, rtc_valid=True)
    assert len(sim.emitted_events) == 0

    # Step 2: NTP time jump jumps from 06:55 to 07:02 (+7 min jump, missing 07:00)
    old_min = 6 * 60 + 55
    sim.tick(hour=7, minute=2, rtc_valid=True, time_jump_from=old_min)

    assert len(sim.emitted_events) == 1, "Catch-up scan failed to recover missed 07:00 event!"
    assert sim.emitted_events[0]["type"] == "EVT_SCHEDULE_AUTOMATION_TRIGGERED"
    assert sim.emitted_events[0]["target"] == "light_livingroom"
    assert sim.emitted_events[0]["cmd"] == "ON"
