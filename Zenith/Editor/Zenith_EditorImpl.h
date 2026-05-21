#pragma once

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Editor/Panels/Zenith_EditorPanel_Viewport.h"   // PendingImGuiTextureDeletion
#include "Flux/Flux_Types.h"                              // Flux_ImGuiTextureHandle, Flux_ImageViewHandle
#include <bitset>
#include <string>
#include <unordered_set>
#include <vector>

// Phase 5.5c: per-Engine editor state. The 36 class-static data members
// on Zenith_Editor + the 10 camera class-statics defined in
// Zenith_EditorCamera.cpp + the 3 file-statics for ImGui texture caching
// move here. Static facade keeps its method surface; method bodies and
// the 42 external readers reach state via g_xEngine.Editor().m_xXxx.
//
// Pre-existing s_xEditorState (Zenith_EditorState aggregate) is included
// as a sub-member -- the codebase was mid-migration into it for some
// subsystems. Keeps the existing 72 s_xEditorState.m_X call sites working
// via the same field layout.
//
// Only compiled under ZENITH_TOOLS -- non-tools builds don't link the
// editor and don't pay the engine member.
class Zenith_EditorImpl
{
public:
	Zenith_EditorImpl() = default;
	~Zenith_EditorImpl() = default;

	Zenith_EditorImpl(const Zenith_EditorImpl&) = delete;
	Zenith_EditorImpl& operator=(const Zenith_EditorImpl&) = delete;

	// Mode flags.
	EditorMode      m_eEditorMode = EditorMode::Stopped;
	EditorGizmoMode m_eGizmoMode  = EditorGizmoMode::Translate;

	// Multi-select state.
	std::unordered_set<Zenith_EntityID> m_xSelectedEntityIDs;
	Zenith_EntityID m_uPrimarySelectedEntityID = INVALID_ENTITY_ID;
	Zenith_EntityID m_uLastClickedEntityID     = INVALID_ENTITY_ID;

	// Viewport state.
	Zenith_Maths::Vector2 m_xViewportSize = { 1280, 720 };
	Zenith_Maths::Vector2 m_xViewportPos  = { 0, 0 };
	bool                  m_bViewportHovered = false;
	bool                  m_bViewportFocused = false;

	// Aggregate state container (mid-migration target -- pre-existing).
	Zenith_EditorState    m_xEditorState;

	// Content Browser state.
	std::string                       m_strCurrentDirectory;
	std::vector<ContentBrowserEntry>  m_xDirectoryContents;
	std::vector<ContentBrowserEntry>  m_xFilteredContents;
	bool                              m_bDirectoryNeedsRefresh = true;
	char                              m_szSearchBuffer[256]    = "";
	int                               m_iAssetTypeFilter       = 0;
	int                               m_iSelectedContentIndex  = -1;
	float                             m_fThumbnailSize         = 80.0f;
	std::vector<std::string>          m_axNavigationHistory;
	int                               m_iHistoryIndex          = -1;
	ContentBrowserViewMode            m_eViewMode              = ContentBrowserViewMode::Grid;

	// Console state.
	std::vector<ConsoleLogEntry>      m_xConsoleLogs;
	bool                              m_bConsoleAutoScroll     = true;
	bool                              m_bShowConsoleInfo       = true;
	bool                              m_bShowConsoleWarnings   = true;
	bool                              m_bShowConsoleErrors     = true;
	std::bitset<LOG_CATEGORY_COUNT>   m_xCategoryFilters       = std::bitset<LOG_CATEGORY_COUNT>().set();

	// Panel visibility (View menu toggles).
	bool                              m_bShowHierarchyPanel    = true;
	bool                              m_bShowPropertiesPanel   = true;
	bool                              m_bShowConsolePanel      = true;

	// Material Editor state.
	Zenith_MaterialAsset*             m_pxSelectedMaterial     = nullptr;
	bool                              m_bShowMaterialEditor    = true;

	// Editor camera state. Defaults match the constants previously hard-
	// coded in Zenith_EditorCamera.cpp (xINITIAL_EDITOR_CAMERA_*).
	Zenith_Maths::Vector3             m_xEditorCameraPosition  = { 0, 100, 0 };
	double                            m_fEditorCameraPitch     = 0.0;
	double                            m_fEditorCameraYaw       = 0.0;
	float                             m_fEditorCameraFOV       = 45.0f;
	float                             m_fEditorCameraNear      = 1.0f;
	float                             m_fEditorCameraFar       = 2000.0f;
	Zenith_EntityID                   m_uGameCameraEntity      = INVALID_ENTITY_ID;
	float                             m_fEditorCameraMoveSpeed = 50.0f;
	float                             m_fEditorCameraRotateSpeed = 0.1f;
	bool                              m_bEditorCameraInitialized = false;

	// ImGui texture handle caching for the game viewport (was file-static
	// in Zenith_Editor.cpp).
	Flux_ImGuiTextureHandle           m_xCachedGameTextureHandle;
	Flux_ImageViewHandle              m_xCachedImageViewHandle;

	// Deferred-deletion queue for ImGui textures (GPU must finish before
	// freeing). Was file-static in Zenith_Editor.cpp.
	std::vector<PendingImGuiTextureDeletion> m_xPendingDeletions;
};

#endif // ZENITH_TOOLS
