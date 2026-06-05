#include "Zenith.h"

#include "CityBuilder/Source/CB_CitizenManager.h"

void CB_CitizenManager::Initialize(CB_CityGrid* pxGrid, CB_RoadNetwork* pxRoads)
{
	m_pxGrid  = pxGrid;
	m_pxRoads = pxRoads;
	m_axCitizens.Clear();
	m_uActive   = 0;
	m_uEmployed = 0;
}

void CB_CitizenManager::Shutdown()
{
	m_axCitizens.Clear();
	m_pxGrid  = nullptr;
	m_pxRoads = nullptr;
	m_uActive   = 0;
	m_uEmployed = 0;
}

void CB_CitizenManager::Sync(uint32_t uHousingCapacity, uint32_t uJobs)
{
	const uint32_t uTarget = (uHousingCapacity < uMAX_CITIZENS) ? uHousingCapacity : uMAX_CITIZENS;

	// Grow the backing store to hold the active set.
	while (m_axCitizens.GetSize() < uTarget)
	{
		CB_Citizen xCit;
		xCit.m_bActive = true;
		m_axCitizens.PushBack(xCit);
	}

	const uint32_t uEmployed = (uTarget < uJobs) ? uTarget : uJobs;

	for (uint32_t u = 0; u < m_axCitizens.GetSize(); ++u)
	{
		CB_Citizen& xCit = m_axCitizens.Get(u);
		xCit.m_bActive = (u < uTarget);
		if (!xCit.m_bActive)
		{
			xCit.m_bEmployed = false;
			xCit.m_eState = CB_CITIZEN_UNEMPLOYED;
			continue;
		}
		const bool bEmployed = (u < uEmployed);
		xCit.m_bEmployed = bEmployed;
		if (!bEmployed)
		{
			xCit.m_eState = CB_CITIZEN_UNEMPLOYED;
		}
		else if (xCit.m_eState == CB_CITIZEN_UNEMPLOYED)
		{
			// Newly employed: start the day at home.
			xCit.m_eState = CB_CITIZEN_AT_HOME;
			xCit.m_fCommuteProgress = 0.0f;
		}
	}

	m_uActive   = uTarget;
	m_uEmployed = uEmployed;
}

void CB_CitizenManager::UpdateCommutes(float fStep)
{
	if (fStep <= 0.0f)
	{
		return;
	}
	for (uint32_t u = 0; u < m_uActive; ++u)
	{
		CB_Citizen& xCit = m_axCitizens.Get(u);
		if (!xCit.m_bActive || !xCit.m_bEmployed)
		{
			continue;
		}
		// Advance progress; cycle through the daily legs when a leg completes.
		// Incremental only — never snaps position (no-teleport rule applies to
		// the visual agents that read this progress).
		xCit.m_fCommuteProgress += fStep;
		if (xCit.m_fCommuteProgress >= 1.0f)
		{
			xCit.m_fCommuteProgress = 0.0f;
			switch (xCit.m_eState)
			{
			case CB_CITIZEN_AT_HOME:        xCit.m_eState = CB_CITIZEN_COMMUTING_WORK; break;
			case CB_CITIZEN_COMMUTING_WORK: xCit.m_eState = CB_CITIZEN_AT_WORK;        break;
			case CB_CITIZEN_AT_WORK:        xCit.m_eState = CB_CITIZEN_COMMUTING_HOME; break;
			case CB_CITIZEN_COMMUTING_HOME: xCit.m_eState = CB_CITIZEN_AT_HOME;        break;
			default:                        xCit.m_eState = CB_CITIZEN_AT_HOME;        break;
			}
		}
	}
}
