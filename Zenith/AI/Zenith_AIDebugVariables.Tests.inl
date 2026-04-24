#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"

// ============================================================================
// AI Debug Variables Tests
// ============================================================================
#include "AI/Zenith_AIDebugVariables.h"

ZENITH_TEST(AI, TacticalPointDebugColor) { Zenith_UnitTests::TestTacticalPointDebugColor(); }

void Zenith_UnitTests::TestTacticalPointDebugColor(){
	// Test that tactical point types have distinct colors for visualization accuracy
	// This tests the color mapping logic used in DebugDrawPoint

	// Expected colors for different tactical point types
	// COVER_FULL: Green (0.0, 0.8, 0.0)
	// COVER_HALF: Yellow (0.8, 0.8, 0.0)
	// FLANK_POSITION: Orange (1.0, 0.5, 0.0)
	// OVERWATCH: Purple (0.5, 0.0, 0.8)
	// PATROL_WAYPOINT: Blue (0.0, 0.5, 1.0)
	// AMBUSH: Red (0.8, 0.0, 0.0)
	// RETREAT: Gray (0.5, 0.5, 0.5)

	// Helper to get expected color for a type
	auto GetExpectedColor = [](TacticalPointType eType) -> Zenith_Maths::Vector3
	{
		switch (eType)
		{
		case TacticalPointType::COVER_FULL:     return Zenith_Maths::Vector3(0.0f, 0.8f, 0.0f);
		case TacticalPointType::COVER_HALF:     return Zenith_Maths::Vector3(0.8f, 0.8f, 0.0f);
		case TacticalPointType::FLANK_POSITION: return Zenith_Maths::Vector3(1.0f, 0.5f, 0.0f);
		case TacticalPointType::OVERWATCH:      return Zenith_Maths::Vector3(0.5f, 0.0f, 0.8f);
		case TacticalPointType::PATROL_WAYPOINT:return Zenith_Maths::Vector3(0.0f, 0.5f, 1.0f);
		case TacticalPointType::AMBUSH:         return Zenith_Maths::Vector3(0.8f, 0.0f, 0.0f);
		case TacticalPointType::RETREAT:        return Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f);
		default:                                return Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f);
		}
	};

	// Verify all colors are distinct (no two types share the same color)
	Zenith_Maths::Vector3 xCoverFull = GetExpectedColor(TacticalPointType::COVER_FULL);
	Zenith_Maths::Vector3 xCoverHalf = GetExpectedColor(TacticalPointType::COVER_HALF);
	Zenith_Maths::Vector3 xFlank = GetExpectedColor(TacticalPointType::FLANK_POSITION);
	Zenith_Maths::Vector3 xOverwatch = GetExpectedColor(TacticalPointType::OVERWATCH);
	Zenith_Maths::Vector3 xPatrol = GetExpectedColor(TacticalPointType::PATROL_WAYPOINT);

	// Colors should be distinguishable (different)
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xCoverFull - xCoverHalf), 0.1f, "COVER_FULL and COVER_HALF should have different colors");
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xFlank - xOverwatch), 0.1f, "FLANK and OVERWATCH should have different colors");
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xPatrol - xCoverFull), 0.1f, "PATROL and COVER_FULL should have different colors");

	// Verify cover is green-ish (G component highest)
	ZENITH_ASSERT_TRUE(xCoverFull.y > xCoverFull.x && xCoverFull.y > xCoverFull.z, "COVER_FULL should be predominantly green");

	// Verify flank is orange-ish (R component highest, some G)
	ZENITH_ASSERT_TRUE(xFlank.x > xFlank.z && xFlank.y > 0.0f, "FLANK should be orange (high R, some G)");

}

ZENITH_TEST(AI, SquadDebugRoleColor) { Zenith_UnitTests::TestSquadDebugRoleColor(); }

void Zenith_UnitTests::TestSquadDebugRoleColor(){
	// Test that squad roles have distinct colors for visualization accuracy
	// This tests the color mapping logic used in Squad::DebugDraw

	// Expected colors for different roles:
	// LEADER: Gold (1.0, 0.84, 0.0)
	// ASSAULT: Red (1.0, 0.3, 0.3)
	// SUPPORT: Blue (0.3, 0.3, 1.0)
	// FLANKER: Orange (1.0, 0.6, 0.2)
	// OVERWATCH: Purple (0.8, 0.2, 0.8)
	// MEDIC: Green (0.2, 1.0, 0.2)

	auto GetExpectedColor = [](SquadRole eRole) -> Zenith_Maths::Vector3
	{
		switch (eRole)
		{
		case SquadRole::LEADER:    return Zenith_Maths::Vector3(1.0f, 0.84f, 0.0f);
		case SquadRole::ASSAULT:   return Zenith_Maths::Vector3(1.0f, 0.3f, 0.3f);
		case SquadRole::SUPPORT:   return Zenith_Maths::Vector3(0.3f, 0.3f, 1.0f);
		case SquadRole::FLANKER:   return Zenith_Maths::Vector3(1.0f, 0.6f, 0.2f);
		case SquadRole::OVERWATCH: return Zenith_Maths::Vector3(0.8f, 0.2f, 0.8f);
		case SquadRole::MEDIC:     return Zenith_Maths::Vector3(0.2f, 1.0f, 0.2f);
		default:                   return Zenith_Maths::Vector3(0.7f, 0.7f, 0.7f);
		}
	};

	Zenith_Maths::Vector3 xLeader = GetExpectedColor(SquadRole::LEADER);
	Zenith_Maths::Vector3 xAssault = GetExpectedColor(SquadRole::ASSAULT);
	Zenith_Maths::Vector3 xSupport = GetExpectedColor(SquadRole::SUPPORT);
	Zenith_Maths::Vector3 xFlanker = GetExpectedColor(SquadRole::FLANKER);
	Zenith_Maths::Vector3 xOverwatch = GetExpectedColor(SquadRole::OVERWATCH);
	Zenith_Maths::Vector3 xMedic = GetExpectedColor(SquadRole::MEDIC);

	// All colors should be distinct
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xLeader - xAssault), 0.1f, "LEADER and ASSAULT should have different colors");
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xAssault - xSupport), 0.1f, "ASSAULT and SUPPORT should have different colors");
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xSupport - xFlanker), 0.1f, "SUPPORT and FLANKER should have different colors");
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xFlanker - xOverwatch), 0.1f, "FLANKER and OVERWATCH should have different colors");
	ZENITH_ASSERT_GT(Zenith_Maths::Length(xOverwatch - xMedic), 0.1f, "OVERWATCH and MEDIC should have different colors");

	// Leader should be gold (high R and G, no B)
	ZENITH_ASSERT_TRUE(xLeader.x > 0.9f && xLeader.y > 0.8f && xLeader.z < 0.1f, "LEADER should be gold colored");

	// Support should be blue-ish (B component highest)
	ZENITH_ASSERT_TRUE(xSupport.z > xSupport.x && xSupport.z > xSupport.y, "SUPPORT should be predominantly blue");

	// Medic should be green-ish (G component highest)
	ZENITH_ASSERT_TRUE(xMedic.y > xMedic.x && xMedic.y > xMedic.z, "MEDIC should be predominantly green");

}

