#pragma once

// ============================================================================
// Flux Backend Contract
// ============================================================================
//
// This header documents the interface that any rendering backend must provide
// for Flux to function. Today the only backend is Vulkan (Zenith/Vulkan/),
// wired via compile-time macro aliases in Zenith_PlatformGraphics_Include.h.
//
// A second backend (e.g., DX12, Metal) would:
//   1. Implement all types and operations listed below
//   2. Provide its own PlatformGraphics include mapping the Flux_* macros
//   3. Compile-time or link-time selection chooses the active backend
//
// This file is documentation, not a compilable interface. It is intentionally
// kept as a reference so a backend author reads one header to understand the
// full contract. Future work may convert this into a struct-of-function-pointers
// or a templated Backend trait for runtime or zero-cost dispatch.
//
// ============================================================================
// BACKEND TYPES (macro-aliased in Zenith_PlatformGraphics_Include.h)
// ============================================================================
//
// Flux_PlatformAPI           -- Main backend singleton (device init, per-frame state,
//                               VRAM registry). Provides static methods:
//                                 ::Initialise(), ::Shutdown()
//                                 ::GetDevice() → native device handle
//                                 ::GetVRAM(Flux_VRAMHandle) → Flux_VRAM*
//                                 ::s_pxCurrentFrame → per-frame state (scratch buffer, pools)
//                                 ::IncrementDescriptorSetAllocations() (debug only)
//
// Flux_MemoryManager         -- GPU resource creation/destruction. Static methods:
//                                 ::CreateRenderTargetVRAM(Flux_SurfaceInfo) → Flux_VRAMHandle
//                                 ::CreateTextureVRAM(Flux_SurfaceInfo, data) → Flux_VRAMHandle
//                                 ::CreateBufferVRAM(...) → Flux_VRAMHandle
//                                 ::CreateShaderResourceView(...) → Flux_ShaderResourceView
//                                 ::CreateUnorderedAccessView(...) → Flux_UnorderedAccessView_*
//                                 ::CreateRenderTargetView(...) → Flux_RenderTargetView
//                                 ::CreateDepthStencilView(...) → Flux_DepthStencilView
//                                 ::QueueVRAMDeletion(...)
//                                 ::UploadBufferData(...)
//                                 ::InitialiseReadWriteBuffer(...)
//                                 ::DestroyVertexBuffer / DestroyIndexBuffer / DestroyReadWriteBuffer
//
// Flux_VRAM                  -- Opaque per-allocation state (image/buffer handle + memory).
//                                 ::GetImage() → native image handle (for barriers)
//
// Flux_CommandBuffer         -- Per-worker command buffer for recording draws/dispatches.
//                                 ::BeginRecording(), ::EndRecording()
//                                 ::SetPipeline(), ::SetVertexBuffer(), ::SetIndexBuffer()
//                                 ::BindSRV(), ::BindCBV(), ::BindUAV_Texture(), ::BindUAV_Buffer()
//                                 ::BindDrawConstants(data, size, binding)
//                                 ::BeginBind(set), ::UseBindlessTextures(set)
//                                 ::Draw(), ::DrawIndexed(), ::DrawIndexedIndirect(), ::DrawIndexedIndirectCount()
//                                 ::Dispatch(x, y, z)
//                                 ::ImageTransitionBarrier(...)
//                                 ::SetShoudClear(bool)
//
// Flux_Swapchain             -- Swapchain lifecycle.
//                                 ::Initialise(), ::Shutdown(), ::Reset()
//                                 ::AcquireNextImage(), ::Present()
//                                 ::GetCurrentFrameIndex() → u_int
//                                 ::GetExtent() → (width, height)
//
// Flux_Shader                -- Compiled shader module + reflection.
//                                 ::Initialise(vertPath, fragPath, ...)
//                                 ::InitialiseCompute(compPath)
//                                 ::InitialiseFromSource(...) (TOOLS only)
//                                 ::GetReflection() → Flux_ShaderReflection&
//
// Flux_Pipeline              -- Compiled pipeline state object (graphics or compute).
//                                 Storage type for both graphics and compute PSOs.
//                                 Contains m_xRootSig for descriptor layout.
//
// Flux_PipelineBuilder       -- Graphics pipeline builder.
//                                 ::FromSpecification(Flux_Pipeline&, Flux_PipelineSpecification&)
//
// Flux_ComputePipelineBuilder -- Compute pipeline builder.
//                                 ::WithShader(), ::WithLayout(), ::Build()
//
// Flux_RootSig               -- Descriptor set / binding group layout for a pipeline.
//                                 Contains m_xLayout (Flux_PipelineLayout) and
//                                 m_axBindingTypes[group][entry] for runtime type queries.
//
// Flux_RootSigBuilder        -- Populates Flux_RootSig from shader reflection.
//                                 ::FromReflection(Flux_RootSig&, Flux_ShaderReflection&)
//
// Flux_Sampler               -- Texture sampler state.
//                                 ::GetSampler() → native sampler handle
//
// ============================================================================
// PER-FRAME STATE
// ============================================================================
//
// Flux_PlatformAPI::s_pxCurrentFrame provides:
//   - Per-worker scratch buffer partitions (see Flux_CommandBindDrawConstants)
//   - AllocateScratchBuffer(size, workerIndex) → offset
//   - GetScratchBufferMappedPtr() → void*
//   - GetScratchBuffer() → native buffer handle
//   - Command pool reset, descriptor pool reset, fence management
//
// ============================================================================
// OPERATIONS NOT IN THE BACKEND (owned by Flux directly)
// ============================================================================
//
// - Render graph (Flux_RenderGraph) — API-neutral pass scheduling
// - Command list DSL (Flux_CommandList) — deferred command recording
// - Shader reflection (Flux_ShaderReflection) — format-neutral binding metadata
// - Shader binder (Flux_ShaderBinder) — reflection-driven binding helpers
// - Material binding (MaterialDrawConstants) — per-draw constant layout
// - View types (Flux_ShaderResourceView, etc.) — opaque handle wrappers
// - Buffer wrappers (Flux_VertexBuffer, etc.) — frame-indexed resource management
//
