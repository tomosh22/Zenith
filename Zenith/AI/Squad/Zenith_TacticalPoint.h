#pragma once

#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"
#include "Collections/Zenith_Vector.h"

/**
 * Types of tactical positions
 */
enum class TacticalPointType : uint8_t
{
	COVER_FULL,       // Full cover - completely concealed
	COVER_HALF,       // Half cover - partially protected
	FLANK_POSITION,   // Good for attacking from the side
	OVERWATCH,        // Elevated position with good sight lines
	PATROL_WAYPOINT,  // Part of a patrol route
	AMBUSH,           // Good for surprise attacks
	RETREAT,          // Safe fallback position

	COUNT
};

/**
 * Flags for tactical point properties
 */
enum TacticalPointFlags : uint32_t
{
	TACPOINT_FLAG_NONE = 0,
	TACPOINT_FLAG_OCCUPIED = 1 << 0,      // Currently in use
	TACPOINT_FLAG_RESERVED = 1 << 1,      // Reserved for future use
	TACPOINT_FLAG_DYNAMIC = 1 << 2,       // Can be destroyed/moved
	TACPOINT_FLAG_ELEVATED = 1 << 3,      // Elevated position
	TACPOINT_FLAG_INDOORS = 1 << 4,       // Inside a structure
	TACPOINT_FLAG_COMPROMISED = 1 << 5,   // Known to enemies
};

/**
 * A tactical position in the world
 */
struct Zenith_TacticalPoint
{
	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xFacing;         // Recommended facing direction
	TacticalPointType m_eType = TacticalPointType::COVER_HALF;
	uint32_t m_uFlags = TACPOINT_FLAG_NONE;
	float m_fScore = 0.0f;                   // Evaluation score (higher = better)

	Zenith_EntityID m_xOwnerEntity;          // Entity that created this point (if any)
	Zenith_EntityID m_xOccupiedBy;           // Entity currently using this point

	bool IsOccupied() const { return (m_uFlags & TACPOINT_FLAG_OCCUPIED) != 0; }
	bool IsReserved() const { return (m_uFlags & TACPOINT_FLAG_RESERVED) != 0; }
	bool IsAvailable() const { return !IsOccupied() && !IsReserved(); }
};

/**
 * Query filter for finding tactical points
 */
struct Zenith_TacticalPointQuery
{
	Zenith_Maths::Vector3 m_xSearchCenter;
	float m_fSearchRadius = 20.0f;

	TacticalPointType m_eType = TacticalPointType::COVER_HALF;
	bool m_bAnyType = false;                 // If true, ignore type filter

	bool m_bMustBeAvailable = true;          // Only return unoccupied points
	uint32_t m_uRequiredFlags = 0;           // Must have these flags
	uint32_t m_uExcludedFlags = 0;           // Must NOT have these flags

	// Optional: scoring context
	Zenith_Maths::Vector3 m_xThreatPosition; // Position of threat (for cover scoring)
	bool m_bHasThreat = false;
	Zenith_EntityID m_xRequestingAgent;      // Agent making the query
};

/**
 * Result of tactical point scoring
 */
struct Zenith_TacticalPointScore
{
	float m_fDistanceScore = 0.0f;       // Based on distance to query center
	float m_fCoverScore = 0.0f;          // Based on protection from threat
	float m_fVisibilityScore = 0.0f;     // Based on sight lines
	float m_fElevationScore = 0.0f;      // Bonus for elevated positions
	float m_fTotal = 0.0f;               // Weighted total
};

/**
 * Zenith_TacticalPointSystem - Manages tactical positions for AI decision-making
 *
 * Provides:
 * - Registration of static and dynamic tactical points
 * - Query system to find best positions based on criteria
 * - Dynamic cover generation around obstacles
 * - Scoring based on threat position, visibility, elevation
 * - Occupation tracking to prevent multiple agents using same point
 */
class Zenith_TacticalPointSystem
{
public:
	static void Initialise();
	static void Shutdown();
	static void Update();

	// Point registration
	static uint32_t RegisterPoint(const Zenith_Maths::Vector3& xPos,
		TacticalPointType eType,
		const Zenith_Maths::Vector3& xFacing = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
		Zenith_EntityID xOwner = Zenith_EntityID());

	static void UnregisterPoint(uint32_t uPointID);
	static void UnregisterPointsByOwner(Zenith_EntityID xOwner);

	// Query system
	static const Zenith_TacticalPoint* FindBestPoint(const Zenith_TacticalPointQuery& xQuery);
	static void FindAllPoints(const Zenith_TacticalPointQuery& xQuery, Zenith_Vector<const Zenith_TacticalPoint*>& axResults, uint32_t uMaxResults = 10);

	// Specialized queries
	static Zenith_Maths::Vector3 FindBestCoverPosition(
		Zenith_EntityID xAgent,
		const Zenith_Maths::Vector3& xThreatPosition,
		float fMaxDistance);

	// Overload that takes agent position directly (for testing or when position is known)
	static Zenith_Maths::Vector3 FindBestCoverPosition(
		const Zenith_Maths::Vector3& xAgentPosition,
		const Zenith_Maths::Vector3& xThreatPosition,
		float fMaxDistance);

	static Zenith_Maths::Vector3 FindBestFlankPosition(
		Zenith_EntityID xAgent,
		const Zenith_Maths::Vector3& xTargetPosition,
		const Zenith_Maths::Vector3& xTargetFacing,
		float fMinDistance,
		float fMaxDistance);

	// Overload that takes agent position directly (for testing or when position is known)
	static Zenith_Maths::Vector3 FindBestFlankPosition(
		const Zenith_Maths::Vector3& xAgentPosition,
		const Zenith_Maths::Vector3& xTargetPosition,
		const Zenith_Maths::Vector3& xTargetFacing,
		float fMinDistance,
		float fMaxDistance);

	static Zenith_Maths::Vector3 FindBestOverwatchPosition(
		Zenith_EntityID xAgent,
		const Zenith_Maths::Vector3& xAreaToWatch,
		float fMinDistance,
		float fMaxDistance);

	// Overload that takes agent position directly (for testing or when position is known)
	static Zenith_Maths::Vector3 FindBestOverwatchPosition(
		const Zenith_Maths::Vector3& xAgentPosition,
		const Zenith_Maths::Vector3& xAreaToWatch,
		float fMinDistance,
		float fMaxDistance);

	// Dynamic generation
	static void GenerateCoverPointsAround(const Zenith_Maths::Vector3& xCenter, float fRadius);
	static void ClearGeneratedPoints();

	// Occupation
	static bool OccupyPoint(uint32_t uPointID, Zenith_EntityID xAgent);
	static void ReleasePoint(uint32_t uPointID, Zenith_EntityID xAgent);
	static bool ReservePoint(uint32_t uPointID, Zenith_EntityID xAgent);
	static void UnreservePoint(uint32_t uPointID, Zenith_EntityID xAgent);

	// Point access
	static Zenith_TacticalPoint* GetPoint(uint32_t uPointID);
	static const Zenith_TacticalPoint* GetPointConst(uint32_t uPointID);
	static uint32_t GetPointCount();

	// Scoring (public for custom queries)
	static Zenith_TacticalPointScore ScorePoint(const Zenith_TacticalPoint& xPoint, const Zenith_TacticalPointQuery& xQuery);

#ifdef ZENITH_TOOLS
	static void DebugDraw();
	static void DebugDrawPoint(const Zenith_TacticalPoint& xPoint);
#endif

private:
	static Zenith_Vector<Zenith_TacticalPoint> s_axPoints;
	static Zenith_Vector<bool> s_axPointActive;   // Sparse array - tracks which indices are valid
	static uint32_t s_uNextPointID;
	static bool s_bInitialised;

	// Scoring weights
	static float s_fDistanceWeight;
	static float s_fCoverWeight;
	static float s_fVisibilityWeight;
	static float s_fElevationWeight;

	static uint32_t AllocatePointSlot();
	static void FreePointSlot(uint32_t uIndex);
	static float EvaluateCoverFromThreat(const Zenith_Maths::Vector3& xPoint, const Zenith_Maths::Vector3& xThreat);
	static float EvaluateFlankAngle(const Zenith_Maths::Vector3& xPoint, const Zenith_Maths::Vector3& xTarget, const Zenith_Maths::Vector3& xTargetFacing);
};

/**
 * Get tactical point type name as string
 */
const char* GetTacticalPointTypeName(TacticalPointType eType);
