# Gate: Ready for shipment

**Phase:** 3 · Build
**Checked by:** fresh subagent — repo + this gate file only, no memory.

## Requirements

| # | What must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Every requirement met | `testing/test-plan.yaml` | All tests verifying each requirement are `successful` |
| 2 | Journey green | Same → `journey:` | Last operator-reported run: all JRN tests pass |
| 3 | Reconcile empty (both directions) | `testing/test-plan.yaml` | No declared-without-impl tests; no orphan executables |
| 4 | All declared tests have `impl:` | Same → `tests:` | `impl:` field is non-empty for every test |
| 5 | User manual current | `docs/UserDocumentation/User-Manual.md` | Documents all features in the current build |

## Mechanical checks

```bash
# No tests with empty impl that are declared
# No executables without a plan entry
# All journey tests have status: "successful" (last result)
# Wave 3 tests are not written until waves 1+2 green (wave-locked)

# Audit: derive from plan
# Every test status is successful (not not_done, not failed)
# Journey tests all green
```

## Judgement checks

- Have all prohibited outcomes been checked on every passing test?
- Does "Ready for shipment" mean *all* requirements, not just the easy ones?
- Are the gaps documented with resolution owners?

## Verdict format

- `OPEN` — gate passes; the AI announces "Ready for shipment" and stops
- `SHUT` — list each unmet requirement with evidence

## Note

This gate is **derived from the plan**, not declared. The AI never announces
Ready for shipment based on memory or partial evidence.
