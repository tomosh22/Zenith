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
  Source/World/                  # Scene placement + terrain/encounter authoring --
                                 #   ZM_DawnmerePlacement.{h,cpp},
                                 #   ZM_PlayerHomePlacement.h, ZM_ProfLabPlacement.h
                                 #   (the compiled anchors each scene is authored from)
  Source/UI/, Source/Save/, ...  # + Core, Gen, Graph, Interaction, Nav, Party,
                                 #   Shop, CareCenter -- by-value, non-ECS logic
  Tests/ZM_Tests_*.cpp           # Boot, data, stats, battle, and integrity units
  Tests/ZM_AutoTests_*.cpp       # Harness-managed automated/windowed tests
  Assets/Scenes/                 # BOOT-AUTHORED (see below) and COMMITTED --
                                 #   all FIVE scenes are tracked (ZM-D-148/174)
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

## Scene authoring + committed assets (IMPORTANT)

**SIX assets ARE committed** (verify with `git ls-files Games/Zenithmon/Assets`):
`Assets/Navmesh/Dawnmere.znavmesh` (ZM-D-147) and all **five**
`Assets/Scenes/*.zscen` -- `Battle`, `Dawnmere`, `FrontEnd`, `PlayerHome`,
`ProfLab` (the fifth added by ZM-D-174) (ZM-D-148). `.gitignore` re-includes
`**/*.zscen` and `**/*.znavmesh` at any depth, which is what lets CI verify
navigation and scene content with no GPU and no bake. **On a normal clone you
need no bake step: the committed bytes are what the game loads**, in `_True`,
`_False` and Android builds alike.

Scenes are AUTHORED by `Project_RegisterEditorAutomationSteps` -- a
**tools-only** function, compiled out of non-tools builds -- and saved via
`AddStep_SaveScene`. So if you ever need to REGENERATE them (a deliberate
re-author, not routine work):

* Only a **`*_True` config** (e.g. `Vulkan_vs2022_Debug_Win64_True`) can author
  a scene at all. A `_False` boot can only load.
* The navmesh and `Dawnmere.zscen` additionally need a **windowed** boot, and
  Dawnmere needs every terrain recipe already warm -- so on a fresh clone it
  takes **two** `_True` boots to regenerate them (boot 1 bakes terrain, boot 2
  authors Dawnmere).
* Dawnmere's re-author is gated behind an explicit authoring mode
  (`sceneAuthoring=AUTHOR_DAWNMERE`); a `DEFERRED` boot silently authors nothing
  and still looks successful.

Scene bytes are boot-shape-independent (dense authoring-order file indices), so a
boot must NOT leave a scene modified in `git status`. If one ever does, that is a
regression of that property -- investigate it rather than just re-committing.

That invariant held again from ZM-D-179 (2026-08-01), which fixed the one instance
it had ever lost: `Zenith_TransformComponent` used to serialize the LIVE JOLT BODY's
pose, so any authored entity with a body was saving a rotation that depended on
physics state, and `Npc_RivalVesper`'s drifted by ~10 ULP in one boot. Serialization
now emits the transform's own cached pose unless the body has genuinely moved, and a
tools-only guard bit-compares the rival's serialized rotation with
`ZM_DawnmereVesperFacing()` immediately before the save. See `Docs/DecisionLog.md`
ZM-D-179 / `Docs/Questions.md` Q-2026-08-01-002.

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
