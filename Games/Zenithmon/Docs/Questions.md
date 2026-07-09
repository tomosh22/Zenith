# Zenithmon Questions -- Async Communication with the User

**Purpose:** each entry is a question or decision-point an autonomous agent wants the user's input on. The agent makes a best-guess and PROCEEDS; the user corrects in batch.

**Format:** append-only during a session; the user (or a future agent on their behalf) moves resolved items to the "Resolved" section at the bottom. Entry template: id (`Q-YYYY-MM-DD-NNN`), question, context, best-guess action taken, cost-if-wrong, status.

**Triage markers (plain ASCII):** `[OPEN]` = waiting for user; `[RESOLVED]` = answered/closed; `[STALE]` = dropped.

---

## Open

### [OPEN] Q-2026-07-09-001 -- Branch protection for `zm-tests` requires a manual GitHub-UI step

**Question:** when will `zm-tests` be registered as a REQUIRED status check on master?

**Context:** `.github/workflows/zm-tests.yml` is written and runs on every PR/push from S0, but adding it to the required-checks list is a manual GitHub-UI step only the user can perform (tracked in ManualSetupChecklist.md; policy in CIPolicy.md). Until it is flipped, a red `zm-tests` run does not technically block a merge.

**Best-guess action taken:** proceeding without it. The workflow still runs on every PR; agents treat a red `zm-tests` as a hard merge blocker by convention and note the CI result in the PR body. The S0 gate item stays unticked in Roadmap.md until the user flips it.

**Cost if wrong:** moderate. A regression could auto-merge on a red run that an agent failed to notice; the next session would inherit a broken master. Bounded by the convention above and by every session starting with a baseline `zenith test Zenithmon --headless` run.

**Status:** asked 2026-07-09. Acting on best guess.

---

### [OPEN] Q-2026-07-09-002 -- Terrain bake time for ~25 terrains is an unmeasured estimate

**Question:** is the ~25-terrain plan (one terrain set per outdoor scene via engine change E1) affordable in bake time and file volume, or should routes share terrain sheets?

**Context:** plan risk #1. A full 64x64 chunk export is ~12k files and minutes-to-hours PER terrain; E2's rect export (routes ~16x24 chunks, towns ~16x16) shrinks the projection to ~25k files total and an estimated 20-40 min cold bake -- but that number is a paper estimate, not a measurement.

**Best-guess action taken:** commit to nothing until measured. S3 includes an explicit task: bake 3 real scenes (Home Village + 2 more recipes) and extrapolate before authoring the remaining ~22 terrain recipes. Documented fallback: multiple routes share one terrain sheet (fewer terrain sets, same scene count).

**Cost if wrong:** low-to-moderate if measured at S3 as planned (recipes are cheap to re-point at shared sheets); HIGH if ignored until S9/S10 (a 25x slow bake would poison every tools boot and CI-adjacent workflow during the content stages).

**Status:** asked 2026-07-09. Measurement lands at S3 (see Roadmap.md).

---

### [OPEN] Q-2026-07-09-003 -- Battle-scene visual isolation at the (0,-2000,0) offset is asserted, not yet proven

**Question:** does the additive battle scene at world offset (0,-2000,0) actually render with zero overworld bleed-through?

**Context:** the overworld<->battle transition is an ADDITIVE scene load + `SetScenePaused` on the overworld (a SINGLE reload would reset render systems + physics and re-stream terrain twice per encounter -- seconds of hitch at encounter frequency, disqualifying). The offset puts the arena outside grass LOD rings (200 m max) and terrain high-LOD streaming (1000 m), and the arena has an enclosing backdrop dome. But global render features (skybox, fog, IBL, shadows) are not per-scene, so isolation is asserted by design reasoning, not yet by pixels.

**Best-guess action taken:** proceed with the additive design; the S5 gate includes a dedicated screenshot check for overworld bleed-through at the offset. Documented fallback: SINGLE load + world-state snapshot. Contingency beyond that (only if needed): a per-scene render visibility toggle in the engine.

**Cost if wrong:** moderate. Falling back to SINGLE + snapshot re-introduces the transition hitch and adds a world-state snapshot/restore surface to test -- but the battle engine, director, HUD, and encounter logic are all transition-agnostic, so the rework is confined to the transition layer.

**Status:** asked 2026-07-09. Verified or falsified at the S5 screenshot gate (see Roadmap.md).

---

## Resolved

(none yet)
