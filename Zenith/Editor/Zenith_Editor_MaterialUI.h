#pragma once

#ifdef ZENITH_TOOLS

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include <functional>
#include <unordered_map>

//=============================================================================
// Shared Material UI Utilities
//
// Common UI components for editing materials across the editor:
// - Material Editor panel
// - Model Component properties
// - Terrain Component properties
//=============================================================================

// Per-Engine state + behaviour for the Material UI subsystem. Replaces the
// `namespace Zenith_Editor_MaterialUI` facade (deleted) and the data-only
// `Zenith_EditorMaterialUI` (folded in here). Accessed via
// g_xEngine.EditorMaterialUI(). Class name `Zenith_EditorMaterialUI` matches
// the engine-member naming convention (no underscore), per Q-9 of the
// drop-impl plan.
class Zenith_EditorMaterialUI
{
public:
	Zenith_EditorMaterialUI() = default;
	~Zenith_EditorMaterialUI() = default;
	Zenith_EditorMaterialUI(const Zenith_EditorMaterialUI&) = delete;
	Zenith_EditorMaterialUI& operator=(const Zenith_EditorMaterialUI&) = delete;

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

	Flux_ImGuiTextureHandle GetOrCreateTexturePreviewHandle(const Zenith_TextureAsset* pxTexture);
	void ClearTexturePreviewCache();

	//-------------------------------------------------------------------------
	// Material Property Editing
	//-------------------------------------------------------------------------

	void RenderMaterialProperties(Zenith_MaterialAsset* pxMaterial, const char* szIdSuffix = "");

	//-------------------------------------------------------------------------
	// Texture Slot Rendering
	//-------------------------------------------------------------------------

	using TextureAssignCallback = std::function<void(const char* szFilePath)>;

	void RenderTextureSlot(
		const char* szLabel,
		Zenith_MaterialAsset& xMaterial,
		TextureSlotType eSlot,
		float fPreviewSize = 48.0f,
		TextureAssignCallback pfnOnAssign = nullptr);

	void RenderAllTextureSlots(Zenith_MaterialAsset& xMaterial, bool bShowPreview = true);

	//-------------------------------------------------------------------------
	// Helpers
	//-------------------------------------------------------------------------

	std::string GetTexturePathForSlot(const Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot);
	void SetTexturePathForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot, const std::string& strPath);
	const Zenith_TextureAsset* GetTextureForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot);

	//-------------------------------------------------------------------------
	// Data members (was Zenith_EditorMaterialUI)
	//-------------------------------------------------------------------------

	struct TexturePreviewCacheEntry
	{
		Flux_ImGuiTextureHandle m_xHandle;
		u_int64 m_ulImageViewHandle = 0;  // Cached to detect changes
	};

	std::unordered_map<u_int64, TexturePreviewCacheEntry> m_xTexturePreviewCache;
};

#endif // ZENITH_TOOLS
