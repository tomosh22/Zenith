#pragma once

#include "Flux/Slang/Flux_SlangCompiler.h"
#include "Flux/Flux_BackendTypes.h"

// Helper class for binding shader resources via typed binding handles. Each Bind*
// takes a generated `Flux_BindingHandle` (set / binding / kind / count, emitted into
// Flux/Shaders/Generated/<Feature>.h by the codegen) — there is no name lookup and no
// runtime name cache (both were removed in the binding-model overhaul). A debug
// resource-kind assertion catches Bind* mix-ups (e.g. BindCBV on a handle reflection
// says is a texture) at the call site, which is far clearer than the eventual Vulkan
// validation-layer error.
//
// Usage:
//   Flux_ShaderBinder xBinder(*pxCmdBuf);
//   namespace DS = Flux_Generated_DeferredShading::DeferredShading;
//   xBinder.BindCBV(DS::hg_xView, &buffer.GetCBV());
//   xBinder.BindSRV(DS::hg_xDiffuseTex, &texture.GetSRV());
//
//   namespace SR = Flux_Generated_SSR::SSR_DenoiseH;
//   xBinder.BindDrawConstants(SR::hPushConstants, &xData, sizeof(xData));
//
// (The BindDrawConstants example comes from SSR rather than DeferredShading
// because the scratch-UBO slot is a per-program binding: DeferredShading's is
// hDeferredShadingConstants, and only some programs spell theirs
// "pushConstants". Read the generated header for the program you are binding —
// do not assume a handle name.)
class Flux_ShaderBinder
{
public:
	Flux_ShaderBinder(Flux_CommandBuffer& xCmdBuf);

	// Each takes a generated Flux_BindingHandle (set + binding + kind + count,
	// emitted per binding into Flux/Shaders/Generated/<Subsystem>.h). The kind
	// carried by the handle is asserted against the overload, so a Bind* mix-up
	// still fails loudly at the call site — without any reflection lookup or a
	// name cache.
	void BindCBV          (const Flux_BindingHandle& xHandle, const Flux_ConstantBufferView*          pxCBV);
	void BindSRV          (const Flux_BindingHandle& xHandle, const Flux_ShaderResourceView*          pxSRV, Flux_Sampler* pxSampler = nullptr);
	void BindUAV_Texture  (const Flux_BindingHandle& xHandle, const Flux_UnorderedAccessView_Texture* pxUAV);
	void BindUAV_Buffer   (const Flux_BindingHandle& xHandle, const Flux_UnorderedAccessView_Buffer*  pxUAV);
	void BindSRV_Buffer   (const Flux_BindingHandle& xHandle, const Flux_ShaderResourceView_Buffer&   xSRV);
	void BindDrawConstants(const Flux_BindingHandle& xHandle, const void* pData, u_int uSize);

private:
	// Builds the Flux_BindingSlot for a resolved (set, binding), flagging it to
	// reset the group when the descriptor set changes from the previous bind --
	// the slot-carried replacement for the old EnsureSet/BeginBind clear.
	Flux_BindingSlot MakeSlot(u_int uSet, u_int uBinding);

	Flux_CommandBuffer& m_xCmdBuf;
	u_int m_uCurrentSet = UINT32_MAX;
};
