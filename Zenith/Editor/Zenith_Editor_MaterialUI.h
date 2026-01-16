#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux_MaterialAsset.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include <functional>

//=============================================================================
// Shared Material UI Utilities
//
// Common UI components for editing materials across the editor:
// - Material Editor panel
// - Model Component properties
// - Terrain Component properties
//=============================================================================

namespace Zenith_Editor_MaterialUI
{
	//-------------------------------------------------------------------------
	// Texture Slot Types
	//-------------------------------------------------------------------------
	enum TextureSlotType
	{
		TEXTURE_SLOT_DIFFUSE,
		TEXTURE_SLOT_NORMAL,
		TEXTURE_SLOT_ROUGHNESS_METALLIC,
		TEXTURE_SLOT_OCCLUSION,
		TEXTURE_SLOT_EMISSIVE
	};

	//-------------------------------------------------------------------------
	// Texture Preview Cache Management
	//-------------------------------------------------------------------------

	/**
	 * Get or create an ImGui texture handle for previewing a Flux_Texture.
	 * Handles caching to avoid re-registration.
	 * @param pxTexture The texture to get a preview handle for
	 * @return Valid ImGui texture handle, or invalid handle if texture is null
	 */
	Flux_ImGuiTextureHandle GetOrCreateTexturePreviewHandle(const Flux_Texture* pxTexture);

	/**
	 * Clear the texture preview cache.
	 * Call this when textures are being unloaded.
	 */
	void ClearTexturePreviewCache();

	//-------------------------------------------------------------------------
	// Material Property Editing
	//-------------------------------------------------------------------------

	/**
	 * Render full material properties panel (base color, metallic, roughness,
	 * emissive, transparency, UV, rendering flags).
	 * @param pxMaterial The material to edit
	 * @param szIdSuffix Optional suffix for ImGui IDs to avoid conflicts
	 */
	void RenderMaterialProperties(Flux_MaterialAsset* pxMaterial, const char* szIdSuffix = "");

	//-------------------------------------------------------------------------
	// Texture Slot Rendering
	//-------------------------------------------------------------------------

	/**
	 * Callback type for custom texture assignment behavior.
	 * Called when a texture is dropped on a slot.
	 * @param szFilePath Path to the dropped texture file
	 */
	using TextureAssignCallback = std::function<void(const char* szFilePath)>;

	/**
	 * Render a texture slot with drag-drop support and optional preview.
	 * @param szLabel Label for the texture slot
	 * @param xMaterial Material to modify
	 * @param eSlot Which texture slot to edit
	 * @param bShowPreview Whether to show texture preview (default true)
	 * @param fPreviewSize Size of the preview image (default 48.0f)
	 * @param pfnOnAssign Optional callback for custom assignment behavior.
	 *                    If null, uses default SetTexturePathForSlot behavior.
	 */
	void RenderTextureSlot(
		const char* szLabel,
		Flux_MaterialAsset& xMaterial,
		TextureSlotType eSlot,
		bool bShowPreview = true,
		float fPreviewSize = 48.0f,
		TextureAssignCallback pfnOnAssign = nullptr);

	/**
	 * Render all texture slots for a material.
	 * @param xMaterial Material to edit
	 * @param bShowPreview Whether to show texture previews
	 */
	void RenderAllTextureSlots(Flux_MaterialAsset& xMaterial, bool bShowPreview = true);

	//-------------------------------------------------------------------------
	// Helpers
	//-------------------------------------------------------------------------

	/**
	 * Get the current texture path from a material for a given slot type.
	 */
	std::string GetTexturePathForSlot(const Flux_MaterialAsset& xMaterial, TextureSlotType eSlot);

	/**
	 * Set a texture path on a material for a given slot type.
	 */
	void SetTexturePathForSlot(Flux_MaterialAsset& xMaterial, TextureSlotType eSlot, const std::string& strPath);

	/**
	 * Get the Flux_Texture pointer for a given slot type.
	 */
	const Flux_Texture* GetTextureForSlot(Flux_MaterialAsset& xMaterial, TextureSlotType eSlot);
}

#endif // ZENITH_TOOLS
