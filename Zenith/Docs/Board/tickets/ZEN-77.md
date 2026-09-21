---
id: "898f20e5-7a58-482c-a6c5-07d2d9aab4fd"
key: "ZEN-77"
type: "TASK"
title: "Warn at file time when a Definition of Done's identifiers have hits outside the declared file list"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 77
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T08:32:28.527Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
Suggestions row 2026-08-25 (contract): *"At file time, grep the repo for the identifiers a Definition of Done proposes to change and warn when hits land outside the ticket's declared file list."* ZM-69's first attempt blocked because its file list was two files short, and the identifiers its DoD named (`ZM_SCENE_GYM1`, `ZM_IsSceneRegisteredForLoad`) would have found them.

In `create` and `update --dod`: extract backtick-quoted identifiers from the DoD, grep the repo (source and tests, not docs) for word-bounded hits, and when the ticket declares a file list (a `## Files` section or the claim payload's list) warn with the hits outside it; without a declared list, print the hit files as information. Never refuse; this is a warning. Vitest with a fixture repo.

## Definition of Done
- [ ] `create` and `update --dod` report identifier hits outside a declared file list, and list hit files when no list is declared; tests cover both
- [ ] The Suggestions row is resolved with this key


## Comments

ran on sonnet/medium via MODERATE+MEDIUM · category Tooling (re-categorised from Engine)

**Done.** Committed to `master` as `90f7a760`.

**The scan already existed** — `dodBlastRadiusWarning` was implemented, wired into `create`,
and tested, and the cited Suggestions row was already `Status: done`. Its *"list hit files when
no list is declared"* clause was also already satisfied by construction (no citations → empty
declared set → every hit counts as outside). The gap was that the function had **exactly one
call site**: `update --dod` ran no scan at all, which is the ticket's headline case, since a DoD
edited after filing is precisely when a file list goes stale.

**★ The finding: the ticket was its own blast radius.** The worker wired `update` correctly and
then flagged a consequence rather than working around it silently — though the workaround it did
use was the tell, its "stays quiet" test having to cite the ticket's own path to pass. Measured
by removing that citation, the real warning read:

> `"…1 file(s) using them outside this ticket's cited file list: ZEN/tickets/ZEN-7.md."`

`create` scans before its file exists; `update` can only scan after `runOperation`, by which
point `applyJournal` has written the ticket into the **worktree** carrying the identifier the new
DoD just introduced — and `git grep` searches tracked working-tree files. That fires on
essentially every `update --dod`, and a warning that always contains a false positive teaches
the reader to skip the line. Fixed with a `selfPath` exclusion; recorded as `ZEN-D-025`.

Four tests, **each mutation-checked**: dropping `selfPath` reds exactly two, dropping the
`dodMayHaveChanged` gate reds exactly one. 55/55 across this file and `write-branches.test.mjs`,
which the reviewer flagged as now shelling out to `git grep` on every run.

**Review: no blocking findings, six non-blocking, five acted on — four of them mine.** It
cleared the risk I flagged as most dangerous (that `selfPath` was a no-op whose own test still
passed): the fixture's board root sits *under* the vault root, so a board-relative path would
have matched nothing and the mutation would have redded nothing. It redded exactly two. What it
caught in my work: a header comment contradicting the test beneath it and teaching the wrong
practice; **an invented mechanism** — I wrote that `git grep` "reads the committed tree", which
is false and contradicted a correct comment in the same change; and a mutation annotation my own
measurement disproved. All corrected.

**Gate note.** `-Phase verify` ran twice and never returned 0; both times the Vault suite went
red and **green on its own rerun**, the second a clean 826/826 (= the 822 baseline + exactly the
4 new tests). Two more distinct slow tests join `ZEN-D-024`, taking it from four to **six** —
one of them at 36 seconds. The reds skipped all five `doc_lint` lines each time; run by hand
after each, exit 0 on every game. `zagent guard` clean, zero excursions.

Also re-categorised Engine → Tooling before dispatch: 15 gate lines including eleven game builds
for a diff with zero C++, against a resolved row that had moved seven sibling tickets on exactly
that reasoning and missed this one. Now 12 lines that execute the diff.

<!-- agent:worklog:start ZEN-77 -->
## Work Log — 2026-09-21

ran on sonnet/medium via MODERATE+MEDIUM · category Tooling (re-categorised from Engine — see below)

**Outcome**: committed to `master` as `90f7a760` · ran on sonnet/medium via MODERATE+MEDIUM

**★ The scan already existed; the gap was that it had exactly one call site.** Fifth
consecutive ticket whose cited Suggestions row (`wmbefyl4…`) was already `Status: done`.
Verified against the source, not the row: `dodBlastRadiusWarning` (`write.mjs:885`) was
implemented, wired into `create`, and covered by its own test block. Its
*"list hit files when no list is declared"* clause was **also already satisfied by
construction** — with no citations the `declared` set is empty, so every hit counts as outside.
Worth checking rather than assuming; it would have been easy to add a redundant second path.

`update --dod` ran no scan at all — the ticket's headline case, since a DoD edited after filing
is exactly when a file list goes stale.

**What changed**

- `Tools/Vault/cli/write.mjs` — `update` now scans, gated on `--dod`/`--body`; the composed
  body is hoisted out of `runOperation`'s `prepare` callback; `dodBlastRadiusWarning` gained a
  `selfPath` exclusion.
- `Tools/Vault/test/claim-rulings-and-create-scan.test.mjs` — a new `E2` block, four tests.

**★ THE FINDING: THE TICKET WAS ITS OWN BLAST RADIUS.** The worker wired the scan correctly
and then **flagged a consequence rather than quietly working around it** — while declining to
fix it, because my boundary said not to touch the scan. That was the right call on
instructions, and the flag was worth more than the fix: the workaround it *did* use was the
tell. Its "stays quiet when cited" test had to cite the **ticket's own path** to pass.

I measured it rather than reasoning about it. Removing that citation produced:

> `"…the repo already has 1 file(s) using them outside this ticket's cited file list:
> ZEN/tickets/ZEN-7.md."`

`create` scans before its ticket file exists; `update` can only scan after `runOperation`, by
which point `applyJournal` has written the ticket into the **worktree** carrying the identifier
the new DoD just introduced — and `git grep` with no tree-ish searches tracked working-tree
files. Left alone this fires on essentially every `update --dod`, and a warning that always
contains a false positive teaches the reader to skip the line, taking the genuine hits with it.
So `update` now passes the subject ticket's `relativePath` as `selfPath`. Recorded as
`ZEN-D-025`.

**Gates** — `-Phase verify` was run twice and **never returned 0**; both times the
`Tools/Vault` `npm test` line went red and **green on its own rerun**
(`flakes: [{firstExit: 1, rerunExit: 0}]`), and the second run's rerun was a clean **826/826**.

| run | failing test(s) | duration |
|---|---|---|
| 1 | `cli-process.test.mjs > …keeps help read-only` · `human-gate tickets stop agents at In Review` | 5080 / 10042 ms |
| 2 | `cli-process.test.mjs` again · `workflow actions edit the per-project workflow config` | 6774 / **36170** ms |

826 = the 822 baseline + exactly the 4 new tests, in both runs. Two more distinct slow tests
join `ZEN-D-024`'s list, taking it from four to **six**; `cli-process` has now appeared in four
consecutive tickets, including one whose diff was a single markdown paragraph. Red→green on the
rerun is the protocol's flake case: record and continue.

A red **skips every line behind it**, so the five `doc_lint` invocations were skipped both
times. Run by hand after each: **exit 0 on all five games**.

| gate line | result |
|---|---|
| 6 assert scripts (`Test_TickGates`, `Test_UnitBaselineManifest`, `Test_T0Harness_…`, `Test_DocLintKeyCitations`, `Test-ZagentClient`, `Test-BuildPrompt`) | PASS both runs |
| `Tools/Vault` `npm test` | **PASS on rerun** — clean 826/826 the second time |
| `doc_lint.ps1` × 5 games | **PASS — by hand, exit 0 each** |
| **hand run** `vitest run claim-rulings-and-create-scan + write-branches` | **55/55** |

`zagent guard ZEN-77`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `write.mjs` (`update` scan, `selfPath`) | The four new tests, run by the `npm test` gate line. Also exercised incidentally by `write-branches.test.mjs:234`, which does an `update --dod` and now shells out to `git grep` on every run — the reviewer flagged it; it passes (55/55 above). |
| `claim-rulings-and-create-scan.test.mjs` | The `npm test` gate line. |

★ **Mutation-checked, each redding precisely its target:**

| mutation | result |
|---|---|
| drop `if (selfPath) declared.add(…)` | **exactly 2** red — "stays quiet when the hit file is cited" and "never reports the ticket's OWN file" |
| drop the `dodMayHaveChanged` gate | **exactly 1** red — the goal-only test |

**Definition of Done**

- [x] `create` and `update --dod` report identifier hits outside a declared file list, and list
      hit files when no list is declared; tests cover both — `create` and the no-list clause
      were already true; `update --dod` is this commit, with four tests, each mutation-checked.
- [x] The Suggestions row is resolved with this key — **already resolved, deliberately not
      re-resolved**, per `ZEN-D-022`.

**Durable findings**: `ZEN-D-025`

- `ZEN-D-025` — a repo-wide scan run *after* a board mutation must exclude the mutated record's
  own file, or it reports that record as its own finding on essentially every run. Includes the
  correction that the cause is the worktree write, not the commit.

**Review: no blocking findings, six non-blocking, five acted on. Four were mine.**

The reviewer cleared the thing I had flagged as most dangerous — that `selfPath` might be a
no-op whose own test still passes if the fixture made the two path forms coincide by accident.
It does not: the fixture's board root sits *under* the vault root, so a board-relative path
would have failed to match and the mutation would have redded nothing. It redded exactly two.

1. **NB1 (acted) — a stale header comment contradicting the test beneath it.** It still said the
   "stays quiet" test cites the ticket's own path; I had removed that citation when I rewrote
   it. Worse, it taught the wrong practice — a reader would conclude a real ticket should cite
   itself to suppress the warning. Rewritten.
2. **NB2 (acted) — my comment asserted an invented mechanism.** I wrote that `git grep` "reads
   the committed tree". It does not; with no tree-ish it searches the tracked working tree,
   which the test file's own comment already said correctly — so my change contained two
   comments asserting opposite semantics for the same command. Right conclusion, wrong reason.
   Corrected in both places, and the correction matters: the self-match would happen under
   `writes.commit: false` too, which my version would not have predicted.
3. **NB5 (acted) — a mutation annotation my own measurement contradicted.** It claimed deleting
   `dodMayHaveChanged` reds test 1; my run showed it reds exactly the goal-only test. In a repo
   where those annotations are load-bearing documentation, a false one is worse than none.
4. **NB3 (acted)** — I had stated a design *preference* ("the composed body only exists inside
   the callback") as an impossibility. `record.bodyText` is available outside, so `create`'s
   ordering was reachable and would have made `selfPath` unnecessary. Comment now says it is a
   choice, and why scanning what landed is worth the exclusion.
5. **NB4 (acted, as a comment)** — on an idempotent replay `executeOperation` returns without
   calling `prepare`, so `composedBody` stays undefined and the warning silently vanishes, where
   `create`'s still emits. Advisory only; recorded at the hoist rather than restructured.
6. **NB6 (partly acted)** — only the subject ticket is excluded, so a hit in *another* board
   ticket is still reported as if a `.md` were code. Noted in the comment; pre-existing and
   shared with `create`, so not otherwise addressed.
7. **Nit (not acted)** — `fixtureWithHit` is duplicated between the E and E2 blocks. Left; the
   duplication keeps each block independently readable and is the existing style there.

**Deviations / follow-ups**

- **Zero excursions.** Both changed files are on `filelist.txt`.
- **I re-categorised the ticket Engine → Tooling before dispatching.** Its diff is entirely
  under `Tools/Vault/**`; as `Engine` it resolved a 15-line gate list including eleven game
  builds for a change containing zero C++. Suggestions row `27c5b04a` (already `done`)
  established exactly that and moved seven sibling tickets — ZEN-74, 75, 76, 78, 83, 97, 98 —
  but missed this one. Now 12 lines, and they execute the diff.
- **Not checked by anyone** (the reviewer's own list, carried forward): Windows path-casing and
  `core.quotePath` for non-ASCII ticket paths; the `DOD_SCAN_FILE_CAP`/timeout behaviour of the
  added `git grep` against a full-sized tracked tree; and the `ctx.repo ≠ ctx.root`
  configuration, where the scan silently covers only the vault subtree.
<!-- agent:worklog:end -->
