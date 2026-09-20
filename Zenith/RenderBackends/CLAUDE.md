# RenderBackends

## Overview

Groups the three concrete Flux render backends under one parent directory:
[Vulkan/](Vulkan/CLAUDE.md) (the real, shipping renderer; Android is
Vulkan-only), [D3D12/](D3D12/CLAUDE.md) (a reserved no-op backend kept as a
link/conformance proof that Flux is genuinely backend-neutral), and
[Null/](Null/CLAUDE.md) (the GPU-less no-op backend that every headless run —
CI batches, unit gates — executes on).

**Exactly one backend compiles per config**, selected by the `RenderBackend`
fragment that prefixes every win64 config name — `Vulkan_`, `Null_`, or
`D3D12_` (see the root `CLAUDE.md` build-configuration table). Sharpmake's
`ConfigureCommonSettings` defines exactly one of `ZENITH_VULKAN` /
`ZENITH_D3D12` / `ZENITH_NULL_RENDERER` per config.

★ **Backend selection is PROJECT MEMBERSHIP PER TARGET, not a per-configuration
source exclude (Phase 4e).** Each backend is its own target-restricted static
library — `ZenithRenderBackendVulkan` (`Sharpmake_ZenithRenderBackendVulkan.cs`,
`RenderBackend.Vulkan` only, win64 **and** agde — agde is always Vulkan),
`ZenithRenderBackendD3D12` (`Sharpmake_ZenithRenderBackendD3D12.cs`,
`RenderBackend.D3D12`, win64 only) and `ZenithRenderBackendNull`
(`Sharpmake_ZenithRenderBackendNull.cs`, `RenderBackend.Null`, win64 only).
Each declares ONLY the `RenderBackend` fragment(s) it supports; wiring into a
solution goes through a `Func<ZenithTarget,bool>` predicate in
`Build/Sharpmake_EngineLibs.cs`. A `Null_*` configuration therefore has no
Vulkan or D3D12 *project* in its solution at all — there is no source file to
exclude, because there is no project membership to begin with. Before this
phase, all three backends were globbed by the single monolithic aggregate
`ZenithProject`, and `ConfigurePlatformExcludes`'s path-segment source-partition
regexes (`.*\Vulkan\.*`, `.*\D3D12\.*`, `.*\Null\.*` — plus a `.*imgui_impl_vulkan.*`
filename exclude, since that TU does not sit under a `\Vulkan\` path segment)
build-excluded the two inactive backends' trees per configuration; that block
is deleted. Each backend library depends on `ZenithFlux` (the direction the
cycle between Flux and the backends is declared —
`Sharpmake_ZenithRenderBackendVulkan.cs`'s header comment has the full
reasoning) and calls only the library-linking helper(s) it actually needs
(`AddVulkanBackendLibs` / `AddD3D12Libs` / neither, for Null — see each
library's own header comment).

This directory is otherwise a pure grouping: it introduces no shared code and
no change to any of the three backends' own internal structure beyond owning
their own build output.
