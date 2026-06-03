#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Squad/Zenith_TacticalPoint.h"
#include "ZenithECS/Zenith_Scene.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

// ============================================================================
// Tactical Point Tests
// ============================================================================
ZENITH_TEST(AI, TacticalPointRegistration) { Zenith_UnitTests::TestTacticalPointRegistration(); }
void Zenith_UnitTests::TestTacticalPointRegistration(){
	Zenith_TacticalPointSystem::Initialise();

	Zenith_Maths::Vector3 xPos(10.0f, 0.0f, 10.0f);
	Zenith_EntityID xOwner;
	xOwner.m_uIndex = 1001;

	Zenith_TacticalPointSystem::RegisterPoint(xPos, TacticalPointType::COVER_FULL,
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), xOwner);

	// Query for cover points
	Zenith_TacticalPointQuery xQuery;
	xQuery.m_xSearchCenter = xPos;
	xQuery.m_fSearchRadius = 5.0f;
	xQuery.m_eType = TacticalPointType::COVER_FULL;
	xQuery.m_bMustBeAvailable = false;

	Zenith_Vector<const Zenith_TacticalPoint*> axPoints;
	Zenith_TacticalPointSystem::FindAllPoints(xQuery, axPoints);

	ZENITH_ASSERT_GE(axPoints.GetSize(), 1, "Should have at least 1 cover point");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, TacticalPointCoverScoring) { Zenith_UnitTests::TestTacticalPointCoverScoring(); }

void Zenith_UnitTests::TestTacticalPointCoverScoring(){
	Zenith_TacticalPointSystem::Initialise();

	// Register a cover point
	Zenith_Maths::Vector3 xCoverPos(10.0f, 0.0f, 0.0f);
	Zenith_TacticalPointSystem::RegisterPoint(xCoverPos, TacticalPointType::COVER_FULL,
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f), Zenith_EntityID());

	// Agent at origin, threat at (20, 0, 0)
	// Cover point is between them - good cover
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(20.0f, 0.0f, 0.0f);

	// Use the overload that takes agent position directly
	Zenith_Maths::Vector3 xBestCover = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 30.0f);

	// Should find the cover point
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xBestCover - xCoverPos), 1.0f, "Should find cover point near registered position");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, TacticalPointFlankScoring) { Zenith_UnitTests::TestTacticalPointFlankScoring(); }

void Zenith_UnitTests::TestTacticalPointFlankScoring(){
	Zenith_TacticalPointSystem::Initialise();

	// Register flank positions on sides of target
	Zenith_Maths::Vector3 xTargetPos(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xFlankLeft(5.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xFlankRight(15.0f, 0.0f, 10.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xFlankLeft, TacticalPointType::FLANK_POSITION,
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f), Zenith_EntityID());
	Zenith_TacticalPointSystem::RegisterPoint(xFlankRight, TacticalPointType::FLANK_POSITION,
		Zenith_Maths::Vector3(-1.0f, 0.0f, 0.0f), Zenith_EntityID());

	// Agent approaching from front (at z=0, in front of target at z=10)
	Zenith_Maths::Vector3 xAgentPos(10.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xTargetFacing(0.0f, 0.0f, -1.0f); // Facing toward agent

	// Use the overload that takes agent position directly
	Zenith_Maths::Vector3 xBestFlank = Zenith_TacticalPointSystem::FindBestFlankPosition(
		xAgentPos, xTargetPos, xTargetFacing, 1.0f, 20.0f);

	// Should find one of the flank positions (to the side)
	float fDistToLeft = Zenith_Maths::Length(xBestFlank - xFlankLeft);
	float fDistToRight = Zenith_Maths::Length(xBestFlank - xFlankRight);
	ZENITH_ASSERT_TRUE(fDistToLeft < 1.0f || fDistToRight < 1.0f, "Should find a flank position to the side");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, FindBestPointNoPointsActive) { Zenith_UnitTests::TestFindBestPointNoPointsActive(); }

void Zenith_UnitTests::TestFindBestPointNoPointsActive(){
	Zenith_TacticalPointSystem::Initialise();

	// No points registered - FindBestPoint should return nullptr
	Zenith_TacticalPointQuery xQuery;
	xQuery.m_xSearchCenter = Zenith_Maths::Vector3(0.0f);
	xQuery.m_fSearchRadius = 50.0f;
	xQuery.m_bAnyType = true;
	xQuery.m_bMustBeAvailable = false;

	const Zenith_TacticalPoint* pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	ZENITH_ASSERT_NULL(pxResult, "FindBestPoint with no points should return nullptr");

	// Register and then unregister a point - should still return nullptr
	uint32_t uID = Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f), TacticalPointType::COVER_FULL);
	Zenith_TacticalPointSystem::UnregisterPoint(uID);

	pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	ZENITH_ASSERT_NULL(pxResult, "FindBestPoint with no active points should return nullptr");

	Zenith_TacticalPointSystem::Shutdown();
}

ZENITH_TEST(AI, FindBestPointOutOfRange) { Zenith_UnitTests::TestFindBestPointOutOfRange(); }

void Zenith_UnitTests::TestFindBestPointOutOfRange(){
	Zenith_TacticalPointSystem::Initialise();

	// Register a point far away
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(100.0f, 0.0f, 100.0f), TacticalPointType::COVER_FULL);

	// Query with small radius - should not find the point
	Zenith_TacticalPointQuery xQuery;
	xQuery.m_xSearchCenter = Zenith_Maths::Vector3(0.0f);
	xQuery.m_fSearchRadius = 5.0f;
	xQuery.m_bAnyType = true;
	xQuery.m_bMustBeAvailable = false;

	const Zenith_TacticalPoint* pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	ZENITH_ASSERT_NULL(pxResult, "FindBestPoint should return nullptr when all points are out of range");

	// Widen the radius - should now find the point
	xQuery.m_fSearchRadius = 200.0f;
	pxResult = Zenith_TacticalPointSystem::FindBestPoint(xQuery);
	ZENITH_ASSERT_NOT_NULL(pxResult, "FindBestPoint should find point when radius is large enough");

	Zenith_TacticalPointSystem::Shutdown();
}

// ============================================================================
// Tactical Point Refactoring Tests
// ============================================================================

ZENITH_TEST(AI, GetEntityPositionValid) { Zenith_UnitTests::TestGetEntityPositionValid(); }

void Zenith_UnitTests::TestGetEntityPositionValid(){

	// Create a real entity in the active scene and verify GetEntityPosition finds it
	Zenith_Scene xActiveScene = g_xEngine.Scenes().GetActiveScene();
	Zenith_SceneData* pxSceneData = g_xEngine.Scenes().GetSceneData(xActiveScene);
	Zenith_Entity xEntity = g_xEngine.Scenes().CreateEntity(pxSceneData, "TacTestAgent");

	Zenith_Maths::Vector3 xExpectedPos(5.0f, 3.0f, 7.0f);
	xEntity.GetComponent<Zenith_TransformComponent>().SetPosition(xExpectedPos);

	Zenith_Maths::Vector3 xOutPos;
	bool bResult = Zenith_TacticalPointSystem::GetEntityPosition(xEntity.GetEntityID(), xOutPos);

	ZENITH_ASSERT_TRUE(bResult, "GetEntityPosition should return true for valid entity");
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xOutPos - xExpectedPos), 0.001f, "GetEntityPosition should return the correct position");

	pxSceneData->MarkForDestruction(xEntity.GetEntityID());

}

ZENITH_TEST(AI, GetEntityPositionInvalid) { Zenith_UnitTests::TestGetEntityPositionInvalid(); }

void Zenith_UnitTests::TestGetEntityPositionInvalid(){

	// Use an invalid entity ID - should return false
	Zenith_EntityID xInvalidID;  // Default-constructed, m_uIndex == INVALID_INDEX
	Zenith_Maths::Vector3 xOutPos(99.0f, 99.0f, 99.0f);

	bool bResult = Zenith_TacticalPointSystem::GetEntityPosition(xInvalidID, xOutPos);
	ZENITH_ASSERT_FALSE(bResult, "GetEntityPosition should return false for invalid entity");

	// Also test with a fabricated ID that doesn't correspond to any entity
	Zenith_EntityID xFakeID;
	xFakeID.m_uIndex = 99999;
	xFakeID.m_uGeneration = 0;
	bResult = Zenith_TacticalPointSystem::GetEntityPosition(xFakeID, xOutPos);
	ZENITH_ASSERT_FALSE(bResult, "GetEntityPosition should return false for non-existent entity");

}

ZENITH_TEST(AI, FindBestPointNoMatches) { Zenith_UnitTests::TestFindBestPointNoMatches(); }

void Zenith_UnitTests::TestFindBestPointNoMatches(){

	Zenith_TacticalPointSystem::Initialise();

	// Register only PATROL_WAYPOINT points
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(5.0f, 0.0f, 5.0f), TacticalPointType::PATROL_WAYPOINT);
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(10.0f, 0.0f, 5.0f), TacticalPointType::PATROL_WAYPOINT);
	Zenith_TacticalPointSystem::RegisterPoint(
		Zenith_Maths::Vector3(15.0f, 0.0f, 5.0f), TacticalPointType::AMBUSH);

	// Search for COVER types using FindBestCoverPosition - should find none
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(20.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 50.0f);

	// When no cover is found, should return agent position (stay in place)
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xResult - xAgentPos), 0.001f, "FindBestCoverPosition with no cover points should return agent position");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, FindBestPointSelectsHighest) { Zenith_UnitTests::TestFindBestPointSelectsHighest(); }

void Zenith_UnitTests::TestFindBestPointSelectsHighest(){

	Zenith_TacticalPointSystem::Initialise();

	// Register multiple cover points at different distances from agent
	// Agent at origin, threat far away at (100, 0, 0)
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(100.0f, 0.0f, 0.0f);

	// Close cover point (should score higher due to distance factor)
	Zenith_Maths::Vector3 xClosePos(3.0f, 0.0f, 0.0f);
	// Far cover point
	Zenith_Maths::Vector3 xFarPos(25.0f, 0.0f, 0.0f);
	// Full cover close point (should get 1.5x bonus)
	Zenith_Maths::Vector3 xFullCoverPos(4.0f, 0.0f, 0.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xFarPos, TacticalPointType::COVER_HALF);
	Zenith_TacticalPointSystem::RegisterPoint(xClosePos, TacticalPointType::COVER_HALF);
	Zenith_TacticalPointSystem::RegisterPoint(xFullCoverPos, TacticalPointType::COVER_FULL);

	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 50.0f);

	// The full cover close point should score highest (close + full cover bonus)
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xResult - xFullCoverPos), 0.001f, "Should select the full cover close point as best");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, ScoreCoverDistance) { Zenith_UnitTests::TestScoreCoverDistance(); }

void Zenith_UnitTests::TestScoreCoverDistance(){

	Zenith_TacticalPointSystem::Initialise();

	// Register two half-cover points at different distances
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xThreatPos(50.0f, 0.0f, 0.0f);

	Zenith_Maths::Vector3 xNearPos(5.0f, 0.0f, 0.0f);
	Zenith_Maths::Vector3 xFarPos(30.0f, 0.0f, 0.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xNearPos, TacticalPointType::COVER_HALF);
	Zenith_TacticalPointSystem::RegisterPoint(xFarPos, TacticalPointType::COVER_HALF);

	// The near point should score higher due to better distance factor
	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestCoverPosition(
		xAgentPos, xThreatPos, 50.0f);

	// Near point should be selected (distance score: 1 - 5/50 = 0.9 vs 1 - 30/50 = 0.4)
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xResult - xNearPos), 0.001f, "Closer cover point should score higher when cover scores are similar");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, ScoreFlankAngle) { Zenith_UnitTests::TestScoreFlankAngle(); }

void Zenith_UnitTests::TestScoreFlankAngle(){

	Zenith_TacticalPointSystem::Initialise();

	// Target at (10, 0, 10) facing (0, 0, -1) (facing toward negative Z)
	Zenith_Maths::Vector3 xTargetPos(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xTargetFacing(0.0f, 0.0f, -1.0f);
	Zenith_Maths::Vector3 xAgentPos(10.0f, 0.0f, 0.0f);

	// Side flank - perpendicular to facing (best flank angle)
	Zenith_Maths::Vector3 xSideFlank(15.0f, 0.0f, 10.0f);
	// Front position - directly in front of target (worst flank angle)
	Zenith_Maths::Vector3 xFrontPos(10.0f, 0.0f, 5.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xFrontPos, TacticalPointType::FLANK_POSITION);
	Zenith_TacticalPointSystem::RegisterPoint(xSideFlank, TacticalPointType::FLANK_POSITION);

	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestFlankPosition(
		xAgentPos, xTargetPos, xTargetFacing, 1.0f, 20.0f);

	// Side flank should score higher (perpendicular angle)
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xResult - xSideFlank), 0.001f, "Side flank position should score higher than front position");

	// Verify the EvaluateFlankAngle function directly
	float fSideScore = Zenith_TacticalPointSystem::EvaluateFlankAngle(xSideFlank, xTargetPos, xTargetFacing);
	float fFrontScore = Zenith_TacticalPointSystem::EvaluateFlankAngle(xFrontPos, xTargetPos, xTargetFacing);
	ZENITH_ASSERT_GT(fSideScore, fFrontScore, "Perpendicular flank angle should score higher than frontal angle");

	Zenith_TacticalPointSystem::Shutdown();

}

ZENITH_TEST(AI, ScoreOverwatchElevation) { Zenith_UnitTests::TestScoreOverwatchElevation(); }

void Zenith_UnitTests::TestScoreOverwatchElevation(){

	Zenith_TacticalPointSystem::Initialise();

	Zenith_Maths::Vector3 xAreaToWatch(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Vector3 xAgentPos(0.0f, 0.0f, 0.0f);

	// Elevated overwatch point (y > 2.0 triggers TACPOINT_FLAG_ELEVATED)
	Zenith_Maths::Vector3 xHighPos(8.0f, 5.0f, 8.0f);
	// Ground-level overwatch point
	Zenith_Maths::Vector3 xLowPos(8.0f, 0.0f, 8.0f);

	Zenith_TacticalPointSystem::RegisterPoint(xLowPos, TacticalPointType::OVERWATCH);
	Zenith_TacticalPointSystem::RegisterPoint(xHighPos, TacticalPointType::OVERWATCH);

	// Verify elevated flag was set
	const Zenith_TacticalPoint* pxHighPoint = Zenith_TacticalPointSystem::GetPointConst(1);
	ZENITH_ASSERT_NOT_NULL(pxHighPoint, "High point should exist");
	ZENITH_ASSERT_NE((pxHighPoint->m_uFlags & TACPOINT_FLAG_ELEVATED), 0, "High point should have ELEVATED flag");

	const Zenith_TacticalPoint* pxLowPoint = Zenith_TacticalPointSystem::GetPointConst(0);
	ZENITH_ASSERT_NOT_NULL(pxLowPoint, "Low point should exist");
	ZENITH_ASSERT_EQ((pxLowPoint->m_uFlags & TACPOINT_FLAG_ELEVATED), 0, "Low point should NOT have ELEVATED flag");

	// The elevated point should score higher due to elevation bonus
	Zenith_Maths::Vector3 xResult = Zenith_TacticalPointSystem::FindBestOverwatchPosition(
		xAgentPos, xAreaToWatch, 1.0f, 30.0f);

	ZENITH_ASSERT_LT(Zenith_Maths::Length(xResult - xHighPos), 0.001f, "Elevated overwatch point should score higher than ground-level");

	Zenith_TacticalPointSystem::Shutdown();

}

