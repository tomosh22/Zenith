#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "UnitTests/Zenith_EditorTestFixture.h"
#include "Input/Zenith_InputSimulator.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"

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
	Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
	if (!xActive.IsValid() || Zenith_SceneManager::GetSceneData(xActive) == nullptr)
	{
		s_xTestScene = Zenith_SceneManager::CreateEmptyScene("EditorTestFixtureScene");
		Zenith_SceneManager::SetActiveScene(s_xTestScene);
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (pxSceneData != nullptr)
	{
		for (auto it = s_axCreatedEntities.rbegin(); it != s_axCreatedEntities.rend(); ++it)
		{
			if (it->IsValid() && pxSceneData->EntityExists(*it))
			{
				pxSceneData->RemoveEntity(*it);
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
		Zenith_SceneManager::UnloadSceneForced(s_xTestScene);
		s_xTestScene = Zenith_Scene();
		s_bCreatedTestScene = false;
	}

	s_bIsSetUp = false;
}

Zenith_EntityID Zenith_EditorTestFixture::CreateTestEntity(const std::string& strName)
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xEntity(pxSceneData, strName.c_str());
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
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
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xEntity = pxSceneData->GetEntity(uEntityID);

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(xPos);
	xTransform.SetRotation(xRot);
	xTransform.SetScale(xScale);

	return uEntityID;
}

void Zenith_EditorTestFixture::SetupHierarchy(Zenith_EntityID uParent, Zenith_EntityID uChild)
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	Zenith_Entity xChild = pxSceneData->GetEntity(uChild);
	xChild.SetParent(uParent);
}

void Zenith_EditorTestFixture::ResetEditorState()
{
	// Clear selection
	Zenith_Editor::ClearSelection();

	// Ensure we're back in Stopped mode after any test that changed modes
	if (Zenith_Editor::GetEditorMode() != EditorMode::Stopped)
	{
		Zenith_Editor::SetEditorMode(EditorMode::Stopped);
	}

	// Flush any pending scene operations (e.g., scene restore after mode transition)
	// This ensures scene state is consistent before the next test runs
	Zenith_Editor::FlushPendingSceneOperations();

	// Reset gizmo mode to translate
	Zenith_Editor::SetGizmoMode(EditorGizmoMode::Translate);

	// Clear undo/redo history
	Zenith_UndoSystem::Clear();

	// Reset mock input state
	Zenith_InputSimulator::ResetAllInputState();
}

Zenith_SceneData* Zenith_EditorTestFixture::GetTestScene()
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	return Zenith_SceneManager::GetSceneData(xActiveScene);
}

#endif // ZENITH_TOOLS
