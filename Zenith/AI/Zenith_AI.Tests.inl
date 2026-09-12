// Zenith_AI.Tests.inl -- the engine AI tick's contract with the unit batch.
// Included from the bottom of Zenith_AI.cpp.
#ifdef ZENITH_TESTING
#include "Core/Zenith_TestFramework.h"
#include "AI/Zenith_AIManagerTestScope.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"

// The regression this file exists for: project init armed the engine AI tick
// BEFORE the boot unit batch, the AI units each Shutdown() the managers, and the
// first game-logic frame after the batch (the editor's play button) hit
// `SquadManager::Update called before Initialise()`. A scope must hand the
// managers back initialised when it found them initialised...
ZENITH_TEST(AI, ManagerTestScopeRestoresAnArmedEngineTick)
{
	const bool bTickWas = Zenith_AI::IsEngineTickEnabled();
	Zenith_AI::SetEngineTickEnabled(true);
	ZENITH_ASSERT_TRUE(Zenith_SquadManager::IsInitialised(), "arming the tick initialises the squad manager");
	ZENITH_ASSERT_TRUE(Zenith_TacticalPointSystem::IsInitialised(), "and the tactical-point system");
	{
		Zenith_AIManagerTestScope xScope;
		ZENITH_ASSERT_TRUE(Zenith_SquadManager::IsInitialised(), "inside the scope the managers are fresh and initialised");
		Zenith_SquadManager::Shutdown();	// what every AI unit used to leave behind
		Zenith_TacticalPointSystem::Shutdown();
	}
	ZENITH_ASSERT_TRUE(Zenith_SquadManager::IsInitialised(), "★ the scope re-armed the squad manager the boot had initialised");
	ZENITH_ASSERT_TRUE(Zenith_TacticalPointSystem::IsInitialised(), "★ and the tactical-point system");
	Zenith_AI::SetEngineTickEnabled(bTickWas);
}

// ...and back SHUT DOWN when it found them shut down: a game that never formed a
// squad must not gain an initialised manager as a side effect of the unit batch.
ZENITH_TEST(AI, ManagerTestScopeLeavesAnUnarmedManagerDown)
{
	const bool bSquadsWere = Zenith_SquadManager::IsInitialised();
	const bool bTacticalWere = Zenith_TacticalPointSystem::IsInitialised();
	Zenith_SquadManager::Shutdown();
	Zenith_TacticalPointSystem::Shutdown();
	{
		Zenith_AIManagerTestScope xScope;
		ZENITH_ASSERT_TRUE(Zenith_SquadManager::IsInitialised(), "the body still gets fresh, initialised managers");
		ZENITH_ASSERT_NOT_NULL(Zenith_SquadManager::CreateSquad("ScopeProbe"), "usable ones");
	}
	ZENITH_ASSERT_FALSE(Zenith_SquadManager::IsInitialised(), "and afterwards they are down again, as found");
	ZENITH_ASSERT_FALSE(Zenith_TacticalPointSystem::IsInitialised(), "both of them");
	// Put back whatever THIS test found, so it obeys the rule it is testing.
	if (bSquadsWere) { Zenith_SquadManager::Initialise(); }
	if (bTacticalWere) { Zenith_TacticalPointSystem::Initialise(); }
}
#endif // ZENITH_TESTING
