#pragma once

#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Zenith_PlatformGraphics_Include.h"

class Flux_CommandList;

// Helper class for binding shader resources using cached Flux_BindingHandle
// This allows binding by name lookup (cached at init time) rather than hardcoded indices
//
// Usage:
//   // At init time, cache binding handles:
//   static Flux_BindingHandle s_xFrameConstantsBinding;
//   s_xFrameConstantsBinding = xShader.GetReflection().GetBinding("FrameConstants");
//
//   // At render time, use cached handles:
//   Flux_ShaderBinder xBinder(g_xCommandList);
//   xBinder.BindCBV(s_xFrameConstantsBinding, &buffer.GetCBV());
//   xBinder.BindSRV(s_xDiffuseTexBinding, &texture.GetSRV());
//
class Flux_ShaderBinder
{
public:
	Flux_ShaderBinder(Flux_CommandList& xCmdList);

	// Bind constant buffer view using cached handle
	void BindCBV(Flux_BindingHandle xHandle, const Flux_ConstantBufferView* pxCBV);

	// Bind shader resource view (texture) using cached handle
	void BindSRV(Flux_BindingHandle xHandle, const Flux_ShaderResourceView* pxSRV, Flux_Sampler* pxSampler = nullptr);

	// Bind unordered access view (texture) using cached handle
	void BindUAV_Texture(Flux_BindingHandle xHandle, const Flux_UnorderedAccessView_Texture* pxUAV);

	// Bind unordered access view (buffer) using cached handle
	void BindUAV_Buffer(Flux_BindingHandle xHandle, const Flux_UnorderedAccessView_Buffer* pxUAV);

	// Push constant data using scratch buffer system
	// Takes a binding handle to determine which set/binding to use (from shader reflection)
	// This allows the scratch buffer to be in the per-draw descriptor set, not set 0
	void PushConstant(Flux_BindingHandle xScratchBufferBinding, const void* pData, u_int uSize);

	// Legacy overload - assumes scratch buffer at set 0, binding 1
	// DEPRECATED: Use the binding handle version for multi-set shaders
	void PushConstant(const void* pData, u_int uSize);

private:
	// Switch to the specified descriptor set if not already active
	void EnsureSet(u_int uSet);

	Flux_CommandList& m_xCmdList;
	u_int m_uCurrentSet = UINT32_MAX;
};
