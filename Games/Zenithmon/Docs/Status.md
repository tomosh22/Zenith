# Zenithmon Status

**Last updated:** 2026-07-16
**Stage:** **S5 (Battle integration slice) IN PROGRESS.** S0-S4 COMPLETE. S5 item 1 (Battle scene / `ZM_BattleArena`) DONE (ZM-D-089). **S5 item 2 = 4 sub-commits: SC2 (encounter selection, ZM-D-090) + SC3 (`ZM_TallGrassSystem`, ZM-D-091) DONE; SC1 (engine E5 grass reset) + SC4 (windowed integration) REMAIN.** The Roadmap S5 item-2 box stays UNCHECKED until SC1+SC4 land.
**Build:** GREEN -- Vulkan Debug True clean. Clean tree at the commit below.
**Tests:** boot unit gate **1930 ran / 1929 passed / 0 failed / 1 skipped** (SC3 added 6 `ZM_Grass` T0 units over the 1924 SC2 baseline); `zenith test Zenithmon --headless` **9 passed / 0 failed**. `.github/workflows/zm-tests.yml` baseline bumped **1924 -> 1930** (Zenithmon-only; engine default 1078 in `Tools/run_unit_gate.ps1` UNCHANGED). TestPlan 5.5 + AgentBriefing updated.

## Current task

**S5 item 2, next sub-commit -- SC1 (engine E5) is recommended next (SC4 depends on it):**

- **SC1 engine E5 (grass-singleton reset hygiene) -- ENGINE change, heavy gate:** `Flux_GrassImpl::Reset()` (`Zenith/Flux/Vegetation/Flux_Grass.cpp:~144`) currently UNDER-clears (only chunks+counters) and has NO source callers -> extend it to the FULL `ClearSceneData()` clear (instances + generated/uploaded flags + density map), then add `g_xEngine.Grass().Reset();` to the `m_pfnResetRenderSystems` lambda in `Zenith/Core/Zenith_Engine.cpp:~617` (next to Terrain/Text/Particles/Skybox/Fog). Tests: new `Zenith/Flux/Vegetation/Flux_Grass.Tests.inl` (`#include`d at the bottom of Flux_Grass.cpp under `#ifdef ZENITH_TESTING`) -- Reset clears instances/flags/density map + is idempotent + no-accumulation across a second generate. **AgentBriefing 6.4 gate:** engine units + RenderTest boots green (its idempotent per-frame grass re-apply repopulates after the reset -- confirm `GetVisibleBladeCount() > 0` after a scene reload) + DP + CityBuilder suites green + a DecisionLog entry. **BASELINE BOOKKEEPING (CRITICAL):** engine units run in EVERY game's boot, so N new engine units bump BOTH the engine default `1078` in `Tools/run_unit_gate.ps1:15` (used by engine-gate.yml [boots Combat] + scaffold-smoke.yml) AND zm-tests `1930 -> 1930+N`. Verify N is uniform: boot Combat (expect 1078+N) AND Zenithmon (expect 1930+N). dp-tests/cb-tests do NOT assert unit counts (automated suites only) -- just keep them green.
- **SC4 windowed integration (needs SC1):** author `ZM_TallGrassSystem` onto the Dawnmere terrain entity (tools authoring). Test 1: subscribe a test callback to `ZM_OnWildEncounter`, drive the player one tile into grass via input-sim state-setters, force + assert the event fires with the expected species. **Dawnmere is a TOWN (no encounter slots), so the current `ForceEncounterOnNextTransitionForTests()` (which synthesizes from the scene's first slot) is a NO-OP there -- SC4 must add an EXPLICIT-species force overload `ForceEncounterOnNextTransitionForTests(ZM_SPECIES_ID, u_int)` to `ZM_TallGrassSystem` (small additive change) OR run on a grass-bearing route.** Test 2: grass cleared on entering an interior -- after Dawnmere grass generates, SINGLE-load PlayerHome (build 40) and assert `g_xEngine.Grass().GetGeneratedInstanceCount()==0` (needs SC1's E5). `m_bRequiresGraphics` + RequestSkip guards.

Full item-2 plan (SC1-SC4 frozen API sketches) is this session's Plan output. Grep DecisionLog before re-deciding; the S5 gate (all 5 items) is the next VISUAL hard-stop.

## Last completed

**S5 item 2 SC3 -- `ZM_TallGrassSystem` (order 109) (ZM-D-091), committed + pushed (see git log).** `Components/ZM_TallGrassSystem.{h,cpp}` + Zenithmon.cpp registration. Owns a `ZM_GrassDensityMap` CPU copy (loaded from the terrain sibling), quantizes the active player to 1 m tiles, rolls `ZM_EncounterZone::RollStepForScene` on grass transitions, and dispatches `ZM_OnWildEncounter` (emit-only). Scene id resolved via the existing `ZM_FindSceneByBuildIndex`. 6 T0 units. Reviewer SHIP.

## Notes for next agent

- **Item 1 (`ZM_BattleArena`) + item 2 SC2 (`ZM_EncounterZone`) + SC3 (`ZM_TallGrassSystem`) are DONE.** Next-free ECS order is **110**.
- **S5 item 3 latent (reviewer-flagged, still open):** `ZM_BattleArena::BuildArena` spawns children into `GetActiveSceneData()` -- item 3's additive-battle-over-active-overworld path must retarget to the arena's own scene (parent entity's `GetSceneData`) or children orphan into the overworld.
- **SC4 force-seam gap (see Current task):** `ForceEncounterOnNextTransitionForTests()` is a no-op on a slot-less scene; SC4 needs an explicit-species force overload to test on Dawnmere.
- **Gate ORDER + baselines (permanent):** `zenith test Zenithmon --headless` FIRST (heals DLLs). Boot unit gate hangs after the units line -> `run_unit_gate.ps1 -Exe <exe> -Baseline N -TimeoutSec 300` (now **1930**). Bump N in `zm-tests.yml` AND (for ENGINE-unit changes like SC1 only) `Tools\run_unit_gate.ps1:15` default (1078) together. New files -> `Build\regen.ps1`. Windowed capture `--filter <Test>`. Run from repo root; `git -C C:/dev/Zenith ...`.
- **Working model:** MASTER-ONLY (ZM-D-031); LOCAL gate is authority; `zm-tests` post-push backstop. Orchestrated: only the orchestrator builds/tests/commits; subagents author, never build. Sweep stray `zenithmon.exe` before ending. NEVER commit baked assets (`Assets/`) or captures.
- **Open Questions (non-blocking):** S2-era `Q-2026-07-12-*` + the S5 bleed-through `Q-2026-07-09-003` (verified at the S5 gate).
