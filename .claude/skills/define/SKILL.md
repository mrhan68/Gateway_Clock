---
name: define
description: >
  Phase 0 of AI Closed-Loop Programming — Definition: engineers the WHAT the
  loop converges on. Writes and evolves the FSD — atomic, falsifiable,
  provenance-tagged requirements each carrying its verification contract. Adapted
  for manual testing: verification contracts that require hardware observation
  include a manual tier with step-by-step instructions for the operator.
---

# Define — Phase 0 (FSD skill)

Creates and maintains the **WHAT**: what the system must do, stated so that
compliance can be demonstrated. Every approved Must/Should carries a
**verification contract** — the requirement and its proof are co-engineered,
because attempting the contract *is* the quality gate: a requirement whose
stimulus cannot be stated is not finished.

## Modes

| Mode | Invoke | Use when |
|------|--------|----------|
| **create** | `/define` + description | No FSD yet. Harvest first (§4), then ask only what is still open. |
| **update** | `/define update <path>` + delta | An FSD exists. Apply the delta surgically. |
| **grill** | `/define --grill` + description | Cold start — brief too thin to infer architecture from. |
| **planes** | `/define --planes [path]` | Write the Method and UserDocumentation planes. |

## The verification contract (adapted for manual testing)

Each Must/Should carries a verification contract **beside the requirement in the
FSD**. The contract states, regardless of tier:

- **Preconditions** — what must be true before the test starts
- **Stimulus** — what triggers the behaviour
- **Expected observation** — what constitutes a pass
- **Timing / tolerance** — when the observation must occur
- **Prohibited outcomes** — what must *not* happen
- **Tier** — where the test runs: `host` (automated), `target` (firmware on
  device, observed via serial), or `manual` (human-operated procedure)
- **Evidence** — how the observation is captured (serial log, manual report)

For the **manual tier**, the contract includes **operator instructions** — step by
step, numbered, with the exact command to flash, what to open, and what output to
look for. Example:

```markdown
**Verification contract — FR-7.1**

- **Tier:** target
- **Preconditions:** Board flashed with `firmware.bin`; serial terminal open at 115200 baud.
- **Stimulus:** Power-cycle the device.
- **Expected:** Within 3 seconds, serial output shows `BOOT: starting`. Then within 10
  seconds, `WIFI: connecting to TestNetwork`.
- **Prohibited:** `WIFI: failed` or no output within 15 seconds.
- **Operator instructions:**
  1. Unplug the ESP32's USB cable, wait 2 seconds, plug it back in.
  2. Watch serial output. Report the first 5 lines after `BOOT: starting`.
  3. Report whether `WIFI: connecting` or `WIFI: failed` appears, and within how many seconds.
```

The contract is the bridge between the FSD (the spec writer) and `/build` (which
consumes it into executable tests).

## What each tier means in manual mode

| Tier | Contract states | Who executes |
|---|---|---|
| **host** | Pure logic function; input → output | Automated (pytest / CI) |
| **target** | Flash firmware; observe serial output | You flash and observe; AI interprets |
| **manual** | Physical action required (button press, cable swap, power cycle from wall) | You follow operator instructions; AI interprets |

A requirement should be tested at the **lowest tier where the defect can
manifest**. Pure logic (layout tables, report builders) → host. Integration and
protocol flow → target (flash + observe serial). Physical actions, real radio
peers you can't simulate → manual.

## Key rules

- **Atomic** — one obligation. Split anything joining two verbs, a behaviour and
  a deadline, or a success and a failure path.
- **Falsyable** — a competent stranger can return pass or fail: precondition,
  stimulus, observable response, deadline, tolerance, failure behaviour, tier.
- **State the numbers the build reads** — flash size, partition offsets, pin
  assignments, clock frequency. If a build can fail on a number, the spec states
  the number.
- **State model for stateful systems** — every (state × event) pair handled,
  ignored, or impossible-by-construction.

## Output layout

```text
docs/00-Overview.md              the plane map
docs/Functionality/              WHAT — the FSD
docs/Method/                     HOW  — build contract, standards, project bindings
docs/UserDocumentation/          OPERATE — one user manual with chapters
testing/test-plan.yaml           every test, what it needs, what it produced
```

## The two unharvestable questions (belong to /harness, not here)

- What has to be debugged before testing starts?
- What does one ordinary successful run look like, end to end?

These cannot be derived from the FSD. `/harness` asks them at the end of Phase 1.

## Exit — Load defined

The FSD is complete when:
- Every Must/Should carries a verification contract
- Architecture (layering), data model, interfaces, state model are present
- All build-dependent numbers are stated in the FSD
- The three planes exist
- No `(assumed)` marker remains on anything architecture-critical

**Run the check in a fresh subagent** — `/define` is the spec author; it is the
worst judge of whether its own work is complete.
