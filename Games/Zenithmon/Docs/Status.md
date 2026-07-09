# Zenithmon Status

**Last updated:** 2026-07-09 -- S0 skeleton complete on branch `zenithmon/s0-skeleton`; first PR about to open.

**Read this first each session.** This file is REPLACED at every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the honest gap audit.

## Build

GREEN. `Vulkan_vs2022_Debug_Win64_True` builds clean via `zenith build Zenithmon` (per-game sln `Games/Zenithmon/zenithmon_win64.sln`, always `/t:Zenithmon` -- never whole-sln).

## Tests

- Unit: **2 / 2 passed** (`Tests/ZM_Tests_Boot.cpp`, `ZENITH_TEST` boot suite).
- Automated: **1 / 1 passed** (`ZM_Boot_Test` in `Tests/ZM_AutoTests_Boot.cpp`); `zenith test Zenithmon --headless` exits 0.
- Runner is the unified `zenith test <Game>` harness (`Tools/ZenithCli/ZenithCli.psm1` -> `ZenithTestHarness.psm1`; flags `--filter/--headless/--results-dir/--config/--per-process/--fail-fast`; exit codes 0 OK / 1 usage / 2 validation / 3 generation / 4 build-or-test / 5 not-found). The old per-game `Tools/run_*_tests.ps1` scripts were DELETED at commit `c29e28f8` -- never reference them as current.

## Current task

**S0 PR.** Open the first PR from `zenithmon/s0-skeleton`, watch `zm-tests` go green, merge. The S0 gate (see [Roadmap.md](Roadmap.md)) also requires the manual branch-protection step below.

## Last completed (this session, 2026-07-09)

1. **Scaffold:** `zenith new Zenithmon` (`Zenithmon.zproj` + `Zenithmon.cpp` `Project_*` entry points + regen; per-game sln generated).
2. **Engine change (name validators):** the blanket `Zenith*`/`Sentinel*` reserved-prefix rule rejected the name "Zenithmon". Both validators -- PS `Test-ZenithGameNameSyntax` in `Build/zenith_buildsystem.psm1` and C++ `ZenithHub_GameScan::ValidateName` -- were narrowed to a PascalCase word boundary: reject `Zenith`/`Sentinel` alone or followed by an uppercase letter/digit; a lowercase continuation (e.g. `Zenithmon`) is a distinct word and valid. Shared pinned vectors in `Tools/ZenithCli/Tests/name_validation_cases.txt` + buildsystem tests updated; suite 45 passed / 0 failed.
3. **ZM_ conventions applied:** `ZM_GameComponent` (registered `"ZM_Game"`, serialization order 100, S0 placeholder bobbing cube); boot-authored `FrontEnd.zscen` at build index 0 (camera + "Zenithmon" title text + game component; authored by tools builds, git-ignored).
4. **Persistence from day one:** `Zenith_SaveData::Initialise("Zenithmon")` at boot + between-tests hook registered (`Zenith_SaveData::ClearForTest`).
5. **Tests:** 2 boot unit tests (`ZM_Tests_Boot.cpp`) + 1 automated boot test (`ZM_AutoTests_Boot.cpp`), both green locally.
6. **CI:** `.github/workflows/zm-tests.yml` written (clone of `dp-tests.yml`): `zenith-setup` action (Vulkan SDK 1.3.290.0, Slang 2026.1) -> `Build/regen.ps1 -UseDotnet` -> `Vulkan_vs2022_Debug_Win64_True` build -> `D3D12_vs2022_Debug_Win64_False` backend-neutrality link proof -> DLL copies -> headless boot check -> `zenith.bat test Zenithmon --headless --results-dir Build/artifacts/test_results/zenithmon` -> artifact `zm-test-results`.
7. **Docs:** `Games/Zenithmon/Docs/` knowledge base seeded (this file + siblings).

## Notes for the next agent

- **Branch:** `zenithmon/s0-skeleton`. If its PR has merged, branch fresh off master for the next stage.
- **MANUAL STEP PENDING:** registering `zm-tests` as a required branch-protection check is a GitHub-UI step only the user can do (see Questions.md Q-2026-07-09-001 and ManualSetupChecklist.md). Until it is flipped, treat a red `zm-tests` run as a hard merge blocker by convention.
- **Next after S0:** two parallel tracks open up -- **S1 data core** (pure headless C++, `Source/Data/`) and **S3 first overworld** (starts with engine changes E1 terrain-set paths + E2 rect export). S2 follows S1; S4 can run alongside. MSBuild dispatch is SERIAL (mspdbsrv constraint) -- parallelism is code-authoring, not builds.
- **Hard rules (locked, see Scope.md):** game-code prefix `ZM_`; ~150 original species / original names everywhere (zero Nintendo IP -- mainline MECHANICS only); no audio (engine has none); no networking/multiplayer/trading; no Dynamax-analog; battle format singles only; game data tables are compiled C arrays, not disk assets; baked assets are git-ignored.
- **CI/assets gotcha:** baked assets (incl. `FrontEnd.zscen`) are git-ignored, so a fresh CI checkout has NO `Assets/` -- every asset/scene-dependent automated test must exists-guard + `RequestSkip` (the CI-fix pattern from commit `94813489`).
- **Session discipline:** replace this file at session end; tick Roadmap.md checkboxes only when the PR is merged AND CI is green; log decisions in DecisionLog.md; log blockers in Questions.md and proceed on best guess.
