#include "Zenith.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Flux_CommandList.h"

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

	EnsureSet(xHandle.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandBindUAV_Buffer>(pxUAV, xHandle.m_uBinding);
}

void Flux_ShaderBinder::PushConstant(Flux_BindingHandle xScratchBufferBinding, const void* pData, u_int uSize)
{
	// Use the binding handle to determine which set and binding to use
	// This allows scratch buffer to be in the per-draw set, not set 0
	if (!xScratchBufferBinding.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_RENDERER, "Flux_ShaderBinder::PushConstant - Invalid scratch buffer binding handle!");
		return;
	}

	EnsureSet(xScratchBufferBinding.m_uSet);
	m_xCmdList.AddCommand<Flux_CommandPushConstant>(pData, uSize, xScratchBufferBinding.m_uBinding);
}

void Flux_ShaderBinder::PushConstant(const void* pData, u_int uSize)
{
	// Legacy overload - assumes scratch buffer at set 0, binding 1
	// DEPRECATED: Use the binding handle version for multi-set shaders
	EnsureSet(0);
	m_xCmdList.AddCommand<Flux_CommandPushConstant>(pData, uSize, 1);
}
