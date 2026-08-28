#include "Zenith.h"

#ifdef ZENITH_INPUT_SIMULATOR

/**
 * RT_PropCollision -- the scattered props own the physics bodies they are
 * supposed to, and only those.
 *
 * RenderTest_ScatterInstancedProps authors ELEVEN instanced-mesh groups (four
 * stone, four deadwood, three wind-animated bushes) and deliberately gives only
 * FIVE of them a per-instance capsule: boulders, standing stones, fallen trunks
 * and stumps are things you walk into; flagstones, pebble clusters, loose
 * branches and bushes are things you walk over or brush past. That asymmetry
 * survives a save/load round trip only because
 * Zenith_InstancedMeshComponent serializes the collider config at v5 and
 * recreates one body per instance on read -- and NOTHING about it is visible in
 * a render. A regression that dropped the config, or one that gave every group
 * colliders, would look identical on screen and cost hundreds of static Jolt
 * bodies (or a player who walks through boulders).
 *
 * ★ THE TWO TIPPED LOG ROWS ARE THE REASON THIS TEST GREW A SECOND HALF. A log
 * is modelled STANDING and laid down by the instance rotation, because
 * Zenith_InstanceColliderConfig can only describe a Y-aligned capsule and
 * CreateInstanceBody rotates it by the instance transform. If that rotation ever
 * stopped being applied, the ledger would still count one body per log and the
 * render would still show a log on its side -- but the capsule would be standing
 * bolt upright through it. So the ray probe fires ALONG each collider group's
 * own local axis after transforming it into world space: for a log that is a
 * horizontal shot down its length, and it can only hit if the capsule really was
 * tipped over with the mesh.
 *
 * Its sibling RT_TreeCollision covers the other half of the claim -- that a
 * player pushing into one of these bodies is actually stopped -- on the tree
 * trunk group. That walk is not repeated here: it is the same capsule code path,
 * and what is unproven for props is the per-group ON/OFF split and the axis.
 */

#include "Core/Zenith_AutomatedTest.h"
#include "Core/Zenith_Engine.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_SceneData.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics.h"

#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_InstancedMeshComponent.h"
// A GAME translation unit may include Flux directly -- the layering ratchet
// scopes Zenith/ only. Needed for the enabled-slot list + transforms.
#include "Flux/InstancedMeshes/Flux_InstanceGroup.h"

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace
{
	enum class RockPhase
	{
		Boot, WaitReady, RayProbe, Done
	};

	// Every group, whether it authors a capsule, and whether the scatter tips it
	// onto its side. Spelled here rather than read from the game's table on
	// purpose: a test that asked the production code what it authored would agree
	// with a flipped flag. These are the OUTCOME the design intends.
	struct PropGroupExpectation
	{
		const char* m_szEntity;
		bool        m_bExpectBodies;
		bool        m_bLaidDown;      // the mesh's local +Y should be HORIZONTAL in world
	};

	const PropGroupExpectation g_axPropExpectations[] =
	{
		{ "TerrainRocks_Boulder",  true,  false },
		{ "TerrainRocks_Slab",     false, false },
		{ "TerrainRocks_Shard",    true,  false },
		{ "TerrainRocks_Pebbles",  false, false },
		{ "FallenTrees_Log",       true,  true  },
		{ "FallenTrees_LogMossy",  true,  true  },
		{ "FallenTrees_Stump",     true,  false },
		{ "FallenTrees_Branches",  false, true  },
		// The bush groups are foliage: zero bodies each, upright. Their rows
		// still earn their place -- the zero-body assertion is what notices a
		// collider config leaking onto them (360 phantom static bodies), and
		// the attitude check that their instance rotation still reaches the
		// loaded scene.
		{ "TerrainBushes_Broad",   false, false },
		{ "TerrainBushes_Mound",   false, false },
		{ "TerrainBushes_Spindly", false, false },
	};
	constexpr int iROCK_GROUPS =
		static_cast<int>(sizeof(g_axPropExpectations) / sizeof(g_axPropExpectations[0]));

	RockPhase g_eRockPhase = RockPhase::Boot;

	uint32_t g_auRockInstances[iROCK_GROUPS] = {};
	uint32_t g_auRockBodies[iROCK_GROUPS] = {};
	bool     g_abRockResolved[iROCK_GROUPS] = {};
	bool     g_bRocksResolved = false;

	// The per-group probe: one instance of every group that should own bodies,
	// shot at broadside so the aim works whatever attitude it is in.
	bool  g_abProbed[iROCK_GROUPS] = {};
	bool  g_abProbeHit[iROCK_GROUPS] = {};
	bool  g_abProbeEntityMatches[iROCK_GROUPS] = {};
	float g_afAxisUpDot[iROCK_GROUPS] = {};   // world Y component of the mesh's local +Y

	Zenith_Entity Rock_FindEntity(const char* szName)
	{
		Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetActiveSceneData();
		if (pxSceneData == nullptr)
		{
			return Zenith_Entity();
		}
		return pxSceneData->FindEntityByName(szName);
	}

	Zenith_InstancedMeshComponent* Rock_FindGroup(const char* szName)
	{
		Zenith_Entity xEntity = Rock_FindEntity(szName);
		if (!xEntity.IsValid())
		{
			return nullptr;
		}
		return xEntity.TryGetComponent<Zenith_InstancedMeshComponent>();
	}

	Zenith_PhysicsBodyID Rock_PlayerBodyID()
	{
		Zenith_Entity xPlayer = Rock_FindEntity("Player");
		if (!xPlayer.IsValid())
		{
			return Zenith_PhysicsBodyID();
		}
		Zenith_ColliderComponent* pxCollider = xPlayer.TryGetComponent<Zenith_ColliderComponent>();
		return pxCollider != nullptr ? pxCollider->GetBodyID() : Zenith_PhysicsBodyID();
	}

	// Probe one group: measure the attitude of its first enabled instance, then
	// shoot STRAIGHT DOWN onto a point only that instance's capsule can occupy.
	//
	// ★ DOWNWARD, not broadside, and the reason is worth keeping. A horizontal ray
	// at a prop standing on a hillside hits the HILL first -- the props sit on
	// sloping ground by design, so a side shot from three radii out is behind the
	// terrain about as often as not. That is exactly what the first version of
	// this probe did, and it failed on the boulders while passing on everything
	// else, which reads like a boulder bug rather than an aiming bug. A ray from
	// above hits the prop before the ground it is standing on, for every group.
	//
	// ★ AND FOR A TIPPED PIECE IT AIMS 70% OF THE WAY ALONG THE TRUNK, not at the
	// capsule centre. That is what makes the probe DISCRIMINATING rather than
	// merely positive: if the instance rotation ever stopped reaching the collider
	// the log would still be DRAWN on its side, the ledger would still count one
	// body per log, but the capsule would be standing upright through the butt
	// end -- and there would be nothing at all out at the far end to hit.
	//
	// Several instances are sampled because one unlucky prop can sit under a tree
	// trunk, whose capsule would be hit first and resolve to the wrong entity.
	// The claim is that these bodies exist and are attributable, so one clean hit
	// settles it; a group where NONE of the samples resolves is a real failure.
	void Prop_ProbeGroup(int iIndex)
	{
		const PropGroupExpectation& xExpect = g_axPropExpectations[iIndex];
		Zenith_InstancedMeshComponent* pxComp = Rock_FindGroup(xExpect.m_szEntity);
		if (pxComp == nullptr || pxComp->GetInstanceGroup() == nullptr)
		{
			return;
		}
		const Flux_InstanceGroup* pxGroup = pxComp->GetInstanceGroup();
		Zenith_Vector<uint32_t> xEnabled;
		pxGroup->ComputeVisibleIndices(xEnabled);
		if (xEnabled.GetSize() == 0)
		{
			return;
		}

		const Zenith_InstanceColliderConfig& xConfig = pxComp->GetInstanceColliderConfig();
		const Zenith_EntityID xOwner = pxComp->GetParentEntity().GetEntityID();
		const uint32_t uSamples = std::min<uint32_t>(8u, xEnabled.GetSize());

		for (uint32_t uSample = 0; uSample < uSamples; ++uSample)
		{
			Zenith_Maths::Vector3 xPosition;
			Zenith_Maths::Quat xRotation;
			Zenith_Maths::Vector3 xScale;
			Zenith_Maths::DecomposeTRS(pxGroup->GetTransforms().Get(xEnabled.Get(uSample)),
				xPosition, xRotation, xScale);

			// The mesh's own long axis, in world space. This is the number that
			// tells a tipped log from an upright one.
			const Zenith_Maths::Vector3 xAxis =
				glm::normalize(xRotation * Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
			if (uSample == 0u)
			{
				g_afAxisUpDot[iIndex] = xAxis.y;
				g_abProbed[iIndex] = true;
			}

			if (!xExpect.m_bExpectBodies || g_abProbeEntityMatches[iIndex])
			{
				continue;   // nothing to shoot at, by design -- or already settled
			}

			const float fRadius = xConfig.m_fRadius * std::max(std::abs(xScale.x), std::abs(xScale.z));
			const float fHalfHeight = xConfig.m_fCylinderHalfHeight * std::abs(xScale.y);
			const Zenith_Maths::Vector3 xCentre = xPosition +
				xRotation * Zenith_Maths::Vector3(0.0f, xConfig.m_fLocalYOffset * xScale.y, 0.0f);
			// Out along the axis for a tipped piece; the centre for an upright one,
			// whose axis offset would only move the aim point up and down.
			const Zenith_Maths::Vector3 xAim = xExpect.m_bLaidDown
				? xCentre + xAxis * (fHalfHeight * 0.70f)
				: xCentre;

			const float fDrop = fRadius * 2.0f + 1.0f;
			const Zenith_Maths::Vector3 xOrigin = xAim + Zenith_Maths::Vector3(0.0f, fDrop, 0.0f);
			const Zenith_Maths::Vector3 xDown(0.0f, -1.0f, 0.0f);
			const Zenith_Physics::RaycastResult xHit =
				g_xEngine.Physics().Raycast(xOrigin, xDown, fDrop * 2.0f, Rock_PlayerBodyID());
			if (xHit.m_bHit)
			{
				g_abProbeHit[iIndex] = true;
				if (xHit.m_xHitEntity == xOwner)
				{
					g_abProbeEntityMatches[iIndex] = true;
				}
			}
		}
	}
}

static void Setup_PropCollision()
{
	g_eRockPhase = RockPhase::Boot;
	for (int i = 0; i < iROCK_GROUPS; ++i)
	{
		g_auRockInstances[i] = 0;
		g_auRockBodies[i] = 0;
		g_abRockResolved[i] = false;
	}
	g_bRocksResolved = false;
	for (int i = 0; i < iROCK_GROUPS; ++i)
	{
		g_abProbed[i] = false;
		g_abProbeHit[i] = false;
		g_abProbeEntityMatches[i] = false;
		g_afAxisUpDot[i] = 0.0f;
	}
}

static bool Step_PropCollision(int iFrame)
{
	switch (g_eRockPhase)
	{
	case RockPhase::Boot:
		g_xEngine.Scenes().LoadSceneByIndex(0, SCENE_LOAD_SINGLE);
		g_eRockPhase = RockPhase::WaitReady;
		return true;

	case RockPhase::WaitReady:
	{
		int iResolved = 0;
		for (int i = 0; i < iROCK_GROUPS; ++i)
		{
			Zenith_InstancedMeshComponent* pxGroup = Rock_FindGroup(g_axPropExpectations[i].m_szEntity);
			if (pxGroup != nullptr && pxGroup->GetInstanceCount() > 0)
			{
				g_auRockInstances[i] = pxGroup->GetInstanceCount();
				g_auRockBodies[i] = pxGroup->GetInstanceBodyCount();
				g_abRockResolved[i] = true;
				iResolved++;
			}
		}
		if (iResolved == iROCK_GROUPS)
		{
			g_bRocksResolved = true;
			g_eRockPhase = RockPhase::RayProbe;
			return true;
		}
		return iFrame < 900;
	}

	case RockPhase::RayProbe:
	{
		for (int i = 0; i < iROCK_GROUPS; ++i)
		{
			Prop_ProbeGroup(i);
		}
		g_eRockPhase = RockPhase::Done;
		return true;
	}

	case RockPhase::Done:
		return false;
	}
	return false;
}

static bool Verify_PropCollision()
{
	bool bPass = true;

	if (!g_bRocksResolved)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PropCollision] the loaded scene does not carry all eleven prop groups "
			"with instances -- the scatter did not reach the asset");
		return false;
	}

	for (int i = 0; i < iROCK_GROUPS; ++i)
	{
		const PropGroupExpectation& xExpect = g_axPropExpectations[i];
		if (xExpect.m_bExpectBodies)
		{
			// One static body per live instance. Anything less means the v5
			// config did not survive the round trip for some of them.
			if (g_auRockBodies[i] != g_auRockInstances[i])
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[PropCollision] %s has %u instances but %u bodies -- the loaded scene has "
					"rocks the player can walk through", xExpect.m_szEntity,
					g_auRockInstances[i], g_auRockBodies[i]);
				bPass = false;
			}
		}
		else if (g_auRockBodies[i] != 0u)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[PropCollision] %s owns %u bodies -- flagstones, pebble clusters, loose "
				"branches and bushes are walked over or brushed past, never collided with",
				xExpect.m_szEntity, g_auRockBodies[i]);
			bPass = false;
		}
	}

	for (int i = 0; i < iROCK_GROUPS; ++i)
	{
		const PropGroupExpectation& xExpect = g_axPropExpectations[i];
		if (!g_abProbed[i])
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[PropCollision] %s had no enabled instance to probe -- its assertions would "
				"be vacuous", xExpect.m_szEntity);
			bPass = false;
			continue;
		}

		// Attitude. A laid-down piece's local +Y must be near-horizontal in world
		// space, an upright one near-vertical. This is what catches an instance
		// rotation that stopped being applied -- to the mesh OR to the capsule.
		if (xExpect.m_bLaidDown)
		{
			if (std::abs(g_afAxisUpDot[i]) > 0.30f)
			{
				Zenith_Log(LOG_CATEGORY_UNITTEST,
					"[PropCollision] %s is meant to lie on its side but its long axis has "
					"world-Y %.3f -- the lay-down rotation did not reach it",
					xExpect.m_szEntity, g_afAxisUpDot[i]);
				bPass = false;
			}
		}
		else if (g_afAxisUpDot[i] < 0.80f)
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[PropCollision] %s is meant to stand upright but its long axis has world-Y "
				"%.3f", xExpect.m_szEntity, g_afAxisUpDot[i]);
			bPass = false;
		}

		if (!xExpect.m_bExpectBodies)
		{
			continue;
		}
		if (!g_abProbeEntityMatches[i])
		{
			Zenith_Log(LOG_CATEGORY_UNITTEST,
				"[PropCollision] no downward ray onto %s resolved to that group across 8 "
				"sampled instances (any hit at all: %d) -- either the ledger counts bodies "
				"that are not in the physics world, or (for a tipped piece) the capsule is "
				"not lying along the trunk", xExpect.m_szEntity, g_abProbeHit[i] ? 1 : 0);
			bPass = false;
		}
	}

	for (int i = 0; i < iROCK_GROUPS; ++i)
	{
		Zenith_Log(LOG_CATEGORY_UNITTEST,
			"[PropCollision] %-24s %4u/%-4u bodies/instances  axisUpY %+.3f  resolved=%d",
			g_axPropExpectations[i].m_szEntity, g_auRockBodies[i], g_auRockInstances[i],
			g_afAxisUpDot[i], g_abProbeEntityMatches[i] ? 1 : 0);
	}
	return bPass;
}

static const Zenith_AutomatedTest g_xPropCollisionTest = {
	"RT_PropCollision",
	&Setup_PropCollision,
	&Step_PropCollision,
	&Verify_PropCollision,
	/*maxFrames*/ 1200,
	/*bRequiresGraphics*/ false,
};
ZENITH_AUTOMATED_TEST_REGISTER(g_xPropCollisionTest);

#endif // ZENITH_INPUT_SIMULATOR
