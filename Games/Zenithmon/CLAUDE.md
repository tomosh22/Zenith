# Zenithmon

Pokemon Sword/Shield-class monster-collecting RPG built entirely on Zenith
systems: ~150 original species, 8 gyms, turn-based battles, all assets
procedurally generated and baked by tools builds.

> **Read `Docs/Status.md` first each session**, then `Docs/Roadmap.md` for the
> S0-S12 stage plan. `Docs/AgentBriefing.md` is the session onboarding guide;
> `Docs/MasterPlan.md` is the full approved program plan behind the Roadmap;
> `Docs/Scope.md` is the binding in/out list. Game code uses the `ZM_` prefix.
> Unattended development runs on `Docs/StartPrompts.md` prompt 0 (the
> lifecycle loop).

Current stage, verified test baseline, and in-flight work live in
`Docs/Status.md`; do not duplicate that fast-moving state here. Operating
policy is ZM-D-031: work directly on `master`, never create a branch, PR, or
worktree, and treat the full local gate as the authority before commit/push.
`zm-tests` runs after the push as a backstop.

## File structure

```
Games/Zenithmon/
  Zenithmon.zproj                # Build descriptor (name, android flag, extras)
  Zenithmon.cpp                  # Project_* entry points, component registration,
                                 #   SaveData init, between-tests hook
  Components/                    # ECS-facing game components
  Source/Battle/                 # Headless deterministic battle engine
  Source/Data/                   # Compiled const gameplay tables + pure formulas
  Tests/ZM_Tests_*.cpp           # Boot, data, stats, battle, and integrity units
  Tests/ZM_AutoTests_*.cpp       # Harness-managed automated/windowed tests
  Assets/Scenes/                 # BOOT-AUTHORED (see below) and COMMITTED --
                                 #   all four scenes are tracked (ZM-D-148)
  Assets/Navmesh/                # Dawnmere.znavmesh -- COMMITTED, CI-loadable
  Docs/                          # Cross-session knowledge base (Status/Roadmap/GDD/...)
  CLAUDE.md                      # This file
```

## Build & run

New games are managed by the `zenith` CLI (no Sharpmake edits):

```
zenith build Zenithmon          # Vulkan_vs2022_Debug_Win64_True
zenith run   Zenithmon          # launch the newest built exe
zenith open  Zenithmon          # regen + open the solution in Visual Studio
```

Or directly:

```
msbuild Games\Zenithmon\zenithmon_win64.sln /t:Zenithmon /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Zenithmon\Build\output\win64\vulkan_vs2022_debug_win64_true\zenithmon.exe
```

## First-run scene caveat (IMPORTANT)

`Assets/Scenes/FrontEnd.zscen` is **not checked in** -- it is authored on boot by
`Project_RegisterEditorAutomationSteps` (a **tools-only** function) and saved via
`AddStep_SaveScene`. Because that function is compiled out of non-tools builds:

* Your **first build + run must be a `*_True` config** (e.g.
  `Vulkan_vs2022_Debug_Win64_True`). That boot authors and saves `FrontEnd.zscen`.
* Thereafter, `*_False` and Android builds **load** the baked `FrontEnd.zscen`.

If a `_False` build shows an empty scene, run a `_True` build once to bake it.

**Five assets ARE committed:** `Assets/Navmesh/Dawnmere.znavmesh` (ZM-D-147) and
all four `Assets/Scenes/*.zscen` (ZM-D-148). They are what let CI verify
navigation and scene content with no GPU and no bake. The navmesh and
`Dawnmere.zscen` are re-authored only by a **windowed** tools boot, and Dawnmere
additionally needs every terrain recipe already warm -- so on a fresh clone it
takes **two** `_True` boots to regenerate them (boot 1 bakes terrain, boot 2
authors Dawnmere). You do not normally need to: the committed bytes are what the
game loads.

Scene bytes are boot-shape-independent (dense authoring-order file indices), so a
boot must NOT leave a scene modified in `git status`. If one ever does, that is a
regression of that property -- investigate it rather than just re-committing.

## Testing

```
pwsh -File Tools\zenith.ps1 test Zenithmon --headless    # full batch
pwsh -File Tools\zenith.ps1 test Zenithmon --filter ZM_Boot_Test
```

Test DISCOVERY and every headless run use the **Null** exe (`zenith build
Zenithmon --headless` first -- headless is a build config, not a flag).
Unit tests (`Tests/ZM_Tests_*.cpp`, `ZENITH_TEST`) run at every boot before the
scene loads; automated tests (`Tests/ZM_AutoTests_*.cpp`) run via the harness.
Conventions (state-setters only, between-tests hook, RequestSkip when baked
assets are absent) are documented in `Docs/TestPlan.md`. CI gate:
`.github/workflows/zm-tests.yml` (required check `zm-tests`).

## Where to go next

* `Docs/Roadmap.md` -- what's next, stage by stage (S0-S12).
* `Components/ZM_GameComponent.h` -- lifecycle hooks (`OnStart`, `OnUpdate`,
  `WriteToDataStream`/`ReadFromDataStream`) are concept-detected by the
  component-meta registry; there is no base class.
* `Zenithmon.cpp` -- the `Project_*` contract + the boot-authored scene.
* For richer examples: `Games/Combat` (multi-scene + Behaviour Graphs),
  `Games/DevilsPlayground` (physics + the Docs/ governance model this game
  copies), `Games/RenderTest` (rendering + terrain authoring showcase).

## Android

This game is win64-only (`"android": false` in `Zenithmon.zproj`). To add an
Android build: copy an existing game's `Android/` Gradle tree (e.g.
`Games/Combat/Android`), retarget its package/name, set `"android": true` in the
descriptor, and run `zenith regen`.
