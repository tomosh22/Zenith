---
key: ZEN-1
type: EPIC
title: Engine gates & pinned baselines
category: Engine
parent: null
sprint: null
status: To Do
assignee: null
priority: HIGH
complexity: MODERATE
risk: MEDIUM
storyPoints: null
position: 1
labels:
  []
fixVersions: []
blocks: []
links: []
source:
  id: npl4x2loj6gm0dzcuzj9kgm1
  risk: MEDIUM
  type: EPIC
  title: Engine gates & pinned baselines
  number: 1
  flagged: false
  due_date: null
  position: 1
  priority: HIGH
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: MODERATE
  created_at: 2026-08-18T01:24:12.702524
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-08-22T23:29:17.27
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:37:01.433Z"
---
## Goal

The engine unit gate and the properties that make a green run meaningful across every game. A backend-neutral engine unit moves EVERY game's pinned count, and the pin has exactly ONE home, `Tools/unit_baselines.json`, read by game name from every gate line and workflow; the gate asserts `ran == baseline` EXACTLY, so a grown suite reds with zero failures until the pin is bumped from an OBSERVED `Null_` run.

Children: the advisory RenderTest row no workflow reads (ZEN-3, `needs-human` because the fix is a workflow leg), the float assertion that passes on NaN (ZEN-7), the RenderTest units that depend on a prior tools boot's asset (ZEN-84), and the aux tools that are pre-existing red in `ToolsEnabled=True` so a whole-solution build is impossible (ZEN-85).
## Definition of Done

- [ ] Every child issue Done
