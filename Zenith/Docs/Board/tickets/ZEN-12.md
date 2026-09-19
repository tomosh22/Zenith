---
key: ZEN-12
type: TASK
title: Remove FLUX_RENDER_VIEW_PREVIEW
category: Engine
parent: "ZEN-25"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: HIGH
complexity: COMPLEX
risk: HIGH
storyPoints: 13
position: 8
labels: ["human-gate","needs-gpu","gpu-reason:visual-measure"]
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
updatedAt: "2026-09-19T20:50:40.769Z"
claimAttempt: "70e84b7a-b660-4925-9434-53e9b6ff325d"
---
## Goal
`FluxRenderViewType` (`Zenith/Flux/RenderViews/Flux_RenderViews.h`) should enumerate genuinely different KINDS of view (main, shadow cascade, and future ones such as planar reflections). `FLUX_RENDER_VIEW_PREVIEW` is not one of those: it exists so the whole pipeline can be rendered a second time to a second final image for the editor's material and animation previews, and the one consumer that branches on it is `Flux_HDR.cpp` (the exposure path), with the preview slots filled by the preview controller. That is a routing decision leaking into the frame.

The target: while a Flux frame is rendered it has NO knowledge of whether the result goes to the swapchain, the editor viewport or an editor preview. Abstract the destination one level up: a render "request" names its camera, its resolution and its output target, and the preview is the same frame loop run again against a different target (the render graph already owns pass ordering and barriers, so a second execution is a second graph instance, not a branch inside passes). Remove the enum value and every `== FLUX_RENDER_VIEW_PREVIEW` test; the preview controller submits a request instead of claiming view slots by type.

Prove it headless: the render-graph unit suite compiles two requests against the Null backend and asserts identical pass lists and no destination-dependent branch remains (a grep-style unit over the pass table, plus the existing `Flux_RenderViews` units). Windowed: the material preview and the animation editor's slot-6 preview render identically before and after (`RT_PhotoTour` and the editor's preview capture), and the main viewport's exposure is unchanged. Engine gates: every game builds; the pins move only if units are added.

## Definition of Done
- [ ] `FLUX_RENDER_VIEW_PREVIEW` no longer exists; `FluxRenderViewType` names only view kinds, and no pass branches on the output destination
- [ ] The editor material preview and animation preview render through the same frame loop as the main view, against their own targets, and look identical to before (captures attached)
- [ ] A headless unit proves two requests with different destinations produce identical pass lists
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added
