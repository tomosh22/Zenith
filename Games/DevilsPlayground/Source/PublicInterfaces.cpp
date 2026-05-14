#include "Zenith.h"

#include "PublicInterfaces.h"
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Navigation/Zenith_NavMesh.h"
#include "AI/Navigation/Zenith_NavMeshGenerator.h"

#include <unordered_map>

// ============================================================================
// W0-stub coordinator state.
// Wave-2 streams (B2 player, B3 items, B6 fog, etc.) extend each section with
// real implementations as they land. Until then every function returns a safe
// default so the project compiles and links.
// ============================================================================

namespace
{
	// ---- DP_Player state (B2 fills in) ----
	Zenith_EntityID g_xPossessedVillager = INVALID_ENTITY_ID;

	// Per-villager held-item record. Mutated by DP_Player::SetHeldItem /
	// RemoveHeldItem, read by DP_Player::GetHeldItem*. EntityID hashes via
	// (m_uIndex,m_uGeneration) — pack to uint64_t for std::unordered_map.
	uint64_t PackEntityID(Zenith_EntityID xID)
	{
		return (static_cast<uint64_t>(xID.m_uGeneration) << 32) | static_cast<uint64_t>(xID.m_uIndex);
	}

	struct VillagerHeldRecord
	{
		Zenith_EntityID m_xItem = INVALID_ENTITY_ID;
		DP_ItemTag      m_eTag  = DP_ItemTag::None;
	};
	std::unordered_map<uint64_t, VillagerHeldRecord> g_xHeldItems;

	// ---- DP_Items side-table (B3 fills in via DPItemBase_Behaviour::OnAwake/OnDestroy) ----
	std::unordered_map<uint64_t, DP_ItemTag> g_xItemTagTable;

	// ---- DP_Fog hole table (B6 fills in via DPFogPass_Behaviour::OnUpdate) ----
	struct FogHole
	{
		Zenith_EntityID m_xId;
		float           m_fRadius;
	};
	std::unordered_map<uint64_t, FogHole> g_xFogHoles;

	// ---- DP_Win state (B3 Pentagram fills in) ----
	uint32_t g_uCollectedObjectivesMask = 0;
	bool     g_bHasWon = false;
}

// ============================================================================
// DP_Player
// ============================================================================
namespace DP_Player
{
	Zenith_EntityID GetPossessedVillager()
	{
		return g_xPossessedVillager;
	}

	void SetPossessedVillager(Zenith_EntityID xId)
	{
		g_xPossessedVillager = xId;
	}

	DP_ItemTag GetHeldItemTag(Zenith_EntityID xVillager)
	{
		auto it = g_xHeldItems.find(PackEntityID(xVillager));
		if (it == g_xHeldItems.end()) return DP_ItemTag::None;
		return it->second.m_eTag;
	}

	Zenith_EntityID GetHeldItemEntity(Zenith_EntityID xVillager)
	{
		auto it = g_xHeldItems.find(PackEntityID(xVillager));
		if (it == g_xHeldItems.end()) return INVALID_ENTITY_ID;
		return it->second.m_xItem;
	}

	void SetHeldItem(Zenith_EntityID xVillager, Zenith_EntityID xItem)
	{
		VillagerHeldRecord xRec;
		xRec.m_xItem = xItem;
		xRec.m_eTag  = DP_Items::GetItemTag(xItem);
		g_xHeldItems[PackEntityID(xVillager)] = xRec;
	}

	// SourceBugFixed: GameJam0 ADPVillager::RemoveHeldItem null-derefs when no
	// held item. This port guards with early-return.
	void RemoveHeldItem(Zenith_EntityID xVillager)
	{
		auto it = g_xHeldItems.find(PackEntityID(xVillager));
		if (it == g_xHeldItems.end()) return;
		g_xHeldItems.erase(it);
	}

	void ResetForTest()
	{
		g_xPossessedVillager = INVALID_ENTITY_ID;
		g_xHeldItems.clear();
	}
}

// ============================================================================
// DP_Items
// ============================================================================
namespace DP_Items
{
	DP_ItemTag GetItemTag(Zenith_EntityID xItem)
	{
		auto it = g_xItemTagTable.find(PackEntityID(xItem));
		if (it == g_xItemTagTable.end()) return DP_ItemTag::None;
		return it->second;
	}

	Vec3 GetItemWorldPos(Zenith_EntityID xItem)
	{
		Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xItem);
		if (pxScene == nullptr) return Vec3(0.0f);
		Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
		if (!xEnt.IsValid()) return Vec3(0.0f);
		if (!xEnt.HasComponent<Zenith_TransformComponent>()) return Vec3(0.0f);
		Vec3 xPos;
		xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
		return xPos;
	}

	bool TryConsumeKeyForUnlock(Zenith_EntityID xVillager, DP_ItemTag eRequiredKey)
	{
		// SkeletonKey is a master key — opens any lock.
		const DP_ItemTag eHeld = DP_Player::GetHeldItemTag(xVillager);
		if (eHeld == DP_ItemTag::None) return false;
		if (eHeld != eRequiredKey && eHeld != DP_ItemTag::SkeletonKey) return false;

		// SkeletonKey persists across uses (matches source); a regular Key is consumed.
		if (eHeld != DP_ItemTag::SkeletonKey)
		{
			Zenith_EntityID xItem = DP_Player::GetHeldItemEntity(xVillager);
			DP_Player::RemoveHeldItem(xVillager);
			if (xItem.IsValid())
			{
				Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xItem);
				if (pxScene != nullptr)
				{
					Zenith_Entity xEnt = pxScene->TryGetEntity(xItem);
					if (xEnt.IsValid())
					{
						Zenith_SceneManager::Destroy(xEnt);
					}
				}
			}
		}
		return true;
	}

	// SourceBugFixed: GameJam0 AItemManager::FindItemByType derefs on miss.
	// This port returns invalid EntityID; callers null-check.
	Zenith_EntityID FindItemByTag(DP_ItemTag eTag)
	{
		for (const auto& [uPacked, eItemTag] : g_xItemTagTable)
		{
			if (eItemTag == eTag)
			{
				const uint32_t uIndex      = static_cast<uint32_t>(uPacked & 0xFFFFFFFFu);
				const uint32_t uGeneration = static_cast<uint32_t>(uPacked >> 32);
				Zenith_EntityID xId;
				xId.m_uIndex = uIndex;
				xId.m_uGeneration = uGeneration;
				return xId;
			}
		}
		return INVALID_ENTITY_ID;
	}

	// Internal: B3 will call these from DPItemBase_Behaviour OnAwake/OnDestroy.
	void Internal_RegisterItemTag(Zenith_EntityID xItem, DP_ItemTag eTag)
	{
		g_xItemTagTable[PackEntityID(xItem)] = eTag;
	}
	void Internal_UnregisterItemTag(Zenith_EntityID xItem)
	{
		g_xItemTagTable.erase(PackEntityID(xItem));
	}
}

// ============================================================================
// DP_Interactables
// ============================================================================
namespace DP_Interactables
{
	void MarkAsInteractable(Zenith_EntityID /*xId*/, Kind /*eKind*/, void* /*pUserData*/)
	{
		// W0 stub. B3 wires this into DPInteractable_Behaviour during script
		// attach; this entry point is reserved for non-behaviour entities (props
		// the editor can flag as interactable).
	}
}

// ============================================================================
// DP_AI
// ============================================================================
namespace
{
	// MVP-1.2.2: real navmesh generated from active-scene collider geometry
	// via Zenith_NavMeshGenerator::GenerateFromScene. With the full engine
	// stack (PRs #32 walls block / #33 perf / #35 collider opt-out / #36
	// portal stitch / #37 region-keyed verts) and a production ground-plane
	// collider in AuthorGameLevelScene, the generated navmesh is connected
	// across the whole playable area.
	//
	// Cache key is the active scene's build index, stable across handle
	// reuse so batched tests reloading the same scene share one ~850ms
	// generation. Cache invalidation is the explicit ResetLevelNavMesh
	// call.
	//
	// Fallback path stays for scenes with no static-collider geometry
	// (FrontEnd menu) -- synthetic 200m flat quad keeps priest spawn /
	// patrol APIs functional without crashing on a null navmesh pointer.
	Zenith_NavMesh* g_pxLevelNavMesh = nullptr;
	int             g_iCachedNavMeshBuildIndex = -1;

	void BuildSyntheticFlatNavMesh()
	{
		// Build a single quad covering [-50, 250] × [-50, 250] at y=1.0 (the
		// priest/villager spawn elevation). One large convex polygon means
		// any pursuit destination lands inside the navmesh, and patrol
		// random-point sampling has the entire walkable area to choose from.
		//
		// Note: dynamic-obstacle support is wired up in Zenith_Pathfinding
		// via Zenith_NavMeshPolygon::FLAG_BLOCKED, but a single-polygon mesh
		// can't selectively block a doorway region — blocking the only
		// polygon would block everything. The plumbing is in place for when
		// this navmesh is replaced by Zenith_NavMeshGenerator's
		// scene-driven output (which produces many smaller polygons aligned
		// with collider geometry); until then, DPDoor::SyncNavMeshBlock is a
		// no-op against a one-poly mesh by design.
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
		Zenith_PerceptionSystem::EmitSoundStimulus(xPos, fLoudness, fRadius, xSource);
	}

	const Zenith_NavMesh* GetOrBuildLevelNavMesh()
	{
		// Cache hit: same build-indexed scene as last call.
		Zenith_Scene xActive = Zenith_SceneManager::GetActiveScene();
		const int iActiveBuildIndex = xActive.IsValid()
			? Zenith_SceneManager::GetSceneData(xActive)->GetBuildIndex()
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
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneData(xActive);
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
		delete g_pxLevelNavMesh;
		g_pxLevelNavMesh = nullptr;
		g_iCachedNavMeshBuildIndex = -1;
	}
}

// ============================================================================
// DP_Fog (B6 fills in with shader/CBV plumbing; this is the surface only)
// ============================================================================
namespace DP_Fog
{
	void RegisterFogHole(Zenith_EntityID xId, float fRadius)
	{
		FogHole xHole;
		xHole.m_xId     = xId;
		xHole.m_fRadius = fRadius;
		g_xFogHoles[PackEntityID(xId)] = xHole;
	}

	void UnregisterFogHole(Zenith_EntityID xId)
	{
		g_xFogHoles.erase(PackEntityID(xId));
	}

	void ClearAllFogHoles()
	{
		g_xFogHoles.clear();
	}

	uint32_t GetFogHoleCount()
	{
		return static_cast<uint32_t>(g_xFogHoles.size());
	}

	uint32_t GatherFogHolePositions(Vec4* pxOutHoles, uint32_t uMaxHoles)
	{
		if (pxOutHoles == nullptr || uMaxHoles == 0) return 0;
		uint32_t uWritten = 0;
		for (const auto& [uPacked, xHole] : g_xFogHoles)
		{
			if (uWritten >= uMaxHoles) break;
			Zenith_SceneData* pxScene = Zenith_SceneManager::GetSceneDataForEntity(xHole.m_xId);
			if (pxScene == nullptr) continue;
			Zenith_Entity xEnt = pxScene->TryGetEntity(xHole.m_xId);
			if (!xEnt.IsValid()) continue;
			if (!xEnt.HasComponent<Zenith_TransformComponent>()) continue;
			Vec3 xPos;
			xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xPos);
			pxOutHoles[uWritten++] = Vec4(xPos.x, xPos.y, xPos.z, xHole.m_fRadius);
		}
		return uWritten;
	}
}

// ============================================================================
// DP_Win
// ============================================================================
namespace DP_Win
{
	uint32_t GetCollectedObjectivesMask()
	{
		return g_uCollectedObjectivesMask;
	}

	bool HasWon()
	{
		return g_bHasWon;
	}

	void NotifyObjectiveCollected(DP_ItemTag eObjective)
	{
		const uint32_t uBit = DP_ObjectiveTagToBit(eObjective);
		if (uBit == 0) return;
		g_uCollectedObjectivesMask |= uBit;
		if (g_uCollectedObjectivesMask == DP_ALL_OBJECTIVES_MASK && !g_bHasWon)
		{
			g_bHasWon = true;
			Zenith_EventDispatcher::Get().Dispatch(DP_OnVictory{});
		}
	}

	void Reset()
	{
		g_uCollectedObjectivesMask = 0;
		g_bHasWon = false;
	}
}

