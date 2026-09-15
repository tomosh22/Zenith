# Foundry -- The Board

**Document purpose:** how the Markdown in this directory and the `FD` Jira board relate. Read it once; after that [Roadmap.md](Roadmap.md) and the board carry the keys and this file only answers "why is it arranged this way". The model is `Games/Zenithmon/Docs/Board.md`; Foundry follows it exactly.

**Board:** project **`FD`**, on the machine named by `ZAGENT_URL`, served by the same `C:\dev\Zenith` checkout as `ZM`, `ZEN`, `DP`, `UV` and `HE`. One ticket in flight per checkout (I5).

---

## 1. What lives where

| | Authority | Why it cannot move |
|---|---|---|
| **Epics, stories, tasks, bugs, blockers, sprints, versions, estimates** | the **board** | A dependency recorded in Markdown cannot stop the loop claiming work out of order. The board's `BLOCKS` links can, and do. |
| **The design** -- `GameDesignDocument.md`, `TechnicalDesignDocument.md`, and the living docs M0-05 seeds (`Status.md`, `TestPlan.md`, `SaveFormat.md`, `Shortfalls.md`, `Questions.md`, `AgentBriefing.md`) | these **files** | A loop worker has no network and no shell; anything it must obey has to be a file it can Read. |
| **Decisions** -- `DecisionLog.md` (`FD-D-` prefix) | this **file** | `zagent decide` appends here; a Zenith session greps here. |
| **The pinned unit baseline** | `Tools/unit_baselines.json` (`"Foundry"` row) | The gate asserts `ran == baseline` EXACTLY; one unprotected home, bumped only from an OBSERVED Null run. |

---

## 2. The milestone epics

Every `## [FD-n]` heading in [Roadmap.md](Roadmap.md) is an epic, one per TDD §20 milestone, plus one for deferred post-ship work.

| Milestone | Epic | Sprint | Version |
|---|---|---|---|
| M0 — Two parallel spikes (the de-risk milestone) | `FD-1` | `FD M0 — Two parallel spikes (the de-risk milestone)` | — |
| M1 — Vertical slice (Windows, mouse-as-touch) | `FD-9` | `FD M1 — Vertical slice (Windows, mouse-as-touch)` | Vertical Slice |
| M2 — Touch & device | `FD-3` | `FD M2 — Touch & device` | Vertical Slice |
| M3a — Fluids + circuits + construction tools | `FD-4` | `FD M3a — Fluids + circuits + construction tools` | Feature Complete |
| M3b — Trains + drones | `FD-5` | `FD M3b — Trains + drones` | Feature Complete |
| M4 — Threat | `FD-6` | `FD M4 — Threat` | Feature Complete |
| M5 — Gigafactory hardening | `FD-7` | `FD M5 — Gigafactory hardening` | Hardened |
| M6 — Content complete & polish | `FD-8` | `FD M6 — Content complete & polish` | Ship 1.0 |
| v1.x — Post-ship candidates (deferred) | `FD-10` | (none -- deferred) | — |

**The epics carry no sprint and no category.** An epic spans its sprint; the sprint holds exactly that epic's children (both writers refuse an epic as a sprint member). Children are filed `--category Foundry` (game work: the Foundry gate list) or `--category Engine` (TDD §19 register items: the Engine gate list, which compiles every game). A Foundry ticket whose diff reaches `Zenith/**` gets the Engine gates unioned in by `zagent gates`.

## 3. Sprints

**Every milestone is also a sprint, one for one with its epic**, named identically, its goal the TDD's exit line. One is ACTIVE at a time (a DB constraint); `zagent sprint start "FD M0 — Two parallel spikes (the de-risk milestone)"` is the human decision that opens the pilot milestone -- until then every ticket shows `OUT OF STAGE` and the queue offers nothing (`zagent check <KEY>` prints it). A targeted `/tick FD-11` still takes out-of-stage work on purpose.

★ **Completing a sprint needs its gate.** Each milestone's acceptance ticket (`human-gate`, e.g. `

## 4. Versions

A **version** is what a build contains; a **sprint** is when work happened.

| Version | Contains | Gate |
|---|---|---|
| **Vertical Slice** | M1 + M2 | the slice playable by touch on the floor phone (M2-14) |
| **Feature Complete** | M3a + M3b + M4 | every GDD §5 system present and digest-tested (M4-12) |
| **Hardened** | M5 | promise-scale budgets met on device through a 2-hour soak (M5-10) |
| **Ship 1.0** | M6 | GDD §13 promises measured true; first win on device (M6-13) |

M0 carries no version: spikes are evidence, not shipped content.

## 5. Markers, and who is in the loop

| Label | Meaning here | Examples |
|---|---|---|
| `needs-human` | never claimed; a person must do the work | on-device sessions (M0-15, M2-11, M4-11, M5-09, M6-12), protected-path edits (M0-16), the release keystore (M6-09) |
| `needs-gpu` | claimed like any other; build `Vulkan_*_True` and run windowed | presentation and art tickets, the render spike harness |
| `human-gate` | the loop does the whole job, then stops at In Review | every milestone-acceptance ticket; the audio mix and final art passes |
| `deferred` | skipped by the queue, claimable by name | the v1.x epic's children; big.LITTLE affinity (M5-08) |
| `engine-gap` | a TDD §19 register item, filed `--category Engine` | M2-03, M2-05, M3A-09, M5-03 … |
| `content` / `visual` / `docs` | what kind of deliverable it is | tables, captures, living docs |

## 6. What the scaffold changed

`Games/Foundry` was committed as a static visual study (commit `84e2fd12`) whose assets are Blender output and gitignored, so `zenith test Foundry --headless` is red on a fresh checkout. The board's first ticket, `

## 7. Drift

`zagent board status --project FD` compares the keys in this directory's `Roadmap.md` with the board and exits 1 on drift. Run it after any re-parenting, re-filing or sprint change, and after `zagent docs sync`.

