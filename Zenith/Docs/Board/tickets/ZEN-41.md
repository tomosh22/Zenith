---
id: "ce18f8ec-8319-4ade-8bf4-038b1f2d184a"
key: "ZEN-41"
type: "TASK"
title: "FluxCompiler lint: reject a raw normal-map decode outside Common/Material.slang"
category: "Engine"
parent: "ZEN-25"
sprint: null
status: "In Progress"
assignee: "agent@zenith.invalid"
priority: "MEDIUM"
complexity: "SIMPLE"
risk: "LOW"
storyPoints: 3
position: 41
labels: []
fixVersions: []
blocks: []
updatedAt: "2026-09-21T02:43:24.283Z"
flagged: false
severity: null
reproducibility: null
dueDate: null
environment: "Null_vs2022_Debug_Win64_True"
claimAttempt: "d56460a1-5ae9-4113-98ea-91b6a7b9e203"
---
## Goal
`Docs/design/Photorealism.md` §4: *"Nothing pairs a texture's exported compression with the decode its shader assumes. §1.10 was exactly that gap"* (BC5 normal maps decoded as three channels cost every terrain shadow, with all five heuristic units green). The decode now has one site (`UnpackNormalMapTS` in `Common/Material.slang`) and the encode is declared per texture in `TextureUsage.ztexdecl`, so the two can only disagree if somebody open-codes a sample again. Nothing mechanically stops that.

Add the FluxCompiler lint the document proposes: reject a raw `Sample(...).xyz * 2.0 - 1.0` (and the equivalent `* 2 - 1` spellings on a two- or three-component swizzle) anywhere outside `Common/Material.slang`, in the same shape as the existing spine lint in `Tools/` (`shader-validation.yml` runs the local script; the lint itself lives under `Tools/`, unprotected). Self-tests: a fixture shader with the open-coded decode fails, `Material.slang` passes, and a shader that calls `UnpackNormalMapTS` passes. Run it over every shipped `.slang` and fix any hit at its source.

## Definition of Done
- [ ] The lint rejects an open-coded normal decode outside `Common/Material.slang`, with self-tests for both directions
- [ ] Every shipped shader passes it, hits fixed at source
- [ ] `Docs/design/Photorealism.md` §4 drops the item and `Zenith/Flux/Shaders/CLAUDE.md` names the rule
- [ ] Every game's pinned baseline bumped from an OBSERVED run if a unit was added
