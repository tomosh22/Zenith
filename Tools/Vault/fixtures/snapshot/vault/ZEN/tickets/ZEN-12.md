---
key: ZEN-12
type: TASK
title: Remove FLUX_RENDER_VIEW_PREVIEW
category: Engine
parent: null
sprint: null
status: To Do
assignee: agent@saas.invalid
priority: HIGH
complexity: COMPLEX
risk: HIGH
storyPoints: null
position: 8
labels: []
fixVersions: []
blocks: []
links: []
source:
  id: g30r57ppgqimjysvh27gjf6d
  risk: HIGH
  type: TASK
  title: Remove FLUX_RENDER_VIEW_PREVIEW
  number: 12
  flagged: false
  due_date: null
  position: 8
  priority: HIGH
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: COMPLEX
  created_at: 2026-09-11T21:27:24.056525
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:31:25.677
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
---
FluxRenderViewType should be for different render views (main, shadow cascade, maybe future views for planar reflections, etc.). FLUX_RENDER_VIEW_PREVIEW exists for rendering the whole render pipeline again to a second final image. This should be abstracted at a higher level. The preview image should be rendered in the same way as the main render image presented to the user or the editor viewport. This could potentially be done by running the entire render loop again but to the preview image instead of the swapchain. While a Flux frame is being rendered it should have no knowledge of whether it is being rendered to the swapchain, the main editor viewport, or an editor preview.