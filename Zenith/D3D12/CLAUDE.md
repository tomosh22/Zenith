# D3D12 Backend (reserved)

## Overview

A **no-op "null" D3D12 render backend**. Its only purpose is to be a **compile +
link + conformance oracle** that proves the Flux renderer surface is genuinely
backend-neutral: the entire engine + games compile and link against it without
any concept or call referring to a concrete Vulkan type.

> **This is NOT the headless backend.** `Zenith/Null` is — see
> [Null/CLAUDE.md](../Null/CLAUDE.md). The two are deliberate twins with exactly
> three intended divergences (the define, the hidden window, and which system
> libs are linked); D3D12 is the one held in reserve for a REAL D3D12
> implementation, while Null stays a no-op forever. Any change to the Flux
> backend concepts updates Vulkan + D3D12 + Null together.

It performs **zero real rendering**. There is no `<d3d12.h>`, no device, no
swapchain, no GPU work — every method is an inline no-op that returns a benign
dummy value. Real D3D12 (DXIL, descriptor heaps, presentation) is future work.
**Vulkan stays the real, shipping backend; Android is Vulkan-only.**

## How it plugs in

Selected by the Sharpmake `RenderBackend` fragment (`D3D12_*` configs define
`ZENITH_D3D12`). The seam:

- `Core/Zenith_BackendAliases.h` — `#elif defined(ZENITH_D3D12)` block
  forward-declares the 12 `Zenith_D3D12_*` classes and aliases
  `Flux_* = Zenith_D3D12_*`. (Moved out of `Flux/Flux_Fwd.h` on 2026-08-05 —
  see `Zenith/Null/CLAUDE.md` for why; `Flux/Flux_Fwd.h` includes it, so no
  consumer changed.) It also `#error`s unless EXACTLY ONE backend is defined.
- `Flux/Flux_BackendTypes.h` — pulls `Zenith_PlatformGraphics_Include_D3D12.h`
  (this dir) for the full class definitions when `ZENITH_D3D12` is set.
- `Flux/Backend/Flux_BackendConformance.cpp` — a `#ifdef ZENITH_D3D12` block
  `static_assert`s every backend concept against the `Zenith_D3D12_*` classes.

## Files

| File | Mirrors (Vulkan) | Concepts satisfied |
|------|------------------|--------------------|
| `Zenith_D3D12.h` + `Zenith_D3D12.cpp` | `Zenith_Vulkan` + `_Sampler` + `_VRAM` | `FluxBackendDevice` (+ `ImGuiTools` in tools) |
| `Zenith_D3D12_MemoryManager.h/.cpp` | `Zenith_Vulkan_MemoryManager` | `MemoryAlloc`, `MemoryDelete`, `TransientAliasing` |
| `Zenith_D3D12_CommandBuffer.h` | `Zenith_Vulkan_CommandBuffer` | `CommandRecorder` (all sub-concepts) + `Sync` |
| `Zenith_D3D12_Swapchain.h` | `Zenith_Vulkan_Swapchain` | `Presentation` |
| `Zenith_D3D12_Pipeline.h` | `Zenith_Vulkan_Pipeline` (6 classes) | `Shader`, `PipelineBuilder`, `ComputePipelineBuilder`, `RootSigBuilder` |
| `Zenith_PlatformGraphics_Include_D3D12.h` | `Zenith_PlatformGraphics_Include.h` | — (the heavy include hub) |

## Conventions / gotchas

- **Stubs are header-only inline** EXCEPT `Zenith_D3D12_MemoryManager.cpp`: the
  `Initialise*Buffer` methods touch the `Flux_*Buffer` wrappers (`Flux_Buffers.h`),
  which can't be included from a seam-reachable header without re-entering the
  `Flux.h` cycle — so they live in the `.cpp`, outside that cycle.
- **Dummy resources look valid.** `Create*VRAM` / `Create*View` / `Initialise*Buffer`
  hand back a monotonic non-zero handle (`ms_uDummyHandle`) on the buffer + every
  view, and a real `m_ulSize`, so the engine's resource-validity asserts pass.
  The swapchain returns a stamped dummy `Flux_RenderAttachment` from
  `GetCurrentSwapchainTarget` so the render graph's final pass has a target.
- **Include `Flux/Flux_Types.h`, NOT `Flux/Flux.h`** in the stub headers (the
  full `Flux.h` is the seam cycle). Win32 types come via the seam's `<Windows.h>`
  (real D3D12 would get it from `<d3d12.h>`); the `APIENTRY` undef there avoids a
  GLFW redefinition clash that `vulkan.hpp` masks on the Vulkan side.
- **No per-frame callbacks needed.** Device `Initialise()` is a pure no-op; the
  engine's per-frame ring advances on its own. `Swapchain::BeginFrame()` returns
  true; `EndFrame()` presents nothing.
- **`RecordFrame` still runs the pass callbacks.** The one device method that is NOT
  a no-op: `Zenith_D3D12::RecordFrame` (out-of-line in `Zenith_D3D12.cpp`) iterates the
  queued render passes and calls `Flux_RenderGraph::RecordPassInto` into a no-op
  `Zenith_D3D12_CommandBuffer`. No GPU work happens, but the pass record callbacks DO
  run — so their side effects (buffer uploads, ECS reads, CPU draw-list builds) occur
  exactly as on Vulkan. This is what lets a full gameplay session run on the null
  backend. The recorder methods are no-ops but still call the shared
  `Flux_RecordValidation.h` asserts (null/handle/size checks).
- **Shaders load reflection only.** `Zenith_D3D12_Shader::Initialise` deserialises
  the checked-in `<program>.spv.refl` via the Slang-free
  `Flux_ShaderReflection::ReadFromDataStream` so the name-based binder resolves —
  no SPIR-V, no Slang, no GPU module.

## Verifying neutrality

CI **builds** `D3D12_vs2022_Debug_Win64_False` in `zm-tests`, `dp-tests` and
`cb-tests`. A clean link with zero unresolved externals = every concept AND
non-concept call the engine makes is satisfied by a second backend.

**CI never EXECUTES a D3D12 exe** (an earlier version of this file claimed it ran
`CB_HumanSession` continuously — it did not). That run was a one-off local
windowed session: CityBuilder reached `passed:true` over 4000 frames with 0
asserts on this backend, real evidence that the surface can carry a full gameplay
session, but a point-in-time result rather than a standing gate. Continuous
execution proof now comes from the Null twin, which every CI gate runs.
