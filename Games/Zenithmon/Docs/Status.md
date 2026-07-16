# Zenithmon Status

**Last updated:** 2026-07-16
**Stage:** **S5 (Battle integration slice) IN PROGRESS.** S0-S4 COMPLETE. **S5 item 1 (Battle scene / `ZM_BattleArena`) DONE (ZM-D-089). S5 item 2 (encounters + tall grass + engine E5) COMPLETE (4 sub-commits ZM-D-090/091/092/093).** **Next: S5 item 3 -- additive load / `SetScenePaused` / camera+HUD switch round trip (grass cleared entering interiors/battle).**
**Build:** GREEN -- Vulkan Debug True clean. Clean tree at the commit below.
**Tests:** boot unit gate **1933 ran / 1932 passed / 0 failed / 1 skipped** (SC4 added only windowed tests -> baseline UNCHANGED at 1933). `zenith test Zenithmon --headless` **11/0** (2 new windowed grass tests skip headless). **SC4 windowed tests PASS (2/0):** `ZM_TallGrassEncounter_Test` (206 frames -- walk grass -> `ZM_OnWildEncounter` fires) + `ZM_TallGrassInteriorClear_Test` (grass cleared on interior via E5). `zm-tests.yml` baseline 1933; engine default 1081 (`Tools/run_unit_gate.ps1`).

## Current task

**S5 item 3 -- additive load / SetScenePaused / camera+HUD switch round trip; grass cleared entering interiors/battle.** Per Roadmap S5 + MasterPlan S5: when a `ZM_OnWildEncounter` fires (item 2 emits it), load the Battle scene (build index 1, world Y=-2000) ADDITIVELY over the paused overworld, switch the camera + HUD to the battle, and on battle end restore EXACT overworld state. Grass is cleared when entering interiors/battle (the E5 engine reset from item 2 SC1 already handles SINGLE loads; the additive battle path needs its own handling since additive loads do NOT hit the render-reset hook). This is where item 3 subscribes to `ZM_OnWildEncounter` (the item-2 emit-only seam) to trigger the additive battle.

**CRITICAL for item 3 (reviewer-flagged, ZM-D-089/091):** `ZM_BattleArena::BuildArena` spawns its dome/platform/dressing children into `g_xEngine.Scenes().GetActiveSceneData()`. That is correct only when Battle is the ACTIVE scene. For the additive-battle-over-active-overworld path, either (a) `SetActiveScene(Battle)` before the arena's OnStart dispatches, or (b) retarget `BuildArena` to the arena's OWN scene (the parent entity's `GetSceneData` rather than the active scene) so children don't orphan into the overworld. Decide + implement in item 3.

Grep DecisionLog before re-deciding; Scope.md is binding; the S5 gate (all 5 items -- windowed encounter round-trip + catch + exp + exact resume + bleed-through screenshot Q-2026-07-09-003) is the next VISUAL hard-stop.

## Last completed

**S5 item 2 COMPLETE (SC4, ZM-D-093), committed + pushed (see git log).** SC4: additive explicit-species force seam on `ZM_TallGrassSystem` + 2 windowed tests proving the tall-grass -> encounter -> `ZM_OnWildEncounter` path end-to-end (runtime-add + manual `OnAwake()`; `ScopedTestIsolation`; data-driven walk) and the E5 interior grass-clear. Item 2 full: SC2 (`ZM_EncounterZone` + event + rate) + SC3 (`ZM_TallGrassSystem` order 109) + SC1 (engine E5 grass reset, cross-game green) + SC4.

## Notes for next agent (S5 item 3)

- **Items 1 + 2 are DONE.** Next-free ECS order is **110**. `ZM_OnWildEncounter` (`Source/World/ZM_EncounterEvents.h`) is emitted by `ZM_TallGrassSystem` but has NO subscriber yet -- item 3 adds it (subscribe via `Zenith_EventDispatcher::Get().Subscribe<ZM_OnWildEncounter>(...)`, `Zenith/ZenithECS/Zenith_EventSystem.h`; use `ScopedTestIsolation` in tests).
- **Additive-load surface:** the Battle scene is build index 1 at world Y=-2000 (`ZM_BattleArena::fARENA_WORLD_Y`). `LoadSceneByIndex(1, SCENE_LOAD_ADDITIVE)` loads it without unloading the overworld; `SetScenePaused`/`SetActiveScene` switch focus. The E5 render-reset hook is SINGLE-only, so the additive battle does NOT auto-clear the overworld grass (by design -- the -2000 offset + the dome keep it out of view; the S5 gate has the bleed-through screenshot check Q-2026-07-09-003). See the `ZM_BattleArena::BuildArena` GetActiveSceneData note above.
- **Gate ORDER + baselines (permanent):** `zenith test Zenithmon --headless` FIRST (heals DLLs). Boot unit gate hangs after units line -> `run_unit_gate.ps1 -Exe <exe> -Baseline N -TimeoutSec 300` (now **1933**). Windowed tests: `--filter <Test>` (warm assets bake fast). New files -> `Build\regen.ps1`. ENGINE-unit changes bump BOTH `zm-tests.yml` AND `Tools\run_unit_gate.ps1` default (now 1081) + need Combat/RenderTest/DP/CityBuilder regression (RenderTest pre-existingly red here -- Q-2026-07-16-001).
- **Working model:** MASTER-ONLY (ZM-D-031); LOCAL gate is authority; `zm-tests` post-push backstop. Orchestrated: only the orchestrator builds/tests/commits; subagents author, never build. Sweep stray `zenithmon.exe` before ending. NEVER commit baked assets or captures.
- **Open Questions:** Q-2026-07-16-001 (RenderTest terrain) + S2-era `Q-2026-07-12-*` + the S5 bleed-through `Q-2026-07-09-003`.
