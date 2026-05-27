# Devil's Playground — Build Environment

**Document purpose:** The exact dependencies, versions, and one-time setup steps required to build and test DevilsPlayground on a fresh Windows machine. `Tools/verify_build_env.ps1` (MVP-0.0.1) checks all of these programmatically; this doc is the human-readable reference.

**Added 2026-05-11** per peer-review consensus that the autonomy chain depends on unstated environment assumptions.

---

## 1. Required software

| Software | Version | Why | How to check |
|---|---|---|---|
| Windows | 10 (build 19041+) or 11 | Engine targets Win32 + Vulkan | `(Get-CimInstance Win32_OperatingSystem).Caption` |
| Visual Studio | 2022 (any edition: Community / Pro / Enterprise) | MSBuild + C++ compiler | Path: `C:\Program Files\Microsoft Visual Studio\2022\<edition>\MSBuild\Current\Bin\MSBuild.exe` |
| VS Workload | "Desktop development with C++" | C++ compiler + Windows SDK + MSBuild | `vswhere.exe -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath` |
| Windows SDK | 10.0.22000.0 or newer | Win32 API headers | Path: `C:\Program Files (x86)\Windows Kits\10\Include\<version>\um` |
| Vulkan SDK | **pinned to 1.3.290.0** (any newer 1.3.x may work but is unverified; older versions have known GLFW interop bugs) | Engine's graphics API | `$env:VULKAN_SDK` must point at the pinned version; check `${env:VULKAN_SDK}\Include\vulkan\vulkan.h` exists |
| .NET SDK | 6.0+ | Sharpmake (C# build) | `dotnet --version` returns >= 6.0 |
| PowerShell | 7+ (`pwsh.exe`) or Windows PowerShell 5.1 (`powershell.exe`) | Test runner + tool scripts | `pwsh.exe -Command "$PSVersionTable.PSVersion"` |
| Git | 2.30+ | Source control | `git --version` |
| GitHub CLI (`gh`) | 2.0+, authenticated | PR creation + auto-merge | `gh auth status` should show authenticated |

**Optional but recommended:**

| Software | Version | Why |
|---|---|---|
| Git LFS | 3.0+ | Binary assets when MVP-3.x lands |
| Vulkan Validation Layers | matches SDK version | Graphics debugging |

---

## 2. One-time repository setup

```powershell
# Clone (if not already)
git clone <repo-url> C:\dev\Zenith

# Authenticate gh CLI
gh auth login

# Verify environment
cd C:\dev\Zenith
.\Tools\verify_build_env.ps1   # MVP-0.0.1 — exits 0 on success
```

---

## 3. First build

```powershell
cd C:\dev\Zenith\Build

# Generate solution files
cmd /c '.\Sharpmake_Build.bat < nul'

# Build DevilsPlayground (tools, debug, x64)
$msbuild = 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe'
& $msbuild zenith_win64.sln /t:DevilsPlayground /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount:1
```

If the build fails on **"cannot access file"** errors, see `CLAUDE.md` (root) section "Hanging Compiler Processes" or run `Build\CleanBuild.bat`.

---

## 4. Slang DLL post-build copy

Some configurations require Slang shader-compiler DLLs to be copied alongside the executable. If you see `DLL_NOT_FOUND` errors at runtime, manually copy:

```powershell
$src = "C:\dev\Zenith\Games\Combat\Build\output\win64\vs2022_debug_win64_true\*.dll"
$dst = "C:\dev\Zenith\Games\DevilsPlayground\Build\output\win64\vs2022_debug_win64_true\"
Copy-Item $src $dst -Force
```

This is the issue noted in `Games/DevilsPlayground/CLAUDE.md`: the automatic copy step uses `pwsh.exe` (PowerShell 7), and if that's not on PATH the post-build step fails silently. **MVP-0.0.5** fixes this by either installing PowerShell 7 or rewriting the post-build step to use `powershell.exe` with `-NoProfile`.

---

## 5. First test run

```powershell
cd C:\dev\Zenith
.\Tools\run_dp_tests.ps1 -Headless
```

Expected: **133 `ZENITH_AUTOMATED_TEST_REGISTER` invocations across 113 .cpp files** at HEAD (verified 2026-05-27 via `grep -c ZENITH_AUTOMATED_TEST_REGISTER Games/DevilsPlayground/Tests/*.cpp`); some are `#ifdef ZENITH_INPUT_SIMULATOR`-gated, so the actual runtime test count depends on the build config. The runner exits 0 if all pass, 1 if any fail. Each per-test JSON now includes a `durationMs` field and the runner prints the slowest-10 after every batch. **Pass-rate caveat (2026-05-27):** see [Status.md](../../../Games/DevilsPlayground/Docs/Status.md) Tests line — a local headless run at HEAD with uncommitted working-tree changes reported failures + an apparent mid-batch hang; treat the canonical pass-rate as "see latest green CI" rather than a local-run snapshot until the root cause is investigated.

Filter to a specific test during dev:

```powershell
.\Tools\run_dp_tests.ps1 -Filter "Possession" -Headless
```

**Note:** the `-Tier`, `-FailFast`, `-AssertionsLog` flags referenced in `TestPlan.md` §7 are all live in `Tools/run_dp_tests.ps1` (shipped in MVP-0.0.4 PR #8, 2026-05-12).

---

## 6. CI environment (GitHub Actions)

The `.github/workflows/dp-pr.yml` and `dp-tests.yml` workflows run on each PR. They use a Windows runner with the same software requirements as above, plus:

- `windows-latest` (or pinned `windows-2022`) for the runner OS.
- A cached Vulkan SDK installer (the GitHub Actions Vulkan setup is a known slow step).
- `gh` is pre-installed on GitHub-hosted runners.

**MVP-0.0.2** (PR #5) shipped `.github/workflows/dp-pr.yml` (the `dp-build` required check); **MVP-0.0.3** (PR #7 skeleton + PR #15 re-add) shipped `.github/workflows/dp-tests.yml` (the `dp-tests` required check). Both are live as of 2026-05-13 and gate every PR to `master` per [CIPolicy.md](CIPolicy.md). The legacy `complexity.yml` runs alongside as the `complexity-gate` check. The doc-lint workflow shipped 2026-05-13 (MVP-0.3.2, PR #24).

---

## 7. Quick triage checklist for "it doesn't build"

| Symptom | Likely cause | Fix |
|---|---|---|
| `MSBUILD : error MSB1009: Project file does not exist.` | Solution not regenerated after adding `.cpp` | Run `Sharpmake_Build.bat` |
| `error MSB3027: Could not copy <pdb>` | Hanging compiler process | Run `Build\CleanBuild.bat` |
| `LNK1318: Unexpected PDB error` | Parallel MSBuild thrash | Add `-maxCpuCount:1` |
| `unresolved external symbol Zenith_AIAgentComponent::` | Dead-strip ate the registration | Add runtime `AddComponent` from a referenced script |
| Runtime `DLL_NOT_FOUND` | Slang DLLs missing | See §4 above |
| Test runner can't find tests | `--list-automated-tests` returns empty | Build was non-tools; rebuild `*_True` config |
| Tests pass tools-mode, fail non-tools | Scene wasn't re-baked after authoring change | Build tools first, run, then test non-tools |

---

## 8. Known good versions

The configuration most recently confirmed working (per project memory 2026-05-10):

- Windows 11
- Visual Studio 2022 Community
- Windows SDK 10.0.22621.0
- Vulkan SDK 1.3.290.0
- .NET 8 SDK
- pwsh 7.4
- `vs2022_Debug_Win64_True` and `vs2022_Release_Win64_False` configs both build
- HumanPlaythrough_Test passes 8390 frames end-to-end

**If you can match this configuration, the build will work.** Drift from these versions may surface issues; bisect against the known-good if so.
