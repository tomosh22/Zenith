---
key: ZEN-10
type: TASK
title: Move mip load bias out of Vulkan and into Flux
category: Engine
parent: null
sprint: null
status: "Done"
assignee: "agent@zenith.invalid"
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
updatedAt: "2026-09-19T14:04:38.679Z"
claimAttempt: null
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

## Comments

Moved the mip-load-bias decision out of `Zenith_Vulkan_Sampler::InitialiseSampler` (which derived it from `eFilter`) and into the backend-neutral `Flux_GraphicsImpl::InitialiseSamplers()`, threaded down as an explicit `float fMipLodBias` parameter through `InitialiseRepeat`/`InitialiseClamp`/`InitialisePointClamp` on all three backends (Vulkan, Null, D3D12) — mirroring how `bAnisotropic` is already an explicit parameter. Bias values are unchanged (-0.5 filtered, 0.0 point-clamp); no other sampler/texture behaviour touched. All gates green on both `measure` and `verify`; no pinned baseline change needed (no unit added/removed). Committed to `master` as `5bd5e0bb41d084a6a48e18807c878e5ea7827f8f`.

<!-- agent:worklog:start ZEN-10 -->
## Work Log — 2026-09-19

**Outcome**: merged to `master` as `5bd5e0bb41d084a6a48e18807c878e5ea7827f8f` · ran on sonnet/low via SIMPLE+MEDIUM · category Engine

**What changed**

- `Zenith/Vulkan/Zenith_Vulkan_Texture.cpp` — `InitialiseSampler` now takes `fMipLodBias` as a parameter instead of deriving it from `eFilter`; the three public `Initialise*` functions forward the caller-supplied bias.
- `Zenith/Vulkan/Zenith_Vulkan.h` — added `float fMipLodBias` to the `InitialiseRepeat`/`InitialiseClamp`/`InitialisePointClamp` declarations.
- `Zenith/Null/Zenith_Null.h` — matching unused `float /*fMipLodBias*/` parameter on the three no-op stubs.
- `Zenith/D3D12/Zenith_D3D12.h` — same signature update as Null, for the same reason.
- `Zenith/Flux/Flux_Graphics.cpp` — `Flux_GraphicsImpl::InitialiseSamplers()` now decides the two bias values (`-0.5f` filtered, `0.0f` point-clamp) and passes them explicitly into the three `Flux_Sampler::Initialise*` calls.

**Files** (`git diff --stat`)

```
 Zenith/D3D12/Zenith_D3D12.h             |  6 +++---
 Zenith/Flux/Flux_Graphics.cpp           | 17 ++++++++++++++---
 Zenith/Null/Zenith_Null.h               |  6 +++---
 Zenith/Vulkan/Zenith_Vulkan.h           |  9 ++++++---
 Zenith/Vulkan/Zenith_Vulkan_Texture.cpp | 34 ++++++++++++++++-----------------
 5 files changed, 43 insertions(+), 29 deletions(-)
```

**Gates**

| Command | Result |
|---|---|
| `zenith build engine` | pass |
| `zenith build Combat --headless` | pass |
| `zenith build Combat --config D3D12_vs2022_Debug_Win64_False` | pass |
| `zenith build Zenithmon --headless` | pass |
| `zenith build CityBuilder --headless` | pass |
| `zenith build RenderTest --headless` | pass |
| `zenith build TilePuzzle --headless` | pass |
| `zenith build DevilsPlayground --headless` | pass |
| `zenith build ScriptTest --headless` | pass |
| `zenith build Foundry --headless` | pass |
| `zenith build Undervault --headless` | pass |
| `run_unit_gate.ps1 -Game Combat` | pass (baseline unchanged — no unit added/removed) |

Both `-Phase measure` and `-Phase verify` runs returned `exitCode: 0` / `"all gates green"`, `state: complete`, `headSha` matching the pre-commit working tree. `skipped: []` on both. No pin bump needed.

**Coverage**

| Changed file | What executes the changed line |
|---|---|
| `Zenith/Flux/Flux_Graphics.cpp` | Runs at engine init on every backend (Vulkan/Null/D3D12), every gate build/boot — but nothing asserts on the bias *value* reaching the sampler. |
| `Zenith/Null/Zenith_Null.h`, `Zenith/D3D12/Zenith_D3D12.h` | Compiled and (Null only) executed as no-ops on every headless boot; the parameter is accepted and discarded, so nothing observes correctness. |
| `Zenith/Vulkan/Zenith_Vulkan_Texture.cpp`, `Zenith/Vulkan/Zenith_Vulkan.h` | **Nothing.** The headless (`Null_*`) gates that run in CI never compile this file (Null configs compile `Zenith/Null` instead of Vulkan), and the D3D12 gate compiles but never executes its own stub, let alone Vulkan's. The only path that compiles+runs this code is a windowed Vulkan boot, which no automated gate drives. This is pre-existing — the ticket did not reduce coverage, but it also didn't add any for the value it moved. |

**Definition of Done**

- [x] The mip load bias value is decided at the Flux level (`Flux_Graphics.cpp`) and passed as an explicit parameter into `Flux_Sampler::InitialiseRepeat`/`InitialiseClamp`/`InitialisePointClamp`, rather than `Zenith_Vulkan_Sampler::InitialiseSampler` deriving it from `eFilter` internally.
- [x] The `Zenith_Vulkan`, `Zenith_Null` and `Zenith_D3D12` `InitialiseRepeat`/`InitialiseClamp`/`InitialisePointClamp` signatures are updated consistently so all three backends keep matching signatures.
- [x] Behaviour is unchanged: the repeat/clamp (linear-filtered) samplers still get a -0.5 mip load bias, and the point-clamp (nearest-filtered) sampler still gets 0.0.
- [x] Every affected game's pinned unit baseline bumped from an OBSERVED run, if the change adds or removes any unit. (N/A — no unit added or removed; gate `-Phase measure` returned exit 0, not 10, confirming the baseline is unchanged.)

**Durable findings**: none

**Deviations / follow-ups**

- The ticket's original description (before this tick) had no `## Goal`/`## Definition of Done` sections at all and failed the contract check on first claim attempt (exit 4). I rewrote the body with `zagent update --body` before re-claiming, reconciling the "similar to anisotropy" framing against the actual code: anisotropy's *enable/disable* choice (`bAnisotropic`) is already a parameter of `InitialiseSampler`, but its *magnitude* (16x) is still hardcoded in Vulkan — so the analogy is approximate, not exact. The rewritten Goal describes the real precedent (the parameter-threading pattern) rather than a stronger equivalence than the code supports.
- Independently verified the worker's diff myself (I2) before running gates: confirmed both bias values unchanged, all three backend signatures consistent and in the same parameter position, no anisotropy/filter/address-mode changes, and no stray test/baseline edits. Reviewer (dispatched because risk MEDIUM is in `reviewOn`, and two DoD lines assert an equivalence no gate alone can verify) independently confirmed the same and returned zero blocking findings.
- No unit test added for this change (see Coverage above) — flagged as a pre-existing gap the worker/reviewer both noted rather than something newly introduced.
<!-- agent:worklog:end -->
