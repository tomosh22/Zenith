# Zenithmon -- Agent Briefing

**Document purpose:** The onboarding doc every agent reads at the start of every
Zenithmon session. Self-contained: if you have never touched this project, read
this and you will know how to act.

**Audience:** Future Claude Code instances (you, in two days, will not remember
this conversation).
**Status:** Living document -- update it whenever conventions evolve.

---

## 0. TL;DR

You are working on **Zenithmon**, a Pokemon Sword/Shield-class monster-collecting
RPG built entirely on Zenith systems (custom C++20 engine, Vulkan renderer, repo
root `C:\dev\Zenith`, game at `Games/Zenithmon/`). The user-approved plan is
locked; the stage plan (S0-S12) lives in [Roadmap.md](Roadmap.md).

### Reading order (every session)

1. **[Status.md](Status.md)** -- current build state, test pass-rate, in-flight task.
2. **[Roadmap.md](Roadmap.md)** -- the S0-S12 stage plan; pick the first un-checked task.
3. **This file** -- conventions, workflow, commands, operating rules.

Plus, always:

- **[Scope.md](Scope.md)** is **binding**. Read it before adding anything. If a
  feature is not in, it is out.
- **[DecisionLog.md](DecisionLog.md)** -- grep it **before re-deciding anything**.
  If a past session already decided the thing you are about to decide, either
  follow the decision or append a superseding entry explaining why.
- If you are orchestrating a multi-agent session, read
  **[OrchestratorPlaybook.md](OrchestratorPlaybook.md)** -- it is your primary
  operating manual; this file covers the conventions you and your subagents inherit.

### Never do

- **Ship Nintendo IP.** ~150 original species, original names everywhere
  (species/moves/abilities/towns). Mainline *mechanics* only.
- **Create a branch, PR, or worktree.** ALL work is committed DIRECTLY to
  `master` and pushed (user policy ZM-D-031); `git checkout -b`, `gh pr create`,
  and git worktrees are forbidden. The LOCAL gate (build + boot unit gate +
  headless) is the authority -- run it green before every `git push origin
  master`. `zm-tests` runs post-push as a backstop; fix forward on red.
- **Build the whole solution.** Always `/t:Zenithmon` (or `zenith build Zenithmon`).
  The aux tools in the sln are pre-existing-red in ToolsEnabled configs.
- **Run Sharpmake / `zenith regen` in a git worktree.** Generated vcxprojs bake
  the cwd's absolute path.
- **Commit baked assets.** Everything under `Assets/` that tools builds generate
  is git-ignored; keep it that way.
- **Run two MSBuilds concurrently.** mspdbsrv + output-dir locks force serial
  dispatch (this is about parallel *agents*/processes; a single build using
  `-maxCpuCount` is fine).
- **Skip the test-first step.** Tests specified in [TestPlan.md](TestPlan.md)
  land WITH the system, in the same commit.

---

## 1. The Project in One Page

- **Game:** ~150 original species, 18 types, 3-stage evolution lines; classic
  8-gym structure (home village -> professor -> starter -> 8 gyms -> League ->
  post-game with Champion rematch + Battle Tower); turn-based battles with the
  mainline formula set (type chart, physical/special split, stat stages, status,
  catching, abilities, natures, IVs/EVs, weather/terrain effects, breeding).
- **Hard exclusions** ([Scope.md](Scope.md) is authoritative): no audio (the
  engine has none), no networking/multiplayer/trading, no Dynamax-analog gimmick,
  battle format **singles only**.
- **Data tables are compiled `const` C arrays** in `Source/Data/*.cpp` -- not
  disk assets. Zero file I/O in headless logic tests. The "assets baked to disk"
  mandate covers meshes/textures/anims/terrains/scenes only.
- **All art is procedurally generated** by `#ifdef ZENITH_TOOLS` code and baked
  to `Assets/` (git-ignored), under manifest guards so warm boots skip the bake.
- **Historical S0 snapshot (2026-07-09, branch
  `zenithmon/s0-skeleton`):** project scaffolded
  via `zenith new Zenithmon`; `ZM_GameComponent` (registered `"ZM_Game"`,
  serialization order 100) bobs a placeholder cube under a "Zenithmon" title in
  the boot-authored `FrontEnd.zscen` (build index 0); 2 boot unit tests
  (`Tests/ZM_Tests_Boot.cpp`) + 1 automated test (`ZM_Boot_Test` in
  `Tests/ZM_AutoTests_Boot.cpp`); `Zenith_SaveData::Initialise("Zenithmon")` +
  `ClearForTest` between-tests hook wired from S0; `Vulkan_vs2022_Debug_Win64_True`
  build green; `zenith test Zenithmon --headless` = 1 passed / 0 failed;
  `.github/workflows/zm-tests.yml` written. At that snapshot, registering
  `zm-tests` as a required branch-protection check was still pending; it was
  completed on 2026-07-10 (see
  [ManualSetupChecklist.md](ManualSetupChecklist.md)). Current project state is
  maintained exclusively in [Status.md](Status.md).
- **S0 engine change worth knowing:** the game-name validators
  (`Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` and the C++
  `ZenithHub_GameScan::ValidateName`) previously reserved the blanket
  `Zenith*`/`Sentinel*` prefixes. Both were narrowed to a PascalCase word
  boundary (reject `Zenith`/`Sentinel` alone or followed by uppercase/digit;
  lowercase continuations like `Zenithmon` are distinct words and valid). The
  shared pinned vectors live in `Tools/ZenithCli/Tests/name_validation_cases.txt`;
  the buildsystem suite passed 45 / 0 after the change.
- **Current landmark (2026-07-16): S0-S4 COMPLETE; S5 (Battle integration slice)
  IN PROGRESS -- item 1 (Battle scene) DONE.** Shipped and gate-signed: S0 skeleton + CI, S1 data core, S2 headless
  battle engine (incl. feature-complete breeding/gender + Battle Tower), S3
  first-overworld (Dawnmere -> PlayerHome(build 40) warp round trip via the
  persistent manager/spawn/trigger orders 102-106 + `ZM_GreyboxVisual` 107 +
  the 0.20 s `WarpFade`; visual-signed), and **S4 asset generators** --
  `ZM_CreatureGen`(v3)/`ZM_CreatureAnimGen`/`ZM_HumanGen`(v1)/`ZM_BuildingGen`(v1)/
  `ZM_PropGen`(v1) + `ZM_BakeManifest`, all baked to git-ignored `Assets/` under a
  per-family manifest guard; the full-family `ZM_AssetGallery_Test` was visually
  signed off 2026-07-16 (ZM-D-088). **S5 item 1 (ZM-D-089): `ZM_BattleArena` (order 108)
  + the Battle scene (build index 1, world Y = -2000).** ECS component orders 100-108 are used;
  **next free is 109**. Boot unit gate **1924 / 0 failed**. The authoritative
  current stage, in-flight task, and exact numbers live in Status.md; this line
  is a coarse pointer. Do NOT begin a stage's next work past a VISUAL GATE
  without the user's sign-off (standing order). The S5 gate visual sign-off is a hard stop.

### Document map

| Doc | Kind | What it is |
|---|---|---|
| [Status.md](Status.md) | living, replaced each session end | build health, pass-rate, current task, notes for next agent. **Read first.** |
| [Roadmap.md](Roadmap.md) | living checklist | S0-S12 stage plan with per-stage task checkboxes and gate checklists. Source of truth for "what's next". |
| [DecisionLog.md](DecisionLog.md) | append-only, newest-first | every non-trivial decision: what / why / tests-that-lock-it / reversibility. |
| [Questions.md](Questions.md) | living | blockers logged with best-guess action + cost-if-wrong; agent proceeds, user resolves in batch. |
| [Shortfalls.md](Shortfalls.md) | living | honest gap audit vs the GDD; updated at stage gates. |
| [Scope.md](Scope.md) | **binding scope gate** | the locked in/out list. |
| [GameDesignDocument.md](GameDesignDocument.md) | reference | pillars, world/story, dex families, battle mechanics spec, progression. |
| [TestPlan.md](TestPlan.md) | reference | test tiers, naming, harness conventions, per-system test specs. |
| [SaveFormat.md](SaveFormat.md) | reference | versioned save schema + migration policy (fleshed out at S7). |
| [AssetManifest.md](AssetManifest.md) | reference | generated-asset catalogue + bake budgets + manifest-stamp scheme. |
| [BuildEnvironment.md](BuildEnvironment.md) | reference | setup, pinned versions, first build, triage. |
| [CIPolicy.md](CIPolicy.md) | living | required checks, branch protection, the zm-tests pipeline. |
| [Glossary.md](Glossary.md) | reference | game + engine term definitions. |
| [ManualSetupChecklist.md](ManualSetupChecklist.md) | one-time gate | human pre-flight steps (branch protection for zm-tests, etc.). |
| [StartPrompts.md](StartPrompts.md) | reference | copy-paste session-start prompts. |
| [OrchestratorPlaybook.md](OrchestratorPlaybook.md) | reference | multi-agent operating manual. Orchestrators read it instead of (in addition to) this one. |
| This file | living reference | general conventions inherited by orchestrator and subagents alike. |

---

## 2. The Session Loop

### 2.1 Start

```
1. cd C:\dev\Zenith; git checkout master; git pull   (ALL work happens on master -- ZM-D-031; NEVER branch/PR/worktree)
2. Read Docs/Status.md (the "Current task" line)
3. Read Docs/Questions.md (skim for open items the user may have answered)
4. Open Docs/Roadmap.md, find the first un-checked task in the current stage
5. If any .zproj or Sharpmake_*.cs changed since your last pull: Build\regen.ps1
```

### 2.2 Execute a task

1. **Read** the surrounding Roadmap tasks and the matching
   [TestPlan.md](TestPlan.md) / [GameDesignDocument.md](GameDesignDocument.md)
   sections for context.
2. **Write the tests first** (or alongside). The TestPlan entry for the system
   defines what must exist. Confirm new tests fail before the implementation.
3. **Implement** the smallest change that makes them pass.
4. **Run the LOCAL gate (the authority):** `zenith build Zenithmon`, then the
   boot unit gate (`pwsh -NoProfile -File Tools\run_unit_gate.ps1 -Exe <exe>
   -Baseline N` -- runs the ZM_* unit tests `zenith test` skips) and
   `zenith test Zenithmon --headless` -- all green.
5. **Update the docs in the same commit:** tick the [Roadmap.md](Roadmap.md) box,
   append to [DecisionLog.md](DecisionLog.md) if you decided anything
   non-trivial, refresh [Status.md](Status.md) at session end. Stage gates
   additionally require [Shortfalls.md](Shortfalls.md) + any affected format
   docs ([SaveFormat.md](SaveFormat.md), [AssetManifest.md](AssetManifest.md),
   [TestPlan.md](TestPlan.md)) updated in the same commit.
6. **Commit** Conventional-Commits style: `feat(zm): S1 - type chart table + matrix tests`.
   Types: `feat`, `fix`, `test`, `chore`, `docs`, `refactor`.
7. **Push DIRECTLY to master** (`git push origin master`) -- NO branch, NO PR
   (ZM-D-031). The local gate above is the pre-push authority; `zm-tests` runs
   post-push as a backstop -- fix forward on red (never revert shipped history,
   force-push master, or `gh run rerun`).

### 2.3 End

```
1. Update Docs/Status.md (replace, don't append; ~25 lines max)
2. Append to Docs/DecisionLog.md (one entry per non-trivial decision)
3. Append to Docs/Questions.md if you hit a blocker (best guess + proceed)
4. Print a one-paragraph session summary for the user
```

### Status.md format

```markdown
# Zenithmon Status

**Last updated:** YYYY-MM-DD
**Stage:** S<n>
**Build:** passing / broken (config + evidence)
**Tests:** N passed / M failed (zenith test Zenithmon --headless)

## Current task
<Roadmap item in flight>

## Last completed
<Roadmap item + commit hash>

## Notes for next agent
<anything not obvious from the diff; empty if nothing>
```

### DecisionLog.md entry format

```markdown
## YYYY-MM-DD -- <short title>

**Decision:** what was chosen (and the alternative considered).
**Why:** the load-bearing reason.
**Tests that lock it:** the test name(s) that fail if it regresses.
**Reversibility:** easy / moderate / hard, and where the change is localised.
```

### Questions.md entry format

```markdown
## Q-YYYY-MM-DD-NNN -- <question>

**Context:** why this came up; the options.
**Best guess if you don't reply:** what the agent will do meanwhile.
**Cost of getting it wrong:** low / moderate / high + a sentence.
**Status:** asked YYYY-MM-DD; acting on best guess.
```

---

## 3. Conventions

### 3.1 The ZM_ prefix

**ALL game code uses the `ZM_` prefix** (mirrors DP's `DP_` and CityBuilder's
`CB_`): components `ZM_Foo_Component` registered as `"ZM_Foo"`, pure logic
classes `ZM_BattleEngine`, data tables `ZM_SpeciesData`, unit-test TUs
`Tests/ZM_Tests_*.cpp`, automated-test TUs `Tests/ZM_AutoTests_*.cpp`, events
`ZM_OnWildEncounter`, debug variables `zm_*` (e.g. `zm_instant_battles`).
Component serialization orders: ZM components claim **100+** and remain unique:
`ZM_GameComponent` = 100, `ZM_TerrainGrass` = 101,
`ZM_PlayerController` = 102, `ZM_FollowCamera` = 103,
`ZM_GameStateManager` = 104, `ZM_SpawnPoint` = 105, and `ZM_WarpTrigger` =
106, and the replaceable blockout renderer `ZM_GreyboxVisual` = 107, and the
battle-arena manager `ZM_BattleArena` = 108; **next free is 109**.

### 3.2 Engine naming conventions (mandatory)

Scope prefixes, then type prefixes, on every variable:

| Scope prefix | Meaning | | Type prefix | Type |
|---|---|---|---|---|
| `m_` | member | | `x` | struct/class instance |
| `s_` | static member | | `p` | pointer |
| `g_` | global | | `px` | pointer to class/struct |
| `ls_` | local static | | `a` | array |
| | | | `u` | unsigned int |
| | | | `b` | bool |
| | | | `f` | float |
| | | | `str` | string |
| | | | `e` | enum value |
| | | | `i` | signed int |
| | | | `ul` | unsigned 64-bit |
| | | | `pfn` | function pointer |

Examples: `m_uSpeciesID`, `pxMonster`, `fDeltaTime`, `strName`, `eStatusKind`.

- **Functions:** `PascalCase` (`ResolveTurn`, `GetBaseStat`).
- **Enums:** `SCREAMING_SNAKE_CASE` with a category prefix
  (`ZM_MOVE_EFFECT_DRAIN`, `ZM_STATUS_BURN`).
- **Constants:** `SCREAMING_SNAKE_CASE` with a type prefix (`uZM_MAX_PARTY_SIZE`).

### 3.3 Code style

- **Tabs**, not spaces.
- **Braces on a new line** for classes, functions, control flow. Short inline
  one-liner getters in headers may use same-line braces.
- **`#pragma once`** in every header; no `using namespace` in headers.
- **Every `.cpp` starts with `#include "Zenith.h"`** (precompiled header).
- Class layout: `public:` first, then `protected:`, then `private:`.
- Tools-only code in `#ifdef ZENITH_TOOLS`; test code in
  `#ifdef ZENITH_INPUT_SIMULATOR` (automated tests) -- unit-test TUs use the
  `ZENITH_TEST` macro which is compiled under `ZENITH_TESTING`.

### 3.4 Containers and asserts

- **No `std::` containers in production code.** Use `Zenith_Vector`,
  `Zenith_HashMap`, `Zenith_Mutex`. `std::function` -> plain function pointers.
  (`Zenith_Vector` has no STL iterators -- index-based loops.)
- **`Zenith_Assert(cond, msg, ...)` (camelCase) is the real runtime assert.**
  The SCREAMING `ZENITH_ASSERT` is an empty marker macro and silently no-ops --
  do not use it expecting a check. (The `ZENITH_ASSERT_*` macros inside
  `ZENITH_TEST` bodies are separate test-framework asserts from
  `Core/Zenith_TestFramework.h` and are fine.)
- **No teleportation for movement** -- use physics (Jolt velocity), not
  `SetPosition`, even in tests. One-time placement at scene load (spawn points)
  is not teleportation.
- **No pimpl.** Forward-declare + by-ptr/ref, or move by-value state to the owner.

### 3.5 Logic placement rule (locked by the plan)

- **Pure headless C++** for everything rule-based: battle engine, stats, data
  tables, party/boxes/bag/dex, story flags, save schema, encounter rolls,
  trainer AI, breeding, Battle Tower. These carry the bulk of the unit tests
  and run with `--headless`.
- **ECS components** only for things touching transforms/physics/camera/input/
  scene lifetime.
- **Behaviour graphs** only for glue (menu flow, NPC scripted events, cutscene
  beats). **The battle turn loop is NOT a graph** -- it is a seeded,
  deterministic C++ state machine emitting a `ZM_BattleEvent` stream.

---

## 4. Workflow

### 4.1 TDD is non-negotiable

Tests specified in [TestPlan.md](TestPlan.md) **land WITH the system they test,
in the same commit**. No human reviews mid-session; the suite is the quality
contract. For every functional change: the test exists, fails before the
change, passes after, and the rest of the suite stays green. A commit that
changes behaviour without changing a test is self-rejected.

### 4.2 Master-only workflow (no branches, no PRs -- ZM-D-031)

- **All work is committed DIRECTLY to `master` and pushed** with
  `git push origin master`. Feature branches, pull requests, and git worktrees
  are FORBIDDEN (`git checkout -b`, `gh pr create`, worktrees). One commit per
  Roadmap task where practical.
- **The LOCAL gate is the pre-push authority** (build + boot unit gate +
  `zenith test --headless`, all green). `zm-tests` runs post-push on master as a
  BACKSTOP only (see [CIPolicy.md](CIPolicy.md)); you do not wait on it.
- **On a red post-push CI run: FIX FORWARD** with another direct commit to
  master. Never `git revert` shipped history, never `git push --force` master,
  never `gh run rerun`, never bypass hooks.

### 4.3 Stage gates

Each Roadmap stage ends with a gate. A gate = build green + unit tests green +
`zenith test Zenithmon --headless` green + the stage-scoped windowed
`--filter` runs + the stage's listed visual check + **docs updated**. The
4-config matrix (see 5.2) runs at major gates. The local gate is authoritative;
`zm-tests` runs after the direct-master push as a backstop and any later red run
is fixed forward. A stage
gate is not passed until [Roadmap.md](Roadmap.md), [Shortfalls.md](Shortfalls.md),
and any affected format docs are updated **in the same commit**.

### 4.4 Docs discipline

- [Status.md](Status.md): refreshed every session end (replace, don't append).
- [DecisionLog.md](DecisionLog.md): append-only, newest-first.
- [Roadmap.md](Roadmap.md): checkboxes ticked as commits land.
- [Shortfalls.md](Shortfalls.md) + format docs: at stage gates.
- Docs update lands **in the same commit** that changes the behavior it describes.

### 4.5 Direct-master pre-push checklist

1. Tests land with the system; failed before, pass after.
2. `zenith test Zenithmon --headless` fully green locally.
3. No new `std::` containers / `std::function`.
4. New `.cpp` files start with `#include "Zenith.h"`; headers `#pragma once`.
5. Tools code in `#ifdef ZENITH_TOOLS`; automated tests in
   `#ifdef ZENITH_INPUT_SIMULATOR`.
6. If files were added: `Build\regen.ps1` (or `zenith regen`) ran; no generated
   files committed.
7. No baked assets committed.
8. No scope creep (check [Scope.md](Scope.md)).
9. Roadmap box ticked; DecisionLog appended; Status refreshed.

---

## 5. Build & Test Commands

Run from the repo root `C:\dev\Zenith` in PowerShell.

### 5.1 The zenith CLI (preferred)

```
zenith build Zenithmon                      # msbuild the per-game sln, /t:Zenithmon
zenith run Zenithmon                        # launch the newest built exe
zenith test Zenithmon --headless            # full batch, headless
zenith test Zenithmon --filter ZM_Boot_Test # scoped (windowed unless --headless)
zenith regen                                # regenerate all solutions (after file adds / .zproj changes)
zenith clean Zenithmon                      # kill hung cl.exe/mspdbsrv + wipe output/obj
```

Fallback if `zenith` is not on PATH: `pwsh -File Tools\zenith.ps1 <verb> ...`
(the CLI implementation is `Tools/ZenithCli/ZenithCli.psm1` +
`ZenithTestHarness.psm1`).

**The old per-game `Tools/run_*_tests.ps1` scripts were DELETED at commit
`c29e28f8`.** There is no `run_zm_tests.ps1` and never will be -- every gate
uses `zenith test Zenithmon`.

Test harness flags: `--filter / --headless / --results-dir / --config /
--per-process / --fail-fast`. Exit codes: 0 OK / 1 usage / 2 validation /
3 generation / 4 build-or-test failure / 5 not-found.

### 5.2 Direct msbuild (what `zenith build` does)

```
msbuild Games\Zenithmon\zenithmon_win64.sln /t:Zenithmon /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64
Games\Zenithmon\Build\output\win64\vulkan_vs2022_debug_win64_true\zenithmon.exe
```

**Always `/t:Zenithmon`, never the whole solution** (aux tools in the sln are
pre-existing-red in ToolsEnabled configs).

**4-config matrix at major stage gates** (repo norm):

| Config | Purpose |
|---|---|
| `Vulkan_vs2022_Debug_Win64_True` | daily driver; tools + editor automation + tests |
| `Vulkan_vs2022_Debug_Win64_False` | runtime-only; loads baked scenes, skips ZENITH_TOOLS code |
| `Vulkan_vs2022_Release_Win64_True` | release + tools |
| `Vulkan_vs2022_Release_Win64_False` | release runtime |
| + `D3D12_vs2022_Debug_Win64_False` | null-backend **link proof** (Flux backend-neutrality) |

### 5.3 First-run scene caveat

`Assets/Scenes/FrontEnd.zscen` is **not checked in** -- it is authored on boot
by `Project_RegisterEditorAutomationSteps` (tools-only) and saved via
`AddStep_SaveScene`. Your **first build + run must be a `*_True` config** to
bake it; `_False` and CI runners load the baked file (or skip -- see 6.6).

---

## 6. Operating Rules

### 6.1 Regenerate-first

Everything Sharpmake emits is git-ignored (`.sln`, `.vcxproj`, generated `.cs`).
After a fresh clone, after any pull touching a `.zproj` or `Sharpmake_*.cs`,
and after adding/removing source files: run `Build\regen.ps1` (or
`zenith regen`). `zenith regen --check` reports staleness. Forgetting regen
after adding a `.cpp` means the file is not in the solution and the build/tests
fail with confusing "not found" symptoms.

### 6.2 Serial MSBuild dispatch

**Parallel agents thrash MSBuild** -- mspdbsrv + output-dir locks force one
build at a time. In multi-agent sessions only the orchestrator builds
(see [OrchestratorPlaybook.md](OrchestratorPlaybook.md) Invariant 1). If a
build fails with "cannot access file", run `zenith clean Zenithmon`.

### 6.3 Never run Sharpmake in a git worktree

Sharpmake bakes the cwd's absolute path into generated vcxprojs
(`GAME_ASSETS_DIR`, `SHADER_SOURCE_ROOT`, post-build copies). All work happens
on the main checkout at `C:\dev\Zenith`, directly on `master`. If the harness
drops you in `.claude/worktrees/<name>/`, treat it as a transient sandbox and
never regen or commit generated output from there.

### 6.4 Engine changes

Engine work is in-scope where Zenith is missing/incomplete/buggy (the plan
pre-approves E1-E5: terrain asset-set names, rect chunk export, UIText
typewriter reveal, UIGridLayoutGroup, grass singleton reset hygiene). Every
engine change lands with:

1. **Unit tests** for the new surface.
2. **RenderTest boot regression** (RenderTest still boots green -- it is the
   terrain/rendering canary).
3. **DP + CityBuilder suites stay green** (engine-wide safety net).
4. A [DecisionLog.md](DecisionLog.md) entry for any new engine surface.

### 6.5 Baked assets are never committed

`Assets/` output (meshes, textures, anims, terrains, scenes, graphs) is
git-ignored, regenerated by `*_True` builds under manifest guards
(generator-version stamp + file-existence -- see
[AssetManifest.md](AssetManifest.md)). Bake determinism (same seed ->
byte-identical output) is a tested invariant from S4 on.

### 6.6 Asset-dependent tests RequestSkip when assets are absent

CI checkouts have **no `Assets/`** (git-ignored). Every automated test that
needs a baked asset or scene must exists-guard and call
`RequestSkip(szReason)` (`Zenith_AutomatedTest.h`) instead of failing --
the established CI-fix pattern (engine commit `94813489`). Headless runs skip
Flux entirely, so graphics-dependent tests must set `m_bRequiresGraphics = true`
(auto-skipped headless).

### 6.7 Test-harness footguns (permanent)

- Automated-test `Step` runs **inside** the main loop: use **input-simulator
  state-setters only** (`SimulateMousePosition/ButtonDown/Up/KeyPress/
  SetKeyHeld/Wheel`). Reentrant calls (`StepFrame`, `SimulateMouseClick`)
  deadlock in `vkWaitForFences`.
- Between batched tests the harness reloads scene 0 and fires the hooks
  registered via `RegisterBetweenTestsHook` (Zenithmon's hook lives in
  `Zenithmon.cpp` and resets `Zenith_SaveData::ClearForTest`; extend it as
  ownerless game globals appear).
- MSVC dead-strips static registrars in unreferenced static-lib objects.
  Zenithmon's `Tests/*.cpp` compile directly into the game exe, which is safe;
  keep it that way.
- `static const Zenith_AutomatedTest` lands in read-only memory -- never
  `const_cast` and write back to the test struct.

---

## 7. Worked Example -- Shipped ZM Component Registration Pattern

The shipped `ZM_WarpTrigger` shows the end-to-end pattern for any future
component:

1. **Header in `Components/`** -- `Components/ZM_WarpTrigger.h`,
   `#pragma once`, class `ZM_WarpTrigger` with a `Zenith_Entity&`
   constructor storing `m_xParentEntity`. Lifecycle hooks (`OnAwake`, `OnStart`,
   `OnUpdate`) are concept-detected by the component-meta registry -- there is
   no base class. Implement the component contract: `WriteToDataStream` /
   `ReadFromDataStream`, plus `RenderPropertiesPanel` under `#ifdef ZENITH_TOOLS`.
2. **Register in `Zenithmon.cpp`** -- `#include` the header and add the
   file-scope `ZENITH_REGISTER_COMPONENT(ZM_WarpTrigger, "ZM_WarpTrigger", 106u)`
   next to the existing registrations (106 is this component's locked order;
   order 107 is now `ZM_GreyboxVisual`, so the next free order is 108). The
   macro must be static-init in an always-linked TU --
   `Zenithmon.cpp` defines the `Project_*` entry points, so it is safe. Do NOT
   call it from `Project_RegisterGameComponents` (the meta registry is sealed
   before that hook runs).
3. **Editor-registry mirror** -- in `Project_RegisterGameComponents()` under
   `#ifdef ZENITH_TOOLS`:
   `Zenith_ComponentEditorRegistry::Get().RegisterComponent<ZM_WarpTrigger>("ZM_WarpTrigger");`
   (this populates the editor's Add Component menu).
4. **Regen** -- `zenith regen` (new file => solution regeneration; generated
   output stays untracked).
5. **Test TU in `Tests/`** -- the shipped traversal units live in
   `Tests/ZM_Tests_WorldTraversal.cpp` under `ZENITH_TEST(ZM_WorldTraversal,
   <Name>)`; `Tests/ZM_AutoTests_WorldTraversal.cpp` is wrapped in
   `#ifdef ZENITH_INPUT_SIMULATOR` with `static const Zenith_AutomatedTest` +
   `ZENITH_AUTOMATED_TEST_REGISTER` (Setup/Step/Verify + maxFrames; see
   `Tests/ZM_AutoTests_Boot.cpp` for the exact shape). Update
   [TestPlan.md](TestPlan.md) if the test spec is new.
6. **Wire into a scene** -- via automation authoring
   (`AddStep_AddComponent("ZM_WarpTrigger")` in the scene recipe), then run a
   `*_True` build once to re-bake the affected `.zscen`. Dawnmere and PlayerHome
   now contain the first live trigger pair; future edges follow the same pattern.
7. **Gate** -- `zenith build Zenithmon`, then
   `zenith test Zenithmon --headless` (all green), plus a `--filter` windowed
   run if the new test needs graphics.
8. **Land** -- include the component + test + docs updates in one commit, then
   push it directly to `master`; tick the Roadmap box.

---

## 8. The "Done" Test

You finish a session well when:

- The current Roadmap task is one box closer to ticked.
- The authoritative local gate was green before the direct-master push;
  `zm-tests` is the post-push backstop and any red run is fixed forward.
- [Status.md](Status.md), [DecisionLog.md](DecisionLog.md),
  [Questions.md](Questions.md) reflect reality.
- The next agent, reading [Status.md](Status.md) cold, can resume in under
  5 minutes.

If any of these are false, finish them before you stop.

## 9. Session bootstrap mechanics (verified gotchas -- survive Status.md replacement)

These are permanent operational facts, each paid for once. Do not re-discover
them.

- **The program plan lives in the repo:** [MasterPlan.md](MasterPlan.md) is the
  user-approved plan (engine-fact survey, per-stage detail, rationale, risks).
  Roadmap.md supersedes its stage/status wording; MasterPlan supplies the WHY.
- **gh auth:** sandboxed sessions have no `gh auth login`, but the GitHub
  credential lives in the git credential manager. Call gh through
  `pwsh -NoProfile -File Tools\zenith_gh.ps1 <gh args...>` -- it derives
  GH_TOKEN via `git credential fill` when needed and forwards verbatim. (Raw
  inline bootstrap makes compound commands that permission allow-rules cannot
  prefix-match; the wrapper exists precisely for that.)
- **Shell forms:** in sandboxed agent sessions `zenith.bat` (the PowerShell 5.1
  shim) can fail on a Get-FileHash resolution quirk. Use the pwsh forms:
  `pwsh -NoProfile -File Tools\zenith.ps1 <cmd>` and
  `pwsh -NoProfile -File Build\regen.ps1`. CI and interactive user machines are
  unaffected either way.
- **Post-push CI re-evaluation:** never use `gh run rerun`. A red `zm-tests`
  run is corrected by a new direct commit to `master`; that push produces a
  fresh run for the new commit (ZM-D-031).
- **Never launch a game exe bare with `--exit-after-frames N`:** that flag is a
  per-TEST max-frames override, consumed only while an automated test runs; a
  bare tools build idles in the editor FOREVER (orphaned processes). Boot
  checks go through `zenith test Zenithmon --filter <Test>` (harness-managed
  exit) or `--list-automated-tests` (exits by itself), or pair the flag with
  `--automated-test <name>`. Sweep strays with `Get-Process zenithmon`.
- **Windowed screenshot evidence** (visual gates): run the game windowed via a
  harness-managed `--filter` test or `zenith run`, then capture with the
  SetWindowPos + CopyFromScreen recipe (see the RenderTest capture notes
  referenced from MasterPlan.md's verification section). Store captures under
  `Build/artifacts/` (never committed) and record paths in Status.md.
- **Unattended permission surface:** the checked-in `.claude/settings.json`
  allowlists git, GitHub inspection, the zenith/regen/gate/scaffold script
  entry points, and msbuild, so loop iterations do not stall on prompts
  (approved 2026-07-10, DecisionLog ZM-D-017). Some legacy `gh pr` permission
  entries remain, but permission is not authorization: ZM-D-031 forbids agents
  from creating PRs. If you add a new routinely-used command, extend that
  allowlist in the same commit that introduces the command.
