---
id: "eb48167a-71b0-436b-b9ae-1bad57a0283e"
key: "ZEN-95"
type: "BUG"
title: "doc_lint C7 guards the pin narration but only runs when the diff already touches the narration"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 95
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T22:10:01.260Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
---
## Goal

**The check that guards the pinned-baseline narration only runs when the diff
already touches the narration.**

`doc_lint` check C7 reconciles `Games/Zenithmon/Docs/Status.md`'s LIVE PIN block
against `Tools/unit_baselines.json` with equality in both directions — a real,
working mechanism. But the `doc_lint` gate line belongs to the Zenithmon
category, and is only unioned into a run when the diff touches
`Games/Zenithmon/`.

MEASURED on ZEN-12 (2026-09-19): `zagent gates` returned 13 Engine lines with
`unioned: []`. The ticket moved all three pins. Only Combat's was checked. The
LIVE PIN block and the Zenithmon pin became checked solely because the
orchestrator chose to edit `Status.md` — and editing the file is what pulled in
the gate that verifies it.

So an engine tick that bumps every pin and touches no Zenithmon document leaves
the LIVE PIN block stale AND unverified, and C7 then reds on the **next**
Zenithmon ticket, which did not cause it. That is the "a stale pin reds a
required check with zero failing tests" shape the one-home baselines refactor
removed, re-created and displaced onto an innocent ticket.

Note the tick protocol currently describes `Status.md` as "read by no gate",
which is false, and is probably why this has gone unnoticed.

**This needs a human**: the fix is a change to gate selection in a protected
file, and there is a real choice to make — run `doc_lint` whenever a pin moves,
or make C7's input something a pin-bumping tick cannot miss.

## Definition of Done

- [ ] A tick that changes any pinned baseline has its pin narration checked,
      whether or not the diff touches a documentation directory
- [ ] A stale narration reds on the tick that caused it, not on the next
      unrelated one
- [ ] Whichever way it is fixed, the claim that the narration is "read by no
      gate" is corrected wherever it appears
