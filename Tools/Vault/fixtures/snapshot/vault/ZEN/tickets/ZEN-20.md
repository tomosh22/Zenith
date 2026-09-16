---
key: ZEN-20
type: TASK
title: Remove bSubmitRenderWork from SubmitRenderWork
category: Engine
parent: null
sprint: null
status: To Do
assignee: agent@saas.invalid
priority: MEDIUM
complexity: SIMPLE
risk: HIGH
storyPoints: null
position: 16
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: tagn27rzxh7msdw2blhwi3n0
  risk: HIGH
  type: TASK
  title: Remove bSubmitRenderWork from SubmitRenderWork
  number: 20
  flagged: false
  due_date: null
  position: 16
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T21:50:02.316464
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:51:28.9
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
---
bSubmitRenderWork looks very hacky. It is only set to false if ProcessDeferredSceneOperations returns false. Can we remove this boolean and just always submit render work?