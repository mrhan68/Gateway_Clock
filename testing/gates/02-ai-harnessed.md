# Gate: AI harnessed

**Phase:** 1 · Harness
**Checked by:** fresh subagent — repo + this gate file only, no memory.

## Requirements

| # | What must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Test plan exists | `testing/test-plan.yaml` | File present |
| 2 | Debugging agenda exists | `testing/debugging-agenda.md` | File present |
| 3 | Testing standard exists | `docs/Method/project/testing-standard.md` | File present |
| 4 | CI workflow exists and has run | `.github/workflows/ci.yml` | `gh run list -L 1` shows at least one green run |
| 5 | Capabilities declared for manual mode | `testing/test-plan.yaml` | `capabilities:` section present, with `available:` on each |
| 6 | Journey tests present | `testing/test-plan.yaml` → `journey:` | At least one JRN test |
| 7 | Firmware hooks match FSD scope | `firmware/main/` | Only modules the FSD requires |

## Mechanical checks

```bash
test -f testing/test-plan.yaml
test -f testing/debugging-agenda.md
test -f docs/Method/project/testing-standard.md
test -f .github/workflows/ci.yml
grep -q "capabilities:" testing/test-plan.yaml
grep -q "journey:" testing/test-plan.yaml
gh run list -L 1   # CI has run at least once

# A /build session can open and state its position without asking
```

## Verdict format

- `OPEN` — gate passes, Phase 2 may begin
- `SHUT` — list each unmet requirement with evidence
