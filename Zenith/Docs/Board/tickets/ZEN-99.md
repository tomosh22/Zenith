---
id: "cc4e12d2-1a17-4313-ad35-68c2a54cceaf"
key: "ZEN-99"
type: "BUG"
title: "zagent decide mints no ID without --project, silently storing an unciteable row"
category: "Tooling"
parent: null
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: null
position: 99
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-20T22:26:41.022Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
claimAttempt: null
---
## Goal
`zagent decide` mints a `PROJ-D-N` ID only when `--project` is passed explicitly. A bare `zagent decide --file x.md` returns exit 0, a rowId and a commit, and stores the row with `ID: null` — silently unciteable, which is the exact failure the minting was added to eliminate.

`appendKnowledgeRow` in `Tools/Vault/cli/write.mjs` derives the project as:

```js
const project = projectOf(index, findTicket(index, ticketKey), ticketKey)
  ?? String(valueOf(argv, 'project') ?? '').toUpperCase();
```

With no `--ticket`, `findTicket(index, undefined)` returns undefined and `projectOf` returns null, so `--project` is the only source. `mintDecisionId` then bails on `if (!project) return values;`.

Three other project derivations in the SAME file (lines 594, 844, 1006) use a richer chain that this one is missing:

```js
valueOf(argv, 'project') ?? policyOf(index).clientFile.project ?? index.boardRoots?.[0]?.project
```

`zagent.project.json` declares `project: "ZEN"` as the checkout default, so that fallback resolves correctly here and is what every other command already honours. The inconsistency is the whole defect.

Measured 2026-09-20: `ZEN-D-010` was filed with `--project ZEN` and minted; a later row filed without it landed `ID: null` and is still unciteable in `metadata.md`. Both rows carry `Ticket: null`, so the in-flight ticket is not the discriminator — the flag is.

This matters because `zagent finish` REFUSES a terminal status whose work log's "Durable findings" line names neither "none" nor a `PROJ-D-N` reference. A tick that records a decision without `--project` cannot then cite it, and cannot close.

## Definition of Done
- [ ] `zagent decide --file x.md` with no `--ticket` and no `--project` mints the next ID in the checkout's default project series
- [ ] An explicitly passed `--project` still wins, and an explicitly passed `ID` still wins over both
- [ ] The project derivation in `appendKnowledgeRow` matches the fallback chain the other three derivations in the same file already use
- [ ] A regression test drives the bare no-flag invocation and asserts a non-null ID, with a positive control that a passed ID is preserved unchanged
- [ ] The existing null-ID decision row recording the `Zenith/**` gate-category coupling is given its ID, or the convention for repairing an append-only row is recorded

## Comments

ran on sonnet/low via SIMPLE+MEDIUM · category Tooling

**Done.** Merged to `master` as `45320735`. 12/12 gate lines green, `skipped: []`,
Vault suite **745 passed (745)**.

**What landed.** `appendKnowledgeRow` in `Tools/Vault/cli/write.mjs` now derives two
projects instead of one: `project` keeps its original narrow expression and still feeds
`knowledgeRecord` and the catalog-less `CONFIGURATION` refusal, while a new
`mintProject` carries the widened checkout-default chain and feeds `mintDecisionId`
alone. +3 regression tests.

**Proof, live rather than only in a fixture:** the decision row for this ticket
(`ZEN-D-015`) was filed with **no `--project` flag** and minted correctly. Before this
change that exact invocation stored `ID: null`.

**★ The DoD's proposed mechanism was wrong and implementing it literally was a measured
regression.** Item 3 asks for the single `project` derivation to be made to match the
siblings. That was dispatch 1, and it broke
`Tools/Vault/test/write-branches.test.mjs` — `project` feeds three consumers, not one,
and widening it makes the catalog-less guard pass, so a bare `zagent ask` with no
ticket, no project and no catalog **silently creates a knowledge file and returns 0**
instead of refusing with exit 1. Caught by `zagent-reviewer`, confirmed by running the
file, fixed by splitting the variable. Item 3 is ticked in substance with the deviation
stated; the reasoning is `ZEN-D-015`.

**Two more ticket corrections.** There are **four** sibling derivations
(`write.mjs:594/844/1006/1103`), not the three the body claims — `version` at `:1103`
was missing. And DoD item 5 names *"the existing null-ID decision row"*, singular:
there are **two**, and one is a byte-identical duplicate of the already-minted
`ZEN-D-012`, so minting it would put two IDs on one decision. Item 5 was taken down its
"record the convention" branch, and the convention distinguishes the two cases.

**★ Four runs of the suite gave three different verdicts; both reds were flakes** —
`mutations.test.mjs:280` on the clean tree, a different `EPERM: rename` in
`project-actions-config.test.mjs` on the first verify, then 745/745 twice. My
pre-dispatch "baseline" asserted the first as a deterministic pre-existing failure on
one observation; that was wrong, and is filed as a Shortfalls row at `high` — a flaky
Tooling gate reds tickets that did not cause it, and `blocked-scope` is refused for
exactly that case because it keys on gate colour.

Full gate table, coverage row and the tick's own findings are in the work log.

<!-- agent:worklog:start ZEN-99 -->
## Work Log — 2026-09-20

ran on sonnet/low via SIMPLE+MEDIUM · category Tooling

**Outcome**: merged to `master` as `45320735` · **Done**. Two worker dispatches (one
fix-forward, 1 of 3 attempts used), one reviewer pass, four full runs of the Vault
suite. Final: **12/12 gate lines green, `skipped: []`, 745 passed (745)**.

**What changed**

- `Tools/Vault/cli/write.mjs` — `appendKnowledgeRow` now derives **two** projects.
  `project` keeps its original narrow expression, byte for byte, and still feeds
  `knowledgeRecord` (where a row goes) and the `!project` catalog-less refusal. A new
  `mintProject` carries the widened chain and feeds `mintDecisionId` alone. A ★-style
  block above the function explains the split and names the regression case.
- `Tools/Vault/test/decide-reference.test.mjs` — **+3 tests**: bare no-flag mints,
  explicit `--project` wins, explicit `ID` wins. Plus a header block recording the
  append-only-row repair convention.

**Files** (`git show --stat 45320735`)

```
 Tools/Vault/cli/write.mjs                  | 26 +++++++++++++-
 Tools/Vault/test/decide-reference.test.mjs | 58 ++++++++++++++++++++++++++++++
 2 files changed, 83 insertions(+), 1 deletion(-)
```

No excursions — both files are on `.zagent/run/ZEN-99/filelist.txt`.
`gatesSource: category`, nothing unioned in (the diff is entirely inside
`Tools/Vault/**`, which `Tooling` already owns).

**★ THE DoD'S PROPOSED MECHANISM WAS WRONG, AND IMPLEMENTING IT LITERALLY WAS A
MEASURED REGRESSION.**

Item 3 asks that *"the project derivation in `appendKnowledgeRow` matches the
fallback chain the other three derivations in the same file already use"*. Dispatch 1
did exactly that. `zagent-reviewer` returned it as a BLOCKING finding; I confirmed it
by running the file:

| | |
|---|---|
| clean tree, pre-change | `write-branches.test.mjs` — **17 passed** |
| dispatch 1 applied | `write-branches.test.mjs` — **1 failed \| 17 passed (18)** |

`project` feeds **three** consumers, not one: `knowledgeRecord` (where the row goes),
the `!project` guard at ~`:1380` (whether that is answerable at all), and
`mintDecisionId` (what the ID is called). The ticket is about the third only. Widening
the shared variable made the guard pass in a catalog-less vault, so a bare
`zagent ask` with no `--ticket` and no `--project` stopped refusing with
exit 1/`CONFIGURATION` and instead **silently created `FD/knowledge/questions.md` and
returned 0** — converting a deliberate, tested refusal into a guess.

The split is the corrected form. Item 3 is ticked **in substance** — `mintProject`
does match the sibling chain — and its literal wording is refuted by measurement.
Recorded as `ZEN-D-015` rather than left in this log, because the next person to read
`appendKnowledgeRow` needs it.

**Also corrected during the tick**

- The ticket says **three** sibling derivations; there are **four** — `write.mjs:594`
  (`queueCandidate`), `:844` (`create`), `:1006` (`sprint`) and **`:1103` (`version`)**.
  Found by the worker, confirmed by me, and dispatch 1's comment still shipped the
  ticket's wrong number; the fix-forward corrected it.
- **A vacuous test.** Dispatch 1's *"an explicit `--project` still wins over the
  checkout default"* passed `--project FD` in a fixture whose default is also `FD`, so
  both candidates produced byte-identical output — it would pass with the whole
  fallback chain deleted, and duplicated an existing test. Caught by the reviewer. Now
  passes `--project ZZ` and asserts `ZZ-D-001`, so it discriminates.
- **DoD item 5 was descoped before dispatch, on measured grounds.** It names *"the
  existing null-ID decision row"*, singular. `zagent rows decisions` shows **two**, and
  one is a **byte-identical duplicate of the already-minted `ZEN-D-012`** — minting it
  would put two decision IDs on one decision, which is worse than one unciteable row.
  `metadata.md` was kept off the file list (it is the Vault's live storage and the
  board commits to it). Item 5's second branch — record the convention — is taken, and
  the convention distinguishes the two cases.

**Gates**

| Command | Result |
|---|---|
| `Test_TickGates.ps1`, `Test_UnitBaselineManifest.ps1`, `Test_T0Harness_SessionCloseFlagsExist.ps1`, `Test_DocLintKeyCitations.ps1` | pass (4 lines) |
| `zagent/Test-ZagentClient.ps1`, `zagent/Test-BuildPrompt.ps1` | pass |
| `Set-Location Tools/Vault; npm test` | pass — **745 passed (745)** |
| `doc_lint.ps1 -Game {Zenithmon,DevilsPlayground,Foundry,Undervault,Hearth}` | pass (5 lines) |

`-Phase measure` exit 0 — **and it ran ZERO gate lines.** `Tooling` has no unit-gate
line, so the measure phase had nothing to measure and reported `steps: 0`,
`exitCode: 0`, `skipped: []`. The log says *"NO unit-gate line in this gate list — no
pin was measured"* and lists 12 deferrals; `tick_gates.json` — the channel this
protocol designates as the trustworthy one — says none of that. Reported below.

`-Phase verify` was run **twice**. First: exit 4, `npm test` red, **5 `doc_lint` lines
skipped**. Second, after the flake analysis below: exit 0, 12/12, `skipped: []`,
`headSha` == HEAD. `zagent guard ZEN-99` exit 0, no violations.

**★ FOUR RUNS OF THE SAME COMMAND PRODUCED THREE DIFFERENT VERDICTS. BOTH REDS WERE
FLAKES — INCLUDING THE ONE I TOLD THE WORKER WAS DETERMINISTIC.**

| run | tree | result |
|---|---|---|
| baseline, pre-dispatch | clean | `1 failed \| 741 passed (742)` — `mutations.test.mjs:280` |
| verify #1 | post-fix-forward | `1 failed \| 744 passed (745)` — a **different** test, `project-actions-config.test.mjs`, `EPERM: rename` on a temp `.vault-*.tmp`, from `atomic` at `server/operations.mjs:59` |
| re-run of the failing line | identical tree | **`745 passed (745)`** |
| verify #2 | identical tree | 12/12 green, `skipped: []` |

I inlined the baseline into the worker's prompt as *"the suite is ALREADY RED on a
clean tree … the target is 1 failed | 742 passed (743)"*. **That was wrong**, and it
was wrong in the way this protocol warns about most: a single observation asserted as
a property. One run is not a baseline for a flaky suite. It caused no harm only
because the worker was told not to act on it. Filed as a Shortfalls row (`high`).

The protocol's flake rule — re-run the failing gate line once; red→green is a flake,
costing no fix-forward attempt — is what rescued this tick, and it worked only because
the failing LINE could be re-run in isolation.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Tools/Vault/cli/write.mjs` (`mintProject`, `mintDecisionId` call) | The 3 new tests in `decide-reference.test.mjs` drive it directly — and, live in this checkout, `zagent decide` **with no `--project`** minted `ZEN-D-015` while writing this log. Before the fix that same invocation stored `ID: null`. |
| `Tools/Vault/cli/write.mjs` (restored narrow `project`) | `write-branches.test.mjs` "knowledge falls back to the metadata catalog and to a new knowledge file" — the test that caught the regression, green again. Not modified, deliberately. |
| `Tools/Vault/test/decide-reference.test.mjs` | Itself, via `npm test`. |

Unlike the two tickets in the 2026-09-20 audit, the gate list here **does** execute
this diff: the `Tooling` category runs the Vault suite, which is where the change
lives. That is the category doing its job.

**Definition of Done**

- [x] `zagent decide --file x.md` with no `--ticket` and no `--project` mints the next
      ID in the checkout's default project series — **verified twice**: by unit
      (`FD-D-001`) and live in this checkout (`ZEN-D-015`, minted with no flag).
- [x] An explicitly passed `--project` still wins, and an explicitly passed `ID` still
      wins over both — both tested, and the `--project` test now discriminates
      (`ZZ-D-001` against an `FD` default) after the reviewer found the original
      vacuous.
- [x] The project derivation in `appendKnowledgeRow` matches the fallback chain the
      other derivations use — **met in substance, and the literal wording is refuted
      by measurement.** `mintProject` carries exactly the sibling chain. `project`
      deliberately does NOT, because making it match is the regression documented
      above and in `ZEN-D-015`. Ticking this with the deviation stated rather than
      silently, per step 7.
- [x] A regression test drives the bare no-flag invocation and asserts a non-null ID,
      with a positive control that a passed ID is preserved unchanged — +3 tests.
- [x] The existing null-ID decision row is given its ID, **or** the convention for
      repairing an append-only row is recorded — second branch, deliberately; see the
      two-row finding above. Recorded in `write.mjs` and `decide-reference.test.mjs`.

**Durable findings**: ZEN-D-015 — in `appendKnowledgeRow`, the project used for
routing and for the catalog-less refusal must stay narrow; only the project used to
mint a `PROJ-D-N` ID may carry the checkout-default fallback. They are two variables
now and must not be recombined. The general form is recorded with it: a variable whose
name is a noun but whose consumers make decisions of different consequence — one
cosmetic, one structural — cannot be widened for the cosmetic consumer without
loosening the structural one.

**Deviations / follow-ups**

- **The `REACHABILITY` warning on this claim is a false positive, and I judged it so
  before dispatching.** It fired because the Goal names `zagent.project.json`. It names
  it only as background — the file *declares* the default the fix reads — and the DoD
  names no protected path; the whole deliverable is in `Tools/Vault/**`. Not labelled
  `needs-human`. This is the case the protocol says the warning is advisory for.
- **`-Phase measure` reports a green verdict having run nothing**, whenever the
  category has no unit-gate line. `steps: 0`, `exitCode: 0`, `skipped: []`, log line
  *"all gates green (exit 0)"*. The deferrals are in the log but not in the JSON, so
  the two channels disagree and the protocol says to trust the JSON. Open ticket
  ZEN-78 covers the deferral reporting; this is a sharper instance of it.
- **`zagent guard`'s success payload carries no `gateVerdictFresh` field** (and
  `gateSelectionStale: null`, not `false`). The protocol documents that field as how
  guard reports the freshness fold. On a pass there is no positive evidence the fold
  ran — only the absence of a refusal.
- **`zagent contract --json` is documented in both root `CLAUDE.md` and `tick.md` and
  is not implemented**: `{"error":"Unknown read command: contract"}`.
- **The prompt builder quotes the root C++ conventions into every ticket** — "No std::
  containers; every .cpp starts with `#include \"Zenith.h\"`; tabs; braces on new
  lines" — on a Node/Vitest ticket. The `Tooling` conventions follow and are correct;
  the C++ ones are noise competing with them under I3. I added a parenthetical to the
  dispatched prompt saying so.
- **Reviewer finding 3 became moot** rather than being applied: it flagged that
  `knowledge()` at `:1414` derives project narrowly while `appendKnowledgeRow` had been
  widened, opening a lost-update window in a hypothetical multi-board vault. The
  fix-forward restored the narrow derivation, so both agree again.
- Both worker reports quoted internally inconsistent test totals (12→15 in one,
  "same 10 test count" in the other). Neither was used; every number in this log is
  from an observed run.
<!-- agent:worklog:end -->
