# Devil's Playground — MVP Roadmap

**Document purpose:** The single-source-of-truth task sequence for the MVP. Autonomous agents pick the next un-checked task and execute. Every task is small enough to land in one session (target: ≤ 4 hours of agent work), test-first, mergeable independently.

**Status discipline:** Tasks are atomic. Check ✅ when **the PR is merged AND CI is green AND Status.md is updated**. Never check a task on a partial implementation.

**Pre-task ritual:** Every task starts with
1. `git checkout master && git pull` on the main repo
2. `git checkout -b dp/mvp-<task-id>`
3. Read [AgentBriefing.md](AgentBriefing.md) for conventions
4. Write the test(s) for this task; confirm they fail
5. Implement until they pass + all existing tests stay green

**Post-task ritual:**
1. Update `Docs/Status.md` (replace "current task" line)
2. Append to `Docs/DecisionLog.md` (one entry per non-trivial decision)
3. Open PR; await CI; auto-merge on green
4. Tick the box in this file as part of the PR

---

## Phase 0 — Foundation Plumbing (2 weeks target)

These tasks land the **infrastructure** needed by every subsequent task. They are small, low-risk, and pay back across the rest of the MVP.

### 0.1 Tuning system

- [x] **MVP-0.1.1** — Add `Source/DP_Tuning.h/.cpp`. Load `Config/Tuning.json` at startup; expose `DP_Tuning::Get<T>(key)` accessor. Cache parsed values.
- [x] **MVP-0.1.2** — Add `Test_P1Tuning_LoadsAndValuesInBand` (Tier 1). Asserts each tuning value is within its sanity range (defined in test alongside the value).
- [ ] **MVP-0.1.3** — Migrate `DPVillager_Behaviour` to read `m_fMaxLife`, `m_fMoveSpeed` from `DP_Tuning`. Add migration test that proves prototype's behaviour unchanged.
- [ ] **MVP-0.1.4** — Migrate `Priest_Behaviour` configuration block to `DP_Tuning`.
- [ ] **MVP-0.1.5** — Migrate `DPInteractable_Behaviour` and all its subclasses' timing constants to `DP_Tuning`.

### 0.2 Archetype system

- [ ] **MVP-0.2.1** — Add `Source/DP_Archetypes.h/.cpp`. Load `Config/Archetypes.json`. Expose `DP_Archetypes::Get(id)` returning const ref to archetype data.
- [ ] **MVP-0.2.2** — Add `m_eArchetype` enum field to `DPVillager_Behaviour`; on `OnAwake`, look up timer + abilities from the registry. Authoring step (`AddStep_AttachScript`) takes archetype id as parameter.
- [ ] **MVP-0.2.3** — Add `Test_P2Archetype_TimersMatchSpec`. Parameterised over the 4 MVP archetypes.
- [ ] **MVP-0.2.4** — Add `Test_P3Inventory_ArchetypeCount`. Asserts manifest declares ≥ 4 MVP archetypes.

### 0.3 Workflow scaffolding

- [ ] **MVP-0.3.1** — Add `Docs/Status.md` (one running paragraph + current task line). Initialised by this commit.
- [ ] **MVP-0.3.2** — Add `Docs/Questions.md` (running list of blocked items). Initialised empty.
- [ ] **MVP-0.3.3** — Add `Docs/DecisionLog.md` (append-only).
- [ ] **MVP-0.3.4** — Add `Tools/agent_session_close.ps1` — script that updates `Status.md`, appends to `DecisionLog.md`, prints the next task name from this roadmap. Agents call it at session end.

### 0.4 Test instrumentation hooks

Per [TestPlan §0.4](TestPlan.md). These three hooks unlock the test plan for shipping; without them, audio/render/save tests cannot be claudia-verified.

- [ ] **MVP-0.4.1** — Add `Zenith_AudioBus::GetEmittedSoundsForTest()` and `ClearEmittedSoundsForTest()`. Test-build-only. Hook called from every code path that *would* emit audio (currently none — the hook just primes the API).
- [ ] **MVP-0.4.2** — Add `Zenith_RenderBus::GetSubmittedDrawCallsForTest()`. Same pattern.
- [ ] **MVP-0.4.3** — Add `Zenith_SaveSystem` skeleton with `GetWrittenBlobsForTest()` and `SetReadbackBlob()`. Doesn't touch disk yet.

---

## Phase 1 — Foundation Gameplay (6 weeks target)

The blocker list from [Shortfalls §1](Shortfalls.md). After Phase 1, the game has real loss states.

### 1.1 Real pause

- [ ] **MVP-1.1.1** — Test_P1Pause_TimerStopsOnEscape (write first; expect to fail).
- [ ] **MVP-1.1.2** — Wire `Zenith_SceneManager::SetScenePaused` (or add it). Pause overlay calls it.
- [ ] **MVP-1.1.3** — Test_P1Pause_PriestStopsOnEscape.
- [ ] **MVP-1.1.4** — Test_P1Pause_InputSimDuringPause.

### 1.2 Real navmesh

- [ ] **MVP-1.2.1** — Test_P1NavMesh_PathRespectsWalls (failing).
- [ ] **MVP-1.2.2** — Implement collider-derived navmesh generator in `AI/Navigation/Zenith_NavMeshGenerator`. Voxelise → polygon soup → triangulate.
- [ ] **MVP-1.2.3** — Replace `DP_AI::GetOrBuildLevelNavMesh` to call the generator.
- [ ] **MVP-1.2.4** — Test_P1NavMesh_ClosedDoorBlocksPath.
- [ ] **MVP-1.2.5** — Test_P1NavMesh_RegenerationOnSceneSwap.

### 1.3 Apprehend / run-loss

- [ ] **MVP-1.3.1** — Test_P1Apprehend_PriestCatchesPlayer (failing).
- [ ] **MVP-1.3.2** — Add `DP_OnRunLost` event + cause enum (Apprehended, Dawn, NoVessels).
- [ ] **MVP-1.3.3** — Add `Apprehend` BT branch to `Priest_Behaviour`: distance < `apprehend_range_m` → 3 s channel → dispatch event.
- [ ] **MVP-1.3.4** — Test_P1Apprehend_SwitchBreaksChannel.
- [ ] **MVP-1.3.5** — Test_P1Apprehend_OutOfRangeIgnored.

### 1.4 Voluntary switch + drop verb

- [ ] **MVP-1.4.1** — Test_P1Switch_FaintNotDie (failing).
- [ ] **MVP-1.4.2** — Add villager `m_eState` field + transitions (Idle, Possessed, Fainted, Dead). Voluntary switch sets Fainted with 10 s recovery.
- [ ] **MVP-1.4.3** — Test_P1Switch_BurnOutDoesDie (negative control).
- [ ] **MVP-1.4.4** — Test_P1Drop_GoesToGroundAtBodyPosition (failing).
- [ ] **MVP-1.4.5** — Add G-key drop verb to `DPPlayerController_Behaviour`. Held item's transform parented from villager-bone-socket to scene root at villager's foot position.
- [ ] **MVP-1.4.6** — Test_P1Drop_PickupChainHandoff.

### 1.5 Possession cooldown

- [ ] **MVP-1.5.1** — Test_P1Cooldown_CannotPossessFor1pt5s (failing).
- [ ] **MVP-1.5.2** — Add `s_fCooldownRemaining` to `DP_Player`. `SetPossessedVillager` checks it.
- [ ] **MVP-1.5.3** — Test_P1Cooldown_NotAffectedByDeath.

### 1.6 Demon-scent

- [ ] **MVP-1.6.1** — Test_P1Scent_AccumulatesOnPossession (failing).
- [ ] **MVP-1.6.2** — Add per-entity scent table to `DP_Player`. Update on possession-switch event; tick decay each frame.
- [ ] **MVP-1.6.3** — Test_P1Scent_NotificationToBlackboard (deferred until hounds — post-MVP).

### 1.7 Sprint + walk-quiet

- [ ] **MVP-1.7.1** — Test_P1Sprint_DrainsLifeFaster.
- [ ] **MVP-1.7.2** — Add Shift + Ctrl handling in `DP_Input::ReadMoveVillager`. Apply speed multiplier and life-cost.
- [ ] **MVP-1.7.3** — Test_P1Sprint_NoDrainWhenNotMoving.

### 1.8 Possession range

- [ ] **MVP-1.8.1** — Test_P1Range_RefusedOutOfRange.
- [ ] **MVP-1.8.2** — Add anchor-position tracking to `DP_Player`. `SetPossessedVillager` checks distance.
- [ ] **MVP-1.8.3** — Test_P1Range_AcceptedInRange.

### 1.9 Priest omniscience removed

- [ ] **MVP-1.9.1** — Test_P1Priest_DoesNotChasePossessedOutOfSight.
- [ ] **MVP-1.9.2** — Remove the `DP_Player::GetPossessedVillager` fallback in `Priest_Behaviour::BridgePerceptionToBlackboard`. Update harness pursuit-test fixture to seed perception properly.
- [ ] **MVP-1.9.3** — Test_P1Priest_PursuesAfterLineOfSight (regression guard).

### 1.10 Save/load run state

- [ ] **MVP-1.10.1** — Test_P1Save_RoundTripMeta (failing).
- [ ] **MVP-1.10.2** — Add `DP_RunState` struct (possessed villager, life timer, held items per villager, scent table, win mask, dawn timer). Serialise via `Zenith_SaveSystem` skeleton from MVP-0.4.3.
- [ ] **MVP-1.10.3** — Test_P1Save_RobustToCorruption.

---

## Phase 2 — Depth (4 weeks target)

The four MVP archetypes, the five MVP reagents, fog-of-war shader, HUD upgrades.

### 2.1 Archetype gameplay

- [ ] **MVP-2.1.1** — Test_P2Archetype_DevoutChannel (failing). Implement possession-channel state in `DP_Player` and `DPVillager_Behaviour`.
- [ ] **MVP-2.1.2** — Test_P2Archetype_DevoutChannelInterrupt. Implement interrupt-on-priest-LOS.
- [ ] **MVP-2.1.3** — Test_P2Archetype_ChildHalfTimer.
- [ ] **MVP-2.1.4** — Test_P2Archetype_ChildCannotCarryTools. Add restriction check in `DPItemBase_Behaviour::OnUpdate` pickup path.
- [ ] **MVP-2.1.5** — Test_P2Archetype_SextonChapelStealth. (Skip for MVP — Sexton ability is post-MVP scope; the archetype just visually distinguishes for now.)

### 2.2 Reagent uniqueness

- [ ] **MVP-2.2.1** — Add reagent registry from `Config/Reagents.json`.
- [ ] **MVP-2.2.2** — Refactor `DPItemBase_Behaviour` pickup to use reagent's `pickup_channel_s` for reagents (1.0 s) vs. 0 s for tools.
- [ ] **MVP-2.2.3** — Test_P2Reagent_UniquePickupChannel.
- [ ] **MVP-2.2.4** — Implement `BogWater::EvaporateAfterDrop` (8 s timer + entity destroy).
- [ ] **MVP-2.2.5** — Test_P2Reagent_BogWaterEvaporates.
- [ ] **MVP-2.2.6** — Implement `BellSoul::RingsBellOnPickup` (dispatch `DP_OnBellRing` + emit perception stimulus via the entire map).
- [ ] **MVP-2.2.7** — Test_P2Reagent_BellSoulRingsBell.

### 2.3 Forge crafting expansion

- [ ] **MVP-2.3.1** — Add forge recipe table (in `DPForge_Behaviour` initially; later move to `Config/Recipes.json` post-MVP).
- [ ] **MVP-2.3.2** — Test_P2Forge_IronWoodSpike.
- [ ] **MVP-2.3.3** — Test_P2Forge_IronBrassKeySkeleton.
- [ ] **MVP-2.3.4** — Test_P2Forge_AudibleAt30m (uses Audio bus instrumentation hook).

### 2.4 Real fog shader

- [ ] **MVP-2.4.1** — Author `Shaders/DP_Fog.slang` — accept CBV with hole array, output single black-fog plane with hole carve-outs + memory-fade. Cheap as possible.
- [ ] **MVP-2.4.2** — Wire `DPFogPass` to upload the CBV and dispatch.
- [ ] **MVP-2.4.3** — Test_P2Fog_AelfricNotRevealed.
- [ ] **MVP-2.4.4** — Test_P2Fog_LightAddsHole.
- [ ] **MVP-2.4.5** — Memory fog implementation: per-tile last-seen-frame buffer. Test_P2Fog_MemoryDimsAfter10s.

### 2.5 HUD upgrades

- [ ] **MVP-2.5.1** — Add whisper-line to HUD (single-line UI text bottom-centre). Subscribes to Aelfric-state-change events.
- [ ] **MVP-2.5.2** — Add demon-scent indicator (numeric for now: "Scent: 0.4"). Polishes later.
- [ ] **MVP-2.5.3** — Add Aelfric awareness icon (placeholder: state-name as text).
- [ ] **MVP-2.5.4** — Add dawn sun-gauge (placeholder: text countdown).
- [ ] **MVP-2.5.5** — Add pause menu (Resume / Restart / Quit).
- [ ] **MVP-2.5.6** — Add main-menu Quit button.

---

## Phase 3 — Asset Substitution (3 weeks target)

Replace the most jarring placeholders with Mixamo + Kenney free assets. Existing tinted-cube system continues for the long tail.

### 3.1 Mixamo villager integration

- [ ] **MVP-3.1.1** — Download Mixamo Y-Bot rigged character + idle/walk/run anims. Import via `ZenithTools` to `.zmodel` / `.zskel` / `.zanim`.
- [ ] **MVP-3.1.2** — Author the canonical bone-set test (Test_AssetSkel_VillagerCanonicalBones). Iterate the imported skeleton until tests pass.
- [ ] **MVP-3.1.3** — Wire `DPVillager_Behaviour` to use the new mesh + animator. Tag with the archetype's `tint_rgb` via `DPMaterials::GetOrCreateColouredVariant`.
- [ ] **MVP-3.1.4** — Test_AssetScene_AllArchetypesInstantiable.

### 3.2 Aelfric placeholder

- [ ] **MVP-3.2.1** — Acquire a Mixamo character with a hat-like silhouette (or model a 5-min stove-pipe-hat geo and attach to a Mixamo body). Dark robe tint.
- [ ] **MVP-3.2.2** — Replace Priest's capsule with the new mesh. Add lantern point-light attached to RightHand socket.
- [ ] **MVP-3.2.3** — Test_AssetSkel_AelfricFaceBlendshapes (deferred until VO; for MVP just verify the mesh loads and instantiates).

### 3.3 Item meshes from Kenney

- [ ] **MVP-3.3.1** — Acquire Kenney Survival Kit + Medieval Kit. Pick meshes for: Iron (ore lump), Wood (log), Brass Key (key), Skeleton Key (ornate key), and the 5 reagents.
- [ ] **MVP-3.3.2** — Import all 10. Author the asset-manifest entries.
- [ ] **MVP-3.3.3** — Replace tinted-cube items in `DPItemBase_Behaviour` with the real mesh per item-tag. **KEEP the cube system as a debug-mode fallback** per AssetManifest §15.
- [ ] **MVP-3.3.4** — Test_AssetMesh_ItemHeldSocket.

### 3.4 Interactable meshes

- [ ] **MVP-3.4.1** — Acquire / model door, double-door, chest, forge, pentagram meshes (Kenney Medieval has all of these).
- [ ] **MVP-3.4.2** — Author Pivot / Leaf_L / Leaf_R / Lid sockets per AssetTestPlan §2.1.
- [ ] **MVP-3.4.3** — Test_AssetMesh_DoorHasPivot, Test_AssetMesh_DoubleDoorHasTwoPivots, Test_AssetMesh_ChestHasLidBone, Test_AssetMesh_PentagramFlat.

### 3.5 Asset linter

- [ ] **MVP-3.5.1** — Implement `ZenithTools.exe lint` sub-command per AssetTestPlan §3. Wraps manifest, loadability, schema, format tests.
- [ ] **MVP-3.5.2** — Add CI step: pre-build lint against the full manifest.
- [ ] **MVP-3.5.3** — Add `Test_AssetScene_VexholmeLoadsCleanly` (integration test; counts asset-load errors during scene load).

---

## Phase 4 — MVP Acceptance (2 weeks target)

The three playthrough tests that define MVP done.

### 4.1 The golden playthrough

- [ ] **MVP-4.1.1** — Author `Test_P4Playthrough_Night1WinGolden`. Multi-phase script per TestPlan §5.1.
- [ ] **MVP-4.1.2** — Iterate on Vexholme's GameLevel scene authoring (via `Project_RegisterEditorAutomationSteps`) until the bot wins consistently in < 150 s.
- [ ] **MVP-4.1.3** — Re-bake `.zscen`.

### 4.2 Loss states

- [ ] **MVP-4.2.1** — Author `Test_P4Playthrough_Night1LossByApprehend`. Possessed villager stands still in priest's pursuit path.
- [ ] **MVP-4.2.2** — Author `Test_P4Playthrough_Night1LossByDawn`. Possess villager, idle, wait for dawn-timer expiry.
- [ ] **MVP-4.2.3** — Author `Test_P4Playthrough_Night1LossByNoVillagers`. Burn out every villager in sequence.

### 4.3 Polish & demo build

- [ ] **MVP-4.3.1** — Tune the MVP loop so the bot wins ~50% of randomised input runs (proxy for fair difficulty).
- [ ] **MVP-4.3.2** — Add post-victory and post-loss "press any key to restart" overlay.
- [ ] **MVP-4.3.3** — Set up the demo packaging script: `Tools/package_mvp_demo.ps1` that copies the build output + assets to a single folder for distribution.
- [ ] **MVP-4.3.4** — Run the MVP demo end-to-end personally. Update Status.md to "MVP COMPLETE."

---

## Roadmap meta-rules

- **Sequencing.** Tasks within a sub-section (e.g. 1.3.1–1.3.5) are strictly sequential. Tasks across sub-sections within the same phase (e.g. 1.3.x vs. 1.4.x) can run in parallel agents if Sharpmake regeneration / build-output locks aren't a problem ([per memory](../../../../Users/tomos/.claude/projects/C--dev-Zenith/memory/feedback_parallel_agents_msbuild_thrash.md): they often are. Default to sequential unless the agent confirms isolation).
- **Branch hygiene.** One PR per task. Branch name `dp/mvp-<task-id>`.
- **Test-first discipline.** Every task starts with a failing test, ends with a passing test. PRs that change behaviour without a corresponding test change are rejected by the reviewer rubric.
- **Tuning changes.** Any change to `Config/*.json` is its own PR, separate from code changes. Easier to review and revert.
- **Scope discipline.** If a task tempts you toward post-MVP scope, **stop and surface in Questions.md**. The MVP exists to ship, not to be perfect.

---

## Post-MVP backlog (for visibility, not commitment)

When MVP ships, the next milestone candidates are:

1. **Autonomous playtest bot** (InputsForAutonomy §2.1). Highest-value engineering investment.
2. **Audio system + first SFX integration.** Closes the engine's largest gap.
3. **Remaining 20 archetypes + 9 reagents.**
4. **Hounds + Aelfric variants.**
5. **Liminal hub + meta-progression + cross-run save.**
6. **Procedural shufflers + multi-Night campaign.**
7. **Final art commission (first money spent).**
8. **Console platform layers.**
9. **Cutscenes + narrative arc.**
10. **Localisation pipeline.**

Each becomes its own roadmap document at the time.
