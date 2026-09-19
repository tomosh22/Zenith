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
updatedAt: "2026-09-19T13:51:28.841Z"
---
## Goal

`Zenith_Vulkan_Sampler::InitialiseSampler` (`Zenith\Vulkan\Zenith_Vulkan_Texture.cpp`) currently
decides the mip load bias itself: it computes `fMipLodBias` from the `eFilter` parameter
(`-0.5f` for the linear-filtered repeat/clamp samplers, `0.0f` for the nearest-filtered
point-clamp sampler) and bakes it straight into the `vk::SamplerCreateInfo`. That decision
should move up to the backend-neutral Flux level, the same way `bAnisotropic` is already an
explicit parameter of `InitialiseSampler` rather than something the function derives on its
own — the caller decides, Vulkan just applies it.

`Flux_Graphics.cpp::Init` already makes the equivalent backend-neutral decision for which
sampler shape to create by choosing which of `Flux_Sampler::InitialiseRepeat` /
`InitialiseClamp` / `InitialisePointClamp` to call for `m_xRepeatSampler` / `m_xClampSampler`
/ `m_xPointSampler`. Thread the mip load bias down through that same call path instead of
recomputing it from `eFilter` inside the Vulkan translation layer.

## Definition of Done

- [ ] The mip load bias value is decided at the Flux level (`Flux_Graphics.cpp` or equivalent
      backend-neutral call site) and passed as an explicit parameter into
      `Flux_Sampler::InitialiseRepeat` / `InitialiseClamp` / `InitialisePointClamp`, rather than
      `Zenith_Vulkan_Sampler::InitialiseSampler` deriving it from `eFilter` internally.
- [ ] The `Zenith_Vulkan`, `Zenith_Null` and `Zenith_D3D12` `InitialiseRepeat` / `InitialiseClamp`
      / `InitialisePointClamp` signatures are updated consistently so all three backends keep
      matching signatures.
- [ ] Behaviour is unchanged: the repeat/clamp (linear-filtered) samplers still get a -0.5 mip
      load bias, and the point-clamp (nearest-filtered) sampler still gets 0.0.
- [ ] Every affected game's pinned unit baseline bumped from an OBSERVED run, if the change adds
      or removes any unit.
