#include "UnitTests/Zenith_UnitTests.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_Formation.h"

// ============================================================================
// Squad Tests
// ============================================================================
ZENITH_TEST(AI, SquadAddRemoveMember) { Zenith_UnitTests::TestSquadAddRemoveMember(); }
void Zenith_UnitTests::TestSquadAddRemoveMember(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);

	pxSquad->AddMember(xMember1);
	pxSquad->AddMember(xMember2);

	ZENITH_ASSERT_EQ(pxSquad->GetMemberCount(), 2, "Should have 2 members");
	ZENITH_ASSERT_TRUE(pxSquad->HasMember(xMember1), "Should have member 1");
	ZENITH_ASSERT_TRUE(pxSquad->HasMember(xMember2), "Should have member 2");

	pxSquad->RemoveMember(xMember1);

	ZENITH_ASSERT_EQ(pxSquad->GetMemberCount(), 1, "Should have 1 member");
	ZENITH_ASSERT_FALSE(pxSquad->HasMember(xMember1), "Should not have member 1");
	ZENITH_ASSERT_TRUE(pxSquad->HasMember(xMember2), "Should still have member 2");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadRoleAssignment) { Zenith_UnitTests::TestSquadRoleAssignment(); }

void Zenith_UnitTests::TestSquadRoleAssignment(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember(1001);
	pxSquad->AddMember(xMember, SquadRole::FLANKER);

	SquadRole eRole = pxSquad->GetMemberRole(xMember);
	ZENITH_ASSERT_EQ(eRole, SquadRole::FLANKER, "Role should be FLANKER");

	pxSquad->AssignRole(xMember, SquadRole::SUPPORT);
	eRole = pxSquad->GetMemberRole(xMember);
	ZENITH_ASSERT_EQ(eRole, SquadRole::SUPPORT, "Role should be SUPPORT after change");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadLeaderSelection) { Zenith_UnitTests::TestSquadLeaderSelection(); }

void Zenith_UnitTests::TestSquadLeaderSelection(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);

	pxSquad->AddMember(xMember1, SquadRole::ASSAULT);
	pxSquad->AddMember(xMember2, SquadRole::LEADER);
	pxSquad->SetLeader(xMember2);

	ZENITH_ASSERT_TRUE(pxSquad->HasLeader(), "Should have leader");
	ZENITH_ASSERT_EQ(pxSquad->GetLeader(), xMember2, "Leader should be member 2");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadSharedKnowledge) { Zenith_UnitTests::TestSquadSharedKnowledge(); }

void Zenith_UnitTests::TestSquadSharedKnowledge(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);
	Zenith_EntityID xTarget(2001);

	pxSquad->AddMember(xMember1);
	pxSquad->AddMember(xMember2);

	// Member 1 shares target info
	Zenith_Maths::Vector3 xTargetPos(50.0f, 0.0f, 50.0f);
	pxSquad->ShareTargetInfo(xTarget, xTargetPos, xMember1);

	ZENITH_ASSERT_TRUE(pxSquad->IsTargetKnown(xTarget), "Target should be known to squad");

	const Zenith_SharedTarget* pxShared = pxSquad->GetSharedTarget(xTarget);
	ZENITH_ASSERT_NOT_NULL(pxShared, "Should have shared target info");
	ZENITH_ASSERT_EQ(pxShared->m_xReportedBy, xMember1, "Should know who reported");

	Zenith_SquadManager::Shutdown();

}

// ============================================================================
// Squad Refactoring Tests (FindSharedTargetIndex, formation slot assignment)
// ============================================================================

ZENITH_TEST(AI, SharedTargetUpdate) { Zenith_UnitTests::TestSharedTargetUpdate(); }

void Zenith_UnitTests::TestSharedTargetUpdate(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xTarget(2001);
	pxSquad->AddMember(xMember1);

	// Share initial target info
	Zenith_Maths::Vector3 xPos1(10.0f, 0.0f, 10.0f);
	pxSquad->ShareTargetInfo(xTarget, xPos1, xMember1);

	const Zenith_SharedTarget* pxTarget = pxSquad->GetSharedTarget(xTarget);
	ZENITH_ASSERT_NOT_NULL(pxTarget, "Target should exist");
	ZENITH_ASSERT_LT(Zenith_Maths::Length(pxTarget->m_xLastKnownPosition - xPos1), 0.01f, "Target position should match");

	// Share updated position for same target — exercises FindSharedTargetIndex "found" path
	Zenith_Maths::Vector3 xPos2(20.0f, 0.0f, 30.0f);
	Zenith_EntityID xMember2(1002);
	pxSquad->AddMember(xMember2);
	pxSquad->ShareTargetInfo(xTarget, xPos2, xMember2);

	pxTarget = pxSquad->GetSharedTarget(xTarget);
	ZENITH_ASSERT_NOT_NULL(pxTarget, "Target should still exist");
	ZENITH_ASSERT_LT(Zenith_Maths::Length(pxTarget->m_xLastKnownPosition - xPos2), 0.01f, "Target position should be updated");
	ZENITH_ASSERT_EQ(pxTarget->m_xReportedBy, xMember2, "Reporter should be updated to member 2");

	// Only one target should exist (not duplicated)
	ZENITH_ASSERT_EQ(pxSquad->GetAllSharedTargets().GetSize(), 1, "Should have exactly 1 shared target after update");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SharedTargetUnknown) { Zenith_UnitTests::TestSharedTargetUnknown(); }

void Zenith_UnitTests::TestSharedTargetUnknown(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	Zenith_EntityID xMember1(1001);
	pxSquad->AddMember(xMember1);

	Zenith_EntityID xUnknown(9999);

	// Exercises FindSharedTargetIndex "not found" path through public API
	ZENITH_ASSERT_FALSE(pxSquad->IsTargetKnown(xUnknown), "Unknown target should not be known");
	ZENITH_ASSERT_NULL(pxSquad->GetSharedTarget(xUnknown), "GetSharedTarget should return nullptr for unknown");
	ZENITH_ASSERT_FALSE(pxSquad->IsTargetEngaged(xUnknown), "IsTargetEngaged should return false for unknown");

	// SetTargetEngaged on unknown target should not crash
	pxSquad->SetTargetEngaged(xUnknown, xMember1);
	ZENITH_ASSERT_FALSE(pxSquad->IsTargetEngaged(xUnknown), "Engaging unknown target should have no effect");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, FormationSlotsLeaderFirst) { Zenith_UnitTests::TestFormationSlotsLeaderFirst(); }

void Zenith_UnitTests::TestFormationSlotsLeaderFirst(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	pxSquad->SetFormation(Zenith_Formation::GetWedge());

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);
	Zenith_EntityID xMember3(1003);

	// Add non-leader first, then leader — leader should still get slot 0
	pxSquad->AddMember(xMember1, SquadRole::ASSAULT);
	pxSquad->AddMember(xMember2, SquadRole::SUPPORT);
	pxSquad->AddMember(xMember3, SquadRole::LEADER);

	const Zenith_SquadMember* pxLeader = pxSquad->GetMember(xMember3);
	ZENITH_ASSERT_NOT_NULL(pxLeader, "Leader member should exist");
	ZENITH_ASSERT_EQ(pxLeader->m_iFormationSlot, 0, "Leader should always get slot 0");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, FormationSlotsRoleMatching) { Zenith_UnitTests::TestFormationSlotsRoleMatching(); }

void Zenith_UnitTests::TestFormationSlotsRoleMatching(){
	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");
	pxSquad->SetFormation(Zenith_Formation::GetWedge());

	Zenith_EntityID xLeader(1001);
	Zenith_EntityID xAssault(1002);
	Zenith_EntityID xFlanker(1003);

	pxSquad->AddMember(xLeader, SquadRole::LEADER);
	pxSquad->AddMember(xAssault, SquadRole::ASSAULT);
	pxSquad->AddMember(xFlanker, SquadRole::FLANKER);

	// All members should have valid formation slots
	const Zenith_SquadMember* pxLeaderM = pxSquad->GetMember(xLeader);
	const Zenith_SquadMember* pxAssaultM = pxSquad->GetMember(xAssault);
	const Zenith_SquadMember* pxFlankerM = pxSquad->GetMember(xFlanker);

	ZENITH_ASSERT_GE(pxLeaderM->m_iFormationSlot, 0, "Leader should have a slot");
	ZENITH_ASSERT_GE(pxAssaultM->m_iFormationSlot, 0, "Assault should have a slot");
	ZENITH_ASSERT_GE(pxFlankerM->m_iFormationSlot, 0, "Flanker should have a slot");

	// All slots should be unique
	ZENITH_ASSERT_NE(pxLeaderM->m_iFormationSlot, pxAssaultM->m_iFormationSlot, "Leader and Assault should have different slots");
	ZENITH_ASSERT_NE(pxLeaderM->m_iFormationSlot, pxFlankerM->m_iFormationSlot, "Leader and Flanker should have different slots");
	ZENITH_ASSERT_NE(pxAssaultM->m_iFormationSlot, pxFlankerM->m_iFormationSlot, "Assault and Flanker should have different slots");

	Zenith_SquadManager::Shutdown();

}

// ============================================================================
// Squad Order Helper and Alive Status Refactoring Tests
// ============================================================================

ZENITH_TEST(AI, SquadPositionOrder) { Zenith_UnitTests::TestSquadPositionOrder(); }

void Zenith_UnitTests::TestSquadPositionOrder(){

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_Maths::Vector3 xTarget(25.0f, 5.0f, 30.0f);
	pxSquad->OrderMoveTo(xTarget);

	const Zenith_SquadOrder& xOrder = pxSquad->GetCurrentOrder();
	ZENITH_ASSERT_EQ(xOrder.m_eType, SquadOrderType::MOVE_TO, "Order type should be MOVE_TO");
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xOrder.m_xTargetPosition - xTarget), 0.01f, "Order position should match issued position");
	ZENITH_ASSERT_FALSE(xOrder.m_xTargetEntity.IsValid(), "Position order should have invalid target entity");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadTargetOrderClearsPosition) { Zenith_UnitTests::TestSquadTargetOrderClearsPosition(); }

void Zenith_UnitTests::TestSquadTargetOrderClearsPosition(){

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xTarget(3001);
	pxSquad->OrderAttack(xTarget);

	const Zenith_SquadOrder& xOrder = pxSquad->GetCurrentOrder();
	ZENITH_ASSERT_EQ(xOrder.m_eType, SquadOrderType::ATTACK, "Order type should be ATTACK");
	ZENITH_ASSERT_EQ(xOrder.m_xTargetEntity, xTarget, "Target entity should match issued target");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadSimpleOrderClearsAll) { Zenith_UnitTests::TestSquadSimpleOrderClearsAll(); }

void Zenith_UnitTests::TestSquadSimpleOrderClearsAll(){

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	// First issue a position order to set position and target
	pxSquad->OrderMoveTo(Zenith_Maths::Vector3(100.0f, 0.0f, 100.0f));

	// Now issue a simple order which should clear everything
	pxSquad->OrderHoldPosition();

	const Zenith_SquadOrder& xOrder = pxSquad->GetCurrentOrder();
	ZENITH_ASSERT_EQ(xOrder.m_eType, SquadOrderType::HOLD_POSITION, "Order type should be HOLD_POSITION");
	ZENITH_ASSERT_LT(Zenith_Maths::Length(xOrder.m_xTargetPosition), 0.01f, "Simple order should zero position");
	ZENITH_ASSERT_FALSE(xOrder.m_xTargetEntity.IsValid(), "Simple order should have invalid target entity");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadDeadMemberTriggersLeaderReassign) { Zenith_UnitTests::TestSquadDeadMemberTriggersLeaderReassign(); }

void Zenith_UnitTests::TestSquadDeadMemberTriggersLeaderReassign(){

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);
	Zenith_EntityID xMember3(1003);

	pxSquad->AddMember(xMember1, SquadRole::LEADER);
	pxSquad->AddMember(xMember2, SquadRole::ASSAULT);
	pxSquad->AddMember(xMember3, SquadRole::ASSAULT);

	ZENITH_ASSERT_EQ(pxSquad->GetLeader(), xMember1, "Leader should be member 1 initially");

	// Kill the leader
	pxSquad->MarkMemberDead(xMember1);

	ZENITH_ASSERT_TRUE(pxSquad->HasLeader(), "Squad should still have a leader after old leader died");
	ZENITH_ASSERT_NE(pxSquad->GetLeader(), xMember1, "Leader should no longer be the dead member");
	ZENITH_ASSERT_TRUE(pxSquad->GetLeader() == xMember2 || pxSquad->GetLeader() == xMember3, "New leader should be one of the alive members");

	Zenith_SquadManager::Shutdown();

}

ZENITH_TEST(AI, SquadAliveMemberPreservesLeader) { Zenith_UnitTests::TestSquadAliveMemberPreservesLeader(); }

void Zenith_UnitTests::TestSquadAliveMemberPreservesLeader(){

	Zenith_SquadManager::Initialise();

	Zenith_Squad* pxSquad = Zenith_SquadManager::CreateSquad("TestSquad");

	Zenith_EntityID xMember1(1001);
	Zenith_EntityID xMember2(1002);

	pxSquad->AddMember(xMember1, SquadRole::LEADER);
	pxSquad->AddMember(xMember2, SquadRole::ASSAULT);

	ZENITH_ASSERT_EQ(pxSquad->GetLeader(), xMember1, "Leader should be member 1 initially");

	// Mark non-leader alive (no-op since already alive, but should not affect leader)
	pxSquad->MarkMemberAlive(xMember2);

	ZENITH_ASSERT_EQ(pxSquad->GetLeader(), xMember1, "Leader should remain member 1 after marking non-leader alive");
	ZENITH_ASSERT_TRUE(pxSquad->IsMemberAlive(xMember2), "Member 2 should be alive");

	Zenith_SquadManager::Shutdown();

}

