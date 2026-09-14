# Testing Standard

## Tier taxonomy

| Tier | What it tests | Who runs it | Cost |
|---|---|---|---|
| **host** | Pure logic (decoders, tables, report builders) | Automated (pytest / CI) | milliseconds |
| **target** | Firmware on device, observed via serial | You (flash + read serial) | minutes |
| **manual** | Physical actions: button presses, cable swaps, real peers | You (follow instructions, report) | minutes |

## Rules for all tests

1. **Independently executable.** Each test can run from a known state without
   depending on another test (except workflow tests, whose dependency is the claim).
2. **Restores to baseline.** Tests clean up after themselves: clear NVS, reset
   credentials, power-cycle if needed.
3. **No predictions.** A test status is `not done` until it runs and you report.
4. **Exact evidence.** The operator reports specific observations, not summaries.
5. **Prohibited outcomes checked.** Every test asserts what must NOT happen.

## Host tier rules

- Run `pytest tests/host/` before every hardware session.
- A red host test is a reason not to start — fix it first.
- Pure functions only — no I/O, no hardware.

## Target tier rules

- The AI gives you flash offsets from `flash_args` (or states them explicitly).
- Always power-cycle after flashing to clear stale state.
- The firmware must emit **structured log lines** with known prefixes so you can
  grep for them in serial output.
- Report exact serial output — at least the first 5 lines after boot.

## Manual tier rules

- The AI gives you numbered, numbered steps with exact physical actions.
- You report what you observed — don't interpret.
- If a step is ambiguous, ask for clarification before executing.

## Evidence capture

- **Host tests:** pytest output saved automatically.
- **Target/manual tests:** your report is the evidence. The AI structures each
  test to ask for specific fields. The result is recorded in `test-plan.yaml`
  with a reference to the commit the firmware was built from.
