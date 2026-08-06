# The Zenith Build System

The single comprehensive reference for how Zenith is generated, built, tested,
packaged, and gated. Companion documents:

- **[GameProjects.md](GameProjects.md)** — the `.zproj` descriptor schema, codegen,
  manifest guard, solution inventory, templates, and the hub. Read that for
  "how do I add/describe a game"; read this for everything else.
- **`Tools/ZenithCli/CLAUDE.md`** — CLI module internals (for working ON the CLI).
- Root `CLAUDE.md` — the quick-start subset of this document.

Sharpmake (checked in at `../Sharpmake`) is the generator. **CMake is not used
and must not be introduced.**

## 1. The moving parts

```
Games/<Name>/<Name>.zproj          descriptors (the ONLY per-game build input)
        |
        v
Build/regen.ps1                    canonical regeneration (zenith regen forwards here)
  ├─ Build/zenith_buildsystem.psm1 scan/validate/codegen/name-rules/shared ops
  ├─ Build/Sharpmake_*.cs          hand-written projects + generated per-game shells
  └─ Build/fix_agde_vcxproj.ps1    AGDE post-fixup (clang c++ standard token)
        |
        v
Games/<Name>/<name>_win64.sln      per-game solutions (+ _agde.sln when android:true)
Build/zenith_engine_win64.sln      engine-only solution (zero games)
        |
        v
zenith build / test / run / package / clean      (zenith.bat -> Tools/zenith.ps1
                                                  -> Tools/ZenithCli/ZenithCli.psm1)
```

Central configuration data lives in **`Build/zenith_config.psd1`** (default and
hub configs, Slang/Vulkan SDK versions CI provisions, the artifact root) and is
read ONLY via accessors in `zenith_buildsystem.psm1` (`Get-ZenithBuildConfigData`,
`Get-ZenithDefaultConfig`, `ConvertTo-ZenithOutputDir`, `Get-ZenithGameExePath`).
Never hardcode a config name or exe path in a script — call the accessor.

## 2. The `zenith` CLI — command reference

`zenith.bat` (repo root) → `Tools/zenith.ps1` → `Tools/ZenithCli/ZenithCli.psm1`.
This is THE entry point for every build-system operation; there are no
alternative/forwarder scripts (no-legacy rule, §11).

```
zenith new <Name> [--template <T>] [--no-android] [--no-open]
zenith open <Name>
zenith list [--json]
zenith regen [--check]
zenith build <Name|engine> [--config <C>] [--headless] [--timeout <min>]
zenith run <Name> [--config <C>] [--build] [-- <game args>]
zenith test <Name|all> [--filter X] [--tier N] [--config <C>] [--headless]
            [--per-process] [--fail-fast] [--build] [--results-dir D]
            [--tests A,B,C] [--batch-order reverse|rotate:<N>]
            [--exit-after-frames N] [--assertions-log F]
zenith clean [<Name>|engine|all] [--processes-only] [--dry-run]
zenith package <Name> [--config <C>] [--out <D>] [--force] [--no-shaders]
zenith hub [--rebuild]
zenith selftest
```

**Exit codes (all commands):** `0` ok · `1` usage · `2` validation · `3`
generation/drift · `4` build/test failure · `5` not found.

Command notes beyond the obvious:

- **`build`** resolves MSBuild via PATH then vswhere (Insiders/prerelease
  included); kills compiler processes older than 30 minutes on entry (hung-lock
  self-heal — a live concurrent build is never touched); `--timeout` arms a
  watchdog that taskkills the msbuild tree. `build engine` builds `/t:Zenith`
  in the requested config plus the three Sentinels in its `_False` sibling
  config (Sentinels exist only in `ToolsEnabled=False`; see §4).
- **`regen --check`** is a read-only staleness report: recomputes the codegen
  text from the descriptors and byte-compares against the on-disk generated
  `.cs`, then verifies every expected `.sln` exists. Exit 3 = run `zenith regen`.
  This is a LOCAL tool — CI always runs a full regen (a fresh checkout has no
  generated files, so a check-only step would always report drift).
- **`test`** — see §5.
- **`clean`** always sweeps hanging `cl`/`mspdbsrv`/`link`/`vctip`/`msbuild`
  first, then (with a target) deletes that target's `output/` + `obj/` trees.
- **`package`** — see §7.
- **`selftest`** runs `Build/Tests/run_buildsystem_tests.ps1` +
  `Tools/ZenithCli/Tests/run_cli_tests.ps1` (77 tests; the buildsystem suite
  includes the tracking-policy regression tests from §6).

## 3. Regeneration (regenerate-first)

`Build/regen.ps1` does, in order: worktree refusal → descriptor validation
(ALL errors collected) → codegen `Sharpmake_GameInstances.generated.cs` → one
Sharpmake run over the `Build/Sharpmake_*.cs` glob → AGDE vcxproj fixup →
obsolete-monolith deletion → **orphan prune** (`Remove-ZenithOrphanGameArtifacts`:
generated slns/vcxprojs of game dirs with no descriptor, and stale agde
artifacts of `android:false` games — never sources, never directories) → sln
inventory print. Details of validation/codegen/manifest-guard:
[GameProjects.md](GameProjects.md).

**Regenerate-first policy: nothing Sharpmake emits is git-tracked** — every
`.sln`, `.vcxproj`, `.filters`, `.vcxproj.user`, and the generated `.cs`.
Consequences:

- After a fresh clone, or any checkout/pull touching a `.zproj` or
  `Sharpmake_*.cs`: run `zenith regen` before building. `zenith regen --check`
  tells you whether you need to.
- Branch switches never produce generated-file churn in `git status`.

**Worktree ban:** Sharpmake bakes ABSOLUTE paths (asset dirs, shader root,
include paths) resolved from its own location, so running it from a linked git
worktree generates projects that point at the wrong tree. `regen.ps1` refuses
(exit 2). Run regen from the main checkout only.

## 4. Configurations, outputs, and the library structure

Config axes (fragments): **RenderBackend** (`Vulkan_` = real renderer /
`Null_` = the GPU-less backend every headless run executes on / `D3D12_` = a
reserved no-op backend kept as the link-neutrality proof) × **vs2022** ×
**Optimization** (`Debug`/`Release`) × **Win64/Agde** × **ToolsEnabled**
(`True` = editor/tools, `False` = runtime-only). Example:
`Vulkan_vs2022_Debug_Win64_True`.

**agde also carries the `Vulkan_` prefix, plus an ABI fragment.** agde is
Vulkan-only, but Sharpmake emits the backend fragment anyway because it is
multi-valued across the target set as a whole, and it adds the Android ABI:

| agde config | MSBuild Platform |
|-------------|------------------|
| `Vulkan_arm64_v8a_vs2022_Debug_Agde_False` | `Android-arm64-v8a` |
| `Vulkan_arm64_v8a_vs2022_Release_Agde_False` | `Android-arm64-v8a` |
| `Vulkan_x86_64_vs2022_Debug_Agde_False` | `Android-x86_64` |
| `Vulkan_x86_64_vs2022_Release_Agde_False` | `Android-x86_64` |

The ABI axis is `ZenithAndroidAbi.All` in `Sharpmake_Common.cs` — one list every
agde project reads. It has **three** mirrors, because C#, Gradle and PowerShell
cannot share a literal, and widening the axis means editing all three:

| Mirror | Read by | Orientation |
|---|---|---|
| `ZenithAndroidAbi` (`Sharpmake_Common.cs`) | every agde project | `ConfigToken()` / `DirName()` |
| `Build/zenith_android_abis.gradle` | every game's `app/build.gradle` | dir name → token |
| `AndroidAbis` (`Build/zenith_config.psd1`) | all PowerShell tooling, via `Get-ZenithAndroidAbis` | token → dir name (the **transpose** of the Gradle map) |

A buildsystem selftest compares all three and fails on drift; read the psd1 one
through the accessor, which returns both spellings as named fields.
`arm64-v8a` is what physical devices run; **`x86_64` is what the Android
emulator runs**, because the QEMU2 emulator cannot host an arm64 guest on an
x86_64 host at all. A dev box with no ARM device can only exercise Android
through x86_64. Gradle merges every built ABI into one APK, so an APK contains
whichever ABIs you actually built.

Mind the two spellings: the config name uses `arm64_v8a` (underscores) while the
on-disk ABI directory is `arm64-v8a` (dash). They coincide for `x86_64`, which
is exactly what makes hardcoding either spelling silently wrong — see
`ZenithAndroidAbi.ConfigToken` vs `.DirName`.

> **Headless is a build config, not a flag.** There is no `--headless` engine
> flag. A `Null_*` config defines `ZENITH_NULL_RENDERER`, compiles `Zenith/Null`
> in place of the Vulkan backend, and creates the window hidden. Crucially every
> render path still RUNS (pass callbacks, buffer uploads, the editor ImGui frame)
> against no-op backend calls, so a headless run exercises the same code as a
> windowed one instead of skipping it behind a runtime branch. `--headless` on
> `zenith build` / `zenith test` selects this config
> (`zenith_config.psd1: HeadlessConfigWin64`). In C++, query it with the
> constexpr `Zenith_IsNullRenderer()`.

**Case rule:** `/p:Configuration=` takes the PascalCase name; Sharpmake
LOWERCASES it to form the output directory leaf. The one place this fact lives
in code is `ConvertTo-ZenithOutputDir`; never `.ToLowerInvariant()` a config
name at a call site.

Layout:

| What | Where |
|------|-------|
| Game exe | `Games/<Name>/Build/output/win64/<lowercase config>/<name>.exe` |
| Game intermediates | `Games/<Name>/Build/obj/…` |
| Engine libs | `Build/output/win64/<lowercase config>/*.lib` |
| Engine/leaf intermediates | `Build/obj/<Lib>/…` (distinct per lib — PCH collision guard) |

**Library structure:** `ZenithBase` (L0: maths/collections/streams/file/memory/
threading) ← `ZenithECS` (L1) ← `ZenithPhysics` ← `ZenithAI` ← `Zenith`
(aggregate: Flux, EntityComponent, Editor, everything else). Each lib compiles
its own `Zenith.pch` (a binary PCH cannot be shared across projects with
different defines). The lockstep rule: `ZENITH_TOOLS`,
`ZENITH_PROFILING_ENABLED`, `ZENITH_MEMORY_TRACKING_LEVEL` must be identical
across the base lib and every consumer (ODR).

**Sentinels** (`SentinelECS`/`SentinelPhysics`/`SentinelAI`,
`Tests/Sentinel*/`): leaf-purity proof exes that link EXACTLY the leaf libs —
an accidental leaf→engine edge fails their link with an unresolved external.
They exist only in `ToolsEnabled=False` configs. The known, deliberate platform
seams of the leaf libs (file access, debug break, the non-profiling mutex, the
unit-test registrar, callstack capture, the profiler's string-zone markers) are
supplied by each sentinel's `sentinel_platform.cpp` — extend THAT file when a
leaf lib grows a new sanctioned seam; anything else unresolved is a real leak.
CI builds AND runs all three (engine-gate, §8).

## 5. Testing

**Engine automated-test protocol** (any game exe): `--list-automated-tests`
(enumeration), `--all-automated-tests` (batch: one process runs every registered
test) or `--automated-test <Name>` (single), `--test-results-dir <D>` /
`--test-results <file>` (per-test JSON:
`passed`/`skipped`/`failures`/`frames`/`durationMs`), `--exit-after-frames N`,
`--fixed-dt`, `--skip-tool-exports`, `--skip-unit-tests`.

There is no `--headless` engine flag: run the game's **Null build** instead
(§4). Tests tagged `m_bRequiresGraphics=true` — the ones that READ PIXELS — are
skipped-as-passed there. **Discovery always uses the Null exe**, in every mode:
a Vulkan `--list-automated-tests` hangs in `vkEnumeratePhysicalDevices` on a
GPU-less runner, so `zenith test` errors with "build the Null config first" if
it is missing.

> **`--exit-after-frames` only applies while the test harness is stepping.** A
> boot with no `--automated-test` / `--all-automated-tests` never exits on its
> own; `Tools/run_unit_gate.ps1` relies on its watchdog kill, which fires AFTER
> the "Unit tests complete" line. A timeout there is expected, not a failure.

**`zenith test`** is the only test entry point, backed by
`Tools/ZenithCli/ZenithTestHarness.psm1` (`Invoke-ZenithGameTests`): pre-run
JSON wipe (stale results must never mask a regression), runtime-DLL self-heal,
discovery, dispatch, tally (a non-zero ENGINE exit fails the batch even if
every JSON says passed — crash-mid-suite guard), and a slowest-tests report.

- **Batch vs per-process:** batch by default; `--per-process`, `--fail-fast`,
  or a non-empty `--filter` forces one process per test (the engine's batch
  flag has no name filter, and batch ignores individual outcomes so fail-fast
  needs per-process).
- `--tier N` filters by the DP naming convention (`Test_T0*` / `Test_P<N>*`)
  BEFORE dispatch (does not force per-process).
- **Order control (cross-test-leak diagnosis).** `--tests A,B,C` runs exactly
  those tests, in that order, in ONE process (engine: `--automated-tests`) —
  the predecessor→victim probe for "does A contaminate B?". Duplicates are
  preserved on purpose (`A,A` probes self-contamination); note both
  occurrences write the same `<name>.json`, last-wins. Manual-only tests run
  when named explicitly. `--batch-order reverse|rotate:<N>` reorders the full
  batch without changing its set (`rotate:<N>` is normalised modulo the suite
  size, so `rotate:0` is the default order). Neither composes with the other,
  nor with `--filter`/`--tier`/`--per-process`/`--fail-fast`; conflicts are a
  usage error (CLI exit 1, engine exit 2). An unknown name in `--tests`
  reports EVERY unknown and runs nothing.
- **Between-test isolation.** Every test — batch member, batch-first, and
  single-test — runs against a world the ENGINE built. Before each test the
  harness normalises input + fixed-dt, calls
  `Zenith_SceneSystem::ResetWorldForNextTest()` (destroying **every** scene,
  including the persistent/DontDestroyOnLoad one), runs hygiene for the few
  things no scene owns (instrumentation logs, deferred-event queue, save
  sandbox, editor session state), reloads the boot scene, and settles. A test
  needs cleanup only for state no lifecycle owns — use `m_pfnTeardown`.
- **Infrastructure failures are exit code 3, not 1.** If the harness cannot
  build a clean world (boot scene will not load, post-reset settle times out)
  the remaining tests are *not run*: the engine writes
  `<results dir>/_infrastructure.json` (`phase` / `reason` / `beforeTest`) plus
  a `skipReason: "infrastructure"` record per unrun test, and the runner reports
  one infrastructure fault instead of a wall of MISSING lines. An
  infra-skipped test counts as neither passed nor failed.
- **Save sandbox.** Automated-test runs never touch real save data. The runner
  creates `Build/artifacts/savedata/<run-id>/`, writes an ownership marker, and
  passes `--test-save-root` + `--test-save-run-id`; `Zenith_SaveData` accepts a
  supplied root only if the marker's run-id matches, else falls back to
  `<exe dir>/TestSaveData/<game>/<run-id>/`. Between tests it deletes `*.zsave`
  only, non-recursively, only in the marker-bearing directory. The runner drops
  the sandbox on success and **keeps it on failure** for triage.
- Frame ceiling defaults to 8500 — a runaway backstop covering the slowest
  known suite; each test's own `m_iMaxFrames` governs its budget.
- Results default to `Build/artifacts/test_results/<game>/` (gitignored).
- Current suite baselines: **CityBuilder 45**, **DevilsPlayground 158**.

**Engine unit tests** run at every boot unless `--skip-unit-tests`. Baseline:
**1344 ran, 0 failed** (observed 2026-08-06 on a `Null_` Combat build) — the
authority is the `-Baseline` default in `Tools/run_unit_gate.ps1`, not this
number; read it there if the two ever disagree. Asserted by that script (CI, which
`Tools/test_scaffold.ps1` also invokes for the scaffold gate), tolerating
exactly one known layout-sensitive skip
(`GraphComponent::RegistryWideNodeRoundTrip`). **When you add engine unit
tests, bump `-Baseline` in `Tools/run_unit_gate.ps1` in the same change** —
and the ZM override in `.github/workflows/zm-tests.yml`, which counts the
engine suite plus ZM's own. Combat carries the CI unit gate
(`engine-gate.yml`).

**PowerShell selftests** (`zenith selftest`): dependency-free assert-runners
covering name validation (shared vector file pinning PS↔C++ hub), the
descriptor validation matrix, codegen golden file, drift detection, process
sweep, DLL heal, orphan prune, the tracking-policy invariants, template
expansion, dispatcher exit codes, and the test-harness parsers/tally.

## 6. Artifacts and the tracking policy

- **`Build/artifacts/`** (gitignored) is the canonical root for EVERYTHING a
  runner/tool emits: `test_results/<game>/`, `telemetry/`, ad-hoc logs. Never
  invent a new ad-hoc `Build/<thing>_results` dir.
- **`dist/`** (gitignored) holds `zenith package` output.
- Tracked under `Build/`: ONLY hand-written files (`Sharpmake_*.cs`, scripts,
  `zenith_config.psd1`, `zenith_android_abis.gradle` — the shared Android ABI
  axis every game's `app/build.gradle` applies — `Templates/`, `Tests/`,
  `TestData/`) — ~39 files.
- Two regression tests in the buildsystem selftest lock this forever: no
  generated/transient pattern may appear in `git ls-files`, and no hand-written
  build file may be gitignored.

## 7. Packaging (relocatable builds)

`zenith package <Name>` stages exe + every runtime DLL (after the shared DLL
heal) + `Games/<Name>/Assets` + `Games/<Name>/Config` (if present) +
`Zenith/Assets` + `Zenith/Flux/Shaders` (skippable via `--no-shaders`) into
`dist/<Name>_<lowercase config>/`, plus a `run.bat` that launches with
`--assets-root "%~dp0"`.

**How relocation works:** the compile-time `GAME_ASSETS_DIR` /
`ENGINE_ASSETS_DIR` / `SHADER_SOURCE_ROOT` defines are absolute build-machine
paths. `Zenith_CommandLine::ResolveUnderAssetsRoot(baked, override, rel)`
(Core, L0 — deliberately NOT in AssetHandling so Flux can call it) substitutes
`<override>/<repo-relative layout>` when `--assets-root` is present and passes
the baked path through untouched otherwise. Consumers: the two asset dirs
(`Zenith_Engine::InitialiseAssets`), the Slang session search paths, Flux's
shader-root search path, and the shader hot-reload watcher.

**Residual limitation** (stated in each package's generated README): game code
that string-bakes `GAME_ASSETS_DIR` into compile-time literals (e.g. scene
build-index registration) bypasses runtime resolution and still points at the
build machine. Fix is a per-game sweep to runtime-resolved paths — never a
runtime alias/remap (§11).

**Verification recipe:** package Combat, copy `dist/...` elsewhere, `run.bat`
— windowed must render (shaders compile from the package tree); hide the
package's `Zenith/Flux/Shaders` and it must FAIL compile against only the
package path (proves no fallback to the baked path).

## 8. CI

All workflows run on `windows-latest`, regen with `Build/regen.ps1 -UseDotnet`
before building, and build with explicit `/t:` targets. Heavy workflows share
`.github/actions/zenith-setup` (composite action): setup-msbuild, vcpkg with
ONE shared cache key, Vulkan SDK cache+install, Slang release cache (105 MB,
previously downloaded every run), all downloads retried with backoff.
Concurrency groups cancel superseded PR runs (master pushes always complete).

| Workflow | Gates |
|----------|-------|
| `cb-tests` | CityBuilder Vulkan `_True` compile proof + **Null `_True` build (the exe every step runs)** + D3D12 `_False` link proof + 45-test headless suite via `zenith test` |
| `dp-tests` | Same shape for DevilsPlayground (158 tests) |
| `engine-gate` | Sentinels (`Vulkan_Debug_Win64_False`) built AND executed + Combat unit gate (`Tools/run_unit_gate.ps1`; it passes no `-Baseline`, so the script's default is the pin — 1344 as of 2026-08-06 — known flake tolerated). Rollout: dispatch → burn-in → required |
| `release-build` | NIGHTLY (not PR-blocking): engine + DP in `Vulkan_vs2022_Release_Win64_True`, build-only — the only Release compile in CI |
| `shader-validation` | FluxCompiler (Release `_True`) catalog/parity/spine-lint + git-status drift gate on shader outputs |
| `scaffold-smoke` | Path-filtered end-to-end `zenith new` → build → boot (units baseline) → teardown leaves git status identical |
| `complexity` / `layering-gate` | `analyze_code_complexity.py` (engine-ci profile; pip cached via `Tools/requirements-ci.txt`). Ratchets: new architecture/lint findings must be FIXED, not allowlisted |
| `memory-gate` | Memory-budget baseline JSON ratchet (stdlib-only) |
| `doc-lint` | 6 consistency checks over `Games/DevilsPlayground/Docs/` |

Notes:

- CI compiles/links against the VENDORED `Middleware/VulkanSDK/<ver>`; the
  installed SDK only supplies `vulkan-1.dll` + the loader on PATH. The
  composite action has a `vulkan-provision: runtime` mode (~2 MB loader zip
  instead of the multi-GB SDK) — flip the default only after one green
  `workflow_dispatch` per heavy workflow verifies the LunarG zip layout.
- Android/AGDE is deliberately NOT in CI (AGDE VSIX absent from hosted
  runners); descriptor-level AGDE generation is exercised by every regen.
  Revisit with a self-hosted runner.
  **That gap has teeth.** Because nothing compiles the Android tree on a push,
  it bit-rots silently: an Android bring-up on 2026-08-03 found the agde build
  broken by MSVC-only `fopen_s`/`tmpfile_s` calls added the SAME day, plus a
  use-after-free in the Android thread bootstrap and several
  Windows-only-contract unit tests. All of it would have been a one-line CI
  failure. Until a runner exists, **build at least one agde config by hand
  before trusting the Android target**:
  ```
  msbuild Games\TilePuzzle\tilepuzzle_agde.sln /t:TilePuzzle ^
    /p:Configuration=Vulkan_x86_64_vs2022_Debug_Agde_False /p:Platform=Android-x86_64
  ```
  x86_64 is the cheap one to check because it also RUNS, on the local emulator.

## 9. Build hygiene and troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| `MSB3027` "could not copy .pdb" / file-in-use | Hanging `cl.exe`/`mspdbsrv.exe` from an interrupted parallel build → `zenith clean` (or just retry `zenith build`, which self-heals stale compilers ≥30 min old). Don't fall back to single-threaded builds. |
| `.sln`/`.vcxproj` missing (fresh clone / branch switch) | Regenerate-first policy → `zenith regen` (`--check` to ask first). |
| MSBuild not found | Install VS2022 C++ workload or run from a Developer PowerShell; the CLI resolves via PATH then vswhere. |
| Exe dies at launch, `STATUS_DLL_NOT_FOUND` (0xC0000135) | Slang dependency tree / assimp missing next to the exe → `zenith test`/`zenith package` self-heal via `Repair-ZenithRuntimeDlls`; for a bare `zenith run`, build once via a game that has them or run `zenith test <Name> --filter <anything>` to trigger the heal. |
| Game hangs at shutdown (headless) | Known; harness/gates tally results from JSON and watchdog-kill. Don't wait on process exit for correctness. |
| Whole-solution build red | Aux tools (FluxCompiler `_True` variants / TilePuzzle tools) are pre-existing-red in `ToolsEnabled=True` — ALWAYS build `/t:<target>`; `zenith build` does. |
| Regen refuses to run | You're in a linked git worktree — run from the main checkout (§3). |
| Sentinel link error | A leaf lib grew an engine/platform reference. If it's a sanctioned L0 seam, add the shim to `Tests/Sentinel*/sentinel_platform.cpp`; otherwise fix the leak (that's the point of the proof). |

## 10. Key defines (set by Sharpmake)

`ZENITH_TOOLS` (True configs), `ZENITH_WINDOWS`/`ZENITH_ANDROID`,
`ZENITH_DEBUG`, `ZENITH_VULKAN`/`ZENITH_D3D12` (exactly one),
`ZENITH_PROFILING_ENABLED`, `ZENITH_MEMORY_TRACKING_LEVEL` (2 Debug / 1
Release), `GAME_ASSETS_DIR`/`ENGINE_ASSETS_DIR`/`SHADER_SOURCE_ROOT` (absolute
baked paths — overridable at runtime by `--assets-root`, §7).

## 11. Invariants for contributors

1. **One entry point per operation; no legacy/compat code whatsoever.** No
   forwarder scripts, no alias remaps, no "deprecated" surfaces — migrate every
   caller and delete the old name in the same commit.
2. Never build a whole solution; always `/t:<target>`.
3. Never run Sharpmake/regen from a linked worktree.
4. Nothing generated is committed; nothing hand-written is ignored (enforced
   by selftest).
5. Runner/tool outputs go under `Build/artifacts/`; packages under `dist/`.
6. Config names/paths come from `zenith_config.psd1` accessors, not literals.
7. Engine unit-test baseline bumps update `Tools/run_unit_gate.ps1` AND
   `Tools/test_scaffold.ps1` together.
8. Every new script function gets selftest coverage; every new engine
   type/method gets unit coverage.
