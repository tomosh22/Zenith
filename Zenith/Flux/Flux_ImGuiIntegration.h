#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux.h"

//=============================================================================
// Flux ImGui Integration
//
// Platform-agnostic API for registering textures with ImGui.
// Abstracts the underlying graphics API (Vulkan/D3D12) from editor code.
//=============================================================================

// Opaque handle to an ImGui texture registration
// Internally wraps the descriptor set (Vulkan) or SRV heap index (D3D12)
class Flux_ImGuiTextureHandle
{
public:
	Flux_ImGuiTextureHandle() = default;

	void SetValue(u_int64 ulVal) { m_ulHandle = ulVal; }
	u_int64 AsUInt64() const { return m_ulHandle; }
	bool IsValid() const { return m_ulHandle != 0; }
	void Invalidate() { m_ulHandle = 0; }

private:
	u_int64 m_ulHandle = 0;
};

namespace Flux_ImGuiIntegration
{
	/**
	 * Register a texture for use with ImGui::Image()
	 *
	 * @param xSRV Shader resource view of the texture
	 * @param xSampler Sampler to use for texture filtering
	 * @return Handle that can be converted to ImTextureID
	 */
	Flux_ImGuiTextureHandle RegisterTexture(
		const Flux_ShaderResourceView& xSRV,
		const Flux_Sampler& xSampler);

	/**
	 * Unregister a texture from ImGui (deferred deletion)
	 *
	 * @param xHandle Handle returned from RegisterTexture
	 * @param uFramesToWait Number of frames to wait before actual deletion (default: 3)
	 */
	void UnregisterTexture(Flux_ImGuiTextureHandle xHandle, u_int uFramesToWait = 3);

	/**
	 * Process pending texture unregistrations
	 * Call once per frame to clean up deferred deletions
	 */
	void ProcessDeferredUnregistrations();

	/**
	 * Get the ImTextureID for use with ImGui::Image()
	 *
	 * @param xHandle Handle returned from RegisterTexture
	 * @return Pointer suitable for ImGui::Image() / ImTextureID cast
	 */
	void* GetImTextureID(Flux_ImGuiTextureHandle xHandle);
}

#endif // ZENITH_TOOLS
