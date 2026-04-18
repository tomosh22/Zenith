#pragma once

// ============================================================================
// Flux Backend Contract
// ============================================================================
//
// Compile-time contract that any rendering backend must satisfy. Backends are
// selected via macro alias in Zenith_PlatformGraphics_Include.h; each Flux_*
// alias resolves to a concrete backend class (currently Zenith_Vulkan_*).
//
// Adding a second backend (DX12 / Metal / WebGPU) is a matter of providing
// concrete classes that satisfy each of the seven concepts below. The
// conformance file Flux_BackendConformance.cpp instantiates static_assert
// against the active backend; any divergence is a compile-time error rather
// than a silent runtime drift.
//
// The seven concepts (one per responsibility — partitioned for diagnostic
// readability rather than reduced to a single mega-concept):
//
//   - FluxBackendDevice                  Device init / shutdown / VRAM registry
//                                        / bindless / ImGui texture wrapper
//   - FluxBackendMemoryAlloc             VRAM + view + buffer creation, uploads
//   - FluxBackendMemoryDelete            Wrapper destroyers + deferred deletion
//   - FluxBackendCommandRecorder         Per-worker command-buffer recording —
//                                        composed from seven sub-concepts so a
//                                        future backend can satisfy individual
//                                        capabilities without depending on the
//                                        whole surface (see Concept_CommandRecorder.h
//                                        for the sub-concept breakdown).
//   - FluxBackendSync                    Engine-typed barrier emission
//                                        (graph-emitted barriers consumed here)
//   - FluxBackendPresentation            Swapchain + frame index
//   - FluxBackendShader / Builder /      Shader load + reflection, root sig
//     ComputePipelineBuilder /           builder, graphics pipeline builder,
//     RootSigBuilder                     compute pipeline builder
//
// ---- Scope of concept-based type checking --------------------------------
// The concepts below type-check the CURRENT platform's types. The concrete
// input types (Flux_Pipeline, Flux_Sampler, Flux_CommandList, Flux_*Buffer)
// resolve through Zenith_PlatformGraphics_Include.h macros to backend-
// specific definitions. In practice this means:
//
//   - The concept check passes iff the active backend's class has the
//     required method signatures given the current-platform aliases.
//   - Concepts do NOT prove "the backend is portable" — swapping in a
//     second backend requires that backend to alias its own concrete
//     types to the Flux_* names AND the method signatures to match. Both
//     are checked by the conformance file when the second backend's
//     static_assert lines are added.
//
// A second-backend author's checklist (the concepts don't enforce order —
// follow this as prose):
//   1. Provide concrete classes matching each of the seven concept-typed
//      surfaces (FluxBackendDevice → backend's top-level class, etc.).
//   2. Add typedef aliases in Zenith_PlatformGraphics_Include.h under a
//      platform guard (e.g. #ifdef ZENITH_D3D12 using Flux_CommandBuffer =
//      Zenith_D3D12_CommandBuffer;).
//   3. Add static_assert FluxBackend*<backend-class> lines in
//      Flux_BackendConformance.cpp under the same platform guard.
//   4. Build; fix every compile error surfaced from the concept substitution.
//
// ---- Reverse-drift limitation --------------------------------------------
// Concepts check that the backend HAS the required methods; they do NOT
// check that the backend ONLY has the required methods. A backend can grow
// a public method that no concept asks for without failing the conformance
// build. This is intentional — backends can expose platform-specific
// helpers (e.g. GetVkInstance()) that the engine uses through a platform
// guard but that don't belong in the neutral contract. The cost is that
// accidental public-surface expansion slips past the compile check. If
// this becomes painful, a "backend public surface" concept can be added
// that enumerates the FULL expected method list; for now the review
// process catches it.
//
// Operations the backend does NOT need to provide (owned by Flux directly,
// API-neutral):
//   - Render graph                  (Flux_RenderGraph)
//   - Command list DSL              (Flux_CommandList)
//   - Shader reflection             (Flux_ShaderReflection from Slang)
//   - Shader binder                 (Flux_ShaderBinder)
//   - View structs                  (Flux_ShaderResourceView, etc.)
//   - Buffer wrappers               (Flux_VertexBuffer, etc.)
//   - Per-frame ring scheduler      (Flux_PerFrame)

#include "Flux/Backend/Concepts/Flux_Concept_Device.h"
#include "Flux/Backend/Concepts/Flux_Concept_MemoryAlloc.h"
#include "Flux/Backend/Concepts/Flux_Concept_MemoryDelete.h"
#include "Flux/Backend/Concepts/Flux_Concept_CommandRecorder.h"
#include "Flux/Backend/Concepts/Flux_Concept_Sync.h"
#include "Flux/Backend/Concepts/Flux_Concept_Presentation.h"
#include "Flux/Backend/Concepts/Flux_Concept_PipelineConstruction.h"
