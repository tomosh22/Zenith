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
	void SetEngineTickEnabled(bool bEnabled) { EngineTickEnabledRef() = bEnabled; }
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
