# Zenithmon Status

**Last updated:** 2026-07-12 -- **S2 box 6 SC1 (`ZM_Breeding` + `ZM_Daycare`) is COMPLETE, committed, and pushed to master.** Pure, deterministic, headless breeding: egg generation (offspring = base-evo of the mother; K-IV inheritance [3, or 5 with Heirloom Knot]; Stasis-Stone nature lock; mother ability; base-evo L1 learnset) + a daycare deposit/level-by-steps/egg-availability model (256-step threshold). A reduced model on the shipped data model (archetype = egg-group proxy; no gender / egg-groups / egg-moves -- Q-2026-07-12-004). Contract: **ZM-D-045**. Next: **S2 box 6 SC2 -- `ZM_BattleTower`** (level-50 clamp, rentals, streak scaling, AI escalation -- consumes `ZM_BattleAI`), then the automated S2 stage gate.

**Read this first each session.** [Roadmap.md](Roadmap.md) is the source of truth for what's next; [Questions.md](Questions.md) holds open decisions; [Shortfalls.md](Shortfalls.md) is the gap audit. Box 6 SC1's breeding contract is **ZM-D-045**; box 5 is ZM-D-044; box 4 is ZM-D-043.

## Working model -- MASTER-ONLY, no branches/PRs (2026-07-10, ZM-D-031)

**All work is committed DIRECTLY to `master` and pushed** (`git push origin master`). NEVER create a feature branch, pull request, or git worktree. The **LOCAL gate is the pre-push authority**: `zenith build` + boot unit gate (`Tools/run_unit_gate.ps1 -Exe <exe> -Baseline N`, runs the ZM_* unit tests `zenith test` skips) + `zenith test --headless`, ALL green before you push. `zm-tests` CI runs post-push as a backstop only; on red, FIX FORWARD (never revert shipped history, force-push, or `gh run rerun`). See StartPrompts.md.

## Orchestrated-session model (this session)

Development is running under [OrchestratorPlaybook.md](OrchestratorPlaybook.md): the primary Claude instance is the ORCHESTRATOR -- it dispatches subagents for spec-extraction / code / test / review authoring but **owns ALL builds/tests/runs itself** (subagents never build; their "it compiles" claims are worthless), and it **alone writes the living docs** and owns the direct-master commit/push. Boxes 4, 5, and box-6 SC1 all landed this way. Per-box pattern that keeps working: (1) a spec-extraction subagent reads the seams + design docs and writes a complete self-contained `<box>_spec.md` to the orchestrator's scratchpad, returning a tight digest; (2) an Implementer and a Test Author author in PARALLEL on DISJOINT files from that one spec (the pinned public API + an offline oracle in the spec keep the parallel seam consistent); (3) an adversarial Reviewer traces the diff; (4) the orchestrator builds, runs the unit gate + headless, applies doc updates, commits + pushes. Invariant 4 keeps earning its keep -- the SC1 test TU had a call-site arity typo the Test Author couldn't have caught (subagents don't build); the orchestrator build caught it and a scoped fix-round cleared it.

## Build / Tests (all GREEN this iteration)

- Regen GREEN (required for four new production files `ZM_Breeding.{h,cpp}` + `ZM_Daycare.{h,cpp}` + one new test TU `ZM_Tests_Breeding.cpp`).
- Build GREEN (`Vulkan_vs2022_Debug_Win64_True`).
- Unit boot gate: **1638 ran, 1637 passed, 0 failed, 1 skipped** (skip = pre-existing quarantined engine `RegistryWideNodeRoundTrip`). Box 6 SC1 added **33** `ZM_Data` tests; workflow baseline bumped **1605 -> 1638**.
- Automated P1: **1/1** (`ZM_Boot_Test`); `zenith test Zenithmon --headless` exits 0.
- No windowed or visual check applies to this headless box.
- Adversarial review GREEN (SAFE-TO-COMMIT): the independent PCG32 oracle guards the inheritance draw order; base-evolution sentinel safety (`ZM_SPECIES_NONE == ZM_SPECIES_COUNT`) and daycare `u_int64` saturation verified. One minor non-blocking coverage note logged in ZM-D-045 (no test steps a compatible pair strictly PAST 256 in a single call; the `>=` branch is covered by exact-256 steps).

## Completed locally this iteration -- S2 box 6 SC1 (`ZM_Breeding` + `ZM_Daycare`)

`Source/Battle/ZM_Breeding.{h,cpp}` + `ZM_Daycare.{h,cpp}` supply pure, deterministic, headless free functions (no globals/UI/overworld). `ZM_GenerateEgg` derives the mother's base evolution, inherits K IVs (3, or 5 with Heirloom Knot) then rolls nature AFTER the IVs (Stasis-Stone locks it without a draw), copies the mother's ability, and fills the base-evo level-1 learnset. Compatibility = both non-legendary AND same archetype. `ZM_Daycare*` deposits up to 2, grants 1 exp/step (level via `ZM_ExpAndLevel`, capped L100), and makes an egg available at the 256-step threshold for a compatible pair. Full contract + locked RNG draw order: ZM-D-045. Reduced-data rulings (no gender / egg-groups / egg-moves): Q-2026-07-12-004.

## Current task

**Next per [Roadmap.md](Roadmap.md) S2: box 6 SC2 -- `ZM_BattleTower` logic (level-50 clamp, rentals, streak scaling, AI escalation).** The Battle Tower AI-escalation tiers map to `ZM_AI_TIER` (RANDOM->GREEDY->SMART->CHAMPION); the tower is LOGIC only this box (the tower SCENE is S11). Suggested approach: a spec-extraction pass pinning the level-50 clamp rule (how a party is normalized to L50 for tower battles), the rental-set model, the streak-scaling + AI-escalation schedule, and any reward table, against Scope.md + the GDD; then disjoint Implementer/Test Author scopes, adversarial review, and the orchestrator-owned serial gate.

After SC2, S2 has its automated stage gate (~370+ unit tests incl. seeded scenario battles with exact expected event streams + a 2,000-battle fuzz soak). Roadmap S2 lists no windowed or visual check, so no `GATE-WAIT` is expected for SC2 or the S2 gate unless the binding gate docs change.

## Notes for the next agent

- **Box 6 SC1 is DONE + PUSHED:** ZM-D-045 locks the reduced deterministic breeding/daycare model on the shipped data. Three reduced-data rulings are logged in Q-2026-07-12-004 (no gender; archetype = egg-group proxy; no egg-moves/hidden-abilities/Ditto/Masuda) -- best-guessed + proceeded; each is additive if the data model later grows, and adding gender/egg-groups is a Scope.md data-model EXPANSION (own scoped item + DecisionLog entry first).
- **Box 5 is DONE:** ZM-D-044 locks the pure, non-perturbing four-tier `ZM_BattleAI` chooser (`ZM_ChooseAction`). The engine does NOT yet call it (opponent-side wiring is a later battle-integration change, S5+). `ZM_BattleTower` (SC2) is the first real consumer of `ZM_AI_TIER`.
- **Box 4 is DONE:** ZM-D-043 locks default-off, player-default progression; per-opponent participation; current cumulative EXP; exact-once settlement; terminal one-edge level evolution.
- **Open follow-up (Q-2026-07-12-001):** `ZM_ValidateEventStream` rule 7 doesn't recognize `WEATHER_DAMAGE`->`FAINT` or ability-chip(`ABILITY_TRIGGER`)->`FAINT` as valid pre-FAINT damage sources -- a PRE-EXISTING test-helper gap; full-battle tests with weather/ability-chip KOs bypass the validator. A good small standalone commit before the content stages.
- **Test-coverage gate is a hard merge bar (user mandate):** every new system/method gets dedicated behavioral tests (for stochastic logic, paired identical-seed positive+control + an INDEPENDENT offline oracle where goldens matter -- the box-6 PCG32 oracle is the model), not just a smoke test. The adversarial review catches false-confidence tests -- keep it for logic-heavy work.
- **BUMP the zm-tests baseline** in the SAME commit whenever `ZM_*` unit tests change (now **1638**, in `.github/workflows/zm-tests.yml`; exact-match gate).
- **DLL-heal gotcha:** Zenithmon's FIRST build/run in a fresh config (or a fresh session) leaves middleware DLLs absent -> exe dies at load (exit `0xC0000135`, empty boot log, no "Unit tests complete" line -- looks like a crash). `zenith test` heals automatically; the BARE unit gate does NOT. Fix: run `zenith test ... --headless` once first (it heals), OR `Import-Module <ABSOLUTE>\Build\zenith_buildsystem.psm1; Repair-ZenithRuntimeDlls -ExeDir <outdir>` (a bare relative `Build\...` module path fails to resolve).
- **Unit-gate timeout is normal:** the gate boots the exe, logs "Unit tests complete: N ran...", then watchdog-kills the windowed idle after 180s. `PASS`/exit 0 with the units line present = green; the "timeout after 180s; killing" line is expected.
- **Invalid-volatile-state gotcha:** setting a volatile BIT without its counter trips a production `Zenith_Assert` at end-of-turn that ABORTS the whole unit boot. Set the bit AND its counter together.
- **LOCKED battle contracts (ZM-D-032..045):** `ZM_BattleEvent` POD append-only; RNG draw order fixed; inactive mechanics draw/emit/mutate ZERO; EoT order fixed; progression order locked (ZM-D-043); `ZM_BattleAI` is a pure non-perturbing chooser (ZM-D-044); `ZM_Breeding` inheritance RNG draw order is locked (ZM-D-045). New systems APPEND so earlier goldens never shift.
- **Working-dir gotcha:** Bash + PowerShell SHARE a cwd -- run regen/build from `C:\dev\Zenith`. Editing existing files needs NO regen; only NEW files do.
- **Orchestrator invariants (OrchestratorPlaybook.md):** only the orchestrator builds/tests/runs; subagents author on DISJOINT file scopes and never claim "builds clean"; the orchestrator alone writes the living docs and owns the direct-master commit/push; parallel authoring is fine on non-overlapping files, but integrate + build SERIALLY.
- **Hard rules (Scope.md):** `ZM_` prefix; original names / zero Nintendo IP; data = compiled C arrays; no audio/networking/Dynamax; singles only; baked assets git-ignored. Scope changes (incl. data-model expansions like gender) need a user DecisionLog entry FIRST.
