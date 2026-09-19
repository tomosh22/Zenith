---
key: ZEN-8
type: TASK
title: "doc_lint: verify bare ticket-key citations, not just markdown links"
category: Engine
parent: "ZEN-28"
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: 3
position: 4
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: dppjj9yzstm4qgm4m01twl5c
  risk: LOW
  type: TASK
  title: "doc_lint: verify bare ticket-key citations, not just markdown links"
  number: 8
  flagged: false
  due_date: null
  position: 4
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-08-23T21:11:00.812629
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-23T21:11:00.812629
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:37:26.008Z"
---
## Goal

`Tools/doc_lint.ps1`'s C6 check resolves markdown LINKS, so a citation written
as `[ZM-53](…url…)` is verified and a bare bracketed key in prose — `[ZM-50 /
ZEN-2]`, which is how the living docs actually write them — is not checked at
all. Both failure directions are live in the repo today:

* **Dangling**: a key that resolves to no ticket (the ZM-D-177 citation ZM-53
  was filed to fix).
* **Wrong**: a key that resolved to nothing when written and resolves to
  something unrelated now, because keys are allocated sequentially. Questions.md
  tagged a question `[ZM-50 / ZEN-2]` meaning "and an engine twin, unfiled";
  filing an unrelated engine ticket later allocated ZEN-2 to it. A reader who
  follows that lands somewhere plausible and wrong.

The client half of this already exists and is tested: `Find-KeyCitations` in
`Tools/zagent/ZagentClient.psm1` greps the living-doc dirs for a word-bounded
key and is wired into `zagent create`, which warns when the key it just
allocated was already cited. That closes the FORWARD direction at the moment it
arms. Nothing closes the dangling direction, and nothing checks the docs except
at create time.

## Definition of Done

- [ ] `doc_lint.ps1` reports bare bracketed ticket keys in the living docs that
      resolve to no existing ticket, distinct from its existing link check
- [ ] A key inside a fenced code block or an inline span is NOT reported — the
      docs quote example keys, and a check that fires on those gets muted
- [ ] The new check has a test that fails without it, in the harness
      `Tools/Test_*.ps1` already uses
- [ ] Every affected game's pinned baseline bumped from an OBSERVED run
