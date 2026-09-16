---
id: "0d84f36d-fcf7-461f-9ed5-aab6c84e3791"
key: "ZEN-23"
type: "TASK"
title: "Verify Vault production cutover"
category: "Engine"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: null
position: 23
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-16T01:30:44.431Z"
---
## Goal

Verify the production Vault cutover in the authoritative C:/dev/Zenith checkout using the real launcher and local web app.

## Definition of Done

- [ ] The production doctor reports a valid index and all 244 migrated tickets are accounted for before this verification ticket is created.
- [ ] The real launcher claims this ticket, preserves its attempt in scratch, and the local app observes the In Progress status.
- [ ] A local app editor save creates a Git commit, and the launcher reports the saved body and new history.
- [ ] Vitest, Playwright, frozen parity, and documentation lint checks pass.
- [ ] The operation recovery inspection reports no unresolved operations and this ticket finishes Done through the launcher.

## Gates

npm --prefix Tools/Vault test
npm --prefix Tools/Vault run test:e2e
npm --prefix Tools/Vault run parity
pwsh -NoProfile -File Tools/doc_lint.ps1
