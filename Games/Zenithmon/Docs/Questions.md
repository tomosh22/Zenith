# Zenithmon Questions -- Async Communication with the User

**Purpose:** each entry is a question or decision-point an autonomous agent wants the user's input on. The agent makes a best-guess and PROCEEDS; the user corrects in batch.

**Format:** append-only during a session; the user (or a future agent on their behalf) moves resolved items to the "Resolved" section at the bottom. Entry template: id (`Q-YYYY-MM-DD-NNN`), question, context, best-guess action taken, cost-if-wrong, status.

**Triage markers (plain ASCII):** `[OPEN]` = waiting for user; `[RESOLVED]` = answered/closed; `[STALE]` = dropped.

---

## Open

### [OPEN] Q-2026-07-12-004 -- ZM_Breeding: reduced model on the shipped data (no gender / egg groups / egg moves)

**Question:** ratify (or override) the reduced breeding model implemented for S2 box-6 SC1. The species table ships with NO egg-group, gender, hatch-cycle, egg-move, or species->ability data; adding any of those is a data-model EXPANSION (the scope-change direction), so I built breeding on the data that exists and flagged the reductions rather than expanding the model unilaterally.

**Best-guess actions taken (faithful reductions, each additive later):**
1. **No gender.** The GDD implies "opposite sexes" but no gender field exists on the species or monster. I use a "first parent = mother" convention for offspring species + ability, with compatibility = shared archetype (the egg-group proxy) + both non-legendary. Cost-if-wrong: additive -- a gender field + species ratio + a gender check in `ZM_AreSpeciesCompatible` can be added later without touching the egg-gen math.
2. **Archetype = egg-group proxy** (no real egg-group taxonomy exists). Cost-if-wrong: low; swapping to a real egg-group column is a localized change in `ZM_GetBreedingGroup`.
3. **No egg moves / no hidden abilities / no Ditto-analog / no Masuda-shiny.** Egg moves = the base-evo level-1 learnset; ability = mother's. Cost-if-wrong: low/additive when the data gains egg-move rows / a second ability slot.

**Cost if wrong:** LOW across all -- each reduction is additive when the underlying data model grows; no golden or contract depends on their ABSENCE.

**Status:** asked 2026-07-12. Implementation + full local gate complete under these rulings (ZM-D-045). OPEN for user override. NOTE: adding gender / real egg groups is a Scope.md data-model expansion -- if the user wants it, it should land as its own scoped item with a DecisionLog entry first.

---

### [OPEN] Q-2026-07-12-003 -- ZM_BattleAI: three in-scope rulings (file location, no-Struggle, tunable thresholds)

**Question:** ratify (or override) three implementation rulings made for the S2 `ZM_BattleAI` box, all judged IN-SCOPE (not scope changes), so I proceeded rather than halting the autonomous loop.

**Best-guess actions taken:**
1. **File location = `Source/Battle/`** (with the nine sibling battle systems), NOT MasterPlan's `Source/AI/`. No `Source/AI/` directory exists and one file does not justify creating one. Cost-if-wrong: trivial -- relocating one `.h`/`.cpp` + one include line is a mechanical additive change.
2. **No Struggle fallback.** `ZM_ChooseAction` assumes >=1 legal action (a move with PP, or a switch). When every move is out of PP and no switch exists there is no "Struggle" action -- a PRE-EXISTING engine gap (the executor has no Struggle either). Cost-if-wrong: low; adding Struggle is additive when the engine gains it, and no test/golden depends on its absence.
3. **SMART thresholds + the GREEDY/CHAMPION rolls are fixed constants** (heal < 50% HP; hopeless = `effIn>=200 && effOut<=100`; KO uses guaranteed roll 85; expected damage uses roll 92). These are balance knobs, flagged S11-tunable. Cost-if-wrong: low; they are named constants, retunable without structural change.

**Cost if wrong:** LOW across all three -- each is localized + additive.

**Status:** asked 2026-07-12. Implementation + full local gate complete under these rulings (ZM-D-044). OPEN for user override; all three remain additive.

---

### [OPEN] Q-2026-07-12-002 -- ZM_ExpAndLevel: two sequencing rulings (move-overflow + evolution triggers)

**Question:** ratify (or override) two implementation rulings made for the S2 `ZM_ExpAndLevel` box, both of which I judged to be IN-SCOPE sequencing (not scope changes) based on Scope.md + the GDD + the Roadmap stage plan, and therefore proceeded on rather than halting the autonomous loop.

**Context:** the spec-extraction pass flagged these as "may need the user." I checked the binding docs: Scope.md lists "exp/levels, evolution" as IN and "no networking/multiplayer/trading" (so trade-evolution is already OUT); the GDD specifies "level-up move learning mid-battle" and lists evolution *lines* (F17/F23/...) but specifies NO trigger method (no level-N/stone/friendship detail); items -- hence stone evolution -- are Roadmap-S9; the battle engine is headless with no UI until S5/S6.

**Best-guess action taken:**
1. **4-move overflow = SKIP.** On a mid-battle level-up that would teach a 5th move, the new move is NOT learned (a `MOVE_LEARNED` event fires only when a move is actually added). The interactive "replace which move?" choice is inherently a UI feature -> deferred to S5/S6. I did NOT reserve a `MOVE_LEARN_PENDING` event now (the `ZM_BattleEvent` POD is append-only, so S5/S6 can add it when the UI needs it -- YAGNI).
2. **Evolution = LEVEL-trigger only this box.** Stage-1 -> stage-2 eligibility is derived at L16 and stage-2 -> stage-3 at L36. The engine emits one terminal `EVOLUTION_QUEUED` edge only for a monster that levelled during the battle; pure `ZM_Evolve()` performs the later mutation. Item/stone evolution -> deferred to S9. Trade evolution -> OUT (Scope.md: no trading). Friendship -> not modeled. **No generic trigger-type field or evolution-ref schema was added in box 4**; S9 will add its concrete item-trigger data/API additively when that contract is implemented.

**Cost if wrong:** LOW-to-MODERATE. The architecture is localized and later trigger paths remain additive, but changing move-overflow behavior or reserving a pending event requires updating the dedicated box-4 tests and affected exact-stream goldens.

**Status:** asked 2026-07-12. The implementation and full local gate are complete under the best-guess rulings; the final contract is recorded in ZM-D-043 in this change. The question remains OPEN for user override; either alternative remains additive.

---

### [OPEN] Q-2026-07-12-001 -- ZM_ValidateEventStream rule 7 rejects WEATHER_DAMAGE->FAINT and ability-chip->FAINT

**Question:** should the `ZM_ValidateEventStream` test helper's rule 7 (a FAINT must be preceded by a recognized damage-source event) be widened to also accept `WEATHER_DAMAGE` and `ABILITY_TRIGGER` (ability chip) as valid pre-FAINT sources, alongside the current DAMAGE_DEALT / STATUS_DAMAGE / RECOIL?

**Context:** surfaced during S2 box-3 SC5 (ZM-D-042). Rule 7 only accepts DAMAGE_DEALT/STATUS_DAMAGE/RECOIL before a FAINT, so a legitimate full-battle stream where a mon is KO'd by a **weather chip** (`WEATHER_DAMAGE`->`FAINT`) or by an **AFTERSHOCK/THORNMAIL contact chip** (`ABILITY_TRIGGER`->`FAINT`) would fail validation. This is a PRE-EXISTING gap (the weather chip and Thornmail predate SC5), not introduced here. SC5's tests worked around it: the AFTERSHOCK lethal-chip case is routed through the executor unvalidated (the same approach SC4's Thornmail lethal test used), and the 2,000-battle ability soak excludes SAND/SNOW callers + uses a non-contact finisher so validation stays green.

**Best-guess action taken:** left the helper as-is and worked around it for SC5 -- there is no production or golden-stream risk (the abilities are correct and separately tested). The fix is a small, isolated widening of rule 7, best done as its own commit so the diff is one helper wide; flagged here rather than bundled into the SC5 commit.

**Cost if wrong:** low. The gap only limits which battles can be fed through the optional stream validator in tests; it does not affect production behavior or any shipped golden. If left unfixed, future full-battle tests involving weather-chip or ability-chip KOs must remember to bypass the validator (a latent footgun), but nothing breaks silently.

**Status:** asked 2026-07-12. Non-blocking; a good small standalone task before the content-heavy stages.

---

### [OPEN] Q-2026-07-09-002 -- Terrain bake time for ~25 terrains is an unmeasured estimate

**Question:** is the ~25-terrain plan (one terrain set per outdoor scene via engine change E1) affordable in bake time and file volume?

**Context:** plan risk #1. A full 64x64 chunk export is ~12k files and minutes-to-hours PER terrain; E2's rect export (routes ~16x24 chunks, towns ~16x16) shrinks the projection to ~25k files total and an estimated 20-40 min cold bake -- but that number is a paper estimate, not a measurement.

**Best-guess action taken:** commit to nothing until measured. S3 includes an explicit task: bake 3 real scenes (Home Village + 2 more recipes) and extrapolate before authoring the remaining ~22 terrain recipes. **One terrain set per outdoor scene/route is a hard requirement, not negotiable** (user directive 2026-07-11) -- shared terrain sheets across routes are OUT OF SCOPE as a fallback. If measurement shows bakes are too slow, the fallback is to optimize the bake pipeline itself (parallelize chunk export across cores/processes, cache/incrementalize unchanged chunks, profile and cut the actual hot path) rather than reduce terrain-set count.

**Cost if wrong:** low-to-moderate if measured at S3 as planned (an optimization pass is scoped work, not a redesign); HIGH if ignored until S9/S10 (a 25x slow bake would poison every tools boot and CI-adjacent workflow during the content stages).

**Status:** asked 2026-07-09. Measurement lands at S3 (see Roadmap.md).

---

### [OPEN] Q-2026-07-09-003 -- Battle-scene visual isolation at the (0,-2000,0) offset is asserted, not yet proven

**Question:** does the additive battle scene at world offset (0,-2000,0) actually render with zero overworld bleed-through?

**Context:** the overworld<->battle transition is an ADDITIVE scene load + `SetScenePaused` on the overworld (a SINGLE reload would reset render systems + physics and re-stream terrain twice per encounter -- seconds of hitch at encounter frequency, disqualifying). The offset puts the arena outside grass LOD rings (200 m max) and terrain high-LOD streaming (1000 m), and the arena has an enclosing backdrop dome. But global render features (skybox, fog, IBL, shadows) are not per-scene, so isolation is asserted by design reasoning, not yet by pixels.

**Best-guess action taken:** proceed with the additive design; the S5 gate includes a dedicated screenshot check for overworld bleed-through at the offset. Documented fallback: SINGLE load + world-state snapshot. Contingency beyond that (only if needed): a per-scene render visibility toggle in the engine.

**Cost if wrong:** moderate. Falling back to SINGLE + snapshot re-introduces the transition hitch and adds a world-state snapshot/restore surface to test -- but the battle engine, director, HUD, and encounter logic are all transition-agnostic, so the rework is confined to the transition layer.

**Status:** asked 2026-07-09. Verified or falsified at the S5 screenshot gate (see Roadmap.md).

### [OPEN] Q-2026-07-10-004 -- Unit-test verification gap in `zenith test` + baseline-ratchet churn

**Question:** `zenith test Zenithmon --headless` (the loop's verify command) passes
`--skip-unit-tests`, so it does NOT run the `ZM_*` unit suite -- only the new CI
boot step (ZM-D-019) and a direct exe boot do. Should the CLI grow a
`zenith test --unit-tests` flag (or run units by default for a single game), and
should the CI unit gate keep the exact-count baseline or switch to a failures-only
check?

**Context:** found while landing S1's first unit tests. The exact-count baseline
(1079) couples zm-tests to the engine unit count -- an unrelated engine PR that
changes that count reddens zm-tests until the baseline is bumped. A failures-only
check (assert the "Unit tests complete" line shows 0 failed, ignore the count)
avoids the coupling but no longer catches a silently-vanishing `ZM_` test.

**Best-guess action taken:** kept the exact-baseline ratchet (matches engine-gate,
strongest guarantee) and verify unit tests locally via a direct exe boot
(`--list-automated-tests --headless` without `--skip-unit-tests`, or
`Tools/run_unit_gate.ps1`) until a CLI flag lands. Bump rule documented in
CIPolicy.md section 1.

**Cost of getting it wrong:** low. Both are small, localized changes; the ratchet's
only symptom is an occasional one-line baseline bump caught immediately by red CI.

**Status:** asked 2026-07-10; acting on best guess.

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
