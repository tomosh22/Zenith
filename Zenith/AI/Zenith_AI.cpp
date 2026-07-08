#include "Zenith.h"
#include "AI/Zenith_AI.h"
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
}
