# PROC design — stage acceptance and mid-sprint integration tickets

Companion to `Zenith2/Docs/Design/PROC.md`. PROC files the eight stage acceptance tickets and the eight mid-sprint integration checks last, after every area catalogue is final (cross-document row X62). This file fixes their shape, the order of the steps each acceptance runs, the rule that derives each acceptance ticket's requirement list, the suite each acceptance run executes, and the evidence each collects, gathered from every area's PROC hand-off.

## Scope of this file

| Ticket | Draft-IDs |
|---|---|
| `Z2 S<N> stage acceptance` | Z2T-PROC-024 (S0), -033 (S1), -043 (S2), -053 (S3), -061 (S4), -072 (S5), -082 (S6), -094 (S7) |
| `Z2 S<N> mid-sprint integration check` | Z2T-PROC-023 (S0), -032, -042, -052, -060, -071, -081, -093 |

## Acceptance ticket shape

Every acceptance ticket follows `Zenith2/Docs/Roadmap.md` → "Stage acceptance mechanics common to every stage", as amended by the orchestrator's ruling that an acceptance ticket carries no stage-wide blocker list (reconciliation R12):

| Field | Value |
|---|---|
| title | `Z2 S<N> stage acceptance` (also the producer name of the budget rows it allocates, [Z2-N-BUDGET-462]) |
| type, kind | TASK, `acceptance` (`Kind: acceptance` trailer) |
| category | `Zenith2Acceptance` |
| labels | `needs-human` |
| parent | `Z2 PROC — Process & gates` |
| sprint, fix version | `Stage <N> — …` (the slug of `Zenith2/Docs/Roadmap.md` → "Sprints and fix versions"), `Z2 Stage <N>` |
| complexity, risk, points | COMPLEX, HIGH, 8 |
| sprint gate | designated in the filer's pass 2 with `zagent sprint gate` |
| `blocked_by` | the stage's mid-sprint check and the stage's PROC activation tickets alone (Stage 0: Z2T-PROC-023 and the scaffold Z2T-MOD-056, which is its evidence); never every ticket of the stage, because the requirement list, not a blocker list, ties the ticket to its stage, and `Coverage.ps1 -Stage N` refuses the acceptance while any stage-≤N requirement's ticket is not Done |
| `Requirements:` | the list of "Requirement list rule"; no requirement is ever attached to a ticket whose stage comes after its own (`Coverage.ps1` fails `CV-ACCEPT-STAGE-ORDER`), so a PROC requirement that recurs per stage has one requirement per stage |
| `Decisions:` | Z2-D-083, Z2-D-065, Z2-D-067, Z2-D-127 and the stage's ruled decisions |

Body sections (the ten of Z2-D-068):

- `## Goal`: "Accept Stage N: run the acceptance procedure below in the main checkout with every lane idle (Z2-D-083), collect the evidence of the table below, and complete the sprint", plus the stage's goal line from `Zenith2/Docs/Roadmap.md` → "Stage overview".
- `## Implementation Contract`: the procedure below, the stage's evidence table, the files the ticket edits (the budget rows it allocates or raises, `libgraph_baseline.json` through the re-pin, the decision entries) and at most three cites.
- `## Definition of Done`: one `- [ ]` item per procedure step and per evidence row; one item "`Coverage.ps1 -Stage N` reports zero `fail` findings"; one item "`Test-AcceptanceRecord.ps1 -Stage N` exits 0 and the SHA-256 it prints is in the `finish --comment`"; one failure item "a failing line leaves the ticket In Progress, a BUG is filed on the failing area's epic, and the sprint is not completed"; the final item "`zagent sprint complete` for the stage, after this ticket is Done".
- `## Validation`: `run.json`, `baseline-run.json` and every evidence file; "Review focus": evidence fingerprints recomputed, not copied; the baseline diff against MAINT's growth report.
- Closure: `zagent finish <KEY> --status Done --human --comment <result> --worklog <worklog>` with `run.json` attached (Z2-D-103).

### Acceptance procedure

The human runs these steps in order. The steps marked MAINT are MAINT's steps A1 to A5 of `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Stage acceptance and mid-sprint steps" (cross-area note MT3 (a)), which run from Stage 2, because the engine version header lands in Stage 2. Stages 0 and 1 run the ownership audit M4 in their place. The acceptance makes its changes as separate commits, so that the clean-tree re-pin of step 6 can run between them (ruling PROC-R2 M3; row MT8).

1. **MAINT A1 (N ≥ 2): release.** `Set-Zenith2EngineRelease.ps1 -Stage <N> -Ticket <key>`, then the review of step 11 on the staged diff, then the commit `MAINT: release engine N.0.0`.
2. **Acceptance run.** `pwsh -NoProfile -File Zenith2\Tools\Gates\Run-Committed.ps1 -Acceptance -Stage <N>`, the cold run of every suite of stages 0 to N (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N"). It writes `run.json`.
3. **Fuzz campaign (N ≥ 2).** After the run, outside the build slot: `pwsh -NoProfile -File Zenith2/Benchmarks/Fuzz/Run-FuzzCampaign.ps1 -Inventory Zenith2/Tools/Security/parser_inventory.json -Stage <N> -Jobs 8 -Out <ZENITH_OUT>/gates/acceptance/stage-<N>/fuzz`.
   - This is SEC's `needs-human` campaign, with the per-tier durations of `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Stage acceptance campaigns" (the full-duration runs of CORE-P9, ASSET-P2, ECS-P3, ENV-P4, SCENE-P3 and SCRIPT-P6). Until DEVT adds `-Inventory` and `-Stage` (row SEC3), the human runs it once per tier, as SEC states.
   - Its evidence is the campaign record of `Zenith2/Docs/Design/SEC/Fuzzing.md` → "Campaign evidence".
   - A reproduced tier 1 finding blocks the acceptance until VERIFIED; a tier 2 or tier 3 finding blocks it until VERIFIED or ACCEPTED.
4. **Coverage.** `pwsh -NoProfile -File Zenith2\Tools\Gates\Coverage.ps1 -Stage <N>`, after the campaign so it reads the campaign record. It exempts this ticket and the stage's `coverageDeferred` apply ticket (`Zenith2/Docs/Design/PROC/ExtendedGates.md` → "Coverage acceptance mode") and must report zero `fail` findings.
5. **Budget re-allocation.** From `run.json`, the step-timeout rows and Z2-B-PROC-006 are re-allocated by the raise-only rule of `Zenith2/Docs/Design/PROC.md` → "Budget rows". At Stage 0 this step is the first allocation from three cold runs, and it also allocates Z2-B-PROC-001 from `Provision-Lane.ps1 -WarmOnly` in the Z2 lane. The step edits `Budgets.md`, `Budgets/GateRows.md`, `DecisionLog.md` and `Zenith2/Docs/Research/Budgets/PROC/allocation-stage-<N>.json`, none of them a `reviewPaths` file, and ends with the commit `Z2 S<N> acceptance: budget re-allocation`.
6. **MAINT A2: surface growth review and re-pin.**
   1. From Stage 2: `New-Zenith2SurfaceGrowthReport.ps1 -Stage <N>`, the human writes the report's disposition, MAINT's `RES-INCOMPLETE` check passes on it, and the human commits `MAINT: surface growth report stage <N>`.
   2. `pwsh -NoProfile -File Zenith2\Tools\Gates\Run-Committed.ps1 -Acceptance -Stage <N> -WriteBaseline` on the now clean tree (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N -WriteBaseline").
   3. The review of step 11 on the staged `libgraph_baseline.json`, then a commit of that file alone, `Z2 S<N> acceptance: libgraph baseline`. At Stages 0 and 1 the re-pin runs without a growth report, which MAINT writes from Stage 2.
7. **MAINT A3 (N = 4 to 7): complexity calibration.** MAINT's ruling ticket `Z2 S<N> complexity calibration` and PROC's apply ticket (Z2T-PROC-062, -073, -083, -095) are both Done. This is a checklist item rather than a `blocked_by` edge, because the ruling measures the tree the acceptance accepts; the apply ticket is the stage's `coverageDeferred` entry.
8. **MAINT A4 (N ≥ 2): MAINT acceptance checks.** `Test-Zenith2Maintenance.ps1 -Acceptance -Stage <N>`.
9. **Human evidence.** The device, visual, measurement and sign-off rows of the stage's evidence table, attached to the ticket.
10. **MAINT A5 (N = 2 to 6): roll.** `Invoke-Zenith2VersionRoll.ps1 -Stage <N> -Ticket <key>`, then the review of step 11, then the commit `MAINT: roll engine to <N+1>.0.0-dev.0`, then the filing of the plan's tickets.
11. **Review.** Before each commit of this ticket that changes a `reviewPaths` file, the human runs the human-run review procedure of `Zenith2/Docs/Design/PROC/Harness.md` → "4. Finish refusal" on the staged diff and records it with `zagent review record <KEY> --verdict pass|fixes-applied --reviewer high`. The affected commits are MAINT's release and roll and the baseline commit. No category is exempt (MAINT's finding on reviewed paths in the acceptance diff).
12. **Record and close.**
    1. Copy `run.json`, `baseline-run.json` and, from Stage 2, the campaign's `campaign-summary.json` to `Zenith2/Docs/Research/Acceptance/stage-<N>/`. This path is clear of the protected research globs; files over the LFS threshold go through LFS, as RESEARCH's archive rule states for the research tree. The full campaign directory stays out of the tree.
    2. Run `pwsh -NoProfile -File Zenith2\Tools\Gates\Acceptance\Test-AcceptanceRecord.ps1 -Stage <N>` (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "Test-AcceptanceRecord"), which must exit 0, and copy its `record.json` beside the others.
    3. Commit `Z2 S<N> acceptance: evidence`.
    4. `zagent finish <KEY> --status Done --human --comment <result> --worklog <worklog>`; the comment carries the SHA-256 the record script printed.
    5. `zagent sprint complete` for the stage.

**Restart rule.** A commit after step 2 is allowed only under `Zenith2/Docs/` or `Zenith2/Tools/Maintenance/`, or on `Zenith_EngineVersion.h` or `libgraph_baseline.json`. Any other commit (for example the fix of a reproduced fuzz finding) voids `run.json`, the campaign and the Coverage result, and the acceptance restarts at step 2. `Run-Committed.ps1 -WriteBaseline` and `Test-AcceptanceRecord.ps1` both re-check the set, so a skipped restart is refused.

## Requirement list rule

The `Requirements:` line of the stage-N acceptance ticket is generated by the filer, never hand-written. It lists every requirement ID of every `Zenith2/Docs/Requirements/<CODE>.md` (and split files) whose `Stage:` is `SN`, byte-ordered, whatever its classification: required, deferred, research, game-side, and excluded (so the coverage step sees them answered). `Coverage.ps1` rule `CV-ACCEPTANCE` fails when the line lacks one or names one of another stage.

The rule turns every area hand-off list into a consistency check instead of an input: COMPRESS-P4, MOD-P2, CORE-P9, PLAT-P4, RHI-P9, GEOM-P3, RPROD-P6, ASSET-P2, ECS-P3, LIGHT-P1, SCRIPT-P5, SCENE-P4, ENV-P1, EDIT-P2, EDIT-P4, SHIP-P6, SAVE-P2, AUDIO-P4, DEVT-P9, CONTENT-P1, PACK-P2, CHAR-P2 and CINE-P5. A hand-off list that disagrees with its catalogue is resolved in favour of the catalogue (for example Z2-R-SCENE-142, moved to S3 by the Q78 ruling).

The counts at the time of the PROC draft (every catalogue's `requirements.json`; the filer recomputes them, and the late catalogues add to S1 to S6):

| Stage | Requirements across all areas |
|---|---|
| S0 | 7 (MOD 2, SHIP 3, DEVT 2) plus PROC's S0 requirements |
| S1 | 182 |
| S2 | 1,478 |
| S3 | 1,725 |
| S4 | 1,674 |
| S5 | 254 |
| S6 | 291 |
| S7 | 7 |

The filer recomputes the line at filing and at every reconcile.

## Stage 0 acceptance

Run-Committed suite (stage 0), mirrored in `acceptance_suites.json`:

| # | Line | When |
|---|---|---|
| 1 | `pwsh -NoProfile -File Zenith2\Tools\Setup\Setup-Zenith2.ps1 -Check` | always |
| 2 | `@category-lines` | always |
| 3 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Tests\Run-GateSelfTests.ps1` | always |
| 4 | `@acceptance-labels` | always |
| 5 | `pwsh -NoProfile -File Zenith2\Tools\Gates\Acceptance\Invoke-ClangTidy.ps1` | `fileExists:Zenith2/.clang-tidy` |

| Evidence | Source |
|---|---|
| ZEN-114 to ZEN-119 Done before the sprint started: the precondition check of Z2T-PROC-001 (six read-only `zagent show ZEN-<n> --json` probes reporting `status: Done`) passed, and its output is in that ticket's work log (Q5 (a), Z2-D-112) | Z2T-PROC-001's work log |
| the scaffold Z2T-MOD-056 ticked end to end in the Z2 lane: claim, worker, measure, verify GREEN, `integrate` GREEN, `finish --status Done` (MOD-P1 (a); Z2-R-MOD-098, -099) | the scaffold's work log and `tick_gates.json` |
| every base step green and `treeShaStart == treeShaEnd` in that `tick_gates.json` | the same |
| `Test-GateScriptsExist.ps1` green: every hand-over line now names an existing script | `Run-GateSelfTests.ps1` output |
| the `android-arm64` build of the Minimal game and `llvm-readelf --dyn-syms` showing `SDL_main` (MOD-P1 (a)) | a human run of `Build.ps1 -Preset android-arm64` |
| a green `zenith2-gates.yml` run on the push containing the scaffold (Z2-R-SHIP-143) | the CI run URL |
| `Test_TickGates.ps1`, the Vault tests, `Test-ZagentClient.ps1`, `Test-HarnessParity.ps1`, `Build-HarnessArtifacts.ps1 -Check` for claude, opencode, codex | their outputs |
| the coverage step clean in acceptance mode `-Stage 0`, its failing fixtures each detected | `Coverage.ps1 -Stage 0`, `Test-Coverage.ps1` |
| the filer's reconcile tests and a `--dry-run` over the live board printing no unexpected change | `Test-FileBacklog.ps1`, the dry-run output |
| the PROC-API refusal test | `test/review-record.test.mjs` |
| `zagent gates` for the sample ZEN and ZM tickets unchanged | the comparison |
| `Setup-Zenith2.ps1 -Check` on the acceptance machine and the CI runners (Z2-R-SHIP-142, -132) | the reports |
| Z2-R-DEVT-103, -104: `Libgraph` and `Complexity` green on the scaffold (DEVT-P1) | the step reports |
| budget rows allocated from three cold runs each: Z2-B-PROC-005 to -009, -011 to -022, -006 and the new Shipping-build row (allocation rule of `Zenith2/Docs/Design/PROC.md` → "Budget rows"); Z2-B-PROC-001 from the Z2 lane's `lane-storage.json` after `-WarmOnly` | `allocation-stage-0.json`, the `Budgets.md` diff, the `zagent.project.json` timeout edit |
| `libgraph_baseline.json` written by the re-pin (`acceptedAt` no longer null) | `baseline-run.json`, the diff |
| the review record of the baseline commit | the ticket's review block |
| the hand-off ledger audited: every hand-off ID of `findings/A5_proc_handoffs.md` and of every area design's PROC hand-off list appears in a row of `Zenith2/Docs/Design/PROC/GateList.md` → "Hand-off ledger", with its disposition (Z2-R-PROC-131) | a table in the work log of Z2T-PROC-025, ID by ID |
| `Test-AcceptanceRecord.ps1 -Stage 0` exit 0 and the tracked copies under `Zenith2/Docs/Research/Acceptance/stage-0/` | the evidence commit |

## Stage 1 acceptance

Run-Committed suite (stage 1), added to stage 0's:

| # | Line | When |
|---|---|---|
| 1 | `pwsh -NoProfile -File Zenith2\Tools\DevTool\Scripts\Invoke-LinuxTsan.ps1 -Configure -Build -Test -Label tsan-smoke` (the TSan smoke on the host the Stage 1 TASK brought up) | always |

| Area | Requirements evidence collected (from the hand-offs) |
|---|---|
| CORE | Z2T-CORE-066 |
| COMPRESS | the outputs of the Stage 1 ruling and harness tickets (Z2-R-COMPRESS-100 to -104) |
| MOD | Z2T-MOD-001, -003 |
| PLAT | Z2T-PLAT-098 (ruling Z2T-PLAT-002) |
| RHI | Z2T-RHI-012 |
| GEOM | Z2T-GEOM-003 |
| RPROD | Z2T-RPROD-001 to -005 |
| ASSET | Z2T-ASSET-003 |
| ECS | Z2T-ECS-007 (rulings 001 to 004 and 006 as inputs) |
| LIGHT | Z2T-LIGHT-003, -005 |
| SCRIPT | Z2T-SCRIPT-018 (rulings 013, 015, 021, allocation 017) |
| SCENE | Z2T-SCENE-001, -002, -093 |
| ENV | Z2T-ENV-001, -002, -006 |
| EDIT | Z2T-EDIT-001 to -006 |
| SHIP | Z2T-SHIP-041, -042 |
| SAVE | Z2T-SAVE-001 |
| AUDIO | Z2T-AUDIO-001 to -003 |
| DEVT | Z2T-DEVT-001 to -013 |
| CONTENT | Z2T-CONTENT-001 (Z2-R-CONTENT-133) |
| PACK | Z2T-PACK-003 (Z2-R-PACK-163) |
| CHAR | Z2T-CHAR-003 (Z2-R-CHAR-022) |
| CINE | Z2T-CINE-002 (Z2-R-CINE-109) |
| PROC | Z2T-PROC-030 (revalidation predicate), Z2T-PROC-034 (the CORE justification-record entries) |
| every area | the Stage 1 items of `Zenith2/Docs/Roadmap.md` → "Stage 1 acceptance ticket" |

## Stage 2 acceptance

Run-Committed suite (stage 2), added to stages 0 and 1:

No line beyond stages 0 and 1: the two `Sanitizers` lines run among `@category-lines` from Z2T-PROC-041, and without ticket context each runs fully, which is what `-All` would add (finding m3 of review round 2), so they are not listed twice.

Human steps: the fuzz campaign (procedure step 3) and the HWASan defect rows on the S24. Budget rows allocated here: Z2-B-PROC-024 and -031 from their first real cold runs; SEC's campaign rows (X415) are SEC's to allocate from the campaign's record.

| Area | Evidence |
|---|---|
| CORE | Z2T-CORE-084 to -089 |
| COMPRESS | Z2T-COMPRESS-028, -030, -031 to -034 |
| MOD | Z2T-MOD-035, -038, -039 |
| PLAT | Z2T-PLAT-042 to -045 |
| RHI | Z2T-RHI-049 to -052, -077 to -080 |
| GEOM | Z2T-GEOM-017, -054 to -057 |
| RPROD | Z2T-RPROD-056 to -059 |
| ASSET | Z2T-ASSET-049 to -051 |
| ECS | Z2T-ECS-077, -082 to -087, -092 to -095; the 16-run determinism matrix per backend |
| LIGHT | Z2T-LIGHT-015 |
| SCRIPT | Z2T-SCRIPT-023 |
| SCENE | Z2T-SCENE-055 to -057; the Stage 2 goldens of -052 |
| SHIP | Z2T-SHIP-067 |
| SAVE | the `SaveRequired_Stage2Replay` group |
| AUDIO | Z2T-AUDIO-004, -005 |
| DEVT | Z2T-DEVT-039, -063 to -068; rulings -029, -060 |
| EDIT | the `EditorRequired_*` group of Z2T-EDIT-019 |
| PACK | Z2T-PACK-011 |
| CINE | the D-SEL-VIDEO research: harness Z2T-CINE-007, ruling Z2T-CINE-011 (Z2-R-CINE-110) |
| SEC | the SecurityConformance step green (Z2T-PROC-044) and the first campaign's `campaign.json` |
| MAINT | steps A1 to A5 |

## Stage 3 acceptance

Run-Committed suite (stage 3), added to stages 0 to 2:

| # | Line | When |
|---|---|---|
| 1 | `pwsh -NoProfile -File Zenith2\Tools\Gates\IntegrationSmoke.ps1` | `fileExists:Zenith2/Tools/Gates/IntegrationSmoke.ps1` |

`BackendParity` runs among the category lines from Z2T-PROC-050. Human steps: the hardware parity half on the RTX 3070, and SEC's HWASan regression replay on the S24.

| Area | Evidence |
|---|---|
| COMPRESS | the Stage 3 requirement Z2-R-COMPRESS-011's ticket output |
| MOD | Z2T-MOD-047, -048 |
| PLAT | Z2T-PLAT-076 to -082 |
| RHI | Z2T-RHI-100, -102 to -106 |
| GEOM | Z2T-GEOM-060, -069, -075, -081, -091, -092 |
| RPROD | Z2T-RPROD-102 to -109 |
| ASSET | Z2T-ASSET-060, -061 |
| ECS | Z2T-ECS-098 |
| LIGHT | Z2T-LIGHT-074 to -079, -081 |
| SCRIPT | Z2T-SCRIPT-088 to -102 (089, 100, 101 human device and measurement runs) |
| SCENE | Z2T-SCENE-058 (S24 HWASan), -073 to -076 |
| ENV | Z2T-ENV-061 to -066 |
| SAVE | `SaveRequired_Stage3Format`, `_Stage3Snapshot`, `_Stage3System`, `_Stage3SaveRuntime`; records 073, 089 |
| AUDIO | Z2T-AUDIO-084 to -090 |
| DEVT | Z2T-DEVT-083, -101 to -103 |
| EDIT | Z2T-EDIT-075 to -083 groups |
| CONTENT | Z2T-CONTENT-021 (the S3 requirements of CONTENT-P1) |
| CHAR | Z2T-CHAR-012, -013, -014, -017 (Z2-R-CHAR-019 and -020 through ANIM's ruling Z2T-ANIM-068) |
| SEC | the Shipping exposure layers green (Z2T-PROC-054) |

## Stage 4 acceptance

Run-Committed suite (stage 4): no line beyond stages 0 to 3. MOD's `CMake_Declarations_MirrorMatches` runs inside the category lines with `ZENITH_ACCEPTANCE_STAGE=4` exported by the run.

| Area | Evidence |
|---|---|
| CORE | Z2T-CORE-090 to -092 |
| MOD | Z2T-MOD-053, -054 |
| PLAT | Z2T-PLAT-085 |
| RHI | Z2T-RHI-111 |
| GEOM | Z2T-GEOM-101, -102 |
| RPROD | Z2T-RPROD-111 |
| ASSET | Z2T-ASSET-072, -090 to -092 |
| ECS | Z2T-ECS-100 |
| LIGHT | Z2T-LIGHT-095 to -097 |
| SCRIPT | Z2T-SCRIPT-104 |
| SCENE | Z2T-SCENE-088 to -090 |
| ENV | Z2T-ENV-106 to -108, -111 to -116 |
| SAVE | `SaveRequired_Stage4` |
| AUDIO | Z2T-AUDIO-111 to -115 |
| DEVT | Z2T-DEVT-107 |
| EDIT | Z2T-EDIT-124 to -127 |
| CONTENT | Z2T-CONTENT-072, -074 to -077 (`z2-acceptance-content` runs `Test_DccMatrix_EverySupportedRouteRecorded`), the `needs-human` record -073, the measurement -083 |
| PACK | Z2T-PACK-020 |
| CHAR | Z2T-CHAR-003, -032, -054, -072, -073, -075 to -082, -088 |
| CINE | Z2T-CINE-084 to -090, -092, -093 (hardware video, `needs-human`), -099, -108, -109 |
| MAINT | steps A1 to A5, including the first calibration (Z2T-PROC-062) |

## Stage 5 acceptance

Run-Committed suite (stage 5): no line beyond stages 0 to 4. `PathParity` runs among the category lines from Z2T-PROC-070. LIGHT's `PT_*` and `AE_*` suites run under ADVGFX's path-tracer runner (LIGHT-P3) and are selected by `@acceptance-labels`: they carry a `z2-acceptance-*` label, and `Test-AcceptanceSuites.ps1` fails while they do not (cross-area note to LIGHT and ADVGFX).

| Area | Evidence |
|---|---|
| RHI | Z2T-RHI-114, -116 |
| GEOM | Z2T-GEOM-115, -119 |
| RPROD | Z2T-RPROD-120, -122, -124, -126, -127 |
| LIGHT | Z2T-LIGHT-117 to -125 |
| ENV | Z2T-ENV-133, -134 |
| AUDIO | Z2T-AUDIO-116, -117 |
| PLAT | Z2T-PLAT-090, -091 |
| EDIT | Z2T-EDIT-133, -136, -138 |
| CONTENT | Z2T-CONTENT-080 |
| CHAR | Z2T-CHAR-084 (the CinematicSlice hair sign-off, `needs-human`) |
| CINE | Z2T-CINE-101, -103 |

## Stage 6 acceptance

Run-Committed suite (stage 6): no line beyond stages 0 to 5. `ApkCheck` with the bundle splits runs among the category lines from Z2T-PROC-080. Human steps: SEC's security hardening evidence (`Zenith2/Docs/Design/SHIP/Qualification.md` → "Security and compatibility items").

| Area | Evidence |
|---|---|
| COMPRESS | the Stage 6 requirements' outputs (091, 106, 109) |
| PLAT | Z2T-PLAT-094, -095 |
| RHI | Z2T-RHI-118, -119 |
| GEOM | Z2T-GEOM-117 |
| RPROD | Z2T-RPROD-129, -130 |
| ASSET | Z2T-ASSET-095 |
| ECS | Z2T-ECS-104 (W-Dense qualification) |
| LIGHT | Z2T-LIGHT-127 (thermal) |
| SCRIPT | Z2T-SCRIPT-108 |
| SCENE | Z2T-SCENE-091 |
| ENV | Z2T-ENV-135 |
| SHIP | Z2T-SHIP-099 to -105 |
| SAVE | `SaveRequired_Stage6`; records 097, 105, 106, 110, 112 to 114 |
| AUDIO | Z2T-AUDIO-118 to -120 |
| CONTENT | Z2T-CONTENT-081 |
| PACK | Z2T-PACK-059, -060 to -063, -065 |
| CHAR | Z2T-CHAR-086 |
| CINE | Z2T-CINE-104 to -106 |

## Stage 7 acceptance

Run-Committed suite (stage 7): the Stage 7 expansion lines run among the category lines. Evidence: for each expansion delivered, its conformance suites, its device or runtime evidence (human) and its measured budget rows; RPROD Z2T-RPROD-132; PLAT Z2T-PLAT-097; EDIT's Stage 7 requirements 177 and 178; MAINT steps A1 to A4 (Stage 7 has no roll) with the calibration Z2T-PROC-095.

## Mid-sprint integration checks

Each stage's `Z2 S<N> mid-sprint integration check` (category `Zenith2`, `needs-human`, kind `integration`, fix version `Z2 Stage <N>`) is due once half the sprint's story points are Done, and it blocks the acceptance (`Zenith2/Docs/Roadmap.md` → "Mid-sprint integration check"). Its Definition of Done:

1. Run the flagship playthrough scripts (before Stage 4, the workloads that exist) on the RTX 3070 desktop and the S24, measure them against `Zenith2/Docs/Budgets.md`, and file each regression as a BUG in the sprint on the regressing area's epic ([Z2-N-CONTRACT-509]).
2. Run MAINT's steps M1 to M4 (`Zenith2/Docs/Design/MAINT/Upkeep.md` → "Stage acceptance and mid-sprint steps"): the maintenance status, the advisory review and the due-work review from Stage 2, and the ownership audit at Stages 0 and 1. A gate phase above 80 % of its warm cap ([Z2-N-BUDGET-353]) is filed here as a headroom ticket from MAINT's template (`Zenith2/Docs/Design/MAINT/Upkeep.md` → "Gate headroom triage"), which is how hand-off MT3 (e) is met without a `TICK.md` edit.
3. Run RESEARCH's four steps (P-RS5; `Zenith2/Docs/Design/RESEARCH/Tooling.md` → "Which benchmarks run in which gate"): `Update-Zenith2ResearchMachineState` for both platforms, `Get-Zenith2ResearchRevalidation`, `Test-Zenith2ResearchArchive`, and one revalidation run for each fired trigger whose decision governs a ticket of the next stage.

The Stage 0 check is different. It regenerates and `-Check`s the three harnesses, runs `zagent doctor`, `Test-HarnessParity.ps1` and `Test-ZagentClient.ps1`, compares `zagent gates` for the sample ZEN and ZM tickets byte for byte with the pre-Stage-0 capture, files any drift as a PROC BUG, and runs MAINT's M4.
