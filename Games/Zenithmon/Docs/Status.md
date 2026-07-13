# Zenithmon Status

**Last updated:** 2026-07-13 -- **S3's three-real-recipe terrain-bake measurement is COMPLETE locally and fully gated (ZM-D-054).** Calibrated same-harness cold walls are Dawnmere **59.035 s**, Thornacre **69.979 s**, and Route1 **80.804 s**; the all-warm boot is **16.874 s** with zero terrain recipes queued. Their ignored terrain families total **896 chunks / 2,700 files / 672,354,172 bytes**. The 11-town + 14-route planning model projects **24,676 files / 5,933,328,436 bytes**, conservative repeated-process **30m 40.833s**, and one-boot/net **23m 55.857s**; the exact-GDD 26-outdoor sensitivity remains recorded separately. The five-config build matrix is green and the boot gate is **1737 ran / 1736 passed / 0 failed / 1 skipped**. **CURRENT TASK: `ZM_PlayerController` (Jolt capsule, velocity-driven) + `ZM_FollowCamera` + `ZM_InputActions`.**

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. E1/E2 terrain contracts are **ZM-D-051/052**; Dawnmere's terrain/grass contract is **ZM-D-053**; the completed three-recipe measurement and projection are **ZM-D-054**.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Orchestrated-session model (this session)

Development is running under [OrchestratorPlaybook.md](OrchestratorPlaybook.md): the primary instance orchestrates spec extraction, implementation, tests, and adversarial review through subagents, while it alone owns builds/tests/runs, living docs, and the direct-master commit/push. Integration and gates remain serial.

## Build / Tests (all GREEN this iteration)

- Regeneration and the default Tools-enabled Vulkan build are GREEN with the three-recipe registry and five new tests included.
- Build matrix GREEN: all four Vulkan Debug/Release x Tools true/false configurations plus `D3D12_vs2022_Debug_Win64_False`.
- Unit boot gate: **1737 ran, 1736 passed, 0 failed, 1 skipped** (the skip is the pre-existing quarantined engine `RegistryWideNodeRoundTrip`). The measurement registry adds exactly **5** `ZM_TerrainRecipeSet` units; Zenithmon workflow baseline 1732 -> 1737. The shared engine default remains 1078.
- Zenithmon automated P1 GREEN: **2/2** headless outcomes with no terrain mutation (`ZM_Boot_Test` passes; graphics-required `ZM_GrassRegeneration_Test` skips as designed). Windowed grass regeneration still observes exactly 200,159 blades from 5,133 triangles on both Dawnmere loads.
- All three serial selected-force cold bakes exited 0 and published validated markers. The all-warm boot reported warm mask `0x7`, queue mask `0x0`, and queued zero terrain recipes.
- Prior engine blast-radius gates remain GREEN: CityBuilder **45/45**, DevilsPlayground **158/158**, and RenderTest windowed `EngineBootShutdownSmoke` + `TerrainEditorSmoke`.

## Completed locally this iteration -- S3 three-real-recipe terrain measurement

`ZM_TerrainAuthoring` now exposes a fixed WorldSpec-driven registry for Dawnmere and Thornacre (16x16 towns, 256 chunks each) plus Route1 (16x24 route, 384 chunks), with distinct deterministic recipes and exact selected-force/auto-missing queue policy. Their family counts are 772 / 772 / 1,156 files and their measured sizes are 204,684,116 / 204,684,116 / 262,985,940 bytes. Internal terminal-action timers were 42.588 / 53.657 / 64.541 seconds. Only Dawnmere warm-authors a preview scene and owns the existing grass lifecycle; Thornacre and Route1 do not yet have playable scenes, trees, dressing, or traversal content.

The 11-town + 15-route GDD sensitivity projects **25,832 files / 6,196,314,376 bytes**, conservative repeated-process **32m 01.637s**, and one-boot/net **24m 59.787s**. The 30-50 minute budget covers the future all-assets bake, not terrain alone; other generators remain unmeasured, no byte cap exists, warm "seconds" is qualitative, and the single route sample is a planning input rather than a statistical upper bound. ZM-D-053's first Dawnmere timings (**63.671 s** cold / **14.614 s** warm graphics) remain historical; the calibrated 59.035-second rerun belongs to ZM-D-054.

## Current task

**Next unchecked Roadmap task: `ZM_PlayerController` (Jolt capsule, velocity-driven) + `ZM_FollowCamera` + `ZM_InputActions`.**

- **S7 forward note (ZM-D-048/050):** when party/daycare save-load lands at S7, its serializer MUST include `m_eGender`; the SC-B/SC-C additions are all DERIVED accessors (no new stored fields to serialize). Nothing serializes these structs today -> no current regression.

---

**S3 -- First overworld (critical path) is active.** Read MasterPlan.md for terrain authoring and bake-risk analysis. Remaining task order (first-unchecked-first):
1. `ZM_PlayerController` (Jolt capsule, velocity-driven) + `ZM_FollowCamera` + `ZM_InputActions`.
2. Persistent `ZM_GameStateManager` + `ZM_WarpTrigger`/`ZM_SpawnPoint` spawn-tag respawn.
3. Player home interior + door warp round trip (SINGLE loads + fade).

Two standing S3 constraints:
- **E1/E2 and the three-recipe measurement are complete and gated.** New terrain authoring must use the named-set, bounded-export, atomic warm-marker, and registry selection contracts; never commit baked terrain or scene artifacts. The three measurement recipes intentionally contain no trees, and only Dawnmere currently owns a preview scene.
- **The S3 gate HAS a visual check** ("windowed automated test walks the village + door round trip; visual check terrain/grass/camera"). The user's standing hard-stop-at-visual-gates order RESUMES: the S3-gate iteration captures windowed screenshot evidence, writes `GATE-WAIT: S3 visual sign-off` into this file with the evidence paths, commits + pushes the docs, and ENDS for the user's sign-off (StartPrompts.md prompt 4). It does NOT self-sign a visual gate.

## Notes for the next agent

- **Box 6 is DONE + PUSHED (both SCs):** ZM-D-045 (breeding/daycare) + ZM-D-046 (Battle Tower logic). All S2 battle-logic boxes 1-6 complete. `ZM_BattleTower` is the first real consumer of box-5 `ZM_AI_TIER`/`ZM_ChooseAction`.
- **Future-integration note (Q-2026-07-12-005):** `ZM_ChooseAction` returns a MOVE for a FAINTED active (it enumerates the fainted active's moves). When the S5+ battle integration wires the enemy side to the AI, the caller must submit the forced SWITCH itself on a faint (or box 5's chooser should later special-case a fainted active). The box-6 tower engine smokes side-step this with a controlled 1v1. NOT a bug today (the tower never runs battles).
- **Open rulings awaiting user ratification (all best-guessed + proceeded, all additive):** Q-2026-07-12-005 (tower tuning + fainted-active note), Q-2026-07-12-003 (AI file location + no-Struggle + tunable thresholds), and Q-2026-07-12-002 (exp/level move-overflow + evolution triggers). Q-2026-07-12-004 was resolved by the user-directed feature-complete breeding expansion. Plus Q-2026-07-12-001 (a `ZM_ValidateEventStream` rule-7 test-helper widening).
- **Test-coverage gate is a hard merge bar (user mandate):** every new system/method gets dedicated behavioral tests; for stochastic logic, paired identical-seed positive+control PLUS an INDEPENDENT offline oracle where goldens matter (the box-6 breeding + tower team-gen oracles are the model). The adversarial review catches false-confidence tests -- keep it.
- **BUMP the zm-tests baseline** in the SAME commit whenever boot unit tests change (now **1737**, in `.github/workflows/zm-tests.yml`; exact-match gate). Engine-test additions also bump the shared engine default (currently **1078** in `Tools/run_unit_gate.ps1`).
- **DLL-heal gotcha:** Zenithmon's FIRST build/run in a fresh config (or fresh session) leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, no "Unit tests complete" line). `zenith test` heals automatically; the BARE unit gate does NOT. Fix: run `zenith test ... --headless` once first (it heals), OR `Import-Module <ABSOLUTE>\Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>` (a bare relative `Build\...` module path fails to resolve).
- **Unit-gate timeout is normal:** the gate boots the exe, logs "Unit tests complete: N ran...", then watchdog-kills the windowed idle after 180s. `PASS`/exit 0 with the units line present = green; the "timeout after 180s; killing" line is expected. If the units line is ABSENT, a production `Zenith_Assert` aborted the boot (read `Build/artifacts/unit_gate_boot.log` -- e.g. SC2's "SubmitAction: active monster is fainted" was a test driving the engine past a faint).
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot. Set the bit AND its counter together.
- **LOCKED battle contracts (ZM-D-032..046):** `ZM_BattleEvent` POD append-only; RNG draw order fixed; inactive mechanics draw/emit/mutate ZERO; EoT order fixed; progression order locked (ZM-D-043); `ZM_BattleAI` is a pure non-perturbing chooser (ZM-D-044); `ZM_Breeding` inheritance draw order locked (ZM-D-045); `ZM_BattleTower` team-gen draw order + streak/tier rules locked (ZM-D-046). New systems APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Orchestrator invariants (OrchestratorPlaybook.md):** only the orchestrator builds/tests/runs; subagents author on DISJOINT file scopes and never claim "builds clean"; the orchestrator alone writes the living docs and owns the direct-master commit/push; parallel authoring is fine on non-overlapping files, but integrate + build SERIALLY.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes (incl. data-model expansions like gender) need a user DecisionLog entry FIRST.
