# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 data core: boxes 1-7 of 8 done. Box 8 (ZM_DataRegistry) CLOSES S1.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## CI / merge policy (NEW 2026-07-10, ZM-D-028)

**Do NOT wait on / idle-watch CI.** The full LOCAL gate is the quality bar: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, all green before push. After opening a PR, enable auto-merge (`zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`) -- it lands when the required `zm-tests` check passes; `--admin`/bypass is blocked by the harness. Fill the CI window designing/prototyping the next task. See StartPrompts.md.

## Build / Tests

- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`). D3D12_False link proof in CI.
- Unit (T0, `ZM_Data`): **1163 ran, 1162 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 93 `ZM_*` (9 type + 24 species + 16 move + 11 item + 6 nature + 6 ability + 4 statcalc + 6 rng + 11 worldspec) + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0. **Baseline is 1163** in `zm-tests.yml`.

## What landed (S1 data core: 7/8 boxes)

- Box 1 types (#147); Box 2 species roster+stats+learnsets (#148/#149/#151); Box 3 moves (#150); Box 4 items (#152); Box 5 abilities+natures (#153); Box 6 StatCalc+RNG (#154).
- **This session:** CI-policy docs (PR #155, ZM-D-028) + **Box 7 `ZM_WorldSpec` skeleton (PR #156, ZM-D-029)** -- world-table schema + 8-scene proving set + 11 referential-integrity tests. NOTE: #156 (worldspec) is stacked on #155 (policy); both auto-merge in order.

## Current task

None in flight (once #155 + #156 merge). **Next Roadmap task: `ZM_DataRegistry` -- Roadmap S1 box 8, which CLOSES S1.** Build name->ID lookup indices across the data tables (`ZM_FindSpeciesByName` / move / item / ability / nature / scene -> the ID or the NONE sentinel; case-insensitive optional) + a unified cross-table `ZM_Tests_DataRegistry` schema-enforcer suite that asserts the tables are MUTUALLY consistent: every species `m_eEvolvesTo` resolves (or is NONE), every TM's taught move resolves, every WorldSpec encounter species resolves, every derived learnset move resolves, name lookups round-trip + reject unknowns, no cross-table ID confusion. Then S1 is DONE.
- **S1 gate:** ~90 unit tests (currently 93 `ZM_*`), chart/stat/registry integrity -- NO visual check, so the loop does NOT hard-stop at the S1 gate; after box 8 it proceeds to S2 (battle engine, headless, ~370 tests). First visual gate is S3.

## Notes for the next agent

- **BUMP THE zm-tests BASELINE** (`.github/workflows/zm-tests.yml`, `run_unit_gate.ps1 -Baseline N`) in the SAME PR whenever ZM_* unit tests change. Currently **1163**.
- **DataRegistry is mostly LOOKUP + a cross-table test suite** -- much of the per-table integrity already exists (species/move/item/etc. suites). The NEW value is name->ID indices (for save-load, WorldSpec authoring, debug) + the CROSS-table checks (species evo targets, TM moves, encounter species) in one place. Keep it pure/headless.
- **S1 patterns:** big tables + golden vectors prototyped/validated in scratchpad Python before building; "data now, executor later" for move/item/ability behaviour (S2); derived placeholders (base stats ZM-D-021, learnsets ZM-D-023) vs exact tables (natures ZM-D-025). `ZM_STAT` lives in `ZM_SpeciesData.h`. `ZM_BattleRNG` is the only sanctioned RNG.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- `Set-Location C:\dev\Zenith` before regen/build; avoid `cd`. Tracked `Tools/**/__pycache__/*.pyc` drift on build -- `git checkout --` them; never stage them.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`). Branch fresh off master.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild.
