# Zenithmon Status

**Last updated:** 2026-07-10 -- **S1 DATA CORE COMPLETE (all 8 boxes). Next stage: S2 battle engine.**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit.

## CI / merge policy (2026-07-10, ZM-D-028)

**Do NOT wait on / idle-watch CI.** The full LOCAL gate is the quality bar: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, all green before push. After opening a PR, enable auto-merge (`zenith_gh.ps1 pr merge <n> --auto --squash --delete-branch`) -- lands when required `zm-tests` passes; `--admin`/bypass is blocked. Fill the CI window designing/prototyping the next task. See StartPrompts.md.

## Build / Tests

- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`). D3D12_False link proof in CI.
- Unit (T0, `ZM_Data`): **1172 ran, 1171 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = **102 `ZM_*`** (9 type + 24 species + 16 move + 11 item + 6 nature + 6 ability + 4 statcalc + 6 rng + 11 worldspec + 9 registry) + 1068 engine + 2 boot.
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0. **Baseline 1172** in `zm-tests.yml`.

## What landed -- S1 COMPLETE (all in `Source/Data/`)

types (#147) / species roster+stats+learnsets (#148/#149/#151) / moves (#150) / items (#152) / abilities+natures (#153) / StatCalc+RNG (#154) / CI-policy docs (#155) / WorldSpec skeleton (#156) / **DataRegistry (#157, this session, closes S1)**. S1 gate MET (102 ZM_Data tests vs ~90 target; no visual check).

**In flight (auto-merging, stacked in order):** #155 (policy) -> #156 (worldspec) -> #157 (dataregistry). All locally gated green; auto-merge lands each when its `zm-tests` passes.

## Current task

None. **Next Roadmap task: S2 box 1 -- `ZM_BattleState` + `ZM_BattleEngine`** (the battle engine, a NEW STAGE). S2 is the biggest suite (~370 unit tests), all headless + seeded (uses `ZM_BattleRNG`). First box: `ZM_BattleState` (teams / active monster / field state) + `ZM_BattleEngine` (`Begin(config,seed)` -> `SubmitAction` -> `ResolveTurn()` emitting an append-only `ZM_BattleEvent` stream; NO UI/string formatting in the engine -- ZM-D-010). Then `ZM_MoveExecutor` (one switch over `ZM_MOVE_EFFECT`, now that the data exists), `ZM_DamageCalc`/`ZM_CatchCalc`/`ZM_StatusLogic`, abilities (wire the `ZM_ABILITY_HOOK` bodies), weather, exp/level/evolution, AI tiers, breeding, tower. See Roadmap S2 + MasterPlan.
- **S2 has NO visual gate** (headless battle logic), so the loop keeps running through S2 autonomously. First visual gate is **S3** (overworld terrain/grass/camera).

## Notes for the next agent

- **This is the S1->S2 boundary.** The whole S1 data core (Source/Data/ZM_*.{h,cpp}) is the foundation S2 consumes: `ZM_SpeciesData`+`ZM_StatCalc`+natures/IVs/EVs build a monster's stats; `ZM_MoveData`+`ZM_MOVE_EFFECT` drive `ZM_MoveExecutor`; `ZM_AbilityData` hook bitmask tells you which hooks to wire; `ZM_BattleRNG` is the seeded RNG; `ZM_ItemData` for held items/catch. **Data now HAS its executor (S2).**
- **BUMP THE zm-tests BASELINE** in the SAME PR whenever ZM_* unit tests change. Currently **1172**.
- **S2 build tips:** the battle engine is pure headless C++ (ZM-D-010), emits `ZM_BattleEvent` (append-only), deterministic from seed -> scripted seeded scenarios assert EXACT event streams (characterization bedrock) + a 2000-battle fuzz soak (termination <500 turns, HP/PP/boost invariants). Prototype expected event streams offline (like the stat/PCG32 golden vectors) before building.
- **S1 patterns to reuse:** big tables + golden vectors prototyped/validated in scratchpad Python before building; derived placeholders vs exact tables; every effect kind/hook coverage-locked. `ZM_STAT` in `ZM_SpeciesData.h`.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- `Set-Location C:\dev\Zenith` before regen/build. Tracked `Tools/**/__pycache__/*.pyc` drift on build -- `git checkout --` them; never stage.
- Editing existing files needs NO regen; only NEW files do (`Build\regen.ps1`). Branch fresh off master.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
- Session discipline: replace this file each session end; tick Roadmap boxes only when merged + green; DecisionLog append-only; serial MSBuild; auto-merge (don't wait on CI).
