---
id: "9780d773-2301-4b69-bd25-3931d0dee0d9"
key: "ZEN-78"
type: "TASK"
title: "tick_gates measure phase reports deferred gate lines and the automated-test registry count"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 78
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T09:14:56.086Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
Suggestions row 2026-08-25 (gates): *"`tick_gates.ps1 -Phase measure` should report the gate lines it deferred, and should print the registry count as well as the unit pin."* A measure phase that defers a gate line silently leaves the verify phase to discover it, and the automated-test registry count (the LIVE PIN block's second number) is read by hand from the harness log today.

`Tools/tick_gates.ps1`: in `-Phase measure`, print every deferred line with its reason and write them into `tick_gates.json` as `deferred: [...]` beside `skipped`; parse the automated-test registry count from the harness summary and print it beside the unit `ran` count. Assert-script coverage in the `Tools/Test_*` harness over a fixture run log.

## Definition of Done
- [ ] Measure reports deferred lines and the registry count, both also in `tick_gates.json`, with assert-script coverage
- [ ] The Suggestions row is resolved with this key


## Comments

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Done.** Committed to `master` as `22410afb`.

**Most of this shipped already** — the cited Suggestions row was `Status: done`, measure already
reports both deferred lines and the registry count, and `deferred` was already in
`tick_gates.json`. I confirmed that half from direct experience: I have been reading
`deferred: [...]` out of that file all session.

**The gap was that the registry count was console-only**, measured across three real
`tick_gates.json` files from this session whose keys were exactly `…steps, skipped, deferred,
flakes` and nothing else. That matters because the console is the one channel this file exists
to replace — `tick_gates.ps1`'s own header and the tick protocol both say to branch on the JSON
and never on prose. A number a tick carries into `Status.md`'s LIVE PIN block had no home in the
structured verdict.

`Write-Result` now emits `registry`, a per-game map, **unconditionally** (`{}` when nothing was
measured, so "no games" is distinguishable from "an older script"). Additive — no existing field
changed, and Vault's `tick-checks.mjs` reads the verdict by property presence, so the new key is
inert to it. `Get-AutomatedRegistryCount` gained an optional `-RepoRoot` so it is fixture-testable.

**Verified empirically, not from the report.** A real measure run writes `registry: {}`; against
the live checkout the helper returns Combat 17, Zenithmon 73, RenderTest 24, `$null` for a game
with no Tests dir. **Zenithmon's 73 matches the hand-maintained "registry 73" in its `Status.md`
LIVE PIN block** that `doc_lint` C7 also reconciles — so the count is right, not merely
well-formed. Mutations discriminate: dropping the paren anchor reds exactly the paren-less test,
dropping the JSON field reds exactly the three registry assertions. 50/50 overall.

**★ One gap recorded rather than claimed closed.** There is still no *direct* assertion that a
deferred line reaches `$script:DeferredLines` at runtime — that population sits behind the
harness's `-NoRun` guard, which exists precisely so a test claiming a gate ran cannot live in
this file. The predicates deciding planned-vs-deferred are covered; the three-line subtraction
consuming them is not. Closing it means extracting that into a pure helper — editing the script
every gate run depends on — for a failure mode that would be loud and immediate. Judged not
worth it; the worker flagged it rather than letting the proxy stand for the real thing.

**Gate note.** Verify went red twice on the `Tools/Vault` `npm test` line, on **different tests
each time** (`cli-process` 6390 ms, then a catalog-families case at 28081 ms), 825/826 both.
The innocence argument is airtight here: the whole diff is `Tools/tick_gates.ps1` +
`Tools/Test_TickGates.ps1` — **zero JavaScript, nothing under `Tools/Vault/`**. Both named tests
are on `ZEN-D-024`'s list. The reds skipped all five `doc_lint` lines; run by hand, exit 0 on
every game. `zagent guard` clean, zero excursions.

Durable findings: none — the behaviour is pinned by its own assert script, and the flake is
`ZEN-D-024`.

<!-- agent:worklog:start ZEN-78 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Outcome**: committed to `master` as `22410afb` · ran on sonnet/low via SIMPLE+LOW

**★ Most of this shipped already — sixth consecutive ticket whose cited Suggestions row
(`s3f3xs6…`) was `Status: done`.** But the DoD has five sub-clauses in three different states,
which is why the row's "both halves exist" was not the whole answer:

| DoD sub-clause | State at claim |
|---|---|
| measure **reports** deferred lines | **done** |
| deferred **in `tick_gates.json`** | **done** |
| measure **reports** the registry count | **done** (console) |
| registry count **in `tick_gates.json`** | **✘ missing** |
| assert-script coverage for either | **✘ missing** |

I could confirm the `deferred` half from direct experience rather than inspection: I have read
`deferred: [...]` out of `tick_gates.json` all session — 12 lines on a Tooling measure, 3 on an
Engine one.

**Why the missing half mattered.** The registry count was published only to the console, which
is the one channel `tick_gates.json` exists to replace. `tick_gates.ps1`'s own header and the
tick protocol both insist a tick branches on the JSON and never on prose, because a pipe or a
trailing `echo` corrupts a console value and the harness notification repeats the corruption.
My own session is the evidence: I read three of these JSON files and never saw a registry
count, and when I needed the unit pin I grepped console text out of `gates.log`.

**What changed**

- `Tools/tick_gates.ps1` — `Write-Result` emits `registry`, a per-game map, **unconditionally**
  (`{}` when nothing was measured, so a reader can distinguish "no games" from "an older script
  that never wrote this"). Additive: no existing field touched. `Get-AutomatedRegistryCount`
  gained an optional `-RepoRoot`, mirroring `Get-CreatedSourceFiles`, so it is fixture-testable.
- `Tools/Test_TickGates.ps1` — three new sections; **50/50** assertions.

**Verified empirically, not from the report**

- A real `-Phase measure` run now writes `registry: {}` into the JSON — the unconditional-empty
  case, confirmed end-to-end rather than by fixture.
- Against the **live checkout**, the helper returns Combat **17**, Zenithmon **73**, RenderTest
  **24**, `$null` for a game with no Tests directory.
- ★ **Zenithmon's 73 exactly matches the hand-maintained `registry **73**` in that game's
  `Status.md` LIVE PIN block**, an independently maintained number `doc_lint` C7 also
  reconciles. That cross-check says the count is *right*, not merely well-formed — a fixture
  test alone could not have told me that.

**Gates** — `-Phase verify`: **exit 4**, the `Tools/Vault` `npm test` line red **twice**, on
**different tests each time**: `cli-process.test.mjs` (6390 ms) then
`create, sprint, version, knowledge, rows, and suggestions mutations cover catalog families`
(28081 ms). 825/826 in both.

★ **The innocence argument here is airtight, more so than on any previous instance of this
flake: the entire diff is `Tools/tick_gates.ps1` + `Tools/Test_TickGates.ps1` — zero
JavaScript, nothing under `Tools/Vault/` at all.** It cannot affect that suite. Both named
tests are on `ZEN-D-024`'s list. This is the fourth consecutive ticket where `cli-process`
flaked, including one whose diff was a single markdown paragraph.

The red skipped all five `doc_lint` lines; run by hand: **exit 0 on every game**.

| gate line | result |
|---|---|
| `Test_TickGates.ps1` | **PASS — 50/50**, including the new sections |
| `Test_UnitBaselineManifest`, `Test_T0Harness_…`, `Test_DocLintKeyCitations` | PASS |
| `Test-ZagentClient`, `Test-BuildPrompt` | PASS |
| `Tools/Vault` `npm test` | 825/826 both attempts, different slow test each time — `ZEN-D-024` |
| `doc_lint.ps1` × 5 games | **PASS — by hand, exit 0 each** |

`zagent guard ZEN-78`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `tick_gates.ps1` (`registry`, `-RepoRoot`) | `Test_TickGates.ps1`, which **is** a gate line — so unlike most tests in this repo these run in CI-equivalent. Also exercised by every `tick_gates.ps1` invocation, including this ticket's own gate runs. |
| `Test_TickGates.ps1` | The gate line itself. |

★ **Mutation-checked, each redding precisely its target:**

| mutation | result |
|---|---|
| drop the paren anchor from the registry regex | **exactly 1** red — "counts a real call site but NOT a paren-less mention in a comment", the function's own stated failure mode |
| drop `registry` from `Write-Result` | **exactly 3** red — the empty-map, populated, and `$null`-entry assertions |

**Definition of Done**

- [x] Measure reports deferred lines and the registry count, both also in `tick_gates.json`,
      with assert-script coverage — the reporting was already done; the JSON field and the
      assertions are this commit. **One part is covered by proxy rather than directly — see
      the known gap below, which I am recording rather than claiming closed.**
- [x] The Suggestions row is resolved with this key — **already resolved, deliberately not
      re-resolved**, per `ZEN-D-022`.

**Durable findings**: none

The behaviour is documented in `tick_gates.ps1`'s own comments and pinned by its assert script;
the flake is `ZEN-D-024`, which this run corroborates rather than extends.

**Deviations / follow-ups**

- **Zero excursions.** Both changed files are on `filelist.txt`.
- **★ KNOWN GAP, deliberately not closed.** There is still no direct assertion that a deferred
  gate line reaches `$script:DeferredLines` at runtime. That population happens inside the
  phase body, behind `Test_TickGates.ps1`'s `-NoRun` dot-sourcing guard — a guard that exists
  precisely so that *"a test that claims a gate ran"* cannot live in this file. The worker
  flagged this itself rather than quietly letting the proxy stand for the real thing, which is
  the right instinct.
  **What IS covered**: the predicates that decide planned-vs-deferred membership.
  **What is NOT**: the three-line set subtraction (`all − planned`) that consumes them. An
  inverted condition there would survive the suite.
  **Why I accepted it**: closing it means extracting that subtraction into a pure helper —
  editing the script every gate run depends on — for a failure mode that is loud and immediate
  (every measure phase would report obvious nonsense). I judged the marginal value below the
  risk, and chose to record the gap precisely rather than override the boundary I had set.
- **Not checked by anyone:** whether `registry` populating end-to-end works on a category that
  *does* measure a game. Tooling has no unit-gate line, so no run in this tick populated it
  live; that path is covered by fixture assertions plus the direct live-checkout helper call
  above, not by an observed populated gate run. The next Engine-category tick will be the first
  real one — worth a glance at its `tick_gates.json`.
<!-- agent:worklog:end -->
