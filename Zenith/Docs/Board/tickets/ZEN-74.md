---
id: "91616675-077d-4f6c-b3ce-5fedc3cd6398"
key: "ZEN-74"
type: "TASK"
title: "zagent claim archives an existing run directory to KEY.prev before writing fresh scratch"
category: "Engine"
parent: "ZEN-28"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 74
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:36:03.386Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
Suggestions row 2026-08-26 (cli): *"zagent claim should archive an existing .zagent/run/<KEY>/ to <KEY>.prev/ so a stale GREEN gate verdict cannot be sitting there when a tick starts."* `Tools/Vault/cli/tick-checks.mjs` §7 already treats a `tick_gates.json` whose `headSha` differs from HEAD as absent, which closes the worst case; the row asks for the run directory itself to be moved aside so no stale `body.md`, `changed.txt` or verdict can be read by mistake.

On `claim` (and `next`), if `.zagent/run/<KEY>/` exists, rename it to `.zagent/run/<KEY>.prev/` (replacing an older `.prev`) before writing the fresh scratch; the claim result reports `archivedPriorRun: true`. Vitest coverage in `Tools/Vault/cli` for the rename, the replace, and a claim with no prior run.

## Definition of Done
- [ ] A claim archives any existing run directory to `<KEY>.prev/` and reports it; tests cover the three cases
- [ ] `Tools/zagent/README.md` and `CLAUDE.md`'s scratch description mention `.prev`
- [ ] The Suggestions row is resolved with this key

