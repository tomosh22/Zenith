# Undervault -- The Board

**Document purpose:** how the Markdown in this directory and the `UV` board relate. Read it once; after that [Roadmap.md](Roadmap.md) and the board carry the keys and this file only answers "why is it arranged this way". The model is `Games/Zenithmon/Docs/Board.md` and `Games/Foundry/Docs/Board.md`; Undervault follows them exactly.

**Board:** project **`UV`**, served by the same `C:\dev\Zenith` checkout as `ZM`, `ZEN`, `DP`, `FD` and the other new game boards -- the board is this checkout, not a separate machine. One ticket in flight per checkout (I5).

---

## 1. What lives where

| | Authority | Why it cannot move |
|---|---|---|
| **Epics, stories, tasks, bugs, blockers, sprints, versions, estimates** | the **board** | A dependency recorded in Markdown cannot stop the loop claiming work out of order. The board's `BLOCKS` links can, and do. |
| **The design** -- `GameDesignDocument.md`, `TechnicalDesignDocument.md`, and the living docs the seed ticket creates (`Status.md`, `TestPlan.md`, `SaveFormat.md`, `Shortfalls.md`, `Questions.md`, `AgentBriefing.md`) | these **files** | A loop worker has no network and no shell; anything it must obey has to be a file it can Read. |
| **Decisions** -- `DecisionLog.md` (`UV-D-` prefix) | this **file** | `zagent decide` appends here; a Zenith session greps here. |
| **The pinned unit baseline** | `Tools/unit_baselines.json` (`"Undervault"` row, added by UV-6) | The gate asserts `ran == baseline` EXACTLY; one unprotected home, bumped only from an OBSERVED Null run. |

---

## 2. The milestone epics

Every `## [UV-n]` heading in [Roadmap.md](Roadmap.md) is an epic, one per TDD §20 phase, plus one for deferred post-launch work.

| Milestone | Epic | Sprint | Version | Gate |
|---|---|---|---|---|
| UV Phase 0 — Spikes | `UV-1` | `UV Phase 0 — Spikes` | — | `UV-13` |
| UV Vertical Slice | `UV-2` | `UV Vertical Slice` | Vertical Slice | `UV-31` |
| UV Alpha — MVP feature-complete | `UV-3` | `UV Alpha — MVP feature-complete` | Alpha | `UV-47` |
| UV Android Beta | `UV-4` | `UV Android Beta` | Android Beta | `UV-55` |
| UV v1.0 | `UV-5` | `UV v1.0` | v1.0 | `UV-65` |
| UV Post-launch candidates (deferred) | `UV-66` | (none -- deferred) | — | — |

**The epics carry no sprint and no category.** An epic spans its sprint; the sprint holds exactly that epic's children. Children are filed `--category Undervault` (the Undervault gate list). A Undervault ticket whose diff reaches `Zenith/**` gets the Engine gates unioned in by `zagent gates <KEY>`; engine work the TDD names is filed on the ZEN or FD board and BLOCKS the consumer here (see the Roadmap's engine-dependency line).

**Backfilled by hand:** `UV-2` was missing from the board (issue numbers ran 1, 3, 4, 5) while its sprint already existed; it was written directly into `Board/tickets/` on 2026-09-19 because a `zagent create` can only ever append the next number.

## 3. Sprints

**Every phase is also a sprint, one for one with its epic**, named identically, its goal the TDD's exit line. One is ACTIVE at a time (a DB constraint); `zagent sprint start "<name>"` is the human decision that opens a phase -- until then every ticket shows `OUT OF STAGE` and the queue offers nothing (`zagent check <KEY>` prints it). Nothing has been started: the first ticket is the scaffold, and its category gates FAIL until that diff lands.

★ **Completing a sprint needs its gate.** Each phase's acceptance ticket (`human-gate`) is the sprint's designated gate (`zagent sprint gate`); `sprint complete` is refused while it is unmet, and `--force` does not reach the check.

## 4. Versions

A **version** is what a build contains; a **sprint** is when work happened. The Version column above says which phases each build contains; Phase 0 / P0 carries no version because spikes and foundations are evidence, not shipped content.

## 5. Markers, and who is in the loop

| Label | Meaning here | Tickets |
|---|---|---|
| `needs-human` | never claimed; a person must do the work | UV-7, UV-10, UV-11, UV-31, UV-46, UV-47, UV-52, UV-53, UV-64 |
| `needs-gpu` | claimed like any other; build `Vulkan_*_True` and run windowed; carries a `gpu-reason:*` companion | UV-9, UV-10, UV-19, UV-20, UV-30, UV-39, UV-49, UV-59, UV-63 |
| `human-gate` | the loop does the whole job, then stops at In Review | UV-13, UV-31, UV-47, UV-55, UV-65 |
| `deferred` | skipped by the queue, claimable by name | UV-67, UV-68, UV-69, UV-70, UV-71, UV-72, UV-73 |
| `question` | a ruling is part of the deliverable | — |
| `content` / `docs` | what kind of deliverable it is | tables, clips, living docs |

## 6. What the scaffold changes

`Games/Undervault` is a static visual study (commit `84e2fd12`): `Undervault.cpp` selects one of three Blender cutaways and the GLB and HUD assets are gitignored, so the category gates are red or vacuous on a fresh checkout. The first ticket, `UV-6`, replaces it with the TDD §1 skeleton and observes the first `Undervault` baseline row; `UV-7` is the human half (unit-gate line, `uv-tests.yml`, doc-lint).

## 7. Drift

`zagent board status --project UV --roadmap Undervault/Docs/Roadmap.md` compares the keys in this directory's `Roadmap.md` with the board and exits 1 on drift. Run it after any re-parenting, re-filing or sprint change, and after `zagent docs sync`.
