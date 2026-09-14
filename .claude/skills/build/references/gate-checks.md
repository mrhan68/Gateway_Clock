# Gate checks (Manual Testing Edition)

A gate that the working session grades itself is not a gate. Every gate check
runs in a **fresh subagent** that did not author the work and has not seen the
conversation.

## The protocol

1. **The check is generated once, at project start.** `/harness` writes one file
   per gate under `testing/gates/`, filled with this project's specifics — its FSD
   path, its requirement id scheme, its board, its capabilities, its markers, its
   CI workflow.
2. **When a gate is reached, spawn a checker** with exactly two inputs: the gate
   file, and the repository.
3. **The checker returns a verdict**, never a fix. `OPEN`, or `SHUT` with each
   unmet requirement named.
4. **On `SHUT`, the driver loops back** to the step that owns each finding, fixes,
   and re-runs the whole check with a new checker.

## Standing exclusion

Evidence is committed artefacts and **your observations** — never memory, transcripts,
or the author's account.

## The five gates

| Gate | Demands |
|---|---|
| **Load defined** | requirements atomic, falsifiable, provenance-tagged; every Must/Should has a verification contract; architecture, data model, interfaces, state model, config, security profile present; three planes committed |
| **AI harnessed** | plan with capabilities and journey tests; debugging agenda; testing standard; firmware hooks matching the FSD's scope; CI green once; **capabilities declared for manual mode** |
| **DUT ready** | toolchain record, DUT record (board type matches FSD), peer records; forward-path evidence (artifact built, flashed, marker observed **by the operator**); project-side capabilities resolved; agenda empty |
| **Ready for shipment** | every requirement met — each verifying test `successful`, prohibited outcomes checked; journey green (last run by operator); reconcile empty both directions; every declared test carries `impl:` |
| **Shipped** | release tagged, you've flashed the released artifact and confirmed the journey green **on the exact bytes** |

## Mechanical checks

These run in a fresh subagent (no memory of prior work):

```bash
# Gate: AI harnessed
test -f testing/test-plan.yaml
test -f testing/debugging-agenda.md
ls .github/workflows/
gh run list -L 1   # CI has run green at least once

# Gate: DUT ready (manual mode)
test -f testing/test-plan.yaml
# No capabilities with available: "unproven"
# Forward-path evidence present in the plan

# Gate: Ready for shipment
# Every test entry has status: "successful" (last result)
# Journey tests all green (last operator-reported run)
# Reconcile: no empty impl:, no orphan executables
```

## Judgement checks

- Does every requirement have a verification contract that includes operator
  instructions for the manual/target tier?
- Could a competent stranger follow those instructions and return pass/fail?
- Is the debugging agenda empty of project-side items?
- Are wave locks respected (deviations not written during Wave 1, etc.)?
