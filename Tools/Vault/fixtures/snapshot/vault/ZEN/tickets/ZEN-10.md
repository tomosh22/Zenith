---
key: ZEN-10
type: TASK
title: Move mip load bias out of Vulkan and into Flux
category: Engine
parent: null
sprint: null
status: To Do
assignee: null
priority: LOW
complexity: SIMPLE
risk: MEDIUM
storyPoints: 1
position: 6
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: uu7t5biyrbstgou20dkvk9un
  risk: MEDIUM
  type: TASK
  title: Move mip load bias out of Vulkan and into Flux
  number: 10
  flagged: false
  due_date: null
  position: 6
  priority: LOW
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-03T21:42:50.401656
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-03T21:44:24.157
  assignee_id: null
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: 1
  reproducibility: null
---
InitialiseSampler in Zenith_Vulkan_Texture.cpp is currently responsible for deciding what mip load bias a sampler should have, this decision should be made and the Flux level and passed down to Vulkan, similar to anisotropy