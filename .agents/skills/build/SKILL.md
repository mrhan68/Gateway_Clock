---
name: build
description: >
  Phase 3 of AI Closed-Loop Programming — the Build phase, and the driver of the
  whole loop. Adapted for manual testing: the AI writes tests with operator
  instructions, builds firmware, and tells the operator exactly what to flash and
  what to observe. The operator executes and reports; the AI interprets and
  iterates until tests pass.
---

# Build — the loop's driver (Manual Testing Edition)

The loop feeds back reality — you flash the code to real hardware, run the tests,
and report results. The AI reads your report, corrects, and loops until tests are
green.

| Control loop | Manual mode |
|---|---|
| Setpoint | The FSD — what must be true |
| Plant | Firmware on the real chip |
| Sensor | Tests — host (automated), target/manual (you observe) |
| Error signal | Failing tests + your observations |
| Controller | The AI, correcting until error = 0 |

**Run me with `/build`.** I open every session by saying where the project is.

## 0. Opening the session — reading the position

I read `testing/test-plan.yaml` and the FSD, derive the position, and open with
it. If I ask you "what do I do next?" — I failed to lead.

The position tells you the wave, the gate, and the blocking chain:

> *"Wave 1 — 3/5 standard green, JRN-01 red (WiFi won't connect); deviations locked.
> I need you to flash the new build to your ESP32 and run JRN-01. See below."*

## 1. The chain — order is not negotiable

```
contract (in FSD, by /define)  →  test declared in plan  →
executable test (xfail)        →  code  →  build  →
[YOU: flash + test]            →  record result + commit  →  green
```

Per requirement, never as project phases. One requirement walks the whole chain
before the next starts.

### Your role in the chain

When the chain reaches the **flash + verify** step, I (the AI) do three things:

1. **Build the artifact** — via CI or local toolchain. I download the binary.
2. **Give you a precise instruction** — the exact flash command, what to open,
   what to observe, and what a pass looks like.
3. **Interpret your report** — pass, fail, or inconclusive. If fail, I correct
   and loop.

Example of what I give you:

```
### JRN-01 — Device boots and connects to WiFi

**Flash this binary:**
  idf.py -p /dev/ttyUSB0 flash  (or use esptool with offsets from flash_args)

**Open serial:**
  idf.py -p /dev/ttyUSB0 monitor  (115200 baud)

**Power-cycle the device** (unplug USB, wait 2s, plug back in).

**Report back exactly:**
  - The first 5 lines of serial output after boot
  - Whether you see "WIFI: connected" or "WIFI: failed"
  - How many seconds it took from boot to the connection message
```

You report back. I record the result in `testing/test-plan.yaml` and interpret it.

### The three test tiers

| Tier | Runs how | Cost | Wave |
|---|---|---|---|
| **host** | Automated — pytest / CI | milliseconds | All waves |
| **target** | You flash firmware, observe serial output | minutes | Waves 1, 2 |
| **manual** | You perform physical actions (button press, cable swap) | minutes | Waves 1, 2, 3 as needed |

**Host tests run first, always.** Before any hardware session, I run `pytest
tests/host/` — a red host test is a reason not to start. Host tests are cheap, so
no selection among them.

## 2. The three waves

| Wave | Writes | Open while | Locked behind it |
|---|---|---|---|
| **1 · Build the running prototype** | `standard` tests | any standard test red or unwritten | deviations, negatives |
| **2 · Cover the normal variants** | `deviation` tests | journey + all standard green | negatives |
| **3 · Build the error handling** | `negative` + `security` | all standard + deviation green | — |

I derive the wave from the plan — never declare it. The position report names the
wave and what the gate is waiting on.

### What the next act is (per requirement)

| State | Next act |
|---|---|
| requirement without contract | **contract step**: send it up to `/define` — the verification contract is the quality gate |
| contract, no declared test | **declaration step**: declare it in the plan; report equipment that does not exist |
| declared, not executable | **executable step**: write the test (xfail/WILL_FAIL while feature is absent) |
| executable, feature absent | **code step**: dispatch to firmware development; the XPASS is the landing signal |
| code exists, no test | **retrofit** (`references/test-lifecycle.md` §3) — both mandatory |

## 3. Dispatch — the loop's muscles (adapted)

I design, declare, sequence, and interpret. I do not compile, flash, or drive
instruments directly — I dispatch:

| Act | Manual-mode dispatch |
|---|---|
| Amend the WHAT (new/changed requirement, contract fix) | `/define` update mode |
| Code · build | `esp-idf-handling` / local toolchain |
| **Flash** | **Me → you: instructions; you execute and report** |
| **Verify:** serial output, WiFi, BLE, GPIO, button presses | **Me → you: test steps; you observe and report** |
| Run the journey / gate suite | **Me → you: instructions; you report results** |
| CI, release workflow | `setup-action` |

**The host tier** runs fully automated (pytest in CI or locally).
**The target + manual tiers** require you.

## 4. Test design — what I design

See `references/test-design.md`: the two opening questions (debugging agenda,
standard run), the four kinds (standard/deviation/negative/security), atomic vs
workflow tests, numbering (`JRN-nn`, `TC-AREA-NN`), seams, controllability.

Every test at the target or manual tier includes **operator instructions** in the
plan entry — numbered steps, exact commands, expected output patterns. This is
how the test stays reproducible: a declared test with no operator instructions
is a gap.

## 5. The operator instruction protocol

This is the heart of manual-mode `/build`. When I need hardware observation, I
always give you:

1. **What to flash** — the binary path and exact offsets (from `flash_args` or
   stated explicitly).
2. **How to observe** — serial port, baud rate, what tool to use.
3. **The procedure** — numbered steps (power-cycle, press button, etc.).
4. **What a pass looks like** — exact expected output or behavior.
5. **What a fail looks like** — what output would indicate a bug.
6. **What to report back** — the specific fields I need.

**Always ask for exact evidence:**

```
Report back:
  - First 5 lines of serial output after boot:
  - Did you see "PROVISIONING_WAITING" within 5 seconds? (yes/no)
  - Did you see "WIFI: connected" within 30 seconds? (yes/no)
  - If no to either, what was the actual output?
```

**Never accept "it works" — get the evidence.** A single line of output is evidence;
a summary is not.

## 6. Interpreting results

From your report, I classify each test as:

- **successful** — the exact expected observation was present
- **failed** — the expected observation was absent, or a prohibited outcome occurred
- **not done** — precondition unmet (board not flashed, serial not captured, etc.)

I never predict: a test I'm confident is about to fail still reads `not done`
until you run it.

## 7. Audit — "are we actually done?"

I report the lifecycle state of every requirement and the gaps by categories
(`references/test-lifecycle.md` §2): specification defects, verification gaps,
implementation gaps, execution/evidence gaps. Each names who resolves it.

## 8. Reconcile

- Declared in the plan with empty `impl:` → the backlog
- Implemented, absent from the plan → undocumented behavior (propose a clause
  upward, or delete the code)

## 9. Shipment

- Dev builds identified by `git describe`; no tags during Build.
- Release: you tag when I announce **Ready for shipment**.
- You flash the released artifact and run the journey once more. I record your
  results. **Shipped = you've confirmed green on the released bytes.**

## Reference index

| File | Covers |
|---|---|
| `references/test-design.md` | Two opening questions, four kinds, waves, atomic vs workflow, numbering |
| `references/test-lifecycle.md` | Test status, computed blocking, gaps, the code-first retrofit |
| `references/test-architecture.md` | Layering, tier taxonomy |
| `references/gate-checks.md` | Fresh-checker protocol, what each gate demands |
