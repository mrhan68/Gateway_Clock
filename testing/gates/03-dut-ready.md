# Gate: DUT ready

**Phase:** 2 · Commissioning
**Checked by:** fresh subagent — repo + this gate file only, no memory.

## Requirements

| # | What must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Toolchain works | CI run / local build | Firmware artifact exists |
| 2 | DUT verified | `testing/test-plan.yaml` → debugging_agenda | Board type matches FSD; "first flash" agenda item resolved |
| 3 | Forward path proven | Same; committed evidence | Operator observed marker output after flash |
| 4 | Debugging agenda empty | `testing/test-plan.yaml` → debugging_agenda | No project-side items remain unproven |

## Mechanical checks

```bash
# Debugging agenda exists and has no "unproven" items
test -f testing/test-plan.yaml
grep -q "debugging_agenda:" testing/test-plan.yaml
# Count unresolved items (should be 0 after commissioning)
```

## Judgement checks

- Has the operator confirmed the board boots and produces the expected marker?
- Does the board match what the FSD specifies?
- Are all peers (if any) confirmed working?

## Verdict format

- `OPEN` — gate passes, Wave 1 of Build may begin
- `SHUT` — list each unmet requirement with evidence

## Standing exclusion

Evidence is committed artefacts and **operator observations** — never memory,
transcripts, or the author's account.
