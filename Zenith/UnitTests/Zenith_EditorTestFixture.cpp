#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "UnitTests/Zenith_EditorTestFixture.h"
#include "UnitTests/Zenith_MockInput.h"
#include "Editor/Zenith_Editor.h"
#include "Editor/Zenith_UndoSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

std::vector<Zenith_EntityID> Zenith_EditorTestFixture::s_axCreatedEntities;
bool Zenith_EditorTestFixture::s_bIsSetUp = false;

void Zenith_EditorTestFixture::SetUp()
{
	// Clear any previous test state
	if (s_bIsSetUp)
	{
		TearDown();
	}

	// Enable mock input for tests
	Zenith_MockInput::EnableMocking(true);

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
	Zenith_Scene& xScene = GetTestScene();
	for (auto it = s_axCreatedEntities.rbegin(); it != s_axCreatedEntities.rend(); ++it)
	{
		if (it->IsValid() && xScene.EntityExists(*it))
		{
			xScene.RemoveEntity(*it);
		}
	}
	s_axCreatedEntities.clear();

	// Reset editor state
	ResetEditorState();

	// Disable mock input
	Zenith_MockInput::EnableMocking(false);

	s_bIsSetUp = false;
}

Zenith_EntityID Zenith_EditorTestFixture::CreateTestEntity(const std::string& strName)
{
	Zenith_Scene& xScene = GetTestScene();
	Zenith_Entity xEntity(&xScene, strName.c_str());
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
	Zenith_Scene& xScene = GetTestScene();
	Zenith_Entity xEntity = xScene.GetEntity(uEntityID);

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
	Zenith_Scene& xScene = GetTestScene();
	Zenith_Entity xEntity = xScene.GetEntity(uEntityID);

	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	xTransform.SetPosition(xPos);
	xTransform.SetRotation(xRot);
	xTransform.SetScale(xScale);

	return uEntityID;
}

void Zenith_EditorTestFixture::SetupHierarchy(Zenith_EntityID uParent, Zenith_EntityID uChild)
{
	Zenith_Scene& xScene = GetTestScene();
	Zenith_Entity xChild = xScene.GetEntity(uChild);
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
	Zenith_MockInput::Reset();
}

Zenith_Scene& Zenith_EditorTestFixture::GetTestScene()
{
	return Zenith_Scene::GetCurrentScene();
}

#endif // ZENITH_TOOLS
