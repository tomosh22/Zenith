# MAINT design — gate baseline and complexity profile upkeep

This file is part of the MAINT design family (entry: `Zenith2/Docs/Design/MAINT.md`). It specifies how the two pieces of gate data that drift with the code are kept true: the libgraph inbound-surface baseline and the Zenith2 complexity profile. PROC owns both files, their formats, the libgraph output formats and the gate steps that read them (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "libgraph_baseline.json", "complexity_profile.json", "Libgraph outputs"); MAINT owns when and how they are re-pinned and recalibrated, the evidence each change carries, the measurement harness, and the admission and audit of complexity exceptions.

## Scope of this file

| Concern | Section |
|---|---|
| the inputs the Libgraph step reads and which of them MAINT keeps | "Libgraph checker inputs" |
| when and how the baseline is re-pinned, and what is reviewed first | "Baseline re-pinning" |
| the committed report of a stage's surface growth | "Surface growth report" |
| a libgraph tool change | "Libgraph tool changes" |
| the inputs the Complexity step reads | "Complexity checker inputs" |
| deriving thresholds from the measured distribution | "Complexity calibration" |
| the rules the calibration functions enforce | "Calibration rules" |
| the measurement script and its data source | "Complexity measurement harness" |
| the committed calibration record | "Calibration record" |
| when an exception is admitted and how it expires | "Complexity exceptions" |
| what lands at Stage 0 | "Stage 0 contents" |

<!-- informative -->
Both files are protected (`Zenith2/Tools/Gates/**`), so every change to them is a `needs-human` edit ([Z2-N-CONTRACT-178], [Z2-N-CONTRACT-419]). MAINT therefore never writes either file in a loop ticket: it produces the evidence and the decision, and the protected edit is made by `Run-Committed.ps1` in the stage acceptance (the baseline) or by a PROC `needs-human` ticket blocked by MAINT's ruling ticket (the profile). PROC adopts the acceptance steps, the per-stage apply tickets and the output schemas in its own revision (ruling MAINT-R1-RULINGS R5, cross-area note MT3).
<!-- /informative -->

## Libgraph checker inputs

| Input | Produced by | Read by the `Libgraph` step for | Kept true by |
|---|---|---|---|
| `Zenith2/Tools/Gates/libgraph_baseline.json` | the re-pin of a stage acceptance, through `Run-Committed.ps1` | rule LG10 (inbound-surface growth) | "Baseline re-pinning" (MAINT procedure, PROC's protected edit) |
| `Zenith2/Tools/Gates/libgraph_policy.json` | PROC tickets | rules LG1 to LG11 | PROC |
| `${ZENITH_OUT}/declared_deps.json` | `zenith_end_build` | declared edges, kinds, backends | every library's `CMakeLists.txt` ([Z2-N-ARCH-071]) |
| `${ZENITH_OUT}/third_party_targets.json` | `zenith_end_build` | third-party ownership | SHIP |
| the CMake File API reply of the `win` tree | CMake | targets, compile groups, link closures | the build |
| `${ZENITH_OUT}/gates/libgraph/surface.json`, `surface_delta.json` | the step itself, in the schemas PROC defines (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs") | its own outputs; MAINT's growth report reads them | PROC |
| the review records under `Zenith2/Docs/Board/tickets/` | PROC-API's `zagent review record` | LG10 coverage without ticket context | the review flow |

## Baseline re-pinning

The baseline is re-pinned once per stage, in `Z2 S<N> stage acceptance`, from the accepted tree (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "The inbound-surface review rule"). MAINT's procedure for that step, executed by the human after the release step (step A2 of `Zenith2/Docs/Design/MAINT/Upkeep.md` → "Stage acceptance and mid-sprint steps"):

1. Run `Run-Committed.ps1 -Acceptance -Stage <N>`; its `Libgraph` line runs without ticket context and fails with `LG10-UNREVIEWED` when any growth pair of the stage lacks a review record, and the acceptance stops there until a review covers it.
2. Run `pwsh -NoProfile -File Zenith2/Tools/Maintenance/New-Zenith2SurfaceGrowthReport.ps1 -Stage <N>`, which reads `surface.json`, `surface_delta.json`, the baseline and the previous stage's committed report, and writes the report of "Surface growth report" to `Zenith2/Docs/Research/Libgraph/stage-<N>/surface_growth.md` and `surface_growth.json`.
3. The human reads the report: every library flagged `largestGrowth` (its inbound pair count grew by more than 25 % or by more than 100 pairs since the previous pin, or it gained a consumer library since the previous report) is listed under `## Largest growth`; for each, the human either accepts it (one sentence in the report's `## Disposition` section) or files a refactor ticket on the owning area's epic (`Zenith2/Docs/Design/MAINT/Upkeep.md` → "Ownership file") to shrink the surface, citing [Z2-N-CONTRACT-025].
4. Commit the report with its disposition (`MAINT: surface growth report stage <N>`, exempt from the release-fix scope, `Zenith2/Docs/Design/MAINT/Versioning.md` → "Version state machine") before the re-pin, because the re-pin requires a clean tree and admits only commits under `Zenith2/Docs/` or `Zenith2/Tools/Maintenance/` since the suites ran; `RES-INCOMPLETE` (`Zenith2/Docs/Design/MAINT/Tests.md` → "Research record rules") is the test that the disposition is filled.
5. Re-pin the baseline with `pwsh -NoProfile -File Zenith2\Tools\Gates\Run-Committed.ps1 -Acceptance -Stage <N> -WriteBaseline` (`Zenith2/Docs/Design/PROC/BaseSteps.md` → "-Acceptance -Stage N -WriteBaseline"); MAINT never runs `Libgraph.ps1 -WriteBaseline` directly and has no other path to the protected file.
6. Commit the baseline alone in the acceptance diff, where the acceptance ticket's review record covers it.

<!-- elaboration -->
The thresholds of step 3 (25 % or 100 pairs) select what a human reads closely; growth below them is still listed in full in the report and was already reviewed per diff by the review rule.

## Surface growth report

The report reads PROC's `surface.json` (one record per library with its `{ header, symbol, consumers }` pairs) and `surface_delta.json` (the per-library `added` and `removed` pair lists against the baseline) in the schemas of `Zenith2/Docs/Design/PROC/StructuralGates.md` → "Libgraph outputs". The committed baseline holds pairs alone, so the consumer lists before the current tree come from the previous stage's committed MAINT report, never from the baseline.

<!-- elaboration -->
`surface_growth.json` (named so that it never collides with libgraph's own `surface_delta.json`):

```json
{
    "schema": "zenith2.surface-growth-report",
    "schemaVersion": 1,
    "stage": 3,
    "engineVersion": "3.0.0",
    "baselineAcceptedAt": "Z2 S2 stage acceptance",
    "baselineHeadSha": "<40 hex>",
    "headSha": "<40 hex>",
    "previousReport": "Zenith2/Docs/Research/Libgraph/stage-2/surface_growth.json",
    "libraries": [
        {
            "name": "ZenithECS",
            "owner": "ECS",
            "pairsBefore": 312,
            "pairsAfter": 401,
            "added": [ { "header": "ZenithECS/Zenith_Query.h", "symbol": "Zenith_Query::Next", "consumers": [ "ZenithAI", "ZenithScript" ] } ],
            "removed": [ { "header": "ZenithECS/Zenith_World.h", "symbol": "Zenith_World::LegacyIterate" } ],
            "consumersBefore": [ "ZenithAI", "ZenithScene" ],
            "consumersAfter": [ "ZenithAI", "ZenithScene", "ZenithScript" ],
            "largestGrowth": true
        }
    ]
}
```

- `libraries[]` holds every library present in either the baseline or the current surface, byte-ordered by `name`; a library absent from the baseline has `pairsBefore` 0 and every pair under `added`.
- `pairsBefore` counts the baseline's pairs of the library; `pairsAfter` the current surface's.
- `consumersBefore` is the previous report's `consumersAfter` for the library; for the first report (Stage 2, `previousReport` null) it is `null` and the new-consumer criterion of step 3 is not applied.
- `consumersAfter` is the union of every current pair's `consumers`, byte-ordered.
- `owner` is the owning area of the library's directory (`Get-Zenith2Owner`).
- `largestGrowth` applies the selection of step 3.

`surface_growth.md` renders the same data: `## Summary` (one table: library, owner, before, after, delta, new consumers), `## Largest growth` (one subsection per selected library listing its added pairs), `## Removed` (pairs that left the surface), `## Disposition` (written by the human). The script writes the first three sections and the single line `Written by the release operator.` under `## Disposition`; the rule `RES-INCOMPLETE` (`Zenith2/Docs/Design/MAINT/Tests.md` → "Research record rules") fails a committed report that still contains that line.

## Libgraph tool changes

A change to `Tools/libgraph/**` (a PROC ticket) can change what libgraph measures without any Zenith2 code changing, which makes subsequent tickets report growth that no diff caused. The PROC ticket that changes libgraph therefore (cross-area note MT3 (b)):

1. runs `Run-Committed.ps1` without ticket context on its base tree and on its result, and attaches both `surface_delta.json` files as evidence;
2. when the two surfaces differ, records the difference in its result so that the reviewers of the tickets before the next re-pin recognise tool-caused growth, and is scheduled as close as possible before the next stage acceptance, whose re-pin absorbs the difference;
3. never lands while a lane is running (it edits a Zenith-side tool every gate run reads).

## Complexity checker inputs

| Input | Produced by | Kept true by |
|---|---|---|
| `Zenith2/Tools/Gates/complexity_profile.json`, profile `zenith2` (format: `Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json") | Z2T-PROC-010 at Stage 0; PROC `needs-human` apply tickets afterwards | "Complexity calibration", "Complexity exceptions" |
| `Zenith2/Tools/Gates/complexity_lints_allowlist.txt` | Z2T-PROC-010 (empty) | stays empty: a lint finding is fixed, never allowlisted, unless a decision admits it |
| `Tools/analyze_code_complexity.py` with the generic keys `include_roots`, `max-file-loc` and `exceptions`, and the entity dump option the harness needs | Z2T-PROC-004 (cross-area note MT3 (j) for the dump option) | PROC |
| every C and C++ source under the profile's include roots | the tree | every ticket |

## Complexity calibration

The thresholds are calibrated from the measured distribution of the Zenith2 tree ([Z2-N-CONTRACT-419]). Under ruling MAINT-R1-RULINGS R3 (orchestrator ruling; the human may override), calibration runs at the acceptance of Stage 4 and of every subsequent stage, and on demand when a human rules that a threshold blocks coherent code. Stages 2 and 3 keep the starting thresholds, so the code of the first stages is not fitted to a small, regular tree.

<!-- elaboration -->
Procedure (a `needs-human` MAINT ruling ticket, `Z2 S<N> complexity calibration`, kind `decision`, one per stage 4 to 7, drafted by MAINT; followed by a PROC `needs-human` apply ticket blocked by it, drafted by PROC, Z2-D-084):

1. On the accepted tree, run `pwsh -NoProfile -File Zenith2/Tools/Maintenance/Measure-Zenith2Complexity.ps1 -Out Zenith2/Docs/Research/Complexity/stage-<N>/`.
2. The script computes, for each metric, the distribution over every entity the gate measures, excluding entities covered by an exception: functions for `max-cc`, `max-cognitive`, `max-nesting`, `max-loc`; files for `max-file-loc`.
3. For each metric it computes `new = min(current, max(floor, ceil(observedMax × 1.25)))`:

| Metric | Floor |
|---|---|
| `max-cc` | 15 |
| `max-cognitive` | 15 |
| `max-nesting` | 4 |
| `max-loc` | 80 |
| `max-file-loc` | 1,000 |

4. It audits every exception ("Complexity exceptions" step 5) and writes the proposed exception list.
5. It writes `calibration.json` and `calibration.md` ("Calibration record") and `complexity_profile.proposed.json`, a copy of the current profile file with the new `fail_on` values and the audited `exceptions`, byte-identical otherwise; `calibration.json` records `baseSha256`, the SHA-256 of the profile file it was generated from, and `proposedSha256`, that of `complexity_profile.proposed.json`, each as 64 lower-case hexadecimal digits over the file's exact working-tree bytes with no end-of-line normalisation (a PowerShell writer lower-cases `Get-FileHash`'s output), and `decision: null`.
6. The human appends the ruling `Z2-D-<next free> Zenith2 complexity profile calibration at Stage <N>` to `Zenith2/Docs/DecisionLog.md` in the ruling ticket's diff (Z2-D-097), naming the old and new value of every metric and every exception added, tightened or removed, and writes its ID into `calibration.json`'s `decision` member and the last line of `calibration.md` (`Decision: Z2-D-<n>`) in the same diff.
7. The PROC apply ticket first compares the SHA-256 of the current `Zenith2/Tools/Gates/complexity_profile.json` with `baseSha256` and refuses when they differ (an exception admitted in between would be overwritten; the ruling ticket is then re-run on the current profile); otherwise it copies `complexity_profile.proposed.json` over the profile and runs the `Complexity` step.

## Calibration rules

| Rule ID | Rule |
|---|---|
| CPX-NEVER-LOOSEN | `new ≤ current` for every metric; loosening is a separate human ruling with its own decision entry, never part of a calibration |
| CPX-FLOOR | `new ≥ floor` for every metric, so a small or very regular tree cannot calibrate the profile into rejecting ordinary code |
| CPX-HEADROOM | when `ceil(observedMax × 1.25) ≥ floor` and `≤ current`, `new` equals it: one quarter of headroom above the worst measured entity |
| CPX-FAILING-TREE | for a metric whose `observedMax` exceeds `current` (the measured tree already fails it; the harness measures such trees on purpose), `new = current`: a failing tree is never calibrated tighter |
| CPX-PASSING-TREE | when the measured tree passes the current profile, every non-exception entity of it passes the calibrated thresholds (`new ≥ ceil(observedMax × 1.25) ≥ observedMax`, or `new = current ≥ observedMax`) |
| CPX-AUDIT | every exception's action is one of `kept`, `tightened`, `removed-under-threshold`, `removed-entity-gone` per "Complexity exceptions" step 5 |
| CPX-MINIMAL-DIFF | the proposed profile differs from the input profile only in `fail_on` values and `exceptions` |
| CPX-BASE | `calibration.json`'s `baseSha256` equals the SHA-256 of the profile the proposal was generated from, and `proposedSha256` that of the proposed profile, both in the encoding of "Complexity calibration" step 5 |

`max-forbidden-tokens`, `max-missing-pragma-once` and `max-include-cycles` stay 0 and are not calibrated.

<!-- informative -->
The headroom factor and the Stage 4 start are ruling R3's answer to the throughput cost of a pure ratchet: fitting the ceilings to the observed maximum of a Stage 2 tree would make most Stage 3 to Stage 5 parsers, builders and importers wait on an exception ruling. The calibration record keeps the percentiles so a human ruling can choose to tighten further and admit exceptions instead.
<!-- /informative -->

## Complexity measurement harness

`Zenith2/Tools/Maintenance/Measure-Zenith2Complexity.ps1 -Out <dir> [-ProfileFile <path>] [-Repo <path>]`:

1. Runs the analyzer with the pinned `python` and the same profile and arguments the `Complexity` step uses, plus the entity dump option, writing `${ZENITH_OUT}/maintenance/complexity/entities.json`.
2. Ignores the analyzer's exit code (the measurement must run over a tree that currently fails, so a human can see the distribution of an on-demand calibration).
3. Computes count, p50, p90, p99, p99.5 and max per metric with the nearest-rank method (`p_q = sorted[ceil(q × n) − 1]`), then the proposals of "Complexity calibration".
4. Writes the three output files of step 5 of that section; exits 0, 2 (usage), 3 (the analyzer or the pinned `python` missing, or `entities.json` absent or unreadable).

The analyzer's JSON report exports per-file aggregates and top-N function lists only, not every function the gate measures, so it cannot give the true maximum or the percentiles. PROC's generic option `--dump-entities <path>` (cross-area note MT3 (j), Z2T-PROC-004; `Zenith2/Docs/Design/PROC/Harness.md` → "dump-entities") writes every measured entity after the profile's excludes are applied, with each entity an exception covers flagged `excepted` and its measured values kept (the exception audit needs them):

```json
{
    "schema": "complexity-entities",
    "schemaVersion": 1,
    "functions": [ { "file": "<repo-relative>", "name": "<qualified name>", "startLine": 12, "cc": 4, "cognitive": 3, "nesting": 2, "loc": 31, "excepted": false } ],
    "files": [ { "path": "<repo-relative>", "lines": 412, "excepted": false } ]
}
```

`lines` is the physical line count `max-file-loc` measures. An analyzer without the option (an older checkout, or a stand-in in a test) makes the harness exit 3 with `Measure-Zenith2Complexity: the analyzer has no --dump-entities option`; no calibration is due before the Stage 4 acceptance.

The module function `Get-Zenith2ComplexityProposal -Entities <object> -Profile <object>` takes the parsed `entities.json` and the parsed profile and holds steps 3 and the audit, so the tests of `Zenith2/Docs/Design/MAINT/Tests.md` → "PowerShell check tests" (family `CPX`) exercise it over hand-written entity files without running the analyzer.

## Calibration record

<!-- elaboration -->
`Zenith2/Docs/Research/Complexity/stage-<N>/calibration.json`:

```json
{
    "schema": "zenith2.complexity-calibration",
    "schemaVersion": 1,
    "stage": 4,
    "engineVersion": "4.0.0",
    "headSha": "<40 hex>",
    "baseSha256": "<64 lower-case hex of the profile file the proposal was generated from>",
    "proposedSha256": "<64 lower-case hex of complexity_profile.proposed.json>",
    "decision": "Z2-D-<n>",
    "metrics": [
        { "key": "max-cc", "entities": 18422, "p50": 2, "p90": 6, "p99": 14, "p995": 17, "max": 19, "current": 25, "floor": 15, "proposed": 24, "new": 24 }
    ],
    "exceptions": [
        { "key": "max-cc", "entity": "Zenith_ToVkFormat", "limit": 170, "measured": 131, "action": "tightened", "newLimit": 164 }
    ]
}
```

`proposed` is `max(floor, ceil(max × 1.25))`; `new` is `min(current, proposed)`. `action` is one of `kept`, `tightened`, `removed-under-threshold`, `removed-entity-gone`. `calibration.md` renders the metrics table and the exception table and ends with the line `Decision: Z2-D-<n>`; the harness writes `decision: null` and the placeholder line `Decision: written by the ruling.`, and the ruling ticket fills both (step 6), so `RES-INCOMPLETE` refuses a record whose ruling never landed. The directory is keyed by stage, so a release fix that moves PATCH after the record is written never moves the record; the engine version is recorded inside.

## Complexity exceptions

An exception lets one named entity exceed one threshold (format: PROC's `exceptions` entries `{ key, entity, limit, decision, comment }`).

<!-- elaboration -->
1. **Request.** The area that needs one records a PROC hand-off note giving the full entry; its implementing ticket stays blocked on the PROC apply ticket, and the worker of an implementation ticket that meets an unplanned threshold failure reports the contradiction instead of splitting the function to pass ([Z2-N-CONTRACT-420]).
2. **Admission criteria**, all of which the ruling states:
   - A1: the entity is one coherent unit (a dispatch table, a generated-like mapping over a closed enumeration, a parser state machine, a test case asserting a long fixed sequence), and splitting it would be artificial fragmentation;
   - A2: a table-driven or data-driven form was considered and is worse, with the reason;
   - A3: `limit` is `ceil(measured × 1.25)`, the calibration's headroom, never more;
   - A4: the entity has its own tests, named in the comment.
3. **Decision.** The ruling ticket appends a `Z2-D-` entry naming the entity, the metric, the measured value and the limit (Z2-D-097); the entry's ID is the entry's `decision`.
4. **Apply.** A PROC `needs-human` ticket adds the entry.
5. **Audit**, at every calibration: an exception whose entity is gone is removed (`removed-entity-gone`); one whose measured value is at or under the new threshold is removed (`removed-under-threshold`); otherwise its limit becomes `ceil(measured × 1.25)` when that is lower (`tightened`), else it is `kept`.
6. **Review.** Reviewer conventions reject artificial fragmentation and prefer a justified exception (Z2-D-007 rationale; the Contracts' rule-to-mechanism matrix).

## Stage 0 contents

MAINT agrees with the Stage 0 contents PROC specifies for Z2T-PROC-010: the baseline lands with `"libraries": []`, and the profile lands with the starting thresholds `max-cc` 25, `max-cognitive` 30, `max-nesting` 5, `max-loc` 150, `max-file-loc` 1500, the three zero-valued lint and cycle keys, and an empty `exceptions` list (`Zenith2/Docs/Design/PROC/StructuralGates.md` → "complexity_profile.json"). No calibration runs before the Stage 4 acceptance (ruling MAINT-R1-RULINGS R3); until then the starting thresholds stand, and an exception follows "Complexity exceptions".
