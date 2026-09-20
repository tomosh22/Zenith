---
key: ZEN-19
type: TASK
title: Flux_ShadowsImpl::UpdateShadowMatrices should be called from within Flux_Shadows instead of in SubmitRenderWork
category: Engine
parent: "ZEN-25"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: MEDIUM
complexity: SIMPLE
risk: LOW
storyPoints: 3
position: 15
labels: ["blocked-scope"]
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
updatedAt: "2026-09-20T21:37:51.472Z"
claimAttempt: "a5b6c9f3-3d69-4e5b-9dc7-f07c6aabc52b"
---
## Goal
`Zenith_Core.cpp` hoists `g_xEngine.Shadows().UpdateShadowMatrices()` into the main-thread seam ("Stage 2: hoisted UpdateShadowMatrices (pre-gather)") and includes `Flux/Shadows/Flux_ShadowsImpl.h` to do it. The shadow system should own when its matrices update; the frame loop should not reach into a Flux subsystem's implementation header. `Flux_RenderViews.h` records the constraint that makes the hoist exist: cascade views activate where their constants are staged, and the gather that follows needs the matrices already computed.

Find the clean seam: a `Flux_Shadows` entry point called from Flux's own pre-gather step (or the render-view update that already runs before the gather), so `Zenith_Core.cpp` drops the include and the call. **If there is no clean way to do this, add a comment to this ticket saying why and let the human address it. Do not perform a hacky, pointless refactor** (the ticket's own instruction). The proof is that the shadow cascades are unchanged: the CSM units and `RT_PhotoTour`'s shadowed poses render identically.

## Definition of Done
- [ ] `Zenith_Core.cpp` no longer includes `Flux_ShadowsImpl.h` or calls `UpdateShadowMatrices`; the shadow system updates its own matrices at the same point in the frame, or a comment on this ticket records why that is not clean
- [ ] Shadow cascades are unchanged: CSM units green and a photo-tour comparison shows no difference
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added
