---
id: "91616675-077d-4f6c-b3ce-5fedc3cd6398"
key: "ZEN-74"
type: "TASK"
title: "zagent claim archives an existing run directory to KEY.prev before writing fresh scratch"
category: "Tooling"
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 74
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T05:25:09.795Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: null
---
## Goal
Suggestions row 2026-08-26 (cli): *"zagent claim should archive an existing .zagent/run/<KEY>/ to <KEY>.prev/ so a stale GREEN gate verdict cannot be sitting there when a tick starts."* `Tools/Vault/cli/tick-checks.mjs` §7 already treats a `tick_gates.json` whose `headSha` differs from HEAD as absent, which closes the worst case; the row asks for the run directory itself to be moved aside so no stale `body.md`, `changed.txt` or verdict can be read by mistake.

On `claim` (and `next`), if `.zagent/run/<KEY>/` exists, rename it to `.zagent/run/<KEY>.prev/` (replacing an older `.prev`) before writing the fresh scratch; the claim result reports `archivedPriorRun: true`. Vitest coverage in `Tools/Vault/cli` for the rename, the replace, and a claim with no prior run.

## Definition of Done
- [ ] A claim archives any existing run directory to `<KEY>.prev/` and reports it; tests cover the three cases
- [ ] `Tools/zagent/README.md` and `CLAUDE.md`'s scratch description mention `.prev`
- [ ] The Suggestions row is resolved with this key


## Comments

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Done.** Committed to `master` as `e11a2479`.

**★ The archive this ticket asks for already existed, and the Suggestions row it cites was
already resolved** — verified from the source and from `.zagent/run/ZEN-74.prev` existing on
disk, created by my own claim of this very ticket. Three of the four DoD clauses were real
work: the result never reported the archive, nothing tested any of it, and neither document
mentioned `.prev`.

`archivePreviousRun` now returns whether it archived, and `writeCompatibilityScratch` records
that as `archivedPriorRun` on the claim/next payload — with the shape check moved **before**
the archive call, so "archived but unreported" is unreachable rather than merely unlikely.

**Five tests**, through the real `runClient`: rename for `claim` **and** `next`,
replace-an-older-`.prev` asserting the stale contents are gone rather than merged,
no-prior-run, and a **refused** claim. That last one is the one to keep: the archive is gated
on `result.exitCode === 0`, and dropping that clause makes a refused claim archive away the
run directory of whatever ticket is in flight — the loss this mechanism prevents, inverted.
Every other test stays green under that mutation, and nothing else in the suite reaches
`writeCompatibilityScratch` on a non-zero exit at all.

Gates: **12/12 green, 0 skipped, 0 flakes**. The Vault suite goes **71 files / 809 tests →
72 / 814** — +5, zero regressions, measured against a baseline taken on a clean tree before
the worker ran rather than inferred (the last figure written down was 742, so inference would
have been wrong by 67). `zagent guard` clean, zero excursions.

**Each test was mutation-checked, because a passing test is not a covering test.** Deleting
the rename reds exactly the two cases that exercise it; dropping `|| next` reds exactly the
`next` case; dropping the `exitCode` guard reds exactly the refusal. Precise discrimination,
not blanket failure.

Review returned no blocking findings and six non-blocking; five were acted on after I verified
the two load-bearing ones against the code. The most consequential was that **my own
documentation was false through a second binary** — `cli/zagent.mjs` writes the same run
scratch on claim/next and does not archive — so both sentences are now qualified by entry
point. Recorded as `ZEN-D-023`; the divergence itself is the real defect and documenting
around it is a stopgap.

I also broke the tests once while applying a review fix — a `test.each` whose context argument
was `undefined`, so both rename tests threw before asserting anything — and caught it before
it shipped.

One flake cost a verify cycle: `project-action-safety.test.mjs` at 12.2 s, green on rerun, not
mine. It matters only because the red **skipped all five `doc_lint` lines**, so the phase was
re-run rather than read as effectively-green. Filed as a Suggestions row.

Durable finding: `ZEN-D-023`.

<!-- agent:worklog:start ZEN-74 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Outcome**: committed to `master` as `e11a2479` · ran on sonnet/low via SIMPLE+LOW

**★ The mechanism this ticket asks for already existed, and its Suggestions row was already
resolved.** Established before dispatch. `archivePreviousRun` (`Tools/Vault/cli/client.mjs`)
has been renaming `.zagent/run/<KEY>/` to `<KEY>.prev/` before each fresh claim for some time;
row `mw0i6mvn…` (2026-08-26) carries `Status: done` with a Resolution describing exactly that
code. Verified not from the row but from the source, and from `.zagent/run/ZEN-74.prev`
existing on disk — created by my own claim of this ticket.

Three of the four DoD clauses were nonetheless real, and I measured each:

| clause | state at claim |
|---|---|
| the rename / replace | **already done** |
| `…and reports it` | **not done** — `archivedPriorRun` appeared nowhere in `Tools/` |
| tests cover the three cases | **not done — zero coverage** |
| README + CLAUDE.md mention `.prev` | **not done** |

One near-miss worth recording: a substring grep for `.prev` in the test tree appears to hit
`mutations.test.mjs:149`, but that line is `previousStatus`. Reporting it as coverage would
have been wrong.

**What changed**

- `Tools/Vault/cli/client.mjs` — `archivePreviousRun` returns a boolean;
  `writeCompatibilityScratch` records it as `archivedPriorRun` on the claim/next payload, in
  place, which reaches both `last.json` and `--json` stdout because `finish()` reads
  `result.payload` afterwards.
- `Tools/Vault/test/run-archive.test.mjs` (new) — five tests through the real `runClient`.
- `Tools/zagent/README.md`, `CLAUDE.md` — the scratch rows.

**Gates** — `-Phase verify`: `state: complete`, `exitCode: 0`, **12/12 green, 0 skipped,
0 deferred, 0 flakes**, `headSha` matched HEAD. No union: the diff is entirely inside
Tooling's own `paths`. `-Phase measure` returned **exit 8** — "measure ran no gate lines" —
which is correct and not a failure: `Tooling` has no unit-gate line, so the measure phase had
nothing to run and the script refuses to call zero steps green.

| Command | Result |
|---|---|
| `Test_TickGates.ps1`, `Test_UnitBaselineManifest.ps1`, `Test_T0Harness_…`, `Test_DocLintKeyCitations.ps1` | PASS |
| `Test-ZagentClient.ps1`, `Test-BuildPrompt.ps1` | PASS |
| `Tools/Vault` `npm test` | PASS — **72 files / 814 tests / 0 failed** |
| `doc_lint.ps1` × five games | PASS |
| **hand run** `npx vitest run test/run-archive.test.mjs` | **5/5** |

**The suite numbers are a comparison, not a guess.** I baselined before the worker touched
anything: **71 files / 809 tests / 0 failed** on a clean tree. After: **72 / 814**. That is
+1 file and +5 tests, exactly the new file, **zero regressions**. Worth having taken —
ZEN-101's work log records this suite at 742, so inferring a delta from the last written
figure would have been wrong by 67.

`zagent guard ZEN-74`: `ok: true`, no violations, `gateSelectionStale: false`,
`gateVerdictFresh: true`.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `client.mjs` (`archivePreviousRun`, the flag) | All five new tests, via `runClient` — and the `npm test` gate line runs them. This is the first ticket in this run whose new tests are genuinely gated. |
| `run-archive.test.mjs` | The `npm test` gate line. |
| `README.md`, `CLAUDE.md` | Nothing. Prose. |

★ **A passing test is not a covering test, so each was mutation-checked** — three mutations,
each redding exactly the case that targets it and nothing else:

| mutation | result |
|---|---|
| delete `await fs.rename(current, previous)` | 2 failed (rename ×1 at the time, replace) — no-prior-run correctly stayed green |
| drop the `\|\| command === 'next'` arm | exactly 1 failed (the `next` rename) |
| drop `&& result.exitCode === 0` | exactly 1 failed (the refusal) |

**Definition of Done**

- [x] A claim archives any existing run directory to `<KEY>.prev/` and reports it; tests cover
      the three cases — reported via `archivedPriorRun`, and **five** tests cover the three
      named cases plus `next` and a refusal (see Deviations for why the extra two).
- [x] `Tools/zagent/README.md` and `CLAUDE.md`'s scratch description mention `.prev` — both,
      and both **qualified by entry point** rather than stated unconditionally, because the
      unconditional form is false (`ZEN-D-023`).
- [x] The Suggestions row is resolved with this key — **it already was, under different work,
      and I deliberately did not re-resolve it**, for the reason recorded as `ZEN-D-022` on
      the previous ticket: its Resolution is an accurate record of the implementing work, and
      overwriting it with a narrative about the reporting/tests half would destroy a better
      record than the replacement.

**Durable findings**: `ZEN-D-023`

- `ZEN-D-023` — two CLI entry points write `.zagent/run/<KEY>/` scratch on claim/next and only
  `client.mjs` archives; `cli/zagent.mjs`'s `persistMutationScratch` does not. Any
  "a claim archives the prior run" statement must name which binary. The divergence itself is
  the real defect; documenting around it is a stopgap.

**Review: no blocking findings, six non-blocking, five acted on.** I verified the two
load-bearing ones against the code before acting rather than taking them on trust.

1. **N2 (acted) — the destructive guard was untested.** Removing `result.exitCode === 0` makes
   a *refused* claim archive away the in-flight run directory. All three original tests stayed
   green under that mutation, and nothing else in the suite reaches
   `writeCompatibilityScratch` on a non-zero exit — `claim-refusal-atomicity.test.mjs` imports
   `dispatchWrite`, never `runClient`. Confirmed, and added the fourth test. The reviewer
   called this the highest-value addition and was right.
2. **N6 (acted) — my documentation was false through a second binary.** Confirmed
   `persistMutationScratch` has zero archive references and is a live documented entry point.
   Both sentences qualified. `ZEN-D-023`.
3. **N1 (acted)** — `next` is in the code guard and both documents and was covered nowhere.
4. **N5 (acted)** — the payload shape-check is now *before* the archive call, so if it ever
   did fail the rename would not already have happened.
5. **N4 (acted)** — the comment's "absent means an older client" inference has a third state:
   a throw between the rm and the rename leaves the old `.prev` destroyed, the stale run
   directory in place, exit 0, and the flag absent. Named in the comment, with `scratchError`
   as the tell.
6. **N3 (not acted)** — the archive-before-`applyScratchWrites` ordering is uncovered because
   the test dispatcher returns `writes: []`. Real, but covering it means a fixture dispatcher
   that emits scratch writes, which is a larger change than the finding warrants; recorded
   here rather than done.

**Deviations / follow-ups**

- **Zero excursions.** All four changed files are on `filelist.txt`.
- **I introduced a defect fixing the review and caught it before it shipped.** My `test.each`
  parameterisation for N1 passed the case value as the only positional argument, so the
  context `t` was `undefined` and both rename tests threw on `t.onTestFinished` before a
  single assertion ran — a test file that *looks* parameterised and asserts nothing. Replaced
  with a shared body called from two plain tests, with a comment recording the signature trap.
- **A flake cost a full verify cycle, and the skip list is the reason it mattered.** The first
  post-worker verify failed on
  `Tools/Vault/test/project-action-safety.test.mjs > a partial property update preserves the
  description and unrelated properties` at **12156 ms**, then passed on `tick_gates.ps1`'s own
  rerun (`flakes: [{firstExit: 1, rerunExit: 0}]`). Not my test, and green in every later run.
  But the red **skipped all five `doc_lint` lines**, so that phase verified far less than
  "one flaky test" suggests — which is why the phase was re-run rather than read as
  effectively-green. Filed as a Suggestions row.
- **`-Phase measure` exit 8 is not a failure** and is worth knowing for any `Tooling` ticket:
  the category has no unit-gate line, so measure has nothing to run, and the script refuses to
  report zero steps as green.
- **Not checked by anyone** (the reviewer's own list, carried forward): concurrency — two
  `zagent claim` processes racing on one key through the non-atomic rm/rename; `.prev`
  lifecycle beyond one generation (nothing deletes or reads one); the e2e/Playwright specs;
  and whether `Tools/zagent/Test-ZagentClient.ps1` asserts anything about the claim payload
  shape.
<!-- agent:worklog:end -->
