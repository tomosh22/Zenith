#include "Zenith.h"
#include "AI/Zenith_AI.h"
#include "AI/Zenith_AIDebugVariables.h"
#include "AI/Perception/Zenith_PerceptionSystem.h"
#include "AI/Squad/Zenith_Squad.h"
#include "AI/Squad/Zenith_TacticalPoint.h"

namespace
{
	// Function-local static (the sanctioned cross-TU singleton shape — not a
	// module-scope static) holds the opt-in flag. Default off: no behaviour change
	// for the games that drive these managers from their own components.
	bool& EngineTickEnabledRef()
	{
		static bool s_bEnabled = false;
		return s_bEnabled;
	}
}

namespace Zenith_AI
{
	// ★ ENABLING THE TICK MUST ALSO ARM THE MANAGERS THAT REQUIRE IT.
	// Zenith_SquadManager::Update and Zenith_TacticalPointSystem::Update both
	// assert on an uninitialised system, so the "one line at init" this header
	// advertises used to fire `SquadManager::Update called before Initialise()`
	// on the very first game-logic frame. No game in the repo had ever taken the
	// opt-in -- every one of them ticks the managers from its own component, and
	// initialises them there -- so the path was untried until ScriptTest, which
	// is forbidden a component to do it from.
	//
	// Both Initialise() calls are idempotent no-ops when already initialised, so
	// a game that enables the tick AND initialises the managers itself is
	// unaffected.
	//
	// Zenith_PerceptionSystem is deliberately NOT initialised here. Its Update
	// needs no initialisation (buckets are created on demand), while its
	// Initialise CLEARS every registered agent and target -- so calling it from a
	// toggle that may be flipped after agents exist would silently drop them.
	void SetEngineTickEnabled(bool bEnabled)
	{
		if (bEnabled)
		{
			Zenith_SquadManager::Initialise();
			Zenith_TacticalPointSystem::Initialise();
		}
		EngineTickEnabledRef() = bEnabled;
	}
	bool IsEngineTickEnabled() { return EngineTickEnabledRef(); }

	void Update(float fDt)
	{
		// Canonical order: perception feeds squad coordination, then tactical-point
		// scoring. Each manager profiles itself internally.
		Zenith_PerceptionSystem::Update(fDt);
		Zenith_SquadManager::Update(fDt);
		Zenith_TacticalPointSystem::Update();
	}

#ifdef ZENITH_TOOLS
	void DebugDraw()
	{
		// Master switch first: one branch per frame when AI visualisation is off.
		if (!Zenith_AIDebugVariables::s_bEnableAllAIDebug)
		{
			return;
		}

		// Each of these re-checks its own section flags (and tolerates an
		// un-Initialise()d manager), so the order here is presentation only:
		// per-agent senses, then squad structure, then the tactical-point field.
		Zenith_PerceptionSystem::DebugDrawAllAgents();
		Zenith_SquadManager::DebugDrawAllSquads();
		Zenith_TacticalPointSystem::DebugDraw();
	}
#endif
}
