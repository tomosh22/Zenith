#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "Editor/Zenith_EditorActions.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_EditorCommands.h"
#include "Editor/Zenith_EditorSceneAccess.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/Zenith_SelectionSystem.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_LightComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Windows/Zenith_Windows_Window.h"

// Windows file dialog helpers (defined in Zenith_Editor.cpp)
#ifdef _WIN32
std::string ShowOpenFileDialog(const char* szFilter, const char* szDefaultExt);
std::string ShowSaveFileDialog(const char* szFilter, const char* szDefaultExt, const char* szDefaultFilename);
#endif

namespace
{
	constexpr const char* szSCENE_FILE_FILTER = "Zenith Scene Files (*" ZENITH_SCENE_EXT ")\0*" ZENITH_SCENE_EXT "\0All Files (*.*)\0*.*\0";

	// The selection as a list of ROOT entities: an entity whose ancestor is also
	// selected is dropped, because deleting/duplicating the ancestor already
	// covers it (and acting on both would double up).
	void CollectSelectedRoots(Zenith_Vector<Zenith_EntityID>& axOut)
	{
		axOut.Clear();
		Zenith_Editor& xEditor = g_xEngine.Editor();
		const std::unordered_set<Zenith_EntityID>& xSelected = xEditor.GetSelectedEntityIDs();
		for (const Zenith_EntityID& xID : xSelected)
		{
			Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xID);
			if (!xEntity.IsValid())
			{
				continue;
			}
			bool bAncestorSelected = false;
			for (const Zenith_EntityID& xOther : xSelected)
			{
				if (xOther != xID && xEntity.IsDescendantOf(xOther))
				{
					bAncestorSelected = true;
					break;
				}
			}
			if (!bAncestorSelected)
			{
				axOut.PushBack(xID);
			}
		}
	}

	// "Lamp" -> "Lamp (1)", "Lamp (1)" -> "Lamp (2)".
	std::string NextCopyName(const std::string& strName)
	{
		const size_t uOpen = strName.rfind(" (");
		if (uOpen != std::string::npos && strName.back() == ')')
		{
			const std::string strNumber = strName.substr(uOpen + 2, strName.size() - uOpen - 3);
			if (!strNumber.empty() && strNumber.find_first_not_of("0123456789") == std::string::npos)
			{
				return strName.substr(0, uOpen) + " (" + std::to_string(atoi(strNumber.c_str()) + 1) + ")";
			}
		}
		return strName + " (1)";
	}

	void AddKindComponents(Zenith_Entity& xEntity, Zenith_EditorActions::CreateKind eKind)
	{
		using Zenith_EditorActions::CreateKind;
		switch (eKind)
		{
		case CreateKind::Camera:
		{
			xEntity.AddComponent<Zenith_CameraComponent>();
			Zenith_SceneData* pxSceneData = xEntity.GetSceneData();
			if (pxSceneData != nullptr && !pxSceneData->GetMainCameraEntity().IsValid())
			{
				Zenith_EditorSceneAccess::SetMainCameraEntity(pxSceneData, xEntity.GetEntityID());
			}
			break;
		}
		case CreateKind::PointLight:
			xEntity.AddComponent<Zenith_LightComponent>().SetLightType(LIGHT_TYPE_POINT);
			break;
		case CreateKind::SpotLight:
			xEntity.AddComponent<Zenith_LightComponent>().SetLightType(LIGHT_TYPE_SPOT);
			break;
		case CreateKind::DirectionalLight:
			xEntity.AddComponent<Zenith_LightComponent>().SetLightType(LIGHT_TYPE_DIRECTIONAL);
			break;
		case CreateKind::Empty:
		default:
			break;
		}
	}

	const Zenith_ComponentEditorRegistryEntry* FindRegistryEntry(const char* szDisplayName)
	{
		const Zenith_Vector<Zenith_ComponentEditorRegistryEntry>& xEntries = Zenith_ComponentEditorRegistry::Get().GetEntries();
		for (u_int u = 0; u < xEntries.GetSize(); ++u)
		{
			if (xEntries.Get(u).m_strDisplayName == szDisplayName)
			{
				return &xEntries.Get(u);
			}
		}
		return nullptr;
	}

	// Editor display names and meta-registry type names are the same strings
	// for every engine component (both registrations live side by side in
	// Zenith_ComponentMeta_Registration.cpp), which is what lets the payload
	// command address a component by the name the inspector shows.
	const char* MetaTypeNameForDisplayName(const char* szDisplayName)
	{
		return szDisplayName;
	}

	bool RunPromptOrProceed(Zenith_EditorPromptState::Action eAction, const std::string& strPath)
	{
		if (!Zenith_EditorActions::HasUnsavedChanges())
		{
			return true;
		}
		Zenith_EditorPromptState& xPrompt = g_xEngine.Editor().m_xEditorState.m_xPrompt;
		xPrompt.m_eAction = eAction;
		xPrompt.m_strPath = strPath;
		xPrompt.m_bOpenRequested = true;
		return false;
	}

	void RememberRecentScene(const std::string& strPath)
	{
		Zenith_EditorPrefs& xPrefs = g_xEngine.Editor().m_xEditorState.m_xPrefs;
		xPrefs.AddRecentScene(strPath);
		xPrefs.Save();
	}
}

namespace Zenith_EditorActions
{

//=============================================================================
// Entities
//=============================================================================

const char* GetCreateKindName(CreateKind eKind)
{
	switch (eKind)
	{
	case CreateKind::Camera:           return "Camera";
	case CreateKind::PointLight:       return "Point Light";
	case CreateKind::SpotLight:        return "Spot Light";
	case CreateKind::DirectionalLight: return "Directional Light";
	case CreateKind::Empty:
	default:                           return "Entity";
	}
}

Zenith_EntityID CreateEntity(CreateKind eKind, Zenith_EntityID xParent)
{
	Zenith_Editor& xEditor = g_xEngine.Editor();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (pxSceneData == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_EDITOR, "Create %s: no active scene", GetCreateKindName(eKind));
		return INVALID_ENTITY_ID;
	}

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, GetCreateKindName(eKind));
	xEntity.SetTransient(false);
	AddKindComponents(xEntity, eKind);

	// Place it where the user is looking, not at the origin they may be 2 km from.
	Zenith_Entity xParentEntity = g_xEngine.Scenes().ResolveEntity(xParent);
	if (xParentEntity.IsValid())
	{
		xEntity.SetParent(xParent);
	}
	else if (Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>())
	{
		pxTransform->SetPosition(xEditor.GetCameraPlacementPoint());
	}

	Zenith_EditorEntitySnapshot xSnapshot;
	xSnapshot.Capture(xEntity);
	const std::string strDescription = std::string("Create ") + GetCreateKindName(eKind);
	Zenith_UndoCommand_EntityLifetime* pxCommand = Zenith_UndoCommand_EntityLifetime::MakeCreate(xSnapshot, xParent, strDescription.c_str());
	if (pxCommand != nullptr)
	{
		pxCommand->SetCurrentEntityID(xEntity.GetEntityID());
		g_xEngine.UndoSystem().Record(pxCommand);
	}

	Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
	xEditor.SelectEntity(xEntity.GetEntityID());
	xEditor.m_xEditorState.m_xHierarchy.m_xScrollTo = xEntity.GetEntityID();
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Created %s (ID: %u)", GetCreateKindName(eKind), xEntity.GetEntityID().m_uIndex);
	return xEntity.GetEntityID();
}

u_int DeleteSelection()
{
	Zenith_Vector<Zenith_EntityID> axRoots;
	CollectSelectedRoots(axRoots);
	if (axRoots.GetSize() == 0)
	{
		return 0;
	}

	Zenith_UndoCommand_Composite* pxComposite = new Zenith_UndoCommand_Composite(
		axRoots.GetSize() == 1 ? "Delete Entity" : "Delete Entities");
	for (u_int u = 0; u < axRoots.GetSize(); ++u)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(axRoots.Get(u));
		if (Zenith_UndoCommand_EntityLifetime* pxDelete = Zenith_UndoCommand_EntityLifetime::MakeDelete(xEntity))
		{
			pxComposite->Add(pxDelete);
		}
	}
	const u_int uCount = pxComposite->GetCount();
	g_xEngine.Editor().ClearSelection();
	g_xEngine.UndoSystem().Execute(pxComposite);
	return uCount;
}

u_int DuplicateSelection()
{
	Zenith_Vector<Zenith_EntityID> axRoots;
	CollectSelectedRoots(axRoots);
	if (axRoots.GetSize() == 0)
	{
		return 0;
	}

	Zenith_Editor& xEditor = g_xEngine.Editor();
	Zenith_UndoCommand_Composite* pxComposite = new Zenith_UndoCommand_Composite(
		axRoots.GetSize() == 1 ? "Duplicate Entity" : "Duplicate Entities");
	Zenith_Vector<Zenith_UndoCommand_EntityLifetime*> axCreates;
	for (u_int u = 0; u < axRoots.GetSize(); ++u)
	{
		Zenith_Entity xSource = g_xEngine.Scenes().ResolveEntity(axRoots.Get(u));
		Zenith_EditorEntitySnapshot xSnapshot;
		if (!xSnapshot.Capture(xSource))
		{
			continue;
		}
		xSnapshot.SetRootName(NextCopyName(xSnapshot.GetRootName()));
		const std::string strDescription = "Duplicate '" + xSource.GetName() + "'";
		if (Zenith_UndoCommand_EntityLifetime* pxCreate = Zenith_UndoCommand_EntityLifetime::MakeCreate(
			xSnapshot, xSnapshot.GetCapturedRootParent(), strDescription.c_str()))
		{
			pxComposite->Add(pxCreate);
			axCreates.PushBack(pxCreate);
		}
	}

	xEditor.ClearSelection();
	g_xEngine.UndoSystem().Execute(pxComposite);

	// Execute selected each copy in turn; make the whole set the selection.
	for (u_int u = 0; u < axCreates.GetSize(); ++u)
	{
		const Zenith_EntityID xCopy = axCreates.Get(u)->GetCurrentEntityID();
		if (xCopy.IsValid())
		{
			xEditor.SelectEntity(xCopy, true);
		}
	}
	return axCreates.GetSize();
}

void RenameEntity(Zenith_EntityID xEntityID, const std::string& strNewName)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	if (!xEntity.IsValid() || xEntity.GetName() == strNewName || strNewName.empty())
	{
		return;
	}
	Zenith_UndoCommand_EntityState::State xOld = Zenith_UndoCommand_EntityState::CaptureState(xEntity);
	Zenith_UndoCommand_EntityState::State xNew = xOld;
	xNew.m_strName = strNewName;
	const std::string strDescription = "Rename '" + xOld.m_strName + "'";
	g_xEngine.UndoSystem().Execute(new Zenith_UndoCommand_EntityState(xEntityID, xOld, xNew, strDescription.c_str()));
}

void SetEntityEnabled(Zenith_EntityID xEntityID, bool bEnabled)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	if (!xEntity.IsValid() || xEntity.IsEnabled() == bEnabled)
	{
		return;
	}
	Zenith_UndoCommand_EntityState::State xOld = Zenith_UndoCommand_EntityState::CaptureState(xEntity);
	Zenith_UndoCommand_EntityState::State xNew = xOld;
	xNew.m_bEnabled = bEnabled;
	g_xEngine.UndoSystem().Execute(new Zenith_UndoCommand_EntityState(xEntityID, xOld, xNew,
		bEnabled ? "Enable Entity" : "Disable Entity"));
}

bool ReparentEntity(Zenith_EntityID xEntityID, Zenith_EntityID xNewParent)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	if (!xEntity.IsValid() || xEntityID == xNewParent || xEntity.GetParentEntityID() == xNewParent)
	{
		return false;
	}
	if (xNewParent.IsValid())
	{
		Zenith_Entity xParent = g_xEngine.Scenes().ResolveEntity(xNewParent);
		// No cycles, and no cross-scene parents.
		if (!xParent.IsValid() || xParent.IsDescendantOf(xEntityID) || xParent.GetScene() != xEntity.GetScene())
		{
			return false;
		}
	}
	Zenith_UndoCommand_EntityState::State xOld = Zenith_UndoCommand_EntityState::CaptureState(xEntity);
	Zenith_UndoCommand_EntityState::State xNew = xOld;
	xNew.m_xParent = xNewParent;
	g_xEngine.UndoSystem().Execute(new Zenith_UndoCommand_EntityState(xEntityID, xOld, xNew,
		xNewParent.IsValid() ? "Reparent Entity" : "Unparent Entity"));
	return true;
}

bool AddComponent(Zenith_EntityID xEntityID, const char* szDisplayName)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	const Zenith_ComponentEditorRegistryEntry* pxEntry = FindRegistryEntry(szDisplayName);
	if (!xEntity.IsValid() || pxEntry == nullptr || pxEntry->m_pfnHasComponent(xEntity))
	{
		return false;
	}
	if (!pxEntry->m_pfnAddComponent(xEntity))
	{
		return false;
	}
	// Recorded AFTER the add (the registry's own add path is the one every
	// authoring step uses); the command holds the fresh payload for redo.
	const char* szTypeName = MetaTypeNameForDisplayName(szDisplayName);
	Zenith_Vector<uint8_t> axAbsent;
	Zenith_Vector<uint8_t> axAdded;
	Zenith_UndoCommand_ComponentBytes::CaptureComponent(xEntity, szTypeName, axAdded);
	const std::string strDescription = std::string("Add ") + szDisplayName;
	g_xEngine.UndoSystem().Record(new Zenith_UndoCommand_ComponentBytes(xEntityID, szTypeName, axAbsent, axAdded, strDescription.c_str()));
	MarkSceneDirtyForEntity(xEntityID);
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Added component '%s' to entity %u", szDisplayName, xEntityID.m_uIndex);
	return true;
}

bool RemoveComponent(Zenith_EntityID xEntityID, const char* szDisplayName)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().ResolveEntity(xEntityID);
	const Zenith_ComponentEditorRegistryEntry* pxEntry = FindRegistryEntry(szDisplayName);
	if (!xEntity.IsValid() || pxEntry == nullptr || !pxEntry->m_pfnHasComponent(xEntity) || pxEntry->m_pfnRemoveComponent == nullptr)
	{
		return false;
	}
	const char* szTypeName = MetaTypeNameForDisplayName(szDisplayName);
	Zenith_Vector<uint8_t> axBefore;
	Zenith_Vector<uint8_t> axAbsent;
	Zenith_UndoCommand_ComponentBytes::CaptureComponent(xEntity, szTypeName, axBefore);
	const std::string strDescription = std::string("Remove ") + szDisplayName;
	// Execute performs the removal through the payload command so undo/redo and
	// the live action are the same code path.
	g_xEngine.UndoSystem().Execute(new Zenith_UndoCommand_ComponentBytes(xEntityID, szTypeName, axBefore, axAbsent, strDescription.c_str()));
	Zenith_Log(LOG_CATEGORY_EDITOR, "[EditorOp] Removed component '%s' from entity %u", szDisplayName, xEntityID.m_uIndex);
	return true;
}

void SelectAll()
{
	Zenith_Editor& xEditor = g_xEngine.Editor();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
	if (pxSceneData == nullptr)
	{
		return;
	}
	xEditor.ClearSelection();
	const Zenith_Vector<Zenith_EntityID>& xEntities = pxSceneData->GetActiveEntities();
	for (u_int u = 0; u < xEntities.GetSize(); ++u)
	{
		xEditor.SelectEntity(xEntities.Get(u), true);
	}
}

void DeselectAll()
{
	g_xEngine.Editor().ClearSelection();
}

void FocusSelection()
{
	g_xEngine.Editor().FocusCameraOnSelection();
}

//=============================================================================
// Scenes
//=============================================================================

bool HasUnsavedChanges()
{
	for (uint32_t i = 0; ; ++i)
	{
		Zenith_Scene xScene = g_xEngine.Scenes().GetSceneAt(i);
		if (!xScene.IsValid())
		{
			break;
		}
		if (g_xEngine.Scenes().GetSceneInfo(xScene).m_bHasUnsavedChanges)
		{
			return true;
		}
	}
	return false;
}

std::string GetActiveSceneDisplayName()
{
	const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(g_xEngine.Scenes().GetActiveScene());
	return xInfo.m_strName.empty() ? std::string("Untitled") : xInfo.m_strName;
}

void NewScene()
{
	if (!RunPromptOrProceed(Zenith_EditorPromptState::Action::NewScene, ""))
	{
		return;
	}
	Zenith_Editor& xEditor = g_xEngine.Editor();
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	if (xActiveScene.IsValid())
	{
		g_xEngine.Scenes().UnloadSceneForced(xActiveScene);
	}
	Zenith_Scene xNewScene = g_xEngine.Scenes().LoadScene("Untitled", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	g_xEngine.Scenes().SetActiveScene(xNewScene);

	xEditor.ClearSelection();
	xEditor.m_xEditorState.m_xCamera.m_uGameCameraEntity = INVALID_ENTITY_ID;
	xEditor.ResetEditorCameraToDefaults();
	g_xEngine.UndoSystem().Clear();
	Zenith_Log(LOG_CATEGORY_EDITOR, "New scene created (handle=%d, name='Untitled')", xNewScene.GetHandle());
}

void OpenSceneDialog()
{
#ifdef _WIN32
	const std::string strPath = ShowOpenFileDialog(szSCENE_FILE_FILTER, ZENITH_SCENE_EXT + 1);
	if (!strPath.empty())
	{
		OpenScenePath(strPath);
	}
#endif
}

void OpenScenePath(const std::string& strPath)
{
	if (!RunPromptOrProceed(Zenith_EditorPromptState::Action::OpenScene, strPath))
	{
		return;
	}
	g_xEngine.Editor().RequestLoadSceneFromFile(strPath);
	RememberRecentScene(strPath);
}

void OpenSceneAdditiveDialog()
{
#ifdef _WIN32
	const std::string strPath = ShowOpenFileDialog(szSCENE_FILE_FILTER, ZENITH_SCENE_EXT + 1);
	if (!strPath.empty())
	{
		g_xEngine.Scenes().LoadScene(strPath, SCENE_LOAD_ADDITIVE);
		Zenith_Log(LOG_CATEGORY_EDITOR, "Scene loaded additively: %s", strPath.c_str());
	}
#endif
}

void RequestExit()
{
	if (!RunPromptOrProceed(Zenith_EditorPromptState::Action::Exit, ""))
	{
		return;
	}
	Zenith_Window::GetInstance()->RequestClose();
}

bool SaveScene()
{
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxSceneData == nullptr)
	{
		return false;
	}
	const Zenith_SceneInfo xInfo = g_xEngine.Scenes().GetSceneInfo(xScene);
	if (xInfo.m_strPath.empty())
	{
		return SaveSceneAs();
	}
	Zenith_EditorSceneAccess::SaveToFile(pxSceneData, xInfo.m_strPath);
	RememberRecentScene(xInfo.m_strPath);
	Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved: %s", xInfo.m_strPath.c_str());
	return true;
}

bool SaveSceneAs()
{
#ifdef _WIN32
	Zenith_Scene xScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xScene);
	if (pxSceneData == nullptr)
	{
		return false;
	}
	const std::string strDefault = GetActiveSceneDisplayName() + ZENITH_SCENE_EXT;
	const std::string strPath = ShowSaveFileDialog(szSCENE_FILE_FILTER, ZENITH_SCENE_EXT + 1, strDefault.c_str());
	if (strPath.empty())
	{
		return false;
	}
	Zenith_EditorSceneAccess::SaveToFile(pxSceneData, strPath);
	Zenith_EditorSceneAccess::Editor_SetPath(pxSceneData, strPath);
	RememberRecentScene(strPath);
	Zenith_Log(LOG_CATEGORY_EDITOR, "Scene saved as: %s", strPath.c_str());
	return true;
#else
	return false;
#endif
}

//=============================================================================
// Play mode
//=============================================================================

void TogglePlay()
{
	Zenith_Editor& xEditor = g_xEngine.Editor();
	if (xEditor.GetEditorMode() == EditorMode::Stopped)
	{
		if (xEditor.m_xEditorState.m_xPrefs.m_bClearConsoleOnPlay)
		{
			xEditor.ClearConsole();
		}
		xEditor.SetEditorMode(EditorMode::Playing);
	}
	else
	{
		xEditor.SetEditorMode(EditorMode::Stopped);
	}
}

void TogglePause()
{
	Zenith_Editor& xEditor = g_xEngine.Editor();
	if (xEditor.GetEditorMode() == EditorMode::Playing)
	{
		xEditor.SetEditorMode(EditorMode::Paused);
	}
	else if (xEditor.GetEditorMode() == EditorMode::Paused)
	{
		xEditor.SetEditorMode(EditorMode::Playing);
	}
}

void Stop()
{
	g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
}

//=============================================================================
// Bookkeeping
//=============================================================================

void MarkSceneDirtyForEntity(Zenith_EntityID xEntity)
{
	if (Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneDataForEntity(xEntity))
	{
		Zenith_EditorSceneAccess::MarkDirty(pxSceneData);
	}
}

} // namespace Zenith_EditorActions

#endif // ZENITH_TOOLS
