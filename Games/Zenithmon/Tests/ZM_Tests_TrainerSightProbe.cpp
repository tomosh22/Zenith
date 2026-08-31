#include "Zenith.h"

// ============================================================================
// ZM_Tests_TrainerSightProbe -- S7 item 3 SC6 unit gate for the OCCLUSION
// FILTER (category ZM_Interaction): ZM_ProbeTrainerSightLine driven against a
// REAL Jolt raycast.
//
// THE RAY IS REAL, NOT MOCKED. Physics is live on the Null backend
// (Zenith_Engine::InitialiseRendererAndPhysics creates it unconditionally), so
// these units cast actual rays at actual bodies in a hermetic scene.
//
// WHY EVERY OCCLUDER HERE IS CREATED BY HAND, and this is BINDING on anyone
// adding a case: TERRAIN IS NOT AN OCCLUDER ON CI. Zenith_ColliderComponent::
// CreateTerrainShape returns nullptr when a terrain has no physics geometry, and
// that geometry lives in Assets/Terrain/Dawnmere/Physics_*.zgeom, which is
// gitignored and NOT committed. An occlusion assertion made against terrain
// would pass on a machine with a warm bake and be vacuous (or red) on a fresh
// checkout. So every body below is an EXPLICITLY CREATED static AABB or dynamic
// capsule inside a local PhysicsSceneScope: no scene file, no baked asset, no
// RequestSkip needed.
//
// The fixture (SceneScope / PhysicsSceneScope / CreateBox / CreateCapsule) is a
// deliberate FILE-LOCAL COPY of ZM_Tests_Overworld.cpp:56-145 -- the original
// lives in that file's anonymous namespace and is not reachable from here.
//
//   1. Probe_ClearLineIsClearAndPhysicsWasActuallyConsulted -- + anti-vacuity.
//   2. Probe_StaticBoxBetweenTrainerAndTargetBlocksTheLine  -- THE occlusion proof.
//   3. Probe_TheTargetsOwnBodyIsNotAnOccluder               -- the id comparison.
//   4. Probe_TheTrainersOwnBodyIsIgnored                    -- the single-body ignore.
//   5. Probe_CoincidentPositionsAreClearWithoutCasting      -- no cast at all.
//   6. Probe_NonFinitePositionFailsClosedAndNeverAsserts    -- totality + polarity.
// ============================================================================

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_TestFramework.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "UnitTests/Zenith_AssertCapture.h"                          // the totality proof
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Zenithmon/Source/Interaction/ZM_TrainerSightProbe.h"

#include <cmath>
#include <limits>

namespace
{
	// A clear 6 m lane down +Z. Every geometric case below is built on it, so a
	// reader only ever has to hold one line in their head.
	const Zenith_Maths::Vector3 xTRAINER_AT_ORIGIN(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xTARGET_SIX_AHEAD(0.0f, 0.0f, 6.0f);

	struct SceneScope
	{
		Zenith_Scene m_xPreviousScene;
		Zenith_Scene m_xScene;
		Zenith_SceneData* m_pxSceneData = nullptr;

		explicit SceneScope(const char* szName)
		{
			m_xPreviousScene = g_xEngine.Scenes().GetActiveScene();
			m_xScene = g_xEngine.Scenes().LoadScene(
				szName, SCENE_LOAD_ADDITIVE_WITHOUT_LOADING);
			m_pxSceneData = g_xEngine.Scenes().GetSceneData(m_xScene);
			if (m_xScene.IsValid())
			{
				g_xEngine.Scenes().SetActiveScene(m_xScene);
			}
		}

		~SceneScope()
		{
			if (m_xPreviousScene.IsValid())
			{
				g_xEngine.Scenes().SetActiveScene(m_xPreviousScene);
			}
			if (m_xScene.IsValid())
			{
				g_xEngine.Scenes().UnloadSceneForced(m_xScene);
			}
		}
	};

	struct PhysicsSceneScope : SceneScope
	{
		bool m_bReady = false;

		explicit PhysicsSceneScope(const char* szName)
			: SceneScope(szName)
		{
			const u_int uLiveColliders =
				g_xEngine.Scenes().QueryAllScenes<Zenith_ColliderComponent>().Count();
			ZENITH_ASSERT_EQ(uLiveColliders, 0u,
				"physics fixture requires an empty collider world before Reset");
			if (m_pxSceneData == nullptr || uLiveColliders != 0u)
			{
				return;
			}

			// Reset is safe only after the zero-live-collider check above. It prevents
			// bodies left by an earlier physics test from affecting this fixture.
			g_xEngine.Physics().Reset();
			g_xEngine.Physics().m_fTimestepAccumulator = 0.0;
			m_bReady = true;
		}
	};

	Zenith_Entity CreateBox(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition,
		const Zenith_Maths::Vector3& xScale,
		RigidBodyType eBodyType)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPosition);
		xTransform.SetScale(xScale);
		xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_AABB, eBodyType);
		return xEntity;
	}

	Zenith_Entity CreateCapsule(
		Zenith_SceneData* pxSceneData,
		const char* szName,
		const Zenith_Maths::Vector3& xPosition)
	{
		Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, szName);
		Zenith_TransformComponent& xTransform =
			xEntity.GetComponent<Zenith_TransformComponent>();
		xTransform.SetPosition(xPosition);
		xTransform.SetScale({ 0.8f, 1.8f, 0.8f });
		xEntity.AddComponent<Zenith_ColliderComponent>().AddCollider(
			COLLISION_VOLUME_TYPE_CAPSULE, RIGIDBODY_TYPE_DYNAMIC);
		return xEntity;
	}
}

// -----------------------------------------------------------------------------
// 1. An empty world is clear -- and physics really was asked
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Probe_ClearLineIsClearAndPhysicsWasActuallyConsulted)
{
	PhysicsSceneScope xFixture("ZM_Probe_ClearLine");
	if (!xFixture.m_bReady) { return; }

	const ZM_TrainerSightProbeResult xResult = ZM_ProbeTrainerSightLine(
		xTRAINER_AT_ORIGIN, INVALID_ENTITY_ID,
		xTARGET_SIX_AHEAD, INVALID_ENTITY_ID);

	ZENITH_ASSERT_TRUE(xResult.m_bClear,
		"an empty physics world put something in the way of a 6 m line");
	ZENITH_ASSERT_FALSE(xResult.m_bBlockerHit,
		"the probe reported a blocker in a world with no bodies at all");
	ZENITH_ASSERT_EQ(xResult.m_xBlockerEntityID, INVALID_ENTITY_ID,
		"the probe named a blocker entity without hitting anything");

	// THE ANTI-VACUITY CLAUSE, and the whole reason m_bPhysicsAvailable exists:
	// without it, "clear" here would be indistinguishable from the no-simulation
	// FAIL-OPEN branch, and every occlusion assertion in this file would be
	// proving nothing at all.
	ZENITH_ASSERT_TRUE(xResult.m_bPhysicsAvailable,
		"physics was NOT consulted -- this 'clear' came from the fail-open branch, "
		"which makes every occlusion unit in this file vacuous");
}

// -----------------------------------------------------------------------------
// 2. THE occlusion proof -- an explicitly created static box, never terrain
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Probe_StaticBoxBetweenTrainerAndTargetBlocksTheLine)
{
	PhysicsSceneScope xFixture("ZM_Probe_BlockedLine");
	if (!xFixture.m_bReady) { return; }

	// Halfway down the lane, wide enough that the ray cannot slip past an edge.
	Zenith_Entity xBlocker = CreateBox(xFixture.m_pxSceneData, "SightBlocker",
		{ 0.0f, 0.0f, 3.0f }, { 4.0f, 4.0f, 0.5f }, RIGIDBODY_TYPE_STATIC);

	const ZM_TrainerSightProbeResult xResult = ZM_ProbeTrainerSightLine(
		xTRAINER_AT_ORIGIN, INVALID_ENTITY_ID,
		xTARGET_SIX_AHEAD, INVALID_ENTITY_ID);

	ZENITH_ASSERT_FALSE(xResult.m_bClear,
		"a static box standing squarely between trainer and target did not block "
		"the sight line -- the occlusion filter is not filtering");
	ZENITH_ASSERT_TRUE(xResult.m_bBlockerHit,
		"the line was reported blocked but no blocker was recorded");
	ZENITH_ASSERT_EQ(xResult.m_xBlockerEntityID, xBlocker.GetEntityID(),
		"the recorded blocker is not the box that was created to do the blocking");
	ZENITH_ASSERT_TRUE(std::isfinite(xResult.m_fBlockerDistance),
		"the blocker distance is non-finite");
	ZENITH_ASSERT_GT(xResult.m_fBlockerDistance, 2.0f,
		"the blocker was reported nearer than the box it must be");
	ZENITH_ASSERT_LT(xResult.m_fBlockerDistance, 4.0f,
		"the blocker was reported further than the box it must be");
}

// -----------------------------------------------------------------------------
// 3. The target's own body terminates the ray and must NOT count as an occluder
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Probe_TheTargetsOwnBodyIsNotAnOccluder)
{
	PhysicsSceneScope xFixture("ZM_Probe_TargetBody");
	if (!xFixture.m_bReady) { return; }

	// A dynamic capsule standing exactly where the player would be, with nothing
	// between it and the trainer. Gravity off so it cannot drift out of the lane.
	Zenith_Entity xTarget = CreateCapsule(
		xFixture.m_pxSceneData, "Target", xTARGET_SIX_AHEAD);
	g_xEngine.Physics().SetGravityEnabled(
		xTarget.GetComponent<Zenith_ColliderComponent>().GetBodyID(), false);

	const ZM_TrainerSightProbeResult xExcused = ZM_ProbeTrainerSightLine(
		xTRAINER_AT_ORIGIN, INVALID_ENTITY_ID,
		xTARGET_SIX_AHEAD, xTarget.GetEntityID());
	ZENITH_ASSERT_TRUE(xExcused.m_bClear,
		"the target's OWN body was treated as an occluder -- Jolt terminates the "
		"ray on it by construction, so this would blind every trainer in the game");
	ZENITH_ASSERT_FALSE(xExcused.m_bBlockerHit,
		"the target's own body was recorded as a blocker");

	// ANTI-VACUITY: the same cast WITHOUT the id must report blocked. Otherwise
	// "clear" above could simply mean the capsule was never hit.
	const ZM_TrainerSightProbeResult xUnexcused = ZM_ProbeTrainerSightLine(
		xTRAINER_AT_ORIGIN, INVALID_ENTITY_ID,
		xTARGET_SIX_AHEAD, INVALID_ENTITY_ID);
	ZENITH_ASSERT_FALSE(xUnexcused.m_bClear,
		"the capsule was never hit at all -- the excused result above proves nothing");
	ZENITH_ASSERT_TRUE(xUnexcused.m_bBlockerHit,
		"the unexcused cast reported blocked without recording the blocker");
	ZENITH_ASSERT_EQ(xUnexcused.m_xBlockerEntityID, xTarget.GetEntityID(),
		"the body that stopped the unexcused ray was not the target capsule");
}

// -----------------------------------------------------------------------------
// 4. The trainer's own body is the ONE body the engine can filter
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Probe_TheTrainersOwnBodyIsIgnored)
{
	PhysicsSceneScope xFixture("ZM_Probe_TrainerBody");
	if (!xFixture.m_bReady) { return; }

	// AT the ray origin: Jolt treats convex shapes as solid from the inside, so an
	// unfiltered cast self-hits at fraction 0. The lane beyond it is empty.
	Zenith_Entity xTrainerBody = CreateBox(xFixture.m_pxSceneData, "TrainerBody",
		xTRAINER_AT_ORIGIN, { 1.0f, 2.0f, 1.0f }, RIGIDBODY_TYPE_STATIC);

	const ZM_TrainerSightProbeResult xIgnored = ZM_ProbeTrainerSightLine(
		xTRAINER_AT_ORIGIN, xTrainerBody.GetEntityID(),
		xTARGET_SIX_AHEAD, INVALID_ENTITY_ID);
	ZENITH_ASSERT_TRUE(xIgnored.m_bClear,
		"the trainer's own body blocked the trainer's own sight line -- the ignore "
		"id is not reaching Zenith_PhysicsQuery::RaycastIgnoring");
	ZENITH_ASSERT_FALSE(xIgnored.m_bBlockerHit,
		"the trainer's own body was recorded as a blocker");

	// ANTI-VACUITY: without the ignore, the self-hit must be real and at ~0 m.
	const ZM_TrainerSightProbeResult xSelfHit = ZM_ProbeTrainerSightLine(
		xTRAINER_AT_ORIGIN, INVALID_ENTITY_ID,
		xTARGET_SIX_AHEAD, INVALID_ENTITY_ID);
	ZENITH_ASSERT_FALSE(xSelfHit.m_bClear,
		"the box at the ray origin was never hit at all -- the ignored result above "
		"proves nothing");
	ZENITH_ASSERT_EQ(xSelfHit.m_xBlockerEntityID, xTrainerBody.GetEntityID(),
		"the body that stopped the unfiltered ray was not the box at the origin");
	ZENITH_ASSERT_EQ_FLOAT(xSelfHit.m_fBlockerDistance, 0.0f, 0.01f,
		"a ray starting inside a convex shape must report distance 0");
}

// -----------------------------------------------------------------------------
// 5. Coincident endpoints: clear, and no cast is issued at all
// -----------------------------------------------------------------------------

ZENITH_TEST(ZM_Interaction, Probe_CoincidentPositionsAreClearWithoutCasting)
{
	PhysicsSceneScope xFixture("ZM_Probe_Coincident");
	if (!xFixture.m_bReady) { return; }

	const Zenith_Maths::Vector3 xSamePlace(2.0f, 0.0f, 2.0f);
	const ZM_TrainerSightProbeResult xResult = ZM_ProbeTrainerSightLine(
		xSamePlace, INVALID_ENTITY_ID, xSamePlace, INVALID_ENTITY_ID);

	// Matches ZM_IsTargetInTrainerSight's own coincident carve-out exactly, so the
	// cone and the filter can never disagree about a player standing ON a trainer.
	ZENITH_ASSERT_TRUE(xResult.m_bClear,
		"two points at the same place had something between them");
	ZENITH_ASSERT_FALSE(xResult.m_bBlockerHit,
		"the coincident carve-out invented a blocker");
	// m_bPhysicsAvailable stays FALSE because the coincident arm returns BEFORE the
	// simulation is ever consulted -- that is what "without casting" means here, and
	// asserting it is how the evaluation ORDER of the answer table is pinned.
	ZENITH_ASSERT_FALSE(xResult.m_bPhysicsAvailable,
		"the coincident early-out issued a raycast instead of answering directly");
}

// -----------------------------------------------------------------------------
// 6. Non-finite input FAILS CLOSED, in every one of the six components, silently
// -----------------------------------------------------------------------------
//
// Written in the local-hit-count form for the two mandatory reasons: (1) NO
// ZENITH_ASSERT_* may appear INSIDE the capture scope -- while one is active
// Zenith_TestRunner::HandleFailure swallows framework failures and merely bumps
// the hit count, so an in-scope assertion could never red this test; (2) the
// count MUST be copied to a local before the closing brace, because
// ~Zenith_AssertCaptureScope restores the previous hit count.
ZENITH_TEST(ZM_Interaction, Probe_NonFinitePositionFailsClosedAndNeverAsserts)
{
	const float afNON_FINITE[3] =
	{
		std::numeric_limits<float>::quiet_NaN(),
		std::numeric_limits<float>::infinity(),
		-std::numeric_limits<float>::infinity()
	};
	// 3 poison values x 6 position components (trainer xyz, then target xyz).
	constexpr u_int uCASE_COUNT = 18u;

	bool abClear[uCASE_COUNT] = {};
	bool abBlockerHit[uCASE_COUNT] = {};
	u_int uHits = 0u;
	{
		Zenith_AssertCaptureScope xCapture;
		u_int uCase = 0u;
		for (u_int uValue = 0u; uValue < 3u; ++uValue)
		{
			for (u_int uComponent = 0u; uComponent < 6u; ++uComponent)
			{
				Zenith_Maths::Vector3 xTrainer = xTRAINER_AT_ORIGIN;
				Zenith_Maths::Vector3 xTarget = xTARGET_SIX_AHEAD;
				if (uComponent < 3u)
				{
					xTrainer[static_cast<int>(uComponent)] = afNON_FINITE[uValue];
				}
				else
				{
					xTarget[static_cast<int>(uComponent - 3u)] = afNON_FINITE[uValue];
				}

				const ZM_TrainerSightProbeResult xResult = ZM_ProbeTrainerSightLine(
					xTrainer, INVALID_ENTITY_ID, xTarget, INVALID_ENTITY_ID);
				abClear[uCase] = xResult.m_bClear;
				abBlockerHit[uCase] = xResult.m_bBlockerHit;
				++uCase;
			}
		}
		uHits = (u_int)xCapture.GetHitCount();
	}

	ZENITH_ASSERT_EQ(uHits, 0u,
		"the occlusion probe asserted on a non-finite position -- Zenith_Assert "
		"breaks in EVERY configuration and the whole unit suite runs at boot, so "
		"one assert here ends the entire gate rather than failing one test");
	for (u_int u = 0u; u < uCASE_COUNT; ++u)
	{
		ZENITH_ASSERT_FALSE(abClear[u],
			"non-finite probe case %u reported a CLEAR sight line -- the probe must "
			"FAIL CLOSED, because one body that goes non-finite must not hand every "
			"trainer in the scene a free line of sight", u);
		ZENITH_ASSERT_FALSE(abBlockerHit[u],
			"non-finite probe case %u invented a blocker it never cast a ray to find", u);
	}
}
