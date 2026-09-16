---
key: FD-7
type: EPIC
title: FD M5 — Gigafactory hardening
category: null
parent: null
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: MODERATE
risk: LOW
storyPoints: null
position: 7
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: ra3z7djjbmyt87lz1vn1nv12
  risk: LOW
  type: EPIC
  title: FD M5 — Gigafactory hardening
  number: 7
  flagged: false
  due_date: null
  position: 7
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: v4nhf8t8xfixi16z4sy0naq2
  complexity: MODERATE
  created_at: 2026-09-10T22:31:08.490722
  project_id: pwd7f8n4egx222rkl7emmzrx
  resolution: null
  updated_at: 2026-09-10T22:31:08.490722
  assignee_id: null
  category_id: null
  environment: null
  reporter_id: n9xhr52s8n49fa832egoqf4z
  resolved_at: null
  story_points: null
  reproducibility: null
---
## Goal

Megafactory generators at promise scale (100k buildings / 250k items); thermal ladder (#4); dirty-range instancing (#6) if not already landed; the flow-field fallback decision (§13); memory/battery soak.

## Definition of Done

- [ ] A promise-scale factory holds 30 UPS at or under 8 ms with 60 fps render on the floor device through a 2-hour thermal soak
- [ ] Save at or under 30 MB; load at or under 5 s
