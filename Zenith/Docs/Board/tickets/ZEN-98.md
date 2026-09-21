---
id: "a938f3df-66f8-4ccc-9357-6bd0a8ae454e"
key: "ZEN-98"
type: "BUG"
title: "doc_lint: wrap the seven remaining Get-Content reads so a single-line doc cannot silently disable a check"
category: "Tooling"
parent: null
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: null
position: 98
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T16:48:09.716Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
claimAttempt: null
---
## Goal

`Tools/doc_lint.ps1` has eight `$lines = Get-Content <file>` sites. ZEN-8 wrapped exactly one of them — the C9 read site — in `@()`, because that ticket's boundary forbade modifying the existing checks. The other **seven remain unwrapped**: lines **152, 262, 282, 315, 349, 434 and 519**, covering every other line-oriented check in the file (C1 through C8).

`Get-Content` on a single-line file returns a `String`, not an array, so `for ($i = 0; $i -lt $lines.Length; $i++) { $line = $lines[$i] }` iterates **characters**. Measured on pwsh 7 during ZEN-8:

```
one.md  -> type=String    .Length=24  $lines[0]='S'
many.md -> type=Object[]  .Length=3   $lines[0]='line one'
```

The check then reports nothing and passes. This is the defect that shipped in C9 and was caught only because ZEN-8 required a test that fails without the check; see `ZEN-D-009`.

It is latent rather than harmless: it is hidden today only because every real living document is multi-line. A one-line `Status.md`, `Questions.md`, `TestPlan.md` or any newly created stub silently disables whichever check reads it, and the run goes green.

## Definition of Done

- [ ] All seven remaining `$lines = Get-Content` sites in `Tools/doc_lint.ps1` are `@()`-wrapped
- [ ] A test asserts that at least one previously-affected check still reports a violation when its input document is a single line, and fails if the wrapper is removed
- [ ] The test pairs any "nothing was reported" assertion with a positive control in the same input, so it cannot pass while detection is silent (`ZEN-D-009`)
- [ ] `doc_lint.ps1` still reports the same results it does today on all five games' real living docs — C9 clean everywhere, and Hearth's pre-existing `[C6] TDD.md:5` broken link to `GDD.md` unchanged

## Comments

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Done — already satisfied before this tick claimed it.** The deliverable landed
in `7afb828f` (2026-09-20). **No diff, no worker dispatched, no code changed.**

**The ticket's own numbers are the disproof.** It says `Tools/doc_lint.ps1` has
eight `$lines = Get-Content` sites with **seven unwrapped** at named lines.
Measured today: **ten sites, all `@()`-wrapped, zero unwrapped**, at entirely
different line numbers — two further reads were added since and both are wrapped.

Every DoD item verified, not just the headline:

- **Wrapped:** 10/10, confirmed by grep and by a structural assertion in the test
  suite that greps `doc_lint.ps1` itself, so unwrapping a check the test does not
  otherwise drive still reds.
- **A test that fails if the wrapper is removed:**
  `Tools/Test_DocLintKeyCitations.ps1:186` has a section headed *"every
  line-oriented check survives a SINGLE-LINE document (ZEN-98)"*, driving **C4**
  (deliberately not C9) with a one-line fixture and requiring exit 1 +
  `VIOLATION [C4]`. Unwrap that read and it iterates characters, reports nothing,
  exits 0 — the assertion fails. Non-vacuous.
- **Positive controls:** present in the same fixture throughout, citing
  `ZEN-D-009` by name.
- **Same results on all five games:** baselined first — Zenithmon,
  DevilsPlayground, Foundry, Undervault, Hearth all `ALL CHECKS PASS`, exit 0,
  **zero findings**.

`Tools/Test_DocLintKeyCitations.ps1` → **12/12 assertions passed**.

**★ DoD item 4's parenthetical is stale, and a literal reading would have been
harmful.** It asks for *"Hearth's pre-existing `[C6] TDD.md:5` broken link to
`GDD.md` unchanged"*. There is no such finding — that line now links to
`GameDesignDocument.md`, which exists, and Hearth is clean. Taken literally the
clause asks for a broken link to be preserved. Recorded as **`ZEN-D-033`**: an
equivalence baseline is a measurement you take, not a landmark you cite. The
corrected baseline is also stronger, since "all five clean" makes any new finding
anywhere a regression.

`zagent guard` exited 1 (`nothing has changed`); per `ZEN-D-028` that is the
zero-diff report, not a Blocked verdict, and it is not being read as a pass. The
20-line gate suite was not run — gates certify a diff and there is none; the
`Tooling` category's own checks for this path were run instead.

Same shape as ZEN-89: the audit commit that FILED this ticket also fixed it. Its
other tickets (ZEN-8, ZEN-40, ZEN-76, ZEN-79) are all already `Done`, so this is
two stragglers, not a backlog.

<!-- agent:worklog:start ZEN-98 -->
## Work Log — 2026-09-21

ran on sonnet/low via SIMPLE+LOW · category Tooling

**Outcome**: already done before this tick claimed it — the deliverable landed in
`7afb828f` (2026-09-20). **No diff, no worker dispatched, no code changed.**
· ran on sonnet/low via SIMPLE+LOW

**The ticket's central claim is false, and its own numbers are the disproof.**
It states `Tools/doc_lint.ps1` has *"eight `$lines = Get-Content` sites"* of which
*"the other **seven remain unwrapped**: lines 152, 262, 282, 315, 349, 434 and
519"*. Measured today:

```
$lines = @(Get-Content …)   ->  10 sites   (168, 278, 298, 331, 365, 450, 535, 716, 879, 1076)
$lines =   Get-Content …    ->   0 sites
```

Ten read sites, **all wrapped, none unwrapped**, and the line numbers have all
moved — two further reads were added after the ticket was written and both are
wrapped too.

**Verified against every DoD item, not just the headline:**

- **DoD 1 — all sites wrapped.** 10/10, confirmed by grep AND by a structural
  assertion inside the test suite: *"no `$lines = Get-Content` in doc_lint.ps1 is
  left unwrapped"*, which greps the file itself, so a future unwrap of a check
  this test does not otherwise drive still reds.
- **DoD 2 — a test that fails if the wrapper is removed.**
  `Tools/Test_DocLintKeyCitations.ps1:186` opens a section headed *"every
  line-oriented check survives a SINGLE-LINE document (ZEN-98)"*. Its
  load-bearing assertion drives **C4** (deliberately not C9, the check that
  originally shipped the bug) with a one-line `[SUPERSEDED]` fixture and requires
  `ExitCode -eq 1` and `VIOLATION [C4]`. Unwrap C4's read and the loop iterates
  characters, reports nothing, exits 0 — the assertion fails. Non-vacuous.
- **DoD 3 — "nothing was reported" paired with a positive control.** Satisfied
  throughout the file, in the same fixture each time: the link-text case asserts
  `-notmatch "cites 'ZM-999'"` **and** `-match "cites 'ZM-800'"`; the slash- and
  comma-separated cases assert exactly one violation naming `ZM-999` and not
  `ZM-50`. The comments cite the vacuity hazard and `ZEN-D-009` by name.
- **DoD 4 — same results on all five games' real living docs.** Baselined as the
  first action of this tick, before anything else: **Zenithmon,
  DevilsPlayground, Foundry, Undervault, Hearth — all `ALL CHECKS PASS`, exit 0,
  zero findings.** See the deviation below about this item's stale parenthetical.

`pwsh -NoProfile -File Tools/Test_DocLintKeyCitations.ps1` → **12/12 assertions
passed, PASS, exit 0.**

**Provenance.** `git log -S"SINGLE-LINE document (ZEN-98)"` names **`7afb828f`**,
"Fix three pre-existing repo defects found by the /tick audit" — the 2026-09-20
audit commit that also FILED this ticket. Identical shape to ZEN-89 and
`fa8ad5b2`. The audit's other tickets (ZEN-8, ZEN-40, ZEN-76, ZEN-79) are all
already `Done`, so this is two stragglers rather than a backlog worth a sweep.

**Gates** — the full 20-line suite was NOT run, for the same reason as ZEN-89:
gates certify a diff and there is none. What was run instead is the thing that
actually verifies this deliverable — `Tools/Test_DocLintKeyCitations.ps1`
(12/12) and `doc_lint.ps1` against all five games — which are the `Tooling`
category's own gate lines for this path. `zagent guard` exited **1**
(`nothing has changed — the worker wrote no files`); per `ZEN-D-028` that is the
zero-diff report and not a Blocked verdict, and it is not being treated as a pass
either.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| *(none — zero diff)* | n/a |

The deliverable is covered regardless: the structural assertion greps
`doc_lint.ps1` for any unwrapped read, and the C4 assertion drives the behaviour
end to end through a real one-line document.

**Definition of Done**

- [x] All seven remaining `$lines = Get-Content` sites are `@()`-wrapped — ten of
      ten are, zero unwrapped.
- [x] A test asserts that at least one previously-affected check still reports a
      violation when its input document is a single line, and fails if the
      wrapper is removed — the C4 single-line assertion, plus a structural one
      over the whole file.
- [x] The test pairs any "nothing was reported" assertion with a positive control
      in the same input — done throughout, citing `ZEN-D-009`.
- [x] `doc_lint.ps1` still reports the same results it does today on all five
      games' real living docs — measured: all five clean, zero findings. **The
      clause's parenthetical is stale; see below.**

**Durable findings**

- `ZEN-D-033` — an equivalence baseline in a DoD is a MEASUREMENT you take, not
  a landmark you cite. Pinning it to a named pre-existing defect rots when the
  defect is fixed and, read literally, asks the next worker to preserve a bug.

**Deviations / follow-ups**

- **★ DoD 4's parenthetical is stale and a literal reading would have been
  harmful.** It requires *"Hearth's pre-existing `[C6] TDD.md:5` broken link to
  `GDD.md` unchanged"*. There is no such finding: `Games/Hearth/Docs/TDD.md:5`
  now links to `GameDesignDocument.md`, which exists in that directory, and
  Hearth reports `ALL CHECKS PASS`. Taken literally the clause asks for a broken
  link to be preserved. Recorded as `ZEN-D-033`; the corrected baseline
  (all five games, zero findings) is also the STRONGER one, since it makes any
  new finding anywhere a regression rather than checking one named line.
- **No worker dispatched.** Step 2's reconciliation found the central claim false
  before any dispatch, so a dispatch would have handed a worker a false premise
  and an already-satisfied DoD.
- **Nothing was re-implemented to manufacture a diff** — that would be a rewrite
  of working code whose only purpose is to satisfy `zagent guard`.
- **No review dispatched**, per the zero-diff rule: there is no patch, so a
  reviewer would receive an empty diff and return nothing, which reads exactly
  like a pass. `review.required` was true for two reasons — an
  assertion-about-tests and an EQUIVALENCE — and both were verified directly
  instead: the test was run (12/12) and its assertions read for vacuity, and the
  equivalence was measured across all five games.
- **Sibling audit tickets checked rather than assumed.** ZEN-8, ZEN-40, ZEN-76
  and ZEN-79 are all `Done`, so no sweep is warranted.
<!-- agent:worklog:end -->
