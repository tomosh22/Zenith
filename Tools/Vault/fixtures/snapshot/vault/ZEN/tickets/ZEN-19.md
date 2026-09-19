---
key: ZEN-19
type: TASK
title: Flux_ShadowsImpl::UpdateShadowMatrices should be called from within Flux_Shadows instead of in SubmitRenderWork
category: Engine
parent: null
sprint: null
status: To Do
assignee: agent@zenith.invalid
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: null
position: 15
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: uyt3pthedn8wjnbnh2byeen3
  risk: LOW
  type: TASK
  title: Flux_ShadowsImpl::UpdateShadowMatrices should be called from within Flux_Shadows instead of in SubmitRenderWork
  number: 19
  flagged: false
  due_date: null
  position: 15
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T21:46:30.691434
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:48:45.365
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
---
If there isn't a clean way of doing this then add a comment to this ticket and let the human address it, do not perform a hacky, pointless refactor.