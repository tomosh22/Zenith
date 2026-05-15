# DP Status

**Last updated:** 2026-05-15 — Phase 1 + Phase 2 substantively complete; Phase 4 loss-state UI shipped. Auto-loop sweep landed PRs #50 through #79 in <24 h. Master HEAD `0e67f13e`. Full suite **~110 PASSED, 0 FAILED** locally via `Tools/run_dp_tests.ps1 -Headless`.
**Build:** ✅ DP target builds clean (`vs2022_Debug_Win64_True`, 0 warnings, 0 errors).
**Tests:** Full local suite green. Headless skips graphics-only tests by m_bRequiresGraphics; all compute-only tests pass.

## Manual setup checklist gating

**⚠️ BEFORE STARTING THE FIRST AGENT SESSION,** the user (Tomos) must tick every box in [Docs/ManualSetupChecklist.md](ManualSetupChecklist.md). The orchestrator pattern depends on human-only setup (branch protection rules, Vulkan SDK install, pwsh.exe install, etc.) that no agent can perform. The orchestrator's session-start ritual checks this file and STOPS if any box is unchecked.

## Current state — wave-by-wave

### Phase 1 — every wave shipped

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-1.1** Real pause | ✅ landed | #30 | Pre-session |
| **MVP-1.2** Real navmesh integration | ✅ landed | #39, #40 | Closed-door + regen-on-swap; PriestPursuit stabilised |
| **MVP-1.3.1–4** Apprehend / run-loss | ✅ landed | #41, #42 | Event + BT branch + switch-breaks + out-of-range guards |
| **MVP-1.3.5** Dawn + NoVessels causes | ✅ landed | #54 (NoVessels), #57 (Dawn) | Both run-loss causes now wired + tested end-to-end |
| **MVP-1.4.1–3** Faint state machine | ✅ landed | #55, #60 (coverage gap) | Idle/Possessed/Fainted/Dead villager state; 10 s recovery; system-possess bypass |
| **MVP-1.4.4–5** Drop verb (G) | ✅ landed | #48 | Drop at foot pos + 0.5 s post-drop pickup cooldown |
| **MVP-1.4.6** Drop-pickup chain handoff | ✅ landed | #52 | Movement-sim test pinned the full chain |
| **MVP-1.5** Possession cooldown | ✅ landed | #43 | 1.5 s after voluntary switch; death/apprehend bypass it |
| **MVP-1.6** Demon-scent | ✅ landed | #45 | Table + decay + write to priest BB_KEY_HIGH_SCENT_TARGET |
| **MVP-1.7.1–3** Sprint | ✅ landed | #46 | Shift+move → 12 m/s + 3 s/s extra life cost |
| **MVP-1.7.4–5** Walk-quiet | ✅ landed | #47 | Ctrl+move → 4 m/s + halved footstep loudness |
| **MVP-1.7.6** Aelfric effective hearing halved | ✅ landed | #53 | Walk-quiet villager's footstep emits below priest hearing threshold |
| **MVP-1.8** Possession range | ✅ landed | #44 | 15 m anchor radius; anchor moves on each successful hop |
| **MVP-1.9** Priest omniscience opt-in | ✅ landed | #49 | Fallback gated behind ZENITH_INPUT_SIMULATOR + opt-in flag |
| **MVP-1.10** Save/load run state | ✅ landed | #56 | DP_RunState struct + serialisation + round-trip / corruption / version-mismatch tests |

### Phase 2 — substantively complete

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-2.1.1–2** Devout possession channel + priest interrupt | ✅ landed | #62 | Channel-mid switch breaks; coverage in P2 archetype tests |
| **MVP-2.1.4** Child cannot carry tools | ✅ landed | #59 | Item pickup rejected when villager archetype = Child |
| **MVP-2.1.6** Aelfric ignores Beggar | ✅ landed | #58 | Priest bridge skips Beggar candidates |
| **MVP-2.2.1–3** Reagent registry + per-tag pickup channel | ✅ landed | #64 | Registry-resolved channel duration; OnAwake + SetTag both re-resolve |
| **MVP-2.2.4–5** BogWater evaporates 8 s after drop | ✅ landed | #65 | Time-driven entity destruction |
| **MVP-2.2.6–7** BellSoul rings bell on pickup | ✅ landed | #66, **#76 (mapwide fix)** | Perception emit + new direct-BB fanout (PR #76) so priest hears from anywhere on the map, not just within 30 m hearing range |
| **MVP-2.3.2–3** Forge per-instance recipes + Wood/Spike tags | ✅ landed | #63 | Each forge entity owns a recipe; Iron→Key default, Wood→Spike new |
| **MVP-2.3.4** Forge audible across the village | ✅ landed | #67, #73 (perception emit fix) | Caught bug: forge originally only called AudioBus, not PerceptionSystem; PR #73 wired the perception emit alongside |
| **MVP-2.4.3–4** Fog GDD-contract regression guards | ✅ landed | #61 | Aelfric not revealed; dim lights cut fog holes |
| **MVP-2.4.5** Memory-fog state machine | ✅ landed | #68, #72 (integration) | Sparse hash map keyed by 1m grid cell; visible -> dim -> faded states |
| **MVP-2.5.1, 2.5.3** HUD whisper line + Aelfric awareness icon | ✅ landed | #70, #73 (integration) | State derived from priest BB each frame; integration test caught classifier issues |
| **MVP-2.5.2, 2.5.4** HUD scent + dawn gauge | ✅ landed | #69 | Both visible only when their owning state is active |
| **MVP-2.5.5–6** Pause-menu R/Q shortcuts + main-menu Quit | ✅ landed | #71, **#74 (state-leak fix)** | R reloads GameLevel; Q quits to FrontEnd. PR #74 caught: HandleRestart was setting flag but not actually reloading scene |

### Phase 4 — loss-state UI shipped

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-4.2** Run-lost HUD banner + playthroughs | ✅ landed | **#75** | DPHUDController subscribes to DP_OnRunLost; per-cause copy ("CAUGHT BY AELFRIC" / "DAWN BREAKS" / "NO VESSELS REMAIN"); 3 integration playthrough tests for the 3 causes |
| **MVP-4.3.1–3** Bot-driven playthrough | ⏸ deferred | — | Different shape than the loss-state pin; needs the test-pathfinder + a real human-bot. Less urgent now that the loss UI exists |
| **MVP-4.3.4** Human-driven playthrough | 🚧 HUMAN_GATE | — | Tomos plays the demo end-to-end |

### Recent quality-of-life landings

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-2.5.5** Rename ResetForTest → ResetForNewRun | ✅ landed | #77 | Misleading name (production HandleRestart uses it); backward-compat alias retained under ZENITH_INPUT_SIMULATOR |
| **MVP-1.3.2 coverage** Priest stillness during apprehend channel | ✅ landed | #78 | Pins "the priest plants and channels in place" GDD framing; samples post-Pursue-settle and asserts <0.1m drift to dispatch |
| **Status.md refresh** Phase 1+2 wave-by-wave + integration-test pattern | ✅ landed | #79 | Promoted every Phase-1 wave from deferred to landed; added Phase-2 + Phase-4 tables |

## Suite growth

Master 36 → 110 PASSED across 50+ merged PRs in <72 h.

**Recent test highlights:**

```
Test_P1Drop_PickupChainHandoff           (PR #52)
Test_P1Faint_RecoversToIdle              (PR #55 + coverage gap PR #60)
Test_P1Faint_SystemPossessBypassesGate
Test_P1WalkQuiet_AelfricEffectiveHearingHalved (PR #53)
Test_P1Save_RoundTripMeta                (PR #56)
Test_P1Save_RobustToCorruption
Test_P1Save_VersionMismatchFallsBackToDefault
Test_P1Dawn_DispatchesRunLost            (PR #57)
Test_P1Dawn_NoFireWhenNotStarted
Test_P1NoVessels_DispatchesRunLost       (PR #54)
Test_P1Apprehend_PriestStandsStillDuringChannel  (PR #78 — coverage gap from Status.md)
Test_P2Archetype_*                       (PR #58, #59, #62 — Beggar / Child / Devout)
Test_P2Reagent_*                         (PR #64-66 — registry + BellSoul + BogWater)
Test_P2Forge_*                           (PR #63, #67, #73)
Test_P2Fog_Memory*                       (PR #68, #72)
Test_P2HUD_*                             (PR #69-70, #73)
Test_P2Menu_PauseAndMainMenuShortcuts    (PR #71)
Test_P2Pause_RestartActuallyReloadsScene (PR #74 — caught real state-leak)
Test_P2BellSoul_PriestHearsTheBell       (PR #72)
Test_P2BellSoul_AudibleAcrossMap         (PR #76 — pins map-wide fix)
Test_P4Playthrough_LossByApprehend       (PR #75)
Test_P4Playthrough_LossByDawn
Test_P4Playthrough_LossByNoVessels
```

## Integration tests caught real bugs this run

The "write the integration test that pins the gameplay scenario, watch it find a bug" pattern paid off twice in 24 h:

1. **PR #73 (forge perception gap):** `Test_P2Forge_PriestHearsTheHammer` failed because `DPForge::HandleInteract` only called `Zenith_AudioBus::EmitSound` (test instrumentation), not `Zenith_PerceptionSystem::EmitSoundStimulus` (priest hearing path). These are DIFFERENT systems. Fix: emit to both.
2. **PR #74 (pause restart state-leak):** `Test_P2Pause_RestartActuallyReloadsScene` failed because `HandleRestart` did `LoadSceneByIndex` but didn't clear `DP_Player` / `DP_Win` / `DP_Fog` / `DP_Night` state. Fix: extracted `ResetAllRunStateBeforeReload()` helper (also calls the new `DP_Player::ResetForNewRun` from PR #77).

Both bugs would have shipped to the human-playthrough gate without the integration tests.

## Major engine + cross-system fixes landed in this run

1. **PriestPursuit_Test was failing for three stacked reasons** (PR #39):
   - **STATIC bodies silently ignore `SetPosition`.** Priest body was authored STATIC. Fixed by switching to DYNAMIC + gravity-off + locked rotations.
   - **Engine BT `MoveTo` returned FAILURE the same frame `SetDestination` was called.** Fixed by checking `pxNav->NeedsPath()` and returning RUNNING.
   - **`Zenith_BTSelector` is a memory-selector**, not reactive. Fixed by `m_xTree.Reset()` on rising edge of `BB_KEY_TARGET_WITH_DEVIL`.
2. **DP_AI navmesh cache** wired to `Zenith_NavMeshGenerator::GenerateFromScene` (PR #39).
3. **Engine BT leaf nodes must set `m_eLastStatus` explicitly** before returning (PR #41).
4. **Priest omniscient fallback compiled out of production** (PR #49).
5. **BellSoul audibility clamp** (PR #76): Zenith_PerceptionSystem clamps each agent's hearing at `min(emit_radius, agent_max_range)`, so a 200m BellSoul emit didn't reach priests >30m away. Added `DP_AI::NotifyAllPriestsOfInvestigatePos` direct-BB fanout that bypasses the perception clamp.

## Architectural surface added in this run

Public APIs new on `DP_Player`:

- `TryVoluntaryPossessSwitch(EntityID) -> bool` — player-driven path (cooldown + range gates).
- `TickPossessionCooldown(fDt)` / `GetPossessionCooldownRemaining()` — MVP-1.5.
- `GetDemonScent(EntityID)` / `TickDemonScent(fDt)` / `WriteHighestScentToBlackboard()` — MVP-1.6.
- `SetTestOmniscientFallback(bool)` / `IsTestOmniscientFallbackEnabled()` — MVP-1.9, test-only.
- `ResetForNewRun()` (PR #77) — clears every per-run state owner. Called by `DPPauseMenuController::HandleRestart/HandleQuit` AND the harness between-tests hook. Was named `ResetForTest`; renamed because the "ForTest" suffix was misleading after MVP-2.5.5 wired it into production. Backward-compat alias `ResetForTest()` retained under `ZENITH_INPUT_SIMULATOR`.

New on `DP_AI`:

- `BB_KEY_HIGH_SCENT_TARGET` — priest BB key the future hound BT will read.
- `NotifyAllPriestsOfInvestigatePos(Vec3)` (PR #76) — direct-BB fanout. Bypasses perception clamp. Used for "map-wide" stimuli like BellSoul.

New on `DP_Night` (PR #57):

- `StartNight(fDuration)` / `TickNight(fDt)` / `GetNightTimeRemaining()` / `IsNightActive()` / `Reset()`.
- Fires `DP_OnRunLost{Dawn}` exactly once on cross-zero.

New on `DP_Save` (PR #56):

- `DP_RunState` struct + binary round-trip. Schema version anchored; corruption + mismatch fall back to default.

New BT nodes in `Components/DP_BT_Nodes.h`:

- `DP_BTAction_Apprehend` (PR #41) — channels in apprehend range, dispatches `DP_OnRunLost{Apprehended}`.

New on `DPHUDController_Behaviour` (PR #75):

- Subscribes to `DP_OnRunLost`. Per-cause permanent banner via `SetStatusText`. Test accessors `DidRunLostHandlerFireForTest` / `LastRunLostCauseForTest` / `ResetRunLostForTest`.

## Ownership

**Per user direction 2026-05-12: engine work is in-scope for autonomous orchestrator + subagents.** No parallel human work track. The orchestrator drives both game-side and engine-side tasks sequentially. Mitigations on engine PRs:

- **Mandatory Reviewer subagent** on every engine PR (per OrchestratorPlaybook §5.4).
- **Full test suite + Combat smoke build** on every engine PR.
- **Design rationale in `DecisionLog.md` before the PR** for any net-new engine namespace or non-trivial algorithm choice.

Tomos's role: tick `ManualSetupChecklist.md` once before the first session; review high-stakes design rationale via `DecisionLog.md`; final eye-test on S2 art when it arrives.

## Notes for next agent

1. **Operating mode for fresh sessions:** orchestrator + subagents (Mode B). Read [OrchestratorPlaybook.md](OrchestratorPlaybook.md) before doing anything else.
2. **Hard invariants:** (a) only the orchestrator invokes MSBuild / Tools/run_dp_tests.ps1 / the game executable; (b) no worktrees, all work on `master` with per-task feature branches; (c) only the orchestrator writes Status.md / Questions.md / DecisionLog.md / MvpRoadmap.md / Config/*.json.
3. **First action:** verify the baseline. Build the prototype, run the full suite via `Tools/run_dp_tests.ps1 -Headless`. Expect **~110 PASSED / 0 FAILED** on `master @ 0e67f13e` (the post-#79 head).
4. **Next-up work** (now that Phase 1 + 2 are substantively done):
   - **Phase 3 (assets)** — Mixamo Y-Bot spike (`MVP-3.0.1` 🚧 HUMAN_GATE), villager / priest skeletons. Most of Phase 3 needs human-provided FBX.
   - **Phase 4 acceptance playthrough** — bot-driven (`MVP-4.3.1-3`) is the biggest deferred chunk. Needs the test-pathfinder + a real human-bot that drives the WASD/click inputs along a full run.
   - **Integration test follow-ups:** the "write the scenario test, find a bug" pattern keeps paying off. Candidates:
     - Pause-then-restart-then-immediately-die transitions (state-clearing race between scene reload + dispatch).
     - HUD banner stacking: Victory + DeathEvent + RunLost dispatched in the same frame — does the right one win?
     - Save+reload+possess: after `LoadFromBlob`, does the possessed-villager handle still resolve?
5. **Local-run pitfalls** (caught 2026-05-15):
   - Direct `devilsplayground.exe --automated-test <name>` invocation differs from `Tools/run_dp_tests.ps1 -Filter <name>` results. The runner adds `--skip-tool-exports --skip-unit-tests` which the engine's automated-test driver requires for reliable batched test behaviour. Always run tests through the runner script, not direct exe invocation.
   - The `Sharpmake_Build.bat` script has a `pause` directive; running it non-interactively hangs. Invoke `Sharpmake.Application.exe` directly with the same args.
6. **Branching:** all work on `master`. Branch as `dp/mvp-<task-id>` or `dp/<scope>`. Per OrchestratorPlaybook Invariant 2 (no git worktrees), prefer `C:\dev\Zenith\` directly; if the harness places you in a `.claude/worktrees/<name>/` checkout, treat it as a transient sandbox and remember Sharpmake bakes the cwd path into generated vcxprojs.
7. **No external spend** unless you find a paid blocker (and even then, log it in Questions.md and pick a different task to unblock yourself).
8. **Engine surface to be aware of** for new tests/features:
   - `Zenith_BTNode::m_eLastStatus` must be assigned by leaf nodes before returning.
   - `Zenith_TransformComponent::SetPosition` routes through Jolt when a body exists. STATIC bodies don't activate; use DYNAMIC for anything that moves via NavMeshAgent or transform writes.
   - `DP_Player::TryVoluntaryPossessSwitch` is the player path (cooldown + range gates); `SetPossessedVillager` is the system path (death, apprehend, tests).
   - Villagers don't self-register with `Zenith_PerceptionSystem`. Sight-based tests need to call `Zenith_PerceptionSystem::RegisterTarget(villager_id, /*hostile=*/true)` manually.
   - **Perception hearing has a clamp**: `min(emit_radius, agent_max_range)`. For "map-wide" stimuli (BellSoul-class) use `DP_AI::NotifyAllPriestsOfInvestigatePos` to bypass.
   - `DP_Player::ResetForNewRun()` is the canonical reset (PR #77). `ResetForTest` is a backward-compat alias for tests only.

## Suggested next coverage gaps

Tests that would catch a SPECIFIC concrete regression. Most prior gaps from earlier Status.md revisions have been closed.

| Test | What it would catch | Effort |
|------|---------------------|--------|
| `Test_P2HUD_VictoryThenRunLostBannerWins` | Run-lost banner overwrites Victory banner (or vice versa) when both dispatch in the same frame | S |
| `Test_P1Save_PossessedVillagerHandleSurvivesReload` | `LoadFromBlob` re-resolves the possessed-villager handle (entity-index reuse after scene unload is a classic dangling-handle source) | M |
| `Test_P4Playthrough_VictoryThenRestartIsClean` | Pause-R after winning produces a clean second run (no leftover DP_Win state, no leftover held items) | M |
| `Test_P2Forge_RecipeRespectsHeldVillagerTag` | Forge crafting checks the holder's archetype tag (Child can't trigger forge) | S |

## Last completed

- **PR #76** (merged `2026-05-15T01:09Z`) — MVP-2.2.6 BellSoul truly map-wide. Direct-BB fanout to every priest, bypassing perception clamp. New `Test_P2BellSoul_AudibleAcrossMap` at 120 m.
- **PR #75** (merged `2026-05-15T00:49Z`) — MVP-4.2 run-lost HUD banner + 3 loss-state playthrough tests.
- **PR #74** (merged `2026-05-14T23:11Z`) — pause Restart actually clears run state + reloads (caught state-leak via integration test).
- **PR #73** (merged `2026-05-14T22:52Z`) — Phase-2 integration tests; caught real forge → priest perception gap.
- **PR #72** (merged `2026-05-14T22:33Z`) — Phase-2 integration: memory fog wiring + BellSoul priest perception.
- **PR #71** (merged `2026-05-14T22:09Z`) — MVP-2.5.{5,6} pause-menu R/Q shortcuts + main-menu Quit.
- **PR #70** (merged `2026-05-14T21:50Z`) — MVP-2.5.{1,3} HUD whisper line + Aelfric awareness icon.
- **PR #69** (merged `2026-05-14T21:32Z`) — MVP-2.5.{2,4} HUD scent + dawn gauge.
- **PR #68** (merged `2026-05-14T21:16Z`) — MVP-2.4.5 memory-fog state machine.
- **PR #67** (merged `2026-05-14T20:42Z`) — MVP-2.3.4 forge audible across the village.

For PRs older than 2026-05-14 see git log.
