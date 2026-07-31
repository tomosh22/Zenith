#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "DataStream/Zenith_DataStream.h"
#include "Core/Zenith_CommandLine.h"
#include "CityBuilder/Source/CB_DayNight.h"
#include "EntityComponent/Components/Zenith_SunComponent.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

// ============================================================================
// CB_DayNightCycleComponent — advances the day/night clock each frame and
// publishes it as the active cycle. It drives the authoritative
// Zenith_SunComponent's time-of-day angle from the clock so the city brightens
// toward noon and darkens at night THROUGH THE SUN MOVING BELOW THE HORIZON +
// atmospheric transmittance -- NOT by animating the renderer's radiometric
// anchor (the old g_xEngine.Skybox().SetSunIntensity leak). The clock + sun
// math (CB_DayNight) is unit-tested; this component is the engine-binding half.
// ============================================================================
class CB_DayNightCycleComponent
{
public:
	CB_DayNightCycleComponent() = delete;
	CB_DayNightCycleComponent(Zenith_Entity& xParentEntity)
		: m_xParentEntity(xParentEntity)
	{
	}

	// Component pools relocate components on resize / swap-and-pop (move-construct
	// + destruct the source), so the moves are hand-written: the published static
	// clock pointer holds a MEMBER address and must follow the live object.
	CB_DayNightCycleComponent(const CB_DayNightCycleComponent&) = delete;
	CB_DayNightCycleComponent& operator=(const CB_DayNightCycleComponent&) = delete;

	CB_DayNightCycleComponent(CB_DayNightCycleComponent&& xOther) noexcept
		: m_xParentEntity(xOther.m_xParentEntity)
		, m_xCycle(xOther.m_xCycle)
	{
		if (s_pxActive == &xOther.m_xCycle)
		{
			s_pxActive = &m_xCycle;
		}
	}

	CB_DayNightCycleComponent& operator=(CB_DayNightCycleComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			m_xParentEntity = xOther.m_xParentEntity;
			m_xCycle        = xOther.m_xCycle;
			if (s_pxActive == &xOther.m_xCycle)
			{
				s_pxActive = &m_xCycle;
			}
		}
		return *this;
	}

	// Component contract. The clock restarts each session (OnStart); only the
	// version tag persists.
	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		const u_int uVersion = 1;
		xStream << uVersion;
	}
	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uVersion = 0;
		xStream >> uVersion;
	}
#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		ImGui::Text("Time of day: %.3f (%s)", m_xCycle.m_fTimeOfDay, m_xCycle.IsDay() ? "day" : "night");
		ImGui::Text("Sun elevation: %.3f", m_xCycle.GetSunElevation());
	}
#endif

	void OnStart()
	{
		m_xCycle = CB_DayNight();
		s_pxActive = &m_xCycle;
		// The day/night clock drives the authoritative celestial geometry, not
		// the renderer's radiance. Co-locate a Zenith_SunComponent on this
		// environment entity (the intended authoring shape) so the engine's one
		// resolved Sun carries the moving geometry; night brightness then
		// emerges from the sun dropping below the horizon + atmospheric
		// transmittance, not from animating solar intensity.
		if (!m_xParentEntity.HasComponent<Zenith_SunComponent>())
		{
			m_xParentEntity.AddComponent<Zenith_SunComponent>();
		}
	}

	void OnUpdate(const float fDt)
	{
		m_xCycle.Advance(fDt);
		// Drive the authoritative Zenith_SunComponent time-of-day from the
		// clock. CB_DayNight: 0.25 sunrise, 0.5 noon, 0.75 sunset, 0 midnight.
		// The Sun component's orbit convention: 0 sunrise, 90 noon, 180 sunset,
		// 270 midnight, so the clock phase maps as (timeOfDay - 0.25) * 360
		// (wrapped by the setter). The renderer derives direct sun radiance from
		// atmospheric transmittance: below-horizon -> zero, so the city darkens
		// physically at night. No Flux sun-intensity is mutated.
		if (Zenith_SunComponent* pxSun = m_xParentEntity.TryGetComponent<Zenith_SunComponent>())
		{
			pxSun->SetTimeOfDayAngleDegrees((m_xCycle.m_fTimeOfDay - 0.25f) * 360.0f);
		}
	}

	void OnDestroy()
	{
		if (s_pxActive == &m_xCycle)
		{
			s_pxActive = nullptr;
		}
	}

	CB_DayNight&        GetCycle()       { return m_xCycle; }
	static CB_DayNight* GetActive()      { return s_pxActive; }

private:
	Zenith_Entity m_xParentEntity;
	CB_DayNight m_xCycle;
	static inline CB_DayNight* s_pxActive = nullptr;
};
