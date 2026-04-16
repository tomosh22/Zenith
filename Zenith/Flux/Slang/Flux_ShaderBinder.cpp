#include "Zenith.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux.h" // for Flux_ShaderResourceView / Flux_UnorderedAccessView_* field access
#include "Flux/RenderGraph/Flux_RenderGraph.h" // for render-graph bind-time assertions

Flux_ShaderBinder::Flux_ShaderBinder(Flux_CommandList& xCmdList)
	: m_xCmdList(xCmdList)
	, m_uCurrentSet(UINT32_MAX)
{
}

void Flux_ShaderBinder::EnsureSet(u_int uSet)
{
	if (m_uCurrentSet != uSet)
	{
		m_xCmdList.AddCommand<Flux_CommandBeginBind>(uSet);
		m_uCurrentSet = uSet;
	}
}

void Flux_ShaderBinder::BindCBV(Flux_BindingHandle xHandle, const Flux_ConstantBufferView* pxCBV)
{
	if (!xHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_ShaderBinder::BindCBV - Invalid binding handle! GetBinding() failed to find the name.");
		return;
	}

	EnsureSet(xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindCBV>(pxCBV, xHandle.m_uBinding);
}

void Flux_ShaderBinder::BindSRV(Flux_BindingHandle xHandle, const Flux_ShaderResourceView* pxSRV, Flux_Sampler* pxSampler)
{
	if (!xHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_ShaderBinder::BindSRV - Invalid binding handle! GetBinding() failed to find the name.");
		return;
	}

	Zenith_Assert(pxSRV != nullptr, "Flux_ShaderBinder::BindSRV: null SRV pointer");
	// Cross-reference against the current render-graph pass (if we're inside a
	// pfnOnRecord callback). Catches "bound a resource I didn't declare with
	// Read()" — the exact class of bug that leaves the graph unable to emit
	// the correct layout transition.
	Flux_RenderGraph::AssertBoundResourceDeclared(pxSRV->m_xVRAMHandle, /*bIsWrite*/false, "BindSRV");

	EnsureSet(xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindSRV>(pxSRV, xHandle.m_uBinding, pxSampler);
}

void Flux_ShaderBinder::BindUAV_Texture(Flux_BindingHandle xHandle, const Flux_UnorderedAccessView_Texture* pxUAV)
{
	if (!xHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_ShaderBinder::BindUAV_Texture - Invalid binding handle! GetBinding() failed to find the name.");
		return;
	}

	Zenith_Assert(pxUAV != nullptr, "Flux_ShaderBinder::BindUAV_Texture: null UAV pointer");
	Flux_RenderGraph::AssertBoundResourceDeclared(pxUAV->m_xVRAMHandle, /*bIsWrite*/true, "BindUAV_Texture");

	EnsureSet(xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindUAV_Texture>(pxUAV, xHandle.m_uBinding);
}

void Flux_ShaderBinder::BindUAV_Buffer(Flux_BindingHandle xHandle, const Flux_UnorderedAccessView_Buffer* pxUAV)
{
	if (!xHandle.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_ShaderBinder::BindUAV_Buffer - Invalid binding handle! GetBinding() failed to find the name.");
		return;
	}

	Zenith_Assert(pxUAV != nullptr, "Flux_ShaderBinder::BindUAV_Buffer: null UAV pointer");
	Flux_RenderGraph::AssertBoundResourceDeclared(pxUAV->m_xVRAMHandle, /*bIsWrite*/true, "BindUAV_Buffer");

	EnsureSet(xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(pxUAV, xHandle.m_uBinding);
}

void Flux_ShaderBinder::BindDrawConstants(Flux_BindingHandle xScratchBufferBinding, const void* pData, u_int uSize)
{
	if (!xScratchBufferBinding.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_ShaderBinder::BindDrawConstants - Invalid scratch buffer binding handle!");
		return;
	}

	EnsureSet(xScratchBufferBinding.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindDrawConstants>(pData, uSize, xScratchBufferBinding.m_uBinding);
}

void Flux_ShaderBinder::BindDrawConstants(const void* pData, u_int uSize)
{
	// Legacy overload - assumes scratch buffer at set 0, binding 1
	// DEPRECATED: Use the binding handle version for multi-set shaders
	EnsureSet(0);
	m_xCmdList.AddCommand<Flux_CommandBindDrawConstants>(pData, uSize, 1);
}
