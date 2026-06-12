#pragma once
#include "Core/Zenith_Engine.h"

#include "ZenithECS/Zenith_Entity.h"
#include "DataStream/Zenith_DataStream.h"
#include "Core/Zenith_CommandLine.h"
#include "CityBuilder/Source/CB_DayNight.h"
#include "Flux/Skybox/Flux_SkyboxImpl.h"

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// ============================================================================
// CB_DayNightCycleComponent — advances the day/night clock each frame and
// publishes it as the active cycle. Applying the sun direction to the engine
// sky/sun light is the visual-pass remainder; the clock + sun math (CB_DayNight)
// is unit-tested and available to any system that wants the time of day.
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
	}

	void OnUpdate(const float fDt)
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
