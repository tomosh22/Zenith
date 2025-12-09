#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "UnitTests/Zenith_EditorTests.h"
#include "Editor/Zenith_SelectionSystem.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Maths/Zenith_Maths.h"
#include <cmath>

void Zenith_EditorTests::RunAllTests()
{
	TestBoundingBoxIntersection();
	TestSelectionSystemEmptyScene();
	TestInvalidEntityID();
	TestTransformRoundTrip();
}

void Zenith_EditorTests::TestBoundingBoxIntersection()
{
	// Test ray-AABB intersection
	BoundingBox xBox(Zenith_Maths::Vector3(-1, -1, -1), Zenith_Maths::Vector3(1, 1, 1));
	
	// Test 1: Ray hitting center of box from Z direction
	{
		Zenith_Maths::Vector3 rayOrigin(0, 0, -5);
		Zenith_Maths::Vector3 rayDir(0, 0, 1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		Zenith_Assert(bHit, "Ray should hit the box");
		Zenith_Assert(std::abs(distance - 4.0f) < 0.001f, "Distance should be ~4");
	}
	
	// Test 2: Ray missing the box
	{
		Zenith_Maths::Vector3 rayOrigin(5, 5, -5);
		Zenith_Maths::Vector3 rayDir(0, 0, 1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		Zenith_Assert(!bHit, "Ray should miss the box");
	}
	
	// Test 3: Ray starting inside the box
	{
		Zenith_Maths::Vector3 rayOrigin(0, 0, 0);
		Zenith_Maths::Vector3 rayDir(0, 0, 1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		Zenith_Assert(bHit, "Ray starting inside should hit");
	}
	
	// Test 4: Ray pointing away from box
	{
		Zenith_Maths::Vector3 rayOrigin(0, 0, -5);
		Zenith_Maths::Vector3 rayDir(0, 0, -1);
		float distance;
		bool bHit = xBox.Intersects(rayOrigin, rayDir, distance);
		Zenith_Assert(!bHit, "Ray pointing away should miss");
	}
	
	Zenith_Log("[EditorTests] TestBoundingBoxIntersection passed");
}

void Zenith_EditorTests::TestSelectionSystemEmptyScene()
{
	// Test that RaycastSelect returns INVALID_ENTITY_ID when no entities hit
	Zenith_SelectionSystem::Initialise();
	Zenith_SelectionSystem::UpdateBoundingBoxes();
	
	Zenith_Maths::Vector3 rayOrigin(0, 0, -100);
	Zenith_Maths::Vector3 rayDir(0, 0, 1);
	
	Zenith_EntityID result = Zenith_SelectionSystem::RaycastSelect(rayOrigin, rayDir);
	Zenith_Assert(result == INVALID_ENTITY_ID, "Empty scene should return INVALID_ENTITY_ID");
	
	Zenith_SelectionSystem::Shutdown();
	
	Zenith_Log("[EditorTests] TestSelectionSystemEmptyScene passed");
}

void Zenith_EditorTests::TestInvalidEntityID()
{
	// Test that INVALID_ENTITY_ID constant is properly defined
	Zenith_Assert(INVALID_ENTITY_ID == static_cast<Zenith_EntityID>(-1), "INVALID_ENTITY_ID should be -1");
	
	// Test that a valid entity ID is not equal to INVALID_ENTITY_ID
	Zenith_EntityID validID = 0;
	Zenith_Assert(validID != INVALID_ENTITY_ID, "Valid entity ID should not equal INVALID_ENTITY_ID");
	
	validID = 1;
	Zenith_Assert(validID != INVALID_ENTITY_ID, "Valid entity ID should not equal INVALID_ENTITY_ID");
	
	Zenith_Log("[EditorTests] TestInvalidEntityID passed");
}

void Zenith_EditorTests::TestTransformRoundTrip()
{
	// Test that transform values can be set and retrieved accurately
	// This is important for property panel editing
	
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();
	
	// Create a test entity
	Zenith_Entity xEntity(&xScene, "TestEntity");
	Zenith_TransformComponent& xTransform = xEntity.GetComponent<Zenith_TransformComponent>();
	
	// Test position round trip
	Zenith_Maths::Vector3 testPos(123.456f, -789.012f, 0.001f);
	xTransform.SetPosition(testPos);
	Zenith_Maths::Vector3 retrievedPos;
	xTransform.GetPosition(retrievedPos);
	Zenith_Assert(glm::length(testPos - retrievedPos) < 0.0001f, "Position round trip failed");
	
	// Test scale round trip
	Zenith_Maths::Vector3 testScale(2.0f, 0.5f, 3.0f);
	xTransform.SetScale(testScale);
	Zenith_Maths::Vector3 retrievedScale;
	xTransform.GetScale(retrievedScale);
	Zenith_Assert(glm::length(testScale - retrievedScale) < 0.0001f, "Scale round trip failed");
	
	// Test rotation round trip (quaternion)
	Zenith_Maths::Quat testRot = glm::angleAxis(glm::radians(45.0f), Zenith_Maths::Vector3(0, 1, 0));
	xTransform.SetRotation(testRot);
	Zenith_Maths::Quat retrievedRot;
	xTransform.GetRotation(retrievedRot);
	
	// Compare quaternions (accounting for sign ambiguity)
	float dotProduct = glm::dot(testRot, retrievedRot);
	Zenith_Assert(std::abs(std::abs(dotProduct) - 1.0f) < 0.0001f, "Rotation round trip failed");
	
	Zenith_Log("[EditorTests] TestTransformRoundTrip passed");
}

#endif // ZENITH_TOOLS
