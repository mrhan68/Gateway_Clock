# Test Design (Manual Testing Edition)

This skill designs tests. It does **not** implement them, run them, or operate
hardware — it designs and documents them. Execution is delegated to the operator.

## 1. Clause inventory

Walk every FSD chapter. Extract **atomic clauses** — one per decision point or
promise. Assign stable IDs: `FSD-<section>-<short-slug>`.

Save to `testing/clauses.yaml`.

## 2. Verification tiers (manual mode)

| Tier | What it tests | Who runs it | Cost |
|---|---|---|---|
| **host** | Pure logic (decoders, tables, policies) | Automated (pytest / CI) | milliseconds |
| **target** | Firmware on device, observed via serial output | You (flash + observe serial) | minutes |
| **manual** | Physical actions: button presses, cable swaps, real radio peers | You (follow instructions, report) | minutes |

**Place each behaviour at the lowest tier where the defect can manifest.**

## 3. Controllability

| Option | Meaning |
|---|---|
| **Drive** | Command the real peer through its normal interface |
| **Feed** | Supply test data through a test seam (bad input, synthetic frame) |
| **Emulate** | Substitute a fake device outside the system under test |
| **Observe** | Watch-only validation, no provocation |
| **Rig** | The test infrastructure creates the condition |

For the manual tier, "rig" = you perform a physical action (press button, unplug
cable, power-cycle from wall).

## 4. Observability

Name how each behaviour will be observed: returned value · state query · log ·
serial output · network message · GPIO level · timing measurement · button
response · BLE advertisement.

**A log line alone is not accepted when stronger evidence is available.** But in
manual mode, the serial log is often the primary evidence — design the firmware to
emit structured, unambiguous lines.

## 5. What to build, and in what order

### First: two questions for the operator

**First: what has to be debugged before anything is tested?**

Ask which project-side parts are not yet known to work at all — the board, its
wiring, its peers. A new board's wiring. A protocol nobody has spoken to the real
peer. Produce a short agenda:

```text
part                    unproven because            proven by
board USB               never flashed on this unit  serial output shows boot marker
sensor wiring           polarity unknown             known pattern survives inversion
```

**Second: what does one ordinary successful run look like, end to end?**

Ordered steps with an observable at each:

```text
1  power on               serial shows "BOOT: starting"
2  WiFi connects           serial shows "WIFI: connected"
3  MQTT connects           serial shows "MQTT: connected"
4  sensor reads            serial shows "SENSOR: value=42"
5  value published         MQTT topic receives the reading
```

Then write **one test per step**, each asserting its own checkpoint.

### The four kinds, gated in three waves

| Kind | Is |
|---|---|
| **standard** | The ordinary case — the product doing its job |
| **deviation** | Still normal, just not the simplest case |
| **negative** | A fault or malfunction is injected |
| **security** | Derived from the threat profile, not from a feature |

| Wave | Writes | Gate to enter |
|---|---|---|
| **1 · Build the running prototype** | `standard` only | DUT ready |
| **2 · Cover the normal variants** | `deviation` | Journey green + all standard green |
| **3 · Build the error handling** | `negative` + `security` | All standard + deviation green |

### Per clause

Derive per clause every class that applies: positive · negative · boundary ·
error-handling · state-transition · persistence · recovery · timeout ·
concurrency · security · performance.

## 5a. Two shapes of test

| | **Atomic test** | **Workflow test** |
|---|---|---|
| Verifies | one requirement, one behaviour | a sequence that only means something in order |
| Shape | precondition → stimulus → observation | ordered steps, an observable at **each** |
| Order | independent; any order | fixed, order is part of the claim |
| Failure | tells you *what* is broken | tells you *where* the journey stops |

### Numbering

```text
JRN-<nn>[.<step>]        workflow — the journey, in order
TC-<area>-<nn>           atomic — one requirement's behaviour
```

Each plan entry carries two links:
- **`verifies:`** — the requirement id(s) this test discharges
- **`impl:`** — where the executable lives

### Operator instructions (mandatory for target/manual tier)

Every test at the target or manual tier must include **operator instructions** in
the plan entry. These are numbered, step-by-step, with exact commands and expected
output. Example:

```yaml
- id: TC-WIFI-01
  name: "Device connects to the test AP"
  tier: target
  kind: standard
  verifies: [FR-7.1]
  impl: "tests/host/test_wifi_logic.py::test_config_builds"
  preconditions:
    - "Board flashed with firmware.bin"
    - "Serial terminal open at 115200 baud"
  steps:
    1: "Power-cycle the device (unplug USB, wait 2s, plug back in)"
    2: "Wait for boot banner"
    3: "Wait up to 30 seconds for connection message"
  expected:
    - "Serial shows: WIFI: connected to TestNetwork"
    - "Serial shows: IP: 192.168.4.x"
  prohibited:
    - "WIFI: failed" or "WIFI: timeout"
  pass_criteria:
    - "You observed 'WIFI: connected' within 30 seconds"
    - "You observed an IP address assigned"
  operator_report_fields:
    - "exact serial output lines 1-10 after boot"
    - "did you see 'WIFI: connected'? (yes/no)"
    - "if no, what did you see instead?"
    - "time from boot to connection (seconds)"
```

## 6. Independence, ordering, cleanup

Every test is independently executable and restores the system to a known state.

For target/manual tests, the AI must ensure each test starts from a known state:
- Power-cycle the device before tests that assume a clean boot
- Clear serial buffer before reading
- Reset test credentials (random SSID/password per run when possible)

## 7. Test data and secrets

Generate distinct test values — choose values that cannot occur by chance, so a
match is unambiguous.

**Secrets never appear in test specifications.** Only references:

```yaml
wifi_password: env:MY_TEST_WIFI_PASSWORD
```

## 8. Test seams

Name the seam on the test that needs it. A seam is something the firmware exposes
to make testing possible — a console command, a debug log, a configuration hook.
The firmware should emit structured logs with a known prefix so the operator can
grep for them.
