#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Squad/Zenith_Formation.h"

ZENITH_TEST(AI, FormationLine) { Zenith_UnitTests::TestFormationLine(); }

void Zenith_UnitTests::TestFormationLine(){
	const Zenith_Formation* pxLine = Zenith_Formation::GetLine();

	ZENITH_ASSERT_NOT_NULL(pxLine, "Line formation should exist");
	ZENITH_ASSERT_GE(pxLine->GetSlotCount(), 3, "Line should have at least 3 slots");

	// Line formation: members spread horizontally (X axis)
	const Zenith_FormationSlot& xSlot0 = pxLine->GetSlot(0);
	ZENITH_ASSERT_EQ(xSlot0.m_xOffset.z, 0.0f, "Line slots should be on same Z");

}

ZENITH_TEST(AI, FormationWedge) { Zenith_UnitTests::TestFormationWedge(); }

void Zenith_UnitTests::TestFormationWedge(){
	const Zenith_Formation* pxWedge = Zenith_Formation::GetWedge();

	ZENITH_ASSERT_NOT_NULL(pxWedge, "Wedge formation should exist");
	ZENITH_ASSERT_GE(pxWedge->GetSlotCount(), 3, "Wedge should have at least 3 slots");

	// Wedge formation: leader at front, others behind
	const Zenith_FormationSlot& xLeaderSlot = pxWedge->GetSlot(0);
	ZENITH_ASSERT_EQ(xLeaderSlot.m_xOffset.z, 0.0f, "Leader should be at front (z=0)");

	if (pxWedge->GetSlotCount() > 1)
	{
		const Zenith_FormationSlot& xFollowerSlot = pxWedge->GetSlot(1);
		ZENITH_ASSERT_LT(xFollowerSlot.m_xOffset.z, 0.0f, "Followers should be behind (z<0)");
	}

}

ZENITH_TEST(AI, FormationWorldPositions) { Zenith_UnitTests::TestFormationWorldPositions(); }

void Zenith_UnitTests::TestFormationWorldPositions(){
	const Zenith_Formation* pxLine = Zenith_Formation::GetLine();

	Zenith_Maths::Vector3 xLeaderPos(10.0f, 0.0f, 10.0f);
	Zenith_Maths::Quaternion xLeaderRot = Zenith_Maths::QuatFromEuler(0.0f, 0.0f, 0.0f);

	Zenith_Vector<Zenith_Maths::Vector3> axPositions;
	pxLine->GetWorldPositions(xLeaderPos, xLeaderRot, axPositions);

	ZENITH_ASSERT_EQ(axPositions.GetSize(), pxLine->GetSlotCount(), "Should have position for each slot");

	// First slot should be at leader position
	ZENITH_ASSERT_LT(Zenith_Maths::Length(axPositions.Get(0) - xLeaderPos), 0.01f, "First slot should be at leader position");

}
