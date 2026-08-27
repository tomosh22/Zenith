#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * RT_TreeCollision -- the player collides with RenderTest's instanced trees.
 *
 * The trunk group (TerrainTrees_Trunk) authors a per-instance capsule collider
 * (Zenith_TerrainEditor_Trees.cpp), which serializes into RenderTest.zscen at
 * InstancedMesh v5 and comes back as one static Jolt body per live instance on
 * load. Nothing about that is visible in a render, and every unit that could
 * notice runs against a synthetic component -- so this test drives the REAL
 * scene: it walks the real player into a real tree on the real input path and
 * asserts it cannot get through.
 *
 * The assertions are ANDed and each carries a reason, because "blocked" alone
 * is vacuous: a player that never moved is also never inside a tree. `walked`
 * and `reached` are what make the block mean something.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "Input/Zenith_Input.h"
#include "Input/Zenith_InputSimulator.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
// A GAME translation unit may include Flux directly -- the layering ratchet
// scopes Zenith/ only. Needed for the enabled-slot list + transforms.
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"

#include <cmath>
#include <cstdint>

namespace
{
	enum class TreePhase
	{
		Boot, WaitReady, PickTarget, Teleport, Settle, Approach, RayProbe, Done
	};

	TreePhase g_eTreePhase = TreePhase::Boot;
	int       g_iTreePhaseFrame = 0;

	// Non-vacuity anchors, captured in WaitReady.
	uint32_t  g_uTrunkInstances = 0;
	uint32_t  g_uTrunkBodies = 0;
	uint32_t  g_uLeafBodies = 0;
	bool      g_bTreesResolved = false;

	// The chosen trunk.
	Zenith_Maths::Vector3 g_xTreePos(0.0f, 0.0f, 0.0f);
	float g_fTreeScaleXZ = 1.0f;
	float g_fBlockRadius = 0.0f;
	bool  g_bTargetPicked = false;

	// The walk.
	float g_fInitialDist = 0.0f;
	float g_fMinDist = 0.0f;
	float g_fFinalDist = 0.0f;
	bool  g_bWalked = false;

	// The probe.
	bool           g_bRayHit = false;
	Zenith_EntityID g_xRayEntity = INVALID_ENTITY_ID;
	Zenith_EntityID g_xTrunkEntity = INVALID_ENTITY_ID;

	bool g_bTreeDone = false;

	// The authored trunk collider radius (Zenith_TerrainEditor_Trees.cpp) and the
	// player's own capsule radius (RenderTest_PlayerComponent). Their sum is how
	// close the player's CENTRE can get to the trunk axis before the two capsules
	// are in contact -- which is what "blocked" has to be measured against.
	constexpr float fAUTHORED_TRUNK_RADIUS = 0.30f;
	constexpr float fPLAYER_CAPSULE_RADIUS = 0.10f;

	Zenith_Entity Tree_FindEntity(const char* szName)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}
		return pxSceneData->FindEntityByName(szName);
	}

	bool Tree_GetEntityPos(const char* szName, Zenith_Maths::Vector3& xOut)
	{
		Zenith_Entity xEntity = Tree_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return false;
		}
		Zenith_TransformComponent* pxTransform = xEntity.TryGetComponent<Zenith_TransformComponent>();
		if (pxTransform == nullptr)
		{
			return false;
		}
		pxTransform->GetPosition(xOut);
		return true;
	}

	Zenith_InstancedMeshComponent* Tree_FindGroup(const char* szName)
	{
		Zenith_Entity xEntity = Tree_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return nullptr;
		}
		return xEntity.TryGetComponent<Zenith_InstancedMeshComponent>();
	}

	float Tree_HorizontalDistanceToAxis(const Zenith_Maths::Vector3& xPoint)
	{
		const float fDX = xPoint.x - g_xTreePos.x;
		const float fDZ = xPoint.z - g_xTreePos.z;
		return std::sqrt(fDX * fDX + fDZ * fDZ);
	}

	// ★ SetKeyHeld is NOT usable for MOVE and its absence would be silent: it
	// writes the simulator's LEVEL array only, while the player's movement goes
	// through the MOVE ACTION, whose key rows are fed by ordered TRANSITIONS.
	// So the steer publishes real down/up EDGES, and only on CHANGE -- one press
	// and one release per hold, which is what a hand does and what the action
	// layer's replay expects. (Copied verbatim from RT_PlayerActions.)
	const Zenith_KeyCode g_aeTreeMoveKeys[4] =
	{
		ZENITH_KEY_W, ZENITH_KEY_S, ZENITH_KEY_A, ZENITH_KEY_D
	};
	bool g_abTreeMoveKeyHeld[4] = { false, false, false, false };

	void Tree_SetMoveKeyHeld(int iKey, bool bHeld)
	{
		if (g_abTreeMoveKeyHeld[iKey] == bHeld)
		{
			return;
		}
		g_abTreeMoveKeyHeld[iKey] = bHeld;
		if (bHeld)
		{
			Zenith_InputSimulator::SimulateKeyDown(g_aeTreeMoveKeys[iKey]);
		}
		else
		{
			Zenith_InputSimulator::SimulateKeyUp(g_aeTreeMoveKeys[iKey]);
		}
	}

	void Tree_ReleaseMovementKeys()
	{
		for (int i = 0; i < 4; ++i)
		{
			Tree_SetMoveKeyHeld(i, false);
		}
	}

	// The enabled trunk instance nearest the player's XZ, decomposed.
	bool Tree_PickNearestTrunk(const Zenith_Maths::Vector3& xPlayerPos)
	{
		Zenith_InstancedMeshComponent* pxTrunk = Tree_FindGroup("TerrainTrees_Trunk");
		if (pxTrunk == nullptr || pxTrunk->GetInstanceGroup() == nullptr)
		{
			return false;
		}
		const Flux_InstanceGroup* pxGroup = pxTrunk->GetInstanceGroup();
		Zenith_Vector<uint32_t> xEnabled;
		pxGroup->ComputeVisibleIndices(xEnabled);
		if (xEnabled.GetSize() == 0)
		{
			return false;
		}

		const Zenith_Vector<Zenith_Maths::Matrix4>& axTransforms = pxGroup->GetTransforms();
		float fBestDistSq = -1.0f;
		uint32_t uBestSlot = 0;
		for (uint32_t u = 0; u < xEnabled.GetSize(); ++u)
		{
			const Zenith_Maths::Matrix4& xM = axTransforms.Get(xEnabled.Get(u));
			const float fDX = xM[3].x - xPlayerPos.x;
			const float fDZ = xM[3].z - xPlayerPos.z;
			const float fDistSq = fDX * fDX + fDZ * fDZ;
			if (fBestDistSq < 0.0f || fDistSq < fBestDistSq)
			{
				fBestDistSq = fDistSq;
				uBestSlot = xEnabled.Get(u);
			}
		}

		Zenith_Maths::Quat xRotation;
		Zenith_Maths::Vector3 xScale;
		Zenith_Maths::DecomposeTRS(axTransforms.Get(uBestSlot), g_xTreePos, xRotation, xScale);
		g_fTreeScaleXZ = std::max(std::abs(xScale.x), std::abs(xScale.z));
		g_fBlockRadius = fAUTHORED_TRUNK_RADIUS * g_fTreeScaleXZ + fPLAYER_CAPSULE_RADIUS;
		return true;
	}

	// Ground under an XZ, from the terrain's own COLLISION surface -- exactly
	// what a resting capsule sits on. Falls back to the tree's own base when the
	// terrain has no physics geometry (a cold bake), which is a legitimate dev
	// state rather than a test failure.
	float Tree_GroundHeightAt(float fX, float fZ, float fFallback)
	{
		float fHeight = 0.0f;
		bool bFound = false;
		g_xEngine.Scenes().QueryActiveScene<Zenith_TerrainComponent>().ForEach(
			[&](Zenith_EntityID, Zenith_TerrainComponent& xTerrain)
			{
				if (!bFound && xTerrain.TryGetGroundHeightAt(fX, fZ, fHeight))
				{
					bFound = true;
				}
			});
		return bFound ? fHeight : fFallback;
	}

	Zenith_PhysicsBodyID Tree_PlayerBodyID()
	{
		Zenith_Entity xPlayer = Tree_FindEntity("Player");
		if (!xPlayer.IsValid())
		{
			return Zenith_PhysicsBodyID();
		}
		Zenith_ColliderComponent* pxCollider = xPlayer.TryGetComponent<Zenith_ColliderComponent>();
		return pxCollider != nullptr ? pxCollider->GetBodyID() : Zenith_PhysicsBodyID();
	}
}

static void Setup_TreeCollision()
{
	Zenith_InputSimulator::SetFixedDt(1.0f / 60.0f);
	// The harness normalised BOTH sides at the test boundary, so the edge
	// tracker must start from the same all-released state they did -- a stale
	// "held" entry silently suppresses the down edge that starts the walk.
	for (int i = 0; i < 4; ++i)
	{
		g_abTreeMoveKeyHeld[i] = false;
	}
	g_eTreePhase = TreePhase::Boot;
	g_iTreePhaseFrame = 0;
	g_uTrunkInstances = 0;
	g_uTrunkBodies = 0;
	g_uLeafBodies = 0;
	g_bTreesResolved = false;
	g_xTreePos = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	g_fTreeScaleXZ = 1.0f;
	g_fBlockRadius = 0.0f;
	g_bTargetPicked = false;
	g_fInitialDist = 0.0f;
	g_fMinDist = 0.0f;
	g_fFinalDist = 0.0f;
	g_bWalked = false;
	g_bRayHit = false;
	g_xRayEntity = INVALID_ENTITY_ID;
	g_xTrunkEntity = INVALID_ENTITY_ID;
	g_bTreeDone = false;
}

static bool Step_TreeCollision(int iFrame)
{
	switch (g_eTreePhase)
	{
	case TreePhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eTreePhase = TreePhase::WaitReady;
		return true;

	case TreePhase::WaitReady:
	{
		Zenith_Entity xPlayer = Tree_FindEntity("Player");
		Zenith_InstancedMeshComponent* pxTrunk = Tree_FindGroup("TerrainTrees_Trunk");
		Zenith_InstancedMeshComponent* pxLeaves = Tree_FindGroup("TerrainTrees_Leaves");
		if (xPlayer.IsValid() && pxTrunk != nullptr && pxLeaves != nullptr &&
			pxTrunk->GetInstanceCount() > 0)
		{
			g_xTrunkEntity = pxTrunk->GetParentEntity().GetEntityID();
			g_uTrunkInstances = pxTrunk->GetInstanceCount();
			g_uTrunkBodies = pxTrunk->GetInstanceBodyCount();
			g_uLeafBodies = pxLeaves->GetInstanceBodyCount();
			g_bTreesResolved = true;
			g_eTreePhase = TreePhase::PickTarget;
		}
		return iFrame < 900;
	}

	case TreePhase::PickTarget:
	{
		Zenith_Maths::Vector3 xPlayerPos;
		if (!Tree_GetEntityPos("Player", xPlayerPos))
		{
			return iFrame < 960;
		}
		g_bTargetPicked = Tree_PickNearestTrunk(xPlayerPos);
		g_eTreePhase = g_bTargetPicked ? TreePhase::Teleport : TreePhase::Done;
		return true;
	}

	case TreePhase::Teleport:
	{
		// SETUP, not gameplay movement: the tree rings sit ~110 m from the player
		// spawn, which is not a reasonable input-sim walk. The approach itself is
		// still driven entirely through the real input path below.
		const float fApproachX = g_xTreePos.x;
		const float fApproachZ = g_xTreePos.z - 8.0f;
		const float fGroundY = Tree_GroundHeightAt(fApproachX, fApproachZ, g_xTreePos.y + 2.5f);
		const Zenith_PhysicsBodyID xBodyID = Tree_PlayerBodyID();
		if (xBodyID.IsInvalid())
		{
			g_eTreePhase = TreePhase::Done;
			return true;
		}
		// Clearance above the rest pose: an authored DYNAMIC body dropped exactly
		// on the surface can burst the substep budget (Physics/CLAUDE.md).
		g_xEngine.Physics().TeleportBody(xBodyID,
			Zenith_Maths::Vector3(fApproachX, fGroundY + 1.6f, fApproachZ));
		g_iTreePhaseFrame = 0;
		g_eTreePhase = TreePhase::Settle;
		return true;
	}

	case TreePhase::Settle:
		if (++g_iTreePhaseFrame >= 45)
		{
			g_iTreePhaseFrame = 0;
			g_eTreePhase = TreePhase::Approach;
		}
		return true;

	case TreePhase::Approach:
	{
		Zenith_Maths::Vector3 xPlayerPos;
		if (!Tree_GetEntityPos("Player", xPlayerPos))
		{
			return iFrame < 2000;
		}
		const float fDist = Tree_HorizontalDistanceToAxis(xPlayerPos);
		if (g_iTreePhaseFrame == 0)
		{
			g_fInitialDist = fDist;
			g_fMinDist = fDist;
		}
		g_fMinDist = std::min(g_fMinDist, fDist);
		g_bWalked = true;

		// Camera yaw starts 0 (GameplayState::Reset in the player's OnAwake) and
		// no mouse input arrives, so camera-relative movement maps W->+Z, D->+X.
		const float fDX = g_xTreePos.x - xPlayerPos.x;
		const float fDZ = g_xTreePos.z - xPlayerPos.z;
		Tree_SetMoveKeyHeld(0, fDZ >  0.15f);   // W
		Tree_SetMoveKeyHeld(1, fDZ < -0.15f);   // S
		Tree_SetMoveKeyHeld(3, fDX >  0.15f);   // D
		Tree_SetMoveKeyHeld(2, fDX < -0.15f);   // A

		// NO early exit on arrival: the point is a SUSTAINED block, so the player
		// keeps pushing into the trunk for the whole budget. Leaving as soon as it
		// got close would measure a moment, not a wall.
		if (++g_iTreePhaseFrame >= 360)
		{
			Tree_ReleaseMovementKeys();
			g_fFinalDist = fDist;
			g_eTreePhase = TreePhase::RayProbe;
		}
		return true;
	}

	case TreePhase::RayProbe:
	{
		Zenith_Maths::Vector3 xPlayerPos;
		if (!Tree_GetEntityPos("Player", xPlayerPos))
		{
			g_eTreePhase = TreePhase::Done;
			return true;
		}
		const Zenith_Maths::Vector3 xOrigin = xPlayerPos + Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f);
		Zenith_Maths::Vector3 xToTree(g_xTreePos.x - xPlayerPos.x, 0.0f, g_xTreePos.z - xPlayerPos.z);
		const float fLen = glm::length(xToTree);
		if (fLen > 1e-4f)
		{
			xToTree /= fLen;
			// The ignore-body overload: the ray starts INSIDE the player's own
			// capsule, and Jolt would otherwise report a fraction-0 self-hit.
			const Zenith_Physics::RaycastResult xHit =
				g_xEngine.Physics().Raycast(xOrigin, xToTree, 4.0f, Tree_PlayerBodyID());
			g_bRayHit = xHit.m_bHit;
			g_xRayEntity = xHit.m_xHitEntity;
		}
		g_bTreeDone = true;
		g_eTreePhase = TreePhase::Done;
		return true;
	}

	case TreePhase::Done:
		return false;
	}
	return false;
}

static bool Verify_TreeCollision()
{
	Zenith_InputSimulator::ClearFixedDt();
	Tree_ReleaseMovementKeys();

	bool bPass = true;
	if (!g_bTreeDone)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TreeCollision] never completed (phase %d)",
			static_cast<int>(g_eTreePhase));
		bPass = false;
	}

	// bodies: the scene deserialized a v5 config and created a body per instance,
	// end to end. Nothing else in the suite observes that.
	if (!g_bTreesResolved)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TreeCollision] the tree entities never resolved");
		bPass = false;
	}
	else
	{
		if (g_uTrunkInstances <= 2000u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[TreeCollision] only %u trunk instances -- the grove did not load, so every "
				"assertion below would be vacuous", g_uTrunkInstances);
			bPass = false;
		}
		if (g_uTrunkBodies != g_uTrunkInstances)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[TreeCollision] %u trunk instances but %u bodies -- the loaded scene has trees "
				"you can walk through", g_uTrunkInstances, g_uTrunkBodies);
			bPass = false;
		}
		if (g_uLeafBodies != 0u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[TreeCollision] the leaves group owns %u bodies -- leaf cards are foliage you "
				"brush past and must author no collider", g_uLeafBodies);
			bPass = false;
		}
	}

	if (!g_bTargetPicked)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST, "[TreeCollision] no enabled trunk instance to aim at");
		bPass = false;
	}

	// walked / reached: without these, "blocked" is satisfied by a player that
	// never moved.
	if (!g_bWalked || (g_fInitialDist - g_fMinDist) < 3.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TreeCollision] the player never genuinely approached (initial %.2f, min %.2f) -- "
			"the block assertion would be vacuous", g_fInitialDist, g_fMinDist);
		bPass = false;
	}
	if (g_fMinDist > 2.0f)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TreeCollision] the player never reached the tree (min dist %.2f)", g_fMinDist);
		bPass = false;
	}

	// blocked / no_tunnel: the actual claim.
	if (g_fFinalDist < 0.85f * g_fBlockRadius)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TreeCollision] the player ended INSIDE the trunk (final %.3f, block radius %.3f) "
			"after 6s of pushing", g_fFinalDist, g_fBlockRadius);
		bPass = false;
	}
	if (g_fMinDist < 0.5f * g_fBlockRadius)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TreeCollision] the player tunnelled through the trunk (min %.3f, block radius %.3f)",
			g_fMinDist, g_fBlockRadius);
		bPass = false;
	}

	// raycast: pins the UserData -> entity resolution, which is the only wiring
	// between an instance body and anything able to name it.
	if (!g_bRayHit)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TreeCollision] a horizontal ray at the trunk from 4m hit nothing");
		bPass = false;
	}
	else if (!(g_xRayEntity == g_xTrunkEntity))
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[TreeCollision] the ray hit entity {%u,%u}, expected the trunk group {%u,%u} -- the "
			"body's UserData does not resolve to its owner",
			g_xRayEntity.m_uIndex, g_xRayEntity.m_uGeneration,
			g_xTrunkEntity.m_uIndex, g_xTrunkEntity.m_uGeneration);
		bPass = false;
	}

	Zenith_Log(LOG_CATEGORY_UNITTEST,
		"[TreeCollision] instances=%u bodies=%u leafBodies=%u initial=%.2f min=%.2f final=%.3f "
		"blockRadius=%.3f", g_uTrunkInstances, g_uTrunkBodies, g_uLeafBodies,
		g_fInitialDist, g_fMinDist, g_fFinalDist, g_fBlockRadius);
	return bPass;
}

static const Zenith_AutomatedTest g_xTreeCollisionTest = {
	"RT_TreeCollision",
	&Setup_TreeCollision,
	&Step_TreeCollision,
	&Verify_TreeCollision,
	/*maxFrames*/ 1800,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xTreeCollisionTest);

#endif // ZENITH_INPUT_SIMULATOR
