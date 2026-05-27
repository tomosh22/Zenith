#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"

// Concept: GPU resource destruction. Wrapper destroyers for each buffer type
// + the deferred-deletion plumbing the engine uses to release VRAM safely
// across the frame-in-flight ring.
//
// All static methods on the backend memory manager. Backends that do not
// support deferred deletion can implement ProcessDeferredDeletions as a
// no-op, but Queue*Deletion must still drain the resource by the time the
// MAX_FRAMES_IN_FLIGHT + 1 frame counter reaches zero.
//
// CONCEPT LIMITATION — pass-by-reference handle invalidation is NOT enforced
// by this concept. A C++20 requires-clause verifies callability of an
// expression, not the argument-binding mode of the parameters. A backend that
// declares `static void QueueVRAMDeletion(Flux_VRAM*, Flux_VRAMHandle xHandle, …)`
// (taking the handle by VALUE) still satisfies this concept — the conformance
// check below cannot distinguish that variant from the by-reference one. The
// "QueueVRAMDeletion auto-invalidates the supplied handle" contract is
// therefore a HUMAN-ENFORCED invariant: backend authors MUST take xHandle by
// non-const reference and clear it before returning. The Vulkan backend
// (Zenith_Vulkan_MemoryManager.cpp) is the reference implementation; mirror
// its signature exactly when adding a second backend.

template <typename T>
concept FluxBackendMemoryDelete = requires(
	T t,
	Flux_VertexBuffer& xVB,
	Flux_DynamicVertexBuffer& xDVB,
	Flux_IndexBuffer& xIB,
	Flux_ConstantBuffer& xCB,
	Flux_DynamicConstantBuffer& xDCB,
	Flux_IndirectBuffer& xIndB,
	Flux_ReadWriteBuffer& xRWB,
	Flux_DynamicReadWriteBuffer& xDRWB,
	Flux_VRAM* pxVRAM,
	Flux_VRAMHandle& xVRAMHandle,
	Flux_ImageViewHandle xImageViewHandle,
	u_int uExtraFrameDelay)
{
	// Wrapper destroyers — each queues VRAM for deletion AND invalidates
	// the buffer's handle to prevent double-free (see Vulkan/CLAUDE.md).
	{ t.DestroyVertexBuffer(xVB)                                              } -> std::same_as<void>;
	{ t.DestroyDynamicVertexBuffer(xDVB)                                      } -> std::same_as<void>;
	{ t.DestroyIndexBuffer(xIB)                                               } -> std::same_as<void>;
	{ t.DestroyConstantBuffer(xCB)                                            } -> std::same_as<void>;
	{ t.DestroyDynamicConstantBuffer(xDCB)                                    } -> std::same_as<void>;
	{ t.DestroyIndirectBuffer(xIndB)                                          } -> std::same_as<void>;
	{ t.DestroyReadWriteBuffer(xRWB)                                          } -> std::same_as<void>;
	{ t.DestroyDynamicReadWriteBuffer(xDRWB)                                  } -> std::same_as<void>;

	// Direct VRAM / view handle deletion. QueueVRAMDeletion auto-invalidates
	// the supplied handle (passed by reference) to prevent double-free on a
	// subsequent destructor pass. Takes four optional image-view handles so
	// render-attachment destruction can free the VRAM plus its RTV/DSV/SRV/UAV
	// in one call, and uExtraFrameDelay for alias-pool-vs-image ordering.
	{ t.QueueVRAMDeletion(pxVRAM, xVRAMHandle,
	                       xImageViewHandle, xImageViewHandle,
	                       xImageViewHandle, xImageViewHandle,
	                       uExtraFrameDelay)                                   } -> std::same_as<void>;
	{ t.QueueImageViewDeletion(xImageViewHandle)                              } -> std::same_as<void>;
	{ t.ProcessDeferredDeletions()                                            } -> std::same_as<void>;
};
