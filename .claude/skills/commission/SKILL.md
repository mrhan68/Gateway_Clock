---
name: commission
description: >
  Phase 2 of AI Closed-Loop Programming — Commissioning: prove the project's
  OWN never-seen-working parts (board, wiring, peers) so that a failing test
  means the code, not the setup. Adapted for manual testing: the operator
  performs physical acts (plug, wire, power, flash) and reports observations;
  the AI interprets.
---

# Commission — Phase 2 (Manual Mode)

A door into the Build skill: **the driver, the chain, and the dispatch map live
in `../build/SKILL.md`** — read that first. This skill proves the project-side
parts work *here*, so that when a test fails during Build, it means the code.

## What commissioning means in manual mode

The debugging agenda lists every **project-side** part that has never been
observed doing its job here:

- the DUT board itself — this unit, first flash, first boot
- project peers and simulators (if any)
- project-specific wiring and its polarity
- the firmware's first contact with each path it uses

Until each is seen working, a failing test measures the environment. The
AI cannot observe hardware directly — **you are the sensor**.

## The law: the operator is the instrument

The AI does not touch hardware. It directs you:

> "Flash `build/coldflash/firmware.bin` to your ESP32. Use the flash offsets from
> `build/flash_args` (or `0x10000` for a simple single-image flash). Then report
> the first 10 lines of serial output."

You execute and report. The AI interprets your report as evidence.

## The four checks (manual adaptation)

### Check 1 — Toolchain works

```bash
# Run locally or via CI — the build must produce an artifact
gh run download <run-id> -D fw
ls fw/*.bin fw/flash_args
```

**You must confirm:** the artifact exists and contains the expected images.

### Check 2 — DUT is the right board

- What board do you have? (ESP32, ESP32-S3, ESP32-C3, etc.)
- What flash size? (Check the chip marking or measure with `esptool flash_id`)
- Does it match what the FSD requires?

**You report:** chip type, revision, flash size, and any identifying info.

### Check 3 — Project peers (if any)

If your project uses external peers (a sensor, a network peer, a simulator):
- Are they connected?
- Do they respond to a basic command?

**You report:** their status and basic response.

### Check 4 — Forward path proven

The AI gives you a trivial known-good program (blink/marker print). You:

1. Flash it to your board
2. Report the serial output (or LED behavior)

**Expected:** a known marker (e.g., `ON`/`OFF` alternating, or `alive 1`, `alive 2`).

**You report:** the exact output you observed.

## What the AI asks you for

When the AI needs hardware observation, it asks for specific evidence:

```
I need you to flash this binary and report back:
  - What board are you using? (e.g., ESP32-S3 DevKitC)
  - What is the exact serial output, line by line?
  - Did you see [specific marker]?
  - How long did it take to appear?

Flash command:
  idf.py -p /dev/ttyUSB0 flash 0x0=bootloader.bin 0x8000=partition-table.bin 0x10000=firmware.bin
```

## The debugging agenda

Born from your answer to "what has to be debugged before testing starts?" in
`/harness`. Each entry has:

```text
part                    unproven because            proven by
board USB               never flashed on this unit  serial output shows boot marker
sensor wiring           polarity unknown             known pattern survives inversion
```

Commissioning burns the agenda down. Each item is proven by your observation,
recorded as a dated capability update in `testing/test-plan.yaml`.

## Commissioning is minutes, not days

For a project with no new hardware and no peers, commissioning is:
- Prove the build produces an artifact (CI or local)
- Flash a known-good marker program
- You report: "board boots, serial shows `alive 1`"

Then `/build` runs the journey tests as the gate.

## Exit — DUT ready

**Delivered to `testing/test-plan.yaml` as dated observations:**

| Record | Content |
|---|---|
| **Toolchain record** | Build produces artifacts; CI has run green once |
| **DUT record** | Board type, chip, flash size — matching the FSD |
| **Peer records** | Each peer: what it is, its status (if any) |
| **Forward-path evidence** | The artifact flashed, flash offsets used, marker observed |
| **Capability updates** | Every `unproven` resolved to `yes` (observed) or `no` (cannot do) |
| **Agenda** | Empty of project-side items |

### The gate

```bash
test -f testing/test-plan.yaml                  # plan exists
yaml has no "unproven" capabilities             # agenda burned down
forward-path evidence: build artifact + marker observed
```

**DUT ready** — derived, never declared: the toolchain works, the DUT is verified
as the unit the FSD names, peers (if any) are confirmed, and the forward path is
proven to deliver code to it. From here `/build` runs the journey tests as the
gate and the loop proper begins.
