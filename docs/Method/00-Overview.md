# Method Plane — Overview

This plane answers **How is it built and changed?** — the build contract, the
development standards, and the project-specific bindings for the AICLP workflow
(adapted for manual testing).

## Contents

| File | Covers |
|---|---|
| `project/architecture.md` | Layering rules, source layout, dependency direction |
| `project/testing-standard.md` | Setup/teardown evidence rules, shared procedures |
| `AI-Workflow.md` | How the AI drives the loop in manual mode |

## Build contract

- **Toolchain:** ESP-IDF in GitHub Actions (pinned container) or local install.
- **Build:** `idf.py build` or via CI on every push.
- **Flash:** Local USB — `idf.py -p <port> flash` or `esptool.py write-flash`.
- **Monitor:** `idf.py -p <port> monitor` at 115200 baud.
- **CI workflow:** `.github/workflows/ci.yml` — builds firmware + runs host tests.

## The AI workflow in manual mode

1. **Define** — `/define` writes the FSD with verification contracts.
2. **Harness** — `/harness` sets up scaffolding, CI, test plan, firmware hooks.
3. **Commission** — `/commission` proves your board works (manual).
4. **Build** — `/build` drives the loop: per requirement, the chain is:
   contract → test declared → executable test → code → build → **you flash** →
   **you observe** → AI interprets → record → green.

### Your role as operator

When the AI needs hardware observation, it gives you:
1. The exact binary and flash offsets (from `flash_args` if available).
2. The serial port and baud rate.
3. A numbered procedure (power-cycle, press button, etc.).
4. What a pass looks like (exact expected output).
5. What to report back (specific fields).

You execute and report. The AI records the result and interprets it.
