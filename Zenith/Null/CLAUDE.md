# Null Render Backend — THE headless/CI backend

## Overview

A **GPU-less no-op render backend**. Unlike the reserved D3D12 twin (which is a
compile/link oracle nobody executes), this backend is **executed constantly**:
every headless automated-test batch, every boot unit gate, and every CI run in
`zm-tests` / `dp-tests` / `cb-tests` / `engine-gate` runs a `Null_*` exe.

It performs **zero real rendering** — no device, no swapchain, no GPU work; every
method is an inline no-op returning a benign dummy value. What it is NOT is a
"skip everything" mode: the engine's render paths all still **run**, against
these no-ops. Pass record callbacks fire, buffer uploads are issued, the editor
composes a full ImGui frame. That is the point — a headless run exercises the
same code a windowed one does.

> **Headless is a BUILD CONFIG, not a runtime flag.** There is no `--headless`.
> `Null_*` configs define `ZENITH_NULL_RENDERER`; that macro IS the headless
> signal. See the root `CLAUDE.md` build-configuration section.

## How it plugs in

Selected by the Sharpmake `RenderBackend` fragment (`Null_*` configs define
`ZENITH_NULL_RENDERER`). The seam:

- `Core/Zenith_BackendAliases.h` — `#elif defined(ZENITH_NULL_RENDERER)` block
  forward-declares the 12 `Zenith_Null_*` classes and aliases
  `Flux_* = Zenith_Null_*`. (It lived in `Flux/Flux_Fwd.h` until 2026-08-05;
  every name it declares is a BACKEND class selected by a build define, not a
  Flux type, and owning it Flux-side forced `Core/Zenith_Engine.h` to carry a
  Core -> Flux include edge. `Flux/Flux_Fwd.h` includes it, so every existing
  consumer is unaffected.)
- `Flux/Flux_BackendTypes.h` — pulls `Zenith_PlatformGraphics_Include_Null.h`
  (this dir) for the full class definitions.
- `Flux/Backend/Flux_BackendConformance.cpp` — a `#ifdef ZENITH_NULL_RENDERER`
  block `static_assert`s every backend concept against the `Zenith_Null_*` classes.
- `Core/Zenith_BackendAliases.h` — `#error`s unless EXACTLY ONE of
  `ZENITH_VULKAN` / `ZENITH_D3D12` / `ZENITH_NULL_RENDERER` is defined.

## Relationship to `Zenith/D3D12`

The two are **deliberate twins**, and D3D12 is the one that is *reserved*:

| | `Zenith/Null` | `Zenith/D3D12` |
|---|---|---|
| Purpose | THE headless/CI backend | Link-neutrality proof + the future home of a real D3D12 implementation |
| Executed? | Constantly (every CI gate) | Compiled + linked only |
| Future | Stays a no-op forever | Becomes real D3D12 |

**Intended divergences — there are exactly three.** Everything else must stay
identical:

1. The define (`ZENITH_NULL_RENDERER` vs `ZENITH_D3D12`).
2. **The window is created HIDDEN.** `Zenith_Windows_Window.cpp` sets
   `GLFW_VISIBLE=FALSE` + `glfwHideWindow` under `ZENITH_NULL_RENDERER`. The
   window is still CREATED (not skipped): ImGui platform init, input pumping and
   window-size queries all depend on it, and the hidden-window shape is what CI
   has proven for months.
3. **No `d3d12` / `dxgi` / `dxguid` libs** are linked (nor any Vulkan/Slang).
   glfw3 is the only graphics-adjacent lib.

**Sync rule:** any change to the Flux backend concepts updates **Vulkan + D3D12 +
Null together**. A concept added to only one backend fails `Flux_BackendConformance.cpp`.

## Files

| File | Mirrors (Vulkan) | Concepts satisfied |
|------|------------------|--------------------|
| `Zenith_Null.h` + `Zenith_Null.cpp` | `Zenith_Vulkan` + `_Sampler` + `_VRAM` | `FluxBackendDevice` (+ `ImGuiTools` in tools) |
| `Zenith_Null_MemoryManager.h/.cpp` | `Zenith_Vulkan_MemoryManager` | `MemoryAlloc`, `MemoryDelete`, `TransientAliasing` |
| `Zenith_Null_CommandBuffer.h` | `Zenith_Vulkan_CommandBuffer` | `CommandRecorder` (all sub-concepts) + `Sync` |
| `Zenith_Null_Swapchain.h` | `Zenith_Vulkan_Swapchain` | `Presentation` |
| `Zenith_Null_Pipeline.h` | `Zenith_Vulkan_Pipeline` (6 classes) | `Shader`, `PipelineBuilder`, `ComputePipelineBuilder`, `RootSigBuilder` |
| `Zenith_Null_ImGuiIntegration.cpp` | `Zenith_Vulkan_ImGuiIntegration.cpp` | — (editor texture registration) |
| `Zenith_PlatformGraphics_Include_Null.h` | `Zenith_PlatformGraphics_Include.h` | — (the heavy include hub) |

## Conventions / gotchas

- **Stubs are header-only inline** EXCEPT `Zenith_Null_MemoryManager.cpp` and the
  out-of-line members in `Zenith_Null.cpp`: the `Initialise*Buffer` methods touch
  the `Flux_*Buffer` wrappers (`Flux_Buffers.h`), which can't be included from a
  seam-reachable header without re-entering the `Flux.h` cycle.
- **Include `Flux/Flux_Types.h`, NOT `Flux/Flux.h`** in the stub headers (the full
  `Flux.h` is the seam cycle). Win32 types come via `Core/Zenith_Win32.h`, which
  owns the GLFW-`APIENTRY` and `WIN32_LEAN_AND_MEAN` guards — never include
  `<Windows.h>` raw.
- **Dummy resources look valid.** `Create*VRAM` / `Create*View` / `Initialise*Buffer`
  hand back a monotonic non-zero handle (`ms_uDummyHandle`) and a real `m_ulSize`,
  so the engine's resource-validity asserts pass. Note `Initialise*Buffer` stamps
  its handle DIRECTLY -- it does not route through `CreateBufferVRAM`, so the two
  must be kept in step by hand.
- **How Q-2026-07-21-001 is actually closed.** The `Invalid buffer VRAM handle`
  assert lives in `Zenith_Vulkan_MemoryManager_Buffers.cpp` -- it is VULKAN-SIDE,
  and a Null build does not compile it at all. So the closure is not "the assert
  is satisfied", it is "that whole code path is gone, and the Null buffer
  initialisers hand back valid-looking handles instead of the invalid ones a
  GPU-less Vulkan boot produced". **Consequence: nothing on the Null path re-checks
  those handles**, so a regression that handed out invalid ones would be silent.
  That is why `Terrain::CullingResourceInitSurvivesOnCurrentBackend` asserts the
  handles' validity EXPLICITLY rather than just surviving the call -- verified by
  mutation (leaving `InitialiseIndirectBuffer`'s handle invalid turns exactly that
  unit red, and nothing else).
- **Buffer readback returns ZEROES by construction.** `DownloadBufferData` zero-fills
  the caller's destination (it does not leave it untouched — that would hand back
  uninitialised stack that reads like GPU data). Nothing was ever written to a GPU, so
  there is no other honest answer. **Consequence: any test asserting on real buffer
  contents is windowed-only** and must not be added to a headless gate; what IS pinned
  headless is the zero-fill itself (`Flux_BufferReadback.Tests.inl`).
- **`RecordFrame` still runs the pass callbacks.** The one device method that is
  NOT a no-op: `Zenith_Null::RecordFrame` (out-of-line) iterates the queued render
  passes and calls `Flux_RenderGraph::RecordPassInto` into a no-op command buffer.
  No GPU work happens, but the callbacks' side effects (buffer uploads, ECS reads,
  CPU draw-list builds) occur exactly as on Vulkan. This is what lets a full
  gameplay session run.
- **ImGui is REAL here.** `InitialiseImGui` creates a genuine ImGui context, the
  renderer-agnostic GLFW platform backend (`ImGui_ImplGlfw_InitForOther`), and
  **builds the font atlas** — only the *renderer* backend is absent, so draw data
  is composed and then discarded. Without this the editor asserts "No current
  context" on the first widget, and no editor-driven test could run headless.
- **Shaders load reflection only.** `Zenith_Null_Shader::Initialise` deserialises
  the checked-in `<program>.spv.refl` via the Slang-free
  `Flux_ShaderReflection::ReadFromDataStream` so the name-based binder resolves —
  no SPIR-V, no Slang, no GPU module.
- **Dead-strip hazard.** A backend-neutral TU whose only caller lives in
  `Zenith/Vulkan/` gets its whole `.obj` stripped in a Null link, silently taking
  any `ZENITH_TEST` registrars with it (this cost 13 units on
  `Flux_ViewSetBinding.cpp`). Host such tests in an always-linked TU.

## Tools-bake semantics (what a Null build deliberately does NOT do)

A Null build must never author render content — a null backend would bake garbage
over good assets. These stay gated on `Zenith_IsNullRenderer()`:

- terrain chunk/texture bakes and grass map rebuilds (`Zenith_TerrainEditor`;
  `RebuildGrass` returns early because the grass coverage/type/height maps land
  in GPU textures),
- RenderTest's testbed asset generation,
- Zenithmon's terrain bake + Dawnmere scene authoring.

Everything else — including scenes with no terrain dependency — is authored in
every config, so the committed output stays byte-identical across backends.
