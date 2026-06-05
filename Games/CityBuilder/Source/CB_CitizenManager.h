#pragma once

#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "Collections/Zenith_Vector.h"
#include <cstdint>

// ============================================================================
// CB_CitizenManager — the abstract citizen pool. Sized to housing capacity and
// matched to available jobs each sim tick; drives the employment rate + a
// happiness contribution. Per-citizen commute progress advances incrementally
// (no teleport) and is the data source for the deferred visual-agent pass.
// ============================================================================

enum CB_ECitizenState : uint8_t
{
	CB_CITIZEN_AT_HOME        = 0,
	CB_CITIZEN_COMMUTING_WORK = 1,
	CB_CITIZEN_AT_WORK        = 2,
	CB_CITIZEN_COMMUTING_HOME = 3,
	CB_CITIZEN_UNEMPLOYED     = 4
};

struct CB_Citizen
{
	CB_ECitizenState m_eState          = CB_CITIZEN_UNEMPLOYED;
	float            m_fCommuteProgress = 0.0f;  // 0..1 along the current leg
	bool             m_bEmployed        = false;
	bool             m_bActive          = false;
};

class CB_CitizenManager
{
public:
	static constexpr uint32_t uMAX_CITIZENS = 200000;

	void Initialize(CB_CityGrid* pxGrid, CB_RoadNetwork* pxRoads);
	void Shutdown();
	bool IsInitialized() const { return m_pxGrid != nullptr; }

	// Resize the pool to housing capacity and assign employment from job count.
	void Sync(uint32_t uHousingCapacity, uint32_t uJobs);
	// Advance commute progress (no teleport — bounded per-call step, state cycles).
	void UpdateCommutes(float fStep);

	uint32_t GetActiveCitizens() const { return m_uActive; }
	uint32_t GetEmployedCount()  const { return m_uEmployed; }
	float    GetEmploymentRate() const { return (m_uActive > 0) ? (static_cast<float>(m_uEmployed) / static_cast<float>(m_uActive)) : 0.0f; }
	float    GetAverageHappiness() const { return 0.40f + 0.60f * GetEmploymentRate(); }

private:
	Zenith_Vector<CB_Citizen> m_axCitizens;
	uint32_t        m_uActive   = 0;
	uint32_t        m_uEmployed = 0;
	CB_CityGrid*    m_pxGrid    = nullptr;
	CB_RoadNetwork* m_pxRoads   = nullptr;
};
