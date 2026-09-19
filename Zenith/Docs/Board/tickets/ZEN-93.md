---
id: "aee34c73-d26e-422c-9a3a-7f026f8d2719"
key: "ZEN-93"
type: "BUG"
title: "An unregistered --automated-test name exits 0 and reports nothing, so a stale capture reads as fresh evidence"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 93
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T22:09:32.558Z"
flagged: false
severity: "S2_MAJOR"
reproducibility: null
dueDate: null
environment: null
---
## Goal

`--automated-test <name>` with a name that matches no registered test **exits 0,
writes nothing, and reports nothing at all.**

MEASURED on 2026-09-19: `devilsplayground.exe --automated-test
MaterialEditorLivePreview --skip-unit-tests` (the real name is
`Test_MaterialEditorLivePreview`) ran to completion, exited 0, and the output
contains zero occurrences of "unknown", "not found", or the name passed.

The failure mode this creates is worse than a silent no-op. The harness in
question writes its captures to fixed paths (`C:/tmp/mat_shot_*.tga`), so the
run left **three-week-old images from a previous session** sitting at exactly the
paths a caller would collect. A tick taking BEFORE/AFTER evidence and trusting
the exit code would have archived those as its baseline and diffed them against
a fresh AFTER, producing a large bogus difference on a path nothing had changed.
It was caught only by checking file mtimes.

This is the same family as the repo's existing
`--exit-after-frames is INERT without --automated-test` gotcha. Note that the
tick protocol's standing advice — "read the authoring log line, not the exit
code" — does not rescue this case, because there is no log line to read.

## Definition of Done

- [ ] A run naming an unregistered automated test FAILS, with a non-zero exit
      code distinguishable from a test that ran and failed
- [ ] It logs the name it was given and, where the registry makes it cheap, the
      closest registered names
- [ ] The same treatment covers any sibling selector that silently accepts an
      unmatched name (`--filter` and friends), or each survivor is recorded with
      a reason it does not need it
- [ ] A unit covers the unmatched-name path so the fail-open cannot return
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run
