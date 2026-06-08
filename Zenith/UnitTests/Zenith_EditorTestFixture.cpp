#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#ifdef ZENITH_TOOLS

#include "UnitTests/Zenith_EditorTestFixture.h"
#include "Input/Zenith_InputSimulator.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "Editor/Zenith_EditorSceneAccess.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"

std::vector<Zenith_EntityID> Zenith_EditorTestFixture::s_axCreatedEntities;
bool Zenith_EditorTestFixture::s_bIsSetUp = false;
Zenith_Scene Zenith_EditorTestFixture::s_xTestScene;
bool Zenith_EditorTestFixture::s_bCreatedTestScene = false;

void Zenith_EditorTestFixture::SetUp()
{
	// Clear any previous test state
	if (s_bIsSetUp)
	{
		TearDown();
	}

	// Ensure an active scene exists. Under the auto-registering ZENITH_TEST
	// framework, tests run in linker-dependent order and previous tests may
	// have unloaded the only active scene. Create a fresh empty scene here
	// and tear it down in TearDown() so each editor/automation test starts
	// from a clean, known scene state.
	s_bCreatedTestScene = false;
	Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
	if (!xActive.IsValid() || g_xEngine.Scenes().GetSceneData(xActive) == nullptr)
	{
		s_xTestScene = g_xEngine.Scenes().LoadScene("EditorTestFixtureScene", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
		g_xEngine.Scenes().SetActiveScene(s_xTestScene);
		s_bCreatedTestScene = true;
	}

	// Enable mock input for tests
	Zenith_InputSimulator::Enable();

	// Reset editor state
	ResetEditorState();

	s_axCreatedEntities.clear();
	s_bIsSetUp = true;
}

void Zenith_EditorTestFixture::TearDown()
{
	if (!s_bIsSetUp)
	{
		return;
	}

	// Clean up created entities (in reverse order to handle hierarchies)
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	if (pxSceneData != nullptr)
	{
		for (auto it = s_axCreatedEntities.rbegin(); it != s_axCreatedEntities.rend(); ++it)
		{
			if (it->IsValid() && pxSceneData->EntityExists(*it))
			{
				Zenith_EditorSceneAccess::RemoveEntity(pxSceneData, *it);
			}
		}
	}
	s_axCreatedEntities.clear();

	// Reset editor state
	ResetEditorState();

	// Disable mock input
	Zenith_InputSimulator::Disable();

	// Unload the test scene we created so the next test starts fresh.
	if (s_bCreatedTestScene && s_xTestScene.IsValid())
	{
		g_xEngine.Scenes().UnloadSceneForced(s_xTestScene);
		s_xTestScene = Zenith_Scene();
		s_bCreatedTestScene = false;
	}

	s_bIsSetUp = false;
}

Zenith_EntityID Zenith_EditorTestFixture::CreateTestEntity(const std::string& strName)
{
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, strName.c_str());
	Zenith_EntityID uEntityID = xEntity.GetEntityID();
	s_axCreatedEntities.push_back(uEntityID);
	return uEntityID;
}

Zenith_EntityID Zenith_EditorTestFixture::CreateTestEntityWithTransform(
	const std::string& strName,
	const Zenith_Maths::Vector3& xPos,
	const Zenith_Maths::Vector3& xScale)
{
	Zenith_EntityID uEntityID = CreateTestEntity(strName);
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntityID);

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(xPos);
	xTransform.SetScale(xScale);

	return uEntityID;
}

Zenith_EntityID Zenith_EditorTestFixture::CreateTestEntityWithTransform(
	const std::string& strName,
	const Zenith_Maths::Vector3& xPos,
	const Zenith_Maths::Quat& xRot,
	const Zenith_Maths::Vector3& xScale)
{
	Zenith_EntityID uEntityID = CreateTestEntity(strName);
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntityID);

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(xPos);
	xTransform.SetRotation(xRot);
	xTransform.SetScale(xScale);

	return uEntityID;
}

void Zenith_EditorTestFixture::SetupHierarchy(Zenith_EntityID uParent, Zenith_EntityID uChild)
{
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);
	xChild.SetParent(uParent);
}

void Zenith_EditorTestFixture::ResetEditorState()
{
	// Clear selection
	g_xEngine.Editor().ClearSelection();

	// Ensure we're back in Stopped mode after any test that changed modes
	if (g_xEngine.Editor().GetEditorMode() != EditorMode::Stopped)
	{
		g_xEngine.Editor().SetEditorMode(EditorMode::Stopped);
	}

	// Flush any pending scene operations (e.g., scene restore after mode transition)
	// This ensures scene state is consistent before the next test runs
	g_xEngine.Editor().FlushPendingSceneOperations();

	// Reset gizmo mode to translate
	g_xEngine.Editor().SetGizmoMode(EditorGizmoMode::Translate);

	// Clear undo/redo history
	g_xEngine.UndoSystem().Clear();

	// Reset mock input state
	Zenith_InputSimulator::ResetAllInputState();
}

Zenith_SceneData* Zenith_EditorTestFixture::GetTestScene()
{
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	return g_xEngine.Scenes().GetSceneData(xActiveScene);
}

#endif // ZENITH_TOOLS
