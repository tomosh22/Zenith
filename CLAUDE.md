# Zenith Game Engine

## Overview

C++20 game engine with custom ECS, Vulkan-based renderer (Flux), and multi-threaded task system.

> **New to the codebase?** Read [Docs/Onboarding/NewcomerMap.md](Docs/Onboarding/NewcomerMap.md) first — it covers startup flow, recommended reading order, and the sharp edges that trip up first-timers.

## Directory Structure

```
Zenith/
├── ZenithECS/         # ECS leaf lib: entity/scene/query/event/component-meta machinery (see ZenithECS/CLAUDE.md)
├── EntityComponent/   # Concrete components + ECS<->engine glue (see EntityComponent/CLAUDE.md)
├── Scripting/         # Behaviour Graph runtime: visual scripting interpreter (see Scripting/CLAUDE.md)
├── Flux/              # Vulkan renderer (see Flux/CLAUDE.md)
│   ├── Decals/        # Deferred decals (see Flux/Decals/CLAUDE.md)
│   ├── Fog/           # Volumetric fog (see Flux/Fog/CLAUDE.md)
│   ├── Gizmos/        # Editor gizmos (see Flux/Gizmos/CLAUDE.md)
│   ├── HDR/           # HDR pipeline (see Flux/HDR/CLAUDE.md)
│   ├── HiZ/           # Hierarchical Z-buffer (see Flux/HiZ/CLAUDE.md)
│   ├── IBL/           # Image-based lighting (see Flux/IBL/CLAUDE.md)
│   ├── MeshAnimation/ # Skeletal animation (see Flux/MeshAnimation/CLAUDE.md)
│   ├── RenderGraph/   # Render graph compile/execute (see Flux/RenderGraph/CLAUDE.md)
│   ├── Shadows/       # Cascaded shadow maps (see Flux/Shadows/CLAUDE.md)
│   ├── SSR/           # Screen-space reflections (see Flux/SSR/CLAUDE.md)
│   ├── SSGI/          # Screen-space GI (see Flux/SSGI/CLAUDE.md)
│   ├── Terrain/       # Terrain rendering (see Flux/Terrain/CLAUDE.md)
│   ├── Vegetation/    # Grass system (see Flux/Vegetation/CLAUDE.md)
│   └── ...            # + Backend, DeferredShading, DynamicLights,
│                      #   InstancedMeshes, MaterialPreview, MeshGeometry, Particles,
│                      #   Present, Primitives, Quads, SDFs, SceneGraph, Shaders, Skybox,
│                      #   Slang, SSAO, UnifiedMesh, Text, Translucency, and more
├── AI/                # AI systems (see AI/CLAUDE.md); decisions live in Scripting/ graphs
│   ├── Navigation/    # NavMesh pathfinding (see AI/Navigation/CLAUDE.md)
│   ├── Perception/    # Sensory systems (see AI/Perception/CLAUDE.md)
│   └── Squad/         # Squad tactics (see AI/Squad/CLAUDE.md)
├── Editor/            # Editor tools (see Editor/CLAUDE.md)
├── TaskSystem/        # Task parallelism (see TaskSystem/CLAUDE.md)
├── Physics/           # Jolt Physics integration (see Physics/CLAUDE.md)
├── Vulkan/            # Vulkan backend (see Vulkan/CLAUDE.md)
├── D3D12/             # No-op null render backend (Flux neutrality proof) (see D3D12/CLAUDE.md)
├── Core/              # Core utilities (see Core/CLAUDE.md)
├── Collections/       # Custom containers (see Collections/CLAUDE.md)
├── Maths/             # GLM wrapper (see Maths/CLAUDE.md)
├── AssetHandling/     # Asset import/export (see AssetHandling/CLAUDE.md)
├── Windows/           # Windows platform layer (see Windows/CLAUDE.md)
├── Android/           # Android platform layer (see Android/CLAUDE.md)
├── DataStream/        # Binary serialization (see DataStream/CLAUDE.md)
├── SaveData/          # Save/load persistence (see SaveData/CLAUDE.md)
├── DebugVariables/    # Runtime debug variable tree (see DebugVariables/CLAUDE.md)
├── Input/             # Input handling (see Input/CLAUDE.md)
├── Profiling/         # CPU profiling system (see Profiling/CLAUDE.md)
├── Telemetry/         # Runtime telemetry capture
├── UnitTests/         # Unit-test harness
├── Prefab/            # Prefab system (see Prefab/CLAUDE.md)
├── UI/                # UI framework (see UI/CLAUDE.md)
└── FileAccess/        # File system abstraction (see FileAccess/CLAUDE.md)
```

## Dependencies

- Vulkan SDK 1.3
- Jolt Physics
- GLFW
- GLM
- ImGui (docking branch)

## Naming Conventions

### Class/Struct Names
- `PascalCase` with namespace prefix
- Core engine classes: `Zenith_ClassName` (e.g., `Zenith_Entity`, `Zenith_Scene`)
- Renderer classes: `Flux_ClassName` (e.g., `Flux_Texture`, `Flux_RenderGraph`)

### Variable Scope Prefixes
| Prefix | Meaning | Example |
|--------|---------|---------|
| `m_` | Member variable | `m_uEntityID` |
| `s_` | Static member | `s_xCurrentScene` |
| `g_` | Global variable | `g_pxAnimUpdateTask` |
| `ls_` | Local static | `ls_bOnce` |

### Type Prefixes (after scope prefix)
| Prefix | Type | Example |
|--------|------|---------|
| `x` | Struct/class instance | `m_xComponents`, `xEntity` |
| `p` | Pointer | `m_pData`, `pData` |
| `px` | Pointer to class/struct | `m_pxParentScene`, `pxAnim` |
| `a` | Array | `m_axColourAttachments`, `auIndices` |
| `u` | Unsigned int | `m_uWidth`, `uIndex` |
| `b` | Bool | `m_bInitialised`, `bSuccess` |
| `f` | Float | `m_fScale`, `fDeltaTime` |
| `str` | String | `m_strName`, `strPath` |
| `e` | Enum value | `m_eFormat`, `eType` |
| `pfn` | Function pointer | `m_pfnFunc` |
| `ul` | Unsigned long (u_int64) | `m_ulSize` |
| `i` | Signed int | `iIndex` |

### Functions
- `PascalCase` (e.g., `GetPosition`, `BuildModelMatrix`, `WriteToDataStream`)

### Enums
- `SCREAMING_SNAKE_CASE` with category prefix
- Values: `CATEGORY_NAME_VALUE` (e.g., `TEXTURE_FORMAT_RGBA8_UNORM`, `RESOURCE_ACCESS_READ_SRV`)

### Constants
- `SCREAMING_SNAKE_CASE` with type prefix (e.g., `uFLUX_MAX_TARGETS`, `fCHAR_ASPECT_RATIO`)

## Code Style

### Braces
Opening braces on new line for class, function, and control flow blocks. Short inline one-liners in headers may use same-line braces:
```cpp
class Zenith_Entity
{
public:
    void DoSomething()
    {
        if (condition)
        {
            // code
        }
    }

    // Short inline getters may use same-line braces
    bool IsValid() const { return m_bValid; }
};
```

### Indentation
- Tabs (not spaces)

### Class Layout
1. `public:` members first
2. `protected:` members
3. `private:` members last

### Headers
- Use `#pragma once` for include guards
- Avoid `using namespace` in headers

### Conditionals
- Tools-only code wrapped in `#ifdef ZENITH_TOOLS`

### Precompiled Header
- All .cpp files must begin with `#include "Zenith.h"`

## Build System

The project uses [Sharpmake](https://github.com/ubisoft/Sharpmake) to generate Visual Studio solution files.

> This section is the quick-start subset. The full reference — CLI commands +
> exit codes, configurations/outputs/PCH structure, the test harness and
> baselines, packaging (`--assets-root`), the CI matrix, hygiene and
> troubleshooting, contributor invariants — is
> **[Docs/BuildSystem.md](Docs/BuildSystem.md)**. The descriptor system
> (`.zproj` schema, codegen, templates, hub) is
> **[Docs/GameProjects.md](Docs/GameProjects.md)**.

### Generating Solution Files

Games are **descriptor-driven**: each game has a `Games/<Name>/<Name>.zproj` (JSON —
`schemaVersion`, `name`, `android`, optional `extraDefines`/`extraSharpmakeProjects`).
Adding or removing a game touches only its descriptor — **no Sharpmake C# edits**.

Regenerate every solution from the repo root (or `Build/`):
```batch
Build\regen.ps1
```
(`zenith regen` forwards here.) regen.ps1 validates all descriptors, codegens
`Build/Sharpmake_GameInstances.generated.cs`, runs Sharpmake once, and fixes up AGDE.

This generates **per-game solutions** plus one **engine-only** solution — there is
NO all-games solution:
- `Games/<Name>/<name>_win64.sln` — one per game (+ `<name>_agde.sln` when `android:true`)
- `Build/zenith_engine_win64.sln` — engine libs + Sentinels + tools + ZenithHub, **zero games**

**Regenerate-first policy:** EVERYTHING Sharpmake emits is gitignored — all `.sln`,
`.vcxproj`, `.vcxproj.filters`, `.vcxproj.user`, and the generated `.cs`. After a fresh
clone, or a checkout/pull that touches any `.zproj` or `Sharpmake_*.cs`, run
`Build\regen.ps1` (or `zenith regen`) before building — generated projects on disk are
untracked and can go stale silently (`zenith regen --check` reports staleness). Test
results, telemetry, and build logs are also never committed; runners write under
`Build/artifacts/`. See **`Docs/GameProjects.md`** for the schema, validation rules,
sln inventory, CI mapping, and troubleshooting.

### Build Configurations

Each project supports these configurations. Every win64 config is prefixed by the
render backend: `Vulkan_` (the real renderer), `Null_` (the GPU-less backend that
**every headless run executes on**), or `D3D12_` (a reserved no-op backend kept as
the link-neutrality proof). The table shows the `Vulkan_` rows in full; the other
two prefixes offer the same four combinations. **agde carries the `Vulkan_` prefix
too, plus an ABI fragment** — `Vulkan_arm64_v8a_…` (devices) and `Vulkan_x86_64_…`
(the emulator, the only ABI a machine with no ARM device can run).

> **HEADLESS IS A BUILD CONFIG, NOT A FLAG.** There is no `--headless`. A `Null_*`
> config defines `ZENITH_NULL_RENDERER`, compiles `Zenith/Null` instead of Vulkan,
> and creates its window hidden. Every render path still RUNS — pass callbacks,
> uploads, the editor ImGui frame — against no-op backend calls, so a headless run
> exercises the same code a windowed one does. `zenith build|test <G> --headless`
> selects this config. Compile-time checks use `Zenith_IsNullRenderer()` (or
> `#ifdef ZENITH_NULL_RENDERER` where one side cannot compile).

| Configuration | Platform | Tools | Description |
|--------------|----------|-------|-------------|
| `Vulkan_vs2022_Debug_Win64_True` | Windows | Yes | Debug build with editor/tools |
| `Vulkan_vs2022_Debug_Win64_False` | Windows | No | Debug build, runtime only |
| `Vulkan_vs2022_Release_Win64_True` | Windows | Yes | Release build with editor/tools |
| `Vulkan_vs2022_Release_Win64_False` | Windows | No | Release build, runtime only |
| `Null_vs2022_Debug_Win64_True` | Windows | Yes | **The headless/CI build** — GPU-less, hidden window |
| `D3D12_vs2022_Debug_Win64_False` | Windows | No | Reserved-backend link/neutrality proof (+ _True / Release variants) |
| `Vulkan_arm64_v8a_vs2022_Debug_Agde_False` | Android (`Android-arm64-v8a`) | No | Android debug, physical devices |
| `Vulkan_arm64_v8a_vs2022_Release_Agde_False` | Android (`Android-arm64-v8a`) | No | Android release, physical devices |
| `Vulkan_x86_64_vs2022_Debug_Agde_False` | Android (`Android-x86_64`) | No | **The emulator build** (+ Release variant) |

### Projects

| Project | Description | Output |
|---------|-------------|--------|
| Zenith | Core engine library | Static lib (.lib) |
| FluxCompiler | Shader compiler utility (Windows only) | Executable (.exe) |
| ZenithTools | Asset tools (Windows only) | Executable (.exe) |
| Game projects | Combat, TilePuzzle, RenderTest, CityBuilder (SimCity/C:S-style — see Games/CityBuilder/CLAUDE.md), DevilsPlayground, Zenithmon (monster-collecting RPG — see Games/Zenithmon/CLAUDE.md) | Executable (.exe) / Shared lib (.so) |

### Building and Running

**The `zenith` CLI (recommended):**
```batch
zenith new <Name>          REM scaffold a new game (regen + open its sln)
zenith build Combat       REM msbuild the game's per-game sln (/t:<Game>)
zenith run Combat         REM launch the newest built exe
zenith test Combat        REM run the game's automated tests (or: zenith test all)
zenith open Combat        REM regen + open the game's sln in Visual Studio
zenith list                REM list games + built configs
zenith clean Combat       REM kill hanging build processes + wipe output/obj
zenith package Combat     REM stage a relocatable build into dist/ (run.bat --assets-root)
zenith regen --check       REM report whether on-disk generated files are stale
zenith hub                 REM launch the Unity-Hub-style GUI launcher
```

**Direct msbuild (per-game solution):**
```batch
msbuild Games\Combat\combat_win64.sln /t:Combat /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Combat\Build\output\win64\vulkan_vs2022_debug_win64_true\combat.exe
```
Always build with `/t:<Game>`, never the whole solution: the aux tools (FluxCompiler /
font libs) present in the sln are pre-existing-red in `ToolsEnabled=True`.

> **Config-name prefix (RenderBackend fragment):** every win64 config is prefixed
> with the render backend — `Vulkan_vs2022_Debug_Win64_True` (the real renderer),
> `Null_vs2022_Debug_Win64_True` (the GPU-less headless/CI backend; see
> `Zenith/Null/CLAUDE.md`), or `D3D12_vs2022_Debug_Win64_False` (the reserved
> backend kept as a link-neutrality proof; see `Zenith/D3D12/CLAUDE.md`). The
> output dir is the lowercased config name. AGDE configs carry the Vulkan prefix
> plus an ABI token (for example `Vulkan_arm64_v8a_vs2022_Debug_Agde_False`).

**Using Visual Studio:** `zenith open <Name>` regenerates then opens
`Games/<Name>/<name>_win64.sln`; set the game as startup project, pick a config
(e.g. `Vulkan_vs2022_Debug_Win64_True|x64`), F5.

**Engine-only work:** `msbuild Build\zenith_engine_win64.sln /t:Zenith` (or
`/t:FluxCompiler` / `/t:Sentinel*` / `/t:ZenithHub`); `zenith build engine` does this.
Never build the whole engine sln (pre-existing-red aux tools).

### Build Troubleshooting

#### Hanging Compiler Processes

MSBuild can sometimes leave hanging `cl.exe` (compiler) and `mspdbsrv.exe` (debug database server) processes that lock build files, preventing subsequent builds from succeeding.

**Symptoms:**
- Build fails with "cannot access file" or "file is being used by another process" errors
- Multiple retries to copy `.pdb` files fail
- Errors like: `error MSB3027: Could not copy "zenith_compiler.pdb"`

**Cause:**
Parallel builds (`-maxCpuCount`) or interrupted builds can leave compiler subprocesses running. These processes hold locks on `.pdb`, `.pch`, and `.obj` files.

**Solution:**

```batch
zenith clean                  REM kill hanging cl/mspdbsrv/link/vctip/msbuild
zenith clean Combat          REM ...and also wipe that game's output/ + obj/
zenith clean engine           REM ...engine + hub intermediates
zenith clean --processes-only REM just the process sweep
```

`zenith build` also self-heals on entry: it kills compiler processes older than
30 minutes (genuinely hung ones only — a live concurrent build is never touched)
before invoking MSBuild, and accepts `--timeout <min>` as a watchdog that kills
the msbuild tree if a build wedges. (Implementation:
`Stop-ZenithBuildProcesses` in `Build/zenith_buildsystem.psm1`.)

**Prevention:**
- Builds use parallel compilation (`-maxCpuCount`, all cores). If a parallel build leaves hanging processes, run `zenith clean` before retrying — don't fall back to single-threaded
- Always let builds complete or use Ctrl+C to properly terminate
- Close Visual Studio when building from command line to avoid file conflicts

### Key Defines

Set automatically by the build system:

| Define | Description |
|--------|-------------|
| `ZENITH_TOOLS` | Enables editor/tools code (True configurations) |
| `ZENITH_WINDOWS` | Windows platform build |
| `ZENITH_ANDROID` | Android platform build |
| `ZENITH_DEBUG` | Debug build |
| `ZENITH_VULKAN` | Vulkan renderer enabled |
| `GAME_ASSETS_DIR` | Absolute path to game assets |
| `ENGINE_ASSETS_DIR` | Absolute path to engine assets |
| `SHADER_SOURCE_ROOT` | Path to shader source files |

### Sharpmake Files

Located in `Build/`:
- `Sharpmake_Common.cs` - Base project class, platform configuration (no `[Sharpmake.Main]`)
- `Sharpmake_Zenith.cs` - Zenith engine project
- `Sharpmake_FluxCompiler.cs` - Shader compiler project
- `Sharpmake_Games.cs` - Abstract `GameProject` / `GameSolution` bases (concrete per-game classes are generated)
- `Sharpmake_Solutions.cs` - `[Sharpmake.Main]` + SHA256 manifest guard + engine-only `ZenithEngineSolution`
- `Sharpmake_GameInstances.generated.cs` - **generated** per-game project/solution shells + manifest (gitignored)
- `Sharpmake_ZenithHub.cs` - Unity-Hub-style launcher project (engine sln only)
- `zenith_buildsystem.psm1` - descriptor scan / validate / codegen / name-validation + shared ops (config accessors, process sweep, DLL heal, drift check, orphan prune) — single source of truth
- `zenith_config.psd1` - central config DATA (default/hub configs, CI-provisioned versions, artifact root); read via the psm1 accessors only
- `regen.ps1` - canonical regenerator (worktree-guard -> validate -> codegen -> one Sharpmake run -> AGDE fixup -> orphan prune)
- `fix_agde_vcxproj.ps1` - AGDE post-Sharpmake fixup (clang c++ standard token)
- `Sharpmake_ZenithTools.cs` - Asset tools project
- `Sharpmake_ZenithAI.cs` - AI module project
- `Sharpmake_ZenithECS.cs` - ECS module project
- `Sharpmake_ZenithPhysics.cs` - Physics module project
- `Sharpmake_SentinelAI.cs` / `Sharpmake_SentinelECS.cs` / `Sharpmake_SentinelPhysics.cs` - Sentinel test modules
- `Sharpmake_FreeType.cs` / `Sharpmake_Msdfgen.cs` / `Sharpmake_MsdfAtlasGen.cs` - Font/text dependency projects
- `Sharpmake_TilePuzzleLevelGen.cs` / `Sharpmake_TilePuzzleRegistryViewer.cs` - TilePuzzle tooling projects

## External agent board (projects `ZM`, `ZEN`, `DP`)

Some work in this repo is queued and executed by an autonomous Claude
Code loop driven from a Jira-style board. **The board is a web
application that may run on an entirely different machine**, and nothing
here knows where its source lives. This repo remains the source of truth
for everything — the board is a queue and an audit log, not a spec. You
can develop here and ignore it entirely, with three exceptions.

**Everything the loop enforces lives HERE, committed:**

| File | Carries |
|---|---|
| `zagent.project.json` | Gate command lines, pinned unit baselines, per-category conventions, branching mode, living-doc directories. **Sent to the board with every request** |
| `.claude/commands/tick.md` | The `/tick` protocol — nine steps, seven invariants |
| `Tools/zagent/` | The client — `zagent.ps1` (argv/env/transport), `ZagentClient.psm1` (pure helpers), `Test-ZagentClient.ps1` (the assert script — it prints its own count; do not pin one here, this line has been stale twice), `zagent.cmd` shim. **No Node, no `node_modules`** |
| `.zagent/` | Run scratch (`last.json`, `run/<KEY>/`). **Gitignored**, so the dirty-tree precondition still holds |

The board keeps only POLICY — the agent account, the complexity→model
routing, guardrail floors — and holds no path into this filesystem.
`zagent.project.json` declares which board projects this checkout serves;
that inversion is the only direction that survives the two being on
separate machines.

**One board project per game area.** `ZM` is Zenithmon, `ZEN` the engine,
`DP` DevilsPlayground — each declared by its CATEGORY in
`zagent.project.json`, with `ZEN` as the checkout default. Each area wants
its own epics, sprints, releases and burndown, which is what a project is
for; a component could carry none of them.

**I5 is unaffected, and that is deliberate.** Every project this file
declares resolves to the SAME checkout, so the advisory lock and the
in-flight check still mean **one ticket at a time for `C:\dev\Zenith`**,
not one per board. `zagent next` with no `--project` walks all three in
declaration order and reports "nothing to claim" only when every one is
empty.

**Blockers are mechanical now.** A `BLOCKS` link between tickets is the
one thing on the board that changes what the loop DOES: the claim query
refuses any ticket whose predecessor has not reached a DONE-category
status, and a targeted claim on one comes back exit 4 naming the ticket
to finish first. Zenithmon's R1-x chain used to be a `deps` column in a
Markdown table inside `Status.md`, which nothing could enforce.

The file is **sent every time rather than registered once**. A stored
copy goes stale, and a stale gate list is exactly how a green run
ratchets the wrong pinned baseline. The board keeps a mirror purely so
its web UI and file-time `create` validation can see categories.

`protectedPaths` from both sides are UNIONed, so this repo may add
protections to itself and can never drop the board's. Both files above
are protected: the loop may file a Suggestions row about its own gates,
never commit a change to them.

**A pinned unit baseline has exactly ONE home: `Tools/unit_baselines.json`.**
Gate lines and workflows name the GAME — `-Game Zenithmon` — and
`run_unit_gate.ps1` resolves the number, deriving the game from the `-Exe`
path when `-Game` is omitted. An explicit `-Baseline N` still wins, so any
caller that passed a number keeps working.

| | |
|---|---|
| The number | `Tools/unit_baselines.json` — **the only place it is written** |
| Who reads it | `zm-tests.yml`, `engine-gate.yml`, `test_scaffold.ps1`, `zagent.project.json`, all by game name |
| Human narration | `Games/*/Docs/Status.md` — read by no gate; stale there reds nothing |

It used to be duplicated across `Status.md`, `.github/workflows/*.yml`,
`run_unit_gate.ps1`'s default and `zagent.project.json`. Two of those are
`protectedPaths`, which made **bumping a pin work the agent loop could
never do** — so every test-adding ticket was unreachable, in a repo whose
conventions ask for a test with all new code. The gate asserts
`ran == Baseline` EXACTLY, so a stale pin still reds a required check with
**zero failing tests**; the difference is that fixing it is now one line in
one unprotected file.

A backend-neutral engine unit still moves **every** game's row, because
they all boot the same engine suite — bump them together, and note which
count you actually measured versus inferred.

**Branching policy is enforced mechanically per area.** Zenithmon and
Engine tickets carry `branching: "direct"` — the loop commits straight
to `master` and hard-refuses `git switch -c`, `git worktree` and
`gh pr` (ZM-D-031). DevilsPlayground overrides to `"branch"`, matching
its `Docs/CIPolicy.md` squash-merge policy. Nothing is ever pushed.

**Leave `master` clean.** The loop's precondition check treats a dirty
tree as fatal and refuses the ticket rather than stashing around it — it
has to, since `direct` mode commits in place. Uncommitted work here
silently blocks the queue.

Gate command lines are copied VERBATIM from this repo's own docs and CI
(`.github/workflows/{zm-tests,engine-gate,dp-tests}.yml`,
`Tools/run_unit_gate.ps1`), never paraphrased. If you change how a game
is built or tested, `zagent.project.json` is a downstream consumer in
the same commit.

**WHICH gate lines run is decided by the DIFF, not by the filing
category.** Each category declares the directories it owns (`paths`), and
`zagent gates <KEY>` unions in the gates of every category the change
actually reached — so a ticket filed `Zenithmon` that edits `Zenith/**`
builds and tests Combat and checks the engine pin as well. It only ever
ADDS; the category's own list stays a prefix. `zagent guard <KEY>`
re-derives that selection and refuses a stale one, which is what keeps it
from being something a tick has to remember.

Four consecutive tickets edited engine code under a one-game gate list
before this existed. ZM-50's own Goal said *"the fix is engine-side"*
while `category: Zenithmon` meant the gates never built Combat — the body
and the category disagreed and only the category had any mechanical
effect. Adding `Zenith/**` to a category's `paths` costs those gates on
every ticket that touches it, which is the trade; `Tools/**` is
deliberately NOT listed, because a one-line pin bump in
`Tools/unit_baselines.json` does not need Combat rebuilt.

### Running the loop

A bare `claude` started in this repo is the whole setup:

```
/tick                # one tick against the head of whichever board has work
/tick ZM             # ...constrained to Zenithmon's board
/tick ZM-22          # run that ticket wherever it sits, once
/loop /tick          # unattended, until the queues empty
```

**No `--add-dir`, no second checkout.** The scratch is local, the client
is local, and the board is reached over HTTP. What the machine does need
is `ZAGENT_URL` and `ZAGENT_TOKEN` — see `Tools/zagent/README.md`;
`zagent doctor` names either if it is missing.

### The `zagent` CLI

`zagent` is the board's command line — the **only** thing that writes to
it from outside the web app. It lives HERE, in `Tools/zagent/`, as a
PowerShell script:

```
zagent <command> [--json]
```

That works from `C:\dev\Zenith` and from any subdirectory of it
(`Games\Zenithmon`), in PowerShell, cmd or Git Bash — it walks up for
`zagent.project.json` the way git walks up for `.git`, so a run from
`Games\Zenithmon` reads the same gates as one from the repo root.

Its scratch is `.zagent/last.json` and `.zagent/run/<KEY>/` at the repo
root, gitignored so `git status --porcelain` stays empty while a ticket
is in flight — the loop's precondition depends on that.

If `zagent` is not on PATH, `Tools/zagent/README.md` has the one-time
setup; you can always call `pwsh -NoProfile -File Tools\zagent\zagent.ps1`
directly.

The client has its own tests — a plain assert script, matching
`Tools/Test_T0Harness_*`, because the only PowerShell this repo requires
is the `pwsh` every gate already needs:

```
pwsh -NoProfile -File Tools/zagent/Test-ZagentClient.ps1
```

They cover the parts that bite: `,@(…)` returns (a PowerShell function
UNROLLS a returned collection, so a one-element `cleanup` array would go
on the wire as a bare string), separator folding (a mismatch makes an
uploaded docs tree match nothing, which mirrors zero pages and reports
success), and reading optional fields through `PSObject.Properties[…]`
(`Set-StrictMode` turns a missing property into a throw, so a project
file that legally omits `baseBranch` would make `doctor` crash instead of
report).

★ **A parse-check plus green unit tests is NOT proof the client runs.**
Splitting the helpers into a module once left `Get-BoardUrl` unexported:
both files parsed, all 47 assertions passed, and every command failed.
Smoke the real binary after touching either file.

Commands you would actually use from here:

| Command | What it does |
|---|---|
| `queue` | what is sitting in Ready for Agent (the repo declares its own project) |
| `auth mint --name <machine>` | **on the board machine only** — a revocable token for one client |
| `show ZEN-6` | one ticket: routing, category, gates, contract errors |
| `create --project ZEN --title "…" --file body.md --category Zenithmon --complexity … --risk …` | file a ticket |
| `doctor` | pre-flight, merged from BOTH sides: the board answers for DB/account/lanes/categories, this machine for branch, cleanliness and gate executables |
| `owns ZEN-6` | is the loop still holding it |
| `gates ZEN-6` | the gate list this DIFF needs — the ticket's own plus every category whose `paths` it touched |
| `guard ZEN-6` | protected paths, **and** that the recorded gate selection still describes the diff |
| `docs ls [<path>]` | the Notion page tree, one full path per line |
| `docs read <path>` | a page body as Markdown, on stdout |
| `docs write <path> --file f.md` | write ONE page — **refuses a mirrored page** |
| `docs search "<text>"` | titles + bodies, with a snippet |
| `rows decisions\|questions\|shortfalls\|changelog\|suggestions` | read the knowledge databases |
| `docs status --project ZEN` | what a living-doc sync would change; writes nothing |
| `docs sync --project ZEN` | mirror `Games/*/Docs` into the Notion page tree |
| `link <KEY> blocks\|relates\|duplicates\|causes <KEY>` | one directed link; `blocks` is the one the claim query reads |
| `blocked --project ZM` | everything waiting, and on what |
| `links <KEY>` / `epic <KEY>` / `parent <KEY> <PARENT>` | the dependency graph and the hierarchy |
| `sprint list\|create\|start\|complete\|add\|remove` | one ACTIVE sprint per project |
| `version list\|create\|release\|set` | milestones — what a BUILD contains |
| `update <KEY> --points --severity --repro --environment --due` | field writes, with a history trail |
| `estimate` / `flag` / `resolve` / `history` | the one-gesture shorthands |
| `report epic\|velocity --project ZM` | per-epic progress; velocity over COMPLETED sprints only |
| `board status --project ZM` | does `Roadmap.md` still describe the board? **exit 1 on drift** |

**Reading a doc from a Zenith session no longer needs a browser.** A
`<path>` is slash-separated page TITLES resolved under
`Agent/Documentation`, so `zagent docs read Zenithmon/Status` is the
whole command — no page ids, no `--project`, no `--org`. `docs ls`
prints exactly what `docs read` accepts, and a mistyped name comes back
with the siblings listed rather than a bare failure. `--recursive`
concatenates a document the sync split into parts
(`zagent docs read Zenithmon/DecisionLog --recursive --out DecisionLog.md`).

In **Git Bash**, spell the project root `//Agent/Scratch/…`, not
`/Agent/Scratch/…` — MSYS rewrites a single leading slash into
`C:/Program Files/Git/…` before the CLI is started. PowerShell and cmd
take either.

**`Games/*/Docs` stays authoritative, and that is now MECHANICAL.** A
`docs write` aimed at a mirrored page is REFUSED, naming the source file
to edit:

```
mirrored from Games/Zenithmon/Docs/Status.md — edit that file and run
`zagent docs sync`. --force writes anyway, and the next sync will then
report this page as a conflict and skip it.
```

That is not politeness. A hand write moves the page's fingerprint away
from what the last sync recorded, so every later sync reports it as a
human edit and skips it — the page stops tracking its file silently and
forever. Targeted writes are for pages the mirror does not own.

None of this reaches a **loop worker**, which still has no shell and no
network and still reads `Docs/AgentBriefing.md` off disk with the Read
tool. This is for your session and for the tick.

**The living docs are MIRRORED into Notion, one way.** `zagent docs
sync` renders `Games/Zenithmon/Docs` and `Games/DevilsPlayground/Docs`
into a page tree — one section per game, a page per file, and an index
page with a child per part for anything past the per-page budget
(`DecisionLog.md` is 932 KB). **These files stay authoritative.** Nothing
writes back: `Tools/doc_lint.ps1` still lints them off disk,
`zagent decide` still appends to `Games/Zenithmon/Docs/DecisionLog.md`,
`Status.md` is still the authority for the pinned ZM baseline, and a
loop worker — which has no shell and no network — still reads its
binding briefing with the Read tool. Edit the Markdown; re-run the sync.

A Notion page a human has edited is reported as a conflict and SKIPPED,
never replaced, so a comment added in the browser survives the next
sync (`--force` if you actually want to discard it).

Every page write is verified by re-reading the persisted CRDT
snapshot, and a re-write is two-phase — append, confirm the append
persisted, then drop the superseded blocks — so a write that fails to
land leaves the page on its previous revision rather than blank, and
says so with `verified: false`.

**Filing a ticket from Zenith work.** The description IS the spec, and
the loop refuses anything it cannot route. Three sections:

```markdown
## Goal
<what outcome — quote the living docs rather than paraphrasing>

## Definition of Done
- [ ] <observable outcome>
- [ ] Baseline bumped in Status.md AND zm-tests.yml AND zagent.project.json

## Gates
<omit this — the category supplies the right gate list>
```

Always pass `--category`: `Zenithmon`, `Engine` or `DevilsPlayground`.
It selects the gate list, the conventions inlined into the worker's
prompt, the branching mode, **and now which board the ticket is filed
on**.

`create` also takes `--type EPIC|STORY|TASK|BUG|SUBTASK`, `--parent`,
`--points N`, `--severity S1_CRITICAL…`, `--repro ALWAYS…`,
`--environment "Null_vs2022_Debug_Win64_True"`, `--due`, `--sprint`,
`--fix-version`, `--blocks a,b` and `--blocked-by a,b`. Sizing is TWO
fields and they are not interchangeable: `--complexity`/`--risk` pick the
MODEL, `--points` sizes the SPRINT. A `ZEN` ticket without one comes back
`contractValid: false` rather than being guessed at, because guessing
between areas means ratcheting the wrong pin.

`create` now refuses at FILE time — you no longer find out hours later
when the loop reaches the card — but only when you file straight into
*Ready for Agent*, where the loop could claim it immediately. Filing
into *To Do* warns and lets you carry on drafting. It also refuses a
body carrying its own `## Gates`, because a ticket's gate list REPLACES
the category's rather than adding to it: pasting the Zenithmon list into
a ticket is how a pinned baseline goes stale, and `echo ok` in a body
would merge on `echo ok`.

**It also refuses a DoD that names a protected path.** `contractValid`
used to answer only "can this be ROUTED" — category, complexity, risk —
and said nothing about whether the work is inside what an agent bound by
`protectedPaths` can reach. Two consecutive tickets were valid and
impossible: one required a `.github/workflows/` file, the other a pin
bump in files the guard refuses. Each cost a claim, a slot in the
one-ticket-per-repo lock and a worker dispatch before anyone noticed.
The scan reads the **Definition of Done only** — the Goal may quote a
protected path as background without being refused — and reports the
path AND the pattern, so the fix is obvious: split the protected half
out as a human task.

Size it deliberately: `--complexity TRIVIAL|SIMPLE|MODERATE|COMPLEX`
picks the model, `--risk LOW|MEDIUM|HIGH` escalates it one tier. Risk
never blocks a merge — it buys more thinking.

**Windowed work gets `--label windowed`.** A headless run may CREATE a
`.zscen` but never CHANGE one, so a slice that re-authors a committed
scene cannot be done by the loop at all — and the units that would
notice are compiled constants that stay green, so the failure is a clean
gate run with the deliverable missing. The label is filtered out of the
claim query and fails the contract on a targeted claim, so the loop
cannot take it either way. Add `--assignee <email>` as well when you
want it to show up as someone's card; the label is what protects it.

This used to be `--assignee` alone, which worked only because the queue
skips assigned tickets — making the SAFE state the one nobody typed.
Now the marker is explicit, greppable, and enforced.

**Handing work over is a drag, not a command.** Nothing runs until a card
reaches *Ready for Agent* on the board. To run one immediately instead,
use `/tick` from a session started here — see **Running the loop** above.

**Exit codes are the interface** — branch on them, not on the text:
`0` ok · `3` nothing to claim · `4` contract invalid · `5` ownership
lost / repo busy · `6` circuit breaker open · `1` error · **`7` could not
REACH the board**. Note that `5` is what you get when this repo already
has a ticket in flight: one at a time, per repo, by design.

`7` is the client's own and is distinct from `1` deliberately: "the board
said no" and "I never reached the board" want opposite responses from an
unattended loop, and collapsing them is how a network blip gets written
into a ticket as a contract failure. On a `7`, change no status and
comment nothing.

**Setting a machine up** (once): `ZAGENT_URL` and `ZAGENT_TOKEN` as user
environment variables, and `Tools\zagent` on `PATH`. The token is minted
**on the board machine** — `zagent auth mint --name <this-machine>` —
shown exactly once, stored only as a SHA-256, and revocable per machine
without disturbing anything else. `Tools/zagent/README.md` has the
commands; `zagent doctor` names whichever piece is missing.

`packages/agent/CLAUDE.md` in the other repo is the full reference for
the dispatcher, `apps/jira/CLAUDE.md` for the HTTP surface.
