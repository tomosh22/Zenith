---
id: "c562e936-a56b-41c9-a305-f87099982a08"
key: "ZEN-94"
type: "TASK"
title: "No gate list ever RUNS a RenderTest automated test, so its harness tests are compile-checked only"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 94
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T22:09:45.621Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
---
## Goal

**A `ZENITH_AUTOMATED_TEST_REGISTER` test under `Games/RenderTest/Tests/` is
compile-checked only. No gate list in this repo ever runs one.**

The Engine gate list has `build RenderTest --headless` but no `test RenderTest`
line, and no category declares `Games/RenderTest/` in its `paths`, so the
`zagent gates` union cannot add one either.

MEASURED on ZEN-12 (2026-09-19). That ticket's Definition of Done required a
headless proof that two views with different destinations produce identical pass
lists. ~115 lines implementing it landed in
`Games/RenderTest/Tests/Test_RenderGraphViewStructure.cpp` and **all 17 gate
lines went green without executing one of them.** The RenderTest boot count
moving 2800 to 2801 looks like confirmation and is not: that +1 is a
`ZENITH_TEST` boot unit in `Zenith/Flux/RenderViews/Flux_RenderViews.Tests.inl`,
while `RT_RenderGraphViewStructure` is a harness test the unit gate never
reaches. It was caught by a reviewer, not by any gate, and then run by hand:
2217 checks, 0 failed.

It is cheap to run. `m_bRequiresGraphics` is false and the driver is
backend-neutral, so the `Null_` exe the Engine gates already build will run it:

    rendertest.exe --automated-test RT_RenderGraphViewStructure --skip-unit-tests

This is the same blast-radius argument the Engine gate list already makes for
naming every game, reached from the other end: **building a game proves its
tests COMPILE, never that they RAN.**

Related and worth deciding at the same time: `Tools/unit_baselines.json` declares
the RenderTest row advisory, so nothing runs `run_unit_gate.ps1 -Game RenderTest`
either. RenderTest is compiled by every Engine tick and has the least mechanical
coverage of any game in the repo.

**This needs a human**: the fix is a gate-line change in a protected file, and it
is a CI-budget decision about how much RenderTest an Engine tick should run.

## Definition of Done

- [ ] An Engine change that breaks a RenderTest automated test reds a gate,
      rather than merging green
- [ ] Either RenderTest's `Null_` unit gate stops being advisory, or the reason
      it stays advisory is recorded where the row is declared
- [ ] The decision about how much RenderTest an Engine tick runs is stated, with
      its CI cost
