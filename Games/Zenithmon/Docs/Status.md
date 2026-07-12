# Zenithmon Status

**Last updated:** 2026-07-12 -- **S3 Engine E1 is COMPLETE locally and fully gated; this change is being committed and pushed directly to master.** `Zenith_TerrainComponent` now owns a serialized, strictly validated terrain-set name (v4; empty keeps the exact legacy layout), all six engine path consumers and the affected CityBuilder state consumers use the resolved directory, and the editor/automation support staged safe-target bakes (ZM-D-051). The complete five-leg build matrix is green; the boot gate is **1725 ran / 1724 passed / 0 failed / 1 skipped**; Zenithmon, CityBuilder, DevilsPlayground, and both RenderTest windowed regressions pass. **CURRENT TASK after this commit is S3 Engine E2 -- rect-bounded chunk export and missing-chunk stream tolerance.**

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. E1's terrain-set contract is **ZM-D-051**.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Orchestrated-session model (this session)

Development is running under [OrchestratorPlaybook.md](OrchestratorPlaybook.md): the primary instance orchestrates spec extraction, implementation, tests, and adversarial review through subagents, while it alone owns builds/tests/runs, living docs, and the direct-master commit/push. E1 used separate planner, runtime, editor, test-author, fix-planner, and reviewer roles; implementation and tests were then integrated and gated serially.

## Build / Tests (all GREEN this iteration)

- No regeneration was required: E1 adds no source files.
- Build matrix GREEN: all four Vulkan Debug/Release x Tools true/false configurations plus `D3D12_vs2022_Debug_Win64_False`.
- Unit boot gate: **1725 ran, 1724 passed, 0 failed, 1 skipped** (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). E1 adds exactly **7** engine tests; Zenithmon workflow baseline 1718 -> 1725 and engine default baseline 1068 -> 1075.
- Zenithmon automated P1 GREEN: **1/1** (`zenith test Zenithmon --headless`).
- Engine blast-radius gates GREEN: CityBuilder **45/45**, DevilsPlayground **158 tests**, and RenderTest windowed `EngineBootShutdownSmoke` + `TerrainEditorSmoke`.
- Adversarial review GREEN after fixes for dirty-session resume, transactional automation rejection, UI draft scoping, and canonical symlink/junction-safe cleanup.

## Completed locally this iteration -- S3 Engine E1 (terrain asset sets)

`Zenith_TerrainComponent` serialization v4 appends a strict terrain-set name: empty keeps legacy `Terrain/`; named sets resolve beneath `Terrain/<Set>/`. Runtime rendering, collision, regeneration, low/high-LOD streaming, editor baking, and CityBuilder state consumers use the resolved directory. The editor stages target changes and commits only at successful full-bake start, resumes same-target dirty sessions, and constrains destructive cleanup to canonical direct `.zmesh` children. `AddStep_TerrainSetAssetSet` safely owns, validates, preflights, and persists its argument. Full contract: ZM-D-051.

## Current task

**Next unchecked Roadmap task: S3 Engine E2 -- `AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` plus verified missing-chunk tolerance in `Flux_TerrainStreamingManager`. E1 is complete under ZM-D-051.**

- **S7 forward note (ZM-D-048/050):** when party/daycare save-load lands at S7, its serializer MUST include `m_eGender`; the SC-B/SC-C additions are all DERIVED accessors (no new stored fields to serialize). Nothing serializes these structs today -> no current regression.

---

**S3 -- First overworld (critical path) is active.** Read MasterPlan.md for the E2 detail and terrain-bake risk analysis. Remaining task order (first-unchecked-first):
1. **Engine E2** -- `AddStep_TerrainExportChunksRect(minX,minY,maxX,maxY)` + missing-chunk stream tolerance on `Flux_TerrainStreamingManager`.
2. Home Village terrain baked via a `ZM_TerrainAuthoring` recipe; grass regenerated OnAwake.
3. **Measure terrain bake time** with 3 real scenes BEFORE committing to ~25 terrains (Q-2026-07-09-002).
4. `ZM_PlayerController` (Jolt capsule) + `ZM_FollowCamera` + `ZM_InputActions`.
5. Persistent `ZM_GameStateManager` + `ZM_WarpTrigger`/`ZM_SpawnPoint` spawn-tag respawn.
6. Player home interior + door warp round trip (SINGLE loads + fade).

Two standing S3 constraints:
- **E2 is an ENGINE change** -- per OrchestratorPlaybook.md section 4 it needs engine unit tests, RenderTest regressions, and the DP/CityBuilder suites green before the direct-master commit, not only the Zenithmon gate.
- **The S3 gate HAS a visual check** ("windowed automated test walks the village + door round trip; visual check terrain/grass/camera"). The user's standing hard-stop-at-visual-gates order RESUMES: the S3-gate iteration captures windowed screenshot evidence, writes `GATE-WAIT: S3 visual sign-off` into this file with the evidence paths, commits + pushes the docs, and ENDS for the user's sign-off (StartPrompts.md prompt 4). It does NOT self-sign a visual gate.

## Notes for the next agent

- **Box 6 is DONE + PUSHED (both SCs):** ZM-D-045 (breeding/daycare) + ZM-D-046 (Battle Tower logic). All S2 battle-logic boxes 1-6 complete. `ZM_BattleTower` is the first real consumer of box-5 `ZM_AI_TIER`/`ZM_ChooseAction`.
- **Future-integration note (Q-2026-07-12-005):** `ZM_ChooseAction` returns a MOVE for a FAINTED active (it enumerates the fainted active's moves). When the S5+ battle integration wires the enemy side to the AI, the caller must submit the forced SWITCH itself on a faint (or box 5's chooser should later special-case a fainted active). The box-6 tower engine smokes side-step this with a controlled 1v1. NOT a bug today (the tower never runs battles).
- **Open rulings awaiting user ratification (all best-guessed + proceeded, all additive):** Q-2026-07-12-005 (tower tuning + fainted-active note), Q-2026-07-12-003 (AI file location + no-Struggle + tunable thresholds), and Q-2026-07-12-002 (exp/level move-overflow + evolution triggers). Q-2026-07-12-004 was resolved by the user-directed feature-complete breeding expansion. Plus Q-2026-07-12-001 (a `ZM_ValidateEventStream` rule-7 test-helper widening).
- **Test-coverage gate is a hard merge bar (user mandate):** every new system/method gets dedicated behavioral tests; for stochastic logic, paired identical-seed positive+control PLUS an INDEPENDENT offline oracle where goldens matter (the box-6 breeding + tower team-gen oracles are the model). The adversarial review catches false-confidence tests -- keep it.
- **BUMP the zm-tests baseline** in the SAME commit whenever boot unit tests change (now **1725**, in `.github/workflows/zm-tests.yml`; exact-match gate). Engine-test additions also bump the shared engine default (now **1075** in `Tools/run_unit_gate.ps1`).
- **DLL-heal gotcha:** Zenithmon's FIRST build/run in a fresh config (or fresh session) leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, no "Unit tests complete" line). `zenith test` heals automatically; the BARE unit gate does NOT. Fix: run `zenith test ... --headless` once first (it heals), OR `Import-Module <ABSOLUTE>\Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>` (a bare relative `Build\...` module path fails to resolve).
- **Unit-gate timeout is normal:** the gate boots the exe, logs "Unit tests complete: N ran...", then watchdog-kills the windowed idle after 180s. `PASS`/exit 0 with the units line present = green; the "timeout after 180s; killing" line is expected. If the units line is ABSENT, a production `Zenith_Assert` aborted the boot (read `Build/artifacts/unit_gate_boot.log` -- e.g. SC2's "SubmitAction: active monster is fainted" was a test driving the engine past a faint).
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot. Set the bit AND its counter together.
- **LOCKED battle contracts (ZM-D-032..046):** `ZM_BattleEvent` POD append-only; RNG draw order fixed; inactive mechanics draw/emit/mutate ZERO; EoT order fixed; progression order locked (ZM-D-043); `ZM_BattleAI` is a pure non-perturbing chooser (ZM-D-044); `ZM_Breeding` inheritance draw order locked (ZM-D-045); `ZM_BattleTower` team-gen draw order + streak/tier rules locked (ZM-D-046). New systems APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Orchestrator invariants (OrchestratorPlaybook.md):** only the orchestrator builds/tests/runs; subagents author on DISJOINT file scopes and never claim "builds clean"; the orchestrator alone writes the living docs and owns the direct-master commit/push; parallel authoring is fine on non-overlapping files, but integrate + build SERIALLY.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes (incl. data-model expansions like gender) need a user DecisionLog entry FIRST.
