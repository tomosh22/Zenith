---
id: "f715fc2c-f338-4564-9cd7-3795a8bcf868"
key: "ZEN-38"
type: "TASK"
title: "Async asset loading: implement the documented NOT IMPLEMENTED loader or delete its scaffolding"
category: "Engine"
parent: "ZEN-24"
sprint: null
status: "To Do"
assignee: null
priority: "LOW"
complexity: "MODERATE"
risk: "MEDIUM"
storyPoints: 5
position: 38
labels: ["tech-debt"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:33:49.954Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Zenith/AssetHandling/CLAUDE.md` carries an explicit "NOT IMPLEMENTED" notice for async asset loading (added by the junior exploration because the headers implied it was usable). Scaffolding that reads like a supported feature and does nothing is exactly what the no-legacy mandate forbids. Decide and act, in one ticket: either implement the async loader against the task system (a load request returns a handle that resolves on the main thread at a defined seam; `Zenith_AssetRegistry` keeps ownership semantics identical to the synchronous path, `Docs/design/Zenith_HashMap.md`-style growth safety for the pending table, headless units over request, completion, cancellation and a fail-closed missing asset) or delete the async declarations, their doc notice and any stub bodies so the API cannot be reached for.

The decision defaults to DELETE unless a game names a consumer: Hearth's TDD §3.2 and Undervault's TDD §2 do not list async asset loading, and Foundry's async need is SAVE (FD-77), not load. Record the decision as a comment on this ticket and in `Zenith/AssetHandling/CLAUDE.md`. If implemented, the `d45bd7b3` rule holds: asset handles must release while the registry is alive.

## Definition of Done
- [ ] Either the async loader works end to end with units over request, completion, cancellation and a missing asset, or every async declaration, stub and doc notice is deleted; the choice and its reason are recorded
- [ ] `Zenith/AssetHandling/CLAUDE.md` describes the shipped state with no "NOT IMPLEMENTED" notice
- [ ] Every game's pinned baseline bumped from an OBSERVED run
