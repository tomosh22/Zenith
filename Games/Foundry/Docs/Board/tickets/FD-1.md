---
key: FD-1
type: EPIC
title: FD M0 — Two parallel spikes (the de-risk milestone)
category: null
parent: null
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: MODERATE
risk: LOW
storyPoints: null
position: 1
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: c9yw9kefsqx7q3msdhythq7l
  risk: LOW
  type: EPIC
  title: FD M0 — Two parallel spikes (the de-risk milestone)
  number: 1
  flagged: false
  due_date: null
  position: 1
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: v4nhf8t8xfixi16z4sy0naq2
  complexity: MODERATE
  created_at: 2026-09-10T22:31:04.11827
  project_id: pwd7f8n4egx222rkl7emmzrx
  resolution: null
  updated_at: 2026-09-10T22:31:04.11827
  assignee_id: null
  category_id: null
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
---
## Goal

SimCore spike (belts + inserters + a machine archetype, command queue, digest + op-count harness) AND the device render spike — which the TDD calls "the single riskiest assumption in the project": the in-tree instancing record is 2,520 instances, desktop, and Foundry needs ~20k on Adreno and Mali.

## Definition of Done

- [ ] A generated 10k-line/50k-item belt city ticks in under 2 ms on desktop
- [ ] Digests match across /Od and /O2; op-counts pinned
- [ ] The device spike measures 20k building + 5k moving instances at the §15 budgets — 60 fps, or the fallback decision (chunk-merged statics / bespoke belt-item renderer) taken NOW
- [ ] While it is cheap
