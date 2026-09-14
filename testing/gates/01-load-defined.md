# Gate: Load defined

**Phase:** 0 · Definition
**Checked by:** fresh subagent — repo + this gate file only, no memory.

## Requirements

| # | What must be true | Where to look | Evidence |
|---|---|---|---|
| 1 | Three plane directories exist | `docs/00-Overview.md`, `docs/Functionality/`, `docs/Method/`, `docs/UserDocumentation/` | All three committed; `00-Overview.md` describes the plane map |
| 2 | FSD exists | `docs/Functionality/FSD.md` | File present, non-empty |
| 3 | FSD has requirements | Same | `grep -c "FR-\|NFR-" docs/Functionality/FSD.md` > 0 |
| 4 | Every Must/Should has a verification contract | Same | Each requirement paragraph has `Verification contract:` |

## Mechanical checks

```bash
# Planes exist
test -f docs/00-Overview.md
test -d docs/Functionality
test -d docs/Method
test -d docs/UserDocumentation

# FSD exists with requirements
test -f docs/Functionality/FSD.md
grep -q "FR-\|NFR-" docs/Functionality/FSD.md

# Every FR/NFR has a verification contract
grep -c "Verification contract" docs/Functionality/FSD.md
```

## Judgement checks

- Does every requirement carry a verification contract that includes operator
  instructions for the target/manual tier?
- Could a competent stranger follow those instructions and return pass/fail?
- Are all build-dependent numbers (flash size, pin assignments, etc.) stated
  in the FSD, not left as defaults?

## Verdict format

- `OPEN` — gate passes, Phase 1 may begin
- `SHUT` — list each unmet requirement with evidence

## Standing exclusion

Evidence is committed artefacts only — never memory, transcripts, or the
author's account of what was done.
