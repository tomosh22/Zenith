# DP Status

**Last updated:** 2026-05-12 by orchestrator (wizardly-payne session)
**Build:** ✅ passing (`vs2022_Debug_Win64_True`, DP target, 0 warnings, 0 errors)
**Tests:** 34/35 — Test_P1Tuning_LoadsAndValuesInBand green; **HumanPlaythrough_Test fails at frame-budget cap** (pre-existing skeleton-state issue, see Questions.md Q-2026-05-12-002)

## Current task

**MVP-0.1.3** — Migrate `DPVillager_Behaviour` to read `m_fMaxLife`, `m_fMoveSpeed` from `DP_Tuning`. Add migration test that proves prototype's behaviour unchanged.

## Last completed

- **MVP-0.1.1 + MVP-0.1.2** (this session) — Added [Source/DP_Tuning.h](../Source/DP_Tuning.h) + [Source/DP_Tuning.cpp](../Source/DP_Tuning.cpp). Loads `Config/Tuning.json` at boot, exposes `DP_Tuning::Get<float|int|bool>(dottedKey)` with linear-scan lookup over a flat-dotted cache. Hooked into `DevilsPlayground::InitializeResources()` (before `DPMaterials::Initialize()`) and `CleanupResources()` (last). [Test_P1Tuning_LoadsAndValuesInBand](../Tests/Test_P1Tuning_LoadsAndValuesInBand.cpp) covers 5 exact-equality + 7 sanity-band assertions. JSON parser cloned verbatim from `DPMaterials.cpp:24-242` (refactor to shared `DP_Json` deferred to MVP-0.2.1 when DP_Archetypes is the 2nd consumer).

## Notes for next agent

1. **Read [OrchestratorPlaybook.md](OrchestratorPlaybook.md) first** if you are the orchestrator of a fresh user-initiated session. The user reaffirmed the Mode-B orchestrator+subagent pattern and the three invariants (serial game execution / no worktrees / single source of truth for state files).
2. **Worktree warning.** This session ran inside `.claude/worktrees/wizardly-payne-c210e5/` (Claude harness placement). The worktree was missing many build artifacts (slang DLLs, opencv/vulkan runtime DLLs, Sharpmake.Application.exe, `Zenith/Assets/`, `Games/DevilsPlayground/Assets/`) which I had to copy from `C:\dev\Zenith\`. Sharpmake also bakes the cwd's absolute path into vcxprojs — committing those would break other checkouts. I restored the vcxprojs and left them out of the PR. **Future autonomous sessions should prefer `C:\dev\Zenith\` over the worktree.** See Questions.md Q-2026-05-12-003.
3. **MVP-0.1.3** depends on `DP_Tuning::Get<float>("movement.walk_speed_mps")` etc. just landed — straightforward refactor; the migration test should snapshot life-timer and walk-speed before/after the migration.
4. **No CI gate for DP.** [.github/workflows/msbuild.yml](../../../.github/workflows/msbuild.yml) builds `Games/Test` only and doesn't run DP tests. Auto-merge merges immediately because there's no DP-specific check. **Run `Tools/run_dp_tests.ps1 -Headless` locally** before opening a DP PR until MVP-0.3.x lands DP CI workflows. See Questions.md Q-2026-05-12-001.
5. **Test runner script broken under Windows PowerShell 5.1** (no `pwsh.exe` on this machine). I bypassed by invoking `devilsplayground.exe --all-automated-tests` directly. See Questions.md Q-2026-05-12-005.
6. **Main repo `C:\dev\Zenith\` has uncommitted in-progress work** (16+ files staged, including changes to `DevilsPlayground.cpp`, `Tuning.json`, doc files, and new Glossary/BuildEnvironment/ManualSetupChecklist docs). DO NOT touch the main repo until the user merges or stashes that work — it would clobber unsaved changes.
