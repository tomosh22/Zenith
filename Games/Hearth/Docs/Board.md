# Hearth -- The Board

**Document purpose:** how the Markdown in this directory and the `HE` board relate. Read it once; after that [Roadmap.md](Roadmap.md) and the board carry the keys and this file only answers "why is it arranged this way". The model is `Games/Zenithmon/Docs/Board.md` and `Games/Foundry/Docs/Board.md`; Hearth follows them exactly.

**Board:** project **`HE`**, served by the same `C:\dev\Zenith` checkout as `ZM`, `ZEN`, `DP`, `FD` and the other new game boards -- the board is this checkout, not a separate machine. One ticket in flight per checkout (I5).

---

## 1. What lives where

| | Authority | Why it cannot move |
|---|---|---|
| **Epics, stories, tasks, bugs, blockers, sprints, versions, estimates** | the **board** | A dependency recorded in Markdown cannot stop the loop claiming work out of order. The board's `BLOCKS` links can, and do. |
| **The design** -- `GameDesignDocument.md`, `TDD.md`, and the living docs the seed ticket creates (`Status.md`, `TestPlan.md`, `SaveFormat.md`, `Shortfalls.md`, `Questions.md`, `AgentBriefing.md`) | these **files** | A loop worker has no network and no shell; anything it must obey has to be a file it can Read. |
| **Decisions** -- `DecisionLog.md` (`HE-D-` prefix) | this **file** | `zagent decide` appends here; a Zenith session greps here. |
| **The pinned unit baseline** | `Tools/unit_baselines.json` (`"Hearth"` row, added by HE-5) | The gate asserts `ran == baseline` EXACTLY; one unprotected home, bumped only from an OBSERVED Null run. |

---

## 2. The milestone epics

Every `## [HE-n]` heading in [Roadmap.md](Roadmap.md) is an epic, one per TDD §17 phase, plus one for deferred post-launch work.

| Milestone | Epic | Sprint | Version | Gate |
|---|---|---|---|---|
| HE P0 — Foundations | `HE-1` | `HE P0 — Foundations` | — | `HE-16` |
| HE P1 — Vertical slice: MVP, "one sim, one house" | `HE-2` | `HE P1 — Vertical slice: MVP, "one sim, one house"` | MVP | `HE-34` |
| HE P2 — Households & town | `HE-3` | `HE P2 — Households & town` | v1.0 | `HE-44` |
| HE P3 — Retail | `HE-4` | `HE P3 — Retail` | Retail 1.0 | `HE-53` |
| HE Post-1.0 candidates (deferred) | `HE-54` | (none -- deferred) | — | — |

**The epics carry no sprint and no category.** An epic spans its sprint; the sprint holds exactly that epic's children. Children are filed `--category Hearth` (the Hearth gate list). A Hearth ticket whose diff reaches `Zenith/**` gets the Engine gates unioned in by `zagent gates <KEY>`; engine work the TDD names is filed on the ZEN or FD board and BLOCKS the consumer here (see the Roadmap's engine-dependency line).

**Backfilled by hand:** `HE-2` was missing from the board (issue numbers ran 1, 3, 4) while its sprint already existed; it was written directly into `Board/tickets/` on 2026-09-19 because a `zagent create` can only ever append the next number.

## 3. Sprints

**Every phase is also a sprint, one for one with its epic**, named identically, its goal the TDD's exit line. One is ACTIVE at a time (a DB constraint); `zagent sprint start "<name>"` is the human decision that opens a phase -- until then every ticket shows `OUT OF STAGE` and the queue offers nothing (`zagent check <KEY>` prints it). Nothing has been started: the first ticket is the scaffold, and its category gates FAIL until that diff lands.

★ **Completing a sprint needs its gate.** Each phase's acceptance ticket (`human-gate`) is the sprint's designated gate (`zagent sprint gate`); `sprint complete` is refused while it is unmet, and `--force` does not reach the check.

## 4. Versions

A **version** is what a build contains; a **sprint** is when work happened. The Version column above says which phases each build contains; Phase 0 / P0 carries no version because spikes and foundations are evidence, not shipped content.

## 5. Markers, and who is in the loop

| Label | Meaning here | Tickets |
|---|---|---|
| `needs-human` | never claimed; a person must do the work | HE-6, HE-15, HE-33, HE-48, HE-51 |
| `needs-gpu` | claimed like any other; build `Vulkan_*_True` and run windowed; carries a `gpu-reason:*` companion | HE-12, HE-14, HE-15, HE-22, HE-26, HE-31, HE-32, HE-38, HE-39 |
| `human-gate` | the loop does the whole job, then stops at In Review | HE-16, HE-34, HE-44, HE-53 |
| `deferred` | skipped by the queue, claimable by name | HE-55, HE-56, HE-57, HE-58, HE-59, HE-60, HE-61 |
| `question` | a ruling is part of the deliverable | HE-43, HE-60, HE-61 |
| `content` / `docs` | what kind of deliverable it is | tables, clips, living docs |

## 6. What the scaffold changes

`Games/Hearth` holds only `Docs/` today, so the category gates fail on a fresh checkout and pass exactly when the first ticket lands. `HE-5` runs `zenith new Hearth` and shapes the result to TDD §2 (seven components, `Source/` tree, three committed scenes, `android:true`) and observes the first `Hearth` baseline row; `HE-6` is the human half (unit-gate line, `he-tests.yml`, doc-lint).

## 7. Drift

`zagent board status --project HE --roadmap Hearth/Docs/Roadmap.md` compares the keys in this directory's `Roadmap.md` with the board and exits 1 on drift. Run it after any re-parenting, re-filing or sprint change, and after `zagent docs sync`.
