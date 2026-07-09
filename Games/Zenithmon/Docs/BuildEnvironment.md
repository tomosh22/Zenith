# Zenithmon -- Build Environment

**Document purpose:** The exact dependencies, pinned versions, and commands
required to build, run, and test Zenithmon on a fresh Windows machine. This
is the human-readable reference; the one-time human steps live in
ManualSetupChecklist.md.

**Companion docs:** ManualSetupChecklist.md (one-time pre-flight),
CIPolicy.md (the CI runner's version of this environment), TestPlan.md
(test-harness conventions), Status.md (current build health).

**Last updated:** 2026-07-09 (S0).

---

## 1. Required software (pinned versions)

| Software | Version | Why | How to check |
|---|---|---|---|
| Windows | 10 (19041+) or 11 | Win32 + Vulkan target | `(Get-CimInstance Win32_OperatingSystem).Caption` |
| Visual Studio | 2022 toolset (any edition) with "Desktop development with C++" | MSBuild + C++20 compiler | `vswhere.exe -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath` |
| Vulkan SDK | **pinned 1.3.290.0** (matches the CI zenith-setup action) | graphics API + vulkan-1.dll loader | `$env:VULKAN_SDK` points at the pinned version |
| Slang | **pinned 2026.1** (fetched by CI; local tree under `Middleware/slang/`) | shader compiler + runtime DLLs | `ls Middleware\slang\bin\slang.dll` |
| vcpkg | as provisioned by `.github/actions/zenith-setup` | third-party deps | CI-managed; local machines use the checked-in Middleware tree |
| .NET SDK | 6.0+ | Sharpmake (`regen.ps1 -UseDotnet`) | `dotnet --version` |
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

The tools (`_True`) build runs editor automation at boot and **bakes
FrontEnd.zscen** (build index 0) plus every other generated asset (see
AssetManifest.md) -- all git-ignored, so a fresh checkout has none of them.
A `_False` (non-tools) build does not author anything; it LOADS the baked
`.zscen` files and will have nothing to load until a `_True` build has run
once.

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

---

## 4. Run + test commands

The unified `zenith test <Game>` harness (Tools/ZenithCli/ZenithCli.psm1 ->
ZenithTestHarness.psm1) is the ONLY test runner. The old per-game
`Tools/run_*_tests.ps1` scripts were deleted at commit `c29e28f8` -- never
reference them.

```powershell
# Full headless batch (the CI command)
.\zenith.bat test Zenithmon --headless

# With results JSON (what CI archives)
.\zenith.bat test Zenithmon --headless --results-dir Build/artifacts/test_results/zenithmon

# Filter to one test during dev (forces per-process)
.\zenith.bat test Zenithmon --filter ZM_Boot --headless
```

Flags: `--filter / --headless / --results-dir / --config / --per-process /
--fail-fast`. Exit codes: 0 OK, 1 usage, 2 validation, 3 generation,
4 build-or-test failure, 5 not-found.

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

---

## 5. Runtime DLL notes

Two DLL families must sit next to `zenithmon.exe`
(`Games\Zenithmon\Build\output\win64\vulkan_vs2022_debug_win64_true\`):

1. **The Slang tree** -- the post-build event copies `slang.dll` itself, but
   the full dependency tree (slang-rt, slang-glslang, slang-glsl-module,
   slang-llvm, slang-compiler, gfx) is needed at runtime or the exe dies
   with STATUS_DLL_NOT_FOUND. Source: `Middleware\slang\bin\*.dll`.
2. **vulkan-1.dll** -- from `$env:VULKAN_SDK\Bin\` on machines whose system
   loader is missing/stale (CI always copies it).

```powershell
$exeDir = 'Games\Zenithmon\Build\output\win64\vulkan_vs2022_debug_win64_true'
Copy-Item 'Middleware\slang\bin\*.dll' $exeDir -Force
Copy-Item (Join-Path $env:VULKAN_SDK 'Bin\vulkan-1.dll') $exeDir -Force
```

This mirrors the "Copy runtime DLLs" step in `.github/workflows/zm-tests.yml`.

---

## 6. Triage checklist for "it doesn't build / run"

| Symptom | Likely cause | Fix |
|---|---|---|
| `MSB1009: Project file does not exist` / stale project contents | generated files stale or absent | `Build\regen.ps1` (check first with `zenith regen --check`) |
| `MSB3027: Could not copy <pdb>` / file-lock errors | hung cl.exe / mspdbsrv | `zenith clean Zenithmon` (or `zenith clean --processes-only`), then rebuild |
| `LNK1318: Unexpected PDB error` | locked PDB from a dead build | `zenith clean` |
| Aux tools (FluxCompiler etc.) fail in a `_True` build | you built the whole sln | always `/t:Zenithmon` (or `zenith build Zenithmon`) -- the aux tools are pre-existing-red |
| Two builds fighting / random lock failures | parallel MSBuild dispatch from concurrent agents | serialize builds -- one MSBuild at a time on this machine |
| Runtime STATUS_DLL_NOT_FOUND | Slang tree / vulkan-1.dll missing beside the exe | section 5 above |
| `_False` build boots to nothing / missing scene | FrontEnd.zscen never baked | build + run `Vulkan_vs2022_Debug_Win64_True` once (section 3) |
| Tests missing from `--list-automated-tests` | non-tools build, or test .cpp not linked (MSVC dead-strip) | rebuild `_True`; ensure the test TU is referenced |
| `zenith.bat` fails oddly inside an agent sandbox | 5.1 shim Get-FileHash quirk | use the pwsh form (section 4.1) |

---

## 7. Known-good configuration (S0, 2026-07-09)

- Windows 11, VS 2022 toolset (18/Insiders MSBuild on the primary machine)
- Vulkan SDK 1.3.290.0, Slang 2026.1
- `Vulkan_vs2022_Debug_Win64_True` builds green
- `zenith test Zenithmon --headless` = 1 passed / 0 failed
- CI `zm-tests` workflow exercises the identical toolchain via
  `.github/actions/zenith-setup` (see CIPolicy.md)
