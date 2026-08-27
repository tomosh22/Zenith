#include "Core/Zenith_Engine.h"
// Per-instance collider tests. These exercise the CONCRETE component against a
// live Jolt world, so they live aggregate-side (the Physics leaf may name no
// concrete component) and are hosted by Zenith_InstancedMeshComponent.cpp, whose
// TU is always linked.
#include "Physics/Zenith_Physics.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "DataStream/Zenith_DataStream.h"
#include "Maths/Zenith_Maths.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLockInterface.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>

#include <cmath>
#include <string>

//==============================================================================
// Helpers
//
// The unit batch runs at boot, BEFORE the editor-automation drain, so no game's
// tree components exist yet and every test below owns the whole world.
//==============================================================================
namespace
{
	// Clone of Zenith_Physics.Tests.inl's ResetPhysicsState, with the precondition
	// extended to instance colliders. A Physics::Reset() deletes the whole Jolt
	// PhysicsSystem, so any live component holding body ids across it is left with
	// a ledger of ids that name nothing -- the same stale-BodyID hazard the
	// collider scan exists for, and the same named-assert payoff when it trips.
	void ResetPhysicsForInstancedTests()
	{
		uint32_t uLiveInstanceBodies = 0;
		g_xEngine.Scenes().QueryAllScenes<Zenith_InstancedMeshComponent>().ForEach(
			[&uLiveInstanceBodies](Zenith_EntityID, Zenith_InstancedMeshComponent& xComp)
			{
				uLiveInstanceBodies += xComp.GetInstanceBodyCount();
			});
		ZENITH_ASSERT_EQ(uLiveInstanceBodies, 0u,
			"ResetPhysicsForInstancedTests: %u live instance body/bodies detected across all loaded "
			"scenes. Tearing down the Jolt PhysicsSystem now would leave the owning component's "
			"ledger full of ids that name nothing. Destroy or unload any instance-collider-bearing "
			"scenes BEFORE calling this helper.", uLiveInstanceBodies);

		g_xEngine.Physics().Reset();
		g_xEngine.Physics().m_fTimestepAccumulator = 0;
	}

	uint32_t CountJoltBodies()
	{
		JPH::PhysicsSystem* pxSystem = g_xEngine.Physics().GetJoltSystem();
		return pxSystem != nullptr ? pxSystem->GetNumBodies() : 0u;
	}

	// Reads the capsule's actual dimensions back off the live body. Nothing else
	// can tell "the config was stored" from "the config reached Jolt".
	bool ReadCapsuleDims(Zenith_PhysicsBodyID xBodyID, float& fRadiusOut, float& fHalfCylOut)
	{
		JPH::PhysicsSystem* pxSystem = g_xEngine.Physics().GetJoltSystem();
		if (pxSystem == nullptr || xBodyID.IsInvalid())
		{
			return false;
		}
		JPH::BodyLockRead xLock(pxSystem->GetBodyLockInterface(), JPH::BodyID(xBodyID.m_uID));
		if (!xLock.Succeeded())
		{
			return false;
		}
		const JPH::CapsuleShape* pxCapsule =
			static_cast<const JPH::CapsuleShape*>(xLock.GetBody().GetShape());
		fRadiusOut = pxCapsule->GetRadius();
		fHalfCylOut = pxCapsule->GetHalfHeightOfCylinder();
		return true;
	}

	// A component on its own entity in a scratch scene. No mesh is loaded: the
	// collider path never touches mesh data, and loading one would drag a GPU
	// asset into a unit test for nothing.
	Zenith_InstancedMeshComponent& MakeInstancedComponent(Zenith_SceneData* pxSceneData, const char* szName)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		return xEntity.AddComponent<Zenith_InstancedMeshComponent>();
	}

	// The body pose of one slot, read straight off Jolt through the public
	// wrapper (not through the component, which stores no pose of its own).
	Zenith_Maths::Vector3 InstanceBodyPosition(const Zenith_InstancedMeshComponent& xComp, uint32_t uSlot)
	{
		return g_xEngine.Physics().GetBodyPosition(xComp.GetInstanceBodyID(uSlot));
	}

	Zenith_Maths::Quat InstanceBodyRotation(const Zenith_InstancedMeshComponent& xComp, uint32_t uSlot)
	{
		return g_xEngine.Physics().GetBodyRotation(xComp.GetInstanceBodyID(uSlot));
	}
}

//==============================================================================
// Config + sweep behaviour
//==============================================================================
ZENITH_TEST(InstancedMesh, ConfigDefaultsToNone)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_ConfigDefault", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "Default");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xComp.GetInstanceColliderConfig().m_eType),
		static_cast<uint32_t>(INSTANCE_COLLIDER_TYPE_NONE),
		"a fresh component must author no collider -- NONE is what keeps every existing "
		"instanced-mesh user byte-identical");
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "a fresh component owns no bodies");
	ZENITH_ASSERT_FALSE(xComp.HasInstanceColliders(), "a fresh component reports no colliders");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, SpawnWithCapsuleConfigCreatesBodies)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_SpawnCreates", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "SpawnCreates");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);

	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));

	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 3u, "one body per spawned instance");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 3u,
		"the ledger must agree with the Jolt world -- a ledger-only count would pass even if "
		"nothing reached the simulation");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, SpawnWithoutConfigCreatesNoBodies)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_NoConfig", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "NoConfig");
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));

	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "NONE spawns no bodies");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline, "NONE touches the Jolt world not at all");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, ConfigAfterSpawnSweepCreates)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_SweepCreates", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "SweepCreates");
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "still NONE before the config is set");

	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 3u,
		"setting the config must sweep the ALREADY-spawned instances, not only future ones");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, ClearConfigSweepDestroys)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_ClearConfig", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "ClearConfig");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 3u, "three bodies before the clear");

	xComp.ClearInstanceColliderConfig();
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "clearing the config destroys every body");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline,
		"the bodies must leave the Jolt world too -- an emptied ledger over live bodies is a leak");
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xComp.GetInstanceColliderConfig().m_eType),
		static_cast<uint32_t>(INSTANCE_COLLIDER_TYPE_NONE), "the config resets to NONE");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, ConfigSweepSkipsDespawnedSlots)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_SweepSkips", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "SweepSkips");
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	const uint32_t uMiddle = xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));
	xComp.DespawnInstance(uMiddle);

	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 2u,
		"the sweep walks the ENABLED slots -- a despawned slot must not get a body, or a "
		"deleted tree would keep colliding");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Instance lifetime
//==============================================================================
ZENITH_TEST(InstancedMesh, DespawnDestroysBodyAndRecycleRecreates)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_Recycle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "Recycle");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	const uint32_t uMiddle = xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));

	xComp.DespawnInstance(uMiddle);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 2u, "despawn destroys that instance's body");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 2u, "and removes it from the Jolt world");

	// Flux_InstanceGroup free-lists removed slots, so this MUST come back as the
	// slot just released -- the case where a stale ledger entry would either leak
	// the old body or block the new one.
	const uint32_t uRecycled = xComp.SpawnInstance(Zenith_Maths::Vector3(30.0f, 0.0f, 0.0f));
	ZENITH_ASSERT_EQ(uRecycled, uMiddle, "the group must recycle the freed slot (test premise)");
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 3u, "the recycled slot gets a fresh body");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 3u, "exactly one new Jolt body, not two");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, ClearInstancesDestroysAllBodies)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_ClearInstances", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "ClearInstances");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));

	xComp.ClearInstances();
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "ClearInstances destroys every body");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline, "and the Jolt world is back to baseline");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, ClearThenReconfigureCreatesNoZombieBodies)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_NoZombies", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "NoZombies");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));
	xComp.ClearInstances();

	// Re-configuring after a clear must find NOTHING to build. The config sweep is
	// the one path with no ledger to fall back on (ClearInstances emptied it), so it
	// trusts the group's enabled-slot list -- and that list used to outlive the
	// instances, because Flux_InstanceGroup::Clear() left per-slot flags set. The
	// values differ from the first call so the idempotence guard cannot mask it.
	xComp.SetInstanceColliderCapsule(0.4f, 2.0f, 1.0f);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u,
		"a cleared group has no live instances, so a reconfigure must build no bodies -- "
		"one per stale flag would be an invisible wall at a deleted instance's transform");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline, "and the Jolt world stays at baseline");

	// The second half of the same defect: a zombie body occupying slot 0 would make
	// CreateInstanceBody early-return for the REAL instance that recycles it, so the
	// respawned instance would silently get no collider at all.
	const uint32_t uRespawned = xComp.SpawnInstance(Zenith_Maths::Vector3(3.0f, 0.0f, 4.0f));
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 1u,
		"an instance spawned after the clear must get its own collider");
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xComp, uRespawned),
		Zenith_Maths::Vector3(3.0f, 1.0f, 4.0f), 1e-3f,
		"and it must sit at ITS pose with the NEW config, not a predecessor's");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, SetInstanceEnabledTogglesBody)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_Toggle", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "Toggle");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	const uint32_t uSlot = xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));

	xComp.SetInstanceEnabled(uSlot, false);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 1u, "a disabled instance must not collide");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 1u, "its body leaves the Jolt world");

	xComp.SetInstanceEnabled(uSlot, true);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 2u, "re-enabling restores the body");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 2u, "one body back, not two");

	// Enabling an already-enabled slot must be inert, not a second body. This is
	// the case CreateInstanceBody early-returns for instead of asserting.
	xComp.SetInstanceEnabled(uSlot, true);
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 2u,
		"enabling an already-enabled instance must not create a second body");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, RefreshDoesNotResurrectDisabledInstanceBody)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_NoResurrect", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "NoResurrect");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	const uint32_t uSlot = xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xComp.SetInstanceEnabled(uSlot, false);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "disabling destroys the body (premise)");

	// Moving a DISABLED instance must not bring its collider back. The two creation
	// paths would otherwise disagree: the config sweep filters through the enabled
	// slot list, so a per-slot refresh that ignores the flags produces a collider
	// the sweep would never have made -- an invisible instance that still blocks the
	// player, and one WriteToDataStream (visible slots only) would not serialize, so
	// it exists in the authoring session and vanishes on reload.
	xComp.SetInstanceTransform(uSlot, Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f));
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u,
		"moving a disabled instance must NOT resurrect its collider");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline, "and must not add a Jolt body");

	xComp.SetInstanceMatrix(uSlot, Zenith_Maths::AuthoringTRS(Zenith_Maths::Vector3(9.0f, 0.0f, 9.0f),
		Zenith_Maths::Quat(1.0f, 0.0f, 0.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f)));
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline, "the matrix overload must behave the same");

	// Re-enabling is still the way back, and it uses the CURRENT transform.
	xComp.SetInstanceEnabled(uSlot, true);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 1u, "re-enabling restores exactly one body");
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xComp, uSlot),
		Zenith_Maths::Vector3(9.0f, 3.5f, 9.0f), 1e-3f,
		"and it lands at the pose set while it was disabled");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, ReconfigureRebuildsWithNewDimensions)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_Reconfigure", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "Reconfigure");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	const uint32_t uSlot = xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	const Zenith_PhysicsBodyID xFirstBody = xComp.GetInstanceBodyID(uSlot);

	// Calling the setter AGAIN with identical values must be a genuine no-op -- the
	// editor panel and the tree authoring both re-call it, and a rebuild there would
	// churn one Jolt body per instance for nothing. Body identity is the check that
	// distinguishes "did nothing" from "destroyed and recreated an identical body".
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	ZENITH_ASSERT_TRUE(xComp.GetInstanceBodyID(uSlot) == xFirstBody,
		"an identical reconfigure must not recycle the body -- it must not rebuild at all");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 1u, "and must not change the Jolt body count");

	// A DIFFERENT config must rebuild, and the new dimensions must reach Jolt --
	// storing them without rebuilding would leave the serialized config and the live
	// bodies disagreeing.
	xComp.SetInstanceColliderCapsule(0.6f, 1.5f, 2.0f);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 1u, "still exactly one body after the rebuild");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 1u, "the old body must be destroyed, not leaked");

	float fRadius = 0.0f;
	float fHalfCyl = 0.0f;
	ZENITH_ASSERT_TRUE(ReadCapsuleDims(xComp.GetInstanceBodyID(uSlot), fRadius, fHalfCyl),
		"the rebuilt body must be a readable capsule");
	ZENITH_ASSERT_EQ_FLOAT(fRadius, 0.6f, 1e-4f, "the NEW radius reached Jolt");
	ZENITH_ASSERT_EQ_FLOAT(fHalfCyl, 1.5f, 1e-4f, "the NEW half-height reached Jolt");
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xComp, uSlot),
		Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f), 1e-3f, "and the NEW local Y offset did too");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, SetInstanceTransformMovesBody)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_MoveBody", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "MoveBody");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	const uint32_t uSlot = xComp.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));

	xComp.SetInstanceTransform(uSlot, Zenith_Maths::Vector3(50.0f, 7.0f, -20.0f));
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 1u,
		"a refresh must leave exactly one body -- destroy-then-recreate, not recreate-then-leak");
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 1u, "and exactly one in the Jolt world");

	const Zenith_Maths::Vector3 xBodyPos = InstanceBodyPosition(xComp, uSlot);
	// Config Y offset 3.5, unit scale, identity rotation.
	ZENITH_ASSERT_NEAR_VEC3(xBodyPos, Zenith_Maths::Vector3(50.0f, 10.5f, -20.0f), 1e-3f,
		"the body must follow the instance transform");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Pose + dimensions
//==============================================================================
ZENITH_TEST(InstancedMesh, BodyPoseAppliesTRSAndScale)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_Pose", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "Pose");
	xComp.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);

	const Zenith_Maths::Vector3 xPosition(10.0f, 5.0f, -3.0f);
	const Zenith_Maths::Quat xYaw = Zenith_Maths::AuthoringRotationY(Zenith_Maths::AuthoringRadians(90.0f));
	const Zenith_Maths::Vector3 xScale(2.0f, 1.5f, 2.0f);
	const uint32_t uSlot = xComp.SpawnInstance(xPosition, xYaw, xScale);
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 1u, "one instance, one body");

	// The local +Y offset is scaled then rotated; a yaw leaves +Y alone, so the
	// expected position is a pure Y displacement of 3.5 * 1.5.
	const Zenith_Maths::Vector3 xExpected = xPosition + xYaw * Zenith_Maths::Vector3(0.0f, 3.5f * 1.5f, 0.0f);
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xComp, uSlot), xExpected, 1e-3f,
		"the body sits at the instance origin plus the rotated, scaled local offset");

	// The rotation reaches Jolt too. Compared through the axis it turns, because
	// q and -q are the same rotation and a component-wise check would red on a
	// sign flip that changes nothing.
	const Zenith_Maths::Vector3 xTurnedAxis =
		InstanceBodyRotation(xComp, uSlot) * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	ZENITH_ASSERT_NEAR_VEC3(xTurnedAxis, xYaw * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), 1e-3f,
		"the instance's yaw reaches the body");

	// Dimensions are read back off the SHAPE, not off the config: this is what
	// separates "the numbers were stored" from "the numbers reached Jolt".
	float fRadius = 0.0f;
	float fHalfCyl = 0.0f;
	ZENITH_ASSERT_TRUE(ReadCapsuleDims(xComp.GetInstanceBodyID(uSlot), fRadius, fHalfCyl),
		"the instance body must be a readable capsule");
	ZENITH_ASSERT_EQ_FLOAT(fRadius, 0.3f * 2.0f, 1e-4f,
		"the radius scales by the larger of |scale.x| and |scale.z|");
	ZENITH_ASSERT_EQ_FLOAT(fHalfCyl, 3.2f * 1.5f, 1e-4f, "the cylinder half-height scales by |scale.y|");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Serialization
//==============================================================================
ZENITH_TEST(InstancedMesh, SerializationV5RoundTrip)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_RoundTrip", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	Zenith_InstancedMeshComponent& xSource = MakeInstancedComponent(pxSceneData, "RoundTripSrc");
	xSource.SetInstanceColliderCapsule(0.42f, 2.75f, 1.25f);
	const uint32_t uA = xSource.SpawnInstance(Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f));
	const uint32_t uB = xSource.SpawnInstance(Zenith_Maths::Vector3(-8.0f, 2.0f, 6.0f));
	const Zenith_Maths::Vector3 xPosA = InstanceBodyPosition(xSource, uA);
	const Zenith_Maths::Vector3 xPosB = InstanceBodyPosition(xSource, uB);

	Zenith_DataStream xStream;
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0u);

	Zenith_InstancedMeshComponent& xTarget = MakeInstancedComponent(pxSceneData, "RoundTripDst");
	xTarget.ReadFromDataStream(xStream);

	const Zenith_InstanceColliderConfig& xConfig = xTarget.GetInstanceColliderConfig();
	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xConfig.m_eType),
		static_cast<uint32_t>(INSTANCE_COLLIDER_TYPE_CAPSULE), "the collider type round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xConfig.m_fRadius, 0.42f, 1e-6f, "the radius round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xConfig.m_fCylinderHalfHeight, 2.75f, 1e-6f, "the half-height round-trips");
	ZENITH_ASSERT_EQ_FLOAT(xConfig.m_fLocalYOffset, 1.25f, 1e-6f, "the Y offset round-trips");

	ZENITH_ASSERT_EQ(xTarget.GetInstanceBodyCount(), 2u,
		"deserializing a v5 stream must CREATE the bodies -- storing the config without them "
		"is exactly the state where a loaded scene has trees you walk through");
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xTarget, uA), xPosA, 1e-3f,
		"the reloaded body poses match the originals");
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xTarget, uB), xPosB, 1e-3f,
		"the reloaded body poses match the originals");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, V4StreamReadsAsNone)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_V4Compat", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	// Hand-built v4 blob: version, three empty asset paths, the procedural-material
	// flag, three animation values, an instance count of zero. This is what every
	// committed .zscen written before this change looks like.
	Zenith_DataStream xStream;
	const uint32_t uVersion = 4u;
	xStream << uVersion;
	const std::string strEmpty;
	xStream << strEmpty;   // mesh
	xStream << strEmpty;   // anim texture
	xStream << strEmpty;   // material
	const bool bHasProceduralMaterial = false;
	xStream << bHasProceduralMaterial;
	const float fDuration = 4.0f;
	const float fSpeed = 1.0f;
	const bool bPaused = false;
	xStream << fDuration;
	xStream << fSpeed;
	xStream << bPaused;
	const uint32_t uInstanceCount = 0u;
	xStream << uInstanceCount;
	xStream.SetCursor(0u);

	Zenith_InstancedMeshComponent& xComp = MakeInstancedComponent(pxSceneData, "V4Compat");
	xComp.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(static_cast<uint32_t>(xComp.GetInstanceColliderConfig().m_eType),
		static_cast<uint32_t>(INSTANCE_COLLIDER_TYPE_NONE),
		"a v4 stream carries no collider block, so the config must stay NONE");
	ZENITH_ASSERT_EQ(xComp.GetInstanceBodyCount(), 0u, "and no bodies are created");
	ZENITH_ASSERT_EQ_FLOAT(xComp.GetAnimationDuration(), 4.0f, 1e-6f,
		"the v4 fields BEFORE the new block must still land where they used to -- a misplaced "
		"insertion point corrupts every old scene silently");
	ZENITH_ASSERT_EQ_FLOAT(xComp.GetAnimationSpeed(), 1.0f, 1e-6f, "v4 animation speed reads back");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// Move semantics (R4: both members must transfer AND the source must be emptied)
//==============================================================================
ZENITH_TEST(InstancedMesh, MoveCtorTransfersLedgerNoDoubleFree)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_MoveCtor", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "MoveCtor");
	{
		Zenith_InstancedMeshComponent xB = [&]()
		{
			Zenith_InstancedMeshComponent xA(xEntity);
			xA.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
			xA.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
			xA.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
			return Zenith_InstancedMeshComponent(std::move(xA));
		}();
		// xA is destroyed by now. If the move had left it holding the ledger, its
		// destructor would have taken these bodies with it.
		ZENITH_ASSERT_EQ(xB.GetInstanceBodyCount(), 2u, "the ledger transferred to the target");
		ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 2u,
			"the moved-from destructor must destroy NOTHING -- the bodies belong to the target now");
		ZENITH_ASSERT_EQ(static_cast<uint32_t>(xB.GetInstanceColliderConfig().m_eType),
			static_cast<uint32_t>(INSTANCE_COLLIDER_TYPE_CAPSULE), "the config transferred too");
	}
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline,
		"destroying the target releases both bodies");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(InstancedMesh, MoveAssignDestroysTargetBodiesFirst)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_MoveAssign", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const uint32_t uBaseline = CountJoltBodies();
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "MoveAssign");
	{
		Zenith_InstancedMeshComponent xTarget(xEntity);
		xTarget.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
		xTarget.SpawnInstance(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
		xTarget.SpawnInstance(Zenith_Maths::Vector3(10.0f, 0.0f, 0.0f));
		xTarget.SpawnInstance(Zenith_Maths::Vector3(20.0f, 0.0f, 0.0f));

		{
			Zenith_InstancedMeshComponent xSource(xEntity);
			xSource.SetInstanceColliderCapsule(0.5f, 1.0f, 1.0f);
			xSource.SpawnInstance(Zenith_Maths::Vector3(100.0f, 0.0f, 0.0f));
			ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 4u, "3 target + 1 source before the move");

			xTarget = std::move(xSource);
		}

		ZENITH_ASSERT_EQ(xTarget.GetInstanceBodyCount(), 1u, "the target now owns the source's one body");
		ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline + 1u,
			"the target's OWN three bodies must be destroyed by the assignment -- overwriting the "
			"ledger without destroying them first leaks three Jolt bodies");
		ZENITH_ASSERT_EQ_FLOAT(xTarget.GetInstanceColliderConfig().m_fRadius, 0.5f, 1e-6f,
			"the source's config came across, not the target's");
	}
	ZENITH_ASSERT_EQ(CountJoltBodies(), uBaseline, "everything released at scope exit");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

//==============================================================================
// The decompose the whole thing rests on
//==============================================================================
ZENITH_TEST(InstancedMesh, MatrixSpawnMatchesTRSSpawn)
{
	Zenith_Scene xTestScene = g_xEngine.Scenes().LoadScene("IMC_MatrixSpawn", SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xTestScene);
	ResetPhysicsForInstancedTests();

	const Zenith_Maths::Vector3 xPosition(-14.0f, 3.5f, 22.0f);
	const Zenith_Maths::Quat xYaw = Zenith_Maths::AuthoringRotationY(Zenith_Maths::AuthoringRadians(37.0f));
	const Zenith_Maths::Vector3 xScale(1.4f, 1.1f, 1.4f);

	Zenith_InstancedMeshComponent& xTRS = MakeInstancedComponent(pxSceneData, "MatrixSpawn_TRS");
	xTRS.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	const uint32_t uTRSSlot = xTRS.SpawnInstance(xPosition, xYaw, xScale);

	Zenith_InstancedMeshComponent& xMatrix = MakeInstancedComponent(pxSceneData, "MatrixSpawn_Matrix");
	xMatrix.SetInstanceColliderCapsule(0.3f, 3.2f, 3.5f);
	const uint32_t uMatrixSlot = xMatrix.SpawnInstanceWithMatrix(
		Zenith_Maths::AuthoringTRS(xPosition, xYaw, xScale));

	// The matrix path is what a scene LOAD takes, so this is the test that says
	// DecomposeTRS recovers what BuildMatrix composed.
	ZENITH_ASSERT_NEAR_VEC3(InstanceBodyPosition(xMatrix, uMatrixSlot),
		InstanceBodyPosition(xTRS, uTRSSlot), 1e-4f,
		"a matrix spawn and the equivalent TRS spawn must place the body identically");

	g_xEngine.Scenes().UnloadSceneForced(xTestScene);
}

ZENITH_TEST(Maths, DecomposeTRSRoundTrip)
{
	const float afYaws[3] = { 0.0f, 90.0f, 37.0f };
	const Zenith_Maths::Vector3 axScales[2] =
	{
		Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f),
		Zenith_Maths::Vector3(2.0f, 1.5f, 2.0f),
	};
	const Zenith_Maths::Vector3 xPosition(12.0f, -4.0f, 7.5f);

	for (int iYaw = 0; iYaw < 3; ++iYaw)
	{
		for (int iScale = 0; iScale < 2; ++iScale)
		{
			const Zenith_Maths::Quat xRotation =
				Zenith_Maths::AuthoringRotationY(Zenith_Maths::AuthoringRadians(afYaws[iYaw]));
			const Zenith_Maths::Matrix4 xComposed =
				Zenith_Maths::AuthoringTRS(xPosition, xRotation, axScales[iScale]);

			Zenith_Maths::Vector3 xOutPosition;
			Zenith_Maths::Quat xOutRotation;
			Zenith_Maths::Vector3 xOutScale;
			Zenith_Maths::DecomposeTRS(xComposed, xOutPosition, xOutRotation, xOutScale);

			ZENITH_ASSERT_NEAR_VEC3(xOutPosition, xPosition, 1e-4f,
				"DecomposeTRS: translation (yaw %.0f)", afYaws[iYaw]);
			ZENITH_ASSERT_NEAR_VEC3(xOutScale, axScales[iScale], 1e-4f,
				"DecomposeTRS: scale (yaw %.0f)", afYaws[iYaw]);

			// The quaternion is compared THROUGH the recomposed matrix rather than
			// component-wise: q and -q are the same rotation, so a component-wise
			// check would red on a sign flip that changes nothing.
			const Zenith_Maths::Matrix4 xRecomposed =
				Zenith_Maths::AuthoringTRS(xOutPosition, xOutRotation, xOutScale);
			for (int iCol = 0; iCol < 4; ++iCol)
			{
				for (int iRow = 0; iRow < 4; ++iRow)
				{
					ZENITH_ASSERT_EQ_FLOAT(xRecomposed[iCol][iRow], xComposed[iCol][iRow], 1e-4f,
						"DecomposeTRS: recompose [%d][%d] (yaw %.0f, scale %.1f)",
						iCol, iRow, afYaws[iYaw], axScales[iScale].x);
				}
			}

			ZENITH_ASSERT_EQ_FLOAT(glm::length(xOutRotation), 1.0f, 1e-5f,
				"DecomposeTRS must return a NORMALISED quat -- Jolt asserts IsNormalized()");
		}
	}
}
