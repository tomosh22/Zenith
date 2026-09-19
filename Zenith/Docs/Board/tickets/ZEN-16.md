---
key: ZEN-16
type: TASK
title: Move Vulkan, D3D12 and Null render backends into dedicated RenderBackends directory
category: Engine
parent: null
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: "MODERATE"
risk: LOW
storyPoints: 5
position: 12
labels: ["tech-debt"]
fixVersions: []
blocks: []
links: []
source:
  id: wa970ljwpifmihhv24pbklzo
  risk: LOW
  type: TASK
  title: Move Vulkan, D3D12 and Null render backends into dedicated RenderBackends directory
  number: 16
  flagged: false
  due_date: null
  position: 12
  priority: MEDIUM
  severity: null
  parent_id: null
  sprint_id: null
  status_id: q3spz9u36h2bf6qa942uqfz1
  complexity: SIMPLE
  created_at: 2026-09-11T21:39:15.244403
  project_id: r1m7idj9n6rkv5b1iulqz2ho
  resolution: null
  updated_at: 2026-09-11T21:40:36.701
  assignee_id: n9xhr52s8n49fa832egoqf4z
  category_id: xmu47blrutzwzsqblzurhc9p
  environment: null
  reporter_id: j8sl77hsstiujri72v58n8vf
  resolved_at: null
  story_points: null
  reproducibility: null
updatedAt: "2026-09-19T17:37:59.142Z"
---
## Goal
`Zenith/Vulkan`, `Zenith/D3D12` and `Zenith/Null` are the three render backends and sit as top-level engine directories beside unrelated modules. Move them under a new `Zenith/RenderBackends/` (`RenderBackends/Vulkan`, `RenderBackends/D3D12`, `RenderBackends/Null`) so the backend set is one directory the `RenderBackend` config fragment selects from.

Mechanics: `git mv` the three directories; update every `#include "Vulkan/..."`, `"Null/..."`, `"D3D12/..."` across engine and games; update `Build/Sharpmake_Common.cs` and `Sharpmake_Zenith.cs` where the backend fragment picks which directory compiles (`Null_` configs compile `Zenith/Null` instead of Vulkan; `D3D12_` is the link-neutrality proof); move the three `CLAUDE.md` files and fix the root `CLAUDE.md` directory table and `Docs/BuildSystem.md`; re-path any layering/allowlist entries. Regenerate and build every configuration prefix (`Vulkan_`, `Null_`, `D3D12_`), because a path mistake in one backend is invisible from the other two.

## Definition of Done
- [ ] The three backends live under `Zenith/RenderBackends/` and nothing remains at the old paths
- [ ] `Vulkan_`, `Null_` and `D3D12_` configurations all build for the engine and every game after `Build\regen.ps1`
- [ ] Allowlists are re-pathed, not loosened; the architecture gates stay green
- [ ] Root `CLAUDE.md`, `Docs/BuildSystem.md` and the three backend `CLAUDE.md` files describe the new layout
