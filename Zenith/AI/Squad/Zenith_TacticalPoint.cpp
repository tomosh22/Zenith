#include "Zenith.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "EntityComponent/Zenith_Scene.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "Physics/Zenith_Physics.h"
#include "Profiling/Zenith_Profiling.h"

#ifdef ZENITH_TOOLS
#include "Flux/Primitives/Flux_Primitives.h"
#endif

// Lookup table for tactical point type properties (color, display score, name)
struct TacticalPointTypeInfo
{
	Zenith_Maths::Vector3 m_xColor;
	float m_fBaseDisplayScore;
};

static const TacticalPointTypeInfo s_axTypeInfo[] =
{
	{ Zenith_Maths::Vector3(0.0f, 0.8f, 0.0f), 3.0f },  // COVER_FULL - Green
	{ Zenith_Maths::Vector3(0.8f, 0.8f, 0.0f), 2.0f },  // COVER_HALF - Yellow
	{ Zenith_Maths::Vector3(1.0f, 0.5f, 0.0f), 2.5f },  // FLANK_POSITION - Orange
	{ Zenith_Maths::Vector3(0.5f, 0.0f, 0.8f), 3.0f },  // OVERWATCH - Purple
	{ Zenith_Maths::Vector3(0.0f, 0.5f, 1.0f), 1.0f },  // PATROL_WAYPOINT - Blue
	{ Zenith_Maths::Vector3(0.8f, 0.0f, 0.0f), 2.5f },  // AMBUSH - Red
	{ Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f), 1.5f },  // RETREAT - Gray
};
static_assert(sizeof(s_axTypeInfo) / sizeof(s_axTypeInfo[0]) == static_cast<size_t>(TacticalPointType::COUNT),
	"s_axTypeInfo must match TacticalPointType::COUNT");

// Static members
Zenith_Vector<Zenith_TacticalPoint> Zenith_TacticalPointSystem::s_axPoints;
Zenith_Vector<bool> Zenith_TacticalPointSystem::s_axPointActive;
uint32_t Zenith_TacticalPointSystem::s_uNextPointID = 0;
bool Zenith_TacticalPointSystem::s_bInitialised = false;

// Scoring weights
float Zenith_TacticalPointSystem::s_fDistanceWeight = 1.0f;
float Zenith_TacticalPointSystem::s_fCoverWeight = 2.0f;
float Zenith_TacticalPointSystem::s_fVisibilityWeight = 1.5f;
float Zenith_TacticalPointSystem::s_fElevationWeight = 0.5f;

// ========== Entity Position Helper ==========

bool Zenith_TacticalPointSystem::GetEntityPosition(Zenith_EntityID xEntity, Zenith_Maths::Vector3& xOutPos)
{
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return false;
	}

	Zenith_Entity xEnt = pxSceneData->TryGetEntity(xEntity);
	if (!xEnt.IsValid() || !xEnt.HasComponent<Zenith_TransformComponent>())
	{
		return false;
	}

	xEnt.GetComponent<Zenith_TransformComponent>().GetPosition(xOutPos);
	return true;
}

// ========== Score Context Structs ==========

struct CoverScoreContext
{
	Zenith_Maths::Vector3 m_xAgentPos;
	Zenith_Maths::Vector3 m_xThreatPos;
	float m_fMaxDistance;
};

struct FlankScoreContext
{
	Zenith_Maths::Vector3 m_xAgentPos;
	Zenith_Maths::Vector3 m_xTargetPos;
	Zenith_Maths::Vector3 m_xTargetFacing;
};

struct OverwatchScoreContext
{
	Zenith_Maths::Vector3 m_xAgentPos;
	Zenith_Maths::Vector3 m_xAreaToWatch;
};

// ========== Score Functions (file-local) ==========

static float ScoreCoverPoint(const Zenith_TacticalPoint& xPoint, const void* pCtx)
{
	const CoverScoreContext* pxCtx = static_cast<const CoverScoreContext*>(pCtx);

	float fDist = Zenith_Maths::Length(xPoint.m_xPosition - pxCtx->m_xAgentPos);

	// Score based on cover from threat
	float fCoverScore = Zenith_TacticalPointSystem::EvaluateCoverFromThreat(xPoint.m_xPosition, pxCtx->m_xThreatPos);
	float fDistScore = 1.0f - (fDist / pxCtx->m_fMaxDistance);  // Prefer closer points
	float fTotalScore = fCoverScore * Zenith_TacticalPointSystem::s_fCoverWeight + fDistScore * Zenith_TacticalPointSystem::s_fDistanceWeight;

	// Bonus for full cover
	if (xPoint.m_eType == TacticalPointType::COVER_FULL)
	{
		fTotalScore *= 1.5f;
	}

	return fTotalScore;
}

static float ScoreFlankPoint(const Zenith_TacticalPoint& xPoint, const void* pCtx)
{
	const FlankScoreContext* pxCtx = static_cast<const FlankScoreContext*>(pCtx);

	// Score based on flank angle (perpendicular to target facing is best)
	float fFlankScore = Zenith_TacticalPointSystem::EvaluateFlankAngle(xPoint.m_xPosition, pxCtx->m_xTargetPos, pxCtx->m_xTargetFacing);

	// Also consider distance from agent (prefer closer to agent's current path)
	float fDistFromAgent = Zenith_Maths::Length(xPoint.m_xPosition - pxCtx->m_xAgentPos);
	float fDistScore = 1.0f / (1.0f + fDistFromAgent * 0.1f);

	float fTotalScore = fFlankScore * 2.0f + fDistScore;

	return fTotalScore;
}

static float ScoreOverwatchPoint(const Zenith_TacticalPoint& xPoint, const void* pCtx)
{
	const OverwatchScoreContext* pxCtx = static_cast<const OverwatchScoreContext*>(pCtx);

	// Score based on:
	// 1. Elevation (higher is better for overwatch)
	// 2. Line of sight to area
	// 3. Distance from agent (prefer reachable)

	float fElevationScore = (xPoint.m_uFlags & TACPOINT_FLAG_ELEVATED) ? 1.5f : 1.0f;
	fElevationScore += xPoint.m_xPosition.y * 0.1f;  // Raw height bonus

	// Line of sight check with physics raycast
	Zenith_Maths::Vector3 xEyeLevel = xPoint.m_xPosition + Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f);
	Zenith_Maths::Vector3 xDirection = pxCtx->m_xAreaToWatch - xEyeLevel;
	float fCheckDist = Zenith_Maths::Length(xDirection);
	Zenith_Physics::RaycastResult xRayResult = Zenith_Physics::Raycast(xEyeLevel, xDirection, fCheckDist);
	float fLOSScore = xRayResult.m_bHit ? 0.0f : 1.0f;  // Full score if clear LOS, zero if blocked

	float fDistFromAgent = Zenith_Maths::Length(xPoint.m_xPosition - pxCtx->m_xAgentPos);
	float fDistScore = 1.0f / (1.0f + fDistFromAgent * 0.05f);

	// Bonus for actual overwatch type
	float fTypeBonus = (xPoint.m_eType == TacticalPointType::OVERWATCH) ? 1.5f : 1.0f;

	float fTotalScore = (fElevationScore * Zenith_TacticalPointSystem::s_fElevationWeight +
		fLOSScore * Zenith_TacticalPointSystem::s_fVisibilityWeight +
		fDistScore * Zenith_TacticalPointSystem::s_fDistanceWeight) * fTypeBonus;

	return fTotalScore;
}

// ========== Type Filter Functions (file-local) ==========

static bool FilterCoverTypes(TacticalPointType eType)
{
	return eType == TacticalPointType::COVER_FULL || eType == TacticalPointType::COVER_HALF;
}

static bool FilterFlankTypes(TacticalPointType eType)
{
	return eType == TacticalPointType::FLANK_POSITION || eType == TacticalPointType::COVER_HALF;
}

static bool FilterOverwatchTypes(TacticalPointType eType)
{
	return eType == TacticalPointType::OVERWATCH ||
		eType == TacticalPointType::COVER_HALF ||
		eType == TacticalPointType::COVER_FULL;
}

// ========== FindBestPointOfType ==========

const Zenith_TacticalPoint* Zenith_TacticalPointSystem::FindBestPointOfType(
	const Zenith_Maths::Vector3& xDistanceRef,
	float fMinDistance,
	float fMaxDistance,
	TacticalTypeFilterFn pfnTypeFilter,
	TacticalScoreFn pfnScore,
	const void* pScoreCtx)
{
	const Zenith_TacticalPoint* pxBest = nullptr;
	float fBestScore = -FLT_MAX;

	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		if (!s_axPointActive.Get(u))
		{
			continue;
		}

		const Zenith_TacticalPoint& xPoint = s_axPoints.Get(u);

		if (!pfnTypeFilter(xPoint.m_eType))
		{
			continue;
		}

		if (!xPoint.IsAvailable())
		{
			continue;
		}

		float fDist = Zenith_Maths::Length(xPoint.m_xPosition - xDistanceRef);
		if (fDist < fMinDistance || fDist > fMaxDistance)
		{
			continue;
		}

		float fScore = pfnScore(xPoint, pScoreCtx);

		if (fScore > fBestScore)
		{
			fBestScore = fScore;
			pxBest = &xPoint;
		}
	}

	return pxBest;
}

void Zenith_TacticalPointSystem::Initialise()
{
	if (s_bInitialised)
	{
		return;
	}

	s_axPoints.Clear();
	s_axPointActive.Clear();
	s_uNextPointID = 0;
	s_bInitialised = true;

	Zenith_Log(LOG_CATEGORY_AI, "TacticalPointSystem initialised");
}

void Zenith_TacticalPointSystem::Shutdown()
{
	s_axPoints.Clear();
	s_axPointActive.Clear();
	s_uNextPointID = 0;
	s_bInitialised = false;

	Zenith_Log(LOG_CATEGORY_AI, "TacticalPointSystem shutdown");
}

void Zenith_TacticalPointSystem::Update()
{
	Zenith_Profiling::Scope xProfileScope(ZENITH_PROFILE_INDEX__AI_TACTICAL_UPDATE);

	// Validate owner entities still exist
	Zenith_Scene xActiveScene = Zenith_SceneManager::GetActiveScene();
	Zenith_SceneData* pxSceneData = Zenith_SceneManager::GetSceneData(xActiveScene);
	if (!pxSceneData)
	{
		return;
	}

	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		if (!s_axPointActive.Get(u))
		{
			continue;
		}

		Zenith_TacticalPoint& xPoint = s_axPoints.Get(u);

		// Check if occupied entity still exists
		if (xPoint.m_xOccupiedBy.IsValid())
		{
			Zenith_Entity xEntity = pxSceneData->TryGetEntity(xPoint.m_xOccupiedBy);
			if (!xEntity.IsValid())
			{
				xPoint.m_xOccupiedBy = Zenith_EntityID();
				xPoint.m_uFlags &= ~TACPOINT_FLAG_OCCUPIED;
			}
		}

		// Check if owner entity (dynamic points) still exists
		if ((xPoint.m_uFlags & TACPOINT_FLAG_DYNAMIC) && xPoint.m_xOwnerEntity.IsValid())
		{
			Zenith_Entity xEntity = pxSceneData->TryGetEntity(xPoint.m_xOwnerEntity);
			if (!xEntity.IsValid())
			{
				FreePointSlot(u);
			}
		}
	}
}

uint32_t Zenith_TacticalPointSystem::RegisterPoint(
	const Zenith_Maths::Vector3& xPos,
	TacticalPointType eType,
	const Zenith_Maths::Vector3& xFacing,
	Zenith_EntityID xOwner)
{
	uint32_t uSlot = AllocatePointSlot();

	Zenith_TacticalPoint& xPoint = s_axPoints.Get(uSlot);
	xPoint.m_xPosition = xPos;
	xPoint.m_xFacing = Zenith_Maths::Normalize(xFacing);
	xPoint.m_eType = eType;
	xPoint.m_uFlags = xOwner.IsValid() ? TACPOINT_FLAG_DYNAMIC : TACPOINT_FLAG_NONE;
	xPoint.m_fScore = 0.0f;
	xPoint.m_xOwnerEntity = xOwner;
	xPoint.m_xOccupiedBy = Zenith_EntityID();

	// Check if elevated
	if (xPos.y > 2.0f)
	{
		xPoint.m_uFlags |= TACPOINT_FLAG_ELEVATED;
	}

	return uSlot;
}

void Zenith_TacticalPointSystem::UnregisterPoint(uint32_t uPointID)
{
	if (uPointID < s_axPoints.GetSize() && s_axPointActive.Get(uPointID))
	{
		FreePointSlot(uPointID);
	}
}

void Zenith_TacticalPointSystem::UnregisterPointsByOwner(Zenith_EntityID xOwner)
{
	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		if (s_axPointActive.Get(u) && s_axPoints.Get(u).m_xOwnerEntity == xOwner)
		{
			FreePointSlot(u);
		}
	}
}

static bool PassesQueryFilter(const Zenith_TacticalPoint& xPoint, bool bActive, const Zenith_TacticalPointQuery& xQuery)
{
	if (!bActive) return false;
	if (!xQuery.m_bAnyType && xPoint.m_eType != xQuery.m_eType) return false;
	if (xQuery.m_bMustBeAvailable && !xPoint.IsAvailable()) return false;
	if ((xQuery.m_uRequiredFlags != 0) && ((xPoint.m_uFlags & xQuery.m_uRequiredFlags) != xQuery.m_uRequiredFlags)) return false;
	if ((xQuery.m_uExcludedFlags != 0) && ((xPoint.m_uFlags & xQuery.m_uExcludedFlags) != 0)) return false;
	float fDist = Zenith_Maths::Length(xPoint.m_xPosition - xQuery.m_xSearchCenter);
	if (fDist > xQuery.m_fSearchRadius) return false;
	return true;
}

const Zenith_TacticalPoint* Zenith_TacticalPointSystem::FindBestPoint(const Zenith_TacticalPointQuery& xQuery)
{
	const Zenith_TacticalPoint* pxBest = nullptr;
	float fBestScore = -FLT_MAX;

	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		const Zenith_TacticalPoint& xPoint = s_axPoints.Get(u);
		if (!PassesQueryFilter(xPoint, s_axPointActive.Get(u), xQuery))
		{
			continue;
		}

		Zenith_TacticalPointScore xScore = ScorePoint(xPoint, xQuery);
		if (xScore.m_fTotal > fBestScore)
		{
			fBestScore = xScore.m_fTotal;
			pxBest = &xPoint;
		}
	}

	return pxBest;
}

void Zenith_TacticalPointSystem::FindAllPoints(
	const Zenith_TacticalPointQuery& xQuery,
	Zenith_Vector<const Zenith_TacticalPoint*>& axResults,
	uint32_t uMaxResults)
{
	axResults.Clear();

	// Collect and score all valid points
	struct ScoredPoint
	{
		const Zenith_TacticalPoint* m_pxPoint;
		float m_fScore;
	};
	Zenith_Vector<ScoredPoint> axScored;

	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		const Zenith_TacticalPoint& xPoint = s_axPoints.Get(u);
		if (!PassesQueryFilter(xPoint, s_axPointActive.Get(u), xQuery))
		{
			continue;
		}

		ScoredPoint xScored;
		xScored.m_pxPoint = &xPoint;
		xScored.m_fScore = ScorePoint(xPoint, xQuery).m_fTotal;
		axScored.PushBack(xScored);
	}

	// Sort by score (descending) - simple bubble sort for small arrays
	for (uint32_t i = 0; i < axScored.GetSize(); ++i)
	{
		for (uint32_t j = i + 1; j < axScored.GetSize(); ++j)
		{
			if (axScored.Get(j).m_fScore > axScored.Get(i).m_fScore)
			{
				ScoredPoint xTemp = axScored.Get(i);
				axScored.Get(i) = axScored.Get(j);
				axScored.Get(j) = xTemp;
			}
		}
	}

	// Return top results
	uint32_t uCount = (axScored.GetSize() < uMaxResults) ? axScored.GetSize() : uMaxResults;
	for (uint32_t u = 0; u < uCount; ++u)
	{
		axResults.PushBack(axScored.Get(u).m_pxPoint);
	}
}

Zenith_Maths::Vector3 Zenith_TacticalPointSystem::FindBestCoverPosition(
	Zenith_EntityID xAgent,
	const Zenith_Maths::Vector3& xThreatPosition,
	float fMaxDistance)
{
	Zenith_Maths::Vector3 xAgentPos;
	if (!GetEntityPosition(xAgent, xAgentPos))
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	return FindBestCoverPosition(xAgentPos, xThreatPosition, fMaxDistance);
}

Zenith_Maths::Vector3 Zenith_TacticalPointSystem::FindBestCoverPosition(
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_Maths::Vector3& xThreatPosition,
	float fMaxDistance)
{
	CoverScoreContext xCtx;
	xCtx.m_xAgentPos = xAgentPos;
	xCtx.m_xThreatPos = xThreatPosition;
	xCtx.m_fMaxDistance = fMaxDistance;

	const Zenith_TacticalPoint* pxBest = FindBestPointOfType(
		xAgentPos, 0.0f, fMaxDistance,
		FilterCoverTypes, ScoreCoverPoint, &xCtx);

	if (pxBest)
	{
		return pxBest->m_xPosition;
	}

	return xAgentPos;  // No valid cover found, stay in place
}

Zenith_Maths::Vector3 Zenith_TacticalPointSystem::FindBestFlankPosition(
	Zenith_EntityID xAgent,
	const Zenith_Maths::Vector3& xTargetPosition,
	const Zenith_Maths::Vector3& xTargetFacing,
	float fMinDistance,
	float fMaxDistance)
{
	Zenith_Maths::Vector3 xAgentPos;
	if (!GetEntityPosition(xAgent, xAgentPos))
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	return FindBestFlankPosition(xAgentPos, xTargetPosition, xTargetFacing, fMinDistance, fMaxDistance);
}

Zenith_Maths::Vector3 Zenith_TacticalPointSystem::FindBestFlankPosition(
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_Maths::Vector3& xTargetPosition,
	const Zenith_Maths::Vector3& xTargetFacing,
	float fMinDistance,
	float fMaxDistance)
{
	FlankScoreContext xCtx;
	xCtx.m_xAgentPos = xAgentPos;
	xCtx.m_xTargetPos = xTargetPosition;
	xCtx.m_xTargetFacing = xTargetFacing;

	const Zenith_TacticalPoint* pxBest = FindBestPointOfType(
		xTargetPosition, fMinDistance, fMaxDistance,
		FilterFlankTypes, ScoreFlankPoint, &xCtx);

	if (pxBest)
	{
		return pxBest->m_xPosition;
	}

	// Generate a flank position if no tactical points available
	Zenith_Maths::Vector3 xRight = Zenith_Maths::Cross(xTargetFacing, Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f));
	xRight = Zenith_Maths::Normalize(xRight);

	// Choose side closer to agent
	Zenith_Maths::Vector3 xToAgent = xAgentPos - xTargetPosition;
	float fDotRight = Zenith_Maths::Dot(xToAgent, xRight);

	float fFlankDist = (fMinDistance + fMaxDistance) * 0.5f;
	if (fDotRight > 0)
	{
		return xTargetPosition + xRight * fFlankDist;
	}
	else
	{
		return xTargetPosition - xRight * fFlankDist;
	}
}

Zenith_Maths::Vector3 Zenith_TacticalPointSystem::FindBestOverwatchPosition(
	Zenith_EntityID xAgent,
	const Zenith_Maths::Vector3& xAreaToWatch,
	float fMinDistance,
	float fMaxDistance)
{
	Zenith_Maths::Vector3 xAgentPos;
	if (!GetEntityPosition(xAgent, xAgentPos))
	{
		return Zenith_Maths::Vector3(0.0f);
	}

	return FindBestOverwatchPosition(xAgentPos, xAreaToWatch, fMinDistance, fMaxDistance);
}

Zenith_Maths::Vector3 Zenith_TacticalPointSystem::FindBestOverwatchPosition(
	const Zenith_Maths::Vector3& xAgentPos,
	const Zenith_Maths::Vector3& xAreaToWatch,
	float fMinDistance,
	float fMaxDistance)
{
	OverwatchScoreContext xCtx;
	xCtx.m_xAgentPos = xAgentPos;
	xCtx.m_xAreaToWatch = xAreaToWatch;

	const Zenith_TacticalPoint* pxBest = FindBestPointOfType(
		xAreaToWatch, fMinDistance, fMaxDistance,
		FilterOverwatchTypes, ScoreOverwatchPoint, &xCtx);

	if (pxBest)
	{
		return pxBest->m_xPosition;
	}

	return xAgentPos;  // No valid position found
}

void Zenith_TacticalPointSystem::GenerateCoverPointsAround(const Zenith_Maths::Vector3& xCenter, float fRadius)
{
	// This would ideally use physics queries to find nearby geometry
	// and generate cover points behind obstacles
	// For now, create a basic grid of potential cover points

	const float fGridSpacing = 3.0f;
	const int32_t iGridSize = static_cast<int32_t>(fRadius / fGridSpacing);

	for (int32_t x = -iGridSize; x <= iGridSize; ++x)
	{
		for (int32_t z = -iGridSize; z <= iGridSize; ++z)
		{
			Zenith_Maths::Vector3 xPos = xCenter;
			xPos.x += static_cast<float>(x) * fGridSpacing;
			xPos.z += static_cast<float>(z) * fGridSpacing;

			float fDist = Zenith_Maths::Length(xPos - xCenter);
			if (fDist > fRadius)
			{
				continue;
			}

			// Raycast downward to find ground
			Zenith_Physics::RaycastResult xGroundResult = Zenith_Physics::Raycast(
				xPos + Zenith_Maths::Vector3(0.0f, 2.0f, 0.0f),
				Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f),
				5.0f);

			if (!xGroundResult.m_bHit)
			{
				continue;  // No ground here, skip this position
			}

			// Snap to ground position
			Zenith_Maths::Vector3 xGroundPos = xGroundResult.m_xHitPoint;

			// Raycast horizontally toward center to check for cover geometry
			Zenith_Maths::Vector3 xToCenter = Zenith_Maths::Normalize(xCenter - xGroundPos);
			Zenith_Maths::Vector3 xCoverCheckStart = xGroundPos + Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
			Zenith_Physics::RaycastResult xCoverResult = Zenith_Physics::Raycast(
				xCoverCheckStart, xToCenter, 2.0f);

			TacticalPointType eCoverType = TacticalPointType::COVER_HALF;
			if (xCoverResult.m_bHit && xCoverResult.m_fDistance < 1.5f)
			{
				// Check if cover is tall (full cover) or short (half cover)
				Zenith_Maths::Vector3 xHighCheck = xGroundPos + Zenith_Maths::Vector3(0.0f, 1.8f, 0.0f);
				Zenith_Physics::RaycastResult xHighResult = Zenith_Physics::Raycast(
					xHighCheck, xToCenter, 2.0f);
				eCoverType = xHighResult.m_bHit ? TacticalPointType::COVER_FULL : TacticalPointType::COVER_HALF;
			}
			else
			{
				continue;  // No nearby geometry for cover, skip this position
			}

			Zenith_Maths::Vector3 xFacing = -xToCenter;  // Face away from cover
			RegisterPoint(xGroundPos, eCoverType, xFacing, Zenith_EntityID());
		}
	}

	Zenith_Log(LOG_CATEGORY_AI, "Generated cover points around (%.1f, %.1f, %.1f) radius %.1f",
		xCenter.x, xCenter.y, xCenter.z, fRadius);
}

void Zenith_TacticalPointSystem::ClearGeneratedPoints()
{
	// Remove all points without an owner (generated points)
	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		if (s_axPointActive.Get(u) && !s_axPoints.Get(u).m_xOwnerEntity.IsValid())
		{
			FreePointSlot(u);
		}
	}
}

bool Zenith_TacticalPointSystem::OccupyPoint(uint32_t uPointID, Zenith_EntityID xAgent)
{
	if (uPointID >= s_axPoints.GetSize() || !s_axPointActive.Get(uPointID))
	{
		return false;
	}

	Zenith_TacticalPoint& xPoint = s_axPoints.Get(uPointID);

	if (!xPoint.IsAvailable())
	{
		return false;
	}

	xPoint.m_xOccupiedBy = xAgent;
	xPoint.m_uFlags |= TACPOINT_FLAG_OCCUPIED;
	xPoint.m_uFlags &= ~TACPOINT_FLAG_RESERVED;  // Clear reservation

	return true;
}

void Zenith_TacticalPointSystem::ReleasePoint(uint32_t uPointID, Zenith_EntityID xAgent)
{
	if (uPointID >= s_axPoints.GetSize() || !s_axPointActive.Get(uPointID))
	{
		return;
	}

	Zenith_TacticalPoint& xPoint = s_axPoints.Get(uPointID);

	if (xPoint.m_xOccupiedBy == xAgent)
	{
		xPoint.m_xOccupiedBy = Zenith_EntityID();
		xPoint.m_uFlags &= ~TACPOINT_FLAG_OCCUPIED;
	}
}

bool Zenith_TacticalPointSystem::ReservePoint(uint32_t uPointID, Zenith_EntityID xAgent)
{
	if (uPointID >= s_axPoints.GetSize() || !s_axPointActive.Get(uPointID))
	{
		return false;
	}

	Zenith_TacticalPoint& xPoint = s_axPoints.Get(uPointID);

	if (!xPoint.IsAvailable())
	{
		return false;
	}

	xPoint.m_xOccupiedBy = xAgent;  // Track who reserved
	xPoint.m_uFlags |= TACPOINT_FLAG_RESERVED;

	return true;
}

void Zenith_TacticalPointSystem::UnreservePoint(uint32_t uPointID, Zenith_EntityID xAgent)
{
	if (uPointID >= s_axPoints.GetSize() || !s_axPointActive.Get(uPointID))
	{
		return;
	}

	Zenith_TacticalPoint& xPoint = s_axPoints.Get(uPointID);

	if ((xPoint.m_uFlags & TACPOINT_FLAG_RESERVED) && xPoint.m_xOccupiedBy == xAgent)
	{
		xPoint.m_xOccupiedBy = Zenith_EntityID();
		xPoint.m_uFlags &= ~TACPOINT_FLAG_RESERVED;
	}
}

Zenith_TacticalPoint* Zenith_TacticalPointSystem::GetPoint(uint32_t uPointID)
{
	if (uPointID < s_axPoints.GetSize() && s_axPointActive.Get(uPointID))
	{
		return &s_axPoints.Get(uPointID);
	}
	return nullptr;
}

const Zenith_TacticalPoint* Zenith_TacticalPointSystem::GetPointConst(uint32_t uPointID)
{
	if (uPointID < s_axPoints.GetSize() && s_axPointActive.Get(uPointID))
	{
		return &s_axPoints.Get(uPointID);
	}
	return nullptr;
}

uint32_t Zenith_TacticalPointSystem::GetPointCount()
{
	uint32_t uCount = 0;
	for (uint32_t u = 0; u < s_axPointActive.GetSize(); ++u)
	{
		if (s_axPointActive.Get(u))
		{
			++uCount;
		}
	}
	return uCount;
}

Zenith_TacticalPointScore Zenith_TacticalPointSystem::ScorePoint(
	const Zenith_TacticalPoint& xPoint,
	const Zenith_TacticalPointQuery& xQuery)
{
	Zenith_TacticalPointScore xScore;

	// Distance score (closer to search center is better)
	float fDist = Zenith_Maths::Length(xPoint.m_xPosition - xQuery.m_xSearchCenter);
	xScore.m_fDistanceScore = 1.0f - (fDist / xQuery.m_fSearchRadius);

	// Cover score (if threat position provided)
	if (xQuery.m_bHasThreat)
	{
		xScore.m_fCoverScore = EvaluateCoverFromThreat(xPoint.m_xPosition, xQuery.m_xThreatPosition);
	}
	else
	{
		xScore.m_fCoverScore = 0.5f;  // Neutral score
	}

	// Visibility score (based on type - overwatch has best visibility)
	static const float s_afVisibilityByType[] =
	{
		0.3f,  // COVER_FULL - limits visibility
		0.7f,  // COVER_HALF
		0.6f,  // FLANK_POSITION
		1.0f,  // OVERWATCH
		0.5f,  // PATROL_WAYPOINT
		0.5f,  // AMBUSH
		0.5f,  // RETREAT
	};
	static_assert(sizeof(s_afVisibilityByType) / sizeof(s_afVisibilityByType[0]) == static_cast<size_t>(TacticalPointType::COUNT),
		"s_afVisibilityByType must match TacticalPointType::COUNT");
	uint8_t uScoreTypeIndex = static_cast<uint8_t>(xPoint.m_eType);
	xScore.m_fVisibilityScore = (uScoreTypeIndex < static_cast<uint8_t>(TacticalPointType::COUNT))
		? s_afVisibilityByType[uScoreTypeIndex]
		: 0.5f;

	// Elevation score
	xScore.m_fElevationScore = (xPoint.m_uFlags & TACPOINT_FLAG_ELEVATED) ? 1.0f : 0.0f;
	xScore.m_fElevationScore += xPoint.m_xPosition.y * 0.05f;  // Small bonus for height

	// Calculate weighted total
	xScore.m_fTotal = xScore.m_fDistanceScore * s_fDistanceWeight +
		xScore.m_fCoverScore * s_fCoverWeight +
		xScore.m_fVisibilityScore * s_fVisibilityWeight +
		xScore.m_fElevationScore * s_fElevationWeight;

	return xScore;
}

#ifdef ZENITH_TOOLS
void Zenith_TacticalPointSystem::DebugDraw()
{
	if (!Zenith_AIDebugVariables::s_bEnableAllAIDebug)
	{
		return;
	}

	for (uint32_t u = 0; u < s_axPoints.GetSize(); ++u)
	{
		if (s_axPointActive.Get(u))
		{
			const Zenith_TacticalPoint& xPoint = s_axPoints.Get(u);

			// Check debug flags based on point type
			bool bShouldDraw = false;
			switch (xPoint.m_eType)
			{
			case TacticalPointType::COVER_FULL:
			case TacticalPointType::COVER_HALF:
				bShouldDraw = Zenith_AIDebugVariables::s_bDrawCoverPoints;
				break;
			case TacticalPointType::FLANK_POSITION:
				bShouldDraw = Zenith_AIDebugVariables::s_bDrawFlankPositions;
				break;
			case TacticalPointType::OVERWATCH:
			case TacticalPointType::PATROL_WAYPOINT:
			case TacticalPointType::AMBUSH:
			case TacticalPointType::RETREAT:
				bShouldDraw = Zenith_AIDebugVariables::s_bDrawCoverPoints; // Use cover points toggle for all tactical points
				break;
			default:
				bShouldDraw = true;
				break;
			}

			if (bShouldDraw)
			{
				DebugDrawPoint(xPoint);
			}
		}
	}
}

void Zenith_TacticalPointSystem::DebugDrawPoint(const Zenith_TacticalPoint& xPoint)
{
	// Color and score from lookup table
	uint8_t uTypeIndex = static_cast<uint8_t>(xPoint.m_eType);
	Zenith_Maths::Vector3 xColor = (uTypeIndex < static_cast<uint8_t>(TacticalPointType::COUNT))
		? s_axTypeInfo[uTypeIndex].m_xColor
		: Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);

	// Dim if occupied
	if (xPoint.IsOccupied())
	{
		xColor *= 0.5f;
	}
	else if (xPoint.IsReserved())
	{
		xColor *= 0.7f;
	}

	// Draw sphere at position
	Flux_Primitives::AddSphere(xPoint.m_xPosition, 0.3f, xColor);

	// Draw facing direction
	Zenith_Maths::Vector3 xFacingEnd = xPoint.m_xPosition + xPoint.m_xFacing * 0.8f;
	Flux_Primitives::AddLine(xPoint.m_xPosition, xFacingEnd, xColor);

	// Draw vertical line for elevated points
	if (xPoint.m_uFlags & TACPOINT_FLAG_ELEVATED)
	{
		Zenith_Maths::Vector3 xTop = xPoint.m_xPosition + Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f);
		Flux_Primitives::AddLine(xPoint.m_xPosition, xTop, Zenith_Maths::Vector3(0.0f, 1.0f, 1.0f));
	}

	// Draw score if enabled
	if (Zenith_AIDebugVariables::s_bDrawTacticalScores)
	{
		// Calculate a base score based on point type and properties
		// (Since m_fScore is only set during queries, we compute a display score here)
		float fDisplayScore = (uTypeIndex < static_cast<uint8_t>(TacticalPointType::COUNT))
			? s_axTypeInfo[uTypeIndex].m_fBaseDisplayScore
			: 1.0f;

		// Bonus for elevation
		if (xPoint.m_uFlags & TACPOINT_FLAG_ELEVATED)
		{
			fDisplayScore += 1.0f;
		}

		// Penalty if occupied/reserved
		if (xPoint.IsOccupied())
		{
			fDisplayScore *= 0.3f;
		}
		else if (xPoint.IsReserved())
		{
			fDisplayScore *= 0.5f;
		}

		// Use stored score if it's non-zero (was calculated by a query)
		if (xPoint.m_fScore != 0.0f)
		{
			fDisplayScore = xPoint.m_fScore;
		}

		// Visual indicator of score (taller = higher score)
		float fScoreHeight = fDisplayScore * 0.3f;
		Zenith_Maths::Vector3 xScoreTop = xPoint.m_xPosition + Zenith_Maths::Vector3(0.0f, 0.5f + fScoreHeight, 0.0f);
		Flux_Primitives::AddLine(xPoint.m_xPosition + Zenith_Maths::Vector3(0.0f, 0.5f, 0.0f), xScoreTop, Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f), 0.03f);

		// Add a small sphere at top to make it more visible
		Flux_Primitives::AddSphere(xScoreTop, 0.08f, Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f));
	}
}
#endif

// ========== Internal Helpers ==========

uint32_t Zenith_TacticalPointSystem::AllocatePointSlot()
{
	// Try to find an empty slot
	for (uint32_t u = 0; u < s_axPointActive.GetSize(); ++u)
	{
		if (!s_axPointActive.Get(u))
		{
			s_axPointActive.Get(u) = true;
			return u;
		}
	}

	// No empty slot, add new one
	uint32_t uIndex = s_axPoints.GetSize();
	s_axPoints.PushBack(Zenith_TacticalPoint());
	s_axPointActive.PushBack(true);
	return uIndex;
}

void Zenith_TacticalPointSystem::FreePointSlot(uint32_t uIndex)
{
	if (uIndex < s_axPointActive.GetSize())
	{
		s_axPointActive.Get(uIndex) = false;
		s_axPoints.Get(uIndex) = Zenith_TacticalPoint();  // Clear data
	}
}

float Zenith_TacticalPointSystem::EvaluateCoverFromThreat(
	const Zenith_Maths::Vector3& xPoint,
	const Zenith_Maths::Vector3& xThreat)
{
	// Calculate how well protected the point is from the threat
	Zenith_Maths::Vector3 xToThreat = xThreat - xPoint;
	float fDist = Zenith_Maths::Length(xToThreat);

	// Further from threat = better cover (up to a point)
	float fDistScore = fDist / 20.0f;  // Normalize to ~20m
	fDistScore = (fDistScore > 1.0f) ? 1.0f : fDistScore;

	// Raycast check for actual occlusion
	// Check from standing height at point toward threat's center
	Zenith_Maths::Vector3 xEyeLevel = xPoint + Zenith_Maths::Vector3(0.0f, 1.5f, 0.0f);
	Zenith_Maths::Vector3 xThreatCenter = xThreat + Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	Zenith_Maths::Vector3 xDirection = xThreatCenter - xEyeLevel;
	float fCheckDist = Zenith_Maths::Length(xDirection);

	if (fCheckDist > 0.001f)
	{
		Zenith_Physics::RaycastResult xResult = Zenith_Physics::Raycast(xEyeLevel, xDirection, fCheckDist);
		if (xResult.m_bHit)
		{
			// Something is blocking line of sight to threat
			// The closer the blocking geometry, the better the cover
			float fOcclusionScore = 1.0f - (xResult.m_fDistance / fCheckDist);
			return fDistScore * 0.5f + fOcclusionScore * 0.5f;  // Blend distance and occlusion
		}
	}

	// No occlusion, just use distance score
	return fDistScore * 0.5f;  // Lower score when exposed
}

float Zenith_TacticalPointSystem::EvaluateFlankAngle(
	const Zenith_Maths::Vector3& xPoint,
	const Zenith_Maths::Vector3& xTarget,
	const Zenith_Maths::Vector3& xTargetFacing)
{
	// Calculate how good the flank angle is
	// Best flank is perpendicular to target's facing direction

	Zenith_Maths::Vector3 xToPoint = Zenith_Maths::Normalize(xPoint - xTarget);
	Zenith_Maths::Vector3 xNormFacing = Zenith_Maths::Normalize(xTargetFacing);

	// Dot product: 0 = perpendicular (best), 1/-1 = front/back (worst)
	float fDot = std::abs(Zenith_Maths::Dot(xToPoint, xNormFacing));

	// Convert to score (0 = front/back, 1 = perpendicular)
	return 1.0f - fDot;
}

const char* GetTacticalPointTypeName(TacticalPointType eType)
{
	switch (eType)
	{
	case TacticalPointType::COVER_FULL:      return "Full Cover";
	case TacticalPointType::COVER_HALF:      return "Half Cover";
	case TacticalPointType::FLANK_POSITION:  return "Flank";
	case TacticalPointType::OVERWATCH:       return "Overwatch";
	case TacticalPointType::PATROL_WAYPOINT: return "Patrol";
	case TacticalPointType::AMBUSH:          return "Ambush";
	case TacticalPointType::RETREAT:         return "Retreat";
	default:                                 return "Unknown";
	}
}
