# Devil's Playground — One-Time Human Setup Checklist

**Document purpose:** A hard gate before the first autonomous agent session. The orchestrator's bootstrap phase (MVP-0.0.x) assumes the items below are already done. They cannot be done by an agent — they require a logged-in human in the GitHub web UI, in the local dev environment, and in software installers.

**Added 2026-05-12 per round-2 peer review.** Three independent reviewers identified the autonomy chain as breaking before task one because Phase 0.0 of the MvpRoadmap assumes admin-level git+GitHub setup that the agent's tools cannot perform.

**Usage:** Tomos ticks each box as completed. The orchestrator's session-start ritual (OrchestratorPlaybook §1.1) checks this file; if any box is unchecked, the orchestrator stops and surfaces in Questions.md.

---

## A. Local development environment

- [ ] **Visual Studio 2022** installed (Community / Pro / Enterprise) with the "Desktop development with C++" workload.
  Verify: `vswhere.exe -products * -requires Microsoft.VisualStudio.Workload.NativeDesktop -property installationPath` returns a path.
- [ ] **Windows SDK** version 10.0.22621.0 (or newer 22000+).
  Verify: `dir "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um\windows.h"` shows the file.
- [ ] **Vulkan SDK** pinned to **1.3.290.0** (or whatever known-good version BuildEnvironment.md §8 lists today).
  Verify: `$env:VULKAN_SDK` is set; `dir "${env:VULKAN_SDK}\Include\vulkan\vulkan.h"` shows the file.
- [ ] **.NET 6 or 8 SDK** installed.
  Verify: `dotnet --version` returns >= 6.0.
- [ ] **PowerShell 7+** (`pwsh.exe`) installed AND on PATH. **Critical.** Without this, the post-build slang DLL copy step fails silently and the game can't load.
  Verify: `pwsh.exe -Command "$PSVersionTable.PSVersion"` returns >= 7.0.
- [ ] **Git** 2.30+ installed.
  Verify: `git --version` returns >= 2.30.
- [ ] **GitHub CLI** (`gh`) installed AND authenticated with **exact scopes** `repo`, `workflow`, `admin:repo_hook` (or, on fine-grained tokens, "Contents: Read/Write" + "Actions: Read/Write" + "Administration: Read/Write" + "Pull requests: Read/Write" on the DP repo).
  Verify: `gh auth status` shows authenticated; `gh auth status --show-token | grep "Token scopes"` includes `repo`, `workflow`, `admin:repo_hook`; `gh api user --jq .login` returns Tomos.
  **Why these specific scopes:** `repo` covers code + PRs; `workflow` is needed to author/update `.github/workflows/` files (without it, the orchestrator's first PR that adds CI workflows is rejected by the API); `admin:repo_hook` is needed to set branch-protection rules via `gh api repos/{owner}/{repo}/branches/master/protection`. **If you log in with a token missing `admin:repo_hook`**, MVP-0.0.6 falls through to a web-UI step (orchestrator surfaces in Questions.md).

## B. Repository state

- [ ] The repo is cloned to `C:\dev\Zenith\` (not in a worktree).
- [ ] `git status` shows a clean tree on `master`.
- [ ] `git pull` succeeds.
- [ ] The DP project builds locally **today**, with `vs2022_Debug_Win64_True`. Verify by running the existing tests:
  ```powershell
  cd C:\dev\Zenith\Build
  cmd /c '.\Sharpmake_Build.bat < nul'
  & 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' zenith_win64.sln /p:Configuration=vs2022_Debug_Win64_True /p:Platform=x64 -maxCpuCount:1
  cd ..
  .\Tools\run_dp_tests.ps1 -Headless
  ```
  Result: the ~110 registered tests run; current master suite is fully green in headless mode. What matters is **the build itself succeeds** so the orchestrator can run + extend it.

## C. GitHub repository configuration (web UI work)

**Two-phase setup** (corrected 2026-05-12 round-5 peer review — earlier version had a circular dependency where branch protection required workflow check names that the orchestrator was supposed to create).

### C.1 Before first agent session (preflight)

- [ ] **Repository write access** for the `gh` CLI token confirmed.
- [ ] **GitHub Actions enabled** for the repo (Settings → Actions → Allow all actions).
- [ ] **Auto-merge enabled at repo level** (Settings → General → Pull Requests → "Allow auto-merge"). This is a one-line setting; it doesn't depend on workflows existing yet.

**Branch protection is intentionally NOT done here.** It requires `dp-build` and `dp-tests` to exist as named status checks, which is what MVP-0.0.2 + MVP-0.0.3 author. Setting branch protection before those workflows exist creates a deadlock (no PR can merge because the required checks don't yet exist to pass).

### C.2 After MVP-0.0.7 smoke PR passes (postflight)

When the orchestrator reports MVP-0.0.7 succeeded (the smoke PR auto-merged via the new `dp-build` + `dp-tests` workflows), Tomos returns to the web UI:

- [ ] **Branch protection** on `master`:
  - Settings → Branches → Add rule → "Require status checks to pass before merging" enabled.
  - Required checks: `dp-build`, `dp-tests` (now existing after MVP-0.0.2/0.0.3 landed).
  - "Require linear history" enabled (matches `gh pr merge --squash`).
  - "Include administrators" off (so Tomos can bypass for emergencies).

The orchestrator surfaces in Questions.md when MVP-0.0.7 completes, prompting Tomos to do this postflight step. After protection lands, all subsequent PRs are gated.

## D. Slang DLLs

- [ ] Slang runtime DLLs are available in a Combat / Test build output directory, so the orchestrator can copy them to the DP build output. The build's post-step is supposed to do this automatically; until MVP-0.0.5 lands, it may not.
  Verify: `ls C:\dev\Zenith\Games\Combat\Build\output\win64\vs2022_debug_win64_true\*.dll` shows slang DLLs.
  If not: build any other game project (e.g. `Sokoban`) first to populate slang DLLs.

## E. Backup and safety

- [ ] **Recent backup** of `C:\dev\Zenith\` (or the working tree is otherwise in cloud sync / version control). The orchestrator can rewrite files; a recent backup limits damage from agent mistakes.
- [ ] **Machine resources confirmed:** 16 GB+ RAM, SSD, free disk ≥ 50 GB. MSBuild + asset import can thrash on less.

## F. Optional but recommended

- [ ] Git LFS installed (`git lfs install`). Not strictly required for MVP (no binary assets in MVP scope yet), but the moment Phase 3 lands it becomes essential. See AssetManifest §0.3.
- [ ] An external monitor and good chair — autonomous sessions still need human-reviewer-time at PR-merge cadence.

---

## After all boxes ticked

You're ready to launch the first orchestrator session. Paste the canonical prompt from [StartPrompts.md](StartPrompts.md) §1 into a fresh Claude Code window opened with cwd = `C:\dev\Zenith`.

The first session will execute MVP-0.0.1 (`Tools/verify_build_env.ps1`). If that passes, the second session will continue with MVP-0.0.2 (`.github/workflows/dp-pr.yml`).

**You will need to re-engage manually** at MVP-0.0.6 (branch protection web-UI configuration once `dp-build` and `dp-tests` check names exist) and MVP-0.0.7 (verify the smoke PR auto-merges). The orchestrator surfaces these in Questions.md when it reaches them. Plan ~30 minutes of human time during the bootstrap phase to handle these.

---

## Items not on this checklist (deliberately)

- Branch creation is automatic (orchestrator runs `git checkout -b dp/...`).
- Sharpmake regen is automatic (orchestrator runs it after `.cpp` additions).
- Test running is automatic (orchestrator owns the build/test gate).

If something on this list seems automatable, it usually isn't — items here either require GitHub web-UI access (no agent tool reaches it), software installation (requires admin elevation), or one-time license acceptance.
