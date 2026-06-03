// This file queries Jolt Physics objects - disable memory tracking macro to avoid conflicts
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "Physics/Zenith_Physics.h"
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>


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

	ZENITH_BEHAVIOUR_TYPE_NAME_INTERNAL(PhysicsTestBehaviour)
};

uint32_t PhysicsTestBehaviour::s_uCollisionEnterCount = 0;
uint32_t PhysicsTestBehaviour::s_uCollisionStayCount = 0;
uint32_t PhysicsTestBehaviour::s_uCollisionExitCount = 0;
Zenith_EntityID PhysicsTestBehaviour::s_xLastCollisionEnterOther;
Zenith_EntityID PhysicsTestBehaviour::s_xLastCollisionExitOther;

//==============================================================================
// Helper Functions
//==============================================================================

// ResetPhysicsState — called at the top of every physics test to guarantee a
// pristine Jolt world. Previously only reset the timestep accumulator, which
// was not enough: prior-test static bodies (e.g. the box at (0,5,0) from
// TestStaticBodyDoesNotFall) could persist into the next test and silently
// collide with the new test's dynamic body. The canonical failure mode was
// TestAddImpulseInstantVelocityChange: impulse sphere launched upward from
// (0,0,0) at v=10 would clip the leftover static box around Y=5, decelerate,
// and land at Y=7.13 instead of ~10.
//
// A full Physics::Reset() tears down and rebuilds the Jolt PhysicsSystem, which
// implicitly removes every body regardless of whether ColliderComponent
// destructors ran correctly. Tests that create new colliders AFTER this call
// pick up fresh bodies in the fresh world, so scene state is still consistent.
//
// Safety: this must only be called from tests where no ColliderComponent is
// currently alive across the reset. All physics tests call this at the top,
// right after creating the empty test scene and BEFORE creating any colliders,
// so the invariant holds. Game code must never call this mid-session — it would
// leave live ColliderComponents with stale (invalid) Jolt BodyIDs.
//
// The assertion below enforces that precondition at runtime. Today the function
// is file-static so only this translation unit can reach it, but promoting it
// (or copy-pasting the pattern into a new test file) without the check would
// silently corrupt every live collider. Querying all loaded scenes for existing
// colliders is cheap — tests run a handful of scenes with <50 entities — and
// gives a concrete error message pointing at the failure instead of a mystery
// Jolt assertion when the next frame touches a stale BodyID.
static void ResetPhysicsState()
{
	// Precondition: no live ColliderComponent anywhere in the engine. See the long
	// block-comment above for why — tearing down the Jolt system with live colliders
	// leaves stale BodyIDs that trip asserts on next access. The assertion includes
	// the live count so a trip points straight at the failing test.
	Zenith_Vector<Zenith_ColliderComponent*> axLiveColliders;
	axLiveColliders.Clear();
	g_xEngine.Scenes().QueryAllScenes<Zenith_ColliderComponent>().ForEach([&axLiveColliders](Zenith_EntityID, Zenith_ColliderComponent& xComp) { axLiveColliders.PushBack(&xComp); });
	ZENITH_ASSERT_EQ(axLiveColliders.GetSize(), 0, "ResetPhysicsState: %u live ColliderComponent(s) detected across all loaded scenes. "
		"Tearing down the Jolt PhysicsSystem now would leave every one of them with a stale "
		"BodyID and trip a Jolt assertion on next dtor or method call. Destroy or unload any "
		"collider-bearing scenes BEFORE calling this helper — the convention is to call it "
		"after creating the empty test scene and before AddCollider.", axLiveColliders.GetSize());

	g_xEngine.Physics().Reset();
	g_xEngine.Physics().m_fTimestepAccumulator = 0;
}

static Zenith_Entity CreatePhysicsSphere(
	Zenith_SceneData* pxSceneData,
	const std::string& strName,
	Zenith_Maths::Vector3 xPos,
	RigidBodyType eType,
	float fScale = 0.5f)
{
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, strName);
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
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, strName);
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xPos);
	xEntity.GetComponent<Zenith_TransformComponent>().SetScale(xScale);
	xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(COLLISION_VOLUME_TYPE_AABB, eType);
	return xEntity;
}

static void StepPhysics(uint32_t uSteps)
{
	for (uint32_t i = 0; i < uSteps; i++)
	{
		g_xEngine.Physics().Update(1.0f / 60.0f);
	}
}

static Zenith_Maths::Vector3 GetBodyPosition(const Zenith_ColliderComponent& xCollider)
{
	JPH::BodyInterface& xBI = g_xEngine.Physics().m_pxPhysicsSystem->GetBodyInterface();
	JPH::RVec3 xPos = xBI.GetPosition(xCollider.GetBodyID());
	return Zenith_Maths::Vector3(
		static_cast<float>(xPos.GetX()),
		static_cast<float>(xPos.GetY()),
		static_cast<float>(xPos.GetZ()));
}

[[maybe_unused]] static bool ApproxEqual(float fA, float fB, float fEpsilon = 0.01f)
{
	return std::abs(fA - fB) < fEpsilon;
}

//==============================================================================
// RunAllTests
//==============================================================================

//==============================================================================
// Cat 1: Gravity & Free-Fall
//==============================================================================
ZENITH_TEST(Physics, DynamicBodyFallsUnderGravity)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Gravity", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "FallSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(60);

	// After 1 second: y = 10 - 0.5 * 9.81 * 1^2 ~= 5.1
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_LT(xPos.y, 6.0f, "TestDynamicBodyFallsUnderGravity: Sphere should have fallen below Y=6 (got %f)", xPos.y);
	ZENITH_ASSERT_GT(xPos.y, 4.0f, "TestDynamicBodyFallsUnderGravity: Sphere should not be below Y=4 after 1s (got %f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, StaticBodyDoesNotFall)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_StaticNoFall", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xBox = CreatePhysicsBox(pxSceneData, "StaticBox",
		Zenith_Maths::Vector3(0, 5, 0), Zenith_Maths::Vector3(1, 1, 1),
		RIGIDBODY_TYPE_STATIC);
	Zenith_ColliderComponent& xCollider = xBox.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 5.0f, 0.01f, "TestStaticBodyDoesNotFall: Static body should remain at Y=5 (got %f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, GravityDisabledBodyStaysStill)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_NoGravity", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "NoGravSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 10.0f, 0.01f, "TestGravityDisabledBodyStaysStill: Body should remain at Y=10 (got %f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, GravityReenabledBodyFalls)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_ReenableGrav", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ReGravSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	StepPhysics(30);
	Zenith_Maths::Vector3 xPosBefore = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPosBefore.y, 10.0f, 0.01f, "TestGravityReenabledBodyFalls: Body should be at Y=10 while gravity disabled (got %f)", xPosBefore.y);

	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), true);
	StepPhysics(30);

	Zenith_Maths::Vector3 xPosAfter = GetBodyPosition(xCollider);
	ZENITH_ASSERT_LT(xPosAfter.y, 10.0f, "TestGravityReenabledBodyFalls: Body should have fallen after re-enabling gravity (got %f)", xPosAfter.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 2: Velocity
//==============================================================================
ZENITH_TEST(Physics, SetLinearVelocity)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_LinVel", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "VelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);
	g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(5, 0, 0));

	StepPhysics(60);

	// After 1 second at 5 m/s, X should be ~5.0
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_TRUE(xPos.x > 4.5f && xPos.x < 5.5f, "TestSetLinearVelocity: Expected X~5.0, got %f", xPos.x);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, GetLinearVelocityMatchesSet)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_GetLinVel", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "GetVelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(3, 4, 5));
	Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());

	ZENITH_ASSERT_EQ_FLOAT(xVel.x, 3.0f, 0.01f, "TestGetLinearVelocityMatchesSet: Expected vx=3, got %f", xVel.x);
	ZENITH_ASSERT_EQ_FLOAT(xVel.y, 4.0f, 0.01f, "TestGetLinearVelocityMatchesSet: Expected vy=4, got %f", xVel.y);
	ZENITH_ASSERT_EQ_FLOAT(xVel.z, 5.0f, 0.01f, "TestGetLinearVelocityMatchesSet: Expected vz=5, got %f", xVel.z);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, SetAngularVelocity)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_AngVel", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "AngVelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);
	g_xEngine.Physics().SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 5, 0));

	StepPhysics(1);

	Zenith_Maths::Vector3 xAngVel = g_xEngine.Physics().GetAngularVelocity(xCollider.GetBodyID());
	ZENITH_ASSERT_GT(std::abs(xAngVel.y), 0.1f, "TestSetAngularVelocity: Angular velocity Y should be non-zero (got %f)", xAngVel.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, GetAngularVelocityMatchesSet)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_GetAngVel", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "GetAngVelSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	g_xEngine.Physics().SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(1, 2, 3));
	Zenith_Maths::Vector3 xAngVel = g_xEngine.Physics().GetAngularVelocity(xCollider.GetBodyID());

	ZENITH_ASSERT_EQ_FLOAT(xAngVel.x, 1.0f, 0.1f, "TestGetAngularVelocityMatchesSet: Expected wx=1, got %f", xAngVel.x);
	ZENITH_ASSERT_EQ_FLOAT(xAngVel.y, 2.0f, 0.1f, "TestGetAngularVelocityMatchesSet: Expected wy=2, got %f", xAngVel.y);
	ZENITH_ASSERT_EQ_FLOAT(xAngVel.z, 3.0f, 0.1f, "TestGetAngularVelocityMatchesSet: Expected wz=3, got %f", xAngVel.z);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, ZeroVelocityNoMovement)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_ZeroVel", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ZeroVelSphere",
		Zenith_Maths::Vector3(5, 5, 5), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);
	g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 0, 0));
	g_xEngine.Physics().SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 0, 0));

	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPos.x, 5.0f, 0.01f, "TestZeroVelocityNoMovement: X should remain 5 (got %f)", xPos.x);
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 5.0f, 0.01f, "TestZeroVelocityNoMovement: Y should remain 5 (got %f)", xPos.y);
	ZENITH_ASSERT_EQ_FLOAT(xPos.z, 5.0f, 0.01f, "TestZeroVelocityNoMovement: Z should remain 5 (got %f)", xPos.z);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 3: Forces & Impulses
//==============================================================================
ZENITH_TEST(Physics, AddForceAcceleratesBody)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Force", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ForceSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	// Apply force each frame for 60 frames
	for (uint32_t i = 0; i < 60; i++)
	{
		g_xEngine.Physics().AddForce(xCollider.GetBodyID(), Zenith_Maths::Vector3(100, 0, 0));
		g_xEngine.Physics().Update(1.0f / 60.0f);
	}

	Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
	ZENITH_ASSERT_GT(xVel.x, 0.0f, "TestAddForceAcceleratesBody: X velocity should be positive (got %f)", xVel.x);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_GT(xPos.x, 0.0f, "TestAddForceAcceleratesBody: X position should have increased (got %f)", xPos.x);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, AddImpulseInstantVelocityChange)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Impulse", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ImpulseSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	// AddImpulse uses AddLinearVelocity internally
	g_xEngine.Physics().AddImpulse(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 10, 0));

	Zenith_Maths::Vector3 xVel = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
	ZENITH_ASSERT_EQ_FLOAT(xVel.y, 10.0f, 0.5f, "TestAddImpulseInstantVelocityChange: Expected vy~10, got %f", xVel.y);

	StepPhysics(60);
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_GT(xPos.y, 9.0f, "TestAddImpulseInstantVelocityChange: Body should have moved upward (got Y=%f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, ForceAccumulatesOverFrames)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_ForceAccum", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Body A: apply force for 30 frames
	Zenith_Entity xSphereA = CreatePhysicsSphere(pxSceneData, "ForceA",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xColliderA = xSphereA.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xColliderA.GetBodyID(), false);

	// Body B: apply force for 60 frames
	Zenith_Entity xSphereB = CreatePhysicsSphere(pxSceneData, "ForceB",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xColliderB = xSphereB.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xColliderB.GetBodyID(), false);

	for (uint32_t i = 0; i < 60; i++)
	{
		if (i < 30)
		{
			g_xEngine.Physics().AddForce(xColliderA.GetBodyID(), Zenith_Maths::Vector3(100, 0, 0));
		}
		g_xEngine.Physics().AddForce(xColliderB.GetBodyID(), Zenith_Maths::Vector3(100, 0, 0));
		g_xEngine.Physics().Update(1.0f / 60.0f);
	}

	Zenith_Maths::Vector3 xVelA = g_xEngine.Physics().GetLinearVelocity(xColliderA.GetBodyID());
	Zenith_Maths::Vector3 xVelB = g_xEngine.Physics().GetLinearVelocity(xColliderB.GetBodyID());

	// Body B had force applied for twice as long, so velocity should be roughly double
	ZENITH_ASSERT_GT(xVelB.x, xVelA.x * 1.5f, "TestForceAccumulatesOverFrames: 60-frame body should be faster than 30-frame body (A=%f, B=%f)", xVelA.x, xVelB.x);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, ImpulseOnStaticBodyNoEffect)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_ImpulseStatic", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xBox = CreatePhysicsBox(pxSceneData, "StaticImpulseBox",
		Zenith_Maths::Vector3(0, 5, 0), Zenith_Maths::Vector3(1, 1, 1),
		RIGIDBODY_TYPE_STATIC);
	Zenith_ColliderComponent& xCollider = xBox.GetComponent<Zenith_ColliderComponent>();

	g_xEngine.Physics().AddImpulse(xCollider.GetBodyID(), Zenith_Maths::Vector3(100, 100, 100));
	StepPhysics(60);

	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 5.0f, 0.01f, "TestImpulseOnStaticBodyNoEffect: Static body should remain at Y=5 (got %f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 4: Collision Detection
//==============================================================================
ZENITH_TEST(Physics, DynamicHitsStaticFloor)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_DynHitsFloor", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
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
	ZENITH_ASSERT_GT(xPos.y, 0.0f, "TestDynamicHitsStaticFloor: Sphere should be above the floor (got Y=%f)", xPos.y);
	ZENITH_ASSERT_LT(xPos.y, 3.0f, "TestDynamicHitsStaticFloor: Sphere should have fallen near the floor (got Y=%f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, TwoDynamicBodiesCollide)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_TwoDynCollide", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Two spheres approaching each other horizontally
	Zenith_Entity xSphereA = CreatePhysicsSphere(pxSceneData, "SphereA",
		Zenith_Maths::Vector3(-5, 0, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xColliderA = xSphereA.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xColliderA.GetBodyID(), false);
	g_xEngine.Physics().SetLinearVelocity(xColliderA.GetBodyID(), Zenith_Maths::Vector3(5, 0, 0));

	Zenith_Entity xSphereB = CreatePhysicsSphere(pxSceneData, "SphereB",
		Zenith_Maths::Vector3(5, 0, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xColliderB = xSphereB.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xColliderB.GetBodyID(), false);
	g_xEngine.Physics().SetLinearVelocity(xColliderB.GetBodyID(), Zenith_Maths::Vector3(-5, 0, 0));

	StepPhysics(120);

	// After collision, the spheres should have bounced or stopped
	// They should NOT have passed through each other
	Zenith_Maths::Vector3 xPosA = GetBodyPosition(xColliderA);
	Zenith_Maths::Vector3 xPosB = GetBodyPosition(xColliderB);

	// If they passed through, A would be at ~X=5 and B at ~X=-5 (after 2 seconds at 5m/s)
	// With collision, positions should be closer to the middle
	ZENITH_ASSERT_LT(xPosA.x, 4.0f, "TestTwoDynamicBodiesCollide: SphereA should not have passed through (X=%f)", xPosA.x);
	ZENITH_ASSERT_GT(xPosB.x, -4.0f, "TestTwoDynamicBodiesCollide: SphereB should not have passed through (X=%f)", xPosB.x);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, StaticStaticNoCollision)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_StaticStatic", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
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

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, SphereOnBoxCollision)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_SphereOnBox", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Static floor - AABB with scale (10, 0.5, 10), center at Y=0
	// Half-extents = scale * 0.5, so floor top surface is at Y = 0.25
	CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);

	// Dynamic sphere (scale = 0.5, physics radius = scale * 0.5 = 0.25)
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "Sphere",
		Zenith_Maths::Vector3(0, 5, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	StepPhysics(300);

	// Sphere should rest at approximately: floor_top + sphere_radius = 0.25 + 0.25 = 0.5
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_GT(xPos.y, 0.2f, "TestSphereOnBoxCollision: Sphere should be above floor surface (got Y=%f)", xPos.y);
	ZENITH_ASSERT_LT(xPos.y, 2.0f, "TestSphereOnBoxCollision: Sphere should have settled near floor (got Y=%f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 5: Collision Events
//==============================================================================
ZENITH_TEST(Physics, CollisionEnterCallback)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_CollEnter", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor with script component
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Dynamic sphere that will fall onto the floor
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "FallingSphere",
		Zenith_Maths::Vector3(0, 3, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	xSphere.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Step enough frames for sphere to hit the floor
	StepPhysics(120);

	ZENITH_ASSERT_GT(PhysicsTestBehaviour::s_uCollisionEnterCount, 0, "TestCollisionEnterCallback: Expected collision enter count > 0, got %u", PhysicsTestBehaviour::s_uCollisionEnterCount);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, CollisionStayCallback)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_CollStay", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Dynamic sphere close to floor so it hits quickly
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "StaySphere",
		Zenith_Maths::Vector3(0, 2, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	xSphere.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Step many frames - after initial contact, stay events should fire each frame
	StepPhysics(180);

	ZENITH_ASSERT_GT(PhysicsTestBehaviour::s_uCollisionStayCount, 0, "TestCollisionStayCallback: Expected collision stay count > 0, got %u", PhysicsTestBehaviour::s_uCollisionStayCount);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, CollisionExitCallback)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_CollExit", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Dynamic sphere close to floor
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ExitSphere",
		Zenith_Maths::Vector3(0, 2, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	xSphere.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Step until collision
	StepPhysics(60);

	// Now launch the sphere upward to separate from floor
	g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 20, 0));
	PhysicsTestBehaviour::s_uCollisionExitCount = 0;

	// Step to allow separation
	StepPhysics(60);

	ZENITH_ASSERT_GT(PhysicsTestBehaviour::s_uCollisionExitCount, 0, "TestCollisionExitCallback: Expected collision exit count > 0, got %u", PhysicsTestBehaviour::s_uCollisionExitCount);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, CollisionEventBothEntitiesReceive)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_CollBoth", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();
	PhysicsTestBehaviour::ResetCounters();

	// Static floor with behaviour
	Zenith_Entity xFloor = CreatePhysicsBox(pxSceneData, "Floor",
		Zenith_Maths::Vector3(0, 0, 0), Zenith_Maths::Vector3(10, 0.5f, 10),
		RIGIDBODY_TYPE_STATIC);
	xFloor.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	// Falling sphere with behaviour
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "BothSphere",
		Zenith_Maths::Vector3(0, 3, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	xSphere.AddComponent<Zenith_ScriptComponent>().AddScript<PhysicsTestBehaviour>();

	StepPhysics(120);

	// Both entities should receive OnCollisionEnter, so count >= 2
	ZENITH_ASSERT_GE(PhysicsTestBehaviour::s_uCollisionEnterCount, 2, "TestCollisionEventBothEntitiesReceive: Expected collision enter count >= 2 (got %u)", PhysicsTestBehaviour::s_uCollisionEnterCount);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 6: Raycasting
//==============================================================================
ZENITH_TEST(Physics, RaycastHitsSphere)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_RayHit", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	CreatePhysicsSphere(pxSceneData, "Target",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_STATIC, 1.0f);

	// Step once to ensure body is in the broadphase
	StepPhysics(1);

	Zenith_Physics::RaycastResult xResult = g_xEngine.Physics().Raycast(
		Zenith_Maths::Vector3(0, 0, -10),
		Zenith_Maths::Vector3(0, 0, 1),
		20.0f);

	ZENITH_ASSERT_TRUE(xResult.m_bHit, "TestRaycastHitsSphere: Raycast should have hit the sphere");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, RaycastMissesNoBody)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_RayMiss", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	(void)pxSceneData;
	ResetPhysicsState();

	// Empty scene - raycast should miss
	Zenith_Physics::RaycastResult xResult = g_xEngine.Physics().Raycast(
		Zenith_Maths::Vector3(0, 0, 0),
		Zenith_Maths::Vector3(0, 0, 1),
		100.0f);

	ZENITH_ASSERT_FALSE(xResult.m_bHit, "TestRaycastMissesNoBody: Raycast should not hit anything in empty scene");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, RaycastReturnsHitPoint)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_RayPoint", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Sphere at origin with radius 1
	CreatePhysicsSphere(pxSceneData, "Target",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_STATIC, 1.0f);

	StepPhysics(1);

	Zenith_Physics::RaycastResult xResult = g_xEngine.Physics().Raycast(
		Zenith_Maths::Vector3(0, 0, -10),
		Zenith_Maths::Vector3(0, 0, 1),
		20.0f);

	ZENITH_ASSERT_TRUE(xResult.m_bHit, "TestRaycastReturnsHitPoint: Should hit sphere");
	// Hit point should be on the sphere surface, approximately at Z = -1 (near side)
	ZENITH_ASSERT_LT(xResult.m_xHitPoint.z, 0.0f, "TestRaycastReturnsHitPoint: Hit point Z should be negative (near side), got %f", xResult.m_xHitPoint.z);
	ZENITH_ASSERT_TRUE(xResult.m_fDistance > 8.0f && xResult.m_fDistance < 10.0f, "TestRaycastReturnsHitPoint: Distance should be ~9 (got %f)", xResult.m_fDistance);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, RaycastReturnsHitEntity)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_RayEntity", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xTarget = CreatePhysicsSphere(pxSceneData, "Target",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_STATIC, 1.0f);
	Zenith_EntityID xExpectedID = xTarget.GetEntityID();

	StepPhysics(1);

	Zenith_Physics::RaycastResult xResult = g_xEngine.Physics().Raycast(
		Zenith_Maths::Vector3(0, 0, -10),
		Zenith_Maths::Vector3(0, 0, 1),
		20.0f);

	ZENITH_ASSERT_TRUE(xResult.m_bHit, "TestRaycastReturnsHitEntity: Should hit sphere");
	ZENITH_ASSERT_EQ(xResult.m_xHitEntity, xExpectedID, "TestRaycastReturnsHitEntity: Hit entity ID should match target");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, RaycastMaxDistanceRespected)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_RayMaxDist", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Sphere at Z=20
	CreatePhysicsSphere(pxSceneData, "FarTarget",
		Zenith_Maths::Vector3(0, 0, 20), RIGIDBODY_TYPE_STATIC, 1.0f);

	StepPhysics(1);

	// Short ray should miss
	Zenith_Physics::RaycastResult xShortResult = g_xEngine.Physics().Raycast(
		Zenith_Maths::Vector3(0, 0, 0),
		Zenith_Maths::Vector3(0, 0, 1),
		5.0f);
	ZENITH_ASSERT_FALSE(xShortResult.m_bHit, "TestRaycastMaxDistanceRespected: Short ray should miss");

	// Long ray should hit
	Zenith_Physics::RaycastResult xLongResult = g_xEngine.Physics().Raycast(
		Zenith_Maths::Vector3(0, 0, 0),
		Zenith_Maths::Vector3(0, 0, 1),
		25.0f);
	ZENITH_ASSERT_TRUE(xLongResult.m_bHit, "TestRaycastMaxDistanceRespected: Long ray should hit");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 7: Body Configuration
//==============================================================================
ZENITH_TEST(Physics, LockRotationPreventsAngularVelocity)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_LockRot", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "LockRotSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	// Set angular velocity first, then lock - LockRotation zeroes velocity on locked axes
	g_xEngine.Physics().SetAngularVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(10, 10, 10));
	g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, true, true);
	StepPhysics(10);

	Zenith_Maths::Vector3 xAngVel = g_xEngine.Physics().GetAngularVelocity(xCollider.GetBodyID());
	float fTotalAngVel = std::abs(xAngVel.x) + std::abs(xAngVel.y) + std::abs(xAngVel.z);
	ZENITH_ASSERT_LT(fTotalAngVel, 1.0f, "TestLockRotationPreventsAngularVelocity: Angular velocity should be near zero (got total=%f)", fTotalAngVel);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, ColliderHasValidBodyAfterAdd)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_ValidBody", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "ValidBodyEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

	ZENITH_ASSERT_TRUE(xCollider.HasValidBody(), "TestColliderHasValidBodyAfterAdd: Collider should have valid body");
	ZENITH_ASSERT_FALSE(xCollider.GetBodyID().IsInvalid(), "TestColliderHasValidBodyAfterAdd: Body ID should not be invalid");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, ColliderBodyIDMatchesJolt)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_BodyIDJolt", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "JoltBodyEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

	// Verify body exists in Jolt
	JPH::BodyInterface& xBI = g_xEngine.Physics().m_pxPhysicsSystem->GetBodyInterface();
	bool bIsAdded = xBI.IsAdded(xCollider.GetBodyID());
	ZENITH_ASSERT_TRUE(bIsAdded, "TestColliderBodyIDMatchesJolt: Body should be added to Jolt physics system");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, RebuildColliderPreservesVelocity)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Rebuild", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "RebuildSphere",
		Zenith_Maths::Vector3(0, 0, 0), RIGIDBODY_TYPE_DYNAMIC, 0.5f);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);

	g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(5, 0, 0));

	// Rebuild the collider (e.g. after scale change)
	xCollider.RebuildCollider();

	Zenith_Maths::Vector3 xVelAfter = g_xEngine.Physics().GetLinearVelocity(xCollider.GetBodyID());
	ZENITH_ASSERT_EQ_FLOAT(xVelAfter.x, 5.0f, 0.5f, "TestRebuildColliderPreservesVelocity: Velocity X should be preserved (~5, got %f)", xVelAfter.x);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 8: Collider Shape Types
//==============================================================================
ZENITH_TEST(Physics, AABBColliderCreation)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_AABB", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "AABBEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_AABB, RIGIDBODY_TYPE_DYNAMIC);

	ZENITH_ASSERT_TRUE(xCollider.HasValidBody(), "TestAABBColliderCreation: AABB collider should have valid body");
	ZENITH_ASSERT_EQ(xCollider.GetRigidBodyType(), RIGIDBODY_TYPE_DYNAMIC, "TestAABBColliderCreation: Should be dynamic");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, SphereColliderCreation)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Sphere", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "SphereEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_SPHERE, RIGIDBODY_TYPE_DYNAMIC);

	ZENITH_ASSERT_TRUE(xCollider.HasValidBody(), "TestSphereColliderCreation: Sphere collider should have valid body");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, CapsuleColliderCreation)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Capsule", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "CapsuleEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCollider(COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);

	ZENITH_ASSERT_TRUE(xCollider.HasValidBody(), "TestCapsuleColliderCreation: Capsule collider should have valid body");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, CapsuleExplicitDimensions)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_ExplCapsule", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);

	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "ExplCapsuleEntity");
	Zenith_ColliderComponent& xCollider = xEntity.AddComponent<Zenith_ColliderComponent>();
	xCollider.AddCapsuleCollider(0.5f, 1.0f, RIGIDBODY_TYPE_DYNAMIC);

	ZENITH_ASSERT_TRUE(xCollider.HasValidBody(), "TestCapsuleExplicitDimensions: Explicit capsule should have valid body");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Cat 9: Physics Timestep
//==============================================================================
ZENITH_TEST(Physics, FixedTimestepOneStep)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_OneStep", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "StepSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	// Exactly one fixed timestep
	g_xEngine.Physics().Update(1.0f / 60.0f);

	// After one step at 60Hz: deltaY = 0.5 * 9.81 * (1/60)^2 ~= 0.00136
	// But velocity after one step: v = 9.81 * (1/60) ~= 0.1635
	// And position: y = 10 - 0.5 * 9.81 * (1/60)^2 ~= 9.9986
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_LT(xPos.y, 10.0f, "TestFixedTimestepOneStep: Body should have moved slightly (got %f)", xPos.y);
	ZENITH_ASSERT_GT(xPos.y, 9.9f, "TestFixedTimestepOneStep: Body should not have moved much in 1 step (got %f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, AccumulatorDoesNotOverStep)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_NoOverStep", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "NoOverStepSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();

	// Update with less than one timestep (0.005s < 1/60s ~= 0.01667s)
	g_xEngine.Physics().Update(0.005f);

	// Accumulator should not have reached threshold - no physics step should occur
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPos.y, 10.0f, 0.01f, "TestAccumulatorDoesNotOverStep: Body should not have moved (got %f)", xPos.y);

	// Now add enough time to trigger a step
	g_xEngine.Physics().Update(0.012f); // 0.005 + 0.012 = 0.017 > 1/60

	Zenith_Maths::Vector3 xPosAfter = GetBodyPosition(xCollider);
	ZENITH_ASSERT_LT(xPosAfter.y, 10.0f, "TestAccumulatorDoesNotOverStep: Body should have moved after accumulator crossed threshold (got %f)", xPosAfter.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
ZENITH_TEST(Physics, ResetClearsPhysicsState)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_Reset", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Create a body and step
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "ResetSphere",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	StepPhysics(10);

	// Unload scene first (destroys collider components which remove bodies)
	g_xEngine.Scenes().UnloadSceneForced(xTestScene);

	// Reset physics (Shutdown + Initialise)
	g_xEngine.Physics().Reset();

	// Verify physics system is valid after reset
	ZENITH_ASSERT_NOT_NULL(g_xEngine.Physics().m_pxPhysicsSystem, "TestResetClearsPhysicsState: Physics system should be valid after Reset");

	// Create a new body and verify it works
	Zenith_Scene xTestScene2 = g_xEngine.Scenes().LoadScene("PhysicsTest_Reset2", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData2 = g_xEngine.Scenes().GetSceneData(xTestScene2);
	ResetPhysicsState();

	Zenith_Entity xSphere2 = CreatePhysicsSphere(pxSceneData2, "ResetSphere2",
		Zenith_Maths::Vector3(0, 10, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider2 = xSphere2.GetComponent<Zenith_ColliderComponent>();
	ZENITH_ASSERT_TRUE(xCollider2.HasValidBody(), "TestResetClearsPhysicsState: New body should be valid after Reset");

	StepPhysics(60);
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider2);
	ZENITH_ASSERT_LT(xPos.y, 10.0f, "TestResetClearsPhysicsState: New body should simulate after Reset (got Y=%f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene2);
}

//==============================================================================
// Cat 10: Scene Cleanup
//==============================================================================
ZENITH_TEST(Physics, UnloadSceneDestroysPhysicsBodies)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_UnloadClean", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Create several physics bodies
	for (int i = 0; i < 10; i++)
	{
		CreatePhysicsSphere(pxSceneData, "UnloadSphere" + std::to_string(i),
			Zenith_Maths::Vector3(static_cast<float>(i), 5, 0), RIGIDBODY_TYPE_DYNAMIC);
	}

	StepPhysics(10);

	// Unload scene - should destroy all bodies without crashing
	g_xEngine.Scenes().UnloadSceneForced(xTestScene);

	// Verify physics still works with a fresh scene
	Zenith_Scene xTestScene2 = g_xEngine.Scenes().LoadScene("PhysicsTest_UnloadClean2", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData2 = g_xEngine.Scenes().GetSceneData(xTestScene2);
	ResetPhysicsState();

	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData2, "PostUnloadSphere",
		Zenith_Maths::Vector3(0, 5, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	ZENITH_ASSERT_TRUE(xCollider.HasValidBody(), "TestUnloadSceneDestroysPhysicsBodies: Body should be valid after previous scene unloaded");

	StepPhysics(30);
	Zenith_Maths::Vector3 xPos = GetBodyPosition(xCollider);
	ZENITH_ASSERT_LT(xPos.y, 5.0f, "TestUnloadSceneDestroysPhysicsBodies: Physics should work after unload (got Y=%f)", xPos.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene2);
}
ZENITH_TEST(Physics, MultipleScenePhysicsIndependence)
{
	Zenith_Scene xSceneA = g_xEngine.Scenes().LoadScene("PhysicsTest_SceneA", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneDataA = g_xEngine.Scenes().GetSceneData(xSceneA);

	Zenith_Scene xSceneB = g_xEngine.Scenes().LoadScene("PhysicsTest_SceneB", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneDataB = g_xEngine.Scenes().GetSceneData(xSceneB);
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
	ZENITH_ASSERT_LT(xPosA.y, 10.0f, "TestMultipleScenePhysicsIndependence: Sphere A should have fallen (Y=%f)", xPosA.y);
	ZENITH_ASSERT_LT(xPosB.y, 10.0f, "TestMultipleScenePhysicsIndependence: Sphere B should have fallen (Y=%f)", xPosB.y);

	// Unload scene A
	g_xEngine.Scenes().UnloadSceneForced(xSceneA);

	// Scene B's sphere should still work
	StepPhysics(30);
	Zenith_Maths::Vector3 xPosBAfter = GetBodyPosition(xColliderB);
	ZENITH_ASSERT_LT(xPosBAfter.y, xPosB.y, "TestMultipleScenePhysicsIndependence: Sphere B should still fall after A unloaded (before=%f, after=%f)", xPosB.y, xPosBAfter.y);

	g_xEngine.Scenes().UnloadSceneForced(xSceneB);
}

//==============================================================================
// Cat 11: Gravity Toggle + Impulse Launch
//==============================================================================
ZENITH_TEST(Physics, GravityOffThenImpulseLaunch)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("PhysicsTest_GravImpulseLaunch", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsState();

	// Create a dynamic sphere at Y=5 with gravity OFF (simulating a ball resting on a plunger)
	Zenith_Entity xSphere = CreatePhysicsSphere(pxSceneData, "LaunchSphere",
		Zenith_Maths::Vector3(0, 5, 0), RIGIDBODY_TYPE_DYNAMIC);
	Zenith_ColliderComponent& xCollider = xSphere.GetComponent<Zenith_ColliderComponent>();
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), false);
	g_xEngine.Physics().LockRotation(xCollider.GetBodyID(), true, true, true);

	// Simulate the "resting on plunger" phase: zero velocity each frame for 30 frames
	for (uint32_t i = 0; i < 30; i++)
	{
		g_xEngine.Physics().SetLinearVelocity(xCollider.GetBodyID(), Zenith_Maths::Vector3(0.f));
		g_xEngine.Physics().Update(1.0f / 60.0f);
	}

	// Ball should still be at Y=5 after being held in place
	Zenith_Maths::Vector3 xPosHeld = GetBodyPosition(xCollider);
	ZENITH_ASSERT_EQ_FLOAT(xPosHeld.y, 5.0f, 0.01f, "TestGravityOffThenImpulseLaunch: Ball should remain at Y=5 while held (got %f)", xPosHeld.y);

	// Now simulate launch: enable gravity, apply upward impulse
	g_xEngine.Physics().SetGravityEnabled(xCollider.GetBodyID(), true);
	g_xEngine.Physics().AddImpulse(xCollider.GetBodyID(), Zenith_Maths::Vector3(0, 20, 0));

	// Step a few frames — ball should rise above launch position
	StepPhysics(10);
	Zenith_Maths::Vector3 xPosRising = GetBodyPosition(xCollider);
	ZENITH_ASSERT_GT(xPosRising.y, 5.0f, "TestGravityOffThenImpulseLaunch: Ball should rise above Y=5 after impulse (got %f)", xPosRising.y);

	// Step many more frames — gravity should pull ball back below launch height
	StepPhysics(300);
	Zenith_Maths::Vector3 xPosFallen = GetBodyPosition(xCollider);
	ZENITH_ASSERT_LT(xPosFallen.y, 5.0f, "TestGravityOffThenImpulseLaunch: Ball should fall below Y=5 after gravity takes over (got %f)", xPosFallen.y);

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}
