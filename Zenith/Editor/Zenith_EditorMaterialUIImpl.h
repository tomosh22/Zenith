#pragma once

#ifdef ZENITH_TOOLS

#include "Flux/Flux_Types.h"
#include <unordered_map>

// Phase 5.5d: per-Engine state for the editor's material UI -- a small
// texture-preview cache keyed by VRAM handle. Was an anon-namespace
// static in Zenith_Editor_MaterialUI.cpp.
class Zenith_EditorMaterialUIImpl
{
public:
	Zenith_EditorMaterialUIImpl() = default;
	~Zenith_EditorMaterialUIImpl() = default;

	Zenith_EditorMaterialUIImpl(const Zenith_EditorMaterialUIImpl&) = delete;
	Zenith_EditorMaterialUIImpl& operator=(const Zenith_EditorMaterialUIImpl&) = delete;

	struct TexturePreviewCacheEntry
	{
		Flux_ImGuiTextureHandle m_xHandle;
		u_int64 m_ulImageViewHandle = 0;  // Cached to detect changes
	};

	std::unordered_map<u_int64, TexturePreviewCacheEntry> m_xTexturePreviewCache;
};

#endif // ZENITH_TOOLS
