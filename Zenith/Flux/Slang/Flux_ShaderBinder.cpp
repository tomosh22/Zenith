#include "Zenith.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux.h" // for Flux_ShaderResourceView / Flux_UnorderedAccessView_* field access
#include "Flux/RenderGraph/Flux_RenderGraph.h" // for render-graph bind-time assertions

Flux_ShaderBinder::Flux_ShaderBinder(Flux_CommandBuffer& xCmdBuf)
	: m_xCmdBuf(xCmdBuf)
	, m_uCurrentSet(UINT32_MAX)
{
}

Flux_BindingSlot Flux_ShaderBinder::MakeSlot(u_int uSet, u_int uBinding)
{
	// Reset the group on a descriptor-set change (the first bind to the set),
	// mirroring the old EnsureSet -> BeginBind(set) clear. Subsequent binds to
	// the same set keep the group's accumulated bindings (multi-draw / partial
	// re-bind), and a group a later set does not touch survives untouched.
	const bool bReset = (m_uCurrentSet != uSet);
	m_uCurrentSet = uSet;
	return Flux_BindingSlot{ uSet, uBinding, bReset };
}

// ============================================================================
// Typed-handle overloads. The handle carries (set, binding, kind, count) from
// codegen, so binding needs no reflection lookup and no name cache. The kind is
// asserted against the overload to keep the "wrong Bind* for this binding"
// safety net; the render-graph declaration cross-check is preserved.
// ============================================================================
void Flux_ShaderBinder::BindCBV(const Flux_BindingHandle& xHandle, const Flux_ConstantBufferView* pxCBV)
{
	Zenith_Assert(FluxKindIsUniformBuffer(xHandle.m_eKind),
		"Flux_ShaderBinder::BindCBV: handle kind %d is not a constant buffer. Wrong Bind* overload?",
		static_cast<int>(xHandle.m_eKind));
	m_xCmdBuf.BindCBV(pxCBV, MakeSlot(xHandle.m_uSet, xHandle.m_uBinding));
}

void Flux_ShaderBinder::BindSRV(const Flux_BindingHandle& xHandle, const Flux_ShaderResourceView* pxSRV, Flux_Sampler* pxSampler)
{
	Zenith_Assert(FluxKindIsSampledTexture(xHandle.m_eKind),
		"Flux_ShaderBinder::BindSRV: handle kind %d is not a sampled texture. Wrong Bind* overload?",
		static_cast<int>(xHandle.m_eKind));
	Zenith_Assert(pxSRV != nullptr, "Flux_ShaderBinder::BindSRV: null SRV pointer (set=%u binding=%u)", xHandle.m_uSet, xHandle.m_uBinding);
	Flux_RenderGraph::AssertBoundResourceDeclared(pxSRV->m_xVRAMHandle, /*bIsWrite*/false, "BindSRV");
	m_xCmdBuf.BindSRV(pxSRV, MakeSlot(xHandle.m_uSet, xHandle.m_uBinding), pxSampler);
}

void Flux_ShaderBinder::BindUAV_Texture(const Flux_BindingHandle& xHandle, const Flux_UnorderedAccessView_Texture* pxUAV)
{
	Zenith_Assert(FluxKindIsStorageImage(xHandle.m_eKind),
		"Flux_ShaderBinder::BindUAV_Texture: handle kind %d is not a storage image. Wrong Bind* overload?",
		static_cast<int>(xHandle.m_eKind));
	Zenith_Assert(pxUAV != nullptr, "Flux_ShaderBinder::BindUAV_Texture: null UAV pointer (set=%u binding=%u)", xHandle.m_uSet, xHandle.m_uBinding);
	Flux_RenderGraph::AssertBoundResourceDeclared(pxUAV->m_xVRAMHandle, /*bIsWrite*/true, "BindUAV_Texture");
	m_xCmdBuf.BindUAV_Texture(pxUAV, MakeSlot(xHandle.m_uSet, xHandle.m_uBinding));
}

void Flux_ShaderBinder::BindUAV_Buffer(const Flux_BindingHandle& xHandle, const Flux_UnorderedAccessView_Buffer* pxUAV)
{
	Zenith_Assert(FluxKindIsStorageBuffer(xHandle.m_eKind),
		"Flux_ShaderBinder::BindUAV_Buffer: handle kind %d is not a storage buffer. Wrong Bind* overload?",
		static_cast<int>(xHandle.m_eKind));
	Zenith_Assert(pxUAV != nullptr, "Flux_ShaderBinder::BindUAV_Buffer: null UAV pointer (set=%u binding=%u)", xHandle.m_uSet, xHandle.m_uBinding);
	Flux_RenderGraph::AssertBoundResourceDeclared(pxUAV->m_xVRAMHandle, /*bIsWrite*/true, "BindUAV_Buffer");
	m_xCmdBuf.BindUAV_Buffer(pxUAV, MakeSlot(xHandle.m_uSet, xHandle.m_uBinding));
}

void Flux_ShaderBinder::BindSRV_Buffer(const Flux_BindingHandle& xHandle, const Flux_ShaderResourceView_Buffer& xSRV)
{
	Zenith_Assert(FluxKindIsStorageBuffer(xHandle.m_eKind),
		"Flux_ShaderBinder::BindSRV_Buffer: handle kind %d is not a storage buffer. Wrong Bind* overload?",
		static_cast<int>(xHandle.m_eKind));
	Zenith_Assert(xSRV.m_xVRAMHandle.IsValid(), "Flux_ShaderBinder::BindSRV_Buffer: invalid SRV VRAM handle (set=%u binding=%u)", xHandle.m_uSet, xHandle.m_uBinding);
	Zenith_Assert(xSRV.m_xBufferDescHandle.IsValid(), "Flux_ShaderBinder::BindSRV_Buffer: invalid SRV descriptor handle (set=%u binding=%u)", xHandle.m_uSet, xHandle.m_uBinding);
	Flux_RenderGraph::AssertBoundResourceDeclared(xSRV.m_xVRAMHandle, /*bIsWrite*/false, "BindSRV_Buffer");
	m_xCmdBuf.BindSRV_Buffer(xSRV, MakeSlot(xHandle.m_uSet, xHandle.m_uBinding));
}

void Flux_ShaderBinder::BindDrawConstants(const Flux_BindingHandle& xHandle, const void* pData, u_int uSize)
{
	Zenith_Assert(FluxKindIsUniformBuffer(xHandle.m_eKind),
		"Flux_ShaderBinder::BindDrawConstants: handle kind %d is not a constant buffer. Wrong Bind* overload?",
		static_cast<int>(xHandle.m_eKind));
	m_xCmdBuf.BindDrawConstants(pData, uSize, MakeSlot(xHandle.m_uSet, xHandle.m_uBinding));
}
