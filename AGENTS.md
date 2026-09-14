# Antigravity Agent Guidelines — AICLP Manual Testing Mode

> **Methodology**: AI Closed-Loop Programming (AICLP) adapted for manual testing.
> **Active Skills**: Located in `.agents/skills/`.

---

## 1. Installed Workspace Skills

The following skills are installed in `.agents/skills/` and can be triggered directly or via slash commands:

| Skill | Trigger / Command | Phase / Role | Description |
|---|---|---|---|
| **Grill-Me** | `/grill-me` | **Pre-Design** | Interviews the user relentlessly about a plan or architecture until reaching shared understanding. Resolves decision trees branch-by-branch. |
| **Define** | `/define`, `/define update`, `/define --grill` | **Phase 0 (FSD)** | Creates and updates the Functional Specification Document. Every requirement must carry an atomic verification contract with manual operator instructions. |
| **Harness** | `/harness` | **Phase 1 (Setup)** | Scaffolds test plans (`testing/test-plan.yaml`), declares manual operator capabilities, sets up CI and firmware integration hooks. |
| **Commission** | `/commission` | **Phase 2 (Bringup)** | Verifies physical hardware, board bringup, wiring, and power before running test suites, ensuring failures mean code issues, not hardware faults. |
| **Build** | `/build` | **Phase 3 (Closed Loop)** | Drives the closed loop. Reads the position in `testing/test-plan.yaml`, writes executable tests (xfail), writes code, builds firmware, provides exact flash/monitor instructions to the operator, and interprets reported results. |

---

## 2. Core Operational Rules & Discipline

1. **No Code Without a Clause**:
   - Every line of code written must trace directly to a requirement in the FSD (`fsd-smart-clock-gateway.md`).
   - If a new requirement arises, it must be drafted and approved via `/define` before implementing code.

2. **Verification Contracts Required**:
   - Every requirement must state:
     - **Preconditions**
     - **Stimulus**
     - **Expected observation**
     - **Timing / tolerance**
     - **Prohibited outcomes**
     - **Tier** (`host`, `target`, or `manual`)
     - **Operator Instructions** (step-by-step numbered steps for flashing, serial monitoring, or hardware manipulation).

3. **Strict Chain of Execution**:
   ```
   Contract in FSD → Test declared in plan → Executable test (xfail) →
   Code implementation → Build → [Operator: Flash & Observe] →
   Record result in test-plan.yaml → Green
   ```

4. **Host Tests First**:
   - Always run automated host-tier tests (`pytest tests/host/`) before any physical hardware session.

5. **Three Development Waves**:
   - **Wave 1**: Standard tests (running prototype). Deviations and negatives locked.
   - **Wave 2**: Deviations / variants. Negatives locked.
   - **Wave 3**: Negatives, error handling, and security.
