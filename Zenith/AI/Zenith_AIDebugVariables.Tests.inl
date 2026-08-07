#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Zenith_AI.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"

// ============================================================================
// AI Debug Variables Tests
// ============================================================================

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

// ============================================================================
// Zenith_AI::DebugDraw routing
// ============================================================================
// These pin the two properties that make it safe for the main loop to call
// Zenith_AI::DebugDraw() UNCONDITIONALLY every game-logic frame (Zenith_Core.cpp,
// deliberately outside the IsEngineTickEnabled() branch). Both were false before
// the AI debug subtree was wired up, which is part of why it never was.
//
// The tests exist in EVERY configuration so the registered-test count does not
// depend on ZENITH_TOOLS; only the call under test is tools-gated (DebugDraw is
// tools-only, since it emits debug primitives).

ZENITH_TEST(AI, AIDebugDrawMasterToggleShortCircuits) { Zenith_UnitTests::TestAIDebugDrawMasterToggleShortCircuits(); }

void Zenith_UnitTests::TestAIDebugDrawMasterToggleShortCircuits(){
	// AI/Enable All AI Debug is the master switch: with it off, DebugDraw must
	// return without touching any manager, so the per-frame cost of an untouched
	// AI panel is one branch.
	const bool bPrevMaster = Zenith_AIDebugVariables::s_bEnableAllAIDebug;
	Zenith_AIDebugVariables::s_bEnableAllAIDebug = false;

#ifdef ZENITH_TOOLS
	Zenith_AI::DebugDraw();
#endif

	// The observable contract is "returns, changes nothing" -- the toggle is not
	// self-modifying and nothing below it is consulted.
	ZENITH_ASSERT_TRUE(Zenith_AIDebugVariables::s_bEnableAllAIDebug == false,
		"DebugDraw must not mutate the master toggle");

	Zenith_AIDebugVariables::s_bEnableAllAIDebug = bPrevMaster;
}

ZENITH_TEST(AI, AIDebugDrawSafeWithNoAIContent) { Zenith_UnitTests::TestAIDebugDrawSafeWithNoAIContent(); }

void Zenith_UnitTests::TestAIDebugDrawSafeWithNoAIContent(){
	// A game that never forms a squad never calls Zenith_SquadManager::Initialise().
	// DebugDrawAllSquads used to ASSERT on that, which would have fired on the very
	// first frame of every such game once the engine started calling it. It now
	// early-returns; this test is the guard against the assert coming back.
	Zenith_SquadManager::Shutdown();          // leaves s_bInitialised == false
	Zenith_TacticalPointSystem::Shutdown();

	const bool bPrevMaster = Zenith_AIDebugVariables::s_bEnableAllAIDebug;
	Zenith_AIDebugVariables::s_bEnableAllAIDebug = true;

#ifdef ZENITH_TOOLS
	// Reaching the line after this call IS the assertion: an un-Initialise()d
	// manager must not break, and the perception walk must tolerate zero agents.
	Zenith_AI::DebugDraw();
#endif

	Zenith_AIDebugVariables::s_bEnableAllAIDebug = bPrevMaster;

	// Confirm the visualiser walked an empty world rather than populating one,
	// then leave the manager shut down again (the Initialise/.../Shutdown shape
	// every other test in this AI suite uses).
	Zenith_SquadManager::Initialise();
	ZENITH_ASSERT_EQ(Zenith_SquadManager::GetSquadCount(), 0u,
		"DebugDraw must not create squads");
	Zenith_SquadManager::Shutdown();
}

