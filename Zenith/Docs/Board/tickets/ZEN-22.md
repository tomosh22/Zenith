---
key: ZEN-22
type: TASK
title: Remove const char* szName overloads of Flux_ShaderBinder functions
category: Engine
parent: "ZEN-25"
sprint: null
status: To Do
assignee: null
priority: MEDIUM
complexity: "MODERATE"
risk: LOW
storyPoints: 5
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
updatedAt: "2026-09-19T17:38:40.344Z"
---
## Goal
`Zenith/Flux/Slang/Flux_ShaderBinder.h` still declares the by-name overloads `BindCBV`, `BindSRV`, `BindUAV_Texture`, `BindUAV_Buffer`, `BindSRV_Buffer` and `BindDrawConstants` taking `const char* szName`, resolved at call time through `ResolveNamedBinding(const Flux_ShaderReflection*, const char*)`, and grants `friend class Zenith_UnitTests` so a unit can call the resolver with a synthetic reflection. The binding-model overhaul made `Flux_BindingHandle&` the API: a handle is resolved once from reflection, so a by-name call per bind is both slower and a second way to spell the same thing.

Remove every `const char* szName` overload; migrate any remaining caller in engine or games to a `Flux_BindingHandle` resolved at setup; delete `ResolveNamedBinding` and the `Zenith_UnitTests` friend declaration once nothing needs them, and move whatever the resolver unit proved onto the handle-resolution path so the coverage is not lost. Shader validation (`shader-validation.yml`'s local equivalent in the Flux binding-validation units) is the guard that a binding that used to resolve by name still resolves by handle.

## Definition of Done
- [ ] No `const char*` binding overload remains on `Flux_ShaderBinder`; every engine and game caller binds through `Flux_BindingHandle`
- [ ] `ResolveNamedBinding` and the `Zenith_UnitTests` friend declaration are deleted, and the resolver's unit coverage lives on the handle path
- [ ] Every game builds and its render tests pass; binding validation units green
- [ ] Every game's pinned baseline bumped from an OBSERVED run
