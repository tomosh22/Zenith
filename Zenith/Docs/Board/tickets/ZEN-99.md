---
id: "cc4e12d2-1a17-4313-ad35-68c2a54cceaf"
key: "ZEN-99"
type: "BUG"
title: "zagent decide mints no ID without --project, silently storing an unciteable row"
category: "Tooling"
parent: null
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "MEDIUM"
storyPoints: null
position: 99
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-20T11:41:12.285Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: null
---
## Goal
`zagent decide` mints a `PROJ-D-N` ID only when `--project` is passed explicitly. A bare `zagent decide --file x.md` returns exit 0, a rowId and a commit, and stores the row with `ID: null` — silently unciteable, which is the exact failure the minting was added to eliminate.

`appendKnowledgeRow` in `Tools/Vault/cli/write.mjs` derives the project as:

```js
const project = projectOf(index, findTicket(index, ticketKey), ticketKey)
  ?? String(valueOf(argv, 'project') ?? '').toUpperCase();
```

With no `--ticket`, `findTicket(index, undefined)` returns undefined and `projectOf` returns null, so `--project` is the only source. `mintDecisionId` then bails on `if (!project) return values;`.

Three other project derivations in the SAME file (lines 594, 844, 1006) use a richer chain that this one is missing:

```js
valueOf(argv, 'project') ?? policyOf(index).clientFile.project ?? index.boardRoots?.[0]?.project
```

`zagent.project.json` declares `project: "ZEN"` as the checkout default, so that fallback resolves correctly here and is what every other command already honours. The inconsistency is the whole defect.

Measured 2026-09-20: `ZEN-D-010` was filed with `--project ZEN` and minted; a later row filed without it landed `ID: null` and is still unciteable in `metadata.md`. Both rows carry `Ticket: null`, so the in-flight ticket is not the discriminator — the flag is.

This matters because `zagent finish` REFUSES a terminal status whose work log's "Durable findings" line names neither "none" nor a `PROJ-D-N` reference. A tick that records a decision without `--project` cannot then cite it, and cannot close.

## Definition of Done
- [ ] `zagent decide --file x.md` with no `--ticket` and no `--project` mints the next ID in the checkout's default project series
- [ ] An explicitly passed `--project` still wins, and an explicitly passed `ID` still wins over both
- [ ] The project derivation in `appendKnowledgeRow` matches the fallback chain the other three derivations in the same file already use
- [ ] A regression test drives the bare no-flag invocation and asserts a non-null ID, with a positive control that a passed ID is preserved unchanged
- [ ] The existing null-ID decision row recording the `Zenith/**` gate-category coupling is given its ID, or the convention for repairing an append-only row is recorded
