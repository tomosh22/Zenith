# Zenithmon Questions -- Async Communication with the User

**Purpose:** each entry is a question or decision-point an autonomous agent wants the user's input on. The agent makes a best-guess and PROCEEDS; the user corrects in batch.

**Format:** append-only during a session; the user (or a future agent on their behalf) moves resolved items to the "Resolved" section at the bottom. Entry template: id (`Q-YYYY-MM-DD-NNN`), question, context, best-guess action taken, cost-if-wrong, status.

**Triage markers (plain ASCII):** `[OPEN]` = waiting for user; `[RESOLVED]` = answered/closed; `[STALE]` = dropped.

---

## Open

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

### [RESOLVED] Q-2026-07-09-001 -- Branch protection for `zm-tests`

**Resolution (2026-07-10):** the user directed the agent to do it ("Add
zm-tests yourself"). Discovery: master had NO branch protection and no
rulesets at all -- the required-checks discipline had been purely
conventional. Classic branch protection was created via
`gh api PUT .../branches/master/protection` with required contexts
`[zm-tests]`, `strict=false`, `enforce_admins=false` (owner direct pushes
bypass; agent PRs are always gated). Full shape + consequences: CIPolicy.md
section 4; checklist item ticked in ManualSetupChecklist.md.
