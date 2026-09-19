---
id: "fa59601a-dc57-4ba2-b115-96a9af0eae2d"
key: "ZEN-67"
type: "TASK"
title: "Rule on local light shadows: implement point and spot shadows on desktop, or delete the dead cast-shadows field"
category: "Engine"
parent: "ZEN-27"
sprint: null
status: "To Do"
assignee: null
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 2
position: 67
labels: ["needs-human","question"]
fixVersions: []
blocks: []
updatedAt: "2026-09-19T17:35:36.884Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
---
## Goal
`Docs/design/Photorealism.md` §4: *"No local light shadows. Every interior lamp leaks through walls. This is the largest remaining structural gap for interiors."* Hearth HE-ENG-16 records the opposite pull: *"interior lights stay shadowless on mobile (IBL + SSAO + emissive carry the read); the dead `m_bCastShadows` field is removed (no-legacy mandate) or implemented desktop-only."* `Zenith_LightComponent` carries `m_bCastShadows` today and nothing reads it.

This is a ruling before it is a feature, and it is the user's: (a) implement point and spot shadows on desktop (cube or dual-paraboloid maps for points, a single map for spots, a budget of N shadowed lights per frame) and keep them off on the mobile tier, or (b) delete the dead field and record that interiors are lit without local shadows. Zenithmon's new interiors (ZM-D-220, the first authored lights in that game) and RenderTest's interior showcase are the scenes the decision is judged on; `RT_PhotoTour` captures of both are attached to this ticket for the decision. `needs-human`.

## Definition of Done
- [ ] A decision is recorded (option a or b) with the captures that informed it, in the Engine decision log once ZEN-82 lands or on this ticket until then
- [ ] ZEN-68 is re-scoped to the chosen option or closed WONT_DO with the reason
- [ ] Every game's pinned baseline bumped from an OBSERVED run
