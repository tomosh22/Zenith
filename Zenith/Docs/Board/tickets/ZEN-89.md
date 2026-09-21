---
id: "0a3b56b3-fd27-4b5f-83e3-6c4d00d52db4"
key: "ZEN-89"
type: "TASK"
title: "build_prompt.ps1 does not emit the blast-radius survey clause the tick protocol mandates as the worker's first instruction"
category: "Engine"
parent: null
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 89
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T12:27:13.809Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
claimAttempt: "ffd8e256-0dea-47f2-b3f9-8738be519937"
---
## Goal

`.claude/commands/tick.md` step 5 mandates, with ★ emphasis, that a
blast-radius survey clause OPEN the worker's prompt as its FIRST instruction,
and says of ZM-69 that it "was the ONLY control that could have caught the
defect". `Tools/zagent/build_prompt.ps1` — the script the same step mandates for
assembling that prompt — does not emit it. `Select-String` for
`Step zero|blast|survey|falsify` over both `build_prompt.ps1` and
`Test-BuildPrompt.ps1` returns **zero matches**, and all 38 of its assertions
pass without noticing.

So a tick that runs the protocol's own mandated command line verbatim ships a
worker prompt missing the control the protocol rates highest, and nothing fails.

**This is not hypothetical and it cost a dispatch on 2026-09-19.** ZEN-12's
orchestrator pasted the clause by hand. The worker's survey then found
`Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp:577` branching on
`FLUX_RENDER_VIEW_PREVIEW` in live code — compiled in every `*_True` config, so
removing the enumerator reds `zenith build RenderTest`, an Engine gate line. The
worker stopped without editing and returned a corrected 10-file list. Without
the hand-pasted clause it would have edited 9 files and the build would have
redded.

The clause is currently carried by whoever remembers to paste it, which is the
exact shape the protocol elsewhere calls a defect: "a rule in prose in front of
a check IS the defect".

## Definition of Done

- [ ] `Tools/zagent/build_prompt.ps1` emits the blast-radius survey clause as the
      FIRST thing in every prompt it writes, ahead of the existing fixed clauses
- [ ] The clause instructs the worker to grep for every identifier it intends to
      change, confirm each hit is inside the file list, read the units that
      reference them, and STOP and report a corrected list rather than widen it
- [ ] It states that the file list bounds WRITES and not READS
- [ ] `Tools/zagent/Test-BuildPrompt.ps1` asserts the clause is present and
      precedes the "Files you may edit" line, so removing it fails a run
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run, if the
      change adds or removes a unit
