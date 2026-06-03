#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "DPCommonTypes.h"

class Zenith_NavMesh;

// ============================================================================
// DP_AI — published by B4. Priest perception bridging + level navmesh cache.
// ============================================================================
namespace DP_AI
{
	constexpr const char* PRIEST_BEHAVIOUR_TYPE = "Priest_Behaviour";

	constexpr const char* BB_KEY_SELF_ACTOR          = "SelfActor";
	constexpr const char* BB_KEY_TARGET_WITH_DEVIL   = "TargetWithDevil";
	constexpr const char* BB_KEY_SUSPICION_RADIUS    = "SuspicionRadius";
	constexpr const char* BB_KEY_INVESTIGATE_POS     = "InvestigatePos";
	constexpr const char* BB_KEY_HAS_INVESTIGATE_POS = "HasInvestigatePos";
	constexpr const char* BB_KEY_PATROL_TARGET       = "PatrolTarget";
	// MVP-1.6: priest reads the highest-scent villager out of this slot.
	constexpr const char* BB_KEY_HIGH_SCENT_TARGET   = "HighScentTarget";

	void EmitNoise(Vec3 xPos, float fLoudness, float fRadius, Zenith_EntityID xSource);

	// MVP-2.2.6+ map-wide bell broadcast. Bypasses perception's hearing-radius
	// clamp by writing straight into every Priest_Behaviour agent's blackboard.
	void NotifyAllPriestsOfInvestigatePos(Vec3 xPos);

	// Lazily-built level navmesh.
	const Zenith_NavMesh* GetOrBuildLevelNavMesh();

	// Reset state on scene unload — the next GetOrBuildLevelNavMesh rebuilds.
	void ResetLevelNavMesh();

	// 2026-05-25: procgen patrol nodes (world-space positions, one per
	// non-pent room near the priest spawn -- see PlaceAI in the
	// generator). The priest's FindPos BT node round-robins through
	// these so the priest actually traverses between rooms instead of
	// patrolling within its spawn room. Bootstrap calls SetPatrolNodes
	// after Generate; ResetLevelNavMesh clears them along with the
	// navmesh on scene unload.
	void SetPatrolNodes(const Zenith_Vector<Vec3>& axNodes);
	void ClearPatrolNodes();
	const Zenith_Vector<Vec3>& GetPatrolNodes();

	// 2026-05-25: F-press equivalent for non-player actors (priest +
	// scripted events). Mediates between Priest_Behaviour and
	// DPDoor_Behaviour without the priest having to include the door
	// header (cross-behaviour rule). Iterates every door in the active
	// scene; for each closed door within the door's own interact
	// radius of xActorPos, calls DPDoor::TryInteract(xActor) which
	// bypasses the DPInteractable visibility/range gating + the
	// F-press release latch. Honors lock state (locked doors reject
	// actors that don't hold the matching key).
	void OpenNearbyDoorsFor(Zenith_EntityID xActor,
	                        const Vec3& xActorPos);
}
