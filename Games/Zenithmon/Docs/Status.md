# Zenithmon Status

**Last updated:** 2026-07-11 -- **S2 box 3 IN PROGRESS at SC1/5. SC1 (weather core) is landing this commit: weather damage multiplier (rain/sun x water/fire via the `uWeatherNum/Den` seam), end-of-turn sand/snow chip (`maxHP/8` min 1, resolved FIRST, immune types), weather + screen countdown/expiry, `WEATHER_CHANGED`/`WEATHER_DAMAGE`/`SCREEN_SET`/`SCREEN_EXPIRED` events, and the shared `ZM_BattleMonsterHasType`. No abilities yet. Next: S2 box 3 SC2 (ability hook fn-pointer infra + SWITCH_IN abilities).**

**Read this first each session.** Replaced every session end. [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. The box-3 execution plan + design rulings are **ZM-D-036** in [DecisionLog.md](DecisionLog.md).

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Build / Tests

- Regen check GREEN (SC1 added NO new files -- edits only; no regen needed).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit (T0): **1428 ran, 1427 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). = 102 `ZM_Data` + **256 `ZM_Battle`** (228 box-1/2 + **28 SC1 weather**) + 1068 engine + 2 boot. **Baseline 1428** in `zm-tests.yml` (bumped this commit).
- Automated (P1): 1/1 (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.

## What landed this session -- S2 box-3 SC1 (weather core)

Ran a design panel (6-way survey -> synth -> adversarial critique, all resolved) that produced the 5-sub-commit box-3 plan (ZM-D-036). SC1 implements weather with ZERO abilities and ZERO new RNG draws: the damage multiplier rides the existing `uWeatherNum/uWeatherDen` seam in `ApplyDamagingHit` (RAIN water x3/2 fire x1/2; SUN inverse; SAND/SNOW/NONE 1/1); `ResolveWeatherEndOfTurn` (new, wired FIRST in `ResolveEndOfTurnPhase`) does the SAND/SNOW chip (`maxHP/8` min 1, PLAYER-active then ENEMY-active, immune = EARTH/STONE/IRON for sand / ICE for snow, underflow-clamped, `WEATHER_DAMAGE` [+`FAINT`]) then weather countdown/expiry; `ResolveScreenEndOfTurn` (new) does the screen countdown + `SCREEN_EXPIRED`; `g_ApplyField` now emits `WEATHER_CHANGED` (with prev-weather in `m_iTag`) + `SCREEN_SET`. New shared `ZM_BattleMonsterHasType`. Ability damage mults are deferred to SC3 and will apply OUTSIDE `ZM_CalcDamage` so its pure-fn goldens never move. Verified via blind parallel test/impl authoring + a 2-lens adversarial review (impl-correctness: clean on 7 axes; test-quality: SOLID, non-tautological -- the 5 coverage gaps it found were closed before commit). 4 box-2 tests tightened (the only sanctioned golden churn).

## Current task

**Next per [Roadmap.md](Roadmap.md) S2 box 3: SC2 -- ability hook fn-pointer infrastructure + SWITCH_IN abilities.** New files `Source/Battle/ZM_AbilityHooks.h/.cpp` (context, 12-pfn hook struct over the 11-bit mask, `const` table keyed by `ZM_ABILITY_ID`, `ZM_GetAbilityHooks`); promote `g_ApplyStatChange` to a shared `ZM_StatusLogic::ApplyStatChange` (state+events overload) with the `bFromFoe` stat-drop veto seam; switch-in dispatch in `Begin`+`DoSwitch`; PressureAura 2-PP; abilities RAINCALLER/SUNCALLER/SANDCALLER/SNOWCALLER/DAUNTINGROAR/PRESSUREAURA. **NEW FILES => run `Build\regen.ps1` before building.** The `*CALLER` weather-setters MUST add the `if weather != target` anti-refresh guard (design section 5.3) that the move-setter path deliberately omits. The Roadmap box-3 line stays UNCHECKED until SC5 lands.

## Notes for the next agent

- **Design archive:** the full box-3 design (all 50 ability bodies + constants, hook struct, dispatch points, per-SC test lists) lives in the session scratchpad `zm_s2_box3_design.md` -- if starting cold and it's gone, ZM-D-036 + the Roadmap S2 line + this file carry enough to resume; re-derive per-SC detail from `ZM_AbilityData.{h,cpp}` (the 50-row table + `m_uHookMask`) + `GameDesignDocument.md`.
- **Coverage invariant (SC2 test):** for every ability id, each set `m_uHookMask` bit must be realized by a live pfn slot OR a documented engine-side handler (NOT "every bit has a pfn") -- QUICKDRAW/DAUNTINGROAR/BLOODRUSH realize MODIFY_STAT engine-side; the weather bit is a condition, not a pfn (ZM-D-036).
- **DLL-heal gotcha:** Zenithmon's FIRST build in a fresh config leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, "no 'Unit tests complete' line"). Fix: `Import-Module Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>`. `zenith test` heals automatically; the bare unit gate does NOT. (Did not recur this session -- config was warm.)
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot (looks like the DLL crash). Set the bit AND its counter together.
- **BUMP the zm-tests baseline** in the SAME commit whenever ZM_* unit tests change (now **1428**).
- **Box-2/3 LOCKED contracts (ZM-D-032..036):** `ZM_BattleEvent` POD is append-only (new kinds append before COUNT, new fields default 0); RNG draw order fixed (PHASE G/M/E); a NONE-ability/weather-NONE/status-free actor draws ZERO and emits ZERO; EoT order = weather chip FIRST -> weather/screen countdown -> PLAYER-then-ENEMY status ticks -> (SC5) ability TURN_END heals (skip fainted) -> TURN_END. Later SCs APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes need a user DecisionLog entry FIRST.
