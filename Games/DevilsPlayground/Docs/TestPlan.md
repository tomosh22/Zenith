# Devil's Playground — Automated Test Plan

**Document purpose:** An exhaustive, test-driven development plan for *Devil's Playground*. Every system in [GameDesignDocument.md](GameDesignDocument.md) is paired with one or more automated tests. Every test runs entirely inside `Zenith_AutomatedTestRunner`, drives the game through `Zenith_InputSimulator`, and produces a machine-readable pass/fail result with **zero human intervention required at any point in the development lifecycle**.

**Author:** Test Design (Claude)
**Companion docs:** [GameDesignDocument.md](GameDesignDocument.md), [Shortfalls.md](Shortfalls.md)
**Last updated:** 2026-05-27 — Phase 1 + Phase 2 + most of Phase 4 substantively complete. Phase 5 telemetry / verification system + instructional HUD shipped 2026-05-15..16; procgen migration complete 2026-05-19 (PRs #96-#117); telemetry v3 + seed-matrix tooling + personality-matrix balance shipped 2026-05-20; 3 new bot personalities (Magpie / Relay / Heretic) shipped 2026-05-21 PR #139, 4th combo personality (Trickster) + follow-up fixes shipped PR #140, full balance pass (door collider physics + life-timer + sprint cost + new `iObjAttemptCap` param + 3-of-5 win condition) shipped 2026-05-22 PR #141. **Personality unification 2026-05-23 (`0813cff6`)** removed all per-personality buffs/nerfs — all 8 bots now run mechanically identical mechanics. **Doors-at-DoorPoints 2026-05-26 (`00bb2382`)** delivers the current 80 % matrix wins (64 / 80) state. **133 `ZENITH_AUTOMATED_TEST_REGISTER` invocations** across 113 .cpp files at HEAD (verified 2026-05-27 via `grep -c`); some `#ifdef ZENITH_INPUT_SIMULATOR`-gated. The 8-personality bot matrix satisfies the user-ratified balance criteria: every personality WR ∈ (0%, 100%) (60–90 % range), every canonical seed winnable by ≥1 personality. See [Status.md](Status.md) for the live wave-by-wave breakdown + a caveat about the local headless pass-rate at HEAD. Individual test entries below describe the intended verify-contract; for any test name listed as "to author" or "planned," cross-check against `Games/DevilsPlayground/Tests/Test_*.cpp` first (many have shipped since 2026-05-11 and just haven't been promoted out of the planned sections here).

**Tests/CLAUDE.md is the index by category** with running instructions, the personality framework, and the seed-matrix tooling. This doc is the design plan; that doc is the operational reference.

---

## 0. Constraints & Verification Contract

### 0.1 Hard constraints from the harness

The Zenith automated-test harness validates **deterministic in-process game state** only. The constraints below shape every test in this document.

| Capability | Available? | Implication for tests |
|---|---|---|
| Simulated input (keys, mouse, wheel, UI-click) | ✅ via `Zenith_InputSimulator` | All player actions drivable from code. |
| Fixed delta-time stepping (`--fixed-dt`) | ✅ | Time-dependent assertions are deterministic. |
| Per-test JSON results (`--test-results-dir`) | ✅ | Claude can parse pass/fail without seeing the screen. |
| Game state queries via `DP_*` public interfaces | ✅ | All public state is observable. |
| Entity state via `DP_Query::ForEachScriptInActiveScene<T>` | ✅ | Internal behaviour state is reachable. |
| Event-dispatch interception via `Zenith_EventDispatcher` | ✅ | Test can subscribe to `DP_On*` events as observer. |
| **Pixel rendering** | ❌ | Replaced by surrogate: assert the *input* to rendering (fog-hole array contents, material handle, model transform). |
| **Audio output** | ❌ | Replaced by surrogate: assert the *trigger* (perception stimulus emitted, audio-emission function called with correct args). |
| **Wall-clock performance** | ⚠️ partial | Game-time only via `Zenith_Core::GetTimePassed()`. Frame-budget tests use **operation counts**, not wall-clock. |
| **Crash recovery** | ❌ | A segfault terminates the test before Verify; the harness reports timeout instead of a meaningful failure. Out-of-process crash detection is handled by the runner script's exit-code contract. |
| **Multi-process / networked** | ❌ | Single-process only. |
| **Save-file binary format** | ⚠️ surrogate | Tests assert round-trip equivalence, not byte layout. |

### 0.2 Verification contract

For every test in this document:

1. **Determinism.** Uses `--fixed-dt 0.01666` (60 Hz fixed). Any RNG seeded explicitly in Setup.
2. **Bounded.** Has an explicit `m_iMaxFrames` budget. Step returns false on completion. The harness never blocks indefinitely.
3. **Self-cleaning.** Uses `Zenith_AutomatedTestRunner::RegisterBetweenTestsHook` (already wired in `DevilsPlayground.cpp`) to reset DP-side globals; entity-owned state cleans itself via OnDestroy when scene 0 reloads between tests.
4. **JSON-emitting.** Pass/fail result + diagnostic counters land in `<results-dir>/<TestName>.json`.
5. **Claude-auditable.** Claude Code reads the JSON and the test's `Verify()` return value. No screenshots, no audio captures, no user observation.

### 0.3 What "fully verifiable by Claude Code" means

Every assertion in every test must reduce to a comparison of values Claude can compute or fetch:

- `expected == actual` (integer, float-with-tolerance, entity-ID, enum)
- `event was dispatched with expected payload` (intercepted via dispatcher subscription)
- `function was called with expected args` (intercepted via a thin instrumentation hook — see §0.4)
- `JSON file at path contains expected key/value` (post-run check by the runner script)

What this **excludes**:

- "The screen shows X." → replaced by asserting the render-input (model handle, fog-hole array, light count).
- "The audio plays Y." → replaced by asserting `Zenith_PerceptionSystem::EmitSoundStimulus` was called with the right loudness/radius, *or* the corresponding event was dispatched.
- "Animation looks natural." → replaced by asserting bone-pose at frame N matches a reference table (see §2.4 archetype tests).

### 0.4 Instrumentation hooks (one-time engineering cost)

Three engine-side instrumentation hooks need to land in Phase 1 so the rest of this plan works. All are cheap and used only when `ZENITH_INPUT_SIMULATOR` is defined.

| Hook | Purpose | Spec |
|---|---|---|
| `Zenith_AudioBus::GetEmittedSoundsForTest()` | Record-and-replay of audio emissions for surrogate audio asserts. | Returns a `Zenith_Vector<EmittedSound>` cleared between tests. Each entry: `{const char* name, Vec3 position, float loudness, float radius, Zenith_EntityID uSourceEntity, uint32_t uFrame}`. **Round-4/5 reconciliation:** `radius` is required by `Test_P2Forge_AudibleAt30m`; `uSourceEntity` is required by walk-quiet tests to distinguish villager footsteps from Aelfric's own footsteps. |
| `Zenith_RenderBus::GetSubmittedDrawCallsForTest()` | Record-and-replay of draw submissions for surrogate render asserts. | Returns a `Zenith_Vector<SubmittedDraw>` cleared between tests. Each entry: `{modelHandle, materialHandle, worldMatrix, frame}`. |
| `Zenith_SaveSystem::GetWrittenBlobsForTest()` | Round-trip read-back of save blobs without disk I/O. | Returns the last-written blob as a buffer; `SetReadbackBlob` injects a buffer for the next load. |

These three together turn rendering, audio, and persistence into in-process asserts. **All three are required for this test plan to be claudia-verifiable.** They are listed in [Shortfalls.md](Shortfalls.md) as part of Phase 1 engineering scope.

### 0.5 Conventions

- **Naming:** `Test_<Phase><System>_<Scenario>` (e.g. `Test_P1Apprehend_PriestCatchesPlayer`).
- **Files:** One test per `.cpp` file under `Games/DevilsPlayground/Tests/`.
- **Setup:** zero phase counter, clear captured state, register event subscribers if needed.
- **Step state-machine:** explicit `m_iPhase` integer, switched on each frame.
- **Verify:** returns true *only* if every captured invariant holds, false otherwise. Logs diagnostic to `Zenith_Log` on failure.
- **Frame budget:** Phase budget × frames-per-phase + 60-frame safety margin. Always ≤ 900 (15 s @ 60 Hz) unless explicitly an endurance test.

### 0.6 Runner

The existing `zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1)` already handles discovery, batch run, and JSON aggregation. **Three additions are scheduled in Phase 0.0 (MVP-0.0.4)** before any Phase 1 test work begins — the runner currently supports only `-Headless`, `-PerProcess`, `-Filter`, `-ResultsDir`, `-ExitAfterFrames`, `-FixedDt`. Phase 1 then adds:

1. `-Tier <0|1|2|3|4>` — filter tests by tier prefix.
2. `-FailFast` — abort the batch on first failure (default off for nightly, on for PR checks).
3. `-AssertionsLog <path>` — collect every assertion failure across all tests into a single grep-able log.

---

## 1. Tier 0 — Harness Sanity (always-on)

Every dev session, on every branch, before any other test. If these fail, no other test result is trustworthy.

### Test_T0Harness_Smoke

**Proves:** the harness boots, scene loads, and a noop test exits clean.
**Frames:** 30. **Phase:** all.

- **Setup:** none.
- **Step:** `return iFrame < 5;`
- **Verify:** `return true;`

### Test_T0Harness_FixedDt

**Proves:** `--fixed-dt 0.01666` pins game-time to 60 Hz exactly.

- **Setup:** capture `Zenith_Core::GetTimePassed()` start value.
- **Step:** run 60 frames. Capture end time. Return false.
- **Verify:** `abs((end - start) - 1.0f) < 0.005f` (50 µs/frame tolerance).

### Test_T0InputSim_KeyPressAutoRelease

**Proves:** `SimulateKeyPress` survives one frame and self-clears.

- **Setup:** none.
- **Step:** phase 0: `SimulateKeyPress(ZENITH_KEY_W)`, advance phase. phase 1: assert `WasKeyPressedThisFrameSimulated(ZENITH_KEY_W)`. phase 2: assert *not* pressed-this-frame. phase 3: return false.
- **Verify:** both assertions in step held.

### Test_T0InputSim_MouseClickAtPosition

**Proves:** `SimulateMouseClick` produces a single-frame mouse-down + mouse-up.

- **Setup:** subscribe a test-side observer to `Zenith_Input::OnMouseButtonEvent` that counts down + up events.
- **Step:** phase 0: `SimulateMouseClick(100.0, 200.0)`. phase 1: assert mouse position == (100, 200). phase 2: return false.
- **Verify:** down count == 1, up count == 1, position matches.

### Test_T0InputSim_MouseWheel

**Proves:** `SimulateMouseWheel(2.0f)` reaches `Zenith_Input::GetMouseWheelDelta` for exactly one frame.

- **Setup:** none.
- **Step:** phase 0: capture `Zenith_Input::GetMouseWheelDelta()` (expect 0). `SimulateMouseWheel(2.0f)`. phase 1: capture again (expect 2.0). phase 2: capture (expect 0 again). return false.
- **Verify:** captures are 0, 2, 0.

### Test_T0InputSim_UIElementClickByName

**Proves:** `SimulateClickOnUIElement("MenuPlay")` resolves the named UI element and triggers its handler.

- **Setup:** `LoadSceneByIndex(0)` (front-end with MenuPlay button).
- **Step:** wait for scene Awake. Phase 1: `SimulateClickOnUIElement("MenuPlay")`. Phase 2: poll `Zenith_SceneManager::GetActiveScene` → expect scene index 1. Return false.
- **Verify:** active scene == 1.

These six tests are the canary. The first five (Smoke, FixedDt, KeyPressAutoRelease, MouseClickAtPosition, MouseWheel) already pass on the prototype today (the harness drove them during M1). The sixth (UIElementClickByName) requires a UI element to exist at the click target and lands once the FrontEnd scene has the MenuPlay button wired up.

---

## 2. Tier 1 — Phase 1: Foundation (months 0–4)

These tests prove the foundational gaps from [Shortfalls.md §1](Shortfalls.md) are closed. **TDD discipline: write the test first, watch it fail, implement, watch it pass.**

### 2.1 Pause is Real

#### Test_P1Pause_TimerStopsOnEscape

**Proves:** GDD §7.2 — Esc pauses simulation; life timer does not tick during pause.

- **Setup:** zero state. `LoadSceneByIndex(1)`.
- **Step:**
  - phase 0: poll for a `DPVillager_Behaviour`, possess it via `DP_Player::SetPossessedVillager`. Capture life L0.
  - phase 1: wait 30 frames (0.5 s). Capture L1. Assert L1 < L0 (timer is ticking).
  - phase 2: `SimulateKeyPress(ZENITH_KEY_ESCAPE)`. Capture L2 immediately. Run 60 frames (1.0 s of game time, *if pause is broken*). Capture L3.
  - phase 3: assert L3 == L2 within 0.001 tolerance (timer froze).
  - phase 4: `SimulateKeyPress(ZENITH_KEY_ESCAPE)` to resume. Run 30 frames. Capture L4. Assert L4 < L3.
  - phase 5: return false.
- **Verify:** L1 < L0 AND L3 ≈ L2 AND L4 < L3.

#### Test_P1Pause_PriestStopsOnEscape

**Proves:** the priest's position does not change during pause.

- **Setup:** `LoadSceneByIndex(1)`. Possess a villager. Force pursuit via the standard fallback path.
- **Step:** capture priest position P0. Run 30 frames. Capture P1 (priest moved). Press Esc. Capture P2. Run 60 frames. Capture P3. Assert `dist(P3, P2) < 0.05 m`. Resume. Run 30 frames. Capture P4. Assert `dist(P4, P3) > 0.5 m`.
- **Verify:** the three distance invariants hold.

#### Test_P1Pause_InputSimDuringPause

**Proves:** input events fire during pause (so menus work), but **no script OnUpdate runs**.

- **Setup:** `LoadSceneByIndex(1)`. Install a frame-counter probe on `DPVillager_Behaviour::OnUpdate` (test-only instrumentation).
- **Step:** capture frame counter C0. Press Esc. Run 30 frames. Capture C1. Assert `C1 == C0` (no OnUpdate calls).
- **Verify:** counters match.

### 2.2 Real NavMesh

#### Test_P1NavMesh_PathRespectsWalls

**Proves:** [Shortfalls §1.2] — the generated navmesh has a hole where each wall is; pursuit goes *around* walls, not through.

- **Setup:** authored test scene with a single 10 m wall between priest start (-5, 0) and target (5, 0). Possess villager at (5, 0).
- **Step:** capture priest path waypoints (the new navmesh agent should expose `GetPath() -> Zenith_Vector<Vec3>` for tests). Run 120 frames. Capture priest end position.
- **Verify:** the path contains at least one waypoint whose X-coordinate has the same sign as the wall's bypass direction, AND the priest's end position has minimum |y| ≥ 0.5 m of bypass (proving it routed around).

#### Test_P1NavMesh_ClosedDoorBlocksPath

**Proves:** a closed `DPDoor` blocks pathing; opening it restores the path.

- **Setup:** authored corridor scene with one door between priest and target. Door starts closed and locked.
- **Step:**
  - phase 0: capture priest path length L_closed (expect "no path" return or very long detour > 30 m).
  - phase 1: programmatically open the door (`door->ForceOpenForTest()`).
  - phase 2: re-request path. Capture L_open (expect < 10 m).
- **Verify:** `L_closed > 30 OR pathInvalid` AND `L_open < 10`.

#### Test_P1NavMesh_RegenerationOnSceneSwap

**Proves:** `DP_AI::ResetLevelNavMesh()` actually causes the next `GetOrBuildLevelNavMesh` to rebuild for a different scene.

- **Setup:** none.
- **Step:** load scene 4 (Gym_Doors). Capture navmesh polygon count P0. Load scene 5 (Gym_Forge). Capture P1. Assert `P0 != P1`.
- **Verify:** polygon counts differ (different scenes → different navmeshes).

### 2.3 Apprehend / Run-Loss

#### Test_P1Apprehend_PriestCatchesPlayer

**Proves:** GDD §4.5 — priest overlap with possessed villager triggers a 3 s apprehend channel, fires `DP_OnRunLost` if not interrupted.

- **Setup:** authored test scene: priest at (0, 0), villager at (1, 0) (within `m_fApprehendRange` = 2.0 m). Subscribe to `DP_OnRunLost`.
- **Step:**
  - phase 0: `SetPossessedVillager(villager)`. Pin the villager (no WASD input). Run 1 frame.
  - phase 1: priest's apprehend channel should start. Capture priest blackboard state. Assert `BB.IsApprehending == true`.
  - phase 2: run 180 frames (3.0 s @ 60 Hz). Run 5 extra frames for dispatch.
  - phase 3: assert `DP_OnRunLost` was dispatched exactly once.
- **Verify:** apprehending flag was set, event count == 1.

#### Test_P1Apprehend_SwitchBreaksChannel

**Proves:** voluntary possession-switch during apprehend channel aborts the apprehension.

- **Setup:** same as above plus a second villager 10 m away.
- **Step:**
  - phase 0: possess villager A. Wait 1 frame for apprehend to start.
  - phase 1: wait 60 frames (1.0 s — channel partial). Confirm `BB.IsApprehending == true`.
  - phase 2: `SetPossessedVillager(villager B)` (or simulate right-click on villager B's screen position).
  - phase 3: wait 5 frames. Assert `BB.IsApprehending == false` AND no `DP_OnRunLost` dispatched.
- **Verify:** apprehending state cleared, no run-lost event.

#### Test_P1Apprehend_OutOfRangeIgnored

**Proves:** priest at distance > apprehend-range does not trigger channel.

- **Setup:** villager at (10, 0), priest at (0, 0). Possess villager. **Set the priest's navmesh agent target to a position 50 m away** so it doesn't pursue.
- **Step:** run 60 frames. Assert `BB.IsApprehending == false` always (sample every 10 frames).
- **Verify:** apprehending false on every sample.

### 2.4 Voluntary Switch + Drop Verb

#### Test_P1Switch_FaintNotDie

**Proves:** GDD §4.1 — voluntary switch faints the old body (10 s recovery) but does not dispatch `DP_OnVillagerDied`.

- **Setup:** subscribe to `DP_OnVillagerDied`. Find two villagers A and B.
- **Step:**
  - phase 0: possess A. Wait 1 frame.
  - phase 1: `SetPossessedVillager(B)` (or `SimulateMouseClick` on B's screen position).
  - phase 2: wait 5 frames. Assert villager A's `GetState() == FAINTED` AND its `m_fFaintRecoveryTimer` > 9 seconds.
  - phase 3: assert `DP_OnVillagerDied` count == 0.
- **Verify:** state correct, no died event.

#### Test_P1Switch_BurnOutDoesDie

**Proves:** life-timer expiry *does* dispatch `DP_OnVillagerDied` (negative control for the prior test).

- **Setup:** subscribe to `DP_OnVillagerDied`. Find villager.
- **Step:** possess. Set `SetRemainingLifeForTest(0.05)`. Wait 10 frames. Assert event count == 1.
- **Verify:** event fired exactly once.

#### Test_P1Drop_GoesToGroundAtBodyPosition

**Proves:** the G key drops the held item at the villager's current position.

- **Setup:** villager at known position P. Manually give the villager an Iron item via `DP_Player::SetHeldItem`.
- **Step:**
  - phase 0: possess. Capture the held-item entity's parent (should be the villager).
  - phase 1: `SimulateKeyPress(ZENITH_KEY_G)`.
  - phase 2: wait 2 frames. Capture the item's world position. Assert `dist(itemPos, P) < 0.5 m`.
  - phase 3: assert `DP_Player::GetHeldItemTag(villager) == None`.
- **Verify:** position close to villager's known position, held tag cleared.

#### Test_P1Drop_PickupChainHandoff

**Proves:** the core gameplay loop — possess A, pick up item, drop at location X, switch to B (already at X), pick up.

- **Setup:** villager A near item I. Villager B near position X. Both possessable.
- **Step:**
  - phase 0: possess A. Walk into pickup range of I. Wait for pickup.
  - phase 1: walk A to X (use `SimulateKeyDown(ZENITH_KEY_W)` for movement; verify position).
  - phase 2: drop item. Switch to B.
  - phase 3: walk B into pickup range of dropped item.
  - phase 4: assert `DP_Player::GetHeldItemTag(B) == DP_ItemTag::Iron`.
- **Verify:** B holds iron at end.

### 2.5 Possession Cooldown

#### Test_P1Cooldown_CannotPossessFor1pt5s

**Proves:** GDD §4.1 — after voluntary switch, possession is denied for 1.5 s.

- **Setup:** two villagers A and B.
- **Step:** possess A. Switch to B. Immediately try `SetPossessedVillager(A)` — should refuse. Wait 90 frames (1.5 s). Try again — should succeed.
- **Verify:** immediate possess returned false, post-cooldown returned true.

#### Test_P1Cooldown_NotAffectedByDeath

**Proves:** burn-out death does *not* trigger the cooldown (only voluntary switches do).

- **Setup:** villager A. `SetRemainingLifeForTest(0.05)`.
- **Step:** possess A. Wait for death. Immediately possess B. Assert success.
- **Verify:** possession succeeded immediately after death.

### 2.6 Demon-Scent

#### Test_P1Scent_AccumulatesOnPossession

**Proves:** GDD §4.5 — demon-scent on a body = sum of recent possessions × 0.3, decays at 0.05/s.

- **Setup:** villager A.
- **Step:**
  - phase 0: assert `DP_Player::GetDemonScent(A) == 0.0`.
  - phase 1: possess A, switch away. Assert scent == 0.3.
  - phase 2: re-possess A (after cooldown). Switch away. Assert scent == 0.6.
  - phase 3: wait 6 s game time (360 frames). Assert scent ≈ 0.3 (decayed 0.05 × 6 = 0.3).
- **Verify:** all three scent checks pass with ±0.02 tolerance.

#### Test_P1Scent_NotificationToBlackboard

**Proves:** the priest's blackboard receives a HighScentTarget key when any villager's scent exceeds the hound-bark threshold (0.5).

- **Setup:** villager A. Priest in scene.
- **Step:** possess A four times in sequence (drives scent to 1.2). Read priest's `BB.HighScentTarget`. Expect = A.
- **Verify:** blackboard entry matches.

### 2.6b Visual telegraphs (GDD §4.7)

**Added 2026-05-12 round-3 peer review.** The visual telegraphs at life-timer thresholds (frost outline, walk-speed drop, burn-out vapour) are load-bearing player feedback. Tests use surrogate measurement: assert the underlying state variable rather than pixel rendering.

#### Test_P1Telegraph_WalkSpeedDropsBelow5s

**Proves:** GDD §4.7 — at <5s life remaining, walk speed drops 25%.

- **Setup:** villager.
- **Step:** possess villager, force life to 6.0 (`SetRemainingLifeForTest(6.0)`). Capture `m_fEffectiveMoveSpeed` at frames 0 (life=6, normal), 60 (life≈5, at threshold), 120 (life≈4, below threshold).
- **Verify:** speed at frame 0 == jog speed; speed at frame 120 == jog speed × 0.75 (per `Tuning.json` `visual_telegraphs.frost_outline_panic_walk_speed_multiplier`), tolerance ±0.01.

#### Test_P1Telegraph_BurnoutVapourSpawnsOnDeath

**Proves:** GDD §4.7 — on burn-out, a frost decal spawns at the body position and a vapour particle plays for 2 seconds.

- **Setup:** villager. Query `Flux_Decals::GetActiveCountForTest` (per recent engine decal work).
- **Step:** possess, `SetRemainingLifeForTest(0.05)`, wait for death. Capture decal count + active-particle count.
- **Verify:** at least one frost decal within `burnout_frost_decal_radius_m` of the death position; at least one vapour particle active for ≥ `burnout_vapour_duration_s` (2.0s).

#### Test_P1Telegraph_FrostOutlineMaterialSwapsBelow15s

**Proves:** GDD §4.7 — at <15s life, the villager's material switches to the frost-outline variant.

- **Setup:** villager.
- **Step:** possess, force life to 16.0, capture material handle. Force life to 14.0, capture again. Force life to 4.0, capture again.
- **Verify:** material handles at life=14 and life=4 differ from the life=16 handle (swap occurred at the 15s threshold). Specific material identity is implementation-defined; test asserts the swap, not which material.

### 2.6c Walk-quiet (Ctrl/L1)

**Added 2026-05-12 round-3/4 peer review.** Walk-quiet is a load-bearing stealth mechanic (MVPScope §1.1) but had no test in earlier rounds.

#### Test_P1WalkQuiet_FootstepLoudnessHalved

**Proves:** GDD §4.2 + Tuning.json `movement.walk_footstep_loudness_multiplier: 0.5` — when the player holds Ctrl/L1, the moving villager's footstep emissions register at half loudness.

- **Setup:** villager. Subscribe to `Zenith_AudioBus::GetEmittedSoundsForTest` (MVP-0.4.1).
- **Step:** possess. Walk normally for 60 frames (1s); capture average emitted-loudness for `footstep_*` events. Then `SetKeyHeld(ZENITH_KEY_LCTRL, true)`. Walk-quiet for 60 frames; capture average.
- **Verify:** walk-quiet average ≈ 0.5 × normal-walk average, ±0.05.

#### Test_P1WalkQuiet_AelfricEffectiveHearingHalved

**Proves:** GDD §4.2 — Aelfric's effective detection range against this villager's footsteps halves when walking quiet.

- **Setup:** villager 25 m from Aelfric. Aelfric's `hearing_range_m: 30.0` from Tuning.json — normally hears the villager at 25 m walking; should NOT hear at 25 m when walking quiet (effective range = 30 × 0.5 = 15 m).
- **Step:** possess. Walk (no Ctrl). Wait 60 frames. Capture `BB.InvestigatePos` — Aelfric should have heard footsteps → set position. Then move villager out, reset, walk-quiet (Ctrl held). Wait 60 frames. Capture `BB.InvestigatePos` — should be unchanged (didn't hear).
- **Verify:** first capture has investigate pos set; second capture does not.

### 2.7 Sprint as Life-Cost

#### Test_P1Sprint_DrainsLifeFaster

**Proves:** GDD §4.2 — sprint costs 3 s/s extra timer drain.

- **Setup:** villager A. Set life to 20 s.
- **Step:**
  - phase 0: possess, hold Shift (`SetKeyHeld(ZENITH_KEY_LSHIFT, true)`), no WASD.
  - phase 1: wait 60 frames (1.0 s game-time).
  - phase 2: assert life ≈ 20 - 4 = 16 s (1 s wall-time × (1.0 base + 3.0 sprint extra)) with ±0.2 s tolerance.
- **Verify:** drain matches expected.

#### Test_P1Sprint_NoDrainWhenNotMoving

**Proves:** sprint penalty only applies when *moving*. Holding Shift while idle does not drain extra.

- **Setup:** as above.
- **Step:** possess. Hold Shift. **Do not press WASD.** Wait 1 s. Expect drain ≈ 1.0 s only.
- **Verify:** drain matches base rate.

### 2.8 Possession Range from Last Anchor

#### Test_P1Range_RefusedOutOfRange

**Proves:** GDD §4.1 — possession range is 15 m from the demon's anchor.

- **Setup:** anchor at origin. Villager A at (20, 0).
- **Step:** try `SetPossessedVillager(A)`. Expect refusal (returns false / GetPossessed == INVALID).
- **Verify:** possession failed.

#### Test_P1Range_AcceptedInRange

**Proves:** the same with villager at (10, 0) succeeds.

### 2.9 Omniscience Fallback Removed

#### Test_P1Priest_DoesNotChasePossessedOutOfSight

**Proves:** [Shortfalls §6.2] — the `DP_Player::GetPossessedVillager()` fallback in `Priest_Behaviour::BridgePerceptionToBlackboard` is gone.

- **Setup:** villager behind a wall, no line-of-sight to priest. Priest cannot hear (player is walking, not jogging).
- **Step:** possess. Walk villager around the wall (still behind it). Run 120 frames.
- **Verify:** priest's `BB.TargetWithDevil == INVALID_ENTITY_ID` for every sampled frame. Priest's path is patrol-shaped (FindPosInSuspicionSphere branch), not pursuit-shaped.

#### Test_P1Priest_PursuesAfterLineOfSight

**Proves:** the priest *does* pursue when a line-of-sight is established (regression test for the perception system itself).

- **Setup:** villager and priest in open ground, no walls.
- **Step:** possess. Wait 5 frames for sight cone to register. Capture `BB.TargetWithDevil`.
- **Verify:** target == villager entity ID.

### 2.10 `.zscen` Re-bake CI Integrity

#### Test_P1Authoring_BakedSceneEquivalentToAuthored

**Proves:** [Shortfalls §3.4] — a tools-mode-authored scene produces the same runtime state as the pre-baked `.zscen` loaded in non-tools mode.

- **Setup:** harness loads scene 1 via the editor-automation chain (tools build). Captures: villager count, door count, item-spawn count, chest count, pentagram count.
- **Step:** harness loads the pre-baked `.zscen` for scene 1 in non-tools mode (this test only runs in tools build but invokes the non-tools loader internally for parity). Captures the same numbers.
- **Verify:** all counts match exactly.

This is the **single most important pre-merge test** because authoring drift is invisible until you hit non-tools builds in QA.

### 2.11 Save / Load Scaffolding

#### Test_P1Save_RoundTripMeta

**Proves:** [Shortfalls §3.5] — the meta-progression blob (Knots, unlocks, completed-nights bitmask) survives a save → in-memory clear → load.

- **Setup:** test instrumentation hook `Zenith_SaveSystem::GetWrittenBlobsForTest()`.
- **Step:**
  - phase 0: set Knots = 42, unlock "WynstanForge_Recipe2", complete Night 1. Save.
  - phase 1: capture the written blob via the test hook. Clear in-memory state.
  - phase 2: `SetReadbackBlob(blob)`, load.
  - phase 3: read state. Assert Knots == 42, unlock present, Night 1 in bitmask.
- **Verify:** all three captures match.

#### Test_P1Save_RobustToCorruption

**Proves:** a truncated save-blob causes a graceful "no progress" load, not a crash.

- **Setup:** test hook.
- **Step:** inject a 4-byte truncated blob. Attempt load. Assert no assert fires (test would terminate otherwise); assert default state.
- **Verify:** load returned a default state (Knots = 0).

---

## 3. Tier 2 — Phase 2: Depth (months 4–10)

**MVP vs post-MVP tagging** (added 2026-05-12 round-5 peer review). Tier 2 was contaminated with tests for systems explicitly deferred from MVP (hounds, priest variants, charms, distractions, Liminal hub, full 24-archetype suite). Three reviewers in round 5 flagged this. The tags below clarify which Tier 2 tests are MVP scope vs which are post-MVP (and therefore should NOT be authored during the MVP build).

| Sub-section | MVP scope? |
|---|---|
| §3.1 Villager Archetypes (4 MVP archetypes only — Farmhand, Beggar, Devout, Child) | **MVP** for the 4 archetypes; **post-MVP** for the full 24 (Test_P3Archetype_TimersMatchSpec is the post-MVP parameterised variant). |
| §3.2 Hounds | **post-MVP** entirely (hounds are post-MVP per MVPScope §2.1) |
| §3.3 Witch-Finder Variants | **post-MVP** entirely (variants are post-MVP per MVPScope §2.1) |
| §3.4 Reagent Uniqueness — but only the 5 MVP reagents (BogWaterEvaporates, BellSoulRingsBell, UniquePickupChannel) | **MVP** for those 5; **post-MVP** for the other 9 reagents' unique-behaviour tests |
| §3.5 Crafting Expansion (the 3 MVP recipes: Iron→Brass Key, Iron+Wood→Spike, Iron+Brass→Skeleton) | **MVP** |
| §3.6 Charms | **post-MVP** entirely (charms are post-MVP per MVPScope §2.2) |
| §3.7 Distractions | **post-MVP** entirely (distractions are post-MVP per MVPScope §2.2) |
| §3.8 Liminal & Meta-Progression | **post-MVP** entirely (Liminal/Knots are post-MVP per MVPScope §2.2) |
| §3.9 Volumetric Fog & Visibility | **MVP** (3 tests, all in MVPScope §1.4 subset) |

**Agents authoring MVP tests must filter Tier 2 by this table.** Any Tier 2 test in a post-MVP-only sub-section is `#ifdef DP_POST_MVP_TIER2`-guarded and NOT in the MVP roadmap. The "Always-On Tests" table in §6 lists the 20-test PR gate; that's the operative MVP gate, not the full Tier 2 surface.

### 3.1 Villager Archetypes

#### Test_P2Archetype_TimersMatchSpec (MVP)

**Proves:** GDD §6.2 + MVPScope §1.2 — each MVP archetype has the timer specified in Archetypes.json.

- **Setup:** load a Gym scene with one instance of each MVP archetype (test-only `Gym_Archetypes_MVP` scene authored via editor automation).
- **Step:** iterate `DP_Query::ForEachScriptInActiveScene<DPVillager_Behaviour>`. For each, query its archetype enum and `GetMaxLife()`. Compare to the **MVP expectation table below**.
- **Verify:** every villager's max-life matches table; the scene contains exactly 4 distinct archetypes.

Expectation table for MVP (in code, `Tests/Test_P2Archetype_TimersMatchSpec.cpp`):

```cpp
struct ArchetypeExpect { DP_ArchetypeId eId; float fMaxLife; };
static const ArchetypeExpect kExpectMVP[] = {
    { DP_Archetype::Farmhand,    30.0f },
    { DP_Archetype::Beggar,      25.0f },
    { DP_Archetype::Devout,      30.0f },
    { DP_Archetype::Child,       15.0f },
};
```

**Important:** the test must reject any extra archetypes added post-MVP from entering the MVP gym scene. Source-of-truth is `Config/Archetypes.json` entries with `"mvp": true`. Sexton was originally an MVP archetype but moved to post-MVP per 2026-05-12 reconciliation (the `can_enter_chapel_unseen` ability requires chapel-bounds + sight-cone integration that would have made the Sexton mechanically identical to a Farmhand in MVP). Beggar replaced it: shorter timer (25s) plus `aelfric_ignores` ability gives a genuinely distinct gameplay role and is testable via a 1-day BT filter.

#### Test_P3Archetype_TimersMatchSpec (post-MVP)

**Proves:** all 24 designed archetypes have correct timers.

- **Setup:** post-MVP scene `Gym_Archetypes_Full` containing one of every archetype.
- **Step:** same as above but iterates all entries in `Config/Archetypes.json` (whether `"mvp": true` or not).
- **Verify:** every archetype's max-life matches its JSON entry.

This is a parameterised test — adding a new archetype to Archetypes.json automatically extends the test's expectation set. It is **post-MVP scope** and lives behind a `#ifdef DP_POST_MVP_ARCHETYPES` guard until those archetypes are implemented.

#### Test_P2Archetype_DevoutChannel

**Proves:** Devout villagers have a 0.8 s possession channel; possession only succeeds if uninterrupted.

- **Setup:** Devout villager in scene.
- **Step:**
  - phase 0: try `SetPossessedVillager`. Assert `IsPossessing` channel started, `IsPossessed` is still false.
  - phase 1: wait 30 frames (0.5 s). Assert still `IsPossessing`, `IsPossessed == false`.
  - phase 2: wait 30 more frames (1.0 s total). Assert `IsPossessed == true`.
- **Verify:** all three states correct.

#### Test_P2Archetype_DevoutChannelInterrupt

**Proves:** Aelfric line-of-sight interrupts the channel (per GDD).

- **Setup:** Devout villager. Place Aelfric with line-of-sight, motionless.
- **Step:** start possession channel. Wait 30 frames. Assert channel was cancelled. Assert `IsPossessed == false`.
- **Verify:** channel state == Cancelled.

#### Test_P2Archetype_ChildHalfTimer

**Proves:** Child archetype has 15 s timer.

- **Setup:** Child villager.
- **Step:** possess. Capture initial life. Assert == 15.0 ±0.1.

#### Test_P2Archetype_ChildInvisibleToHounds

**Proves:** hounds within 6 m of a possessed Child do *not* bark.

- **Setup:** Child villager, hound 4 m away. High demon-scent on the Child (force to 1.0).
- **Step:** possess Child. Wait 30 frames. Capture hound's `m_uBarkEventCount`.
- **Verify:** bark count == 0.

#### Test_P2Archetype_ChildCannotCarryTools

**Proves:** Child possession refuses to pick up Iron, Wood, Brass Key (tools), but accepts reagents.

- **Setup:** Child, Iron at child's feet, Reagent_HaresTongue at child's feet.
- **Step:** possess. Wait 5 frames. Iron should *not* be auto-equipped. Reagent should be.
- **Verify:** `GetHeldItemTag == Objective1`.

For each archetype, a corresponding `Test_P2Archetype_<Name><Ability>` test exists. The pattern is uniform: setup the archetype, exercise the ability, query state. Tests Test_P2Archetype_Carter_FastMovement, Test_P2Archetype_SmithApp_FreeForge, Test_P2Archetype_BellRinger_SilenceImmunity, etc. follow the same template.

### 3.2 Hounds

#### Test_P2Hounds_StayNearAelfric

**Proves:** Aram and Tobit remain within 15 m of Aelfric.

- **Setup:** Aelfric + 2 hounds in open scene. Make Aelfric patrol (no target).
- **Step:** run 300 frames. Every 30 frames, sample both hound-Aelfric distances.
- **Verify:** every sample has `dist <= 15.5 m` (small overshoot tolerance).

#### Test_P2Hounds_BarkOnHighScent

**Proves:** hound within 6 m of high-demon-scent body emits a `DP_OnHoundBark` event.

- **Setup:** villager with forced scent = 1.0. Hound 4 m away.
- **Step:** subscribe to `DP_OnHoundBark`. Wait 60 frames.
- **Verify:** event count >= 1.

#### Test_P2Hounds_DistractedByMeat

**Proves:** raw-meat item dropped within hound perception radius makes the hound move toward the meat.

- **Setup:** hound at (0, 0). Drop raw-meat item at (10, 0).
- **Step:** capture hound start position. Run 180 frames. Capture end position.
- **Verify:** `dist(end, (10, 0)) < dist(start, (10, 0)) - 5 m`.

### 3.3 Witch-Finder Variants

#### Test_P2Variant_Cautious_LongerMemory

**Proves:** Cautious variant retains last-investigated waypoint for 30 s instead of 8 s.

- **Setup:** set Aelfric variant = Cautious. Emit a noise stimulus. Capture waypoint set.
- **Step:** wait 25 s game-time. Assert waypoint still present. Wait 10 more. Assert waypoint gone.
- **Verify:** memory window correct.

#### Test_P2Variant_Cruel_FastApprehend

**Proves:** Cruel variant apprehends in 1.5 s, not 3.0 s.

- **Setup:** Cruel Aelfric, motionless villager in range.
- **Step:** possess. Wait 90 frames (1.5 s + small). Assert `DP_OnRunLost` fired.
- **Verify:** event count == 1 within 100 frames.

#### Test_P2Variant_Drunk_SkipsEveryFourth

**Proves:** Drunk variant ignores every fourth stimulus (deterministic).

- **Setup:** Drunk Aelfric. Emit 12 distinct noise stimuli with 30 frames between each.
- **Step:** capture how many stimuli triggered an investigate-branch transition.
- **Verify:** investigate count == 9 (12 minus every fourth = 12 - 3 = 9).

### 3.4 Reagent Uniqueness

#### Test_P2Reagent_BogWaterEvaporates

**Proves:** the bog-water reagent, once dropped, despawns 8 s later.

- **Setup:** possess villager carrying bog-water phial. Get item entity ID.
- **Step:** drop. Capture entity validity at frame 0, 240 (4 s), 480 (8 s), 540 (9 s).
- **Verify:** valid at 0, 240; invalid at 480, 540 (gives a 1-frame leeway).

#### Test_P2Reagent_BellSoulRingsBell

**Proves:** picking up the Bell's Soul reagent dispatches a `DP_OnBellRing` event.

- **Setup:** subscribe to `DP_OnBellRing`. Villager near bell-clapper item.
- **Step:** possess, walk into pickup range.
- **Verify:** `DP_OnBellRing` fired exactly once with `m_xSource == clapper-entity`.

#### Test_P2Reagent_UniquePickupChannel

**Proves:** reagents take 1.0 s to pick up, tools take 0 s.

- **Setup:** villager at iron item and reagent item.
- **Step:** approach iron. Wait 5 frames. Assert held == Iron. Drop. Approach reagent. Wait 5 frames. Assert NOT held. Wait 60 frames (1.0 s). Assert held == reagent.
- **Verify:** instant vs. delayed pickup matches design.

### 3.5 Crafting Expansion

#### Test_P2Forge_WoodToSpike

**Proves:** Wood at forge = Spike (single-input recipe; the original spec said Iron + Wood but the shipped recipe in PR #63 is the simpler Wood -> Spike, matching `DPForge_Behaviour`'s per-instance recipe table).

- **Setup:** villager at forge holding Wood.
- **Step:** interact (F). Wait 1 s.
- **Verify:** held == Spike, Wood entity destroyed, craft-count incremented. **Shipped** via [`Test_P2Forge_WoodToSpike.cpp`](../Tests/Test_P2Forge_WoodToSpike.cpp) (PR #63).

#### Test_P2Forge_IronToSkeletonKey

**Proves:** Iron at forge = SkeletonKey (originally specced as Iron + Brass Key; the shipped MVP recipe per `DPForge_Behaviour` is Iron -> SkeletonKey).

- **Setup:** villager at forge holding Iron.
- **Step:** interact. Wait 1 s.
- **Verify:** held == SkeletonKey. **Shipped** via [`Test_P2Forge_IronToSkeletonKey.cpp`](../Tests/Test_P2Forge_IronToSkeletonKey.cpp) (PR #63).

#### Test_P2Forge_PriestHearsTheHammer

**Proves:** the forge's hammer sound emission reaches the priest's perception system (not just the AudioBus instrumentation). Caught a real wiring bug (PR #73): the forge originally only emitted via `Zenith_AudioBus::EmitSound`, missing the `Zenith_PerceptionSystem::EmitSoundStimulus` call the priest's hearing path needs.

- **Setup:** load GameLevel, pick priest + a villager, possess the villager, teleport to forge.
- **Step:** drive a forge interact, tick frames for the priest's BridgePerceptionToBlackboard to observe the heard sound.
- **Verify:** priest's `BB.HasInvestigatePos` flips true. **Shipped** via [`Test_P2Forge_PriestHearsTheHammer.cpp`](../Tests/Test_P2Forge_PriestHearsTheHammer.cpp) (PR #73).

#### Test_P2Forge_AudibleAt30m

**Proves:** forge use emits a sound stimulus at loudness ≥ 0.8 reaching 30 m.

- **Setup:** subscribe to `Zenith_AudioBus::GetEmittedSoundsForTest` (Phase 1 instrumentation).
- **Step:** interact at forge. Capture the emitted sound array.
- **Verify:** at least one entry with `name == "forge_hammer"` and `radius >= 30.0`.

### 3.6 Charms

#### Test_P2Charm_RowanNegatesOneDetection

**Proves:** holding Rowan converts the next sight-detection event into a no-op, then consumes the charm.

- **Setup:** villager with Rowan. Aelfric in line-of-sight.
- **Step:**
  - phase 0: capture `BB.TargetWithDevil`. Expect INVALID (rowan blocking).
  - phase 1: capture inventory after 1 sight-frame. Assert Rowan was consumed.
  - phase 2: capture `BB.TargetWithDevil` next frame. Expect villager (rowan gone, detection resumes).
- **Verify:** all three states correct.

#### Test_P2Charm_SaltPouchSingleUse

**Proves:** salt pouch negates one bark event, then consumes itself.

- **Setup:** villager holds salt. Hound within 6 m. High scent.
- **Step:** capture bark events at frame 30 (expect 0 — salt absorbing), frame 31 (charm consumed). At frame 60, capture again (expect ≥ 1 — bark now fires).
- **Verify:** bark count window matches.

### 3.7 Distractions

#### Test_P2Distract_MusicBoxEmits8s

**Proves:** music box, once triggered, emits sound stimuli for 8 s.

- **Setup:** subscribe to perception emissions.
- **Step:** trigger box. Capture emission count per second for 12 seconds.
- **Verify:** emissions occur each second from 0 to 7 inclusive; zero from 8 onward.

#### Test_P2Distract_TinderBundleIgnites30s

**Proves:** the tinder bundle creates a light source that lasts 30 s, then despawns.

- **Setup:** capture initial light count via `DP_Fog::GetFogHoleCount` or equivalent.
- **Step:** trigger bundle. Wait 5 frames. Assert hole count + 1. Wait 1800 frames (30 s). Assert hole count returned to baseline.
- **Verify:** hole appeared and disappeared at correct time.

### 3.8 Liminal & Meta-Progression

#### Test_P2Liminal_KnotEarnedOnInscribe

**Proves:** each reagent inscribed at pentagram = +1 Knot in the meta-state.

- **Setup:** clear meta-state Knots = 0.
- **Step:** trigger an inscribe via `DPPentagram::HandleInteract` for each of 5 objectives.
- **Verify:** Knots == 5 after.

#### Test_P2Liminal_ChainBonus

**Proves:** uninterrupted hand-off chains bonus +1 Knot per chain link.

- **Setup:** simulate a 3-link hand-off.
- **Step:** sequence: possess A, pick up reagent, drop. Possess B, pick up, drop. Possess C, pick up, inscribe.
- **Verify:** Knots == base 1 + chain bonus 2 = 3.

#### Test_P2Liminal_UnlockPersists

**Proves:** purchasing a hermit unlock persists into the next Night.

- **Setup:** Knots = 5. Buy `WynstanForge_Recipe2` (cost 3).
- **Step:** save → clear in-memory state → load.
- **Verify:** unlock list contains `WynstanForge_Recipe2`, Knots == 2.

### 3.9 Volumetric Fog & Visibility

#### Test_P2Fog_MemoryDimsAfter10s

**Proves:** tiles seen 10 s ago are tagged as "memory" in the fog buffer.

- **Setup:** villager A walks past coordinate X. Tag X as seen at frame 0.
- **Step:** walk far away. Sample fog state at X at frames 0, 600 (10 s), 1200 (20 s), 1800 (30 s).
- **Verify:** state == Visible at 0, Memory at 600 & 1200, Hidden at 1800.

#### Test_P2Fog_AelfricNotRevealed

**Proves:** Aelfric does *not* cut a fog hole around himself.

- **Setup:** villager near Aelfric.
- **Step:** capture `DP_Fog::GatherFogHolePositions` array. Assert no entry's position is within 1 m of Aelfric's position.
- **Verify:** correct.

#### Test_P2Fog_LightAddsHole

**Proves:** each `Zenith_Light` in scene contributes one fog hole sized to its falloff.

- **Setup:** scene with 4 lights of known ranges.
- **Step:** capture fog-hole array. Filter entries that match the light positions ±0.1 m.
- **Verify:** 4 matches, each with `w == light.range + 0.5`.

---

## 4. Tier 3 — Phase 3: Content Validation (months 10–14)

### 4.1 Content Inventory

#### Test_P3Inventory_ArchetypeCount

**Proves:** GDD §6.2 — all 24 archetypes are registered and instantiable.

- **Setup:** none.
- **Step:** iterate the archetype registry (new `DP_Archetypes::GetAllRegistered()` query). Count entries.
- **Verify:** count == 24.

For each archetype, the parameterised test runs Setup → instantiate → query timer + ability → destroy. Failure here means an archetype regressed.

#### Test_P3Inventory_ReagentCount

**Proves:** all 14 reagents are registered.

- **Setup:** none.
- **Step:** `DP_Reagents::GetAllRegistered()`. Count.
- **Verify:** count == 14.

#### Test_P3Inventory_AllReagentSpawnAnchorsValid

**Proves:** every reagent's spawn-candidate set references at least one valid spawn anchor in the level.

- **Setup:** load Vexholme. Build a side-table of anchor positions.
- **Step:** iterate every reagent's spawn-candidate set. For each, assert at least one anchor exists in the level.
- **Verify:** zero unmatched candidates.

#### Test_P3Inventory_PropAssetIntegrity

**Proves:** every interactable kind has a non-null model handle and material handle (no missing assets).

- **Setup:** load Vexholme.
- **Step:** iterate every `DPInteractable_Behaviour` in scene. For each, query the model component and assert `pxModelInstance != nullptr` AND material count > 0.
- **Verify:** zero null handles.

### 4.2 Procedural Shufflers

#### Test_P3Procgen_FiveOfTwentyFiveReagents

**Proves:** GDD §5.5 — reagent shuffler picks 5 anchors per Night.

- **Setup:** seed RNG to known value (42). Load Night 1.
- **Step:** count reagent entities spawned. Capture their anchor IDs.
- **Verify:** count == 5 AND all 5 IDs are distinct AND drawn from the candidate set.

#### Test_P3Procgen_SeedDeterminism

**Proves:** same seed → same world.

- **Setup:** none.
- **Step:** load Night 1 with seed 42 twice. Capture spawn-anchor list each time.
- **Verify:** lists are identical.

#### Test_P3Procgen_DifferentSeedsDifferent

**Proves:** different seeds → reasonably different worlds (regression against accidental determinism bug).

- **Setup:** none.
- **Step:** load Night 1 with seed 1 then seed 2. Capture both anchor lists.
- **Verify:** the intersection is at most 4 (out of 5) anchors (allows incidental overlap but flags constant output).

#### Test_P3Procgen_VillagerCountInBand

**Proves:** villager-set drawer produces 14–18 villagers.

- **Setup:** none.
- **Step:** for each seed in [1..10], load Night 1 and count villagers.
- **Verify:** every count in [14, 18].

#### Test_P3Procgen_AelfricVariantDrawn

**Proves:** exactly one Aelfric variant is active per Night.

- **Setup:** seed RNG.
- **Step:** load Night 1 ten times. Capture variant each time.
- **Verify:** every load returned one of {Cautious, Cruel, Drunk}; over 10 runs, at least 2 distinct variants seen.

### 4.3 Localisation

#### Test_P3Loc_AllStringsHaveAllLanguages

**Proves:** every string key has an entry in all 8 languages.

- **Setup:** `Zenith_Localisation::GetAllKeys()` returns the key set; for each language, `IsKeyPresent(key)` returns presence.
- **Step:** outer loop: each language. Inner loop: each key. Count missing.
- **Verify:** missing == 0.

#### Test_P3Loc_NoTruncation

**Proves:** no UI string in any language exceeds its element's width budget.

- **Setup:** UI elements have a `m_iMaxCharsForTest` field. Every string is checked against it.
- **Step:** for each language, for each (element, string-key) pair: `ASSERT len(localised(key, lang)) <= element.m_iMaxCharsForTest`.
- **Verify:** zero overruns.

#### Test_P3Loc_NoUnescapedFormatSpecifiers

**Proves:** localised strings don't accidentally introduce printf-style format mismatches.

- **Setup:** none.
- **Step:** iterate all strings. Compare format-specifier counts between reference (English) and each translation.
- **Verify:** every locale matches English's specifier list.

### 4.4 Performance Proxies

These tests don't measure wall-clock (the harness uses fixed-dt). Instead they measure **operation counts** — how many entity touches, how many path queries, how many fog-hole writes per frame.

#### Test_P3Perf_NavQueriesPerFrameUnderBudget

**Proves:** the priest + 2 hounds together perform ≤ 6 navmesh queries per frame.

- **Setup:** scene with Aelfric, 2 hounds, 18 villagers.
- **Step:** instrument `Zenith_NavMesh::FindPath` with a per-frame counter. Run 600 frames. Capture max.
- **Verify:** max ≤ 6.

#### Test_P3Perf_FogHoleCountUnderBudget

**Proves:** fog-hole count ≤ 64 per frame in the worst-case scene (Night 6, 18 villagers + 30 torches).

- **Setup:** load Night 6.
- **Step:** every frame for 300 frames, capture `DP_Fog::GetFogHoleCount`. Track max.
- **Verify:** max ≤ 64.

#### Test_P3Perf_EntityIterationStableOver5Minutes

**Proves:** no scene-tree leak — entity count after 5 minutes of play equals start ± expected delta.

- **Setup:** load Night 1, count entities.
- **Step:** run 18,000 frames (5 min @ 60 Hz fixed). Re-count.
- **Verify:** delta ≤ 20 (allows for transient particles, dropped items).

### 4.5 Cutscene Triggers (Phase 3 stubs for Phase 4 polish)

#### Test_P3Cutscene_StartsOnNightLoad

**Proves:** loading Night 1 dispatches `DP_OnCutsceneRequested` with id "Night1_Opening".

- **Setup:** subscribe.
- **Step:** load Night 1. Wait 5 frames.
- **Verify:** one event with id == "Night1_Opening".

#### Test_P3Cutscene_SkipAdvancesToGameplay

**Proves:** `SimulateKeyPress(ZENITH_KEY_ESCAPE)` during cutscene advances to gameplay state.

- **Setup:** trigger Night 1 opening cutscene.
- **Step:** confirm `Zenith_GameState::GetMode() == Cutscene`. Press Esc. Wait 5 frames.
- **Verify:** mode == Playing.

---

## 5. Tier 4 — Phase 4: Ship Readiness (months 14–18)

### 5.1 Full-Run Playthroughs

The biggest tests in the suite. Each scripts a full Night start-to-end and asserts win/loss state.

**Critical: all Tier 4 playthrough tests use `Zenith_NavMeshTestPathfinder::ComputePath` (the navmesh-aware wrapper landing in MVP-1.2.9), NOT the existing `Test_HumanPlaythrough.cpp:432` grid-based `ComputePathAStar`.**

**Reconciled 2026-05-12 round-4 peer review.** The earlier wording in this section said tests should reuse the existing `ComputePathAStar` from the prototype. That function is a grid pathfinder over a separate occupancy buffer — it does NOT consult the navmesh. Once MVP-1.2 lands real navmesh with door portals, the grid pathfinder would route bots through walls (test bots could "win" by cheating). MVP-1.2.9 retires `ComputePathAStar` from the test suite and replaces it with `Zenith_NavMeshTestPathfinder::ComputePath` which wraps the real `Zenith_NavMesh::FindPath`. **Tier 4 tests use the navmesh-aware API only.**

**Why a real-navmesh-aware pathfinder is required:** Raw timed key-holds (`SetKeyHeld(W, true) for 5 s`) flake the moment a collider is nudged. A grid-A* pathfinder routes through walls. The harness must own:
- Path planning that uses the same navmesh the priest uses.
- Per-frame replanning when blocked (e.g. a door closed in the priest's wake — and the door's `SyncNavMeshBlock` disables the matching portal).
- WASD-input synthesis from path waypoints (transforms `Vec3 path[i+1] - pos` into camera-relative WASD presses).
- Stuck-detection: if position hasn't moved >0.5 m in 60 frames, abort the leg, re-plan from current position; if 3 consecutive re-plans fail, fail the test with diagnostic.

Tier 4 tests are **blocked on MVP-1.2 (real navmesh) completing**. They cannot be authored until the navmesh generator is integrated; the prototype's flat-quad navmesh would let the playthrough cheat through walls.

#### Test_P4Playthrough_Night1WinGolden

**Proves:** the canonical Night 1 win path completes within budget on real geometry.

- **Setup:** load Night 1, seed = "tutorial" (tutorial seed is deterministic and authoritative). Capture the 5 reagent entity IDs.
- **Step:** state machine over **legs**, not over fixed frame counts. For each of the 5 reagents (selected nearest-first):
  1. **Pick body:** find the closest possessable villager to the reagent's position via on-navmesh distance; `DP_Player::SetPossessedVillager(id)`.
  2. **Path to reagent:** `Zenith_NavMeshTestPathfinder::ComputePath(villager_pos, reagent_pos, outWaypoints)`; assert returns true and waypoints non-empty.
  3. **Drive:** per frame, `DriveWASDToward(next waypoint at 0.5m tolerance)` via `SetKeyHeld`. Replan if stuck.
  4. **Pickup:** wait until `DP_Player::GetHeldItemTag(villager) == reagent.tag`.
  5. **Path to pentagram:** repeat.
  6. **Inscribe:** at <2m from pentagram, `SimulateKeyPress(ZENITH_KEY_F)`; wait for inscription event.
- **Max frames:** 9000 (2.5 min @ 60 Hz fixed). Most legs complete in ~300 frames; budget is generous.
- **Verify:** `DP_Win::HasWon() == true`; no `DP_OnRunLost`; ≤1 `DP_OnVillagerDied` event (allows one burn-out per run on the worst leg).

#### Test_P4Playthrough_Night1LossByApprehend

**Proves:** when the player stands still in Aelfric's pursuit cone, the run loses by apprehension.

- **Setup:** authored test scene with priest placed 8 m from a villager's spawn anchor, in line-of-sight. Subscribe to `DP_OnRunLost`.
- **Step:** `SetPossessedVillager(villager)`. Pin the villager (no input). Wait until run-lost event fires or max frames hit.
- **Max frames:** 600 (priest closes 8 m at 7 m/s in ~1.2 s, then 3 s apprehend channel = ~5 s budget; we give 10 s).
- **Verify:** `DP_OnRunLost` fired exactly once with cause `Apprehended`. No `DP_OnVictory`.

#### Test_P4Playthrough_Night1LossByDawn

**Proves:** dawn timeout ends the run when objectives not completed.

- **Setup:** Night 1 scene. Override `DP_Tuning::SetForTest("night.dawn_timer_s", 30.0)` so the run ends quickly.
- **Step:** possess a villager. Pin (no input). Wait 35 s game-time (2100 frames @ 60Hz fixed).
- **Max frames:** 2200.
- **Verify:** `DP_OnRunLost` fired exactly once with cause `Dawn`. No `DP_OnVictory`.

#### Test_P4Playthrough_Night1LossByNoVillagers

**Proves:** running out of villagers ends the run.

- **Setup:** authored test scene with 2 villagers only. Override dawn timer to large value so it doesn't trigger.
- **Step:** possess each in sequence; let each burn out (use `SetRemainingLifeForTest(0.05)` per villager to accelerate). Wait for second burn-out + dispatch.
- **Max frames:** 300 (each burn-out + dispatch is ~30 frames).
- **Verify:** `DP_OnRunLost` fired exactly once with cause `NoVessels`.

#### Test_P4Playthrough_Night5_FreeJoan

**Proves:** Night 5's win path includes freeing Joan from the longhouse.

- **Setup:** Night 5, seed = "joan".
- **Step:** scripted path: get the longhouse key from chest, unlock door, interact with Joan, then collect reagents and inscribe.
- **Verify:** `DP_OnJoanFreed` AND `DP_OnVictory` both fired.

#### Test_P4Playthrough_Night7_NoProcgen

**Proves:** Night 7's layout is deterministic regardless of seed.

- **Setup:** load Night 7 with seed = 1 then 2 then 999.
- **Step:** capture villager + reagent + interactable counts and positions for each load.
- **Verify:** all three loads produce identical state.

#### Test_P4Playthrough_AllSevenNights_CampaignArc

**Proves:** completing Nights 1–7 in sequence produces the canonical ending state.

- **Setup:** clear save. Run a scripted golden path through each Night in sequence.
- **Max frames:** 90,000 (25 min game-time @ fixed 60 Hz).
- **Verify:** save state contains `EndingId == "Ending_A"` (the canonical golden path's ending).

This is the **acceptance test** for the entire campaign. It pre-flights every Night's win condition in a single CI run.

### 5.2 Console Input Parity

Console builds use gamepad input mapped to the same actions. The simulator extends to `SimulateGamepadButtonPress` and `SimulateGamepadStick` (Phase 1 engine work).

#### Test_P4Gamepad_PossessOnSouthButton

**Proves:** A / X gamepad button triggers the same possession action as left-click on the cursor target.

- **Setup:** position cursor over villager (`SimulateMousePosition(x, y)`). Switch input source to gamepad.
- **Step:** `SimulateGamepadButtonPress(SOUTH)`. Wait 2 frames.
- **Verify:** villager is possessed (same outcome as left-click).

#### Test_P4Gamepad_LeftStickMovesPossessed

**Proves:** left stick directional input drives the same movement as WASD.

- **Setup:** possess villager. Capture position P0.
- **Step:** `SimulateGamepadStick(LEFT, 1.0, 0.0)` (full right) for 60 frames.
- **Verify:** `position.x > P0.x + 5.0 m` (movement happened).

#### Test_P4Gamepad_StartPauses

**Proves:** Start / Options button pauses (same as Esc).

- **Setup:** in-game.
- **Step:** `SimulateGamepadButtonPress(START)`. Capture pause state.
- **Verify:** `Zenith_GameState::IsPaused() == true`.

### 5.3 Console Certification Proxies

These tests proxy the certification requirements that *can* be tested in-process. The remaining requirements (real controller disconnect, real suspend/resume, friend-list integration) need platform-specific test rigs and are tracked separately.

#### Test_P4Cert_ControllerDisconnectPauses

**Proves:** TRC-001 — controller disconnect mid-game pauses the game.

- **Setup:** in-game.
- **Step:** `Zenith_InputSimulator::SimulateGamepadDisconnect(0)`. Wait 5 frames.
- **Verify:** `Zenith_GameState::IsPaused() == true` AND a controller-reconnect prompt UI element is visible.

#### Test_P4Cert_SuspendResumePreservesState

**Proves:** TCR-1.3.1 — suspend/resume preserves game state.

- **Setup:** in-game; capture full game state via test hook.
- **Step:** `Zenith_PlatformLifecycle::SimulateSuspend()`. Wait 30 frames in suspended state. `SimulateResume()`. Capture state again.
- **Verify:** state matches (entity count, possession state, life timer, objectives mask).

#### Test_P4Cert_SaveCorruptionRecovery

**Proves:** cert requirement — game must not crash on corrupted save.

- **Setup:** inject deliberately corrupted save blob.
- **Step:** attempt load. Confirm fallback to default state. Continue gameplay for 60 frames.
- **Verify:** no asserts fired, game playable.

#### Test_P4Cert_AchievementFires

**Proves:** completing Night 1 fires the `Achievement_Night1Complete` callback.

- **Setup:** subscribe to `Zenith_Achievements::OnUnlockForTest`.
- **Step:** run Test_P4Playthrough_Night1WinGolden's path.
- **Verify:** unlock fired with id `Achievement_Night1Complete`.

### 5.4 Accessibility

#### Test_P4Access_TimeScale60Pct

**Proves:** with time-scale = 0.6, the game-time clock advances 60% as fast.

- **Setup:** set `Zenith_GameTime::SetTimeScale(0.6f)`. Possess villager.
- **Step:** wait 60 real frames (1.0 s). Capture life drain.
- **Verify:** life drain ≈ 0.6 s ±0.05.

#### Test_P4Access_WhisperLineHasEveryDecision

**Proves:** every priest decision fires a corresponding whisper-line entry.

- **Setup:** subscribe to both `Zenith_PerceptionSystem::OnStimulusReceived` and `DP_HUD::OnWhisperLineUpdated`.
- **Step:** emit 5 noise stimuli. Wait 60 frames.
- **Verify:** whisper line was updated 5 times.

#### Test_P4Access_HighContrastOutlineEnabled

**Proves:** toggling high-contrast adds outline material to every interactable.

- **Setup:** capture material list of every interactable.
- **Step:** `Zenith_Settings::SetHighContrast(true)`. Capture again.
- **Verify:** every interactable has a "HC_Outline" material entry where it didn't before.

#### Test_P4Access_SingleStickPlay

**Proves:** in single-stick mode, A-button auto-targets the nearest visible villager for possession.

- **Setup:** enable single-stick. Place 3 villagers at known distances.
- **Step:** `SimulateGamepadButtonPress(SOUTH)`. Wait 2 frames.
- **Verify:** nearest villager is possessed.

### 5.5 Replay Debugger

#### Test_P4Replay_LastTenSecondsRecorded

**Proves:** the replay system records the last 10 s of input by default.

- **Setup:** `Zenith_Replay::SetEnabled(true)`. Possess villager.
- **Step:** drive scripted input (W for 3 s, D for 3 s, Esc, then S for 3 s). Capture replay buffer.
- **Verify:** buffer length == 10 s, every input event present.

#### Test_P4Replay_PlaybackReproducesState

**Proves:** replaying the recorded input produces identical final state.

- **Setup:** run a 10-second canned input. Capture final state S1. Save replay.
- **Step:** reset scene. Play back replay.
- **Verify:** final state S2 matches S1 (entity positions, possession state, objectives).

This is the **production tuning instrument** called out in GDD §13.2. Without it, Aelfric's behaviour cannot be tuned with confidence.

---

## 6. Always-On Tests (run every commit)

A subset of the above runs on every commit via PR-gating CI:

| Tier | Tests | Why on every commit |
|---|---|---|
| T0 | All 6 | Catches harness regressions immediately. |
| T1 — Pause | 3 | Pause is a stealth-game showstopper. |
| T1 — Apprehend | 3 | Loss states are easy to break. |
| T1 — Save | 2 | Persistence corruption is recoverable only with this test catching it. |
| T1 — Authoring parity | 1 | The `.zscen` drift bug is silent. |
| T1 — Priest omniscience | 2 | Regression-prone. |
| T2 — Archetype timers | 1 | Single test, covers all archetypes' max-life. |
| T2 — Volumetric fog | 1 (`AelfricNotRevealed`) | Cheap, catches fog-of-war regression. |
| T4 — Night1 golden playthrough | 1 | The single best regression test for the entire game. |

Total: 20 tests, ~6 min batch runtime @ fixed-dt 60 Hz. Acceptable for PR gating.

The remaining tests run **nightly** in a larger ~45-minute batch and on every release-branch tag.

---

## 7. Continuous Integration

### 7.1 PR gating

```yaml
# .github/workflows/dp-pr.yml (sketch)
- name: Build (tools, debug)
  run: cmd.exe /c "Build\\CleanBuild.bat Build\\zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount"
- name: Sanity batch
  run: pwsh.exe -File zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --tier 0,1 --headless --fail-fast
- name: Golden playthrough
  run: pwsh.exe -File zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --filter Night1WinGolden --headless
```

### 7.2 Nightly

```yaml
- name: Full batch
  run: pwsh.exe -File zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --headless --assertions-log build/dp_assertions.log
- name: Aggregate
  run: pwsh.exe -File Tools/aggregate_dp_results.ps1 -ResultsDir Build/artifacts/test_results/devilsplayground -OutSummary build/dp_summary.json
- name: Upload artifact
  uses: actions/upload-artifact@v4
  with:
    name: dp-nightly-results
    path: |
      Build/artifacts/test_results/devilsplayground/
      build/dp_summary.json
      build/dp_assertions.log
```

### 7.3 Release-branch gating

In addition to nightly: the release branch also runs Tier 4 in non-tools build (`vs2022_Release_Win64_False`) to catch the dead-strip issue mentioned in [Shortfalls §3.8](Shortfalls.md). Until non-tools builds are repaired, this gate is stubbed.

### 7.4 Claude-driven verification

Claude Code reads the aggregate `dp_summary.json` produced by `aggregate_dp_results.ps1`. The summary contains for each test:

```json
{
  "name": "Test_P1Apprehend_PriestCatchesPlayer",
  "passed": true,
  "max_frames": 240,
  "frames_used": 192,
  "diagnostics": {
    "apprehending_at_frame_60": true,
    "run_lost_dispatch_count": 1,
    "run_lost_cause": "Apprehended"
  }
}
```

Claude's verification loop:

1. Run `zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --headless --assertions-log ...`.
2. Read `build/dp_summary.json`.
3. For each test, check `passed == true`. If false, parse `diagnostics` and the assertions log to determine root cause.
4. Open the test source file, the implementation under test, and the diagnostic log to localise the bug.
5. Propose a fix, apply, re-run.

This loop is fully autonomous. No human inspects screenshots; no human listens to audio.

---

## 8. Authoring Conventions for New Tests

To keep the suite navigable as it grows from the current 117 tests to the ~250 tests this plan describes:

### 8.1 File template

```cpp
#ifdef ZENITH_INPUT_SIMULATOR
#include "Zenith.h"
#include "Core/Zenith_AutomatedTest.h"
#include "Source/PublicInterfaces.h"
// ... other DP headers

namespace
{
    struct Test_<Name>_State
    {
        int   m_iPhase = 0;
        // Captures (every assertion derives from a captured value, never a one-off check).
        // ...
        bool  m_bPassed = false;
    };
    static Test_<Name>_State s_xState;

    void Setup_<Name>()
    {
        s_xState = Test_<Name>_State{};
        // event subscriptions, RNG seeding, etc.
    }

    bool Step_<Name>(int iFrame)
    {
        switch (s_xState.m_iPhase)
        {
            case 0: /* ... */; s_xState.m_iPhase = 1; return true;
            case 1: /* ... */; return true;
            // ...
            case N: return false;
        }
        return true;
    }

    bool Verify_<Name>()
    {
        // Every assertion is a comparison of captures.
        // On failure, Zenith_Log every captured value for Claude to read.
        if (!s_xState.m_bPassed)
        {
            Zenith_Log(LOG_CATEGORY_TEST,
                "FAIL Test_<Name>: phase=%d ...", s_xState.m_iPhase);
        }
        return s_xState.m_bPassed;
    }
}

static const Zenith_AutomatedTest g_xTest_<Name> = {
    "Test_<Name>", &Setup_<Name>, &Step_<Name>, &Verify_<Name>, /*max frames*/ 240
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTest_<Name>);
#endif
```

### 8.2 Mandatory hygiene

1. **No `std::function`.** Use function pointers (Zenith convention).
2. **No `std::vector`.** Use `Zenith_Vector` (Zenith convention).
3. **No `std::mutex`.** Use `Zenith_Mutex` (Zenith convention).
4. **All asserts go through `Zenith_Assert`** for consistency with the rest of the engine. Test asserts that fail terminate the test; design test logic such that this is acceptable.
5. **Setup is idempotent.** The harness reloads scene 0 between tests, but Setup must still handle a clean slate.
6. **Step is pure.** No side effects beyond input simulation, programmatic state mutation (allowed for fixtures), and capture mutation.
7. **Verify is read-only.** No state mutation. Just compare captures, log, return.

### 8.3 Diagnostic logging

Every test logs a single failure line in the format:

```
FAIL <TestName>: phase=<N> capture_<key>=<value> capture_<key>=<value> ...
```

Claude reads these lines from the assertions log to localise failures.

### 8.4 Time budgeting

Every test specifies its frame budget with margin. As a guideline:

| Test category | Typical frames |
|---|---|
| Tier 0 sanity | 30–120 |
| Tier 1 single-mechanic | 120–300 |
| Tier 2 multi-system | 300–600 |
| Tier 3 inventory checks | 60–180 |
| Tier 4 single-Night playthrough | 9000 |
| Tier 4 full-campaign | 90000 |

---

## 9. Roll-out Plan

Tests are written **before** the implementation they validate. Phasing:

| Month | Tests added | Tests passing |
|---|---|---|
| 0 | Tier 0 (6 tests) — port from existing harness | 6 |
| 1 | Tier 1 Pause + NavMesh + Apprehend (9 tests) — initially all failing | 6 of 15 |
| 2 | Phase 1 engineering lands Pause, NavMesh, Apprehend | 15 of 15 |
| 3 | Tier 1 remaining (Switch/Drop, Cooldown, Scent, Sprint, Range, Save) — initially failing | 15 of 30 |
| 4 | Phase 1 engineering completes | 30 of 30 |
| 5–10 | Tier 2 (~80 tests across archetypes, hounds, variants, reagents, crafting, charms, distractions, fog, Liminal) | grows from 30 → 110 |
| 10–14 | Tier 3 inventory + procgen + loc + perf (~50 tests) | 110 → 160 |
| 14–18 | Tier 4 playthroughs + console + cert + accessibility + replay (~90 tests) | 160 → 250 |

**Total at ship:** ~250 automated tests, batch runtime ~45 minutes @ fixed 60 Hz.

The discipline: **every PR introduces at least one new test or modifies an existing test.** Tests that "drift to passing" without an underlying behaviour change are a code smell to investigate.

---

## 10. Out-of-Scope (Won't be tested by this suite)

To be explicit about the boundary:

- **Visual fidelity.** A reviewer's eye is needed for "this lighting looks right." The suite asserts only the inputs to the renderer.
- **Audio mix.** A reviewer's ear is needed for "the music feels eerie." The suite asserts only that emissions fire correctly.
- **Game feel / fun.** Designers play the game. No automated test claims to assess fun.
- **First-party platform certification.** TCRs and TRCs that require physical hardware (controller hot-plug, retail-disc loading, video output) ship via separate compliance harnesses on dev kits. This suite covers the in-process proxies.
- **Localisation language quality.** LQA is a vendor process. This suite catches missing keys and overruns, not bad translations.
- **Monetisation funnels.** Premium one-time purchase; no funnel to test.
- **Server-side anything.** No server in the game.

---

## 11. Summary

This plan describes ~250 automated tests, each runnable by `Zenith_AutomatedTestRunner` under `--fixed-dt 0.01666`, each producing a JSON pass/fail readable by Claude Code, and each derivable from a state-machine `Step` function that uses `Zenith_InputSimulator` to drive game input.

**No human ever has to watch the screen for any of these tests to pass or fail.** The combination of:

1. `Zenith_InputSimulator` for input
2. `DP_*` public interfaces for state queries
3. `Zenith_EventDispatcher` for event interception
4. Three Phase-1 instrumentation hooks for rendering, audio, and save round-trip surrogates

…closes the loop. The dev team works against this suite from week one of Phase 1 through the day the gold master burns.

If a test in this document cannot be implemented because the engine lacks a needed query API, that's a Shortfalls entry. If a behaviour in the GDD cannot be tested *by this suite*, that's a design issue: define the surrogate or drop the requirement.
