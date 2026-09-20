---
id: "a938f3df-66f8-4ccc-9357-6bd0a8ae454e"
key: "ZEN-98"
type: "BUG"
title: "doc_lint: wrap the seven remaining Get-Content reads so a single-line doc cannot silently disable a check"
category: "Tooling"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: null
position: 98
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-20T23:32:00.535Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
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
