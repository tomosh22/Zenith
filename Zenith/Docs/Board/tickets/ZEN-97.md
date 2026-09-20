---
id: "c3e4e521-6a25-4104-a74b-e1f15a126f0b"
key: "ZEN-97"
type: "BUG"
title: "The tick protocol's step-0 pre-flight tells a healthy Vault checkout to stop"
category: "Tooling"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: 3
position: 97
labels: ["needs-human"]
fixVersions: []
blocks: []
updatedAt: "2026-09-20T23:31:54.487Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
---
## Goal

**The tick protocol's step-0 pre-flight tells a healthy checkout to stop.**

Step 0 carries a starred check, with an explicit instruction that failing it
means "you are running the wrong `zagent` and must stop": it greps `zagent
doctor` output for three prose rows — a client repo line, a repo branch line and
a repo clean line — and requires all three.

MEASURED on 2026-09-19: `zagent doctor` on the live Vault backend emits JSON and
nothing else — `{"ok":true,"diagnostics":[],"projects":[...],"ticketCount":535}`
— so the count is **0**, not 3. The prose rows do not exist in any form, and
`doctor` has no non-JSON output mode.

Step 0 does open with a Vault paragraph redirecting to a different preflight, so
the correct behaviour is recoverable. But it is three lines of plain prose ahead
of roughly forty lines of heavily emphasised legacy instruction ending in a hard
stop, and the legacy half reads as the authoritative one. A reader following the
emphasis stops on a healthy system; a reader who skips it has skipped the
section that catches a genuinely wrong client.

Two more, measured the same day, worth fixing in the same pass:

- Step 5's mandated prompt-assembly command line is spelled with Windows
  backslashes. In a POSIX shell those are escapes, so it resolves to a single
  run-together filename and fails. The protocol elsewhere insists a check be
  re-run "in the shell you will actually use" for exactly this class of trap,
  but does not apply it to its own mandated command.
- The root `CLAUDE.md` command table documents `zagent contract --json`, and
  calls `Tools/zagent/contract.json` its golden copy "asserted against by both
  sides' tests". Neither exists: the command returns `Unknown read command:
  contract` with exit 1, and the file is absent from the directory.

**This needs a human**: the protocol file is protected, and the loop may not
commit a change to its own gates.

## Definition of Done

- [ ] The step-0 pre-flight, followed literally, passes on a healthy checkout
      running the live backend
- [ ] The check that catches a genuinely wrong client still exists and still
      stops the tick, expressed against output the live backend actually produces
- [ ] The mandated prompt-assembly command line works in both shells this repo
      is driven from, or states which one it requires
- [ ] The documented command surface matches what the client implements — either
      the missing command exists, or the documentation stops promising it
