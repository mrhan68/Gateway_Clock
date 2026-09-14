---
name: harness
description: >
  Phase 1 of AI Closed-Loop Programming — harness the AI for a project: the
  one-time setup that straps the AI to this particular load so the loop can run.
  Adapted for manual testing: no testbench installation, no self-hosted runner.
  CI handles host tests + build; the operator handles all target/manual tests.
---

# Harness — Phase 1 sequencer (Manual Testing Edition)

Runs **once per project**. This skill owns the **order** and checklist of what
"harnessed" means; every step delegates to the skill that does the work.

## The sequence

| # | Step | Delegate to | Manual-mode adaptation |
|---|------|-------------|----------------------|
| 1 | **Definition first.** No FSD → run `/define` now. FSD exists → verify every Must/Should carries a verification contract | `/define` | Same. Contracts include manual-tier operator instructions. |
| 2 | Three planes installed | `/define --planes` | Same. |
| 3 | Testing standard stamped | `/define` templates | Same — one file: setup/teardown/evidence rules + shared procedures for manual testing. |
| 4 | `testing/test-plan.yaml` created; **capabilities declared** — what the operator can do (flash, read serial, press buttons), what cannot be automated (WiFi outage, RF injection, etc.) | this skill | **Capabilities are manual.** `available: yes` = you can do it. `available: no` = cannot be done; consequence stated. |
| 5 | Firmware integration hooks — only modules the FSD requires | (inline) | UDP/serial logging always. WiFi/OTA/BLE hooks only when the FSD requires them. |
| 6 | CI: build on push, host tests on PR | `setup-action` | **No release-verify job** (that needs the testbench). The gate runs host tests only — see "What a hosted runner cannot reach" below. |
| 7 | Devcontainer with toolchain | (inline) | Local build via CI or local ESP-IDF. No self-hosted runner installation. |
| 8 | **Generate gate checks** — one file per gate under `testing/gates/` | this skill | Same, but adapted for manual testing (no bench URL, no chip info API calls). |
| 9 | **Two unharvestable questions** | this skill | Same questions, same purpose. |

## Step 4 — Declaring manual capabilities

```yaml
capabilities:
  # --- What the operator can do ---
  flash-local-usb:
    what:      "Flash firmware to the ESP32 via local USB (esptool or idf.py)"
    available: yes
    tier:      manual

  serial-read:
    what:      "Read serial output at 115200 baud"
    available: yes
    tier:      manual

  button-press:
    what:      "Press BOOT and RESET buttons on the device"
    available: yes
    tier:      manual

  wifi-provision:
    what:      "Create a WiFi access point with a known SSID/password"
    available: yes
    tier:      manual

  ble-scan:
    what:      "Scan for BLE advertisements using a phone app"
    available: yes
    tier:      manual

  # --- What cannot be done without automation ---
  wifi-outage:
    what:      "Stop and restart the AP on demand"
    available: no
    consequence: "fault-injection tests for WiFi recovery are excluded"

  rf-injection:
    what:      "Inject RF at 433 MHz"
    available: no
    consequence: "RF receiver sensitivity tests are excluded"
```

A test needing an `available: no` capability renders as `not done` — the gap
is recorded for the audit, not counted as a failure.

## Step 5 — firmware hooks (no hardware touched)

The DUT is **completely out of Phase 1**. Not flashed, not reset, not read.
Only the firmware skeleton is created — it compiles, produces an artifact, and
emits one marker. It implements no requirement.

> **The skeleton needs a hardware fact to compile?** Flash size, partition
> offsets, pin assignments. **Send it back to `/define`** — a constraint the build
> depends on belongs in the FSD.

## Step 6 — CI without a testbench

**What a hosted runner can reach:** host tests (pure logic), linting, firmware
compilation. It cannot reach: USB devices, your LAN equipment, physical buttons,
real radio peers.

The CI workflow builds firmware and runs host tests. Hardware tests run as a
**pre-release step** that you trigger manually — the AI flashes the artifact, you
test, you report results, the AI records them.

```yaml
# .github/workflows/ci.yml — host tests + lint
name: host tests + lint
# runs on every push and PR
# gates: compilation + host test pass
```

There is **no release-verify job** (that needs the testbench to flash the released
artifact). Instead, the release process is:
1. Tag the release.
2. CI builds the artifact.
3. You flash it and run the journey tests.
4. You report results.
5. The AI records them; if green, the release is good.

## Step 9 — the two unharvestable questions

Asked at the end, when you know the system's shape:

1. **What has to be debugged before testing starts?** Which project-side parts
   have never been observed doing their job here — the board, its wiring, its
   peers. Becomes the debugging agenda that `/commission` burns down.

2. **What does one ordinary successful run look like, end to end?** Ordered steps,
   an observable at each. Becomes the **journey tests** — the gate every hardware
   session runs behind.

## Exit — AI harnessed

The check (run in a fresh subagent):

```bash
test -f testing/test-plan.yaml && test -f testing/debugging-agenda.md
ls .github/workflows/                       # build + host tests present
gh run list -L 1                            # CI has actually run green once
```

Then: **open a `/build` session and see whether it can state its position and
name the next act without asking you anything.** If it can — the plan is readable,
the capabilities are declared, the agenda exists — the AI is harnessed.

**AI harnessed** — a `/build` or `/commission` session can open, state its
position from the plan, and act without asking.
