#include "Zenith.h"
#include "Core/Zenith_Engine.h"

// Play-mode transitions + deferred scene-restore, split out of Zenith_Editor.cpp.
// All methods are Zenith_Editor members; calls to sibling editor methods
// (ClearSelection / SwitchToEditorCamera / EnterPlayMode / EnterStopMode) resolve
// via Zenith_Editor.h. See Editor/CLAUDE.md "Editor Modes" + "Deferred Operations".
#ifdef ZENITH_TOOLS

#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorState.h"
#include "Editor/Zenith_EditorSceneAccess.h"
#include "Editor/Zenith_UndoSystem.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_ComponentMeta.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "Core/Zenith_CommandLine.h"
#include "FileAccess/Zenith_FileAccess.h"
#include "Flux/Flux_RendererImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_BackendTypes.h"

#include <filesystem>

// STOPPED -> PLAYING: backup scene state, locate the game camera, then dispatch
// Unity-style OnAwake / OnEnable / OnStart for every active entity (in three
// passes — OnAwake first across all, then OnEnable + MarkAwoken, then OnStart
// re-fetching the active list since OnAwake may have created entities).
// Returns false if no active scene data is loaded — caller must restore the
// prior mode.
bool Zenith_Editor::EnterPlayMode()
{
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Entering Play Mode");

	m_xEditorState.m_xPlayBackup.m_strBackupScenePath = std::filesystem::temp_directory_path().string() + "/zenith_scene_backup" ZENITH_SCENE_EXT;

	// Persistent entities only — transient entities have runtime-only resources
	// (procedural meshes) that can't serialise, and behaviour scripts will
	// regenerate them in OnStart (running below). Including them would
	// duplicate after OnStart re-creates them on restore.
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: No active scene data, cannot enter Play Mode");
		return false;
	}

	Zenith_EditorSceneAccess::SaveToFile(pxSceneData, m_xEditorState.m_xPlayBackup.m_strBackupScenePath, false);
	m_xEditorState.m_xPlayBackup.m_bHasBackup = true;
	m_xEditorState.m_xPlayBackup.m_iBackupSceneHandle = xActiveScene.GetHandle();
	const Zenith_SceneInfo xSceneInfo = g_xEngine.Scenes().GetSceneInfo(xActiveScene);
	m_xEditorState.m_xPlayBackup.m_strBackupSceneName = xSceneInfo.m_strName;
	m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath = xSceneInfo.m_strPath;
	m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex = xSceneInfo.m_iBuildIndex;

	Zenith_Log(LOG_CATEGORY_EDITOR, "Scene state backed up to: %s", m_xEditorState.m_xPlayBackup.m_strBackupScenePath.c_str());

	m_xEditorState.m_xCamera.m_uGameCameraEntity = pxSceneData->GetMainCameraEntity();
	if (m_xEditorState.m_xCamera.m_uGameCameraEntity == INVALID_ENTITY_ID)
	{
		Zenith_Vector<Zenith_CameraComponent*> xCameras;
		Zenith_EditorSceneAccess::GetAllOfComponentType<Zenith_CameraComponent>(pxSceneData, xCameras);

		for (Zenith_Vector<Zenith_CameraComponent*>::Iterator xIt(xCameras); !xIt.Done(); xIt.Next())
		{
			Zenith_CameraComponent* pxCam = xIt.GetData();
			Zenith_Entity* pxEntity = &pxCam->GetParentEntity();
			m_xEditorState.m_xCamera.m_uGameCameraEntity = pxEntity->GetEntityID();
			Zenith_EditorSceneAccess::SetMainCameraEntity(pxSceneData, m_xEditorState.m_xCamera.m_uGameCameraEntity);
			break;
		}
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Dispatching OnAwake/OnEnable for %u entities", Zenith_EditorSceneAccess::GetEntityCount(pxSceneData));
	Zenith_ComponentMetaRegistry& xRegistry = Zenith_ComponentMetaRegistry::Get();

	const Zenith_Vector<Zenith_EntityID>& xEntityIDs = pxSceneData->GetActiveEntities();
	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (pxSceneData->EntityExists(uID))
		{
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			xRegistry.DispatchOnAwake(xEntity);
		}
	}

	for (u_int u = 0; u < xEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xEntityIDs.Get(u);
		if (pxSceneData->EntityExists(uID))
		{
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			if (xEntity.IsEnabled())
			{
				xRegistry.DispatchOnEnable(xEntity);
			}
			pxSceneData->MarkEntityAwoken(uID);
		}
	}

	// Third pass: OnStart for enabled entities (Unity-style: called before first
	// Update). Re-fetch the active entity list since OnAwake/OnEnable may have
	// created new entities.
	const Zenith_Vector<Zenith_EntityID>& xStartEntityIDs = pxSceneData->GetActiveEntities();
	for (u_int u = 0; u < xStartEntityIDs.GetSize(); ++u)
	{
		Zenith_EntityID uID = xStartEntityIDs.Get(u);
		if (pxSceneData->EntityExists(uID))
		{
			Zenith_Entity xEntity = pxSceneData->GetEntity(uID);
			if (xEntity.IsEnabled())
			{
				xRegistry.DispatchOnStart(xEntity);
			}
			Zenith_EditorSceneAccess::Editor_MarkEntityStarted(pxSceneData, uID);
		}
	}
	return true;
}

// PLAYING/PAUSED -> STOPPED: queue a deferred scene-restore from backup.
// CRITICAL: must defer to next frame's Update(); loading mid-frame would have
// SubmitRenderTasks try to render new terrain components before render systems
// have registered them.
void Zenith_Editor::EnterStopMode()
{
	Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Stopping Play Mode");

	if (m_xEditorState.m_xPlayBackup.m_bHasBackup && !m_xEditorState.m_xPlayBackup.m_strBackupScenePath.empty())
	{
		m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad = true;
		m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath = m_xEditorState.m_xPlayBackup.m_strBackupScenePath;
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene restore queued for next frame: %s", m_xEditorState.m_xPlayBackup.m_strBackupScenePath.c_str());
		// m_xEditorState.m_xPlayBackup.m_bHasBackup / m_xEditorState.m_xPlayBackup.m_strBackupScenePath cleared in Update() after the load completes.
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Warning: No scene backup available to restore");
	}

	// Clear the game camera reference since scene will be reloaded.
	m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;
}

void Zenith_Editor::SetEditorMode(EditorMode eMode)
{
	if (m_xEditorState.m_eEditorMode == eMode) return;

	EditorMode oldMode = m_xEditorState.m_eEditorMode;
	m_xEditorState.m_eEditorMode = eMode;

	if (oldMode == EditorMode::Stopped && eMode == EditorMode::Playing)
	{
		// Dispatcher owns the mode revert on failure so EnterPlayMode is a
		// pure transition routine — single source of truth for mode state.
		if (!EnterPlayMode())
		{
			m_xEditorState.m_eEditorMode = oldMode;
			return;
		}
	}
	else if (oldMode != EditorMode::Stopped && eMode == EditorMode::Stopped)
	{
		EnterStopMode();
	}
	else if (eMode == EditorMode::Paused)
	{
		// Stay on game camera during pause so player can see game state.
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Pausing - physics and scene updates suspended");
	}
	else if (oldMode == EditorMode::Paused && eMode == EditorMode::Playing)
	{
		Zenith_Log(LOG_CATEGORY_EDITOR, "Editor: Resuming - physics and scene updates resumed");
	}
}

void Zenith_Editor::RequestLoadRegisteredScene(int iBuildIndex)
{
	m_xEditorState.m_xDeferredOps.m_bPendingRegisteredSceneLoad = true;
	m_xEditorState.m_xDeferredOps.m_iPendingRegisteredSceneBuildIndex = iBuildIndex;
}

void Zenith_Editor::RequestLoadSceneFromFile(const std::string& strPath)
{
	m_xEditorState.m_xDeferredOps.m_bPendingSceneLoadFromFile = true;
	m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadFromFilePath = strPath;
}

// Synchronously flush the staging buffer, wait for GPU idle, drain deferred
// deletions, and clear pending command lists. Required before any operation
// that destroys GPU resources still in flight (scene reset, scene load).
void Zenith_Editor::WaitForGPUAndFlushDeferred(const char* szReason)
{
	// Headless mode (Zenith_CommandLine::IsHeadless()): Flux::EarlyInitialise is
	// skipped, so command buffers / allocator / device are never created. Every
	// call below would assert. The semantics ("ensure GPU is idle before
	// destroying resources") collapse to a no-op when there is no GPU.
	if (Zenith_CommandLine::IsHeadless())
	{
		return;
	}

	Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Flushing staging buffer...");
	g_xEngine.FluxMemory().Flush();

	Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Waiting for GPU idle before %s...", szReason);
	g_xEngine.FluxBackend().WaitForGPUIdle();

	for (u_int u = 0; u < MAX_FRAMES_IN_FLIGHT; u++)
	{
		g_xEngine.FluxMemory().ProcessDeferredDeletions();
	}
	g_xEngine.FluxRenderer().ClearPendingRenderPasses();
}

// Pending scene load: flush GPU; if this is the editor's stop-mode backup
// restore, force-unload every non-persistent scene and reset the persistent
// scene's entities (game's SCENE_LOAD_SINGLE may have destroyed the original
// scene during play, leaving a stale backup handle); then create a fresh
// scene and load the file into it. Also handles plain (non-backup) loads.
void Zenith_Editor::HandlePendingSceneLoadDeferred()
{
	if (!m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad) return;
	m_xEditorState.m_xDeferredOps.m_bPendingSceneLoad = false;

	WaitForGPUAndFlushDeferred("scene load");

	const bool bIsBackupRestore = m_xEditorState.m_xPlayBackup.m_bHasBackup && m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath == m_xEditorState.m_xPlayBackup.m_strBackupScenePath;

	if (bIsBackupRestore)
	{
		// 1. Reset all Flux render systems BEFORE destroying entities.
		g_xEngine.Scenes().ResetAllRenderSystems();

		Zenith_Scene xPersistentScene = g_xEngine.Scenes().GetPersistentScene();

		// 2. Force-unload all non-persistent scenes. UnloadSceneForced bypasses
		// the "last scene" guard — after SCENE_LOAD_SINGLE during play only one
		// game scene remains and UnloadScene would silently refuse to unload it.
		Zenith_Vector<Zenith_Scene> axScenesToUnload;
		// GetSceneAt returns INVALID_SCENE past the last visible scene, so walk
		// slot order until that sentinel (was bounded by GetLoadedSceneCount).
		for (uint32_t i = 0; ; ++i)
		{
			Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
			if (!xScene.IsValid()) break;
			if (xScene == xPersistentScene) continue;
			axScenesToUnload.PushBack(xScene);
		}
		for (u_int i = 0; i < axScenesToUnload.GetSize(); ++i)
		{
			g_xEngine.Scenes().UnloadSceneForced(axScenesToUnload.Get(i));
		}

		// 3. Reset persistent scene entities (clears component pools).
		Zenith_SceneData* pxPersistentData = g_xEngine.Scenes().GetSceneData(xPersistentScene);
		if (pxPersistentData)
		{
			pxPersistentData->Reset();
		}

		// 4. Create a fresh scene with the original name and restore metadata.
		Zenith_Scene xRestoredScene = g_xEngine.Scenes().LoadScene(m_xEditorState.m_xPlayBackup.m_strBackupSceneName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(xRestoredScene);

		Zenith_SceneData* pxRestoredData = g_xEngine.Scenes().GetSceneData(xRestoredScene);
		if (pxRestoredData)
		{
			Zenith_EditorSceneAccess::Editor_SetPath(pxRestoredData, m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath);
			Zenith_EditorSceneAccess::Editor_SetBuildIndex(pxRestoredData, m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex);
		}
	}

	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (pxSceneData)
	{
		Zenith_EditorSceneAccess::LoadFromFile(pxSceneData, m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath);
	}
	Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Scene loaded from %s", m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath.c_str());

	ClearSelection();
	g_xEngine.UndoSystem().Clear();
	m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;

	if (bIsBackupRestore)
	{
		std::filesystem::remove(m_xEditorState.m_xPlayBackup.m_strBackupScenePath);
		m_xEditorState.m_xPlayBackup.m_bHasBackup = false;
		m_xEditorState.m_xPlayBackup.m_strBackupScenePath = "";
		m_xEditorState.m_xPlayBackup.m_iBackupSceneHandle = -1;
		m_xEditorState.m_xPlayBackup.m_strBackupSceneName = "";
		m_xEditorState.m_xPlayBackup.m_strBackupOriginalPath = "";
		m_xEditorState.m_xPlayBackup.m_iBackupBuildIndex = -1;
		Zenith_Log(LOG_CATEGORY_EDITOR, "[FlushPending] Backup scene file cleaned up");
	}

	if (m_xEditorState.m_xCamera.m_bInitialized)
	{
		SwitchToEditorCamera();
	}

	m_xEditorState.m_xDeferredOps.m_strPendingSceneLoadPath.clear();
}

void Zenith_Editor::FlushPendingSceneOperations()
{
	HandlePendingSceneLoadDeferred();
}

#endif // ZENITH_TOOLS
