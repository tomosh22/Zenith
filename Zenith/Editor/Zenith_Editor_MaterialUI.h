#pragma once

#ifdef ZENITH_TOOLS

#include "AssetHandling/Zenith_MaterialAsset.h"
#include "Flux/Flux_ImGuiIntegration.h"
#include "Collections/Zenith_HashMap.h"

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

	// Takes a NON-const pointer because it takes a REFERENCE to the asset -- the
	// signature says what it does with it.
	Flux_ImGuiTextureHandle GetOrCreateTexturePreviewHandle(Zenith_TextureAsset* pxTexture);
	void ClearTexturePreviewCache();

	// Drops entries not drawn for a while, releasing both the ImGui descriptor set
	// and the texture pin. Runs at most once per frame from
	// GetOrCreateTexturePreviewHandle; nothing else needs to call it.
	void SweepTexturePreviewCache();

	// PURE: has an entry last drawn on uLastTouched gone stale by uNow? Separated
	// out because the one thing this arithmetic gets silently wrong is the frame
	// counter WRAPPING -- a signed or widened subtraction turns the wrap into a
	// gigantic age and evicts every live entry for one frame, which on a real
	// backend is a descriptor set freed under a panel that is still drawing.
	static bool ShouldEvictPreviewEntry(u_int uNow, u_int uLastTouched);

	// How many entries the cache holds. For units; the panels never ask.
	u_int GetTexturePreviewCacheSize() const { return m_xTexturePreviewCache.GetSize(); }

	//-------------------------------------------------------------------------
	// Material Property Editing
	//-------------------------------------------------------------------------

	void RenderMaterialProperties(Zenith_MaterialAsset* pxMaterial, const char* szIdSuffix = "");

	//-------------------------------------------------------------------------
	// Texture Slot Rendering
	//-------------------------------------------------------------------------

	// Function-pointer callback (engine convention: NO std::function). The
	// context pointer comes first; callers thread arbitrary state through it
	// and cast it back inside the callback body.
	typedef void (*TextureAssignCallback)(void* pContext, const char* szFilePath);

	void RenderTextureSlot(
		const char* szLabel,
		Zenith_MaterialAsset& xMaterial,
		TextureSlotType eSlot,
		float fPreviewSize = 48.0f,
		TextureAssignCallback pfnOnAssign = nullptr,
		void* pOnAssignContext = nullptr);

	void RenderAllTextureSlots(Zenith_MaterialAsset& xMaterial, bool bShowPreview = true);

	//-------------------------------------------------------------------------
	// Helpers
	//-------------------------------------------------------------------------

	std::string GetTexturePathForSlot(const Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot);
	void SetTexturePathForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot, const std::string& strPath);
	Zenith_TextureAsset* GetTextureForSlot(Zenith_MaterialAsset& xMaterial, TextureSlotType eSlot);

	//-------------------------------------------------------------------------
	// Data members (was Zenith_EditorMaterialUI)
	//-------------------------------------------------------------------------

	// ★ EVERY ENTRY OWNS ITS TEXTURE, AND IS KEYED ON THE PINNED POINTER.
	// RegisterTexture bakes the texture's raw image view into an ImGui descriptor
	// set once and never revalidates it, so an unpinned entry is a dangling
	// descriptor the moment UnloadUnused() collects the asset -- and because
	// Flux_VRAMHandle / Flux_ImageViewHandle are bare LIFO free-list indices with
	// NO generation counter, the slot comes back on the next allocation and the
	// thumbnail silently draws whatever took it.
	//
	// This used to key on m_xVRAMHandle.AsUInt() and hold no reference, which made
	// the RECYCLED index the identity: free texture A, load texture B, and B lands
	// on A's VRAM and view indices, matches A's cached entry on both, and is handed
	// A's descriptor set. Pinning fixes the key as a side effect -- a live asset's
	// address cannot be reused -- so the two defects have one fix.
	struct TexturePreviewCacheEntry
	{
		TextureHandle m_xTexture;         // OWNING — keeps the image view alive under the descriptor set
		Flux_ImGuiTextureHandle m_xHandle;
		u_int64 m_ulImageViewHandle = 0;  // re-register if the texture reloads in place
		u_int m_uLastTouchedFrame = 0;    // eviction: see SweepTexturePreviewCache
	};

	// Keyed on the PINNED Zenith_TextureAsset*, not on any recyclable handle.
	Zenith_HashMap<u_int64, TexturePreviewCacheEntry> m_xTexturePreviewCache;
	u_int m_uTexturePreviewSweepFrame = 0;
};

#endif // ZENITH_TOOLS
