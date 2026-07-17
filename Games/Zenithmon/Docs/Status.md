# Zenithmon Status

**Last updated:** 2026-07-17
**Stage:** **S5 (Battle integration slice) IN PROGRESS.** S0-S4 COMPLETE. **S5 items 1+2 DONE. S5 item 3 (additive load / pause / camera switch round trip) IN PROGRESS -- SC1 of 5 DONE (ZM-D-094).**
**Build:** GREEN -- Vulkan Debug True clean. Clean tree at the commit below.
**Tests:** boot unit gate **1934 ran / 1933 passed / 0 failed / 1 skipped** (SC1 added 1 unit -> baseline 1933 -> **1934**, bumped in `zm-tests.yml` this commit). `zenith test Zenithmon --headless` **12/0**. Windowed `--filter ZM_BattleArena` **2/0** (`ZM_BattleArenaOwnScene_Test` + the pre-existing `ZM_BattleArena_Test`). Engine default 1081 (`Tools/run_unit_gate.ps1`) UNCHANGED -- SC1 touches no engine code.

## Current task

**S5 item 3 -- SC2 of 5: `ZM_TerrainGrass::RegenerateForSceneResume()` -- the overworld grass-restore seam.** The approved 5-sub-commit plan for item 3 is on disk at `Build/artifacts/zm_s5_item3_plan/` (see "Notes" -- it is git-ignored; regenerate it if absent). Item 3 builds the encounter -> battle -> overworld TRANSITION MACHINERY only; the battle itself (`ZM_BattleDirector` / `ZM_UI_BattleHUD` / event-stream interpreter) is item 4 and is OUT of item 3's scope.

Remaining sub-commits:
- **SC2** -- `ZM_TerrainGrass::RegenerateForSceneResume()`: the additive battle path gets NO grass reset for free (the E5 hook is SINGLE-load-only, ZM-D-092), and the overworld's `ZM_TerrainGrass` cannot self-heal on resume (its `OnUpdate` is gated by the pause and its apply path latches behind `m_bGrassApplied`). Explicit seam, driven from the persistent scene.
- **SC3** -- `ZM_BattleTransition` (ECS order **110**, next free) as pure policy + encounter latch + persistent root + shared fade helper. Subscribes to `ZM_OnWildEncounter` with a **static function pointer + static latch** (never a `this`-capturing lambda -- the component relocates on `DontDestroyOnLoad`). Gets its OWN entity + OWN `BattleFade` overlay, NOT co-located on `ZM_GameStateRoot` (`ZM_GameStateManager::OnUpdate` drives the shared `WarpFade` unconditionally every frame at `.cpp:129` and would stomp a second owner).
- **SC4** -- the round-trip state machine: fire-and-forget additive load -> poll -> one-shot entry (park player, pause overworld, `SetActiveScene(Battle)` = the camera switch, clear grass) -> `RequestBattleEnd()` seam for item 4 -> restore.
- **SC5** -- the windowed round-trip gate + docs; ticks the item-3 Roadmap box.

## Last completed

**S5 item 3 SC1 (ZM-D-094), committed + pushed (see git log).** `ZM_BattleArena::BuildArena` now resolves `m_xParentEntity.GetSceneData()` instead of `GetActiveSceneData()`, killing the reviewer-flagged orphan hazard **before** any additive-battle code exists. Also deleted a dead `GetActiveSceneData()` fetch in `OnStart` and added `uCHILD_COUNT` (9) + `GetChildEntityID(u_int)`. Locked by `ZM_BattleArenaOwnScene_Test`, **proven to fail with the fix reverted and pass with it applied**.

## Notes for next agent (S5 item 3, SC2 onward)

- **The item-3 gotcha recorded in the previous Status.md is DISCHARGED.** `BuildArena` no longer reads the active scene, so SC4 has no ordering constraint to honour. Do NOT re-litigate option (a) -- ZM-D-094 proves it is unreachable from game code (the deferred load drains AFTER every `OnUpdate`; the next frame's `DispatchPendingStarts` precedes every `OnUpdate`).
- **Next-free ECS order is 110** (`ZM_BattleTransition` claims it in SC3).
- **`ScopedTestIsolation` semantics (settles a contradiction -- verify before relying):** the guard **steals the live subscription tables, leaving the dispatcher EMPTY for the scope's lifetime** (`Zenith_EventSystem.h:142-146` + ctor body `:189-198`). So a test under isolation SUPPRESSES the game's real `ZM_OnWildEncounter` subscriber rather than keeping it -- which means ZM-D-093's claim that isolation "future-proofs the encounter test against item 3's subscriber" is CORRECT, and the shipped `ZM_TallGrassEncounter_Test` stays safe once SC3/SC4 add a live subscriber. Corollary: SC5's round-trip test must **NOT** construct a `ScopedTestIsolation`, because it needs the game's own subscriber alive to observe.
- **Gate ORDER + baselines (permanent):** `zenith test Zenithmon --headless` FIRST (heals DLLs). Boot unit gate hangs after the units line -> `run_unit_gate.ps1 -Exe <exe> -Baseline N -TimeoutSec 300` (now **1934**; the 300s timeout-kill is EXPECTED, the units line is already logged). Windowed: `--filter <Test>`. New files -> `Build\regen.ps1`. ENGINE-unit changes bump BOTH `zm-tests.yml` AND `Tools\run_unit_gate.ps1`'s default (1081) + need Combat/RenderTest/DP/CityBuilder regression (RenderTest pre-existingly red -- Q-2026-07-16-001).
- **Working model:** MASTER-ONLY (ZM-D-031); LOCAL gate is authority; `zm-tests` post-push backstop. Orchestrated: only the orchestrator builds/tests/commits; subagents author, never build. Sweep stray `zenithmon.exe` before ending. NEVER commit baked assets or captures.
- **Open Questions:** Q-2026-07-16-001 (RenderTest terrain) + S2-era `Q-2026-07-12-*` + the S5 bleed-through `Q-2026-07-09-003` (NOT resolvable by item 3 -- it is verified/falsified at the S5 SCREENSHOT gate). Item 3's plan raised 3 new ones for the orchestrator to log: the invented `BiomeForScene` table (no biome column exists on `ZM_WorldSpec`), the "HUD switch" wording (ZERO `ZM_UI*` code exists today -- `ZM_UI_BattleHUD` is item 4's box), and party-restore (item 5's scope, not item 3's).
- **The S5 gate is the next VISUAL hard-stop** (all 5 items: windowed encounter round-trip + catch + exp + exact resume + the bleed-through screenshot). Do not tick it or start S6 without the user's sign-off.
