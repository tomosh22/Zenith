# Zenithmon Questions -- Async Communication with the User

**Purpose:** each entry is a question or decision-point an autonomous agent wants the user's input on. The agent makes a best-guess and PROCEEDS; the user corrects in batch.

**Format:** append-only during a session; the user (or a future agent on their behalf) moves resolved items to the "Resolved" section at the bottom. Entry template: id (`Q-YYYY-MM-DD-NNN`), question, context, best-guess action taken, cost-if-wrong, status.

**Triage markers (plain ASCII):** `[OPEN]` = waiting for user; `[RESOLVED]` = answered/closed; `[STALE]` = dropped.

---

## Open

### [OPEN] Q-2026-07-21-001 -- ENGINE: terrain sets up GPU culling resources with no headless guard

**Question:** should `Zenith_TerrainComponent::InitializeCullingResources()` (and the unified terrain buffer upload around it) skip GPU-driven culling setup when the engine is running headless?

**Context:** found 2026-07-21 while restoring the RenderTest canary (Q-2026-07-16-001). With RenderTest's terrain fully baked -- 12,313 files, no missing chunks, clean boot -- `zenith test RenderTest --headless` still dies on `[Core] Assertion failed: Invalid buffer VRAM handle`, immediately after `Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling with LOD support`. A headless run short-circuits Flux init and has no Vulkan device, so the buffers it asks for have no VRAM handle. This is why the original Q's "it crashes because the terrain is absent" premise was only half right.

**Consequence today -- and this is CRASH-CLASS, not a logged degradation.** `Zenith_Assert` is not a warning: it calls `Zenith_DebugBreak()`, which on Windows is `__debugbreak()` (`Zenith/Windows/Zenith_Windows_DebugBreak.cpp`). With no debugger attached that raises an unhandled breakpoint exception and TERMINATES the process. Proven, not inferred: the harness records `FAIL exit=-2147483645` for these runs, and -2147483645 is 0x80000003 = STATUS_BREAKPOINT. And because `ZENITH_ASSERT` is `#define`d unconditionally (`Zenith/Core/Zenith.h:138`, one line above its own `#ifdef`, making the no-assert `#else` branch unreachable), it terminates in EVERY config including Release -- there is no build in which this degrades gracefully.

So: any terrain-bearing scene is not merely "unsupported" headless, it hard-kills the process. RenderTest can therefore only ever be a WINDOWED canary, and the terrain path has no CI coverage on a GPU-less runner. Player impact is nil (shipping builds are not headless); the cost is entirely to testability.

**Checked and clear:** no assert fires on any currently-green path -- a full `zenith test Zenithmon --headless` run (35/0) and the windowed `TerrainEditorSmoke` both log ZERO "Assertion failed" lines. The assert-capture hook (`Zenith_AssertCaptureScope`) is RAII-scoped to tests that deliberately trigger asserts, so it is not masking anything globally. This is the only assert firing anywhere in the suite.

**Best-guess action taken:** none -- left untouched and logged. It is an engine change in the Flux/terrain streaming path, which is a materially larger blast radius than the ECS registry fix landed alongside it, and it was not what the user asked for. The canary works windowed, which was the goal.

**Cost if wrong:** low-to-moderate. The terrain path stays headless-hostile, so terrain regressions can only ever be caught by a windowed run on a GPU machine. If a future engine change wants headless terrain coverage this has to be fixed first.

**Status:** asked 2026-07-21. Not blocking; a well-scoped standalone ENGINE task with a clear repro (`zenith test RenderTest --headless`).

---

### [OPEN] Q-2026-07-21-002 -- RenderTest `RT_TennisDeterminismDigest` fails windowed

**Question:** is `RT_TennisDeterminismDigest`'s failure a real determinism regression, or a stale golden digest?

**Context:** with the terrain baked and RenderTest finally runnable, the windowed suite is **8 passed / 1 failed**. The one failure is `RT_TennisDeterminismDigest` (2490 frames, 81 s, and an EMPTY `failures` array in its result JSON, which is itself odd -- it fails without recording why). It is unaffected by, and predates, both the terrain bake and the ZM-D-132 engine change; nothing in this work touches tennis.

**Best-guess action taken:** none -- reported rather than chased. It was outside the requested scope, and a determinism digest wants deliberate investigation (is the digest stale, or is the sim genuinely non-deterministic?) rather than a quick patch.

**Cost if wrong:** low while RenderTest is used as a terrain/grass canary (`TerrainEditorSmoke` is the relevant test and it passes). Moderate if RenderTest is ever promoted to a full required gate, since one red test would mask others.

**Status:** asked 2026-07-21. Non-blocking.

---

### [OPEN] Q-2026-07-20-001 -- S6 item 3 defers BOTH behaviour graphs and the navmesh to S7 (mechanism change, not a scope change)

**Question:** the Roadmap line for S6 item 3 reads *"`ZM_Interactable` + NPC graphs via `ZM_GraphAuthoring`; `ZM_NpcWalker` (navmesh wanderers)"*. The decomposition pass ruled that **both** named mechanisms are deferred to S7, while the item's actual payload -- a player walks up to an NPC, presses E, and gets dialogue / a shop / a Care Center heal -- ships in full at S6. Surfaced for ratification or override, because the shipped code will not match the Roadmap's literal wording until SC9 amends it.

**Best-guess rulings taken (implementation PROCEEDS on these -- see ZM-D-123 for the full evidence):**

1. **Behaviour graphs deferred; interact dispatch is plain C++.** Zenithmon has zero lines of graph code today; the payload is `NPC role -> one of three already-shipped raise statics`, i.e. a closed 3-way enum that would cost ~300-700 lines of node classes plus a registration hook to express as a graph, with no designer to benefit. A `.bgraph` would also add a silent-failure axis: `Assets/` is git-ignored, so a cold checkout resolves the graph slot *unresolved* -- no crash, no log, a mute NPC. ZM-D-010 already scopes graphs to "glue only (menu flow, NPC scripted events, cutscene beats)", and a fixed 3-way dispatch is not that. Graphs earn their keep at the S7 trainer beat (sight -> freeze -> approach -> dialogue -> battle), which genuinely branches and carries state.
   **Cost if wrong:** S7 pays the first-graph tax plus converting a 3-arm switch into node dispatch -- roughly one additive sub-commit. Every existing test is unaffected, because they assert screen state rather than how the raise happened. A single `Interact()` dispatch function is kept as the latent seam.

2. **Navmesh deferred; the wanderer is a deterministic 2-waypoint patrol.** **(Justification CORRECTED 2026-07-20 -- the original was wrong; see ZM-D-123 ruling B.)** To be explicit, because the first draft of this entry said otherwise: **the Zenith navmesh generator IS terrain-capable.** `Zenith_NavMeshGenerator::GenerateFromGeometry` takes arbitrary triangle geometry and carries a walkable-slope filter (`m_fMaxSlope = 45.0f`, `IsWalkableSlope`), which exists precisely because it is designed for non-flat ground. What cannot handle terrain is the convenience scene-scraper `Zenith_AINavGeometry::GenerateFromScene`, which models every static collider as a **box OBB** and never reads a mesh or heightfield -- so it is the wrong tool for terrain, while being exactly right for DevilsPlayground's box-collider levels.
   The actual reason to defer is **scope, not capability**: using the navmesh here means sourcing terrain triangles to feed `GenerateFromGeometry`, and no terrain-geometry accessor has been identified yet; there is also a grid-coverage question at Dawnmere's 1024 m domain (the generator truncates its voxel grid at 1024 cells per axis, ~307 m at the default 0.3 m cell). That is unscoped work of unknown size, and **the S6 gate contains no clause that NPC motion helps pass** -- a stationary-plus-patrolling cast meets every criterion. Deferring costs no engine change and no cross-game regression. Note `.znavmesh` file I/O has zero in-repo callers, so persistence would be first-of-its-kind work too.
   **Cost if wrong:** ~120 lines of pure state machine plus ~15 units are superseded and deleted at S7. The `ZM_Interactable` interface, the NPC data table, all four walk-up tests and the consolidated gate are unaffected, because the walker sits behind `SetWanderEnabled(bool)` and a per-frame velocity write.

**Why the loop did not stop for this:** `Scope.md` Section 4 governs what SHIPS -- "NPC wanderers exist and are interactable" -- and that is unchanged; only the mechanism differs. Both rulings are logged with their engine evidence and both are additive to reverse.

**Status:** asked 2026-07-20 at the start of S6 item 3. Implementation proceeds under these rulings. If the user overrides either, the correction lands as its own additive S7 sub-commit rather than a rework.

---

### [OPEN] Q-2026-07-18-001 -- S5 item 5 scoping rulings (persistent GameState + catch/exp/faint/whiteout)

**Question:** five FOUNDATIONAL scope boundaries for S5 item 5 ("Catch / exp / faint / whiteout applied to GameState"), from the decomposition pass (durable plan at `Build/artifacts/zm_s5_item5_plan/plan.md`). Item 5 introduces the FIRST persistent player GameState/party (item 4 used a discarded placeholder), so these calls set the shape a lot of later work builds on -- surfaced for ratification/override before the persistence layer is authored on them.

**Context:** item 5 is almost pure WIRING -- every battle rule seam shipped in S2 (the engine awards exp when `m_bAwardExp` flips, `DoItemAction` already rolls the catch + emits `CATCH_RESULT`; its own comment says "the party-add is box 4/5" = item 5). No `ZM_Party`/`ZM_GameState`/persistent `ZM_Monster`/whiteout code exists yet (grep-verified). S6 owns party/bag/dex UI; S7 owns the full versioned `ZM_SaveSchema` + migration -- so item 5 must NOT pull those forward.

**Best-guess actions taken (plan proceeds on these unless corrected):**
1. **Single-LEAD battle; defer switching.** Only the party lead enters battle; multi-member battles + forced-switch-on-faint + the Party UI defer to S6+. Whiteout = "lose the (single-mon) battle." **Cost if wrong: MODERATE** -- adds ~2 SCs for a `ZM_BattleDirectorCore` FORCED_SWITCH state (the engine primitives already exist), but no data-model change.
2. **Initial party = one fixed test starter** (Fernfawn L5). No lab/starter-choice until S8, so item 5's party is seeded at boot. Cost if wrong: trivial (one seed line).
3. **GameState in-memory ONLY** -- no `ZM_SaveSchema` disk fields yet (S7 owns disk + the migration gate); SaveFormat.md gets a "not yet serialized" note (like the S3 traversal-stream note). Cost if wrong: low -- S7 adds the Read/Write; item 5's in-memory shape is the source of truth.
4. **Catch UX = a direct "Catch" root-menu button + a fixed default ball** (submits `{ITEM, ball}`); no Bag UI (S6 `ZM_UI_Bag`). Cost if wrong: low/additive (the Bag menu can grow it).
5. **Whiteout -> Dawnmere (build 2, spawn tag "TownCenter")**, party healed full; no Care Center until S6/S8. Cost if wrong: low (one warp target).

**Cost if wrong:** low-moderate overall -- each is localized; only #1 (single-lead) would add SCs, and even that reuses existing engine primitives. None touch the shipped battle engine goldens.

**Status:** asked 2026-07-18; the loop will PROCEED on these best-guesses (Questions.md protocol) unless the user overrides. The item-5 plan's SC1 (the pure `ZM_Party`/`ZM_Monster`/`ZM_GameState` data model) is needed under ALL of these rulings, so it is safe to author first regardless.

### [OPEN] Q-2026-07-17-002 -- S5 item 4 scoping rulings (BattleDirector + BattleHUD)

**Question:** four scope boundaries for S5 item 4 (`ZM_BattleDirector` + `ZM_UI_BattleHUD` + E3 typewriter), from the planning pass (plan at `Build/artifacts/zm_s5_item4_plan/plan.md`): (a) player-team source, (b) is exp awarded/presented in item 4 or item 5, (c) action-menu subset, (d) wild-battle enemy AI tier.

**Context:** there is NO persistent player party / GameState in the codebase yet (verified -- no `ZM_GameState`/`ZM_Party`). Item 5 is "Catch / exp / faint / whiteout applied to GameState"; S6 owns the general menu/dialogue/party/bag/dex/shop UI. Item 4 must present a battle to resolution without pulling item-5 or S6 scope forward.

**Best-guess action taken (proceeding on these unless corrected):** (a) the director synthesizes a deterministic **placeholder** player team, discarded on battle unload -- item 5/S7 swaps in a real GameState read; (b) `m_bAwardExp` stays **OFF** in item 4 (item 4 = present the battle; item 5 = turn exp/catch write-back on), but the EXP/LEVEL/CATCH events are mapped for totality so item 5 flips one flag; (c) action menu = **Fight + Run only** in item 4; Bag/ball-throw + party/switch (and `m_bCanCatch`) defer to item 5 where capture is meaningful and the S5 catch gate lives; (d) wild enemy AI tier = **GREEDY** (more believable than RANDOM; one-line change if you prefer RANDOM's mainline-faithfulness).

**Cost if wrong:** low-moderate. Each is a localized change: (a) one placeholder builder; (b) one config flag + already-mapped events; (c) additional menu buttons the SC5 menu machine can grow; (d) one enum. None touch save format or the battle engine.

**Status:** asked 2026-07-17; acting on best guess. Plus 3 lower-stakes item-4 questions in the plan (creature-anim depth, text-line pacing/confirm-to-advance, whether the catch test needs item-4 HUD surface) deferred to their SCs.

### [OPEN] Q-2026-07-17-001 -- `ZM_BattleTransition::BiomeForScene` is a hard-coded table, not a `ZM_WorldSpec` column

**Question:** should the source-scene -> battle-arena-biome mapping live as a `ZM_BATTLE_BIOME m_eBattleBiome` column on `ZM_WorldSpec`, rather than the hard-coded `BiomeForScene` switch item 3 introduced?

**Context:** item 3 (SC4) needed a deterministic, unit-testable biome input for `ZM_BattleArena::SetBiome`. `ZM_WorldSpec` has no biome column (its rows are id/name/build-index/kind/terrain-set/connections/spawn-tags/encounters/rate and nothing else), so `ZM_BattleTransition::BiomeForScene(ZM_SCENE_ID)` is a component-local lookup table -- currently every registered scene maps to `ZM_BATTLE_BIOME_MEADOW` (the default). This is an INVENTION, not an authored data source; it works for the S5 gate (only DAWNMERE->MEADOW is exercised) but does not scale to the ~40-scene world.

**Best-guess action taken:** ship the hard-coded table for S5; propose adding `m_eBattleBiome` to `ZM_WorldSpec` at S9/S10 when the full world is authored, and delete the table then. Not a blocker.

**Cost if wrong:** low. Moving the mapping to a WorldSpec column later is a mechanical refactor confined to `BiomeForScene` + the world table; no save-format or battle-engine surface depends on it.

**Status:** asked 2026-07-17; acting on best guess.

### [OPEN] Q-2026-07-12-005 -- ZM_BattleTower: defaulted tuning + a future-integration note

**Question:** ratify (or override) the defaulted rulings for the S2 box-6 SC2 `ZM_BattleTower` logic. Battle Tower LOGIC is a Roadmap box-6 item (in-scope); all of these are additive/retunable, so I proceeded rather than halting.

**Best-guess actions taken:**
1. **Numeric tuning = S11-tunable defaults** (the GDD fixes only "boss every 7th"): tier bands at streak 7/21/35 (RANDOM->GREEDY->SMART->CHAMPION), one-tier boss bump, team size 3, rarity ceiling bands COMMON / UNCOMMON(>=7) / RARE(>=21). Cost-if-wrong: retune named constants, zero API/golden impact.
2. **Tower battles award no EXP** (`ZM_MakeTowerBattleConfig` sets `awardExp=false`) and generated opponents use ability NONE (the reduced data has no per-species ability). Cost-if-wrong: low/additive.
3. **Procedural-by-seed opponent teams from the existing dex** (no bespoke tower species table) -- a ~150-row tower pool is S11 content, not box-6 logic. Cost-if-wrong: low; a data table can later replace the procedural path behind the same `ZM_GenerateTowerTeam` signature.

**Related future-integration note (tracked, not itself a question):** `ZM_ChooseAction` (box 5) returns a MOVE for a FAINTED active (it enumerates the fainted active's moves), so when the S5+ battle integration wires the enemy side to the AI it must submit the forced SWITCH itself on a faint rather than delegating that turn to `ZM_ChooseAction` (or box 5's chooser should later special-case a fainted active). The box-6 tower engine smokes side-step this with a controlled 1v1. No production bug today (the tower is a pure logic layer that never runs battles).

**Cost if wrong:** LOW across all -- each is additive/retunable.

**Status:** asked 2026-07-12. Implementation + full local gate complete (ZM-D-046). OPEN for user override.

---

### [RESOLVED] Q-2026-07-12-004 -- ZM_Breeding: reduced model on the shipped data (no gender / egg groups / egg moves)

**Resolution (2026-07-12, ZM-D-048):** the user directed FULL mainline breeding + gender. The reductions are removed (they under-delivered against Scope.md Section 1's "breeding/eggs/daycare" + "mainline mechanics only"). Now implementing across SC-A (gender foundation, LANDED) / SC-B (egg groups + GLOOPET Ditto + gendered compatibility) / SC-C (egg moves + ability/hidden-ability inheritance + hatch-cycle data). Confirmed boundary decisions: Ditto=GLOOPET; hidden abilities IN; shiny/Masuda deferred to S5+; hatch-cycle data now (step-driving at S9). See ZM-D-048.

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

### [OPEN] Q-2026-07-10-004 -- Unit-test verification gap in `zenith test` + baseline-ratchet churn

**Question:** `zenith test Zenithmon --headless` (the loop's verify command) passes
`--skip-unit-tests`, so it does NOT run the `ZM_*` unit suite -- only the new CI
boot step (ZM-D-019) and a direct exe boot do. Should the CLI grow a
`zenith test --unit-tests` flag (or run units by default for a single game), and
should the CI unit gate keep the exact-count baseline or switch to a failures-only
check?

**Context:** found while landing S1's first unit tests. The exact-count baseline
(1079 at the time of asking; **2319 today**) couples zm-tests to the engine unit count -- an unrelated engine PR that
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

### [RESOLVED] Q-2026-07-16-001 -- RenderTest is pre-existingly red in this checkout (missing baked terrain); the E5 grass canary was substituted

**Question:** the AgentBriefing 6.4 engine-change gate names "RenderTest still boots green" as the terrain/grass canary, but `zenith test RenderTest --headless` CRASHES in this checkout (`[Core] Assertion failed: Invalid buffer VRAM handle`, "127 missing/invalid terrain chunks") because RenderTest's baked terrain is absent (git-ignored `Assets/`, never `_True`-baked here). It crashes during terrain streaming BEFORE grass generates. Should a future session `_True`-bake RenderTest's terrain (seed 1337) so the RenderTest engine-change canary works locally again?

**Best-guess action taken (E5 / ZM-D-092):** proceeded. Proved the RenderTest crash is ORTHOGONAL to E5 via a stash-revert diagnostic (RenderTest crashes IDENTICALLY, 0/10 same assertion, with E5 reverted), and validated E5's grass behaviour instead via Zenithmon's windowed `ZM_GrassRegeneration_Test` (a more direct E5 canary -- it exercises the exact grass clear->regenerate-on-scene-load cycle E5 changes) plus Combat 1081/0 (the official engine-gate canary), DP 158/0, CityBuilder 45/0.

**Cost if wrong:** LOW. E5 is thoroughly validated by other games + the grass-regen windowed test + engine units; the RenderTest gap is an environment issue, not a code risk. A future engine change touching terrain/grass would benefit from a locally-baked RenderTest, so this is worth a one-time `_True` bake when convenient (or a RequestSkip hardening so RenderTest degrades gracefully on absent terrain like DP does).

**Resolution (2026-07-21, ZM-D-132): BAKED, canary restored -- and the original premise was only HALF right.**

RenderTest's terrain was `_True`-baked (it generates procedurally at boot from seed 1337 via tools-only editor automation): **12,313 files / 1.78 GB** under `Games/RenderTest/Assets/Terrain/`. That fixed the boot path -- the `127 missing/invalid terrain chunks` warning and the boot-time crash are gone, `--list-automated-tests` boots clean, and **`TerrainEditorSmoke` (the actual terrain/grass canary) PASSES windowed.** It was then used as a real canary for the ZM-D-132 engine change and stayed green.

**★ The premise correction, recorded so nobody re-inherits it.** This Q said RenderTest "CRASHES ... because RenderTest's baked terrain is absent". The bake was NECESSARY but NOT SUFFICIENT: with the terrain fully baked, `zenith test RenderTest --headless` still asserts `Invalid buffer VRAM handle`, and the measured stack is `Zenith_TerrainComponent::InitializeCullingResources() - Setting up GPU-driven terrain culling` -- i.e. the terrain sets up GPU-driven culling resources unconditionally, with no headless guard, and there is no Vulkan device in a headless run. **That is a separate, still-open ENGINE gap, not an asset problem.** It does not block the canary, because the canary is run WINDOWED (as every local gate in this project is).

**Residual, unrelated:** RenderTest windowed is **8 passed / 1 failed** -- `RT_TennisDeterminismDigest` fails (2490 frames, empty `failures` array) for pre-existing tennis-determinism reasons that predate and are unaffected by this work. Not chased here.

**Status:** RESOLVED 2026-07-21 for the terrain bake + canary. Two follow-ups spun out: the headless terrain-culling guard, and `RT_TennisDeterminismDigest`.

---

---


### [RESOLVED] Q-2026-07-20-002 -- engine gap: `Zenith_ComponentMetaRegistry::Finalize()` has no duplicate-serialization-order detection

**Question:** should `Zenith_ComponentMetaRegistry::Finalize()` warn (or assert) when two components are registered at the SAME `m_uSerializationOrder`?

**Context:** found by review during S6 item 3 SC7 (2026-07-20). `Finalize()` builds its sorted meta list and sorts by `m_uSerializationOrder`, logging each entry, but performs **no duplicate detection at all**. Two components sharing an order therefore sort arbitrarily against each other -- a silent, potentially build-dependent ordering that affects serialization. Component orders are a manually-assigned global namespace (engine components sit at <= 95, AI at 90, Zenithmon at 100-113 with 114 next free), so a copy-paste collision is an easy mistake to make and an invisible one to have made.

**Why this was NOT fixed in the bug-fix pass:** it is an **engine (`Zenith/`) change**. Every engine change moves the engine unit baseline (currently 1097) and owes a cross-game regression run (Combat / DevilsPlayground / CityBuilder) plus a RenderTest boot check, per the AgentBriefing engine-change gate. That is a materially different task with a different gate, and bundling it into a Zenithmon game-only fix pass would have made the diff impossible to gate cleanly.

**Best-guess action taken:** left the engine untouched and defended Zenithmon game-side instead -- `GateRoster_InteractableIsRegisteredExactlyOnce` (Tests/ZM_Tests_Interactable.cpp) asserts that exactly ONE registered meta sits at `ZM_Interactable`'s order, so a future collision with order 113 specifically is caught at boot. That is a spot check, not the general fix.

**Cost if wrong:** low today, rising with component count. Nothing currently collides (verified: no engine component sits at 113). The risk is a future collision landing silently and only surfacing as a confusing serialization bug. The fix itself is small -- a duplicate scan in `Finalize()` with a `Zenith_Warning` (or an assert in debug) -- but it should land as its own engine commit with the full cross-game gate.

**Resolution (2026-07-21, ZM-D-132): FIXED.** `Finalize()` now tie-breaks its sort on the type name (so a collision is at least DETERMINISTIC -- previously `std::sort` is unstable over a hash-map walk, so the ordering under collision was not even reproducible between builds), logs a `Zenith_Error` per colliding pair naming both components and the shared order, and emits a summary `Zenith_Check`. Six units land in `Zenith/EntityComponent/Zenith_ComponentMetaRegistry.Tests.inl` -- five over the new pure `CountDuplicateSerializationOrders`, plus `DuplicateOrders_LiveRegistryHasNoCollisions`, which runs the check against the ACTUAL registry this build ships and is what turns a future copy-pasted order into a boot-time unit failure in every game.

**`Zenith_Check`, not `Zenith_Assert`, for two verified reasons:** (1) `ZENITH_ASSERT` is `#define`d UNCONDITIONALLY one line above its own `#ifdef` in `Zenith/Core/Zenith.h`, so `Zenith_Assert` calls `Zenith_DebugBreak()` in EVERY config including Release -- hard-breaking a shipped player's game over a developer's numbering mistake is exactly what the check tier exists to avoid; (2) `Finalize()` runs at engine init, BEFORE the boot unit suite, so an assert would pre-empt the very unit written to catch this and it could never report. Both were measured, not assumed -- an earlier draft of this fix carried the opposite (false) claim in a comment.

**Engine baseline moved 1097 -> 1103** (+6, exactly these units). Cross-game gate: Combat 1103/0 + suite 14/0, DevilsPlayground 1104/0 + suite 158/0, CityBuilder 1104/0 + suite 45/0, Zenithmon 2325/0 with 35/0 headless and 35/0 windowed, RenderTest terrain canary green. Mutation-verified: giving `ZM_Interactable` the same order as `ZM_UI_MenuStack` logs the named pair and reds the live-registry unit while boot continues.

**Status:** RESOLVED 2026-07-21 (ZM-D-132).

---

---


### [RESOLVED] Q-2026-07-09-003 -- Battle-scene visual isolation at the (0,-2000,0) offset is asserted, not yet proven

**Question:** does the additive battle scene at world offset (0,-2000,0) actually render with zero overworld bleed-through?

**Context:** the overworld<->battle transition is an ADDITIVE scene load + `SetScenePaused` on the overworld (a SINGLE reload would reset render systems + physics and re-stream terrain twice per encounter -- seconds of hitch at encounter frequency, disqualifying). The offset puts the arena outside grass LOD rings (200 m max) and terrain high-LOD streaming (1000 m), and the arena has an enclosing backdrop dome. But global render features (skybox, fog, IBL, shadows) are not per-scene, so isolation is asserted by design reasoning, not yet by pixels.

**Best-guess action taken:** proceed with the additive design; the S5 gate includes a dedicated screenshot check for overworld bleed-through at the offset. Documented fallback: SINGLE load + world-state snapshot. Contingency beyond that (only if needed): a per-scene render visibility toggle in the engine. **Update (2026-07-17, item 3 SC5):** the additive battle path now CLEARS the overworld grass on entry (`ZM_BattleRoundTrip_Test` asserts `Grass().GetGeneratedInstanceCount() == 0` at IN_BATTLE) and regenerates it on resume, which strictly REDUCES the bleed-through surface (the tallest, most visible overworld geometry is gone during the battle). Still not pixel-proven -- global render features (skybox/fog/IBL/shadows) remain per-frame, not per-scene -- so this Q stays OPEN for the S5 SCREENSHOT gate.

**Cost if wrong:** moderate. Falling back to SINGLE + snapshot re-introduces the transition hitch and adds a world-state snapshot/restore surface to test -- but the battle engine, director, HUD, and encounter logic are all transition-agnostic, so the rework is confined to the transition layer.

**Resolution (2026-07-18, ZM-D-112):** VERIFIED. The user reviewed the captured overworld/battle frames at the S5 screenshot gate and **APPROVED** -- there is no visible bleed-through at the offset, so the additive design stands and the documented SINGLE + world-state-snapshot fallback was not needed (it remains available if a future change reintroduces bleed). The S5 stage gate was signed off on that evidence.

**Status:** RESOLVED 2026-07-18 by the user's visual sign-off (ZM-D-112).

---


### [RESOLVED] Q-2026-07-09-001 -- Branch protection for `zm-tests`

**Resolution (2026-07-10):** the user directed the agent to do it ("Add
zm-tests yourself"). Discovery: master had NO branch protection and no
rulesets at all -- the required-checks discipline had been purely
conventional. Classic branch protection was created via
`gh api PUT .../branches/master/protection` with required contexts
`[zm-tests]`, `strict=false`, `enforce_admins=false` (owner direct pushes
bypass; agent PRs are always gated). Full shape + consequences: CIPolicy.md
section 4; checklist item ticked in ManualSetupChecklist.md.

---

### [RESOLVED] Q-2026-07-09-002 -- Terrain bake time for ~25 terrains is an unmeasured estimate

**Resolution (2026-07-13, ZM-D-054):** the three-real-recipe study is complete
and supports continuing with one terrain set per outdoor scene/route. Calibrated
same-harness cold wall times were Dawnmere **59.035 s**, Thornacre **69.979 s**,
and Route1 **80.804 s**; their internal recipe timers were **42.588 s**,
**53.657 s**, and **64.541 s**. The all-warm baseline was **16.874 s** and
queued zero terrain recipes. The measured families total 896 chunks, **2,700 files**,
and **672,354,172 bytes**.

The 11-town + 14-route planning model projects **24,676 terrain-family files /
5,933,328,436 bytes** (5.933 GB / 5.526 GiB), a conservative repeated-process
wall of **30m 40.833s**, and a one-boot/net estimate of **23m 55.857s**. The
GDD's exact 11-town + 15-route outdoor count is retained as a sensitivity:
**25,832 files / 6,196,314,376 bytes** (6.196 GB / 5.771 GiB), **32m 01.637s**
repeated and **24m 59.787s** net. Both conservative terrain projections sit
within the documented 30-50 minute full-cold range, so no bake-pipeline
optimization is required before the next S3 task.

**Question:** is the ~25-terrain plan (one terrain set per outdoor scene via
engine change E1) affordable in bake time and file volume?

**Original context/action:** a full 64x64 chunk export is ~12k files and
minutes-to-hours per terrain. E2's rect export (routes ~16x24, towns ~16x16)
was expected to reduce this to ~25k files and 20-40 minutes. Dawnmere's first
standalone bake measured **63.671 s** with a **14.614 s** warm graphics boot;
that historical ZM-D-053 observation remains valid and is distinct from the
calibrated 59.035-second same-harness rerun above. The chosen action was to
measure two more real recipes before scale-up. One set per outdoor scene/route
remains mandatory; if later full-project measurements are too slow, optimize
the pipeline rather than share terrain sets.

**Acceptance cautions:** the 30-50 minute budget is for the eventual
**all-assets** cold bake, not terrain alone; the other asset generators remain
unmeasured. There is no explicit byte-volume cap, and "seconds" for warm boot
is qualitative rather than a numeric SLA. The conservative repeated-process
figure is a planning bound, not a statistical confidence bound: the sample has
two towns but only one route and assumes later recipes use the same 16x16 /
16x24 crop classes. The `~25` plan and GDD's exact 26 outdoor scenes are both
shown above rather than silently choosing one.

**Cost if wrong:** low-to-moderate while the remaining recipes are still
unbuilt; remeasure representative recipes and optimize the bake hot path if
future content or the full all-assets bake exceeds budget. Shared terrain
sheets remain out of scope.

**Status:** RESOLVED 2026-07-13 by the completed three-recipe measurement and
projection (ZM-D-054).
