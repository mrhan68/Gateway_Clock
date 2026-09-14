# Test Lifecycle — status, gaps, and the retrofit

What a result means, how to report what's missing, and how to recover a project
where the code came first.

## 1. Test status, and what a requirement being met means

A test has exactly three statuses, **written by whatever ran it** and carrying
the commit so staleness is visible:

```text
not done      no result — see the reason
successful    it ran and passed, including its prohibited-outcome check
failed        it ran and did not
```

### In manual mode: the operator writes the status

When you run a test manually and report the result, the AI records the status.
The operator's report **is** the evidence. The AI classifies:

- **successful** — the exact expected observation was present (you showed the
  serial output, the button press, the network result)
- **failed** — the expected observation was absent, or a prohibited outcome
  occurred
- **not done** — precondition unmet (board not flashed, serial not captured,
  equipment unavailable)

### A requirement is met

When every test that verifies it is `successful`, **including its prohibited-outcome
checks**. This is a derived answer, not a stored one.

### Blocked is computed, never typed

A test listing a `needs:` capability that is `available: no` renders as blocked
with the reason. No test is typed "blocked" by hand.

### The two reconcile lists (both must end empty)

- declared in the plan with empty `impl:` → the backlog
- an executable that no entry's `impl:` points at → undocumented test (retire or
  document it)

## 2. Gap reports — four categories

### 2.1 Specification defects

compound requirement · undefined initial state · undefined resulting state ·
missing timeout or tolerance · ambiguous term · missing failure behaviour ·
conflicting requirements · unverifiable observation · uncontrollable stimulus ·
missing security assumption · missing configuration rule

### 2.2 Verification gaps

approved requirement without a declared test · normative state transition
without a test · numeric boundary without a boundary test · rejection rule
without a negative test · test lacking evidence criteria · test infeasible with
available equipment

### 2.3 Implementation gaps

approved requirement without implementation mapping · executable test absent ·
production behaviour detected but undocumented · obsolete implementation after a
requirement change

### 2.4 Execution and evidence gaps

executable test never run · result not tied to a commit · missing raw evidence ·
stale evidence after a requirement or implementation change · test passed but
prohibited outcomes were never checked

### Also listed (by design, not neglect)

clauses marked `pending: true` (the FSD itself is unresolved),
`testability: philosophical` (no rig can decide them), and **wave-locked
tests** — deviation/negative tests unwritten because their wave has not opened.

## 3. When the code already exists

Write the test from the contract with the implementation closed. Then prove the
test can fail — break the code deliberately, watch it go red, restore.

## 4. Evidence in manual mode

For host tests: pytest output, saved to `test-results/<run-id>/`.

For target/manual tests: the **operator's report** is the evidence. The AI
structures the test to ask for specific fields:

```text
TC-WIFI-01: report
  - exact serial output (first 5 lines after boot)
  - did you see "WIFI: connected"? (yes/no)
  - timestamp of connection message
  - any prohibited output (e.g., "WIFI: failed")
```

The operator's report is recorded in the plan entry's `result` field with a
reference to the commit it ran against. This keeps staleness visible — a result
from a different commit is stale evidence.
