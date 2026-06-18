#pragma once

// Shared, backend-neutral validation for direct command-buffer recording.
//
// These asserts previously lived in the Flux_CommandList command constructors,
// which compiled in EVERY backend config — so they fired on the Vulkan backend
// AND the D3D12 null backend. With the command-list DSL removed and render
// systems recording the backend command buffer directly, the same checks now run
// at the top of the backend recorder methods, sourced from these shared inline
// helpers so the assert text lives in one place.
//
// The D3D12 null recorder (whose methods were otherwise no-ops with NO validation)
// calls the full set, restoring the cross-backend parity the constructors gave.
// The Vulkan recorder already carries its own equivalent-or-stronger,
// backend-specific asserts on the view/buffer binds (e.g. image-view-handle
// validity), so it only calls the helpers where it had a GAP — pipeline null and
// draw-constant size.
//
// SEAM RULE: this header includes ONLY Flux_Types.h (+ Flux_Fwd.h), which the
// D3D12 command-buffer header already pulls. It must NOT include Flux_Buffers.h
// — that re-enters the Flux.h include cycle the null-backend headers are kept out
// of (see D3D12/CLAUDE.md). Consequently the helpers here validate the view /
// pipeline / draw-constant inputs (whose full types live in Flux_Types.h), which
// is the bulk of the recorder surface. The three buffer-WRAPPER VRAM-validity
// checks (vertex / index / indirect — whose full types live in the seam-bound
// Flux_Buffers.h) stay inline in the Vulkan recorder methods: they are the
// "forgot to upload the mesh to the GPU?" diagnostics that matter on the real
// backend, and they are vacuous on the null backend (its buffers carry dummy
// always-valid handles), so nothing meaningful is lost there.

#include "Flux/Flux_Fwd.h"     // Flux_Pipeline alias (backend class, forward-declared)
#include "Flux/Flux_Types.h"   // Flux_ShaderResourceView(_Buffer) / Flux_UnorderedAccessView_* / Flux_ConstantBufferView

inline void FluxAssertPipeline(const Flux_Pipeline* pxPipeline, const char* szCall)
{
	Zenith_Assert(pxPipeline != nullptr, "%s: pipeline is null", szCall);
}

inline void FluxAssertValidSRV(const Flux_ShaderResourceView* pxSRV)
{
	Zenith_Assert(pxSRV != nullptr, "BindSRV: SRV is null");
	Zenith_Assert(pxSRV->m_xVRAMHandle.IsValid(), "BindSRV: SRV has invalid VRAM handle");
}

inline void FluxAssertValidSRVBuffer(const Flux_ShaderResourceView_Buffer& xSRV)
{
	Zenith_Assert(xSRV.m_xVRAMHandle.IsValid(), "BindSRV_Buffer: SRV has invalid VRAM handle");
	Zenith_Assert(xSRV.m_xBufferDescHandle.IsValid(), "BindSRV_Buffer: SRV has invalid descriptor handle");
}

inline void FluxAssertValidUAVTexture(const Flux_UnorderedAccessView_Texture* pxUAV)
{
	Zenith_Assert(pxUAV != nullptr, "BindUAV_Texture: UAV is null");
	Zenith_Assert(pxUAV->m_xVRAMHandle.IsValid(), "BindUAV_Texture: UAV has invalid VRAM handle");
}

inline void FluxAssertValidUAVBuffer(const Flux_UnorderedAccessView_Buffer* pxUAV)
{
	Zenith_Assert(pxUAV != nullptr, "BindUAV_Buffer: UAV is null");
	Zenith_Assert(pxUAV->m_xVRAMHandle.IsValid(), "BindUAV_Buffer: UAV has invalid VRAM handle");
}

inline void FluxAssertValidCBV(const Flux_ConstantBufferView* pxCBV)
{
	Zenith_Assert(pxCBV != nullptr, "BindCBV: CBV is null");
	Zenith_Assert(pxCBV->m_xVRAMHandle.IsValid(), "BindCBV: CBV has invalid VRAM handle");
}

inline void FluxAssertDrawConstantsSize(size_t uSize, size_t uMax)
{
	Zenith_Assert(uSize <= uMax, "BindDrawConstants: payload too big (%zu > %zu)", uSize, uMax);
}
