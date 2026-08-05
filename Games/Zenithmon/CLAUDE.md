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
                                 #   ZM_HumanBody.h -- THE body contract: how big a
                                 #     person is (1.8 m tall, 0.8 m footprint, a
                                 #     0.4/0.5 capsule), installed explicitly and
                                 #     NEVER derived from transform scale (ZM-D-181)
                                 #   ZM_HumanAssetPolicy.{h,cpp} -- the injectable
                                 #     "is the human bake loadable right now?" seam;
                                 #     how a test forces the cold-start fallback
                                 #     without touching shared baked files
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

That invariant has been lost **twice**, both times on `Npc_RivalVesper`'s rotation
and both times with every existing guard green. Read both before touching it:

* **ZM-D-179** (2026-08-01) -- `Zenith_TransformComponent` serialized the LIVE JOLT
  BODY's pose, so any authored entity with a body saved a rotation that depended on
  physics state. Serialization now emits the transform's own cached pose unless the
  body genuinely moved, and a tools-only guard bit-compares the rival's serialized
  rotation with `ZM_DawnmereVesperFacing()` immediately before the save.
* **ZM-D-183** (2026-08-04) -- the AUTHORED VALUE ITSELF was build-configuration
  dependent. `ZM_DawnmereVesperFacing()` was a runtime `std::atan2` fed to
  `glm::angleAxis`, and MSVC Debug and Release codegen disagree on those by 1-2 ULP,
  so a **Release** tools boot and a **Debug** tools boot wrote different bytes and
  the file ping-ponged in git forever. The facing is now four FROZEN `std::bit_cast`
  constants, authored via `AddStep_SetTransformRotationQuat` (verbatim, no math).

> **★ IF THIS BREAKS A THIRD TIME, DO NOT ASSUME IT IS THE SERIALIZER.** ZM-D-183
> presented identically to ZM-D-179 and had an unrelated cause. The diff tells you
> which: `WriteToDataStream` takes position and rotation from the SAME source, so a
> falling/moved body CANNOT produce a drifted rotation beside a bit-identical
> position. Rotation-only drift means the authored value moved, not the body.
>
> **★ AND DO NOT TRUST THE PRE-SAVE GUARD TO CATCH IT.** It compares the serialized
> bytes against `ZM_DawnmereVesperFacing()` evaluated in the SAME BINARY, so when the
> computation itself moves, both sides move together and it logs a clean
> `authored == serialised == liveBody` while writing bytes that differ from git. The
> headless guard on the COMMITTED bytes is
> `Tests/ZM_Tests_CommittedSceneBytes.cpp` -- that is the one that can actually see
> it, because CI runs `Null_..._True` and never performs the windowed authoring boot
> the pre-save guard lives on.
>
> **★ RULE:** any authored entity whose rotation lands in a COMMITTED scene file uses
> `AddStep_SetTransformRotationQuat` with a frozen constant. `AddStep_SetTransformYaw`
> / `...RotationEuler` build their quaternion with libm at authoring time and are only
> safe for transient or gitignored scenes. (Identity is exact in every config, which
> is why the four shipped townsfolk never surfaced this.)

See `Docs/DecisionLog.md` ZM-D-179 and ZM-D-183 / `Docs/Questions.md`
Q-2026-08-01-002.

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

This game ships an Android build (`"android": true` in `Zenithmon.zproj`), with
its Gradle tree under `Games/Zenithmon/Android/` (`com.zenith.zenithmon`). It is
multi-ABI: `app/build.gradle` applies the shared axis
`Build/zenith_android_abis.gradle`, so `abiFilters` and the `jniLibs` roots come
from the one list that mirrors `ZenithAndroidAbi.All` in
`Build/Sharpmake_Common.cs` — arm64-v8a for devices, x86_64 for the emulator
(the only ABI a dev box with no ARM hardware can execute).

```
msbuild Games\Zenithmon\zenithmon_agde.sln /t:Zenithmon ^
  /p:Configuration=Vulkan_x86_64_vs2022_Debug_Agde_False /p:Platform=Android-x86_64
adb install -r -t <apk>        REM -t is REQUIRED: AGP marks debug APKs testOnly
```

The manifest declares `landscape` as a game-design choice. Orientation is no
longer a technical constraint: the swapchain requests an `IDENTITY` preTransform
so the compositor handles any surface rotation — including landscape on a
portrait-native panel, which is what the emulator (`Medium_Phone`) is.
See `Zenith/Android/CLAUDE.md` for the full bring-up sequence, the two ABI
spellings, and how surface rotation is handled.
