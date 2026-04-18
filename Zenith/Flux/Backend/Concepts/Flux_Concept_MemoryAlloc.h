#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Zenith_PlatformGraphics_Include.h"

// Concept: GPU resource allocation. VRAM handles, buffers, textures, and
// every kind of view (SRV / UAV / RTV / DSV / CBV).
//
// All static methods on the backend memory manager (aliased as
// Flux_MemoryManager). All inputs and outputs are engine-typed
// (Flux_VRAMHandle, Flux_ShaderResourceView, Flux_VertexBuffer, ...);
// nothing in this concept should require a vk::* parameter or return.
//
// Lifecycle methods (Initialise / Shutdown / BeginFrame / EndFrame) are
// included because EndFrame still does staging-buffer flush + memory
// command-buffer end (deferred deletion has been hoisted to a separate
// Flux_PerFrame end-frame callback).

template <typename T>
concept FluxBackendMemoryAlloc = requires(
	bool b,
	const void* pData,
	size_t sz,
	size_t uDestOffset,
	u_int uInt32Size,
	u_int64 ulPoolSize,
	u_int64 ulPoolAlignment,
	u_int64 ulPoolOffset,
	u_int64& ulOutProbeSize,
	u_int64& ulOutProbeAlignment,
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
	{ T::Initialise()                                                          } -> std::same_as<void>;
	{ T::Shutdown()                                                            } -> std::same_as<void>;
	{ T::BeginFrame()                                                          } -> std::same_as<void>;
	{ T::EndFrame(b)                                                           } -> std::same_as<void>;

	// VRAM allocation. Size parameters are explicitly typed (u_int for the
	// 32-bit buffer path, u_int64 for the aliasing pool path) so the concept
	// actually verifies the backend's signature — narrowing casts here would
	// let an incompatibly-widened or -narrowed backend satisfy the check.
	{ T::CreateRenderTargetVRAM(xInfo)                                         } -> std::same_as<Flux_VRAMHandle>;
	{ T::CreateTextureVRAM(pData, xInfo, b)                                    } -> std::same_as<Flux_VRAMHandle>;
	{ T::CreateBufferVRAM(uInt32Size, eFlags, eResidency)                      } -> std::same_as<Flux_VRAMHandle>;

	// View creation. Each returns the engine-typed view struct that wraps
	// only opaque handles (Flux_ImageViewHandle / Flux_BufferDescriptorHandle).
	{ T::CreateShaderResourceView(xVRAMHandle, xInfo, uMip, uMipCount)         } -> std::same_as<Flux_ShaderResourceView>;
	{ T::CreateUnorderedAccessView(xVRAMHandle, xInfo, uMip)                   } -> std::same_as<Flux_UnorderedAccessView_Texture>;
	{ T::CreateRenderTargetView(xVRAMHandle, xInfo, uMip)                      } -> std::same_as<Flux_RenderTargetView>;
	{ T::CreateRenderTargetViewForLayer(xVRAMHandle, xInfo, uLayer, uMip)      } -> std::same_as<Flux_RenderTargetView>;
	{ T::CreateDepthStencilView(xVRAMHandle, xInfo, uMip)                      } -> std::same_as<Flux_DepthStencilView>;

	// Wrapped buffer initialisers — store data + create the underlying VRAM,
	// returning by reference into the buffer object.
	{ T::InitialiseVertexBuffer(pData, sz, xVB, b)                             } -> std::same_as<void>;
	{ T::InitialiseDynamicVertexBuffer(pData, sz, xDVB, b)                     } -> std::same_as<void>;
	{ T::InitialiseIndexBuffer(pData, sz, xIB)                                 } -> std::same_as<void>;
	{ T::InitialiseConstantBuffer(pData, sz, xCB)                              } -> std::same_as<void>;
	{ T::InitialiseDynamicConstantBuffer(pData, sz, xDCB)                      } -> std::same_as<void>;
	{ T::InitialiseIndirectBuffer(sz, xIndB)                                   } -> std::same_as<void>;
	{ T::InitialiseReadWriteBuffer(pData, sz, xRWB)                            } -> std::same_as<void>;
	{ T::InitialiseDynamicReadWriteBuffer(pData, sz, xDRWB)                    } -> std::same_as<void>;

	// Upload paths. Distinct `sz` (source-size) and `uDestOffset` (destination
	// offset) slots — both are size_t so the compile check passes either way,
	// but naming them separately keeps the concept honest about what each slot
	// means, and matches the backend's parameter names. Semantic drift in the
	// impl (e.g. swapping size/offset) won't be caught by type-check alone, but
	// a reviewer reading this file will now see the intent immediately.
	{ T::UploadBufferData(xVRAMHandle, pData, sz)                                      } -> std::same_as<void>;
	{ T::UploadBufferDataAtOffset(xVRAMHandle, pData, sz, uDestOffset)                 } -> std::same_as<void>;

	// Transient memory aliasing. Backends that don't support aliasing return
	// false from SupportsTransientAliasing() and may leave CreateAliasPoolVRAM
	// / CreateAliasedImageVRAM as stubs returning invalid handles — the
	// render graph treats an invalid pool handle as "fall back to standalone
	// allocation" and the frame proceeds unchanged (just without VRAM savings).
	//
	// Distinct slots for size / alignment / offset / output-probe-size /
	// output-probe-alignment prevent a backend author from accidentally swapping
	// semantically-different same-typed parameters (size vs offset, input vs
	// output). Output-reference slots (ulOutProbeSize / ulOutProbeAlignment)
	// are declared as `u_int64&` in the requires-clause header so
	// ProbeImageMemoryRequirements's non-const-reference signature is exercised
	// rather than accepting any lvalue.
	{ T::SupportsTransientAliasing()                                                   } -> std::same_as<bool>;
	{ T::CreateAliasPoolVRAM(ulPoolSize, ulPoolAlignment)                              } -> std::same_as<Flux_VRAMHandle>;
	{ T::CreateAliasedImageVRAM(xInfo, xVRAMHandle, ulPoolOffset)                      } -> std::same_as<Flux_VRAMHandle>;
	{ T::ProbeImageMemoryRequirements(xInfo, ulOutProbeSize, ulOutProbeAlignment)      } -> std::same_as<void>;
};
