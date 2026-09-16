---
key: ZEN-22
type: TASK
title: Remove const char* szName overloads of Flux_ShaderBinder functions
category: Engine
parent: null
sprint: null
status: To Do
assignee: agent@saas.invalid
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: null
position: 18
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: rzbp81ydr07zulz53kepq6vz
  risk: LOW
  type: TASK
  title: Remove const char* szName overloads of Flux_ShaderBinder functions
  number: 22
  flagged: false
  due_date: null
  position: 18
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T22:02:03.289092
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T22:04:07.141
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
---
Flux_BindingHandle& overloads must be used instead. The const char* overloads must be completely removed. ResolveNamedBinding and the Zenith_UnitTests friend declaration should be able to go as well