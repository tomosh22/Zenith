#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Flux/Flux_ImGuiIntegration.h"

//=============================================================================
// D3D12 null-backend implementation of the Flux ImGui integration.
//
// Editor code (viewport / content browser / material editor / terrain editor)
// registers textures with ImGui through this neutral seam. This backend
// renders nothing, so there is no descriptor set to allocate and nothing to
// defer-delete -- but the symbols MUST exist or every tools-enabled D3D12 build
// fails to link, and the handles MUST look valid or the editor's IsValid()
// guards silently drop the panels they gate.
//
// Same dummy-handle philosophy as the rest of the D3D12 null backend: hand back a
// monotonic non-zero id. ImGui itself never dereferences an ImTextureID -- only
// a rendering backend does, and this one draws nothing.
//=============================================================================

namespace
{
	u_int64 s_ulDummyImGuiTexture = 1;
}

Flux_ImGuiTextureHandle Flux_ImGuiIntegration::RegisterTexture(
	const Flux_ShaderResourceView& xSRV,
	const Flux_Sampler& /*xSampler*/)
{
	Flux_ImGuiTextureHandle xHandle;

	// Mirror the Vulkan implementation's one real precondition, so an editor
	// path that registers an uninitialised view gets the same invalid handle
	// here as it would on the real backend.
	if (!xSRV.m_xImageViewHandle.IsValid())
	{
		return xHandle;
	}

	xHandle.SetValue(s_ulDummyImGuiTexture++);
	return xHandle;
}

void Flux_ImGuiIntegration::UnregisterTexture(Flux_ImGuiTextureHandle /*xHandle*/, u_int /*uFramesToWait*/)
{
	// Nothing was allocated, so nothing is deferred.
}

void Flux_ImGuiIntegration::ProcessDeferredUnregistrations()
{
	// No deferred queue on the null backend.
}

void* Flux_ImGuiIntegration::GetImTextureID(Flux_ImGuiTextureHandle xHandle)
{
	return reinterpret_cast<void*>(xHandle.AsUInt64());
}

#endif // ZENITH_TOOLS
