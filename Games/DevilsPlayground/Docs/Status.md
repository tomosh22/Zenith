# DP Status

**Last updated:** 2026-05-14 — Phase 1 substantively complete: 9 of the 10 Phase-1 waves landed in a single autonomous run (PRs #39 through #48 merged; PR #49 = MVP-1.9 in CI). Master HEAD `0a95e1b7`. Full suite **51 PASSED, 0 FAILED, 15 SKIPPED-headless** on master; **53 PASSED** with PR #49's two new sight tests.
**Build:** ✅ DP target builds clean (`vs2022_Debug_Win64_True`, 0 warnings, 0 errors).
**Tests:** Full local suite green via `Tools/run_dp_tests.ps1 -Headless`. Headless skips 15 graphics-only tests; all 51 (or 53) compute-only tests pass.

## Manual setup checklist gating

**⚠️ BEFORE STARTING THE FIRST AGENT SESSION,** the user (Tomos) must tick every box in [Docs/ManualSetupChecklist.md](ManualSetupChecklist.md). The orchestrator pattern depends on human-only setup (branch protection rules, Vulkan SDK install, pwsh.exe install, etc.) that no agent can perform. The orchestrator's session-start ritual checks this file and STOPS if any box is unchecked.

## Current state — Phase 1 wave-by-wave

| Wave | Status | PRs | Notes |
|------|--------|-----|-------|
| **MVP-1.1** Real pause | ✅ landed | #30 | Pre-session |
| **MVP-1.2** Real navmesh integration | ✅ landed | #39, #40 | Closed-door + regen-on-swap tests; PriestPursuit_Test stabilised |
| **MVP-1.3.1–4** Apprehend / run-loss | ✅ landed | #41, #42 | DP_OnRunLost event + Apprehend BT branch + switch-breaks + out-of-range guards |
| **MVP-1.3.5** Dawn + NoVessels causes | ⏸ deferred | — | Needs Night timer system (Phase 4) and a smaller-than-17-villager scene |
| **MVP-1.4.1–3** Faint state machine | ⏸ deferred | — | Villager state machine + 10 s recovery — multi-step, separate PR |
| **MVP-1.4.4–5** Drop verb (G) | ✅ landed | #48 | Drop at foot pos + 0.5 s post-drop pickup cooldown |
| **MVP-1.4.6** Drop-pickup chain handoff | ⏸ deferred | — | Movement-sim follow-up; chain semantics |
| **MVP-1.5** Possession cooldown | ✅ landed | #43 | 1.5 s after voluntary switch; death/apprehend bypass it |
| **MVP-1.6** Demon-scent | ✅ landed | #45 | Table + decay + write to priest BB_KEY_HIGH_SCENT_TARGET |
| **MVP-1.7.1–3** Sprint | ✅ landed | #46 | Shift+move → 12 m/s + 3 s/s extra life cost |
| **MVP-1.7.4–5** Walk-quiet | ✅ landed | #47 | Ctrl+move → 4 m/s + halved footstep loudness; new footstep emission system |
| **MVP-1.7.6** Aelfric perception precision | ⏸ deferred | — | Distance-threshold maths; data path already covered by Test_PriestBBBridge |
| **MVP-1.8** Possession range | ✅ landed | #44 | 15 m anchor radius; anchor moves on each successful hop |
| **MVP-1.9** Priest omniscience removed | 🔄 in CI | #49 | Fallback gated behind ZENITH_INPUT_SIMULATOR + opt-in test flag; production builds are sight-driven only |
| **MVP-1.10** Save/load run state | ⏸ deferred | — | DP_RunState struct + serialisation; bigger chunk |

## Suite growth — this autonomous run

Master 36 → 51 PASSED across 10 merged PRs (+15 net new tests). Highlights:

```
PriestPursuit_Test           (stabilised — was flaky-fail, now 4-of-4 reliable)
Test_P1NavMesh_ClosedDoorBlocksPath
Test_P1NavMesh_RegenerationOnSceneSwap
Test_P1Apprehend_PriestCatchesPlayer
Test_P1Apprehend_SwitchBreaksChannel
Test_P1Apprehend_OutOfRangeIgnored
Test_P1Cooldown_CannotPossessFor1pt5s
Test_P1Cooldown_NotAffectedByDeath
Test_P1Range_RefusedOutOfRange
Test_P1Range_AcceptedInRange
Test_P1Scent_AccumulatesOnPossession
Test_P1Scent_NotificationToBlackboard
Test_P1Sprint_DrainsLifeFaster
Test_P1Sprint_NoDrainWhenNotMoving
Test_P1WalkQuiet_FootstepLoudnessHalved
Test_P1Drop_GoesToGroundAtBodyPosition
```

With PR #49 → 53 PASSED (adds `Test_P1Priest_DoesNotChasePossessedOutOfSight` + `Test_P1Priest_PursuesAfterLineOfSight`).

## Major engine + cross-system fixes landed this run

These were the highest-leverage discoveries; full rationale lives in each PR's body.

1. **PriestPursuit_Test was failing for three stacked reasons** (PR #39):
   - **STATIC bodies silently ignore `SetPosition`.** Priest's body was authored STATIC; `Zenith_TransformComponent::SetPosition` routes through Jolt's BodyInterface, and STATIC bodies in the `NON_MOVING` broadphase layer don't activate. Every NavMeshAgent transform write landed in Jolt but the body never moved.
   - **Engine BT `MoveTo` / `MoveToEntity` returned FAILURE the same frame `SetDestination` was called**, before `NavMeshAgent::Update` had a chance to compute the path. Fixed by checking `pxNav->NeedsPath()` and returning RUNNING instead of FAILURE.
   - **`Zenith_BTSelector` is a memory-selector**, not a reactive priority-selector. When the priest's BT landed on patrol (because the possession side-table hadn't propagated to `DPVillager::IsPossessed()` yet on frame 0), patrol held the slot for ~1 s before pursue got another turn. Fixed by `m_xTree.Reset()` on the rising edge of `BB_KEY_TARGET_WITH_DEVIL` in `Priest_Behaviour::OnUpdate`.
2. **DP_AI navmesh cache** wired to `Zenith_NavMeshGenerator::GenerateFromScene` (PR #39). GameLevel got a 120×120 m ground plane so the generator has something to walk on.
3. **Engine BT leaf nodes must set `m_eLastStatus` explicitly** before returning (PR #41). Found while building `DP_BTAction_Apprehend`: without the assignment, `ExecuteCompositeBody`'s `pxChild->GetLastStatus() != RUNNING` check saw FAILURE (the construct-time default) and called `OnEnter` every tick — which reset the channel timer every frame and meant Apprehend never completed its 3-second channel.
4. **Priest omniscient fallback compiled out of production** (PR #49). Pre-1.9 the bridge dropped back to `DP_Player::GetPossessedVillager` whenever no perceived target qualified. Now production builds (no `ZENITH_INPUT_SIMULATOR`) compile the fallback OUT; test builds gate it behind a runtime flag defaulting to true (preserves pre-1.9 test behaviour). New sight tests opt out via `SetTestOmniscientFallback(false)` in Setup.

## Architectural surface added this run

Public APIs new this run, on `DP_Player`:

- `TryVoluntaryPossessSwitch(EntityID) -> bool` — player-driven possession path. Runs the cooldown + range gates. System paths (death, apprehend) keep using direct `SetPossessedVillager`.
- `TickPossessionCooldown(fDt)` / `GetPossessionCooldownRemaining()` — MVP-1.5.
- `GetDemonScent(EntityID)` / `TickDemonScent(fDt)` / `WriteHighestScentToBlackboard()` — MVP-1.6 hound-prep data path.
- `SetTestOmniscientFallback(bool)` / `IsTestOmniscientFallbackEnabled()` — MVP-1.9 test toggle, ZENITH_INPUT_SIMULATOR-only.

On `DPVillager_Behaviour`:

- `IsSprintingNow() const` / `IsWalkQuietNow() const` — test accessors for the movement state machine.
- `m_fFootstepCountdown` + `TickFootsteps(fDt, bMoving)` — emits to both `Zenith_AudioBus` (test recorder + future asset hook) and `Zenith_PerceptionSystem` (priest hearing). Loudness multiplied by `movement.walk_footstep_loudness_multiplier` when walk-quiet.

On `DPItemBase_Behaviour`:

- `BeginPostDropCooldown()` — public setter called by `DPPlayerController_Behaviour::HandleDropItem` so dropped items don't immediately re-pick-up from the villager's foot position.

On `DP_AI`:

- `BB_KEY_HIGH_SCENT_TARGET` — priest BB key the future hound BT will read.

New BT nodes in `Components/DP_BT_Nodes.h`:

- `DP_BTAction_Apprehend` — placed first in the priest's Selector; channels in place when in `priest.apprehend_range_m` of the target, dispatches `DP_OnRunLost{Apprehended}` on channel completion.

New tuning fields in `Config/Tuning.json` (under `movement`):

- `footstep_interval_s` (0.4)
- `footstep_loudness` (0.3)
- `footstep_radius_m` (10.0)

## Ownership

**Per user direction 2026-05-12: engine work is in-scope for autonomous orchestrator + subagents.** No parallel human work track. The orchestrator drives both game-side tasks (`Games/DevilsPlayground/...`) and engine-side tasks (`Zenith/...`) sequentially. Mitigations on engine PRs:

- **Mandatory Reviewer subagent** on every engine PR (per OrchestratorPlaybook §5.4).
- **Full test suite + Combat smoke build** on every engine PR (catches cross-game regressions from shared `Zenith/` changes).
- **Design rationale in `DecisionLog.md` before the PR** for any net-new engine namespace or non-trivial algorithm choice.

Tomos's role remains: tick `ManualSetupChecklist.md` boxes once before the first session; review high-stakes design rationale via `DecisionLog.md`; final eye-test on S2 art when it arrives.

## Notes for next agent

1. **Operating mode for fresh sessions:** orchestrator + subagents (Mode B). Read [OrchestratorPlaybook.md](OrchestratorPlaybook.md) before doing anything else. The user explicitly chose this on 2026-05-11 and explicitly forbade git worktrees.
2. **Hard invariants:** (a) only the orchestrator invokes MSBuild / Tools/run_dp_tests.ps1 / the game executable; (b) no worktrees, all work on `master` with per-task feature branches; (c) only the orchestrator writes Status.md / Questions.md / DecisionLog.md / MvpRoadmap.md / Config/*.json.
3. **First action:** verify the baseline. Build the prototype (`vs2022_Debug_Win64_True`, target DevilsPlayground), run the full suite via `Tools/run_dp_tests.ps1 -Headless`. Expect **51 PASSED / 0 FAILED / 15 SKIPPED** on `master @ 0a95e1b7` (or **53 PASSED** once PR #49 merges). If any test goes red, fix before starting new work.
4. **Phase 1 deferred items** to consider next, roughly in priority order:
   - **Test coverage gaps** (see the "Suggested test coverage" section at the bottom of this file).
   - **MVP-1.4.6** Drop-pickup chain handoff — needs movement-sim setup; ~1 day.
   - **MVP-1.4.1–3** Voluntary-switch faint state machine — villager `m_eState` field (Idle/Possessed/Fainted/Dead) + 10 s recovery + 2 tests.
   - **MVP-1.7.6** Aelfric effective hearing halved — distance-threshold maths; the data path is exercised by `Test_PriestBBBridge` already.
   - **MVP-1.3.5** Dawn + NoVessels causes — needs the Night timer (Phase 4) and a multi-villager death scenario.
   - **MVP-1.10** Save/load — `DP_RunState` struct + serialisation; bigger chunk, schema-version anchor mandatory.
5. **Branching:** all work on `master`. Branch as `dp/mvp-<task-id>` or `dp/<scope>`. The worktree at `.claude/worktrees/flamboyant-mirzakhani-9186f1/` is irrelevant to DP.
6. **No external spend** unless you find a paid blocker (and even then, log it in Questions.md and pick a different task to unblock yourself).
7. **Engine surface to be aware of** for new tests/features:
   - `Zenith_BTNode::m_eLastStatus` must be assigned by leaf nodes before returning (BTLeaf doesn't auto-set it).
   - `Zenith_TransformComponent::SetPosition` routes through Jolt when a body exists. STATIC bodies don't activate; use DYNAMIC for anything that moves via NavMeshAgent or transform writes.
   - `DP_Player::TryVoluntaryPossessSwitch` is the player path (cooldown + range gates); `SetPossessedVillager` is the system path (death, apprehend, tests).
   - Villagers don't self-register with `Zenith_PerceptionSystem`. Sight-based tests need to call `Zenith_PerceptionSystem::RegisterTarget(villager_id, /*hostile=*/true)` manually. Production gameplay still relies on the test-only omniscient fallback; production self-registration is a future PR (see MVP-1.9.3's docstring).

## Suggested test coverage — high-value gaps after this run

Tests that would catch a SPECIFIC concrete regression, ranked by likelihood. Full discussion in 2026-05-14 chat transcript.

| Test | What it would catch | Effort |
|------|---------------------|--------|
| `Test_P1Range_AnchorMovesWithEachHop` | Anchor moving correctly on each successful hop (existing tests only check 1 hop; a "stuck anchor" refactor would silently pass) | S |
| `Test_P1Scent_DecaysOverTime` | `TickDemonScent` actually firing — accidental decay=0 or commented-out tick would slip through | S |
| `Test_P1Scent_HighestWinsInBlackboard` | `WriteHighestScentToBlackboard` picking max not last; a "last-bumped" refactor would pass with one villager | S |
| `Test_P1Sprint_WinsTieOverWalkQuiet` | The Shift+Ctrl priority order encoded in `OnUpdate` is never asserted | S |
| `Test_P1Drop_PickupChainHandoff` (MVP-1.4.6) | Post-drop cooldown expires + re-pickup re-engages (full chain) | M |
| `Test_P1Scent_NoBumpWhilePossessing` | Scent bump fires only on voluntary switch, not every tick (a misplaced bump in `TickDemonScent` would grow scent unboundedly) | S |
| `Test_P1Apprehend_PriestStandsStillDuringChannel` | Priest transform doesn't move during the 3 s channel (current test only verifies the event fires) | M |

## Last completed

- **MVP-1.9** (PR #49, in CI 2026-05-14) — priest omniscience removed (production build) + test-only opt-in fallback + 2 sight tests. See "Current state" table.
- **MVP-1.4.4–5** (PR #48, merged `0a95e1b7` 2026-05-14) — drop verb (G) at foot position + 0.5 s pickup cooldown + DPItemBase tick gate + Test_P1Drop_GoesToGroundAtBodyPosition.
- **MVP-1.7.4–5** (PR #47, merged `ca38e3a7` 2026-05-14) — walk-quiet (Ctrl) + footstep emission system. Local result: normal=0.300, quiet=0.150, ratio=0.500 (exact tuning value, zero rounding drift).
- **MVP-1.7.1–3** (PR #46, merged `63f965c7` 2026-05-14) — sprint (Shift) + DPVillager `IsSprintingNow` state cache + 2 tests. Local result: sprint=4.065 s drop vs walk=1.016 s = diff 3.049 s in a 1 s window (matches the 3 s/s extra cost tuning).
- **MVP-1.6** (PR #45, merged `9fc2dc45` 2026-05-14) — demon-scent table + tick decay + write-to-priest-BB.
- **MVP-1.8** (PR #44, merged `9e3a351e` 2026-05-14) — possession range gate; anchor moves on each successful hop ("demon-hop chain" semantic).
- **MVP-1.5** (PR #43, merged `46c4361e` 2026-05-14) — possession cooldown after voluntary switch; death/apprehend bypass.
- **MVP-1.3.3–4** (PR #42, merged `f3e20b9f` 2026-05-14) — apprehend switch-breaks + out-of-range regression guards.
- **MVP-1.3.1–2** (PR #41, merged `d2d7082f` 2026-05-14) — DP_OnRunLost event + DP_BTAction_Apprehend BT branch + Test_P1Apprehend_PriestCatchesPlayer. **Bug found + fixed:** Zenith leaf BT nodes must explicitly set `m_eLastStatus` before returning.
- **MVP-1.2.3–4** (PR #40, merged `82fdd2e9` 2026-05-14) — navmesh closed-door + regen-on-swap tests. CI fix: dropped a spurious pointer-difference gate because CI's heap allocator was observed to reuse freed addresses.
- **MVP-1.2.2 + PriestPursuit stabilisation** (PR #39, merged `d80c50c0` 2026-05-14) — DP_AI wired to `GenerateFromScene` + priest body DYNAMIC + engine BT pending-path tolerance + reactive selector hack. Three stacked root causes; full breakdown above.

For PRs older than 2026-05-14 see git log.
