#pragma once

#include "EntityComponent/Zenith_Entity.h"
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
}
