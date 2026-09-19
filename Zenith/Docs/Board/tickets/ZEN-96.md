---
id: "32fc76b8-1e5a-40e1-8699-7f0cff495862"
key: "ZEN-96"
type: "TASK"
title: "Six of seven categories declare no decision log, so the tick protocol's ruling-inlining invariant has nothing to read"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 96
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T22:10:06.538Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
---
## Goal

**The tick protocol's ruling-inlining clause is inoperable on six of seven
categories, including the one this board defaults to.**

Invariant I3 requires every governing RULING to be pasted into the worker's
prompt, and argues at length that its absence cost a ticket (ZM-23 shipped a
change contradicting ZM-D-203 Ruling 3). It tells the reader where to look:
*"`zagent.project.json` already declares `decisionLog` and `decisionIdPrefix`,
so you are told where the rulings live — you were just never told to read them."*

MEASURED: only `Zenithmon` declares either. `Engine` — the ZEN board's category
and this checkout's default — declares neither, and nor do `DevilsPlayground`,
`ScriptTest`, `Undervault`, `Hearth` or `Foundry`.

So on a ZEN ticket there is no ruling store to grep, and I3's instruction has
nothing to act on.

This compounds with the `Durable findings` requirement at the other end of the
protocol, which refuses any terminal status whose work log does not name either
"none" or a `PROJ-D-N` reference. With no id prefix resolved for Engine, a tick
that genuinely learns something durable has no sanctioned id series to record it
under. ZEN-7 and ZEN-12 established `ZEN-D-001` through `ZEN-D-004` by hand on
2026-09-19 precisely because none existed.

**This needs a human**: the fix is a declaration in a protected file, and
choosing where engine-wide decisions live is a judgement about this repo's
documentation shape, not a mechanical edit.

## Definition of Done

- [ ] An Engine-category ticket resolves to a decision log and an id prefix, so
      I3's instruction has something to read and `zagent decide` has a home
- [ ] The id series already in use for engine decisions is either adopted or
      explicitly superseded, with the existing entries reachable either way
- [ ] The same question is answered for the other categories that declare
      neither, or each is recorded with the reason it does not need one
