# Zenithmon Status

**Last updated:** 2026-07-16
**Stage:** **S5 (Battle integration slice) IN PROGRESS.** S0-S4 COMPLETE. S5 item 1 (Battle scene / `ZM_BattleArena`) DONE (ZM-D-089). **S5 item 2 = 4 sub-commits: SC2 (encounter logic, ZM-D-090), SC3 (`ZM_TallGrassSystem`, ZM-D-091), SC1 (engine E5 grass reset, ZM-D-092) DONE; only SC4 (windowed integration + scene wiring) REMAINS.** The Roadmap S5 item-2 box ticks when SC4 lands.
**Build:** GREEN -- Vulkan Debug True clean across the engine change (Zenithmon + Combat + RenderTest + DP + CityBuilder all built). Clean tree at the commit below.
**Tests:** boot unit gate **1933 ran / 1932 passed / 0 failed / 1 skipped** (SC1 added 3 ENGINE `Flux_Grass` units over 1930). `zenith test Zenithmon --headless` **9/0**. Engine E5 cross-game regression ALL GREEN: **Combat unit gate 1081/0** (engine-gate canary), **Zenithmon 1933/0** + windowed **`ZM_GrassRegeneration_Test` PASS** (the direct E5 grass canary), **DP 158/0**, **CityBuilder 45/0**. Dual baseline bump: engine default `1078->1081` (`Tools/run_unit_gate.ps1`) + `zm-tests.yml 1930->1933`.
**KNOWN (Q-2026-07-16-001):** `zenith test RenderTest --headless` is PRE-EXISTINGLY red in this checkout (missing baked terrain -> `Invalid buffer VRAM handle` during terrain streaming, BEFORE grass); proven orthogonal to E5 by a stash-revert diagnostic (identical crash without E5). E5's grass path validated via the windowed grass-regen test instead.

## Current task

**S5 item 2, LAST sub-commit -- SC4 (windowed integration + scene wiring):**
- **Author `ZM_TallGrassSystem` onto the Dawnmere terrain entity** (tools scene authoring in `Zenithmon.cpp`'s `Project_RegisterEditorAutomationSteps`, on the same entity as `ZM_TerrainGrass`); re-bake Dawnmere.zscen via a `_True` boot.
- **Windowed test 1 (encounter fires):** subscribe a test callback to `ZM_OnWildEncounter` (dispatcher `Zenith/ZenithECS/Zenith_EventSystem.h`); drive the player one tile into grass via input-sim state-setters; assert the event fires with the expected species. **BLOCKER to resolve:** Dawnmere is a TOWN (0 encounter slots), so `ZM_TallGrassSystem::ForceEncounterOnNextTransitionForTests()` (which synthesizes from the scene's first slot) is a NO-OP there. SC4 must add an EXPLICIT-species force overload `void ForceEncounterOnNextTransitionForTests(ZM_SPECIES_ID, u_int)` to `ZM_TallGrassSystem` (small additive change to Components/ZM_TallGrassSystem.{h,cpp}) so the test can force a known species on Dawnmere -- OR author the grass onto a grass-bearing ROUTE (only Dawnmere terrain is baked in S3, so the explicit-species overload is the simpler path).
- **Windowed test 2 (grass cleared on interior):** after Dawnmere grass generates, SINGLE-load PlayerHome (build 40) and assert `g_xEngine.Grass().GetGeneratedInstanceCount()==0` (now guaranteed by the E5 engine reset from SC1). `m_bRequiresGraphics` + RequestSkip guards on absent assets.
- After SC4 lands: **tick the Roadmap S5 item-2 box** (all 4 sub-commits done).

Full item-2 plan + frozen API sketches are this session's Plan output. Grep DecisionLog before re-deciding; the S5 gate (all 5 items) is the next VISUAL hard-stop.

## Last completed

**S5 item 2 SC1 -- engine E5 grass-singleton reset (ZM-D-092), committed + pushed (see git log).** `Flux_GrassImpl::Reset()` -> full `ClearSceneData()` clear + wired into `Zenith_Engine`'s `m_pfnResetRenderSystems` SINGLE-load hook (grass leak closed). 3 engine `Flux_Grass` units. Dual baseline bump 1078->1081 + 1930->1933. Reviewer + full cross-game regression green.

## Notes for next agent

- **Item 1 + item 2 SC2/SC3/SC1 are DONE.** Next-free ECS order is **110**. Only S5 item-2 SC4 remains before ticking the item-2 box, then S5 items 3-5.
- **S5 item 3 latent (reviewer-flagged, still open):** `ZM_BattleArena::BuildArena` spawns children into `GetActiveSceneData()` -- item 3's additive-battle-over-active-overworld path must retarget to the arena's own scene (parent entity's `GetSceneData`).
- **RenderTest canary degraded here (Q-2026-07-16-001):** a future engine change touching terrain/grass should `_True`-bake RenderTest's terrain (seed 1337) first, or RenderTest crashes on missing terrain. DP/CityBuilder handle missing assets gracefully (RequestSkip); RenderTest does not.
- **Gate ORDER + baselines (permanent):** `zenith test Zenithmon --headless` FIRST (heals DLLs). Boot unit gate hangs after units line -> `run_unit_gate.ps1 -Exe <exe> -Baseline N -TimeoutSec 300` (now **1933**). For ENGINE-unit changes bump BOTH `zm-tests.yml` AND the `Tools\run_unit_gate.ps1` default (now 1081); verify uniform via Combat (engine default) + Zenithmon. Engine changes need Combat + RenderTest + DP + CityBuilder regression (AgentBriefing 6.4). New files -> `Build\regen.ps1`. Run from repo root.
- **Working model:** MASTER-ONLY (ZM-D-031); LOCAL gate is authority; `zm-tests` post-push backstop. Orchestrated: only the orchestrator builds/tests/commits; subagents author, never build. Sweep stray `zenithmon.exe`/other exes before ending. NEVER commit baked assets or captures.
- **Open Questions (non-blocking):** Q-2026-07-16-001 (RenderTest terrain) + S2-era `Q-2026-07-12-*` + the S5 bleed-through `Q-2026-07-09-003`.
