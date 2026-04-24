#pragma once

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"
#include <vector>
#include <string>

class Zenith_EditorTestFixture
{
public:
	// Setup: Creates a clean test environment
	static void SetUp();

	// Teardown: Cleans up test entities and resets state
	static void TearDown();

	// Create test entities
	static Zenith_EntityID CreateTestEntity(const std::string& strName);

	static Zenith_EntityID CreateTestEntityWithTransform(
		const std::string& strName,
		const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Vector3& xScale);

	static Zenith_EntityID CreateTestEntityWithTransform(
		const std::string& strName,
		const Zenith_Maths::Vector3& xPos,
		const Zenith_Maths::Quat& xRot,
		const Zenith_Maths::Vector3& xScale);

	// Setup parent-child hierarchy
	static void SetupHierarchy(Zenith_EntityID uParent, Zenith_EntityID uChild);

	// Reset editor state (selection, mode, etc.)
	static void ResetEditorState();

	// Get current scene for tests
	static Zenith_SceneData* GetTestScene();

	// Track created entities for cleanup
	static const std::vector<Zenith_EntityID>& GetCreatedEntities() { return s_axCreatedEntities; }

private:
	static std::vector<Zenith_EntityID> s_axCreatedEntities;
	static bool s_bIsSetUp;
	// Scene created by SetUp() when no active scene exists. Unloaded by
	// TearDown(). Ensures editor/automation tests have a stable scene to
	// operate against regardless of test order under the auto-registering
	// ZENITH_TEST framework.
	static Zenith_Scene s_xTestScene;
	static bool s_bCreatedTestScene;
};

// Macros for consistent test structure.
// No live logging — the final suite summary reports pass/fail.
#define EDITOR_TEST_BEGIN(TestName) \
	Zenith_EditorTestFixture::SetUp();

#define EDITOR_TEST_END(TestName) \
	Zenith_EditorTestFixture::TearDown();

#endif // ZENITH_TOOLS
