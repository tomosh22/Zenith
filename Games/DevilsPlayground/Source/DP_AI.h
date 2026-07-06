#pragma once

#include "ZenithECS/Zenith_Entity.h"
#include "DPCommonTypes.h"

class Zenith_NavMesh;

// ============================================================================
// DP_AI — published by B4. Priest perception bridging + level navmesh cache.
// ============================================================================
namespace DP_AI
{
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
	// clamp by writing straight into every Priest_Component agent's blackboard.
	void NotifyAllPriestsOfInvestigatePos(Vec3 xPos);

	// W3: the scent-fanout twin - writes the highest-scent villager handle to
	// every priest's decision blackboard (DP_Priest.bgraph). Called from
	// DP_Player::WriteHighestScentToBlackboard; INVALID is a legitimate value.
	void WriteHighScentTargetToAllPriests(Zenith_EntityID xHighestId);

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
	// scripted events). Mediates between Priest_Component and
	// DPDoor_Component without the priest having to include the door
	// header (cross-component rule). Iterates every door in the active
	// scene; for each closed door within the door's own interact
	// radius of xActorPos, calls DPDoor::TryInteract(xActor) which
	// bypasses the DPInteractable visibility/range gating + the
	// F-press release latch. Honors lock state (locked doors reject
	// actors that don't hold the matching key).
	void OpenNearbyDoorsFor(Zenith_EntityID xActor,
	                        const Vec3& xActorPos);

	// 2026-07-01 stitch-health instrumentation (priest stuck-in-buildings
	// fix). DPDoor_Component calls NotifyDoorStitchFailed when every
	// navmesh-portal probe combo fails for a door -- previously log-only,
	// which let procgen seeds ship rooms that were sealed for pathfinding
	// even with the door physically open. The count is scene-scoped
	// (cleared by ResetLevelNavMesh) and gated at 0 by
	// Test_ProcLevel_PriestReachability across the canonical 10 seeds.
	void NotifyDoorStitchFailed();
	uint32_t GetUnstitchedDoorCount();

	// True when both positions sit on (within fMaxVerticalDist of) the
	// level navmesh AND their polygons share a connected component of the
	// polygon-neighbour graph. Door stitches count as connections; dynamic
	// BLOCKED state is deliberately ignored -- a stitched door that is
	// merely closed still counts, because the priest opens unlocked doors
	// on approach (OpenNearbyDoorsFor). False when either point is
	// off-mesh.
	bool ArePositionsConnected(const Vec3& xFrom, const Vec3& xTo,
	                           float fMaxVerticalDist = 3.0f);

	// ========================================================================
	// Cross-component priest-state forwarders for the HUD. Iterate every
	// Priest_Component in the active scene and read its blackboard / position.
	// Moved here from DPHUDController so the HUD header no longer
	// includes Priest_Component.h (cross-component rule).
	// ========================================================================

	// True if any priest has a valid BB_KEY_TARGET_WITH_DEVIL (pursuing).
	bool IsAnyPriestPursuing();

	// True if any priest has BB_KEY_HAS_INVESTIGATE_POS set (investigating).
	bool IsAnyPriestInvestigating();

	// Horizontal (XZ) distance from xFrom to the nearest priest in the
	// active scene, or -1.0f if there is no priest.
	float GetNearestPriestDistanceFrom(const Vec3& xFrom);
}
