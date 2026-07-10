# Zenithmon Status

**Last updated:** 2026-07-10 -- **S0 COMPLETE and merged to master; gate met.**

**Read this first each session.** This file is REPLACED at every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the honest gap audit.

## Build

GREEN on master. `Vulkan_vs2022_Debug_Win64_True` + `D3D12_vs2022_Debug_Win64_False` (link proof) both clean via `zenith build Zenithmon` (per-game sln, always `/t:Zenithmon` -- never whole-sln).

## Tests

- Unit: **2 / 2 ZM tests passed** inside the 1070-test boot suite (0 failed).
- Automated: **1 / 1 passed** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0; windowed `--filter ZM_Boot_Test` run also green.
- CI: **zm-tests green on every run so far** and now a REQUIRED branch-protection check (CIPolicy.md section 4).

## What landed (S0, merged 2026-07-10)

- **PR #143** (rebase-merged as `4c35f55d` + `4e57c680`): name-validator PascalCase-word-boundary narrowing (unblocks the name "Zenithmon"); the game skeleton (ZM_ conventions, boot-authored FrontEnd.zscen at build index 0, SaveData init + between-tests hook, hello unit/automated tests); `.github/workflows/zm-tests.yml`; this 17-file Docs base incl. the full GDD.
- **PR #144** (`0844689e`, squash): fixed the 3 PRE-EXISTING master-red gates -- engine-gate (unit baseline 1053->1068, single-sourced in `Tools/run_unit_gate.ps1`; `test_scaffold.ps1` now reuses it), layering-gate (`Flux_HDR.cpp` g_xEngine 45->~35 via local hoists), scaffold-smoke (regen.ps1 dotnet-fallback when `Sharpmake.Application.exe` is absent + `lfs: true` checkout + `/p:WindowsTargetPlatformVersion=10.0` on the smoke build -- its first green EVER).
- **Branch protection created** (user-directed): master requires `zm-tests`; `enforce_admins=false`. ManualSetupChecklist.md is now fully ticked.

## Current task

None in flight. **Next up per [Roadmap.md](Roadmap.md): two parallel tracks open** --
1. **S1 data core** (pure headless C++, `Source/Data/`: ZM_Types/TypeChart/SpeciesData/MoveData/ItemData/AbilityData stubs/NatureData/StatCalc/BattleRNG + WorldSpec skeleton + DataRegistry; gate ~90 unit tests). No engine changes; safe to run fully parallel.
2. **S3 first overworld** (starts with engine changes E1 terrain-set paths + E2 rect export, each with unit tests + RenderTest boot regression).
S2 (battle engine) follows S1; S4 (asset generators) can run alongside.

## Notes for the next agent

- **The project now runs on the lifecycle loop:** StartPrompts.md prompt 0 is
  one idempotent iteration (resume in-flight work or take the next Roadmap
  task; PR -> CI -> merge -> docs; hard-stop at visual gates with a GATE-WAIT
  marker here). MasterPlan.md is the full approved plan behind the Roadmap;
  Tools\zenith_gh.ps1 wraps gh with self-bootstrapping auth; the checked-in
  .claude/settings.json allowlists the routine commands. See AgentBriefing.md
  section 9 for the verified bootstrap gotchas.
- Branch fresh off master (`git pull` first; master tip after S0 = `4e57c680`).
- **Gotchas from S0 (all verified):** bare `game.exe --exit-after-frames N` NEVER exits (per-test override only -- use `zenith test --filter` or `--list-automated-tests`); `gh run rerun` re-uses the run's ORIGINAL merge commit (rebase+push to re-evaluate a PR against new master); in sandboxed agent sessions use `pwsh -File Tools\zenith.ps1 ...` / `pwsh -File Build\regen.ps1` (the 5.1 `zenith.bat` shim hits a Get-FileHash resolution quirk there; CI + user machines unaffected).
- **Hard rules (locked, see Scope.md):** `ZM_` prefix; ~150 original species / original names (zero Nintendo IP); no audio; no networking/trading; no Dynamax-analog; singles only; game data = compiled C arrays; baked assets git-ignored (asset-dependent tests exists-guard + RequestSkip).
- **Session discipline:** replace this file at session end; tick Roadmap.md boxes only when the PR is merged AND CI green; DecisionLog.md is append-only; serial MSBuild dispatch (never build in parallel agents).
