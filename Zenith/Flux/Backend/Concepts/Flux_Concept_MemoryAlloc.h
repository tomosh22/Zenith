#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

// Concept: GPU resource allocation. VRAM handles, buffers, textures, and
// every kind of view (SRV / UAV / RTV / DSV / CBV).
//
// All static methods on the backend memory manager (aliased as
// Flux_MemoryManager). All inputs and outputs are engine-typed
// (Flux_VRAMHandle, Flux_ShaderResourceView, Flux_VertexBuffer, ...);
// nothing in this concept should require a vk::* parameter or return.
//
// Lifecycle methods: Initialise / Shutdown, plus two drain points. Memory
// operations are ad-hoc — callable at any time with no frame bracket; the
// backend opens its internal command machinery lazily. Flush() is the
// synchronous ad-hoc drain (submit pending uploads, CPU-wait);
// SubmitFrameMemoryWork() is the renderer's once-per-frame deferred handoff
// (pending memory work submitted ahead of render work). Deferred deletion is
// driven by a separate Flux_PerFrame end-frame callback.

template <typename T>
concept FluxBackendMemoryAlloc = requires(
	T t,
	bool b,
	const void* pData,
	size_t sz,
	size_t uDestOffset,
	u_int uInt32Size,
	u_int uMemoryFlags,
	const Flux_SurfaceInfo& xInfo,
	Flux_VRAMHandle xVRAMHandle,
	uint32_t uMip,
	uint32_t uLayer,
	uint32_t uMipCount,
	MemoryFlags eFlags,
	MemoryResidency eResidency,
	Flux_VertexBuffer& xVB,
	Flux_DynamicVertexBuffer& xDVB,
	Flux_IndexBuffer& xIB,
	Flux_ConstantBuffer& xCB,
	Flux_DynamicConstantBuffer& xDCB,
	Flux_IndirectBuffer& xIndB,
	Flux_ReadWriteBuffer& xRWB,
	Flux_DynamicReadWriteBuffer& xDRWB)
{
	// Lifecycle
	{ t.Initialise()                                                          } -> std::same_as<void>;
	{ t.Shutdown()                                                            } -> std::same_as<void>;
	{ t.Flush()                                                               } -> std::same_as<void>;
	{ t.SubmitFrameMemoryWork()                                               } -> std::same_as<void>;

	// VRAM allocation. Size parameters are explicitly typed (u_int for the
	// 32-bit buffer path, u_int64 for the aliasing pool path) so the concept
	// actually verifies the backend's signature — narrowing casts here would
	// let an incompatibly-widened or -narrowed backend satisfy the check.
	{ t.CreateRenderTargetVRAM(xInfo)                                         } -> std::same_as<Flux_VRAMHandle>;
	{ t.CreateTextureVRAM(pData, xInfo, b)                                    } -> std::same_as<Flux_VRAMHandle>;
	{ t.CreateBufferVRAM(uInt32Size, eFlags, eResidency)                      } -> std::same_as<Flux_VRAMHandle>;

	// View creation. Each returns the engine-typed view struct that wraps
	// only opaque handles (Flux_ImageViewHandle / Flux_BufferDescriptorHandle).
	{ t.CreateShaderResourceView(xVRAMHandle, xInfo, uMip, uMipCount)         } -> std::same_as<Flux_ShaderResourceView>;
	{ t.CreateUnorderedAccessView(xVRAMHandle, xInfo, uMip)                   } -> std::same_as<Flux_UnorderedAccessView_Texture>;
	{ t.CreateRenderTargetView(xVRAMHandle, xInfo, uMip)                      } -> std::same_as<Flux_RenderTargetView>;
	{ t.CreateRenderTargetViewForLayer(xVRAMHandle, xInfo, uLayer, uMip)      } -> std::same_as<Flux_RenderTargetView>;
	{ t.CreateDepthStencilView(xVRAMHandle, xInfo, uMip)                      } -> std::same_as<Flux_DepthStencilView>;

	// Wrapped buffer initialisers — store data + create the underlying VRAM,
	// returning by reference into the buffer object.
	{ t.InitialiseVertexBuffer(pData, sz, xVB, b)                             } -> std::same_as<void>;
	{ t.InitialiseDynamicVertexBuffer(pData, sz, xDVB, b)                     } -> std::same_as<void>;
	{ t.InitialiseIndexBuffer(pData, sz, xIB)                                 } -> std::same_as<void>;
	{ t.InitialiseConstantBuffer(pData, sz, xCB)                              } -> std::same_as<void>;
	{ t.InitialiseDynamicConstantBuffer(pData, sz, xDCB)                      } -> std::same_as<void>;
	{ t.InitialiseIndirectBuffer(sz, xIndB)                                   } -> std::same_as<void>;
	{ t.InitialiseReadWriteBuffer(pData, sz, xRWB)                            } -> std::same_as<void>;
	{ t.InitialiseDynamicReadWriteBuffer(pData, sz, xDRWB)                    } -> std::same_as<void>;

	// Upload paths. Distinct `sz` (source-size) and `uDestOffset` (destination
	// offset) slots — both are size_t so the compile check passes either way,
	// but naming them separately keeps the concept honest about what each slot
	// means, and matches the backend's parameter names. Semantic drift in the
	// impl (e.g. swapping size/offset) won't be caught by type-check alone, but
	// a reviewer reading this file will now see the intent immediately.
	{ t.UploadBufferData(xVRAMHandle, pData, sz)                                      } -> std::same_as<void>;
	{ t.UploadBufferDataAtOffset(xVRAMHandle, pData, sz, uDestOffset)                 } -> std::same_as<void>;
};

// OPTIONAL concept: transient-memory aliasing. Split out of FluxBackendMemoryAlloc
// so a backend that doesn't implement real aliasing can opt out of this slice
// while still satisfying the core allocator. Such a backend (e.g. the D3D12 null
// stub) ships these as stubs — SupportsTransientAliasing() returns false and the
// Create*/Probe methods return invalid handles / no-op — and the render graph,
// gated on SupportsTransientAliasing() (Flux_RenderGraph.h), falls back to
// standalone allocation, so the frame proceeds unchanged (just without VRAM
// savings). Today every shipping backend provides the stubs, so conformance
// asserts this as a dual positive (static_assert per backend), not an absence.
//
// Distinct slots for size / alignment / offset / output-probe-size /
// output-probe-alignment prevent accidentally swapping semantically-different
// same-typed parameters; the output-reference slots are `u_int64&` so the
// non-const-reference signature of ProbeImageMemoryRequirements is exercised.
template <typename T>
concept FluxBackendTransientAliasing = requires(
	T t,
	u_int64 ulPoolSize,
	u_int64 ulPoolAlignment,
	u_int64 ulPoolOffset,
	u_int64& ulOutProbeSize,
	u_int64& ulOutProbeAlignment,
	const Flux_SurfaceInfo& xInfo,
	Flux_VRAMHandle xVRAMHandle)
{
	{ t.SupportsTransientAliasing()                                                   } -> std::same_as<bool>;
	{ t.CreateAliasPoolVRAM(ulPoolSize, ulPoolAlignment)                              } -> std::same_as<Flux_VRAMHandle>;
	{ t.CreateAliasedImageVRAM(xInfo, xVRAMHandle, ulPoolOffset)                      } -> std::same_as<Flux_VRAMHandle>;
	{ t.ProbeImageMemoryRequirements(xInfo, ulOutProbeSize, ulOutProbeAlignment)      } -> std::same_as<void>;
};
