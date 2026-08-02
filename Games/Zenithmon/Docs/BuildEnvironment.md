# Zenithmon -- Build Environment

**Document purpose:** The exact dependencies, pinned versions, and commands
required to build, run, and test Zenithmon on a fresh Windows machine. This
is the human-readable reference; the one-time human steps live in
ManualSetupChecklist.md.

**Companion docs:** ManualSetupChecklist.md (one-time pre-flight),
CIPolicy.md (the CI runner's version of this environment), TestPlan.md
(test-harness conventions), Status.md (current build health).

**Last updated:** 2026-08-01 (doc audit at `c9d64994`: section 3's
"FrontEnd.zscen is baked, not committed" premise was false -- all five `.zscen`
are tracked; the Null/headless build requirement, the boot unit gate, and the
real CI DLL-heal were added; the `.NET SDK` row corrected to runtime-only).
A follow-up sweep the same day split the **Vulkan SDK** row: 1.3.290.0 is the
CI pin, never a local requirement -- the local machine runs 1.4.313.1 and
builds green, and the old "point `$env:VULKAN_SDK` at the pinned version"
instruction was unattainable here (1.3.290.0 is not installed).
Section 7's known-good snapshot remains an S0 (2026-07-09) record.

---

## 1. Required software (pinned versions)

| Software | Version | Why | How to check |
|---|---|---|---|
| Windows | 10 (19041+) or 11 | Win32 + Vulkan target | `(Get-CimInstance Win32_OperatingSystem).Caption` |
| Visual Studio | 2022 toolset (any edition) with "Desktop development with C++" | MSBuild + C++20 compiler | `vswhere.exe -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath` |
| Vulkan SDK | **locally: any 1.3+ SDK** (this machine runs **1.4.313.1**). **CI pins 1.3.290.0** -- that pin is a CI FACT (`zm-tests.yml` `VULKAN_SDK_VERSION`, consumed by the zenith-setup action), NOT a local requirement; do not reinstall to match it. | graphics API + vulkan-1.dll loader | `$env:VULKAN_SDK` is set and points at an installed 1.3+ SDK (`ls (Split-Path $env:VULKAN_SDK -Parent)` lists what you have) |
| Slang | **pinned 2026.1** (fetched by CI; local tree under `Middleware/slang/`) | shader compiler + runtime DLLs | `ls Middleware\slang\bin\slang.dll` |
| vcpkg | as provisioned by `.github/actions/zenith-setup` | third-party deps | CI-managed; local machines use the checked-in Middleware tree |
| .NET **runtime** | 6.0+ | Sharpmake's `-UseDotnet` fallback (`dotnet exec Sharpmake/Sharpmake.Application.dll`). The **SDK is NOT required** -- regen's default path runs the tracked prebuilt `Sharpmake/Sharpmake.Application.exe` and needs no dotnet at all. | `dotnet exec Sharpmake\Sharpmake.Application.dll` runs. **Do not check with `dotnet --version`** -- that needs the SDK and fails on a runtime-only machine (as this one does) while regen works fine. |
| PowerShell | 7+ (`pwsh.exe`) recommended; 5.1 works for most flows (see 4.1 caveat) | zenith CLI + build scripts | `pwsh -Command '$PSVersionTable.PSVersion'` |
| Git | 2.30+ | source control | `git --version` |

**This machine's MSBuild** (VS "18/Insiders" layout -- your path may be the
standard `...\2022\<edition>\...` instead):

```
C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\MSBuild.exe
```

---

## 2. Regenerate-first policy

EVERYTHING Sharpmake emits is git-ignored -- all `.sln`, `.vcxproj`,
`.vcxproj.filters`, and the generated `.cs`. After a fresh clone, or any
checkout/pull that touches a `.zproj` or `Build/Sharpmake_*.cs`, regenerate
BEFORE building:

```powershell
Build\regen.ps1          # or: zenith regen
zenith regen --check     # report staleness without regenerating
```

`zenith new Zenithmon` already ran this at scaffold time; you only re-run it
when descriptors/Sharpmake change or after adding source files.

---

## 3. First build MUST be Vulkan_vs2022_Debug_Win64_True

The tools (`_True`) build runs editor automation at boot and bakes every
generated asset -- meshes, textures, anims, terrains (see AssetManifest.md) --
all git-ignored, so a fresh checkout has none of them. A `_False` (non-tools)
build authors nothing and will render an empty/untextured world until a `_True`
build has run once.

**The five `.zscen` files are NOT in that set -- they are committed.** All five
(`Battle`, `Dawnmere`, `FrontEnd`, `PlayerHome`, `ProfLab`) plus
`Assets/Navmesh/Dawnmere.znavmesh` are tracked (ZM-D-147/148); verify with
`git ls-files Games/Zenithmon/Assets`. This section claimed FrontEnd.zscen was
baked-not-committed until the 2026-08-01 audit. Re-authoring the committed
files needs a WINDOWED `_True` boot, and Dawnmere needs the terrain recipes
already warm -- two boots on a fresh clone. You normally never need to: the
committed bytes are what the game loads, and a boot must NOT leave a `.zscen`
dirty in `git status`.

```powershell
# Recommended
zenith build Zenithmon

# Equivalent direct msbuild (never build the whole sln -- /t:Zenithmon only)
msbuild Games\Zenithmon\zenithmon_win64.sln /t:Zenithmon `
    /p:Configuration=Vulkan_vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount
```

Then run once to execute the bake:

```powershell
zenith run Zenithmon
# exe lands at:
# Games\Zenithmon\Build\output\win64\vulkan_vs2022_debug_win64_true\zenithmon.exe
```

**★ AN ALREADY-WARM TREE RE-BAKES WHENEVER A GENERATOR VERSION MOVES, AND THAT IS
NOT A FAULT.** Bake stamps are `(version, expected file count)` and do not hash the
baked bytes, so a change that rewrites contents without changing the file count is
invalidated *only* by its version bump. Two are live right now and both cost a
one-off re-bake on an existing tree:

| Bump | Cost on next `_True` boot |
|---|---|
| `uZM_HUMANGEN_VERSION` 1 -> 2 (ZM-D-181, centre-anchored bind space) | re-bakes the 35-model humans family |
| `uZM_TERRAIN_MANIFEST_VERSION` 1 -> 2 (ZM-D-182, collision divisor 8 -> 4) | re-bakes **all three** terrain recipes, several minutes |

If a boot appears to hang for minutes on a tree that was warm yesterday, check
whether a version moved before hunting a bug. **A terrain bake that is interrupted
leaves the family cold** (`ZM_PrepareTerrainBake` removes the stamp first), so it
restarts next boot -- do not run the bake under a short watchdog such as
`run_unit_gate.ps1`, which kills the process; let a normal `zenith run` finish it
once. The Dawnmere scene is NOT re-authored by a bake boot
(`m_bAuthorDawnmereScene` requires every recipe already warm), so no committed
`.zscen` moves while the bake is catching up.

---

## 4. Run + test commands

The unified `zenith test <Game>` harness (Tools/ZenithCli/ZenithCli.psm1 ->
ZenithTestHarness.psm1) is the ONLY test runner. The old per-game
`Tools/run_*_tests.ps1` scripts were deleted at commit `c29e28f8` -- never
reference them.

**Build the Null config first.** `--headless` is a BUILD-CONFIG selector
(`Null_vs2022_Debug_Win64_True`, from `Build/zenith_config.psd1`), not a runtime
flag, and test DISCOVERY always uses the Null exe -- so even a *windowed*
`zenith test` aborts with "test discovery needs the Null build" until it exists:

```powershell
.\zenith.bat build Zenithmon --headless
```

```powershell
# Full headless batch (the CI command)
.\zenith.bat test Zenithmon --headless

# With results JSON (what CI archives)
.\zenith.bat test Zenithmon --headless --results-dir Build/artifacts/test_results/zenithmon

# Filter to one test during dev (forces per-process -- and per-process DISCARDS
# the child's stdout, so the test's own failure text is lost; see the note below)
.\zenith.bat test Zenithmon --filter ZM_Boot --headless

# Named run in ONE process -- this path TEES engine output to the console,
# so it is how you read a failing test's diagnostics
.\zenith.bat test Zenithmon --tests ZM_Boot_Test
```

Flags: `--filter / --tier / --tests / --batch-order / --headless /
--results-dir / --config / --per-process / --fail-fast / --build /
--exit-after-frames / --assertions-log`. Exit codes: 0 OK, 1 usage,
2 validation, 3 generation, 4 build-or-test failure, 5 not-found.

**`--exit-after-frames` only does anything while an automated test is running.**
It is safe in the list above only because `zenith test` always runs one. It is an
engine flag the harness forwards to the exe, and it is a per-TEST max-frames
OVERRIDE -- it replaces every test's own `maxFrames`, so don't pass it "for
safety" or you will truncate long tests into failures. Handed to a **bare game
exe** with no `--automated-test` / `--all-automated-tests` it is silently ignored
and a tools build idles in the editor forever. See the bullet in
[AgentBriefing.md](AgentBriefing.md) section 9 for the failure signature.

**Result JSON carries no failure text.** `Zenith_AutomatedTest.h` records that
the per-test JSON's `"failures"` field "is currently always an empty array ...
Tests that need detail today print to stdout instead", and the harness's
per-process path (`--filter` / `--per-process` / `--fail-fast`) runs the exe as
`& $Exe @runArgs 2>&1 | Out-Null`. Between them, a failing test's
`Zenith_Log`/`Zenith_Error` output is unreachable from both the console and
`Build/artifacts/test_results/**/<Test>.json`. Use `--tests <Name>` (or a full
batch) when you need to read it.

### 4.1 The pwsh form (sandboxed agent sessions)

`zenith.bat` shims through Windows PowerShell 5.1. In SOME sandboxed agent
sessions that shim hits a Get-FileHash cmdlet-resolution quirk and fails;
CI runners and normal user machines are unaffected. If `zenith.bat`
misbehaves in an agent sandbox, use the direct pwsh form -- it is exactly
equivalent:

```powershell
pwsh -NoProfile -File Tools/zenith.ps1 test Zenithmon --headless
pwsh -NoProfile -File Tools/zenith.ps1 build Zenithmon
```

### 4.2 The boot unit gate (`zenith test` does NOT run it)

`zenith test` passes `--skip-unit-tests`, so the ZENITH_TEST suite runs only
here (and in the equivalent `zm-tests` step):

```powershell
pwsh -NoProfile -File Tools\run_unit_gate.ps1 `
    -Exe Games\Zenithmon\Build\output\win64\null_vs2022_debug_win64_true\zenithmon.exe `
    -Baseline <N> -TimeoutSec 600
```

- `-Exe` **must** be the `Null_*` build -- a Vulkan exe hangs in
  `vkEnumeratePhysicalDevices` and its unit count differs (it also compiles the
  Vulkan-only tests).
- `-TimeoutSec 600` is not optional for Zenithmon: the script's 180 s default
  sits inside this suite's measured runtime (175/193/229/235 s on one idle dev
  machine), and losing that race is reported as "no 'Unit tests complete' line
  in boot output" -- which reads like a crash rather than a slow suite
  (ZM-D-163). `zm-tests.yml` passes 600.
- Run it AFTER `zenith test`, which heals a fresh Null output dir's DLLs.
- `-Baseline` is the live pin in `.github/workflows/zm-tests.yml`; the script's
  own `$Baseline` default is the ENGINE-only number and the two are never
  interchangeable. Read current values from those pins / Status.md, never from
  this file.

---

## 5. Runtime DLL notes

Every game exe dir needs more DLLs than the Sharpmake post-build event copies.
A FIRST build in a new config leaves them absent and the exe dies with
STATUS_DLL_NOT_FOUND (exit `0xC0000135`) before `main()` -- an empty log that
reads as a build failure.

1. **The Slang tree** -- the post-build event copies `slang.dll` itself, but
   the full dependency tree (slang-rt, slang-glslang, slang-glsl-module,
   slang-llvm, slang-compiler, gfx) is needed at runtime. Source:
   `Middleware\slang\bin\*.dll`.
2. **The assimp runtime tree** -- `assimp-vc143-mt[d]` + draco / minizip /
   poly2tri / pugixml / zlib. The engine links the assimp import chain into
   EVERY game exe. Debug DLLs live in `Tools\Middleware\assimp\debug\bin`,
   release in the bare `Tools\Middleware\assimp\bin`.
3. **vulkan-1.dll** -- only for `Vulkan_*` exes, from `$env:VULKAN_SDK\Bin\`, on
   machines whose system loader is missing/stale. **A `Null_*` build never loads
   a Vulkan loader and does not want it** -- which is why the CI step does not
   copy it.

**Use the shared healer rather than hand-copying** -- it is the single source of
truth for what an exe dir needs, never overwrites an existing DLL, and is
exactly what `zenith test` and the `zm-tests` "Copy runtime DLLs" step call:

```powershell
Import-Module .\Build\zenith_buildsystem.psm1 -Force
Repair-ZenithRuntimeDlls -ExeDir (Resolve-Path `
    'Games\Zenithmon\Build\output\win64\null_vs2022_debug_win64_true').Path
```

(NOTE the leading `.\` on the Import-Module path: without it PowerShell reads
the argument as a MODULE NAME and searches `$PSModulePath`, not the repo -- a
defect that once killed four CI gates.)

---

## 6. Triage checklist for "it doesn't build / run"

| Symptom | Likely cause | Fix |
|---|---|---|
| `MSB1009: Project file does not exist` / stale project contents | generated files stale or absent | `Build\regen.ps1` (check first with `zenith regen --check`) |
| `MSB3027: Could not copy <pdb>` / file-lock errors | hung cl.exe / mspdbsrv | `zenith clean Zenithmon` (or `zenith clean --processes-only`), then rebuild |
| `LNK1318: Unexpected PDB error` | locked PDB from a dead build | `zenith clean` |
| Aux tools (FluxCompiler etc.) fail in a `_True` build | you built the whole sln | always `/t:Zenithmon` (or `zenith build Zenithmon`) -- the aux tools are pre-existing-red |
| Two builds fighting / random lock failures | parallel MSBuild dispatch from concurrent agents | serialize builds -- one MSBuild at a time on this machine |
| Runtime STATUS_DLL_NOT_FOUND (`0xC0000135`) / exe exits with an EMPTY log | Slang or assimp tree missing beside the exe (typical of a FIRST build in a new config) | `Repair-ZenithRuntimeDlls` -- section 5 above |
| Unit gate says "no 'Unit tests complete' line in boot output" | usually NOT a crash: either the 180 s default timeout expired, or the exe died in the loader (row above) | pass `-TimeoutSec 600`; run the gate after `zenith test` (section 4.2) |
| `zenith test` errors "test discovery needs the Null build" | the `Null_*` exe has never been built | `zenith build Zenithmon --headless` (section 4) |
| Build boots to an untextured / empty-looking world | generated meshes+textures+terrain never baked (the `.zscen` files themselves ARE committed) | build + run `Vulkan_vs2022_Debug_Win64_True` once (section 3) |
| Tests missing from `--list-automated-tests` | non-tools build, or test .cpp not linked (MSVC dead-strip), or a new `.cpp` added without a regen | rebuild `_True`; `Build\regen.ps1`; ensure the test TU is referenced |
| `zenith.bat` fails oddly inside an agent sandbox | 5.1 shim Get-FileHash quirk | use the pwsh form (section 4.1) |

---

## 7. Known-good configuration (S0, 2026-07-09)

- Windows 11, VS 2022 toolset (18/Insiders MSBuild on the primary machine)
- Vulkan SDK 1.3.290.0, Slang 2026.1
- `Vulkan_vs2022_Debug_Win64_True` builds green
- `zenith test Zenithmon --headless` = 1 passed / 0 failed
- CI `zm-tests` workflow exercises the identical toolchain via
  `.github/actions/zenith-setup` (see CIPolicy.md)
