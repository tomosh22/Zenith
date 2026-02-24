// This file queries Jolt Physics objects - disable memory tracking macro to avoid conflicts
#include "Zenith.h"
#define ZENITH_PLACEMENT_NEW_ZONE
#include "Memory/Zenith_MemoryManagement_Disabled.h"

#include "UnitTests/Zenith_PhysicsTests.h"
#include "Physics/Zenith_Physics.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>

#include "Memory/Zenith_MemoryManagement_Enabled.h"

#include <cmath>

//==============================================================================
// PhysicsTestBehaviour - tracks collision callbacks via static counters
//==============================================================================

class PhysicsTestBehaviour : public Zenith_ScriptBehaviour
{
public:
	PhysicsTestBehaviour(Zenith_Entity& xEntity) { m_xParentEntity = xEntity; }

	static uint32_t s_uCollisionEnterCount;
	static uint32_t s_uCollisionStayCount;
	static uint32_t s_uCollisionExitCount;
	static Zenith_EntityID s_xLastCollisionEnterOther;
	static Zenith_EntityID s_xLastCollisionExitOther;

	static void ResetCounters()
	{
		s_uCollisionEnterCount = 0;
		s_uCollisionStayCount = 0;
		s_uCollisionExitCount = 0;
		s_xLastCollisionEnterOther = Zenith_EntityID();
		s_xLastCollisionExitOther = Zenith_EntityID();
	}

	void OnCollisionEnter(Zenith_Entity xOther) override
	{
		s_uCollisionEnterCount++;
		s_xLastCollisionEnterOther = xOther.GetEntityID();
	}

	void OnCollisionStay(Zenith_Entity) override
	{
		s_uCollisionStayCount++;
	}

	void OnCollisionExit(Zenith_EntityID xOtherID) override
	{
		s_uCollisionExitCount++;
		s_xLastCollisionExitOther = xOtherID;
	}

	const char* GetBehaviourTypeName() const override { return "PhysicsTestBehaviour"; }
};

uint32_t PhysicsTestBehaviour::s_uCollisionEnterCount = 0;
uint32_t PhysicsTestBehaviour::s_uCollisionStayCount = 0;
uint32_t PhysicsTestBehaviour::s_uCollisionExitCount = 0;
Zenith_EntityID PhysicsTestBehaviour::s_xLastCollisionEnterOther;
Zenith_EntityID PhysicsTestBehaviour::s_xLastCollisionExitOther;

//==============================================================================
// Helper Functions
//==============================================================================

static void ResetPhysicsState()
{
	Zenith_Physics::s_fTimestepAccumulator = 0;
}

static Zenith_Entity CreatePhysicsSphere(
	Zenith_SceneData* pxSceneData,
	const std::string& strName,
	Zenith_Maths::Vector3 xPos,
	RigidBodyType eType,
	float fScale = 0.5f)
{
	Zenith_Entity xEntity(pxSceneData, strName);
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(
		Zenith_Maths::Vector3(fScale, fScale, fScale));
	xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_SPHERE, eType);
	return xEntity;
}

static Zenith_Entity CreatePhysicsBox(
	Zenith_SceneData* pxSceneData,
	const std::string& strName,
	Zenith_Maths::Vector3 xPos,
	Zenith_Maths::Vector3 xScale,
	RigidBodyType eType)
{
	Zenith_Entity xEntity(pxSceneData, strName);
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(xScale);
	xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_AABB, eType);
	return xEntity;
}

static void StepPhysics(uint32_t uSteps)
{
	for (uint32_t i = 0; i < uSteps; i++)
	{
		Zenith_Physics::Update(1.0f / 60.0f);
	}
}

static Zenith_Maths::Vector3 GetBodyPosition(const Zenith_ColliderComponent& xCollider)
{
	JPH::BodyInterface& xBI = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
	JPH::RVec3 xPos = xBI.GetPosition(xCollider.GetBodyID());
	return Zenith_Maths::Vector3(
		static_cast<float>(xPos.GetX()),
		static_cast<float>(xPos.GetY()),
		static_cast<float>(xPos.GetZ()));
}

static bool ApproxEqual(float fA, float fB, float fEpsilon = 0.01f)
{
	return std::abs(fA - fB) < fEpsilon;
}

//==============================================================================
// RunAllTests
//==============================================================================

void Zenith_PhysicsTests::RunAllTests()
{
	Zenith_Log(LOG_CATEGORY_UNITTEST, "=== Running Physics Tests ===");

	// Cat 1: Gravity
	TestDynamicBodyFallsUnderGravity();
	TestStaticBodyDoesNotFall();
	TestGravityDisabledBodyStaysStill();
	TestGravityReenabledBodyFalls();

	// Cat 2: Velocity
	TestSetLinearVelocity();
	TestGetLinearVelocityMatchesSet();
	TestSetAngularVelocity();
	TestGetAngularVelocityMatchesSet();
	TestZeroVelocityNoMovement();

	// Cat 3: Forces & Impulses
	TestAddForceAcceleratesBody();
	TestAddImpulseInstantVelocityChange();
	TestForceAccumulatesOverFrames();
	TestImpulseOnStaticBodyNoEffect();

	// Cat 4: Collision Detection
	TestDynamicHitsStaticFloor();
	TestTwoDynamicBodiesCollide();
	TestStaticStaticNoCollision();
	TestSphereOnBoxCollision();

	// Cat 5: Collision Events
	TestCollisionEnterCallback();
	TestCollisionStayCallback();
	TestCollisionExitCallback();
	TestCollisionEventBothEntitiesReceive();

	// Cat 6: Raycasting
	TestRaycastHitsSphere();
	TestRaycastMissesNoBody();
	TestRaycastReturnsHitPoint();
	TestRaycastReturnsHitEntity();
	TestRaycastMaxDistanceRespected();

	// Cat 7: Body Configuration
	TestLockRotationPreventsAngularVelocity();
	TestColliderHasValidBodyAfterAdd();
	TestColliderBodyIDMatchesJolt();
	TestRebuildColliderPreservesVelocity();

	// Cat 8: Collider Types
	TestAABBColliderCreation();
	TestSphereColliderCreation();
	TestCapsuleColliderCreation();
	TestCapsuleExplicitDimensions();

	// Cat 9: Timestep
	TestFixedTimestepOneStep();
	TestAccumulatorDoesNotOverStep();
	TestResetClearsPhysicsState();

	// Cat 10: Cleanup
	TestUnloadSceneDestroysPhysicsBodies();
	TestMultipleScenePhysicsIndependence();

	Zenith_Log(LOG_CATEGORY_UNITTEST, "=== Physics Tests Complete ===");
}

//==============================================================================
// Cat 1: Gravity & Free-Fall
//==============================================================================

void Zenith_PhysicsTests::TestDynamicBodyFallsUnderGravity()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Gravity");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "FallSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(60);

	// After 1 second: y = 10 - 0.5 * 9.81 * 1^2 ~= 5.1
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.y < 6.0f, "TestDynamicBodyFallsUnderGravity: Sphere should have fallen below Y=6 (got %f)", xPos.y);
	Zenith_Assert(xPos.y > 4.0f, "TestDynamicBodyFallsUnderGravity: Sphere should not be below Y=4 after 1s (got %f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDynamicBodyFallsUnderGravity PASSED");
}

void Zenith_PhysicsTests::TestStaticBodyDoesNotFall()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_StaticNoFall");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xBox = CreatePhysicsBox(pxSceneData, "StaticBox",
		Zenith_Maths::Vector3(0, 5, 0), Zenith_Maths::Vector3(1, 1, 1),
		RIGIDBODY_TYPE_STATIC);
	Zenith_ColliderComponent& xCollider = xBox.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(ApproxEqual(xPos.y, 5.0f), "TestStaticBodyDoesNotFall: Static body should remain at Y=5 (got %f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaticBodyDoesNotFall PASSED");
}

void Zenith_PhysicsTests::TestGravityDisabledBodyStaysStill()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_NoGravity");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "NoGravSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(ApproxEqual(xPos.y, 10.0f), "TestGravityDisabledBodyStaysStill: Body should remain at Y=10 (got %f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGravityDisabledBodyStaysStill PASSED");
}

void Zenith_PhysicsTests::TestGravityReenabledBodyFalls()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_ReenableGrav");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ReGravSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	StepPhysics(30);
	Zenith_Maths::Vector3 xPosBefore = GetBodyPosition(xCollider);
	Zenith_Assert(ApproxEqual(xPosBefore.y, 10.0f), "TestGravityReenabledBodyFalls: Body should be at Y=10 while gravity disabled (got %f)", xPosBefore.y);

	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), true);
	StepPhysics(30);

	Zenith_Maths::Vector3 xPosAfter = GetBodyPosition(xCollider);
	Zenith_Assert(xPosAfter.y < 10.0f, "TestGravityReenabledBodyFalls: Body should have fallen after re-enabling gravity (got %f)", xPosAfter.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGravityReenabledBodyFalls PASSED");
}

//==============================================================================
// Cat 2: Velocity
//==============================================================================

void Zenith_PhysicsTests::TestSetLinearVelocity()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_LinVel");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "VelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);
	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(5, 0, 0));

	StepPhysics(60);

	// After 1 second at 5 m/s, X should be ~5.0
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.x > 4.5f && xPos.x < 5.5f, "TestSetLinearVelocity: Expected X~5.0, got %f", xPos.x);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetLinearVelocity PASSED");
}

void Zenith_PhysicsTests::TestGetLinearVelocityMatchesSet()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_GetLinVel");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "GetVelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(3, 4, 5));
	Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());

	Zenith_Assert(ApproxEqual(xVel.x, 3.0f), "TestGetLinearVelocityMatchesSet: Expected vx=3, got %f", xVel.x);
	Zenith_Assert(ApproxEqual(xVel.y, 4.0f), "TestGetLinearVelocityMatchesSet: Expected vy=4, got %f", xVel.y);
	Zenith_Assert(ApproxEqual(xVel.z, 5.0f), "TestGetLinearVelocityMatchesSet: Expected vz=5, got %f", xVel.z);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetLinearVelocityMatchesSet PASSED");
}

void Zenith_PhysicsTests::TestSetAngularVelocity()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_AngVel");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "AngVelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);
	Zenith_Physics::SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 5, 0));

	StepPhysics(1);

	Zenith_Maths::Vector3 xAngVel = Zenith_Physics::GetAngularVelocity(xCollider.GetBodyID());
	Zenith_Assert(std::abs(xAngVel.y) > 0.1f, "TestSetAngularVelocity: Angular velocity Y should be non-zero (got %f)", xAngVel.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSetAngularVelocity PASSED");
}

void Zenith_PhysicsTests::TestGetAngularVelocityMatchesSet()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_GetAngVel");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "GetAngVelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	Zenith_Physics::SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(1, 2, 3));
	Zenith_Maths::Vector3 xAngVel = Zenith_Physics::GetAngularVelocity(xCollider.GetBodyID());

	Zenith_Assert(ApproxEqual(xAngVel.x, 1.0f, 0.1f), "TestGetAngularVelocityMatchesSet: Expected wx=1, got %f", xAngVel.x);
	Zenith_Assert(ApproxEqual(xAngVel.y, 2.0f, 0.1f), "TestGetAngularVelocityMatchesSet: Expected wy=2, got %f", xAngVel.y);
	Zenith_Assert(ApproxEqual(xAngVel.z, 3.0f, 0.1f), "TestGetAngularVelocityMatchesSet: Expected wz=3, got %f", xAngVel.z);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestGetAngularVelocityMatchesSet PASSED");
}

void Zenith_PhysicsTests::TestZeroVelocityNoMovement()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_ZeroVel");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ZeroVelSphere",
		Zenith_Maths::Vector3(5, 5, 5), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);
	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 0, 0));
	Zenith_Physics::SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 0, 0));

	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(ApproxEqual(xPos.x, 5.0f), "TestZeroVelocityNoMovement: X should remain 5 (got %f)", xPos.x);
	Zenith_Assert(ApproxEqual(xPos.y, 5.0f), "TestZeroVelocityNoMovement: Y should remain 5 (got %f)", xPos.y);
	Zenith_Assert(ApproxEqual(xPos.z, 5.0f), "TestZeroVelocityNoMovement: Z should remain 5 (got %f)", xPos.z);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestZeroVelocityNoMovement PASSED");
}

//==============================================================================
// Cat 3: Forces & Impulses
//==============================================================================

void Zenith_PhysicsTests::TestAddForceAcceleratesBody()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Force");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ForceSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	// Apply force each frame for 60 frames
	for (uint32_t i = 0; i < 60; i++)
	{
		Zenith_Physics::AddForce(xCollider.GetBodyID(), Zenith_Maths::Vector3(100, 0, 0));
		Zenith_Physics::Update(1.0f / 60.0f);
	}

	Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
	Zenith_Assert(xVel.x > 0.0f, "TestAddForceAcceleratesBody: X velocity should be positive (got %f)", xVel.x);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.x > 0.0f, "TestAddForceAcceleratesBody: X position should have increased (got %f)", xPos.x);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddForceAcceleratesBody PASSED");
}

void Zenith_PhysicsTests::TestAddImpulseInstantVelocityChange()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Impulse");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ImpulseSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	// AddImpulse uses AddLinearVelocity internally
	Zenith_Physics::AddImpulse(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 10, 0));

	Zenith_Maths::Vector3 xVel = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
	Zenith_Assert(ApproxEqual(xVel.y, 10.0f, 0.5f), "TestAddImpulseInstantVelocityChange: Expected vy~10, got %f", xVel.y);

	StepPhysics(60);
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.y > 9.0f, "TestAddImpulseInstantVelocityChange: Body should have moved upward (got Y=%f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAddImpulseInstantVelocityChange PASSED");
}

void Zenith_PhysicsTests::TestForceAccumulatesOverFrames()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_ForceAccum");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Body A: apply force for 30 frames
	Zenith_Entity xSphereA = CreatePhysicsSphere(pxSceneData, "ForceA",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xColliderA = xSphereA.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xColliderA.GetBodyID(), false);

	// Body B: apply force for 60 frames
	Zenith_Entity xSphereB = CreatePhysicsSphere(pxSceneData, "ForceB",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xColliderB = xSphereB.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xColliderB.GetBodyID(), false);

	for (uint32_t i = 0; i < 60; i++)
	{
		if (i < 30)
		{
			Zenith_Physics::AddForce(xColliderA.GetBodyID(), Zenith_Maths::Vector3(100, 0, 0));
		}
		Zenith_Physics::AddForce(xColliderB.GetBodyID(), Zenith_Maths::Vector3(100, 0, 0));
		Zenith_Physics::Update(1.0f / 60.0f);
	}

	Zenith_Maths::Vector3 xVelA = Zenith_Physics::GetLinearVelocity(xColliderA.GetBodyID());
	Zenith_Maths::Vector3 xVelB = Zenith_Physics::GetLinearVelocity(xColliderB.GetBodyID());

	// Body B had force applied for twice as long, so velocity should be roughly double
	Zenith_Assert(xVelB.x > xVelA.x * 1.5f, "TestForceAccumulatesOverFrames: 60-frame body should be faster than 30-frame body (A=%f, B=%f)", xVelA.x, xVelB.x);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestForceAccumulatesOverFrames PASSED");
}

void Zenith_PhysicsTests::TestImpulseOnStaticBodyNoEffect()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_ImpulseStatic");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xBox = CreatePhysicsBox(pxSceneData, "StaticImpulseBox",
		Zenith_Maths::Vector3(0, 5, 0), Zenith_Maths::Vector3(1, 1, 1),
		RIGIDBODY_TYPE_STATIC);
	Zenith_ColliderComponent& xCollider = xBox.GetComponent<Zenith_ColliderComponent>();

	Zenith_Physics::AddImpulse(xCollider.GetBodyID(), Zenith_Maths::Vector3(100, 100, 100));
	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(ApproxEqual(xPos.y, 5.0f), "TestImpulseOnStaticBodyNoEffect: Static body should remain at Y=5 (got %f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestImpulseOnStaticBodyNoEffect PASSED");
}

//==============================================================================
// Cat 4: Collision Detection
//==============================================================================

void Zenith_PhysicsTests::TestDynamicHitsStaticFloor()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_DynHitsFloor");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Static floor at Y=0
	CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);

	// Dynamic sphere above the floor
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "FallingSphere",
		Zenith_Maths::Vector3(0, 5, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	// Step enough frames for the sphere to fall and settle
	StepPhysics(300);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	// Sphere should be resting above Y=0 (floor top + sphere radius)
	Zenith_Assert(xPos.y > 0.0f, "TestDynamicHitsStaticFloor: Sphere should be above the floor (got Y=%f)", xPos.y);
	Zenith_Assert(xPos.y < 3.0f, "TestDynamicHitsStaticFloor: Sphere should have fallen near the floor (got Y=%f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestDynamicHitsStaticFloor PASSED");
}

void Zenith_PhysicsTests::TestTwoDynamicBodiesCollide()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_TwoDynCollide");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Two spheres approaching each other horizontally
	Zenith_Entity xSphereA = CreatePhysicsSphere(pxSceneData, "SphereA",
		Zenith_Maths::Vector3(-5, 0, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xColliderA = xSphereA.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xColliderA.GetBodyID(), false);
	Zenith_Physics::SetLinearVelocity(xColliderA.GetBodyID(), Zenith_Maths::Vector3(5, 0, 0));

	Zenith_Entity xSphereB = CreatePhysicsSphere(pxSceneData, "SphereB",
		Zenith_Maths::Vector3(5, 0, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xColliderB = xSphereB.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xColliderB.GetBodyID(), false);
	Zenith_Physics::SetLinearVelocity(xColliderB.GetBodyID(), Zenith_Maths::Vector3(-5, 0, 0));

	StepPhysics(120);

	// After collision, the spheres should have bounced or stopped
	// They should NOT have passed through each other
	Zenith_Maths::Vector3 xPosA = GetBodyPosition(xColliderA);
	Zenith_Maths::Vector3 xPosB = GetBodyPosition(xColliderB);

	// If they passed through, A would be at ~X=5 and B at ~X=-5 (after 2 seconds at 5m/s)
	// With collision, positions should be closer to the middle
	Zenith_Assert(xPosA.x < 4.0f, "TestTwoDynamicBodiesCollide: SphereA should not have passed through (X=%f)", xPosA.x);
	Zenith_Assert(xPosB.x > -4.0f, "TestTwoDynamicBodiesCollide: SphereB should not have passed through (X=%f)", xPosB.x);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestTwoDynamicBodiesCollide PASSED");
}

void Zenith_PhysicsTests::TestStaticStaticNoCollision()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_StaticStatic");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Two overlapping static boxes - should not collide (layer filter: NON_MOVING vs NON_MOVING = false)
	CreatePhysicsBox(pxSceneData, "StaticA",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(1, 1, 1),
		RIGIDBODY_TYPE_STATIC);
	CreatePhysicsBox(pxSceneData, "StaticB",
		Zenith_Maths::Vector3(0.5f, 0, 0), Zenith_Maths::Vector3(1, 1, 1),
		RIGIDBODY_TYPE_STATIC);

	// If this doesn't crash, the test passes
	StepPhysics(10);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestStaticStaticNoCollision PASSED");
}

void Zenith_PhysicsTests::TestSphereOnBoxCollision()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_SphereOnBox");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Static floor - AABB with half-extents (10, 0.5, 10), center at Y=0
	// So floor top surface is at Y = 0.5
	CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);

	// Dynamic sphere with radius 0.5 (scale = 0.5, sphere radius = max scale component)
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "Sphere",
		Zenith_Maths::Vector3(0, 5, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(300);

	// Sphere should rest at approximately: floor_top + sphere_radius = 0.5 + 0.5 = 1.0
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.y > 0.5f, "TestSphereOnBoxCollision: Sphere should be above floor surface (got Y=%f)", xPos.y);
	Zenith_Assert(xPos.y < 2.0f, "TestSphereOnBoxCollision: Sphere should have settled near floor (got Y=%f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSphereOnBoxCollision PASSED");
}

//==============================================================================
// Cat 5: Collision Events
//==============================================================================

void Zenith_PhysicsTests::TestCollisionEnterCallback()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_CollEnter");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor with script component
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Dynamic sphere that will fall onto the floor
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "FallingSphere",
		Zenith_Maths::Vector3(0, 3, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	xSphere.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Step enough frames for sphere to hit the floor
	StepPhysics(120);

	Zenith_Assert(PhysicsTestBehaviour::s_uCollisionEnterCount > 0,
		"TestCollisionEnterCallback: Expected collision enter count > 0, got %u",
		PhysicsTestBehaviour::s_uCollisionEnterCount);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCollisionEnterCallback PASSED");
}

void Zenith_PhysicsTests::TestCollisionStayCallback()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_CollStay");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Dynamic sphere close to floor so it hits quickly
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "StaySphere",
		Zenith_Maths::Vector3(0, 2, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	xSphere.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Step many frames - after initial contact, stay events should fire each frame
	StepPhysics(180);

	Zenith_Assert(PhysicsTestBehaviour::s_uCollisionStayCount > 0,
		"TestCollisionStayCallback: Expected collision stay count > 0, got %u",
		PhysicsTestBehaviour::s_uCollisionStayCount);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCollisionStayCallback PASSED");
}

void Zenith_PhysicsTests::TestCollisionExitCallback()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_CollExit");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Dynamic sphere close to floor
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ExitSphere",
		Zenith_Maths::Vector3(0, 2, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	xSphere.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Step until collision
	StepPhysics(60);

	// Now launch the sphere upward to separate from floor
	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 20, 0));
	PhysicsTestBehaviour::s_uCollisionExitCount = 0;

	// Step to allow separation
	StepPhysics(60);

	Zenith_Assert(PhysicsTestBehaviour::s_uCollisionExitCount > 0,
		"TestCollisionExitCallback: Expected collision exit count > 0, got %u",
		PhysicsTestBehaviour::s_uCollisionExitCount);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCollisionExitCallback PASSED");
}

void Zenith_PhysicsTests::TestCollisionEventBothEntitiesReceive()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_CollBoth");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor with behaviour
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	// Falling sphere with behaviour
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "BothSphere",
		Zenith_Maths::Vector3(0, 3, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	xSphere.AddComponent<Zenith_ScriptComponent>().SetBehaviour<PhysicsTestBehaviour>();

	StepPhysics(120);

	// Both entities should receive OnCollisionEnter, so count >= 2
	Zenith_Assert(PhysicsTestBehaviour::s_uCollisionEnterCount >= 2,
		"TestCollisionEventBothEntitiesReceive: Expected collision enter count >= 2 (got %u)",
		PhysicsTestBehaviour::s_uCollisionEnterCount);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCollisionEventBothEntitiesReceive PASSED");
}

//==============================================================================
// Cat 6: Raycasting
//==============================================================================

void Zenith_PhysicsTests::TestRaycastHitsSphere()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_RayHit");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	CreatePhysicsSphere(pxSceneData, "Target",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_STATIC, 1.0f);

	// Step once to ensure body is in the broadphase
	StepPhysics(1);

	Zenith_Physics::RaycastResult xResult = Zenith_Physics::Raycast(
		Zenith_Maths::Vector3(0, 0, -10),
		Zenith_Maths::Vector3(0, 0, 1),
		20.0f);

	Zenith_Assert(xResult.m_bHit, "TestRaycastHitsSphere: Raycast should have hit the sphere");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRaycastHitsSphere PASSED");
}

void Zenith_PhysicsTests::TestRaycastMissesNoBody()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_RayMiss");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	(void)pxSceneData;
	ResetPhysicsState();

	// Empty scene - raycast should miss
	Zenith_Physics::RaycastResult xResult = Zenith_Physics::Raycast(
		Zenith_Maths::Vector3(0, 0, 0),
		Zenith_Maths::Vector3(0, 0, 1),
		100.0f);

	Zenith_Assert(!xResult.m_bHit, "TestRaycastMissesNoBody: Raycast should not hit anything in empty scene");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRaycastMissesNoBody PASSED");
}

void Zenith_PhysicsTests::TestRaycastReturnsHitPoint()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_RayPoint");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Sphere at origin with radius 1
	CreatePhysicsSphere(pxSceneData, "Target",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_STATIC, 1.0f);

	StepPhysics(1);

	Zenith_Physics::RaycastResult xResult = Zenith_Physics::Raycast(
		Zenith_Maths::Vector3(0, 0, -10),
		Zenith_Maths::Vector3(0, 0, 1),
		20.0f);

	Zenith_Assert(xResult.m_bHit, "TestRaycastReturnsHitPoint: Should hit sphere");
	// Hit point should be on the sphere surface, approximately at Z = -1 (near side)
	Zenith_Assert(xResult.m_xHitPoint.z < 0.0f, "TestRaycastReturnsHitPoint: Hit point Z should be negative (near side), got %f", xResult.m_xHitPoint.z);
	Zenith_Assert(xResult.m_fDistance > 8.0f && xResult.m_fDistance < 10.0f,
		"TestRaycastReturnsHitPoint: Distance should be ~9 (got %f)", xResult.m_fDistance);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRaycastReturnsHitPoint PASSED");
}

void Zenith_PhysicsTests::TestRaycastReturnsHitEntity()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_RayEntity");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xTarget = CreatePhysicsSphere(pxSceneData, "Target",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_STATIC, 1.0f);
	Zenith_EntityID xExpectedID = xTarget.GetEntityID();

	StepPhysics(1);

	Zenith_Physics::RaycastResult xResult = Zenith_Physics::Raycast(
		Zenith_Maths::Vector3(0, 0, -10),
		Zenith_Maths::Vector3(0, 0, 1),
		20.0f);

	Zenith_Assert(xResult.m_bHit, "TestRaycastReturnsHitEntity: Should hit sphere");
	Zenith_Assert(xResult.m_xHitEntity == xExpectedID,
		"TestRaycastReturnsHitEntity: Hit entity ID should match target");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRaycastReturnsHitEntity PASSED");
}

void Zenith_PhysicsTests::TestRaycastMaxDistanceRespected()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_RayMaxDist");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Sphere at Z=20
	CreatePhysicsSphere(pxSceneData, "FarTarget",
		Zenith_Maths::Vector3(0, 0, 20), RIGIDBODY_TYPE_STATIC, 1.0f);

	StepPhysics(1);

	// Short ray should miss
	Zenith_Physics::RaycastResult xShortResult = Zenith_Physics::Raycast(
		Zenith_Maths::Vector3(0, 0, 0),
		Zenith_Maths::Vector3(0, 0, 1),
		5.0f);
	Zenith_Assert(!xShortResult.m_bHit, "TestRaycastMaxDistanceRespected: Short ray should miss");

	// Long ray should hit
	Zenith_Physics::RaycastResult xLongResult = Zenith_Physics::Raycast(
		Zenith_Maths::Vector3(0, 0, 0),
		Zenith_Maths::Vector3(0, 0, 1),
		25.0f);
	Zenith_Assert(xLongResult.m_bHit, "TestRaycastMaxDistanceRespected: Long ray should hit");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRaycastMaxDistanceRespected PASSED");
}

//==============================================================================
// Cat 7: Body Configuration
//==============================================================================

void Zenith_PhysicsTests::TestLockRotationPreventsAngularVelocity()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_LockRot");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "LockRotSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	// Set angular velocity first, then lock - LockRotation zeroes velocity on locked axes
	Zenith_Physics::SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(10, 10, 10));
	Zenith_Physics::LockRotation(xCollider.GetBodyID(), true, true, true);
	StepPhysics(10);

	Zenith_Maths::Vector3 xAngVel = Zenith_Physics::GetAngularVelocity(xCollider.GetBodyID());
	float fTotalAngVel = std::abs(xAngVel.x) + std::abs(xAngVel.y) + std::abs(xAngVel.z);
	Zenith_Assert(fTotalAngVel < 1.0f, "TestLockRotationPreventsAngularVelocity: Angular velocity should be near zero (got total=%f)", fTotalAngVel);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestLockRotationPreventsAngularVelocity PASSED");
}

void Zenith_PhysicsTests::TestColliderHasValidBodyAfterAdd()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_ValidBody");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	Zenith_Entity xEntity(pxSceneData, "ValidBodyEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Assert(xCollider.HasValidBody(), "TestColliderHasValidBodyAfterAdd: Collider should have valid body");
	Zenith_Assert(!xCollider.GetBodyID().IsInvalid(), "TestColliderHasValidBodyAfterAdd: Body ID should not be invalid");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestColliderHasValidBodyAfterAdd PASSED");
}

void Zenith_PhysicsTests::TestColliderBodyIDMatchesJolt()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_BodyIDJolt");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	Zenith_Entity xEntity(pxSceneData, "JoltBodyEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

	// Verify body exists in Jolt
	JPH::BodyInterface& xBI = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
	bool bIsAdded = xBI.IsAdded(xCollider.GetBodyID());
	Zenith_Assert(bIsAdded, "TestColliderBodyIDMatchesJolt: Body should be added to Jolt physics system");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestColliderBodyIDMatchesJolt PASSED");
}

void Zenith_PhysicsTests::TestRebuildColliderPreservesVelocity()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Rebuild");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "RebuildSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Physics::SetGravityEnabled(xCollider.GetBodyID(), false);

	Zenith_Physics::SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(5, 0, 0));

	// Rebuild the collider (e.g. after scale change)
	xCollider.RebuildCollider();

	Zenith_Maths::Vector3 xVelAfter = Zenith_Physics::GetLinearVelocity(xCollider.GetBodyID());
	Zenith_Assert(ApproxEqual(xVelAfter.x, 5.0f, 0.5f), "TestRebuildColliderPreservesVelocity: Velocity X should be preserved (~5, got %f)", xVelAfter.x);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestRebuildColliderPreservesVelocity PASSED");
}

//==============================================================================
// Cat 8: Collider Shape Types
//==============================================================================

void Zenith_PhysicsTests::TestAABBColliderCreation()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_AABB");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	Zenith_Entity xEntity(pxSceneData, "AABBEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Assert(xCollider.HasValidBody(), "TestAABBColliderCreation: AABB collider should have valid body");
	Zenith_Assert(xCollider.GetRigidBodyType() == RIGIDBODY_TYPE_DYNAMIC, "TestAABBColliderCreation: Should be dynamic");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAABBColliderCreation PASSED");
}

void Zenith_PhysicsTests::TestSphereColliderCreation()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Sphere");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	Zenith_Entity xEntity(pxSceneData, "SphereEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Assert(xCollider.HasValidBody(), "TestSphereColliderCreation: Sphere collider should have valid body");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestSphereColliderCreation PASSED");
}

void Zenith_PhysicsTests::TestCapsuleColliderCreation()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Capsule");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	Zenith_Entity xEntity(pxSceneData, "CapsuleEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Assert(xCollider.HasValidBody(), "TestCapsuleColliderCreation: Capsule collider should have valid body");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCapsuleColliderCreation PASSED");
}

void Zenith_PhysicsTests::TestCapsuleExplicitDimensions()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_ExplCapsule");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);

	Zenith_Entity xEntity(pxSceneData, "ExplCapsuleEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCapsuleCollider(0.5f, 1.0f, RIGIDBODY_TYPE_DYNAMIC);

	Zenith_Assert(xCollider.HasValidBody(), "TestCapsuleExplicitDimensions: Explicit capsule should have valid body");

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestCapsuleExplicitDimensions PASSED");
}

//==============================================================================
// Cat 9: Physics Timestep
//==============================================================================

void Zenith_PhysicsTests::TestFixedTimestepOneStep()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_OneStep");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "StepSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	// Exactly one fixed timestep
	Zenith_Physics::Update(1.0f / 60.0f);

	// After one step at 60Hz: deltaY = 0.5 * 9.81 * (1/60)^2 ~= 0.00136
	// But velocity after one step: v = 9.81 * (1/60) ~= 0.1635
	// And position: y = 10 - 0.5 * 9.81 * (1/60)^2 ~= 9.9986
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.y < 10.0f, "TestFixedTimestepOneStep: Body should have moved slightly (got %f)", xPos.y);
	Zenith_Assert(xPos.y > 9.9f, "TestFixedTimestepOneStep: Body should not have moved much in 1 step (got %f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestFixedTimestepOneStep PASSED");
}

void Zenith_PhysicsTests::TestAccumulatorDoesNotOverStep()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_NoOverStep");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "NoOverStepSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	// Update with less than one timestep (0.005s < 1/60s ~= 0.01667s)
	Zenith_Physics::Update(0.005f);

	// Accumulator should not have reached threshold - no physics step should occur
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(ApproxEqual(xPos.y, 10.0f), "TestAccumulatorDoesNotOverStep: Body should not have moved (got %f)", xPos.y);

	// Now add enough time to trigger a step
	Zenith_Physics::Update(0.012f); // 0.005 + 0.012 = 0.017 > 1/60

	Zenith_Maths::Vector3 xPosAfter = GetBodyPosition(xCollider);
	Zenith_Assert(xPosAfter.y < 10.0f, "TestAccumulatorDoesNotOverStep: Body should have moved after accumulator crossed threshold (got %f)", xPosAfter.y);

	Zenith_SceneManager::UnloadScene(xTestScene);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestAccumulatorDoesNotOverStep PASSED");
}

void Zenith_PhysicsTests::TestResetClearsPhysicsState()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Reset");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Create a body and step
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ResetSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	StepPhysics(10);

	// Unload scene first (destroys collider components which remove bodies)
	Zenith_SceneManager::UnloadScene(xTestScene);

	// Reset physics (Shutdown + Initialise)
	Zenith_Physics::Reset();

	// Verify physics system is valid after reset
	Zenith_Assert(Zenith_Physics::s_pxPhysicsSystem != nullptr, "TestResetClearsPhysicsState: Physics system should be valid after Reset");

	// Create a new body and verify it works
	Zenith_Scene xTestScene2 = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_Reset2");
	Zenith_SceneData* pxSceneData2 = Zenith_SceneManager::GetSceneData(xTestScene2);
	ResetPhysicsState();

	Zenith_Entity xSphere2 = CreatePhysicsSphere(pxSceneData2, "ResetSphere2",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider2 = xSphere2.GetComponent<Zenith_ColliderComponent>();
	Zenith_Assert(xCollider2.HasValidBody(), "TestResetClearsPhysicsState: New body should be valid after Reset");

	StepPhysics(60);
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider2);
	Zenith_Assert(xPos.y < 10.0f, "TestResetClearsPhysicsState: New body should simulate after Reset (got Y=%f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene2);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestResetClearsPhysicsState PASSED");
}

//==============================================================================
// Cat 10: Scene Cleanup
//==============================================================================

void Zenith_PhysicsTests::TestUnloadSceneDestroysPhysicsBodies()
{
	Zenith_Scene xTestScene = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_UnloadClean");
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xTestScene);
	ResetPhysicsState();

	// Create several physics bodies
	for (int i = 0; i < 10; i++)
	{
		CreatePhysicsSphere(pxSceneData, "UnloadSphere" + std::to_string(i),
			Zenith_Maths::Vector3(static_cast<float>(i), 5, 0), RIGIDBODY_TYPE_DYNAMIC);
	}

	StepPhysics(10);

	// Unload scene - should destroy all bodies without crashing
	Zenith_SceneManager::UnloadScene(xTestScene);

	// Verify physics still works with a fresh scene
	Zenith_Scene xTestScene2 = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_UnloadClean2");
	Zenith_SceneData* pxSceneData2 = Zenith_SceneManager::GetSceneData(xTestScene2);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData2, "PostUnloadSphere",
		Zenith_Maths::Vector3(0, 5, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	Zenith_Assert(xCollider.HasValidBody(), "TestUnloadSceneDestroysPhysicsBodies: Body should be valid after previous scene unloaded");

	StepPhysics(30);
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	Zenith_Assert(xPos.y < 5.0f, "TestUnloadSceneDestroysPhysicsBodies: Physics should work after unload (got Y=%f)", xPos.y);

	Zenith_SceneManager::UnloadScene(xTestScene2);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestUnloadSceneDestroysPhysicsBodies PASSED");
}

void Zenith_PhysicsTests::TestMultipleScenePhysicsIndependence()
{
	Zenith_Scene xSceneA = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_SceneA");
	Zenith_SceneData* pxSceneDataA = Zenith_SceneManager::GetSceneData(xSceneA);

	Zenith_Scene xSceneB = Zenith_SceneManager::CreateEmptyScene("PhysicsTest_SceneB");
	Zenith_SceneData* pxSceneDataB = Zenith_SceneManager::GetSceneData(xSceneB);
	ResetPhysicsState();

	// Sphere in scene A
	Zenith_Entity xSphereA = CreatePhysicsSphere(pxSceneDataA, "SphereA",
		Zenith_Maths::Vector3(-5, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xColliderA = xSphereA.GetComponent<Zenith_ColliderComponent>();

	// Sphere in scene B
	Zenith_Entity xSphereB = CreatePhysicsSphere(pxSceneDataB, "SphereB",
		Zenith_Maths::Vector3(5, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xColliderB = xSphereB.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(30);

	// Both spheres should fall
	Zenith_Maths::Vector3 xPosA = GetBodyPosition(xColliderA);
	Zenith_Maths::Vector3 xPosB = GetBodyPosition(xColliderB);
	Zenith_Assert(xPosA.y < 10.0f, "TestMultipleScenePhysicsIndependence: Sphere A should have fallen (Y=%f)", xPosA.y);
	Zenith_Assert(xPosB.y < 10.0f, "TestMultipleScenePhysicsIndependence: Sphere B should have fallen (Y=%f)", xPosB.y);

	// Unload scene A
	Zenith_SceneManager::UnloadScene(xSceneA);

	// Scene B's sphere should still work
	StepPhysics(30);
	Zenith_Maths::Vector3 xPosBAfter = GetBodyPosition(xColliderB);
	Zenith_Assert(xPosBAfter.y < xPosB.y, "TestMultipleScenePhysicsIndependence: Sphere B should still fall after A unloaded (before=%f, after=%f)", xPosB.y, xPosBAfter.y);

	Zenith_SceneManager::UnloadScene(xSceneB);
	Zenith_Log(LOG_CATEGORY_UNITTEST, "TestMultipleScenePhysicsIndependence PASSED");
}
