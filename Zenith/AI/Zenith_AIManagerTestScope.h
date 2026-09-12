#pragma once
// Zenith_AIManagerTestScope -- a unit test's view of the squad + tactical-point
// managers that puts them back the way it found them.
//
// WHY THIS EXISTS. Project init runs BEFORE the boot unit batch, and ScriptTest's
// Project_RegisterGameComponents arms the engine AI tick (Zenith_AI::
// SetEngineTickEnabled), which Initialise()s both managers. Every squad and
// tactical-point unit then called Initialise()/Shutdown() around its body and
// LEFT THE MANAGERS SHUT DOWN -- so the first game-logic frame after the batch
// (the editor's play button, in a *_True build) hit
// `SquadManager::Update called before Initialise()`. `zenith test` passes
// --skip-unit-tests, so no headless suite could see it. Same shape as
// Zenith_EventDispatcher::ScopedTestIsolation: a test may reset shared engine
// state only if it restores what was there.
//
// The body always runs against FRESH managers (Shutdown + Initialise, since
// Initialise() is a no-op on an already-initialised manager); the destructor
// shuts them down and re-initialises whichever were initialised on entry.
#ifdef ZENITH_TESTING
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"

class Zenith_AIManagerTestScope
{
public:
	Zenith_AIManagerTestScope()
		: m_bSquadsWereInitialised(Zenith_SquadManager::IsInitialised())
		, m_bTacticalPointsWereInitialised(Zenith_TacticalPointSystem::IsInitialised())
	{
		Zenith_SquadManager::Shutdown();
		Zenith_SquadManager::Initialise();
		Zenith_TacticalPointSystem::Shutdown();
		Zenith_TacticalPointSystem::Initialise();
	}

	~Zenith_AIManagerTestScope()
	{
		Zenith_SquadManager::Shutdown();
		Zenith_TacticalPointSystem::Shutdown();
		if (m_bSquadsWereInitialised)
		{
			Zenith_SquadManager::Initialise();
		}
		if (m_bTacticalPointsWereInitialised)
		{
			Zenith_TacticalPointSystem::Initialise();
		}
	}

	Zenith_AIManagerTestScope(const Zenith_AIManagerTestScope&) = delete;
	Zenith_AIManagerTestScope& operator=(const Zenith_AIManagerTestScope&) = delete;

private:
	bool m_bSquadsWereInitialised;
	bool m_bTacticalPointsWereInitialised;
};
#endif // ZENITH_TESTING
