#pragma once

#ifdef ZENITH_TOOLS

#include "EntityComponent/Zenith_Scene.h"
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
	static Zenith_Scene& GetTestScene();

	// Track created entities for cleanup
	static const std::vector<Zenith_EntityID>& GetCreatedEntities() { return s_axCreatedEntities; }

private:
	static std::vector<Zenith_EntityID> s_axCreatedEntities;
	static bool s_bIsSetUp;
};

// Macros for consistent test structure
#define EDITOR_TEST_BEGIN(TestName) \
	Zenith_Log(LOG_CATEGORY_UNITTEST, "Running " #TestName "..."); \
	Zenith_EditorTestFixture::SetUp();

#define EDITOR_TEST_END(TestName) \
	Zenith_EditorTestFixture::TearDown(); \
	Zenith_Log(LOG_CATEGORY_UNITTEST, "[EditorTests] " #TestName " passed");

#endif // ZENITH_TOOLS
