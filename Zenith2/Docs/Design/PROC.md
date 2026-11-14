# PROC design — process and gates

This is the entry document of the PROC (Process & gates) design family. PROC owns every protected harness, configuration and gate edit of the Zenith2 programme, each delivered as a `needs-human` ticket that names the file and the change. This entry document states the scope, the dependencies, the full file layout, the public interfaces of the gate tooling, the common contracts every gate step shares, the error contract, the test strategy, the budget rows, the stage plan, the decisions and the requirement index. The split files hold the rest.

## Purpose and scope

### What PROC owns

| Concern | What PROC delivers | Where it is specified |
|---|---|---|
| the Z2 gate list | every line of every Z2 category's `gates` and `measureSteps` arrays in `zagent.project.json`, as it grows from the hand-over list through Stages 2, 3, 5 and 7, each change one `needs-human` PROC ticket | `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage" |
| the protected gate scripts | every file under `Zenith2/Tools/Gates/`: the step scripts, the shared module, the policy and allowlist files, the baselines and every self-test with its fixtures | `Zenith2/Docs/Design/PROC/BaseSteps.md`, `Zenith2/Docs/Design/PROC/StructuralGates.md`, `Zenith2/Docs/Design/PROC/ExtendedGates.md` |
| the Zenith-side generic tooling | the `tick_gates.ps1` external-build mode, the Vault pass-through, PROC-API, `reviewerConventions`, `postFinish`, the per-project lane provisioning hook, the rulings-matcher citation weight, the analyzer keys, the scanner audit, the libgraph CMake File API front end, the root delegation line, the `needs-revalidation` claim predicate, and the N-lanes specification held as an assumption pending flagged proposal P2 | `Zenith2/Docs/Design/PROC/Harness.md` |
| the protected configuration | the guardrail additions in `vault.config.json`, the Z2 category edits in `zagent.project.json`, `.github/workflows/zenith2-gates.yml` and `Zenith2/CLAUDE.md` edits after it becomes protected | `Zenith2/Docs/Design/PROC/Harness.md` → "Guardrail additions", `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket" |
| the stage acceptance and mid-sprint tickets | the eight `Z2 S<N> stage acceptance` tickets and the eight `Z2 S<N> mid-sprint integration check` tickets, with the requirement list and the evidence each collects | `Zenith2/Docs/Design/PROC/Acceptance.md` |
| the human procedures | the A8a docs-and-config commit, the journal archival before A6, the A6 dry run and filing, the A8b seed commit, the A8c coverage commit, the post-seed steps and the Stage 0 start, and every rollback | `Zenith2/Docs/Design/PROC/HumanProcedures.md` |
| the hand-off ledger | the disposition of every PROC hand-off every area recorded: the PROC ticket that carries it, or the ruling or other ticket that answered it | `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger" |

### Exclusions

| Not PROC's | Owner | Why |
|---|---|---|
| the content of `Zenith2/ThirdParty/manifest.json` beyond the Stage 0 deliverable (toolchain pins and the doctest entry), the setup script's design, the CI matrix specification | SHIP (`Zenith2/Docs/Design/SHIP.md` → "Stage 0", "Pin landing order"; `Zenith2/Docs/Design/SHIP/Qualification.md` → "CI matrix") | SHIP specifies; PROC lands the protected or Stage 0 files against SHIP's specification (Z2-D-008, Z2-D-031) |
| every dependency pin after doctest | SHIP's `needs-human` pinning tickets Z2T-SHIP-043 to -062 and CORE's Z2T-CORE-053 | Z2-D-008: `Zenith2/Docs/Design/SHIP.md` → "Pin landing order" absorbed the pin hand-offs COMPRESS-P1, ECS-P1, PLAT-P5, RHI-P2, RPROD-P1, RPROD-P4, SCRIPT-P1, GEOM-P1, ASSET-P3, LIGHT-P2, AUDIO-P1, CORE-P8; the manifest is not a protected path, so CINE-P1 and CINE-P2 are SHIP and CINE work too |
| the scaffold (`CMakeLists.txt`, `CMakePresets.json`, the CMake helpers, the Player stub) | MOD, ticket Z2T-MOD-056 | Z2-D-128; PROC's Stage 0 acceptance proves it |
| the `linux-tsan` host provisioning, the TSan smoke suite, the fuzz campaign runner, the evidence fingerprint module | DEVT (`Zenith2/Docs/Design/DEVT/QualityTools.md` → "The linux-tsan host", "Fuzz campaign runner"; `Zenith2/Docs/Design/DEVT/Evidence.md` → "Zenith2Evidence module") | Z2-D-011, Z2-D-065: PROC's gate scripts call them |
| the shader lint definitions and their allowlist | RPROD and the Rendering core document (`Zenith2/Docs/Rendering/ShaderSystem.md` → "Shader lints") | Z2-D-014: the `ShaderChecks` step only aggregates the lint files |
| the trust-boundary content: which symbols are development services, which parsers are fuzzed and for how long, the confined-API allow records, the conformance checks | SEC, with each owning area | Z2-D-128 gives PROC the protected edits only; PROC owns the mechanisms (see "Coordination with SEC" below) |
| complexity-threshold recalibration and the surface growth review | MAINT ([Z2-N-CONTRACT-419]; `Zenith2/Docs/Design/MAINT/GateData.md`) | Z2-D-007: a recalibration or a re-pin edits a protected file and is therefore `needs-human`, but its evidence and decision belong to MAINT |
| the session filer and the session coverage checker, including their `Kind:` and `Managed-Fields:` trailers for the seed | the documentation session (Z2-D-113, Z2-D-114), changed by the orchestrator | PROC's production filer and checker are their Stage 0 successors; PROC states the trailer order they must emit (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "The managed region") |
| misuse codes | none: PROC has no Luau binding and reserves no misuse-code block (ruling MISUSE-BLOCKS-NEW-AREAS-2 leaves ZS3300 to ZS3399 unassigned) | Z2-D-128 (PROC's work is protected edits, never runtime code) |

### Inventory items covered

| ID | Item | Requirements |
|---|---|---|
| Z2-I-PROC-01 | every protected harness edit, as a `needs-human` ticket | Z2-R-PROC-001, -002, -004 to -020, -079 to -084, -086, -089, -090, -092, -119, -131, -132, -146, -155, -157, -158, -166 |
| Z2-I-PROC-02 | every protected config edit, as a `needs-human` ticket | Z2-R-PROC-002, -003, -024, -025, -065, -078, -079, -085, -087, -090, -091, -095, -099, -104, -106, -107, -122 to -132, -136, -139, -142, -155, -170 |
| Z2-I-PROC-03 | every protected gate edit, as a `needs-human` ticket | Z2-R-PROC-021 to -024, -026 to -064, -066 to -077, -088, -093, -094, -096 to -098, -100 to -103, -105, -106, -108 to -118, -121, -131 to -141, -143 to -154, -156, -159 to -173 |

### Document family

| File | Holds | Headings |
|---|---|---|
| `Zenith2/Docs/Design/PROC.md` | this entry: scope, dependencies, layout, interfaces, common contracts, error contract, tests, budgets, stage plan, decisions, requirement index | the skeleton below |
| `Zenith2/Docs/Design/PROC/GateList.md` | the Z2 gate list by stage, the category JSON after each PROC ticket, the PROC ticket map with Draft-IDs, the hand-off ledger | Scope of this file · Gate list by stage · Category edits by ticket · PROC ticket map · Hand-off ledger |
| `Zenith2/Docs/Design/PROC/BaseSteps.md` | the shared gate module, the common step lifecycle, and the steps Configure, Build, Test, SourceLayout, CompileCoverage, Format, HeaderSelfContained, ApiContractCheck, ReviewSignals, ShaderChecks, EcsConformance, Sweep, Run-Committed and IntegrationSmoke | Scope of this file · Shared gate module · Step lifecycle · Configure · Build · Test · SourceLayout · CompileCoverage · Format · HeaderSelfContained · ApiContractCheck · ReviewSignals · ShaderChecks · EcsConformance · Sweep · Run-Committed · IntegrationSmoke · Gate self-tests |
| `Zenith2/Docs/Design/PROC/StructuralGates.md` | ForbiddenCheck, TestSeparationCheck, Libgraph and Complexity: rules, inputs, outputs, allowlist and policy formats, fixtures | Scope of this file · ForbiddenCheck · ForbiddenCheck rule table · ForbiddenCheck allowlist files · Writable-data scan · ForbiddenCheck fixtures · TestSeparationCheck · Libgraph · Libgraph policy files · Complexity |
| `Zenith2/Docs/Design/PROC/ExtendedGates.md` | Coverage, the production filer, ApkCheck, Sanitizers, BackendParity and PathParity | Scope of this file · Coverage · Coverage rules · Coverage fixtures · Coverage acceptance mode · Production filer · ApkCheck · Sanitizers · Sanitizer path selection · BackendParity · PathParity |
| `Zenith2/Docs/Design/PROC/Harness.md` | every Zenith-side generic change and the three-harness regeneration they force | Scope of this file · Guardrail additions · Harness regeneration · tick_gates external-build mode · Vault pass-through · Analyzer keys · Scanner audit · Root delegation · libgraph CMake front end · PROC-API review records · Reviewer conventions · postFinish · Rulings citation weight · Lane provisioning hook · Z2 lane registration · Revalidation claim predicate · N lanes per project · CI workflow |
| `Zenith2/Docs/Design/PROC/Acceptance.md` | the stage acceptance and mid-sprint ticket shapes, the requirement-list rule, the per-stage evidence lists from every area | Scope of this file · Acceptance ticket shape · Requirement list rule · Stage 0 acceptance · Stage 1 acceptance · Stage 2 acceptance · Stage 3 acceptance · Stage 4 acceptance · Stage 5 acceptance · Stage 6 acceptance · Stage 7 acceptance · Mid-sprint integration checks |
| `Zenith2/Docs/Design/PROC/HumanProcedures.md` | the exact human procedures for A8a, the journal archival, A6, A8b, A8c and the post-seed steps, with preconditions and rollback | Scope of this file · Order of the landing · Before A8a · A8a docs and config commit · Journal archival before A6 · A6 dry run · A6 filing · A8b seed commit · A8c coverage commit · After the seed · Rollback |

### Reconciliations with the core documents

<!-- elaboration -->
Each row is a point where the core documents and the area hand-offs left PROC a choice, the choice made, and the cross-document item filed where a core sentence must follow.

| # | Point | Choice | Core follow-up |
|---|---|---|---|
| R1 | Budgets names the base-steps ticket and the coverage ticket as producers of the step-timeout rows, but a step timeout is sized from a measured cold run ([Z2-N-BUDGET-349]), and no real run exists before the scaffold, which Z2-D-128 makes the last Stage 0 implementation ticket | the Stage 0 acceptance ticket `Z2 S0 stage acceptance` (Z2T-PROC-024) measures the cold runs and adopts Z2-B-PROC-005 to -009, -011 to -022, -006 and Z2-B-PROC-001 (the lane's storage after the scaffold); Z2-B-PROC-024 and -031 are adopted by `Z2 S2 stage acceptance` from their first real runs; until a row is adopted each step applies the provisional value of `Zenith2/Tools/Gates/step_timeouts.json` and prints that it did | X383 (Producer cells of `Zenith2/Docs/Budgets.md` → "Row index" and "Producer tickets", the "Stage 0 budget rows" sentence of `Zenith2/Docs/Roadmap.md`) and its amendment row for Z2-B-PROC-001 |
| R2 | the cold phase cap (Z2-B-PROC-005) is needed by the scaffold's first tick, which is a cold build in a fresh lane | every Z2 category carries a generic key `coldPhaseTimeoutMinutes` with the provisional value 120 (four times the warm cap), the same placeholder form `Zenith2Acceptance`'s `phaseTimeoutMinutes: 240` already uses for Z2-B-PROC-006; the Stage 0 acceptance replaces it with the adopted value | the same row as R1 |
| R3 | the hand-offs ask for many sanitizer paths, while `linux-tsan` builds no GPU library ([Z2-N-ARCH-173]) and so a GPU path should select `win-asan` alone | the `Zenith2Memory` category's `paths` are the union of two committed glob lists in `Zenith2/Tools/Gates/sanitizer_paths.json`, one per preset, and each `Sanitizers` line runs only when the diff's changed set reaches its own preset's list; Z2-D-044's five categories are unchanged | none: the mechanism lives inside the protected gate directory |
| R4 | the Test step must run the `win-shipping-test` preset (row X216), and building it inside verify would spend the warm verify cap on a compile | the Stage 0 category edit adds `pwsh -NoProfile -File Zenith2\Tools\Gates\Build.ps1 -Preset win-shipping-test` to every Z2 category's `measureSteps` and to the `Zenith2` gates, after the `win-release` line | the matrix row of R16 |
| R5 | the mixed-union refusal (Z2-D-106) needs an opt-in a ticket states out loud (pending Q8) | default draft, the orchestrator's recommendation (a): the opt-in is the existing `needs-human` label, read as Q8 (a)'s "PROC-ish" because PROC tickets share the `Zenith2` category with every ordinary ticket. A ticket carrying it is never claimed by the loop, so only a human-driven gate run of a PROC ticket can reach a mixed union, and it is allowed there. The refusal is also wider than Z2-D-106's text: it refuses any mix, including a Sharpmake-category ticket whose diff unions a Z2 category. If Q8 is ruled (b), the plain refusal applies and those PROC tickets are gated by hand (`Zenith2/Docs/Design/PROC/Harness.md` → "Classification") | X435: a CROSS_DOC row, applied when Q8 is ruled: `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 2 names the opt-in, and Z2-D-106 records the wider refusal |
| R6 | three Zenith2Memory path hand-offs and the A1 hand-over list differ from the `Zenith2Memory` paths row of `Zenith2/Docs/Contracts.md` → "Enforceability matrix" | the Stage 2 sanitizer-activation ticket (Z2T-PROC-041) makes the category paths equal the union of `sanitizer_paths.json`, which includes every path that row names | a cross-area note to the orchestrator: folding the path list into the A1 package before A8a makes the row true at hand-over and leaves Z2T-PROC-041 the two gate lines alone |
| R7 | [Z2-N-CONTRACT-417] gives `forbidden_allowlist.json` entries as `{ "rule", "path", "decision" }` | the file deviates in four ways, each with its reason: (1) a required `id` (unique, `^[A-Z0-9]+-[0-9]{2}$`), so a finding names the entry that exempted it; (2) a top-level wrapper (`schema`, `schemaVersion`, `comment`), the format convention of every PROC data file; (3) the top-level `f21Scope` list, because F21's scope ends in prose ("every further tool directory … that an area's design names") that a script cannot read; (4) the `rule` value `SANITIZER-HEADER`, the scope extension of [Z2-N-CONTRACT-432] for `<sanitizer/*.h>`; (5) the top-level `f25Boundary` list, the third-party boundary files rule F25 (c) admits (reconciliation R21); (6) the top-level `profilerReachable` list, the sources outside the profiler's trees that rule F26 (i) also covers (reconciliation R22). The optional `token` and `comment` members are new as well. `decision` keeps the contract's grammar exactly, a `Z2-D-NNN` that exists: no pending form (ruling PROC-R1 (8)) | X437: a CROSS_DOC row against [Z2-N-CONTRACT-417] (the entry shape and the wrapper) and [Z2-N-CONTRACT-432] (the `SANITIZER-HEADER` rule value) |
| R8 | Q5 (a) gates all Zenith2 work on ZEN-114 to ZEN-119, while Z2-D-112 is settled: no Z2 ticket is linked to or from another project's ticket, and its rejected alternative is exactly "links added after the seed" | no link is made, before or after the seed (ruling PROC-R1 (1)). The gate is enforced three times: the Stage 0 entry criterion of `Zenith2/Docs/Roadmap.md`, the human's `zagent sprint start` for Stage 0, and the read-only precondition check that is the first Definition-of-Done item of Z2T-PROC-001 (six `zagent show ZEN-<n> --json` probes requiring `status: Done`). Every other Stage 0 PROC ticket is `blocks`-linked from Z2T-PROC-001, inside project Z2 | X434: a CROSS_DOC row: `Zenith2/Docs/Roadmap.md` → "Stage 0 entry criteria" item 4 drops "at A6 the filer links each of them `blocks` to the Stage 0 kickoff" and states the three enforcements instead |
| R9 | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" lists ForbiddenCheck rules F1 to F21 ([Z2-N-CONTRACT-416]), PROC adds dynamic loading ([Z2-N-ARCH-274]) and third-party headers outside their owner ([Z2-N-ARCH-165]), and SEC adds the confined OS APIs | PROC keeps F22 (dynamic loading) and F23 (third-party headers); SEC's confined-API rule is F24 (ruling PROC-R1 (3)). A token both F22 and F24 match is one finding with both IDs, allowed only where both allow it | X436: a CROSS_DOC row adding F22 to F24 to the Contracts table and to [Z2-N-CONTRACT-416]'s "rules F1 to F21", superseding X414 (a)'s number |
| R10 | Z2-D-067 and `Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 9 place the coverage outputs in `Zenith2/out/coverage/`, while [Z2-N-CONTRACT-573] and [Z2-N-ARCH-160] confine every gate output to `Zenith2/out/<checkout-hash>/` | the outputs move under `${ZENITH_OUT}/coverage/` (ruling PROC-R1 (7)); no exception to [Z2-N-CONTRACT-573] | X438: a CROSS_DOC row against Z2-D-067's text and the Roadmap item; a note to the orchestrator for the session checker's report path and its `Coverage.md` paragraph |
| R11 | [Z2-N-CONTRACT-151] step 5 and [Z2-N-CONTRACT-578] check line (1) of the signpost block mechanically and leave lines (2) to (4) to review | line (1) is a `fail`; lines (2) to (4) are `report` findings (`F16-SIGNPOST-LINE`), so the gate is no stricter than the contract | none |
| R12 | `Zenith2/Docs/Roadmap.md` → "Stage acceptance mechanics common to every stage" still makes each acceptance ticket `blocks`-linked from every ticket of its stage, which the orchestrator's ruling replaced | each acceptance ticket is blocked by its mid-sprint check and its stage's PROC activation tickets alone (Stage 0: Z2T-PROC-023 and the scaffold) | X439: a CROSS_DOC row for that Roadmap bullet |
| R13 | rows addressed to PROC after its draft (SEC-P1 to P8, P-RS1 to P5, NT9, SP10, MT3, X386, X387, X389, CONTENT-P1 to P3, PACK-P2, CHAR-P2, CINE-P1 to P5) | each absorbed with a disposition (ruling PROC-R1 (2); `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger", "Late hand-offs"); SEC-P1 adds the gate step `SecurityConformance`, so "Gate step command lines" and the `Zenith2/CLAUDE.md` step list grow by one | X414 (b) carries the Contracts rows; the `Zenith2/CLAUDE.md` step list is Z2T-PROC-044's protected edit |
| R14 | step timeouts adopted once from an empty scaffold would make every subsequent cold build a `GATE-TIMEOUT`, and one acceptance cap cannot fit both Stage 0 and Stage 6; the `Libgraph` Shipping build and `SecurityConformance` have no row | raise-only re-allocation of every step-timeout row and of Z2-B-PROC-006 at each stage acceptance, plus a per-stage acceptance watchdog computed from the stage's suite (ruling PROC-R1 (6)); two new rows, "Step timeout Build win-shipping" and SEC's "Step timeout SecurityConformance" (X415) | X440: a CROSS_DOC row for the re-allocation rule text of [Z2-N-BUDGET-349] and the new Shipping-build row |
| R15 | the "Shared gate environment" table of `Zenith2/Docs/Contracts.md` gives `Get-Zenith2CompileFlags` `includes[]` and `defines[]`, and `New-Zenith2HeaderTU` the TU path | `Get-Zenith2CompileFlags` also returns `compileCommandFragments`, and `New-Zenith2HscOverlay` is an added function that builds the include overlay | X441: a CROSS_DOC row adding both to the table |
| R16 | the "Enforceability matrix" rows for `Build` ("a build of `win-debug`, `win-release` or `android-arm64`") and for the `Zenith2Memory` paths are narrower than the design | the matrix should say "a build of a preset of `Build.ps1`'s preset table" and "the union of `Zenith2/Tools/Gates/sanitizer_paths.json`", so no worker concludes from it that ZenithAI or ContentCore diffs run no sanitizer | X442: a CROSS_DOC row for both matrix rows; Z2T-PROC-008's `Zenith2/CLAUDE.md` gate-step line lists `win-shipping-test` |
| R17 | `HeaderSelfContained`'s overlay and the compile fragments go beyond the contract's function table | as R15 | as R15 |
| R19 | Coverage acceptance mode requires every leaf and proof of every stage-≤N requirement Done, but runs while the accepted ticket itself is open and before its stage's complexity apply ticket can be Done | the Done test exempts the accepted ticket and the stage's `coverageDeferred` apply ticket; no requirement attaches to a ticket whose stage comes after its own, so the per-stage PROC requirements are one per stage (ruling PROC-R2 M1) | X445: `Zenith2/Docs/Roadmap.md` → "Stage acceptance mechanics common to every stage", acceptance-mode bullet |
| R20 | [Z2-N-BUDGET-353] has the tick orchestrator file the headroom ticket, and `TICK.md` may not name Zenith2 | the Z2 `postFinish` hook runs `File-Backlog.ps1 --check-headroom`, which files it on three consecutive exceedances; the mid-sprint check stays the backstop (ruling PROC-R2 M9) | X446: [Z2-N-BUDGET-353] names the Z2 `postFinish` hook as the filer |
| R21 | the naming rules of `Zenith2/CLAUDE.md` → "Naming in Zenith Hungarian" (no `noexcept` or `[[nodiscard]]` unless a language or standard-library mechanism requires it, the `uint8` … `sint64` integer names, `final` only on a class with a base) and the ban on `vk::` in Slang were review-only, while the Contracts step list of `ApiContractCheck` still required every public function to be `noexcept`, and ZenithCore's per-platform threading header must include `<pthread.h>` and `<semaphore.h>` although a public header reaches it | the gates enforce the conventions, not their predecessors: `ApiContractCheck` checks contract blocks only and emits no `noexcept` finding (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "ApiContractCheck"); ForbiddenCheck rule F25 refuses `noexcept` and `[[nodiscard]]` (except on a line signposted `// ZENITH-ALLOW: <reason>`), `u_int*`, `int8_t` … `uint64_t` (except in `Zenith_Types.h` and the `f25Boundary` third-party boundary files under `Internal/`) and `final` on a class with no base, in runtime and tool code; `ShaderChecks` item 2 (`NoVulkanAttributes`) refuses `vk::` in every `.slang` file; the `publicPlatformHeaders` list of `platform_headers.json`, SourceLayout SL3/SL4 and the HeaderSelfContained overlay admit `Zenith2/Engine/ZenithCore/Platform/<Platform>/Zenith_PlatformThreading.h`, each signposted. Keeping a member `const` in an `IMMUTABLE` type is decided by review, through the reviewer conventions ("Z2T-PROC-019 reviewerConventions edit" of `Zenith2/Docs/Design/PROC/GateList.md`), because no gate can decide "where possible" | a CROSS_DOC row for the core documents: `Zenith2/Docs/Contracts.md` → "Contract blocks per type" (subsection "ApiContractCheck behaviour") drops the `noexcept` step and the rule IDs `ACC-NOEXCEPT-MISSING` and `ACC-NOEXCEPT-CONDITIONAL`; the table "ForbiddenCheck rules" and [Z2-N-CONTRACT-416] gain F25; [Z2-N-CONTRACT-417] gains `f25Boundary`; [Z2-N-CONTRACT-432] gains the `publicPlatformHeaders` exception |
| R18 | [Z2-N-CONTRACT-414] condition 4 reads the game executable's "link map"; it states seven conditions | TS4 reads the PDB publics, which carry the same public symbol set and need no second linker flag; TS8 repeats LG8's closure check inside the step a test author reads and is not an eighth contract condition | none: the evidence is the same symbol set, and TS8 adds a check rather than changing one |
| R22 | a profiler hook that locks a profiled `Zenith_Mutex`, `Zenith_ConditionVariable` or `Zenith_Semaphore` would call the hook again; the lock-profiling design makes that impossible by types (profiler code names only the `_NoProfiling` variants), by a gate and by a run-time re-entry guard in the dispatch functions, while the Contracts table "ForbiddenCheck rules" ends at F25 and [Z2-N-CONTRACT-416] says "rules F1 to F25" | PROC adds rule F26 (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table"): clause (i) refuses a profiled primitive (the alias names as whole tokens, or a platform class template instantiated with `true`) in `Zenith2/Engine/ZenithProfiling/**`, in `Zenith2/Engine/ZenithCore/Internal/Profiling/**` and in each file of the `profilerReachable` list of `forbidden_allowlist.json` (a new top-level key, like `f21Scope` and `f25Boundary`; its initial contents are the twenty-five sources a hook dispatch reaches: ZenithCore's dispatch header, clock, `Zenith_SpscRing.h`, thread identity, failure path and logging, as CORE confirmed against `Zenith2/Docs/Design/CORE/Threading.md`, and DEVT's two `Zenith_CrashLogMirror` log-sink sources under `ZenithPlatform/Internal/CrashHandler/`); clause (ii) refuses a direct use of the hook table outside `Zenith2/Engine/ZenithCore/Zenith_ProfilerHooks.h` and tests. No allowlist entry and no signpost exempts it; the number is the next free one after F25 | a CROSS_DOC row for the core documents: the table "ForbiddenCheck rules" of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" gains the F26 row; [Z2-N-CONTRACT-416] and the `ForbiddenCheck` row of the "Enforceability matrix" say "F1 to F26"; [Z2-N-CONTRACT-417] lists `profilerReachable` among the wrapper keys; the matrix gains a row for the rule |

### Coordination with SEC

SEC's design (`Zenith2/Docs/Design/SEC.md` → "Protected files SEC specifies") files eight PROC hand-offs, SEC-P1 to SEC-P8, all absorbed here (reconciliation R13). PROC owns the gate mechanisms and protected files SEC's evidence uses; SEC owns the content:

| Mechanism | PROC owns | SEC (and the owning area) owns |
|---|---|---|
| the Shipping exposure scan | the `Libgraph` Shipping build and closure check, the file format of `shipping_forbidden_symbols.json` (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph policy files"), the step's call of SEC's audit script for layers E2 to E4 and the positive controls from Z2T-PROC-054 ("Shipping exposure layers") | which entry symbols and strings are development services and test keys, the audit script and its layers; each area files its entries through a PROC ticket (SEC-P5 to SEC-P7) |
| the confined OS APIs | ForbiddenCheck rule F24, its matcher, its fixtures (Z2T-PROC-045) | the API classes and the allow records of `confined_apis.json` (SEC-P4) |
| the conformance check | the gate step `SecurityConformance`, its gate line, its timeout row and its place in `Coverage` (Z2T-PROC-044) | the checks SC01 to SC16, the inputs and the messages (`Zenith2/Docs/Design/SEC/ConformanceCheck.md`, SEC-P1) |
| the protected Security gate directory | `Zenith2/Tools/Gates/Security/` as a protected PROC deliverable (Z2T-PROC-046, SEC-P9; ruling SEC-R1 R9), the one SL9 exception for the configure's execution of `Test-Zenith2UpdateKeys.ps1`, and the runner globs that discover `Tests/Security/Test-Security.ps1` | the readers, matchers and scripts' design (`Zenith2/Docs/Design/SEC.md` → "Zenith2SecurityGates module"); the data stays in `Zenith2/Tools/Security/*.json` under `reviewPaths` (SEC-P8) |
| the fuzz legs | the `win-asan` smoke run inside `Sanitizers`, its selection by the diff and its budget (Z2T-PROC-041), and the fuzz-owning paths in `sanitizer_paths.json` | the selection rules, the budget formula, the tiers, the campaign durations and the campaign itself, a `needs-human` step of each acceptance from Stage 2 (SEC-P2, SEC-P3; `Zenith2/Docs/Design/SEC/Fuzzing.md`) |
| the review paths | `reviewPaths` gains `update_keys.json` and `parser_inventory.json` (Z2T-PROC-016) | the two files (SEC-P8) |
| the tampered-patch tests | none: they are ordinary PACK and PLAT tests run by the `Test` step | the test list |

## Dependencies

### Areas PROC depends on

| Area | What PROC relies on | Exact headings |
|---|---|---|
| MOD | the scaffold's presets, helpers, `declared_deps.json`, `zenith_tests`, `zenith_players`, `zenith_host_tools` and `zenith_apks` targets | `Zenith2/Docs/Design/MOD.md` → "Gate step interplay"; `Zenith2/Docs/Design/MOD/GameBuild.md` → "The Apk target" |
| SHIP | the manifest, the setup script, `Zenith2Env.psm1` with `Get-Zenith2CheckoutHash`, `Enter-Zenith2Env.ps1`, `third_party_targets.json`, the CI matrix | `Zenith2/Docs/Design/SHIP.md` → "Stage 0", "Stage 0 bootstrapping order", "Pin landing order"; `Zenith2/Docs/Design/SHIP/Qualification.md` → "CI matrix"; `Zenith2/Docs/Design/SHIP/Manifest.md` → "Setup script" |
| DEVT | the `linux-tsan` host and its runner module, the TSan smoke suite, the fuzz campaign runner, the evidence module and its assert scripts | `Zenith2/Docs/Design/DEVT/QualityTools.md` → "The linux-tsan host", "TSan smoke suite", "Fuzz campaign runner", "Gates in CI"; `Zenith2/Docs/Design/DEVT/Evidence.md` → "Zenith2Evidence module" |
| RPROD and the Rendering core | the lint JSON files and `ZenithShaderCompiler --check`; the parity labels `z2-parity-backend` and `z2-parity-path` | `Zenith2/Docs/Rendering/ShaderSystem.md` → "Shader lints", "Inputs and outputs"; `Zenith2/Docs/Design/RPROD/Tests.md` → "Parity suites" |
| ECS | the `ZenithECS_Conformance` executable and its per-backend CTest names | `Zenith2/Docs/Design/ECS/Tests.md` → "Conformance suite", "Gate steps per test group" |
| SCRIPT and the Scripting core | the firewall rules ForbiddenCheck enforces | `Zenith2/Docs/Scripting.md` → "Luau integration and the exception firewall" |
| every area | its PROC hand-offs and its requirement catalogue for the acceptance lists | `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger"; `Zenith2/Docs/Design/PROC/Acceptance.md` → "Requirement list rule" |
| the core documents | the gate contracts this design implements | `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates", "Enforceability matrix", "Review and change management", "No mutable global state", "Layering DAG"; `Zenith2/Docs/Architecture/BuildSystem.md` → "Hygiene gates fed by the build", "Presets", "The declared_deps.json record", "The linux-tsan host"; `Zenith2/Docs/Roadmap.md` → "Stage 0 Tooling bring-up", "Board conventions"; `Zenith2/Docs/Budgets.md` → "Gate budget rows" |

### Zenith-side code PROC edits

These are the generic Zenith tools PROC tickets change. None of the changes names Zenith2 except the root delegation line (Z2-D-030) and the Z2 values inside `zagent.project.json` and `vault.config.json`.

| File or directory | Changed by | Reference anchors at commit `268b8ef6` |
|---|---|---|
| `Tools/tick_gates.ps1`, `Tools/Test_TickGates.ps1` | Z2T-PROC-003 | `Get-GateList`, `Invoke-GateLine`, `Invoke-GateLineFlakeAware`, `Test-IsZenithBuildLine`, `Write-Result`, the MEASURE and VERIFY phases |
| `Tools/Vault/cli/legacy-pure/routing.mjs`, `Tools/Vault/cli/compat.mjs`, `Tools/Vault/cli/read.js`, `Tools/Vault/spec/command-matrix.md`, `Tools/Vault/test/*.test.mjs` | Z2T-PROC-002, -016, -017, -019, -030 | `resolveRules`, `effectiveGates`, `gateSelectionOf`, `dispatchGates`, `showPayload` |
| `Tools/Vault/cli/write.mjs`, `Tools/Vault/cli/legacy-pure/flags.mjs`, `Tools/Vault/cli/legacy-pure/ticket.mjs` | Z2T-PROC-016, -020, -030 | `COMMAND_FLAGS`, `dispatchWrite`, `isWriteCommand`, `specOf`, `upsertWorklog`, `matchGoverningRulings`, `RULINGS_FILE_MIN_SCORE` |
| `Tools/Vault/cli/lanes.mjs`, `Tools/lane_init.ps1`, `Tools/Test_LaneInit.ps1`, `Tools/Vault/test/lanes-add.test.mjs` | Z2T-PROC-021 | `laneAdd`, `LANE_ALLOWED_FLAGS`, `Invoke-LaneInit` |
| `Tools/analyze_code_complexity.py`, `Tools/complexity_profiles.json`, `Tools/tests/test_complexity.py` | Z2T-PROC-004 (with `--dump-entities`) | `find_source_files`, `_COMPLEXITY_FAIL_ON_KEYS`, `_resolve_profile` |
| `Tools/libgraph/**` | Z2T-PROC-007 | `model.py` `build_model`, `libgraph.py` `_build_arg_parser`, `tests/CONTRACT.md` |
| `Tools/zagent/protocol/TICK.md`, `Tools/zagent/protocol/roles/reviewer.md`, `Tools/zagent/protocol/tier-mentions.allowlist`, `Tools/zagent/Build-HarnessArtifacts.ps1`, `Tools/zagent/Test-HarnessParity.ps1` and every generated harness file | Z2T-PROC-001, -016, -017, -019 | `TICK.md` steps 6.3, 8 and 9; `$mustBeProtected` |
| root `CLAUDE.md`, `vault.config.json`, `zagent.project.json` | Z2T-PROC-001, -006, -008, and each activation ticket | the board section, the guardrail lists, the categories |
| the two grep-guard tests under `Tools/Vault/test/` (Z2-D-100), `Build/zenith_buildsystem.psm1`, `Tools/doc_lint.ps1`, `Tools/check_project_membership.ps1`, `Tools/check_boot_unit_assets.ps1` | Z2T-PROC-005 | `Test-ZenithProjectShapingPath`; `doc_lint` checks C5 and C9 |
| `.github/workflows/*.yml` | Z2T-PROC-005 (positive `paths:` filters), Z2T-PROC-015 (`zenith2-gates.yml`) | — |

### Interfaces PROC declares for other areas

- **The gate step command line** (`Zenith2/Docs/Design/PROC.md` → "Gate step command lines"): every area names a step by its name and parameters in Validation sections, never by path.
- **The CTest label contract**: the `Test` step excludes the labels `z2-parity-backend`, `z2-parity-path` and every `z2-acceptance-*` label, `Test.ps1 -Label <label>` runs exactly one label, and `BackendParity`, `PathParity` and `EcsConformance` select theirs (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test").
- **The allowlist and policy file formats** (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files", "Libgraph policy files"): an area requests an entry through a PROC hand-off that gives the entry exactly in these formats.
- **The acceptance hooks**: a `z2-acceptance-<area>` CTest suite is run by `Run-Committed.ps1 -Acceptance -Stage N` and by nothing else (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "Run-Committed").

## Library and file layout

### The protected gate directory

PROC builds no C++ library. Every file below is protected from the moment Z2T-PROC-001 lands (`Zenith2/Tools/Gates/**`), and every one is created by the PROC ticket named.

```
Zenith2/Tools/Gates/
	Common/Zenith2Gates.psm1                 shared gate module (Z2T-PROC-008)
	Common/Zenith2GateReport.psm1            the gate report writer and reader (Z2T-PROC-008)
	Common/Zenith2ClangAst.psm1              the streaming clang JSON AST reader of ApiContractCheck (Z2T-PROC-011)
	Configure.ps1  Build.ps1  Test.ps1  Sweep.ps1  SourceLayout.ps1  CompileCoverage.ps1      (Z2T-PROC-008)
	ForbiddenCheck.ps1  Forbidden/Zenith2Forbidden.psm1                                       (Z2T-PROC-009)
	TestSeparationCheck.ps1  Libgraph.ps1  Complexity.ps1                                     (Z2T-PROC-010)
	Format.ps1  HeaderSelfContained.ps1  ApiContractCheck.ps1  ReviewSignals.ps1  ShaderChecks.ps1  EcsConformance.ps1   (Z2T-PROC-011)
	Run-Committed.ps1  Zenith2RunCommitted.psm1  acceptance_suites.json                       (Z2T-PROC-012)
	Acceptance/Invoke-ClangTidy.ps1  Acceptance/Test-AcceptanceRecord.ps1                     (Z2T-PROC-012)
	Security/Zenith2SecurityGates.psm1  Security/Test-Zenith2ShippingExposure.ps1  Security/Test-Zenith2UpdateKeys.ps1   (Z2T-PROC-046, SEC-P9)
	Coverage.ps1  Coverage/Zenith2Coverage.psm1  Coverage/rules.json                          (Z2T-PROC-013)
	ApkCheck.ps1                                                                              (Z2T-PROC-040)
	Sanitizers.ps1  sanitizer_paths.json                                                      (Z2T-PROC-041)
	SecurityConformance.ps1                                                                   (Z2T-PROC-044)
	confined_apis.json                                                                        (Z2T-PROC-045)
	BackendParity.ps1                                                                         (Z2T-PROC-050)
	shipping_forbidden_strings.json                                                           (Z2T-PROC-054)
	IntegrationSmoke.ps1                                                                      (Z2T-PROC-051)
	PathParity.ps1                                                                            (Z2T-PROC-070)
	step_timeouts.json                                                                        (Z2T-PROC-008)
	std_headers.json  platform_headers.json  forbidden_allowlist.json  writable_data_allowlist.json   (Z2T-PROC-009)
	libgraph_policy.json  libgraph_baseline.json  shipping_forbidden_symbols.json             (Z2T-PROC-010)
	complexity_profile.json  complexity_lints_allowlist.txt                                   (Z2T-PROC-010; the profile re-applied by Z2T-PROC-062, -073, -083, -095)
	Suppressions/tsan.supp  Suppressions/ubsan.supp  Suppressions/asan.supp                   (Z2T-PROC-041; empty files with a header comment)
	Tests/Run-GateSelfTests.ps1                                                               (Z2T-PROC-012)
	Tests/<Step>/Test-<Step>.ps1  Tests/<Step>/Fixtures/<Case>/...                            (with each step's ticket)
	Tests/Test-CategoryAgreement.ps1  Tests/Test-LocalSubsetOfCi.ps1  Tests/Test-TreeStability.ps1   (Z2T-PROC-012)
	Tests/Test-GateScriptsExist.ps1  Tests/Test-AcceptanceSuites.ps1                          (Z2T-PROC-012)
	Tests/Test-StepTimeouts.ps1                                                               (Z2T-PROC-008)
	Tests/Security/Test-Security.ps1  Tests/Security/Fixtures/**                              (Z2T-PROC-046)
	Tests/Test-SanitizerPathsMatchCategory.ps1                                                (Z2T-PROC-041)
	Tests/Test-SecurityConformance.ps1  Tests/SecurityConformance/**                          (Z2T-PROC-044)
```

- `Common/Zenith2Gates.psm1` carries the three functions of `Zenith2/Docs/Contracts.md` → "Tests and code hygiene gates" (table "Shared gate environment") and the functions of `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module".
- `Tests/**/Fixtures/**` holds deliberately failing inputs; every step excludes `Zenith2/Tools/Gates/Tests/**` from its scan set, and the fixture trees contain no `CMakeLists.txt` the build globs (the root `CMakeLists.txt` adds no subdirectory under `Tools/Gates/`, and `zenith_add_subdirectories(Tools)` reads `Tools/*/CMakeLists.txt` only, so `Tools/Gates/` must never hold a `CMakeLists.txt` at its top level; `SourceLayout` enforces it).
- The per-step self-tests are `Tests/<Step>/Test-<Step>.ps1` (for example `Tests/Coverage/Test-Coverage.ps1`, `Tests/ForbiddenCheck/Test-ForbiddenCheck.ps1`); the named cases inside them use the `Test_<Step>_<Case>` form.
- The production filer is not under the gate directory: it is `Zenith2/Tools/Backlog/File-Backlog.ps1` with `Zenith2/Tools/Backlog/Zenith2Backlog.psm1` and `Zenith2/Tools/Backlog/Tests/`, created by Z2T-PROC-014 (`Zenith2/Docs/Roadmap.md` → "Stage 0 deliverables" item 10 names the path). It is unprotected like every Z2 tool. Because `postFinish` runs it with board-write authority after every Done, `Zenith2/Tools/Backlog/**` is a `reviewPaths` glob (Z2T-PROC-016), so every change to it gets an independent, revision-bound review.

### Other Zenith2 files PROC tickets create or edit

| File | Ticket | Note |
|---|---|---|
| `Zenith2/ThirdParty/manifest.json`, `Zenith2/ThirdParty/licence_policy.json`, `Zenith2/ThirdParty/licences/*`, `Zenith2/Tools/Setup/Setup-Zenith2.ps1`, `Zenith2/Tools/Setup/Enter-Zenith2Env.ps1`, `Zenith2/Tools/Setup/Zenith2Env.psm1` | Z2T-PROC-018 | the content is SHIP's specification (`Zenith2/Docs/Design/SHIP.md` → "Stage 0"); the human downloads and verifies every pin |
| `Zenith2/Tools/Setup/Provision-Lane.ps1` | Z2T-PROC-021 | the Z2 value of the per-project lane provisioning hook (`Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook"); unprotected, under SHIP's setup directory, written by PROC because it lands with the hook |
| `Zenith2/CLAUDE.md` | Z2T-PROC-001 (protects it), and any subsequent PROC ticket that edits it | after Z2T-PROC-001 every edit is PROC work |
| `Zenith2/Docs/Budgets.md`, `Zenith2/Docs/Budgets/GateRows.md`, `Zenith2/Docs/DecisionLog.md` | every stage acceptance (Z2T-PROC-024, -033, -043, -053, -061, -072, -082, -094) and the activation tickets Z2T-PROC-040, -041, -044, -050, -051, -070 | each allocates or raises the gate rows it measures, and appends the allocation entry to the decision log in its own diff (Z2-D-097) |

### Supported matrices

The gate scripts are PowerShell 7 scripts run on the Windows gate host and the self-hosted CI runners; `Sanitizers -Preset linux-tsan` drives the WSL2 distro locally and runs natively on the Linux CI runner through `Run-Committed.ps1 -Host linux`. No gate script is a CMake target, so none declares a `ZENITH_SUPPORTED` matrix.

| Host | Steps that run there |
|---|---|
| Windows gate host and lanes | every step except the native Linux leg |
| Windows CI runner (`[self-hosted, windows, x64, zenith2]`) | every active line whose host is Windows |
| Linux CI runner (`[self-hosted, linux, x64, zenith2]`) | the `Sanitizers -Preset linux-tsan` line, natively |

## Public API

### API surface kinds

PROC's public surface is not C++: it is command lines, PowerShell module functions, JSON file formats, Vault CLI verbs and category keys. Zenith Hungarian applies to C++ only; PowerShell functions follow the approved `Verb-Noun` form with the `Zenith2` noun prefix, and parameters are PascalCase.

### Gate step command lines

Every gate line has the form `pwsh -NoProfile -File Zenith2\Tools\Gates\<Step>.ps1 [<parameters>]` (Z2-D-032; `Zenith2/Docs/Roadmap.md` → "Board conventions"). These are the exact parameter sets:

| Step | Parameters | Exit codes |
|---|---|---|
| `Configure.ps1` | none | 0, 1 (a configure failed), 3 (environment), 124 |
| `Build.ps1` | `-Preset win-debug\|win-release\|win-shipping-test\|win-shipping\|win-asan\|android-arm64\|android-arm64-hwasan` (mandatory) | 0, 1, 2 (unknown preset), 3, 124 |
| `Test.ps1` | `[-Label <label>]`, `[-Preset win-debug\|win-release\|win-shipping-test]` (default: all three) | 0, 1, 2, 3, 124 |
| `SourceLayout.ps1`, `CompileCoverage.ps1`, `ForbiddenCheck.ps1`, `TestSeparationCheck.ps1`, `Complexity.ps1`, `Format.ps1`, `HeaderSelfContained.ps1`, `ApiContractCheck.ps1`, `ShaderChecks.ps1`, `EcsConformance.ps1`, `ApkCheck.ps1`, `BackendParity.ps1`, `PathParity.ps1` | none | 0, 1, 3, 124 |
| `Libgraph.ps1` | none as a gate line; `[-Acceptance] [-WriteBaseline]` only from `Run-Committed.ps1 -Acceptance` (refused with exit 2 otherwise) | 0, 1, 2, 3, 124 |
| `SecurityConformance.ps1` (from Z2T-PROC-044) | none as a gate line; `[-Root <dir>] [-Out <dir>]` for SEC's self-test (`Zenith2/Docs/Design/SEC/ConformanceCheck.md` → "Command line and exit codes") | 0, 1, 2, 3, 124 |
| `ReviewSignals.ps1` | none | always 0 unless its own input is unreadable (3) or it times out (124) |
| `Coverage.ps1` | `[-Stage <0..7>]` (acceptance mode), `[-Fixture <dir>]` (self-test only) | 0, 1, 2, 3, 124 |
| `Sanitizers.ps1` | `-Preset win-asan\|linux-tsan` (mandatory), `[-All]` (ignore the path selection; acceptance and CI) | 0, 1, 2, 3, 124 |
| `Sweep.ps1` | none | always 0 (it reports what it killed) |
| `IntegrationSmoke.ps1` | `[-Game <Name>]` | 0, 1, 3, 124 |
| `Run-Committed.ps1` | `-ListJobs -Json` · `-Host windows\|linux` · `-Acceptance -Stage <0..7> [-WriteBaseline]` · `-InsideSlot` (internal: the re-run under the build slot) · `[-Category <name>]` · `[-DryRun]` | 0, 1, 2, 3, 11 (slot busy, acceptance only), 124 (acceptance watchdog) |
| `Acceptance/Invoke-ClangTidy.ps1` | none (an acceptance line, never a gate line) | 0, 1, 3 |
| `Acceptance/Test-AcceptanceRecord.ps1` | `-Stage <0..7>` (an acceptance procedure step, never a gate line) | 0, 1, 3 |
| `Security/Test-Zenith2ShippingExposure.ps1`, `Security/Test-Zenith2UpdateKeys.ps1` | SEC's (`Zenith2/Docs/Design/SEC.md` → "Release-time and acceptance tools"); called by `Libgraph`, by the configure and by SHIP's release verification, never a gate line | SEC's |

Exit code meanings are the enum of "Gate step exit codes" below.

### Shared module functions

The functions of `Common/Zenith2Gates.psm1`, specified in `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module":

```powershell
Get-Zenith2MsvcEnvironment                                       # Contracts.md "Shared gate environment"
New-Zenith2HeaderTU -Library <string> -Header <string>           # Contracts.md "Shared gate environment"
Get-Zenith2CompileFlags -Target <string> -Preset <string>        # Contracts.md "Shared gate environment"
Get-Zenith2RepoRoot                                              # [string] absolute, forward slashes
Get-Zenith2OutRoot [-Linux]                                      # [string] ${ZENITH_OUT} for Windows or WSL presets
Enter-Zenith2GateStep -Name <string> [-Preset <string>] [-Parameters <hashtable>]   # [Zenith2GateContext]
Exit-Zenith2GateStep -Context <Zenith2GateContext> -ExitCode <int> -Verdict <string>  # writes the report, exits
Add-Zenith2Finding -Context <Zenith2GateContext> -File <string> -Line <int> -Rule <string> -Message <string> [-Severity fail|report]
Get-Zenith2StepTimeoutMinutes -Step <string> [-Preset <string>]  # [int] adopted row value or provisional value
Get-Zenith2ScanFiles [-Extensions <string[]>] [-Under <string[]>] # [string[]] tracked + untracked, repo-relative
Get-Zenith2FileApiReply -ConfigurePreset <string>                # [pscustomobject] the codemodel-v2 reply index
Get-Zenith2DeclaredDeps                                          # [pscustomobject] declared_deps.json
Get-Zenith2ActiveGateLines [-ExcludeCategory <string[]>]         # [pscustomobject[]] { category, line, host }
Get-Zenith2ChangedPaths                                          # [string[]] or $null when no ticket context
Get-Zenith2TestEnvironment                                       # [hashtable] VK_DRIVER_FILES and the validation layer path
Invoke-Zenith2Process -FilePath <string> -ArgumentList <string[]> -TimeoutMinutes <int> [-WorkingDirectory <string>] [-Environment <hashtable>]   # { ExitCode; Output; Seconds; TimedOut }
Invoke-Zenith2ExeSweep                                           # [string[]] killed image paths
Read-Zenith2BudgetRow -Id <string>                                # { Id; Status; Value; Unit }
Invoke-Zenith2Configure -ConfigurePreset <string>                # { ExitCode; Ran; Seconds; Tail }
Invoke-Zenith2Build -Preset <string> [-Target <string>]          # { ExitCode; Cold; Reasons; Seconds; FirstError }; signals a cold tree
Get-Zenith2AcceptanceStage                                       # [int] or $null outside Run-Committed -Acceptance
New-Zenith2HscOverlay -Library <string>                          # [string] the overlay root (an addition, R15)
```

`Get-Zenith2CompileFlags` also returns the group's `compileCommandFragments` (reconciliation R15).

### Vault and harness interfaces

| Interface | Shape | Specified in |
|---|---|---|
| category key `buildSystem` | `"external"` or absent (absent means the Sharpmake classification) | `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode" |
| category key `measureSteps` | array of gate lines; non-empty when `buildSystem` is `external` | the same |
| category keys `phaseTimeoutMinutes`, `coldPhaseTimeoutMinutes` | positive integers, minutes | the same |
| category key `reviewPaths` | array of repo-relative globs | `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records" |
| category key `reviewerConventions` | string, inlined into the reviewer prompt | `Zenith2/Docs/Design/PROC/Harness.md` → "Reviewer conventions" |
| category key `postFinish` | array of command lines, each with a pre-authorised prefix | `Zenith2/Docs/Design/PROC/Harness.md` → "postFinish" |
| `lanes.provision.<PROJECT>` in `zagent.project.json` | `{ "copyIgnored", "regen", "provisionGames", "steps" }` | `Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook" |
| `zagent review record <KEY> --verdict pass\|fixes-applied --reviewer <tier>` | a new write verb | `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records" |
| `gates.json` fields `buildSystem`, `measureSteps`, `phaseTimeoutMinutes`, `coldPhaseTimeoutMinutes`, `reviewPaths`, `review.requiredByDiff`, `review.paths`, `unioned[].buildSystem`, `mixedUnionAllowed`, `postFinish`, `reviewerConventions` | additive payload fields | `Zenith2/Docs/Design/PROC/Harness.md` → "Vault pass-through" |
| environment variables a gate line receives from `tick_gates.ps1` | `ZAGENT_GATES_JSON` (absolute path of the ticket's `gates.json`), `ZAGENT_COLD_SIGNAL` and `ZAGENT_REVIEW_SIGNAL` (absolute paths of temporary files), `ZAGENT_TICKET` (the key) | `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode" |
| environment variables every acceptance line receives | `ZENITH_ACCEPTANCE_STAGE=<N>`, `ZENITH2_ACCEPTANCE=1` | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N" |

### Handle kinds used

None. PROC defines no C++ type.

## Data structures and formats

### Format conventions

- Every JSON file PROC defines is UTF-8 without BOM, LF line endings, four-space indentation when written by a tool, and carries `"schema": "<name>"` and `"schemaVersion": <integer ≥ 1>` as its first two members; a reader refuses an unknown `schema` or a `schemaVersion` greater than its own with exit 3 and a message naming the file, the found value and the supported value.
- JSON carries no comments, so a `"comment"` member is the comment form, as [Z2-N-CONTRACT-153] fixes for `writable_data_allowlist.json`.
- Arrays that a tool writes are sorted by byte order of their key member, so a re-run on unchanged input writes identical bytes.
- Paths inside every file are repo-relative with `/` separators and no leading `./`; a reader normalises `\` to `/` before matching.
- Globs use the `routing.mjs` glob dialect (`*` within one segment, `**` across segments, no braces), the same dialect `zagent gates` applies to category `paths`, so one glob means one set everywhere.

### Formats index

| Format | `schema` | Specified in |
|---|---|---|
| gate step report | `zenith2.gate-step` | "The gate step report" below |
| step timeout table | `zenith2.step-timeouts` | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module" |
| `std_headers.json`, `platform_headers.json`, `forbidden_allowlist.json` | `zenith2.std-headers`, `zenith2.platform-headers`, `zenith2.forbidden-allowlist` | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck allowlist files" |
| `writable_data_allowlist.json` | the object form of [Z2-N-CONTRACT-153], plus `schema` and `schemaVersion` | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Writable-data scan" |
| `libgraph_policy.json`, `libgraph_baseline.json`, `shipping_forbidden_symbols.json` | `zenith2.libgraph-policy`, `zenith2.libgraph-baseline`, `zenith2.shipping-forbidden-symbols` | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph policy files" |
| `complexity_profile.json` | the analyzer's profile-file form (`zenith2` profile) | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Complexity" |
| `sanitizer_paths.json` | `zenith2.sanitizer-paths` | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizer path selection" |
| coverage rule table `Coverage/rules.json` and coverage report | `zenith2.coverage-rules`, `zenith2.coverage-report` | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage rules" |
| Run-Committed job plan | `zenith2.ci-jobs` | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Run-Committed" |
| acceptance run record, baseline re-pin record | `zenith2.acceptance-run`, `zenith2.acceptance-baseline-run` | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N", "-Acceptance -Stage N -WriteBaseline" |
| acceptance suites | `zenith2.acceptance-suites` | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "acceptance_suites.json" |
| acceptance record | `zenith2.acceptance-record` | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test-AcceptanceRecord" |
| analyzer entity dump | `complexity-entities` | `Zenith2/Docs/Design/PROC/Harness.md` → "dump-entities" |
| libgraph surface and surface delta | `zenith2.libgraph-surface`, `zenith2.libgraph-surface-delta` | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs" |
| lane storage record | `zenith2.lane-storage` | `Zenith2/Docs/Design/PROC/Harness.md` → "The Z2 step" |
| `confined_apis.json`, `shipping_forbidden_strings.json` | SEC's formats (`zenith2.confined-apis`; SEC's string-list schema) | `Zenith2/Docs/Design/SEC/PathTraversal.md` → "confined_apis.json"; `Zenith2/Docs/Design/SEC/ShippingExposure.md` → "shipping_forbidden_strings.json schema" |
| review record block | the `agent:review` sentinel block | `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records" |

### The gate step report

Every step writes `${ZENITH_OUT}/gates/<Step>.json` (for a step with `-Preset`, `<Step>.<preset>.json`; for `Test -Label <l>`, `Test.label-<l>.json`) at exit, on every exit path including a caught exception and its own timeout:

```json
{
    "schema": "zenith2.gate-step",
    "schemaVersion": 1,
    "step": "ForbiddenCheck",
    "parameters": { },
    "checkoutHash": "0123456789abcdef",
    "headSha": "<git rev-parse HEAD>",
    "startedUtc": "2026-10-16T12:00:00Z",
    "finishedUtc": "2026-10-16T12:01:12Z",
    "seconds": 72.4,
    "timeoutMinutes": 20,
    "timeoutSource": "adopted Z2-B-PROC-016",
    "exitCode": 1,
    "verdict": "FAIL: 2 finding(s)",
    "cold": null,
    "findings": [
        { "file": "Zenith2/Engine/ZenithCore/Internal/X.cpp", "line": 12, "rule": "F1", "severity": "fail", "message": "malloc outside ZenithCore/Internal/Memory/" }
    ],
    "counts": { "filesScanned": 412, "fail": 2, "report": 0 },
    "artifacts": [ "gates/ForbiddenCheck_WritableData.json" ]
}
```

| Member | Type | Rule |
|---|---|---|
| `parameters` | object | the step's bound parameters, names as on the command line |
| `headSha` | string | `git rev-parse HEAD`; informational, the freshness authority stays `tick_gates.json` |
| `timeoutSource` | string | `adopted Z2-B-PROC-NNN`, `provisional Z2-B-PROC-NNN (row pending)` or `none` |
| `cold` | object or null | every step that built a cold tree through `Invoke-Zenith2Build`: `{ "cold": true, "presets": [ … ], "reason": "<missing CMakeCache.txt\|presets hash changed\|manifest hash changed\|CMake/** hash changed>" }` |
| `findings` | array | sorted by `file`, `line`, `rule`; at most 2,000 entries, after which one finding with rule `GATE-TRUNCATED` states the total |
| `severity` | `fail` or `report` | only `fail` makes the exit code 1 |
| `artifacts` | array | paths relative to `${ZENITH_OUT}`, every further file the step wrote |

The last line of the step's standard output is always `<Step>: PASS` or `<Step>: FAIL (<n> finding(s)); report <path>`, so a reader of `gates.log` sees the verdict without opening the JSON. Each finding is also printed as `<Step>: <file>:<line>: <rule>: <message>`, the form [Z2-N-CONTRACT-151] and `ApiContractCheck` fix.

## Algorithms and state machines

### Algorithm index

| Algorithm | Specified in |
|---|---|
| the step lifecycle (enter, timeout watchdog, sweep, report, exit) | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Step lifecycle" |
| each step's checks | its section in `BaseSteps.md`, `StructuralGates.md` or `ExtendedGates.md` |
| the external-build measure/verify split, the union validity check and the mixed-union rule | `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode" |
| the review-record hash and the finish refusal | `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records" |
| the reconcile algorithm of the production filer | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer" |
| the acceptance run | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Run-Committed" |
| the requirement-list derivation for acceptance tickets | `Zenith2/Docs/Design/PROC/Acceptance.md` → "Requirement list rule" |

### PROC ticket state machine

A PROC ticket is `needs-human`, so the loop never claims it (Z2-D-103). Its life on the board:

| State | Event | Next state | Action |
|---|---|---|---|
| To Do (blocked) | every `blocks` predecessor Done | To Do (ready for a human) | none: the queue skips it because of `needs-human` |
| To Do | the human starts it: `zagent move <KEY> "In Progress" --human` | In Progress | the human works in the main checkout, on `master`, with every lane idle when the ticket edits `zagent.project.json`, `vault.config.json`, `Tools/tick_gates.ps1` or `Tools/Vault/**` (every lane reads the main checkout's configuration live) |
| In Progress | the change and its tests pass by hand: the listed self-tests, then the gates of "How a PROC ticket is gated" below | In Progress | the human records `tick_gates.json` (or the hand-run outputs) and the self-test outputs as evidence files |
| In Progress | evidence complete | Done | `git commit` of the diff (the human's commit), then `zagent finish <KEY> --status Done --human --comment <result> --worklog <worklog>` |
| In Progress | a check fails and cannot be fixed in the ticket | Blocked | `zagent finish <KEY> --status Blocked --human …`; a BUG is filed on the PROC epic |
| any | the ticket's change must be undone after landing | — | `git revert` of the ticket's commit as a new commit, with the same lane-idle rule, then a new PROC ticket carries the corrected change |

**How a PROC ticket is gated.** Every PROC ticket is filed in `Zenith2`, so `zagent gates <KEY>` always selects the base lines, but those lines name scripts that do not exist until their PROC tickets land. The rule (ruling PROC-R1, F16):

1. **Until the scaffold Z2T-MOD-056 has landed** (ruling PROC-R2 M5), no PROC ticket is run through `tick_gates.ps1`. The switch is mechanical: it is the presence of `Zenith2/CMakePresets.json` on `master`, which only the scaffold creates. Until then `Configure`, `Build`, `Test`, `CompileCoverage`, `HeaderSelfContained` and `Libgraph` have no tree to run on and would be red or exit 3 for want of a scaffold, never for a defect in the diff. This covers every PROC ticket that lands before Z2T-MOD-056, which is every Stage 0 PROC ticket except Z2T-PROC-024 and -025. The human runs the ticket's own listed self-tests by hand, and `Tests/Run-GateSelfTests.ps1` once it exists. For a diff that reaches the Tooling category's `paths` (`Tools/*.ps1`, `Tools/zagent/**`, `Tools/Vault/**`), the human also runs every line of that category's `gates` array verbatim, for example `pwsh -NoProfile -File Tools\Test_TickGates.ps1` and `pwsh -NoProfile -Command "Set-Location Tools/Vault; npm test"`. For a protocol change, the human also runs `Build-HarnessArtifacts.ps1 -Harness <h> -Check` for the three harnesses. The outputs go in the work log. Scripts that exist and need no build tree run directly against their fixtures (their self-tests). The base lines are not run.
2. **From the scaffold's landing on**, every base script exists and has a tree (`Test-GateScriptsExist.ps1` passed when Z2T-PROC-012 landed after -013), and a PROC ticket runs `pwsh -NoProfile -File Tools/tick_gates.ps1 <KEY> -Phase all` whenever its diff reaches a gated path; a mixed union is allowed for it because it carries `needs-human` (reconciliation R5, pending Q8). The first PROC tickets gated this way are Z2T-PROC-025 and the Stage 0 acceptance's own lines; every Stage 1+ PROC ticket follows rule 2.
3. If Q8 is ruled (b), rule 1 extends to every PROC ticket whose diff reaches `Tools/**` as well as `Zenith2/**`: the Sharpmake-side gates are run by hand and `tick_gates.ps1` runs only the Z2 side.

### Gate line activation state machine

| State of a gate line | Event | Next state |
|---|---|---|
| absent from every category | its PROC activation ticket lands the step script (inactive, unreferenced) | script present, line absent |
| script present, line absent | the activation ticket adds the line to the category's `gates` (and to `measureSteps` when it is a build line) | active: `/tick` runs it for every diff that reaches the category's `paths`, and `Run-Committed.ps1` runs it in CI |
| active | a PROC ticket removes the line | script present, line absent |

A line is never added before its script exists in the same commit or an earlier one, with one exception: the hand-over lines of the A1 package (the 15 base lines, `ShaderChecks`, `EcsConformance`, the memory label line and the eight acceptance lines), which land at A8a before any script exists (`Zenith2/Docs/Roadmap.md` says so). They are inert until the scripts land, because no Z2 ticket is claimable before Stage 0 starts and the PROC tickets are gated by "How a PROC ticket is gated" above. The gate self-test `Test-GateScriptsExist` asserts the rule from Z2T-PROC-013's landing on (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "Gate self-tests"), and the Stage 0 acceptance records it.

## Threading lifetime and error contract

### Concurrency of gate runs

- `tick_gates.ps1` holds the machine-wide build slot (`Tools/build_lock.ps1`, `Local\ZenithBuildSlot`) for the whole phase; no Z2 gate step acquires the slot itself, because the slot is not re-entrant and a nested acquire would wait on its own holder.
- `Run-Committed.ps1 -Acceptance` runs outside `tick_gates.ps1`, so it acquires the slot through `Tools/build_lock.ps1 -Run "<its own command line> -InsideSlot"` around its whole run and exits 11 when the slot stays busy past `buildSlotWaitMinutes` (Z2-D-083). Its `-WriteBaseline` form is the one gate run that writes a tracked file (`libgraph_baseline.json`), and only after the suites have passed.
- `Run-Committed.ps1 -Host windows|linux` in CI runs on a runner that runs nothing else (one job per runner at a time), so it takes no slot.
- Steps run strictly in sequence inside a phase; no step starts work in the background that outlives its own exit: every child process is started through `Invoke-Zenith2Process`, which kills the child's process tree (`taskkill /T /F` on Windows, `kill -TERM` then `-KILL` of the process group on Linux) when the step's timeout passes or the step exits.
- Every step writes only under `${ZENITH_OUT}` (`Zenith2/out/<checkout-hash>/` on Windows, `$HOME/.zenith2/out/<checkout-hash>/` for `linux-tsan`), never into `.zagent/run/` and never into a tracked path ([Z2-N-CONTRACT-573]).

### Gate step exit codes

| Code | Name | Meaning | `tick_gates.ps1` reads it as |
|---|---|---|---|
| 0 | `GATE_EXIT_PASS` | every check passed | green |
| 1 | `GATE_EXIT_FINDINGS` | at least one `fail` finding, a failing build or a failing test | a red gate line (exit 4 of the phase) |
| 2 | `GATE_EXIT_USAGE` | an unknown or missing parameter | a red gate line |
| 3 | `GATE_EXIT_ENVIRONMENT` | the pinned environment is missing or wrong (`Setup-Zenith2.ps1 -Check` would fail, a pinned tool is absent, a reader met an unknown schema) | a red gate line; its verdict names the setup command |
| 11 | `GATE_EXIT_SLOT_BUSY` | `Run-Committed.ps1 -Acceptance` only | not reached through `tick_gates.ps1` |
| 124 | `GATE_EXIT_STEP_TIMEOUT` | the step's own timeout killed its work | a red gate line, never flake-retried (the same code the phase watchdog uses) |

### Failure without findings

A step that throws prints `<Step>: INTERNAL ERROR: <message> at <file>:<line>`, writes its report with `exitCode` 1 and the verdict `FAIL: internal error`, and exits 1. An internal error is never a pass; the self-test of every step includes one case that forces an internal error and asserts exit 1.

## Diagnostics and profiling

| Output | Where | Content |
|---|---|---|
| per-step report | `${ZENITH_OUT}/gates/<Step>[.<preset>].json` | "The gate step report" above |
| per-step log | `${ZENITH_OUT}/gates/logs/<Step>[.<preset>].log` | the full output of every child process, in order |
| CTest JUnit | `${ZENITH_OUT}/gates/tests/<Name>.xml` | one per test executable ([Z2-N-ARCH-229]) |
| libgraph reports | `${ZENITH_OUT}/gates/libgraph/` | `violations.json`, `swap_<family>.json`, `shipping_closure_<Game>_<preset>.json`, `surface.json`, `surface_delta.json`, the surface cache, the exposure reports, `index.html` |
| coverage outputs | `${ZENITH_OUT}/coverage/` (reconciliation R10) | `Coverage.md` (regenerated), `Normative.md`, `normative.json`, `implemented.md`, `report.json`, `stale_cites.json` |
| acceptance run record | `${ZENITH_OUT}/gates/acceptance/stage-<N>/run.json` | every line run, its exit code, its timing and its report path |
| timing for the gate budget | `tick_gates.json` `steps[].Seconds` | the MP-GATE evidence of the gate rows ([Z2-N-BUDGET-353]) |

Every step prints its start line `<Step>: start (timeout <n> min, <source>)`. Every build through `Invoke-Zenith2Build` prints `sccache --show-stats` before and after ([Z2-N-ARCH-233]) and its cold reason. No step emits a profiler zone or a telemetry record: the gate scripts are host tools outside the engine.

## Authoring and cooking

PROC has no authoring surface and no cook step. Its authored inputs are the protected data files of the gate directory (the allowlists, the policy files, the profile, the baselines, `sanitizer_paths.json`, `step_timeouts.json`); each is edited only by a PROC `needs-human` ticket that gives the entry exactly in the file's format and names the decision that admits it, and each format is specified in the split files. The one data file other tickets change without a PROC ticket is none: an area that needs an allowlist entry records a PROC hand-off, and its dependent ticket waits for the PROC ticket that carries the entry.

## Failure handling

| Failure | Defined behaviour |
|---|---|
| a gate step script is missing when its line runs | `pwsh -File` fails with a non-zero exit; the line is red. `Test-GateScriptsExist` prevents this from being committed |
| the pinned environment is absent (fresh lane, setup not run) | the step exits 3 with `<Step>: environment: <what> missing; run pwsh -NoProfile -File Zenith2/Tools/Setup/Setup-Zenith2.ps1 -Check` |
| a reader meets a newer `schemaVersion` | exit 3 naming the file and both versions |
| a step's own timeout | its child tree is killed, the report records `exitCode` 124, the line is red and never flake-retried |
| the phase cap | `tick_gates.ps1` kills the running line (`Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode") |
| a cold start | whichever step builds the cold tree writes the cold reason to `ZAGENT_COLD_SIGNAL` (inside `Invoke-Zenith2Build`), and `tick_gates.ps1` applies `coldPhaseTimeoutMinutes` to that phase; provisioning warms `warmPresets`, so this is rare after a lane's first build |
| out of disk during a step | the child fails; the step exits 1 with the child's message; `Sweep.ps1` never deletes build output, so recovery is the human's `zenith2` out-tree cleanup |
| a findings list over 2,000 | truncated with `GATE-TRUNCATED`, the exit code is still 1 |
| an allowlist entry whose path matches no file | a `report` finding `ALLOWLIST-STALE`, never a failure, so an entry can land before its file |
| an allowlist entry without a decision ID | a `fail` finding (`ALLOWLIST-NO-DECISION`), per [Z2-N-CONTRACT-417] and [Z2-N-CONTRACT-153] |
| the WSL distro is missing or `vm.mmap_rnd_bits` is above 28 | `Sanitizers -Preset linux-tsan` exits 3 with DEVT's message ([Z2-N-ARCH-219]) |
| a human-run PROC ticket is interrupted mid-edit of `zagent.project.json` or `vault.config.json` | lanes stay idle; the human restores the file with `git checkout -- <file>` and re-runs the ticket; `zagent doctor` must be `ok` before any lane is fired again |

## Test strategy

### Test files and the gate that runs them

| Suite | Location | Run by |
|---|---|---|
| every gate step's own self-test | `Zenith2/Tools/Gates/Tests/<Step>/Test-<Step>.ps1` | `Tests/Run-GateSelfTests.ps1`, which the human runs in every PROC ticket that edits the gate directory, and `Run-Committed.ps1 -Acceptance` runs at every stage acceptance |
| the cross-cutting gate tests | `Tests/Test-CategoryAgreement.ps1`, `Tests/Test-LocalSubsetOfCi.ps1`, `Tests/Test-TreeStability.ps1`, `Tests/Test-GateScriptsExist.ps1`, `Tests/Test-AcceptanceSuites.ps1`, `Tests/Test-SanitizerPathsMatchCategory.ps1` (from Z2T-PROC-041) | the same |
| `tick_gates.ps1` fixtures | `Tools/Test_TickGates.ps1` | the Tooling category's gates |
| Vault tests | `Tools/Vault/test/*.test.mjs` | the Tooling category's `npm test` line |
| libgraph tests | `Tools/libgraph/tests/` | `python -m pytest Tools/libgraph/tests` in the PROC ticket; the Tooling category gains no line for it (libgraph is not under `Tools/*.ps1`, `Tools/zagent/**` or `Tools/Vault/**`) |
| analyzer tests | `Tools/tests/test_complexity.py` | the existing complexity CI workflow |
| production filer tests | `Zenith2/Tools/Backlog/Tests/Test-FileBacklog.ps1` | registered by Z2T-PROC-014 as the CTest `Pwsh_Proc_FileBacklog` through SHIP's `zenith_register_pwsh_test`, so the `Test` step runs it once SHIP's registry function exists; until then by the human in the PROC ticket |
| harness parity | `Tools/zagent/Test-HarnessParity.ps1`, `Build-HarnessArtifacts.ps1 -Check` | the Tooling category's gates |

The named tests and the case each asserts are listed per step in the split files. Each self-test is a plain assert script in the `Tools/Test_TickGates.ps1` form (an `Assert-That` helper, a pass count, exit 1 on any failure) and runs each step against its fixture trees by passing `-Fixture`-style environment variables documented in the step's section, never against the live tree's state.

### Fixture rule

Every failing fixture is a directory `Tests/<Step>/Fixtures/<Case>/` holding the minimal tree that triggers exactly one rule, plus `expect.json` `{ "schema": "zenith2.gate-fixture", "schemaVersion": 1, "exitCode": 1, "rules": ["F1"] }`; the self-test asserts the step's exit code and that the set of rule IDs in its report equals `rules`. Every passing fixture has `"exitCode": 0, "rules": []`. Fixtures are text files only (Q55); a binary input (an object file for F16, an APK for ApkCheck) is generated by the self-test from text sources at test time into `${ZENITH_OUT}/gates/selftest/`.

## Budget rows

| Row | Name | Status | Producer (as this design assigns it, R1) |
|---|---|---|---|
| Z2-B-PROC-001 | Lane checkout storage per Z2 lane | pending | Z2T-PROC-024 (the lane measured after the scaffold, `Provision-Lane.ps1 -WarmOnly`) |
| Z2-B-PROC-002, -003, -004 | warm phase caps measure and verify, tree-change retry cap | adopted, 30 min | Z2-D-028 |
| Z2-B-PROC-005 | Cold phase cap | pending (provisional 120 min in `coldPhaseTimeoutMinutes`) | Z2T-PROC-024 |
| Z2-B-PROC-006 | Acceptance phase timeout | pending (provisional 240 min) | Z2T-PROC-024 |
| Z2-B-PROC-007 to -009, -011 to -022 | base step timeouts | pending (provisional values of `step_timeouts.json`) | Z2T-PROC-024 |
| Z2-B-PROC-010, -023 | Build android-arm64, ApkCheck | pending | Z2T-PROC-040 |
| Z2-B-PROC-024, -031 | ShaderChecks, EcsConformance | pending | Z2T-PROC-043 (Stage 2 acceptance, first real runs) |
| Z2-B-PROC-025, -026 | Sanitizers win-asan, linux-tsan | pending | Z2T-PROC-041 |
| Z2-B-PROC-027 | BackendParity | pending | Z2T-PROC-050 |
| Z2-B-PROC-028 | IntegrationSmoke | pending | Z2T-PROC-051 |
| Z2-B-PROC-029 | PathParity | pending | Z2T-PROC-070 |
| Z2-B-PROC-030 | Integration smoke frame count | adopted, 300 frames | Z2-D-031 |
| new: Step timeout Build win-shipping | the `Libgraph` LG9 Shipping build | requested (R14) | Z2T-PROC-024 |
| new (X415): Step timeout SecurityConformance | the SEC step | requested | Z2T-PROC-044 |

**First allocation.** The producer runs the step cold three times on the gate host (an empty `${ZENITH_OUT}` for that preset, sccache cleared with `sccache --zero-stats` and its cache directory renamed aside for the run), takes the maximum, multiplies by 1.5 and rounds up to a whole multiple of 5 minutes. At the Stage 0 acceptance, whose tree is only the scaffold, each step-timeout row also has a floor of one third of its provisional value, so no build row starts below 30 minutes. The cold phase cap is the sum of the cold timeouts of the measure lines plus 10 minutes, rounded up to a multiple of 5. The acceptance row Z2-B-PROC-006 is the maximum of 240 and 1.5 times the measured cold run of `Run-Committed.ps1 -Acceptance -Stage 0`, rounded up to a multiple of 15.

**Raise-only re-allocation at every stage acceptance** (ruling PROC-R1 (6)). The acceptance run is cold by construction (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N" item 3), so its `run.json` holds a cold measurement of every active step. For each adopted step-timeout row whose step ran, `new = ceil5(1.5 × the longest measured seconds of that step's lines / 60)`; the row is raised to `new` when `new` exceeds it and is never lowered. The cold phase cap is recomputed from the raised measure-line rows, and Z2-B-PROC-006 is raised to `ceil15(1.5 × the run's total minutes)` when that exceeds it. Every raise is recorded in the acceptance ticket's diff with its decision entry, and the per-stage acceptance watchdog applies on top. Each producer writes the measured cold times into `Zenith2/Docs/Research/Budgets/PROC/allocation-stage-<N>.json` and appends its decision entry.

## Stage plan

| Stage | PROC delivers | Gate list after the stage |
|---|---|---|
| 0 | the 23 Stage 0 PROC tickets (Z2T-PROC-001 to -022 and the landing and acceptance-shape verification Z2T-PROC-025) of `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map", the mid-sprint check and the acceptance ticket; the scaffold (MOD) is proven by the acceptance | the hand-over list plus the `win-shipping-test` build line |
| 1 | the `needs-revalidation` claim predicate; the CORE justification-record allowlist entries (Z2T-PROC-034); the mid-sprint check and the acceptance. Nothing for flagged proposal P2 is filed (Z2-D-130) | unchanged |
| 2 | Android build and `ApkCheck`; the sanitizer pair with `sanitizer_paths.json`, SEC's smoke selection and the fuzz-owning paths; the protected Security gate directory (Z2T-PROC-046, SEC-P9); `SecurityConformance` (Z2T-PROC-044); ForbiddenCheck F24 (Z2T-PROC-045); mid-sprint check; acceptance | base gains `Build -Preset android-arm64`, `ApkCheck` and `SecurityConformance`; `Zenith2Memory` gains the two `Sanitizers` lines |
| 3 | `BackendParity`; the CI-only `IntegrationSmoke`; the Shipping exposure layers (Z2T-PROC-054); mid-sprint check; acceptance | `Zenith2Render` gains `BackendParity`; `Run-Committed.ps1`'s CI-only list gains `IntegrationSmoke` |
| 4 | the first complexity-profile apply ticket (Z2T-PROC-062); mid-sprint check; acceptance | unchanged |
| 5 | `PathParity`; the apply ticket Z2T-PROC-073; mid-sprint check; acceptance | `Zenith2Render` gains `PathParity` |
| 6 | `ApkCheck` over the App Bundle splits; the apply ticket Z2T-PROC-083; mid-sprint check; acceptance | unchanged lines (the step gains a check) |
| 7 | one PROC ticket per expansion that brings tooling (dedicated-server preset, OpenXR with SCOPE's scripted runtime, the `ps5`, `xbox-series` and `switch2` presets behind their SDK-access tickets); the apply ticket Z2T-PROC-095; mid-sprint check; acceptance | per expansion |

Interfaces land first: Z2T-PROC-002 (the Vault fields) lands before Z2T-PROC-003 (the reader in `tick_gates.ps1`), and the shared module (Z2T-PROC-008) before every step that imports it.

## Decisions

### Settled decisions governing PROC

Z2-D-007 (libgraph and complexity hard gates), Z2-D-008 (reproducibility), Z2-D-011 (sanitizer presets), Z2-D-028 (gate budget), Z2-D-029 (the Z2 lane), Z2-D-030 (root delegation), Z2-D-031 (CI), Z2-D-032 (gate activation), Z2-D-033 (backlog before scaffold), Z2-D-042 (hard gates), Z2-D-044 (process and categories), Z2-D-049 (API review from the diff), Z2-D-066 (normative traceability), Z2-D-067 (coverage), Z2-D-068 (ticket standard), Z2-D-069 (filing), Z2-D-083 (acceptance outside the loop), Z2-D-086 (identical category settings), and the session rulings Z2-D-087 to Z2-D-132, of which these bind PROC's mechanics directly: Z2-D-090, Z2-D-091, Z2-D-093, Z2-D-095, Z2-D-096, Z2-D-097, Z2-D-098, Z2-D-101, Z2-D-102, Z2-D-103, Z2-D-105, Z2-D-106, Z2-D-107, Z2-D-110, Z2-D-111, Z2-D-112, Z2-D-115, Z2-D-116, Z2-D-117, Z2-D-118, Z2-D-127, Z2-D-128, Z2-D-130.

### Human rulings PROC implements

| Ruling (2026-10-16) | PROC's realisation |
|---|---|
| Q1 (a): the one-row `metadata.md` `jira.projects` edit in A8a | `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "A8a docs and config commit" step 6 |
| Q2 (a): a human-run archival of complete Vault journals before A6 | `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Journal archival before A6" |
| Q3 (a): a Stage 0 generic per-project lane provisioning hook | Z2T-PROC-021; `Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook" |
| Q5 (a) and Q6 (b): the ZEN tooling tickets ZEN-114 to ZEN-119 are Done before any Zenith2 work | without links (Z2-D-112, ruling PROC-R1 (1)): the Roadmap entry criterion, the human's Stage 0 `sprint start`, and Z2T-PROC-001's read-only precondition check; Z2-R-PROC-001; `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Order of the landing", "After the seed" |

The human's code-convention ruling of 2026-10-23 (no `noexcept` or `[[nodiscard]]` unless required, the `uint8` … `sint64` integer names, `final` only with a base, `const` members in `IMMUTABLE` types, no `vk::` in Slang, and synchronisation types as one typed class per platform) is realised in reconciliation R21.

The orchestrator's ruling PROC-R1-RULINGS (2026-10-17) settles the eight questions of the round-1 review. It is realised in reconciliations R5 and R7 to R14, in the late hand-off ledger, in the acceptance procedure and in the PROC ticket map.

### Open decisions and flagged proposals PROC depends on

- Flagged proposal P2 (N lanes per project, Z2-D-130): awaiting the human. Following the P1 precedent, nothing realising it is filed: Z2-R-PROC-120 is recorded as research pending approval (in "Excluded and deferred" of the catalogue, not in its machine list), no Draft-ID is reserved, no acceptance depends on it, and the specification stays an assumption block in `Zenith2/Docs/Design/PROC/Harness.md` → "N lanes per project". Approval files the ticket; rejection makes the requirement `excluded` with the rejection's decision.
- Open question Q8 (the shape of the mixed-union opt-in) is pending: the default draft is the orchestrator's recommendation (a) with the `needs-human` label as the opt-in (reconciliation R5), and Z2-R-PROC-009 and -010 carry "(pending Q8)".
- Open question Q4 (the `needs-revalidation` claim effect): the pairing with `deferred` stands (Z2-D-102) and Z2T-PROC-030 adds the claim predicate in Stage 1.

No open decision of the core list governs a PROC ticket.

## Requirement index

| Requirement | Design headings |
|---|---|
| Z2-R-PROC-001 | `Zenith2/Docs/Design/PROC.md` → "Reconciliations with the core documents"; `Zenith2/Docs/Design/PROC/HumanProcedures.md` → "Order of the landing", "After the seed" |
| Z2-R-PROC-002 to -005 | `Zenith2/Docs/Design/PROC/Harness.md` → "Guardrail additions", "Harness regeneration" |
| Z2-R-PROC-006 to -016 | `Zenith2/Docs/Design/PROC/Harness.md` → "tick_gates external-build mode" |
| Z2-R-PROC-017 to -020 | `Zenith2/Docs/Design/PROC/Harness.md` → "Vault pass-through" |
| Z2-R-PROC-021, -022 | `Zenith2/Docs/Design/PROC/Harness.md` → "Analyzer keys" |
| Z2-R-PROC-023, -024 | `Zenith2/Docs/Design/PROC/Harness.md` → "Scanner audit" |
| Z2-R-PROC-025 | `Zenith2/Docs/Design/PROC/Harness.md` → "Root delegation" |
| Z2-R-PROC-026, -027, -032 | `Zenith2/Docs/Design/PROC/Harness.md` → "libgraph CMake front end" |
| Z2-R-PROC-028 to -031, -134 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph", "Libgraph policy files" |
| Z2-R-PROC-033 to -035 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module", "Step lifecycle"; this file → "The gate step report", "Gate step exit codes" |
| Z2-R-PROC-036 to -044 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Configure", "Build", "Test", "SourceLayout", "CompileCoverage", "Format", "HeaderSelfContained", "ApiContractCheck" |
| Z2-R-PROC-045 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "TestSeparationCheck" |
| Z2-R-PROC-046 to -055 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck", "ForbiddenCheck rule table", "ForbiddenCheck allowlist files", "Writable-data scan", "ForbiddenCheck fixtures" |
| Z2-R-PROC-056 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Complexity" |
| Z2-R-PROC-057 to -063 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "ReviewSignals", "ShaderChecks", "EcsConformance", "Sweep", "Run-Committed", "Gate self-tests" |
| Z2-R-PROC-064, -065 | `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Shared gate module" |
| Z2-R-PROC-066 to -072 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage", "Coverage rules", "Coverage fixtures", "Coverage acceptance mode" |
| Z2-R-PROC-073 to -077 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Production filer" |
| Z2-R-PROC-078 | `Zenith2/Docs/Design/PROC/Harness.md` → "CI workflow" |
| Z2-R-PROC-079 to -083 | `Zenith2/Docs/Design/PROC/Harness.md` → "PROC-API review records" |
| Z2-R-PROC-084, -085 | `Zenith2/Docs/Design/PROC/Harness.md` → "Reviewer conventions" |
| Z2-R-PROC-086, -087 | `Zenith2/Docs/Design/PROC/Harness.md` → "postFinish" |
| Z2-R-PROC-088 | `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map" |
| Z2-R-PROC-089 | `Zenith2/Docs/Design/PROC/Harness.md` → "Rulings citation weight" |
| Z2-R-PROC-090 to -092 | `Zenith2/Docs/Design/PROC/Harness.md` → "Lane provisioning hook", "Z2 lane registration" |
| Z2-R-PROC-093, -094 | `Zenith2/Docs/Design/PROC/Acceptance.md` → "Stage 0 acceptance", "Mid-sprint integration checks" |
| Z2-R-PROC-095 | `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage", "Category edits by ticket" |
| Z2-R-PROC-096 to -098, -121 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "ApkCheck" |
| Z2-R-PROC-099 to -103 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Sanitizers", "Sanitizer path selection" |
| Z2-R-PROC-104 to -108 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "BackendParity", "PathParity"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "IntegrationSmoke" |
| Z2-R-PROC-109 to -118 | `Zenith2/Docs/Design/PROC/Acceptance.md` |
| Z2-R-PROC-119 | `Zenith2/Docs/Design/PROC/Harness.md` → "Revalidation claim predicate" |
| Z2-R-PROC-120 | not in the machine catalogue (pending flagged proposal P2); `Zenith2/Docs/Design/PROC/Harness.md` → "N lanes per project" |
| Z2-R-PROC-122 | `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage" |
| Z2-R-PROC-123 to -130 | `Zenith2/Docs/Design/PROC/HumanProcedures.md` |
| Z2-R-PROC-131 | `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger" |
| Z2-R-PROC-132, -133, -158 | `Zenith2/Docs/Design/PROC.md` → "PROC ticket state machine", "Gate line activation state machine", "Gate step command lines" |
| Z2-R-PROC-135 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "forbidden_allowlist.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map" |
| Z2-R-PROC-136 | `Zenith2/Docs/Design/PROC/GateList.md` → "Gate list by stage", "Late hand-offs"; `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Additional runs" |
| Z2-R-PROC-137 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table" |
| Z2-R-PROC-138, -139 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Smoke selection and budget", "Sanitizer path selection" |
| Z2-R-PROC-140, -141 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Shipping exposure layers", "shipping_forbidden_symbols.json" |
| Z2-R-PROC-142, -157 | `Zenith2/Docs/Design/PROC/GateList.md` → "Category edits by ticket"; `Zenith2/Docs/Design/PROC/Harness.md` → "4. Finish refusal" |
| Z2-R-PROC-143, -160 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Additional runs", "Coverage acceptance mode", "Outputs" |
| Z2-R-PROC-144, -159 | `Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure", "Mid-sprint integration checks" |
| Z2-R-PROC-145, -161 to -163 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json"; `Zenith2/Docs/Design/PROC/GateList.md` → "PROC ticket map" |
| Z2-R-PROC-146, -150, -151 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule", "Libgraph outputs" |
| Z2-R-PROC-147 to -149 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N", "-Acceptance -Stage N -WriteBaseline", "acceptance_suites.json" |
| Z2-R-PROC-152, -153 | `Zenith2/Docs/Design/PROC.md` → "Budget rows"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N" |
| Z2-R-PROC-154 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Functions", "Build" |
| Z2-R-PROC-155 | `Zenith2/Docs/Design/PROC/Harness.md` → "The Z2 step" |
| Z2-R-PROC-156 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test" |
| Z2-R-PROC-164 | `Zenith2/Docs/Design/PROC.md` → "Coordination with SEC", "The protected gate directory"; `Zenith2/Docs/Design/PROC/GateList.md` → "Late hand-offs" |
| Z2-R-PROC-165 | `Zenith2/Docs/Design/PROC/Harness.md` → "dump-entities" |
| Z2-R-PROC-166 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "--check-headroom"; `Zenith2/Docs/Design/PROC/Harness.md` → "postFinish" |
| Z2-R-PROC-167 | `Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "acceptance_suites.json" |
| Z2-R-PROC-168 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test-AcceptanceRecord"; `Zenith2/Docs/Design/PROC/Acceptance.md` → "Acceptance procedure" |
| Z2-R-PROC-169 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Timeout resolution", "Cross-cutting tests" |
| Z2-R-PROC-170 | `Zenith2/Docs/Design/PROC/GateList.md` → "Stage 7 lines"; `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build" |
| Z2-R-PROC-171 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "SourceLayout" |
| Z2-R-PROC-172 | `Zenith2/Docs/Design/PROC/BaseSteps.md` → "Build", "Timeout resolution" |
| Z2-R-PROC-173 | `Zenith2/Docs/Design/PROC/StructuralGates.md` → "ForbiddenCheck rule table", "ForbiddenCheck allowlist files", "ForbiddenCheck fixtures" |
