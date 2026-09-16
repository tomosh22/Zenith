---
key: ZEN-14
type: TASK
title: Zenith library source files must be in their own dedicated directories
category: Engine
parent: null
sprint: null
status: To Do
assignee: agent@saas.invalid
priority: MEDIUM
complexity: MODERATE
risk: MEDIUM
storyPoints: null
position: 10
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: m3b3a8je19zyw6kued14n5nf
  risk: MEDIUM
  type: TASK
  title: Zenith library source files must be in their own dedicated directories
  number: 14
  flagged: false
  due_date: null
  position: 10
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: MODERATE
  created_at: 2026-09-11T21:34:05.259819
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:38:09.874
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
---
Zenith libraries, such as ZenithPhysics and ZenithECS currently have their source files in the same directory as all the other Zenith source files. This means that the source files get included in the main Zenith library vcxproj and have to be explicitly marked as being excluded from build. These file should live in a physically different directory and Sharpmake shouldn't have to exclude them from the main Zenith library as they shouldn't be there in the first place.