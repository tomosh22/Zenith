---
key: ZEN-9
type: TASK
title: Nothing checks that the twinned needsDocsTree rule agrees across client and board
category: Engine
parent: null
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: SIMPLE
risk: MEDIUM
storyPoints: null
position: 5
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: hgcigrrssx397l7isgup2ono
  risk: MEDIUM
  type: TASK
  title: Nothing checks that the twinned needsDocsTree rule agrees across client and board
  number: 9
  flagged: false
  due_date: null
  position: 5
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-08-23T21:12:08.917394
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-23T21:12:08.917394
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:37:30.100Z"
---
## Goal

The predicate that decides which commands upload the client's living-doc tree
is spelled TWICE, on purpose — `needsDocsTree` in
`packages/agent/src/dispatch.ts` on the board, and `Test-NeedsDocsTree` in
`Tools/zagent/ZagentClient.psm1` on the client. The duplication is not
avoidable: the two run on different machines in different languages, and the
client must decide what to upload before the board sees the request.

Both files say in their own comments that the DRIFT is the danger, and describe
the failure mode precisely — a command that reads the docs but is missing from
the client's list arrives with no tree, and the board answers "no Roadmap.md in
the uploaded tree", which reads like a client mistake rather than a rule that
disagrees with itself.

Nothing checks that the two agree. Each side has its own tests
(`dispatch.test.ts` describe('needsDocsTree'), `Test-ZagentClient.ps1`) and
both pass while disagreeing. This is the shape this repo keeps naming: a rule
stated in prose in front of a check that does not exist. The board comment
already carries one instance of the rot it warns about — it names
`Tools/zagent/zagent.ps1` as the twin, which is where the function used to live
before it moved into `ZagentClient.psm1`.

## Definition of Done

- [ ] A check fails when the two command lists disagree, rather than either
      side's own tests passing independently
- [ ] The check names which side is missing which command, since "they differ"
      without a direction sends the reader to both files
- [ ] The stale cross-reference in the board-side comment points at the file
      that actually holds the twin
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run

## Comments

**Closed WONT_DO 2026-09-19 by the board audit: the twin no longer exists.** The ticket asked for a check that `needsDocsTree` in `packages/agent/src/dispatch.ts` (the remote SaaS board) and `Test-NeedsDocsTree` in `Tools/zagent/ZagentClient.psm1` agree. The remote board and its docs-tree upload were retired at the Vault cutover (2026-09-16, ZEN-23): `zagent.ps1` is a dispatch shim onto `Tools/Vault/cli/client.mjs`, the board is this checkout, and no command ships a docs tree anywhere. There is one predicate now, so there is nothing to drift.
