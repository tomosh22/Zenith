#pragma once

#ifdef ZENITH_TOOLS

#include "Maths/Zenith_Maths.h"
#include "EntityComponent/Zenith_Scene.h"
#include <vector>

class Zenith_Entity;
class Zenith_Scene;

enum class EditorMode
{
	Stopped,
	Playing,
	Paused
};

enum class EditorGizmoMode
{
	Translate,
	Rotate,
	Scale
};

class Zenith_Editor
{
public:
	static void Initialise();
	static void Shutdown();
	static void Update();
	static void Render();

	// Editor state
	static EditorMode GetEditorMode() { return s_eEditorMode; }
	static void SetEditorMode(EditorMode eMode);
	
	// Object selection - now uses EntityID for safer memory management
	static void SelectEntity(Zenith_EntityID uEntityID);
	static void ClearSelection();
	static Zenith_EntityID GetSelectedEntityID() { return s_uSelectedEntityID; }
	static Zenith_Entity* GetSelectedEntity();  // Helper to safely get entity from ID
	static bool HasSelection() { return s_uSelectedEntityID != INVALID_ENTITY_ID; }
	
	// Gizmo
	static EditorGizmoMode GetGizmoMode() { return s_eGizmoMode; }
	static void SetGizmoMode(EditorGizmoMode eMode) { s_eGizmoMode = eMode; }

private:
	static void RenderMainMenuBar();
	static void RenderToolbar();
	static void RenderHierarchyPanel();
	static void RenderPropertiesPanel();
	static void RenderViewport();
	static void HandleObjectPicking();
	static void RenderGizmos();
	static void HandleGizmoInteraction();  // New method for Flux_Gizmos integration
	
	static EditorMode s_eEditorMode;
	static EditorGizmoMode s_eGizmoMode;
	static Zenith_EntityID s_uSelectedEntityID;  // Changed from pointer to ID
	
	// Viewport
	static Zenith_Maths::Vector2 s_xViewportSize;
	static Zenith_Maths::Vector2 s_xViewportPos;
	static bool s_bViewportHovered;
	static bool s_bViewportFocused;
	
	// Scene state backup (for play mode)
	static Zenith_Scene* s_pxBackupScene;

	// Deferred scene operations (to avoid concurrent access during render tasks)
	static bool s_bPendingSceneLoad;
	static std::string s_strPendingSceneLoadPath;
	static bool s_bPendingSceneSave;
	static std::string s_strPendingSceneSavePath;
};

#endif // ZENITH_TOOLS
