# Functional Specification Document (FSD)
# [Project Name]

> **Placeholder.** Run `/define` to generate this FSD from your project description,
> or write it by hand following this template.

## 1. System Overview

**Purpose.** [What the system does — one sentence.]

**Goals:** [What success looks like — measurable.]
**Non-goals:** [What is explicitly out of scope.]

## 2. System Architecture

### 2.1 Logical Architecture

[How the components interact — a diagram or description.
e.g., `Phone app → (BLE NUS) → ESP32-S3 → (USB HID) → host computer`]

### 2.2 Hardware / Platform Architecture

[What hardware is required. e., `ESP32-S3 (USB-OTG), powered from USB bus power.`]

### 2.3 Software Architecture

[Runtime structure — tasks, threads, models.
e.g., `FreeRTOS tasks: BLE rx, HID tx, WiFi/OTA.`]

### 2.4 Component Layering

Stacked layer diagram (application on top, foundation at bottom):

- **L2 — Application logic:** [your logic here]
- **L1 — Interfaces:** [external-facing modules you own]
- **L0 — Foundation:** [SDKs, managed services, configured not implemented]

## 3. Implementation Phases

- **Phase 1:** [Goal. Exit criterion.]
- **Phase 2:** [Goal. Exit criterion.]

## 4. Risks, Assumptions & Dependencies

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| [name] | [Low/Medium/High] | [Low/Medium/High] | [how you'll handle it] |

## Part A — Application Logic (L2)

### [FR-x.x] — [Component name]

**Purpose.** [One line.]

**Requirements:**

- **FR-x.x** [Must]: [The requirement, one obligation, falsifiable.]

**Verification contract:**

- **Tier:** [host | target | manual]
- **Preconditions:** [What must be true before]
- **Stimulus:** [What triggers the behaviour]
- **Expected:** [What a pass looks like — exact output/observation]
- **Prohibited:** [What must NOT happen]
- **Operator instructions:** [For target/manual tier — numbered steps]
  1. [Step 1]
  2. [Step 2]
  3. [Step 3]

**Decision logic.** [How it works.]

**Failure modes.** [What can go wrong and what happens.]

## Part B — Interfaces (L1)

### [FR-x.x] — [Interface name]

**Purpose & peer.** [What it talks to.]

**Requirements:**
- **FR-x.x** [Must]: [Requirement]

[Repeat for each interface]

## Part C — Foundation / Transport (L0)

**Purpose.** [What it is, configured not implemented.]
[Test **transitively** via L1/L2 tests.]

## Part D — Cross-cutting Concerns

### Security

[Threat model first, then requirements derived from it.]

### Logging / Observability

[Structured logs the operator can grep for.]

## Part E — Operations & Verification

### Operational Procedures

[How the operator uses the system: flash, provision, use, recover.]

### Verification

**Test architecture:** Tiers: host, target, manual. [Mapping of layers to tiers.]
**Acceptance:** [End-to-end journey the operator runs.]
**Traceability:** Generated — see `testing/test-plan.yaml`.
