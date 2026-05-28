#include "Zenith.h"

#include "DP_AI.h"
#include "DP_Query.h"
#include "DPCommonTypes.h"

#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_SceneSystem.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"
#include "AI/Components/Zenith_AIAgentComponent.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

#include "../Components/Priest_Behaviour.h"
#include "../Components/DPDoor_Behaviour.h"

namespace
{
	// MVP-1.2.2: real navmesh generated from active-scene collider geometry
	// via Zenith_NavMeshGenerator::GenerateFromScene. Cache key is the active
	// scene's build index, stable across handle reuse so batched tests
	// reloading the same scene share one ~850ms generation. Cache invalidation
	// is the explicit ResetLevelNavMesh call.
	//
	// Fallback path stays for scenes with no static-collider geometry
	// (FrontEnd menu) -- synthetic 200m flat quad keeps priest spawn /
	// patrol APIs functional without crashing on a null navmesh pointer.
	Zenith_NavMesh* g_pxLevelNavMesh = nullptr;
	int             g_iCachedNavMeshBuildIndex = -1;

	// 2026-05-25: procgen patrol nodes (anon-namespace state; populated
	// by SetPatrolNodes from the bootstrap, read by the priest's BT
	// FindPos node to cycle between rooms). Cleared by ResetLevelNavMesh
	// so scene reloads start fresh.
	Zenith_Vector<Vec3> g_axPatrolNodes;

	void BuildSyntheticFlatNavMesh()
	{
		const float fMinX = -50.0f;
		const float fMaxX = 250.0f;
		const float fMinZ = -50.0f;
		const float fMaxZ = 250.0f;
		const float fY    = 1.0f;

		g_pxLevelNavMesh = new Zenith_NavMesh();

		// CCW winding when viewed from above (Y-up). See navmesh CLAUDE.md
		// "Polygon Winding Order (Critical)" — V0=BL, V1=TL, V2=TR, V3=BR
		// gives positive +Y normal.
		const uint32_t uV0 = g_pxLevelNavMesh->AddVertex({ fMinX, fY, fMinZ }); // BL
		const uint32_t uV1 = g_pxLevelNavMesh->AddVertex({ fMinX, fY, fMaxZ }); // TL
		const uint32_t uV2 = g_pxLevelNavMesh->AddVertex({ fMaxX, fY, fMaxZ }); // TR
		const uint32_t uV3 = g_pxLevelNavMesh->AddVertex({ fMaxX, fY, fMinZ }); // BR

		Zenith_Vector<uint32_t> axIndices;
		axIndices.PushBack(uV0);
		axIndices.PushBack(uV1);
		axIndices.PushBack(uV2);
		axIndices.PushBack(uV3);
		g_pxLevelNavMesh->AddPolygon(axIndices);

		g_pxLevelNavMesh->ComputeSpatialData();
		g_pxLevelNavMesh->ComputeAdjacency();
		g_pxLevelNavMesh->BuildSpatialGrid();

		Zenith_Log(LOG_CATEGORY_AI,
			"DP_AI: built synthetic flat navmesh (%u verts, %u polys, bounds [%g..%g] x [%g..%g] at y=%g)",
			g_pxLevelNavMesh->GetVertexCount(),
			g_pxLevelNavMesh->GetPolygonCount(),
			fMinX, fMaxX, fMinZ, fMaxZ, fY);
	}
}

namespace DP_AI
{
	void EmitNoise(Vec3 xPos, float fLoudness, float fRadius, Zenith_EntityID xSource)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::EmitNoise must be called from main thread");
		Zenith_PerceptionSystem::EmitSoundStimulus(xPos, fLoudness, fRadius, xSource);
	}

	void NotifyAllPriestsOfInvestigatePos(Vec3 xPos)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::NotifyAllPriestsOfInvestigatePos must be called from main thread");
		// Direct-BB-write fanout, deliberately bypassing the perception
		// system. The perception path clamps each agent's hearing radius
		// at agent_max_range (priest default 30 m) -- so a 200 m bell
		// emit doesn't reach a priest 100 m away. The GDD intent for
		// BellSoul is "map-wide" so we iterate every priest in the
		// active scene and write straight into its BB.
		DP_Query::ForEachScriptInActiveScene<Priest_Behaviour>(
			[&xPos](Zenith_EntityID xPriestId, Priest_Behaviour&)
			{
				Zenith_SceneData* pxScene =
					g_xEngine.Scenes().GetSceneDataForEntity(xPriestId);
				if (pxScene == nullptr) return;
				Zenith_Entity xEnt = pxScene->TryGetEntity(xPriestId);
				if (!xEnt.IsValid()) return;
				if (!xEnt.HasComponent<Zenith_AIAgentComponent>()) return;
				Zenith_AIAgentComponent& xAg =
					xEnt.GetComponent<Zenith_AIAgentComponent>();
				Zenith_Blackboard& xBB = xAg.GetBlackboard();
				xBB.SetVector3(BB_KEY_INVESTIGATE_POS, xPos);
				xBB.SetBool(BB_KEY_HAS_INVESTIGATE_POS, true);
			});
	}

	void OpenNearbyDoorsFor(Zenith_EntityID xActor, const Vec3& xActorPos)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::OpenNearbyDoorsFor must be called from main thread");
		// 2026-05-26: door-open radius for AI actors is bumped to 4 m
		// (double the player's 2 m InteractRadius). The priest's BT
		// patrol picks random reachable navmesh points and walks to
		// them; with a 2 m radius the priest has to almost touch the
		// door before opening, which on rooms larger than ~6 m diag
		// the patrol's interior-biased random sampling rarely
		// achieves -- telemetry-confirmed on seed 5: priest moved
		// 1.4 x 2.5 m in a single room for 200 s, never approaching
		// a door's 2 m circle. 4 m means a patrol point within ~4 m
		// of a wall door triggers the open, which the patrol's
        // uniform-random sampling reliably reaches inside one or two
		// cycles. AI's "arm length" being twice the player's is a
		// pragmatic asymmetry -- the player decides explicitly when
		// to F-press, the priest needs to discover doors via
		// proximity alone.
		//
		// Iterate every DPDoor in the active scene. For each closed
		// door within 4 m of xActorPos, fire TryInteract -- which goes
		// through DPDoor's normal state machine (key check, audio
		// cue, DP_OnDoorOpened dispatch, collider-sensor toggle).
		// Same code path the player would take; the only difference
		// is the gated DPInteractable proximity poll is bypassed.
		constexpr float kAiDoorRadiusM = 4.0f;
		constexpr float kAiDoorRadiusSq = kAiDoorRadiusM * kAiDoorRadiusM;
		DP_Query::ForEachScriptInActiveScene<DPDoor_Behaviour>(
			[&xActor, &xActorPos](Zenith_EntityID /*xDoorId*/, DPDoor_Behaviour& xDoor)
			{
				if (!xDoor.BlocksPath()) return;          // already open/opening
				const Vec3 xC = xDoor.GetInteractionCentre();
				const float fDx = xActorPos.x - xC.x;
				const float fDz = xActorPos.z - xC.z;
				if (fDx * fDx + fDz * fDz > kAiDoorRadiusSq) return;
				xDoor.TryInteract(xActor);
			});
	}

	const Zenith_NavMesh* GetOrBuildLevelNavMesh()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::GetOrBuildLevelNavMesh must be called from main thread");
		Zenith_Scene xActive = g_xEngine.Scenes().GetActiveScene();
		const int iActiveBuildIndex = xActive.IsValid()
			? g_xEngine.Scenes().GetSceneData(xActive)->GetBuildIndex()
			: -1;
		if (g_pxLevelNavMesh != nullptr && iActiveBuildIndex >= 0
			&& iActiveBuildIndex == g_iCachedNavMeshBuildIndex)
		{
			return g_pxLevelNavMesh;
		}

		// Cache miss -- drop stale + rebuild.
		delete g_pxLevelNavMesh;
		g_pxLevelNavMesh = nullptr;
		g_iCachedNavMeshBuildIndex = -1;

		if (xActive.IsValid())
		{
			Zenith_SceneData* pxScene = g_xEngine.Scenes().GetSceneData(xActive);
			if (pxScene != nullptr)
			{
				NavMeshGenerationConfig xCfg{};
				// Tightened agent radius: DP doorways are 0.8-3m gaps and
				// 0.4m default radius erodes narrow doorways. 0.2m keeps
				// every authored doorway passable.
				xCfg.m_fAgentRadius = 0.2f;
				Zenith_NavMesh* pxGenerated =
					Zenith_NavMeshGenerator::GenerateFromScene(*pxScene, xCfg);
				if (pxGenerated != nullptr && pxGenerated->GetPolygonCount() > 0)
				{
					g_pxLevelNavMesh = pxGenerated;
					g_iCachedNavMeshBuildIndex = iActiveBuildIndex;
					Zenith_Log(LOG_CATEGORY_AI,
						"DP_AI: built real navmesh for build-index=%d (%u verts, %u polys)",
						iActiveBuildIndex,
						g_pxLevelNavMesh->GetVertexCount(),
						g_pxLevelNavMesh->GetPolygonCount());
					return g_pxLevelNavMesh;
				}
				delete pxGenerated; // null-safe
				Zenith_Log(LOG_CATEGORY_AI,
					"DP_AI: generator returned empty navmesh for build-index=%d -- falling back to synthetic flat quad",
					iActiveBuildIndex);
			}
		}

		// Fallback: legacy 200m flat quad for scenes with no static collider
		// geometry (FrontEnd menu). NOT cached against the build index --
		// the next call with real geometry will rebuild.
		BuildSyntheticFlatNavMesh();
		return g_pxLevelNavMesh;
	}

	void ResetLevelNavMesh()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::ResetLevelNavMesh must be called from main thread");
		delete g_pxLevelNavMesh;
		g_pxLevelNavMesh = nullptr;
		g_iCachedNavMeshBuildIndex = -1;
		// 2026-05-25: patrol nodes are scene-scoped, drop them alongside
		// the navmesh so the next Generate writes a fresh set.
		g_axPatrolNodes.Clear();
	}

	void SetPatrolNodes(const Zenith_Vector<Vec3>& axNodes)
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::SetPatrolNodes must be called from main thread");
		g_axPatrolNodes.Clear();
		const uint32_t uN = axNodes.GetSize();
		for (uint32_t i = 0; i < uN; ++i)
		{
			g_axPatrolNodes.PushBack(axNodes.Get(i));
		}
	}

	void ClearPatrolNodes()
	{
		Zenith_Assert(g_xEngine.Threading().IsMainThread(),
			"DP_AI::ClearPatrolNodes must be called from main thread");
		g_axPatrolNodes.Clear();
	}

	const Zenith_Vector<Vec3>& GetPatrolNodes()
	{
		return g_axPatrolNodes;
	}
}
