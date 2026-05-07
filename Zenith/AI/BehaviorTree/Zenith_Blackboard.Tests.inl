#include "UnitTests/Zenith_UnitTests.h"
#include "AI/BehaviorTree/Zenith_Blackboard.h"

// ============================================================================
// Blackboard Tests
// ============================================================================
ZENITH_TEST(AI, BlackboardBasicTypes) { Zenith_UnitTests::TestBlackboardBasicTypes(); }
void Zenith_UnitTests::TestBlackboardBasicTypes(){
	Zenith_Blackboard xBlackboard;

	// Test float
	xBlackboard.SetFloat("health", 100.0f);
	ZENITH_ASSERT_EQ(xBlackboard.GetFloat("health"), 100.0f, "Float should be 100.0");

	// Test int
	xBlackboard.SetInt("ammo", 30);
	ZENITH_ASSERT_EQ(xBlackboard.GetInt("ammo"), 30, "Int should be 30");

	// Test bool
	xBlackboard.SetBool("isAlerted", true);
	ZENITH_ASSERT_EQ(xBlackboard.GetBool("isAlerted"), true, "Bool should be true");

}

ZENITH_TEST(AI, BlackboardVector3) { Zenith_UnitTests::TestBlackboardVector3(); }

void Zenith_UnitTests::TestBlackboardVector3(){
	Zenith_Blackboard xBlackboard;

	Zenith_Maths::Vector3 xTestVec(1.0f, 2.0f, 3.0f);
	xBlackboard.SetVector3("targetPos", xTestVec);

	Zenith_Maths::Vector3 xResult = xBlackboard.GetVector3("targetPos");
	ZENITH_ASSERT_TRUE(xResult.x == 1.0f && xResult.y == 2.0f && xResult.z == 3.0f, "Vector3 values should match");

}

ZENITH_TEST(AI, BlackboardEntityID) { Zenith_UnitTests::TestBlackboardEntityID(); }

void Zenith_UnitTests::TestBlackboardEntityID(){
	Zenith_Blackboard xBlackboard;

	Zenith_EntityID xTestID(12345);
	xBlackboard.SetEntityID("targetEntity", xTestID);

	Zenith_EntityID xResult = xBlackboard.GetEntityID("targetEntity");
	ZENITH_ASSERT_TRUE(xResult.IsValid() && xResult.m_uIndex == 12345, "EntityID should match");

}

ZENITH_TEST(AI, BlackboardHasKey) { Zenith_UnitTests::TestBlackboardHasKey(); }

void Zenith_UnitTests::TestBlackboardHasKey(){
	Zenith_Blackboard xBlackboard;

	ZENITH_ASSERT_FALSE(xBlackboard.HasKey("missing"), "Key should not exist initially");

	xBlackboard.SetFloat("exists", 1.0f);
	ZENITH_ASSERT_TRUE(xBlackboard.HasKey("exists"), "Key should exist after set");

	xBlackboard.RemoveKey("exists");
	ZENITH_ASSERT_FALSE(xBlackboard.HasKey("exists"), "Key should not exist after remove");

}

ZENITH_TEST(AI, BlackboardClear) { Zenith_UnitTests::TestBlackboardClear(); }

void Zenith_UnitTests::TestBlackboardClear(){
	Zenith_Blackboard xBlackboard;

	xBlackboard.SetFloat("a", 1.0f);
	xBlackboard.SetInt("b", 2);
	xBlackboard.SetBool("c", true);

	xBlackboard.Clear();

	ZENITH_ASSERT_FALSE(xBlackboard.HasKey("a"), "All keys should be cleared");
	ZENITH_ASSERT_FALSE(xBlackboard.HasKey("b"), "All keys should be cleared");
	ZENITH_ASSERT_FALSE(xBlackboard.HasKey("c"), "All keys should be cleared");

}

ZENITH_TEST(AI, BlackboardDefaultValues) { Zenith_UnitTests::TestBlackboardDefaultValues(); }

void Zenith_UnitTests::TestBlackboardDefaultValues(){
	Zenith_Blackboard xBlackboard;

	// Test defaults for non-existent keys
	ZENITH_ASSERT_EQ(xBlackboard.GetFloat("missing", 42.0f), 42.0f, "Should return default float");
	ZENITH_ASSERT_EQ(xBlackboard.GetInt("missing", 99), 99, "Should return default int");
	ZENITH_ASSERT_EQ(xBlackboard.GetBool("missing", true), true, "Should return default bool");

}

ZENITH_TEST(AI, BlackboardOverwrite) { Zenith_UnitTests::TestBlackboardOverwrite(); }

void Zenith_UnitTests::TestBlackboardOverwrite(){
	Zenith_Blackboard xBlackboard;

	xBlackboard.SetFloat("value", 1.0f);
	ZENITH_ASSERT_EQ(xBlackboard.GetFloat("value"), 1.0f, "Initial value");

	xBlackboard.SetFloat("value", 2.0f);
	ZENITH_ASSERT_EQ(xBlackboard.GetFloat("value"), 2.0f, "Overwritten value");

}

ZENITH_TEST(AI, BlackboardSerialization) { Zenith_UnitTests::TestBlackboardSerialization(); }

void Zenith_UnitTests::TestBlackboardSerialization(){
	Zenith_Blackboard xBlackboard;
	xBlackboard.SetFloat("health", 75.0f);
	xBlackboard.SetInt("level", 5);
	xBlackboard.SetBool("active", true);
	xBlackboard.SetVector3("pos", Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));

	// Serialize
	Zenith_DataStream xStream(256);
	xBlackboard.WriteToDataStream(xStream);

	// Deserialize into new blackboard
	xStream.SetCursor(0);
	Zenith_Blackboard xLoaded;
	xLoaded.ReadFromDataStream(xStream);

	ZENITH_ASSERT_EQ(xLoaded.GetFloat("health"), 75.0f, "Serialized float should match");
	ZENITH_ASSERT_EQ(xLoaded.GetInt("level"), 5, "Serialized int should match");
	ZENITH_ASSERT_EQ(xLoaded.GetBool("active"), true, "Serialized bool should match");

	Zenith_Maths::Vector3 xLoadedPos = xLoaded.GetVector3("pos");
	ZENITH_ASSERT_TRUE(xLoadedPos.x == 1.0f && xLoadedPos.y == 2.0f && xLoadedPos.z == 3.0f, "Serialized Vector3 should match");

}

