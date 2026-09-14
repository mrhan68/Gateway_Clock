# Project Documentation — Plane Map

This project uses the **three-plane model** for documentation. Each plane answers
one question and lives in its own directory.

| Question | Plane | Directory |
|---|---|---|
| **What must be true?** | WHAT — the FSD | `docs/Functionality/` |
| **How is it built?** | HOW — the Method | `docs/Method/` |
| **How do I run it?** | OPERATE — the User Manual | `docs/UserDocumentation/` |

## Current contents

- **FSD**: `docs/Functionality/FSD.md`
- **Method**: `docs/Method/00-Overview.md`, `docs/Method/project/architecture.md`
- **User Manual**: `docs/UserDocumentation/User-Manual.md`

## Getting started

1. **Read the FSD** to understand what the system must do.
2. **Read the Method** to understand how to build and change it.
3. **Read the User Manual** to understand how to run and maintain it.
4. **Run `/build`** to start the closed-loop development process.

## The AICLP workflow

This project uses AI Closed-Loop Programming (adapted for manual testing):

1. `/define` — Write the FSD with verification contracts
2. `/grill-me` — Interview to resolve design decisions (if needed)
3. `/harness` — Set up scaffolding, CI, test plan
4. `/commission` — Prove your board works (manual)
5. `/build` — Drive the loop: AI writes tests → code → builds → you flash/test → AI iterates
