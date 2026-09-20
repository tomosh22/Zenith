---
key: ZEN-8
type: TASK
title: "doc_lint: verify bare ticket-key citations, not just markdown links"
category: Engine
parent: "ZEN-28"
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: 3
position: 4
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: dppjj9yzstm4qgm4m01twl5c
  risk: LOW
  type: TASK
  title: "doc_lint: verify bare ticket-key citations, not just markdown links"
  number: 8
  flagged: false
  due_date: null
  position: 4
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-08-23T21:11:00.812629
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-23T21:11:00.812629
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-20T03:22:28.118Z"
claimAttempt: null
---
## Goal

`Tools/doc_lint.ps1`'s C6 check resolves markdown LINKS, so a citation written
as `[ZM-53](…url…)` is verified and a bare bracketed key in prose — `[ZM-50 /
ZEN-2]`, which is how the living docs actually write them — is not checked at
all. Both failure directions are live in the repo today:

* **Dangling**: a key that resolves to no ticket (the ZM-D-177 citation ZM-53
  was filed to fix).
* **Wrong**: a key that resolved to nothing when written and resolves to
  something unrelated now, because keys are allocated sequentially. Questions.md
  tagged a question `[ZM-50 / ZEN-2]` meaning "and an engine twin, unfiled";
  filing an unrelated engine ticket later allocated ZEN-2 to it. A reader who
  follows that lands somewhere plausible and wrong.

The client half of this already exists and is tested: `Find-KeyCitations` in
`Tools/zagent/ZagentClient.psm1` greps the living-doc dirs for a word-bounded
key and is wired into `zagent create`, which warns when the key it just
allocated was already cited. That closes the FORWARD direction at the moment it
arms. Nothing closes the dangling direction, and nothing checks the docs except
at create time.

## Definition of Done

- [ ] `doc_lint.ps1` reports bare bracketed ticket keys in the living docs that
      resolve to no existing ticket, distinct from its existing link check
- [ ] A key inside a fenced code block or an inline span is NOT reported — the
      docs quote example keys, and a check that fires on those gets muted
- [ ] The new check has a test that fails without it, in the harness
      `Tools/Test_*.ps1` already uses
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run

## Comments

ran on sonnet/low via SIMPLE+LOW · category Engine

**Done** — merged to `master` as `76aed619`. All four Definition of Done items met.

`doc_lint.ps1` gains check **C9**: a bare bracketed ticket key (`[ZM-50 / ZEN-2]`) that resolves to no ticket file is reported, with fenced blocks (``` and ~~~), inline code spans, decision keys (`ZM-D-203`) and markdown link text suppressed. **Stronger than the ticket assumed**: C6 `continue`s on `^https?://`, so it never verified ticket keys either — C9 is the first check to verify them at all.

**The registry is six directories, not one, and that correction was the worker's — against my prompt.** I told it the registry was `Zenith/Docs/Board/tickets/` (91 files); that is only ZEN. Zenithmon has 116, Foundry 136, Undervault 73, Hearth 61, DevilsPlayground 58 — **535 total**. Had it trusted me, ~408 real citations across five games would have false-flagged.

**Two real defects, both caught by the test the DoD demanded:**

1. `Get-Content` returns a **String** for a single-line file, so the loop iterated *characters* and the check silently reported nothing (`one.md -> type=String, .Length=24, $lines[0]='S'`). Fixed by `@()`-wrapping. Deterministic red→red across two runs; **7/10 before, 10/10 after**, the load-bearing dangling-key assertion among the failures.
2. **Six assertions were passing vacuously** — asserting only "no violation appears", equally true when the check is broken. Each now carries a dangling control key in the same document and asserts both directions, so it fails if suppression breaks *or* if detection goes silent. One fixture stays single-line as the regression pin.

**★ No gate line runs `doc_lint.ps1` or any `Tools/Test_*.ps1` for this diff** — zero, checked against the recorded step list. `Tools/**` is in no category's `paths`, so eleven C++ builds went green having executed nothing this ticket touched. All verification was by hand: the test 10/10, and C9 clean against all five games' real living docs. Hearth exits 1 on a **pre-existing** `[C6] TDD.md:5` broken link to `GDD.md`, byte-identical before and after.

**Three corrections to the ticket's own text**, made before dispatch: `Find-KeyCitations` in `ZagentClient.psm1` **does not exist** (another casualty of the PowerShell-to-Node-shim migration, like `Restore-RefusedClaim`); C6 does not verify key citations; and DoD item 4's pin clause cannot apply to PowerShell tooling.

**Durable finding: ZEN-D-009** — `Get-Content` on a single-line file returns a `String`; always `@(Get-Content …)`. The generalisation matters more than the idiom: *a test whose only assertion is that nothing was reported cannot distinguish "the rule holds" from "the checker never ran".* Two tickets this session needed that property independently.

**Filed ZEN-98**: seven further unwrapped `Get-Content` sites (lines 152, 262, 282, 315, 349, 434, 519) carry the same defect in every other line-oriented check, C1–C8. Out of this ticket's boundary.

<!-- agent:worklog:start ZEN-8 -->
## Work Log — 2026-09-20

ran on sonnet/low via SIMPLE+LOW · category Engine

**Outcome**: merged to `master` as `76aed619` · ran on sonnet/low via SIMPLE+LOW

**What changed**

- `Tools/doc_lint.ps1` — new check **C9** (`Check-KeyCitations`) plus helpers `Get-KnownTicketKeys` and `Hide-InlineCodeSpans`, the `$script:ticketKeyPattern` regex, a header-comment entry, and registration in the run-all block (now "9 checks").
- `Tools/Test_DocLintKeyCitations.ps1` — NEW, 10 assertions, black-box.

**Files** (`git diff --stat`)

```
 Tools/Test_DocLintKeyCitations.ps1 | 365 +++++++++++++++++++++++++++++++++++++
 Tools/doc_lint.ps1                 |  ... 
 2 files changed, 366 insertions(+), 1 deletion(-)
```

**Gates**

`-Phase measure` exit 0 (no pin movement) · `-Phase verify` **exit 0, all gates green**, 13 steps, `skipped: []`, `headSha` matching HEAD · `guard` exit 0, no violations.

**★ No gate line runs `doc_lint.ps1` or any `Tools/Test_*.ps1` for this diff — zero, checked programmatically against the recorded step list.** `Tools/**` is in no category's `paths`, so the Engine list ran unchanged: eleven C++ game builds, `test Combat`, Combat's unit gate. The gates went green having executed nothing this ticket touched. All real verification below was run by hand.

| Hand-run verification | Result |
|---|---|
| `Tools/Test_DocLintKeyCitations.ps1` | **10/10, exit 0** (was 7/10 before the fix — see below) |
| `doc_lint.ps1 -Game Zenithmon` | exit 0, `PASS [C9] … (535 known tickets)` |
| `doc_lint.ps1 -Game DevilsPlayground` | exit 0, C9 clean |
| `doc_lint.ps1 -Game Foundry` | exit 0, C9 clean |
| `doc_lint.ps1 -Game Undervault` | exit 0, C9 clean |
| `doc_lint.ps1 -Game Hearth` | exit 1 — **C9 clean**; one PRE-EXISTING `[C6] TDD.md:5: broken markdown link: 'GDD.md'` (the file is `GameDesignDocument.md`), byte-identical before and after this change |

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Tools/doc_lint.ps1` (C9 only) | `Tools/Test_DocLintKeyCitations.ps1`, 10 assertions — **by hand only, no gate runs it.** Additionally exercised against all five games' real living docs. |
| `Tools/Test_DocLintKeyCitations.ps1` | is the test; **no gate runs it.** |
| `Tools/doc_lint.ps1` (C1–C8) | unchanged by this diff. |

**Definition of Done**

- [x] `doc_lint.ps1` reports bare bracketed ticket keys in the living docs that resolve to no existing ticket, distinct from its existing link check — C9. **Stronger than the ticket assumed**: C6 skips `^https?://` and `^mailto:` targets outright, so it never verified ticket keys either; C9 is the first check to verify them at all, not a complement to an existing key check.
- [x] A key inside a fenced code block or an inline span is NOT reported — both fence characters (``` and ~~~), inline code spans, decision keys (`ZM-D-203`) and markdown link text, each with a positive control (below).
- [x] The new check has a test that fails without it, in the harness `Tools/Test_*.ps1` already uses — demonstrated the hard way: it **did** fail, 7/10, against the first implementation.
- [x] Every affected game's pinned baseline bumped from an OBSERVED run — **zero games affected, and that is observed, not assumed.** `-Phase measure` ran Combat's unit gate and returned exit 0 with the pin unchanged at its current value. PowerShell tooling cannot move a C++ boot unit count; `Tools/unit_baselines.json` was not edited and no number was quoted.

**The registry is six directories, not one — and that correction was the worker's, against my prompt.**

My dispatch stated the registry is `Zenith/Docs/Board/tickets/` (91 files). That is only ZEN. Every project has its own: Zenithmon 116, Foundry 136, Undervault 73, Hearth 61, DevilsPlayground 58 — 535 in total, which is what C9 now reports. The worker grepped the real tree, found this before finishing, and generalised `Get-KnownTicketKeys` to union all six. Had it trusted my prompt, ~408 real citations across five games would have false-flagged as dangling. Verified independently afterwards by listing all six directories.

**Two defects the test caught, both real.**

1. **`Get-Content` returns a `String` for a single-line file, not an array.** So `for ($i=0; $i -lt $lines.Length; $i++) { $line = $lines[$i] }` iterated *characters*, `$line` was `'S'`, no bracket ever matched, and the check silently reported nothing. Measured: `one.md -> type=String, .Length=24, $lines[0]='S'` versus `many.md -> type=Object[], .Length=3`. Fixed by `@()`-wrapping the C9 read site. Re-run twice before fixing to rule out a flake — deterministic red→red both times.
2. **Six of the ten assertions were passing VACUOUSLY.** They asserted only "no violation appears", which is equally true when the check is broken — the suppression cases all "passed" precisely because nothing was ever reported. Each now carries a dangling control key on a plain line in the same document and asserts both that the suppressed key is absent *and* that the control key is present, so it fails if suppression breaks or if detection goes silent. One fixture is deliberately kept single-line as the regression pin for defect 1.

**Durable findings**: ZEN-D-009

- **ZEN-D-009 — `Get-Content` on a single-line file returns a `String`; always `@(Get-Content …)`.** The failure is silent and data-dependent: a line-oriented linter written this way works on every real document and reports nothing on a one-line file. Carries the generalisation that matters more than the idiom: **a test whose only assertion is that nothing was reported cannot distinguish "the rule holds" from "the checker never ran"** — pair every such assertion with a positive control in the same input. Two tickets in this session needed that property independently (see also ZEN-D-007's accepted-claim control in ZEN-79's atomicity suite).

**Deviations / follow-ups**

- **Three corrections to the ticket's own text, made before dispatch and inlined as corrections.** (a) It states *"the client half of this already exists and is tested: `Find-KeyCitations` in `Tools/zagent/ZagentClient.psm1`"* — **it does not exist**; that module has five functions and none is it, and the only repo-wide hits are copies of this ticket's own text under `Tools/Vault/fixtures` and `Tools/Vault/artifacts`. Presumably lost when `zagent.ps1` became a Node shim, the same way `Restore-RefusedClaim` was. (b) It states C6 verifies `[ZM-53](…url…)` citations — C6 explicitly `continue`s on `^https?://`, so neither direction was covered. (c) DoD item 4's pin clause does not apply to PowerShell tooling.
- **Seven more unwrapped `Get-Content` sites remain**, at `Tools/doc_lint.ps1` lines 152, 262, 282, 315, 349, 434 and 519 — the same defect in every other line-oriented check, C1 through C8. Out of this ticket's boundary, which forbade modifying existing checks. **Filed as ZEN-98.**
- **`C9` was also earmarked by ZEN-73.** `Docs/Roadmap_Engine.md` lists ZEN-73 as *"doc_lint check C9: reconcile AssetManifest roster counts and generator version stamps"*. This ticket has taken C9; ZEN-73 will need the next free id. Flagged by the worker, not fixed here — the roadmap is board-generated.
- **`Docs/Roadmap_Engine.md` and `Zenith/Docs/**` get no doc_lint coverage at all** — the tool only ever lints `Games/<Game>/Docs`, so the Engine's own living docs are unlinted by C1–C9. Noted, not fixed.
- **I used `git stash` to prove Hearth's C6 violation was pre-existing, and should not have.** The protocol says never stash. It restored cleanly and the tree was verified afterwards, but the risk was unnecessary: C9 passing for Hearth already established the violation came from C1–C8, which this diff never touches. Recording it because an unrecorded rule-break is worse than the break.
- **I misread an exit code exactly the way the protocol warns.** `pwsh -File Test_… | tail -8; echo $?` reported 0 — that was `tail`'s code. The real code was 1 and the test was failing. Re-read via a file, as the protocol's own rule says.
<!-- agent:worklog:end -->
