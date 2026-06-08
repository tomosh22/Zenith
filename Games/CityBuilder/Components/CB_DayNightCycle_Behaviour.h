#pragma once
#include "Core/Zenith_Engine.h"

#include "EntityComponent/Components/Zenith_ScriptComponent.h"
#include "CityBuilder/Source/CB_DayNight.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"

// ============================================================================
// CB_DayNightCycle_Behaviour — advances the day/night clock each frame and
// publishes it as the active cycle. Applying the sun direction to the engine
// sky/sun light is the visual-pass remainder; the clock + sun math (CB_DayNight)
// is unit-tested and available to any system that wants the time of day.
// ============================================================================
class CB_DayNightCycle_Behaviour ZENITH_FINAL : Zenith_ScriptBehaviour
{
	friend class Zenith_ScriptComponent;
public:
	ZENITH_BEHAVIOUR_TYPE_NAME(CB_DayNightCycle_Behaviour)

	CB_DayNightCycle_Behaviour() = delete;
	CB_DayNightCycle_Behaviour(Zenith_Entity& /*xParentEntity*/) {}

	void OnStart() ZENITH_FINAL override
	{
		m_xCycle = CB_DayNight();
		s_pxActive = &m_xCycle;
	}

	void OnUpdate(const float fDt) ZENITH_FINAL override
	{
		m_xCycle.Advance(fDt);
		// Visual pass: drive the sky's sun intensity from the time of day so the
		// city visibly brightens toward noon and darkens at night.
		if (!Zenith_CommandLine::IsHeadless())
		{
			const float fElev = m_xCycle.GetSunElevation();   // 0 at night .. 1 at noon
			g_xEngine.Skybox().SetSunIntensity(0.2f + 2.6f * fElev);
		}
	}

	void OnDestroy() ZENITH_FINAL override
	{
		if (s_pxActive == &m_xCycle)
		{
			s_pxActive = nullptr;
		}
	}

	CB_DayNight&        GetCycle()       { return m_xCycle; }
	static CB_DayNight* GetActive()      { return s_pxActive; }

private:
	CB_DayNight m_xCycle;
	static inline CB_DayNight* s_pxActive = nullptr;
};
