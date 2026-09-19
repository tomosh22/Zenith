---
key: ZEN-20
type: TASK
title: Remove bSubmitRenderWork from SubmitRenderWork
category: Engine
parent: null
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: SIMPLE
risk: "MEDIUM"
storyPoints: 3
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
updatedAt: "2026-09-19T17:38:27.920Z"
---
## Goal
`Zenith_Core.cpp`: `UpdateEditorAndTuning(bool& bSubmitRenderWork, bool& bShouldUpdateGameLogic)` sets `bSubmitRenderWork = g_xEngine.Editor().Update()` under `ZENITH_TOOLS` and `SubmitRenderWork(bool bSubmitRenderWork)` returns early when it is false, after `UploadFrameConstants`. The flag is only ever false when the editor's update reports it does not want a render this frame (the deferred scene operations path). The question the ticket asks: can this boolean be removed and render work always submitted?

Answer it with evidence, not preference. Enumerate the frames on which `Editor().Update()` returns false (a deferred `LoadScene` drain, a pending scene operation), and establish what submitting the render graph on such a frame would do: render an in-between scene state, or nothing. If always-submit is safe, delete the parameter and the early return and let the editor's deferred operations gate themselves. If it is not, replace the boolean with the thing it actually means (a frame-state query on the editor or scene manager that the submit step reads itself) so the tick no longer threads a flag through three functions. The `Zenith_Editor` unit suite and the scene-load automated tests (`Editor.SceneSaveDeltaClassifiesPublish`, the DP and ZM scene-swap tests) are the guards.

## Definition of Done
- [ ] `bSubmitRenderWork` no longer exists as a threaded parameter: either render work is always submitted or the submit step reads the frame state itself, with the reason recorded in a comment at the seam
- [ ] Deferred scene loads and editor mode changes still render correctly: the editor unit suite and the scene-swap automated tests pass on every game
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added
