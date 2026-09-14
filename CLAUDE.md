# AICLP — Manual Test Mode

> **AI Closed-Loop Programming, adapted for manual testing.**
>
> *Spec to silicon, hands off — except for the hands you don't have.*

This project uses the **AICLP** (AI Closed-Loop Programming) methodology from the
Embedded-AI-Harness, adapted for projects **without a testbench**. Instead of
automated hardware (Raspberry Pi + HTTP API), **you** (the human operator) handle
flashing and testing. The AI still closes the loop — it writes code, builds it,
gives you exact instructions for what to flash and what to observe, interprets
your results, and iterates until tests pass.

## The Control Loop (Manual Mode)

```
         FSD  ─── the setpoint: what "done" means
          │
          ▼
  ┌─ code → build ──┐        forward path (AI)
  │                 ▼
  │        instructions → YOU (flash, test, observe)
  │                 │
  └── correct ◄─────┘        feedback (you report results)

  the loop exits when the error signal is zero: tests green
```

- **Setpoint**: The FSD — what must be true
- **Plant**: Firmware on the real chip
- **Sensor**: Tests (some automated on host, some you run manually)
- **Error signal**: Failing tests
- **Controller**: The AI, correcting until error = 0

## The Four Phases

| Phase | Skill | You do | The AI does |
|---|---|---|---|
| **0 · Define** | `/define` | Describe the product; answer questions | Writes the FSD — atomic, falsifiable requirements, each with its verification contract |
| **1 · Harness** | `/harness` | Answer two questions (debugging agenda, standard run) | Creates project scaffolding, test plan, CI, firmware hooks |
| **2 · Commission** | `/commission` | Plug in board, power on, read serial output, report back | Guides you through proving the forward path works |
| **3 · Build** | `/build` | Flash what the AI tells you, run tests, report results | Drives the loop: writes tests → code → builds → tells you what to do → iterates |

## How Manual Testing Works

The AI **never touches hardware** in your setup. When it needs a test result, it:

1. **Gives you a precise instruction** — "flash `build/firmware.bin` to your ESP32
   at offset `0x10000`, then open a serial terminal at 115200 baud. You should see
   `PROVISIONING_WAITING` within 5 seconds."
2. **You execute** and report the exact output.
3. **The AI interprets** — pass, fail, or inconclusive — records the result in the
   test plan, and either marks it green or corrects and loops.

### Test Tiers (Adapted)

| Tier | What it is | Who runs it |
|---|---|---|
| **host** | Pure logic — layout tables, decoders, report builders | Automated (CI or `pytest`) |
| **target** | Firmware on the device, tested via serial output | You (flash + observe serial) |
| **manual** | Step-by-step procedures requiring human action | You (follow instructions, report) |

### Capabilities

Capabilities that the testbench would have provided are declared in
`testing/test-plan.yaml`. Each has an `available:` status:

```yaml
capabilities:
  flash-local-usb:
    what:      "Flash firmware to the ESP32 via local USB"
    available: yes       # you can do this
    tier: manual
  serial-read:
    what:      "Read serial output at 115200 baud"
    available: yes       # you can do this
    tier: manual
  wifi-ap-outage:
    what:      "Stop and restart the access point on demand"
    available: no        # cannot be done without automation
    consequence: "fault-injection tests for WiFi recovery are excluded"
  button-press:
    what:      "Press the BOOT button on the device"
    available: yes       # you can do this
    tier: manual
```

A test needing `available: no` is `not done`, not `failed` — the gap is recorded
for the audit.

## Getting Started

1. **Pick a project idea** (or use the example).
2. Run `/grill-me` to flesh out the design, then `/define` to write the FSD.
3. Run `/harness` to set up scaffolding, CI, test plan, and firmware hooks.
4. Run `/commission` to prove your board works (manual mode).
5. Run `/build` to start the closed loop — you flash and test; the AI iterates.

## The Discipline

- **No code without a clause.** Every line of code must trace to an FSD
  requirement. If you ask for something not in the spec, the AI routes it through
  `/define` first.
- **Tests before code.** The executable test is written before or alongside the
  code. Tests for absent features are `xfail`.
- **Per-requirement chain.** One requirement walks the whole chain before the
  next starts: contract → test declared → executable test → code → build → flash
  → verify → record → green.
- **Gates.** Each phase ends at a gate, checked by a fresh subagent that did not
  author the work. A shut gate loops back.
- **Three waves.** Wave 1: standard tests. Wave 2: deviations (locked until
  Wave 1 green). Wave 3: negatives and security (locked until Wave 2 green).
- **The journey.** Journey tests are the gate for every hardware session — while
  they fail, later results are unreadable.

## Skills Reference

| Skill | Command | Purpose |
|---|---|---|
| Define | `/define`, `/define update <path>`, `/define --grill`, `/define --planes` | Write and evolve the FSD |
| Grill-Me | `/grill-me` | Interview to resolve design decisions |
| Harness | `/harness` | One-time project setup |
| Commission | `/commission` | Prove the board works (manual) |
| Build | `/build` | Drive the closed loop |

---

*This is a living document. Edit it as your workflow evolves.*
