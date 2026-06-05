#include "Zenith.h"

#include "CityBuilder/Source/CB_EconomyManager.h"

namespace
{
	float Clamp(float f, float lo, float hi) { return (f < lo) ? lo : ((f > hi) ? hi : f); }
}

void CB_EconomyManager::ComputeDemand(CB_CityStats& xStats) const
{
	const float fPop = static_cast<float>(xStats.m_uPopulation);
	const float fCom = static_cast<float>(xStats.m_uCommercialJobs);
	const float fInd = static_cast<float>(xStats.m_uIndustrialJobs);
	const float fJobs = fCom + fInd;

	// Seed demand so an empty, zoned + road-connected city starts to grow.
	const float fBase  = 100.0f;
	const float fScale = 300.0f;

	// Jobs pull residents; an oversupply of residents (more people than jobs)
	// pushes residential demand negative.
	xStats.m_fResDemand = Clamp((fJobs + fBase - fPop) / fScale, -1.0f, 1.0f);
	// Population creates retail demand; commerce satisfies it.
	xStats.m_fComDemand = Clamp((fPop * 0.30f - fCom) / fScale, -1.0f, 1.0f);
	// Population + commerce create demand for goods/jobs that industry supplies.
	xStats.m_fIndDemand = Clamp((fPop * 0.20f + fCom * 0.50f - fInd) / fScale, -1.0f, 1.0f);
}

void CB_EconomyManager::CollectTaxes(const CB_BuildingManager& xBuildings, const CB_RoadNetwork& xRoads, CB_CityStats& xStats) const
{
	float fIncome = 0.0f;
	float fExpenses = 0.0f;

	const uint32_t uRecords = xBuildings.GetRecordCount();
	for (uint32_t u = 0; u < uRecords; ++u)
	{
		const CB_BuildingRecord& xRec = xBuildings.GetRecord(u);
		if (!xRec.m_bActive)
		{
			continue;
		}
		const CB_BuildingDef& xDef = CB_BuildingDefs::Get(xRec.m_eType);
		const float fOccFrac = (xDef.m_uMaxOccupants > 0)
			? (static_cast<float>(xRec.m_uOccupants) / static_cast<float>(xDef.m_uMaxOccupants))
			: 1.0f;
		const float fTaxMul = xRec.m_bPowered ? 1.0f : 0.5f;
		fIncome   += xDef.m_fTaxRevenue * fOccFrac * fTaxMul;
		fExpenses += xDef.m_fUpkeep;
	}

	// Road maintenance.
	fExpenses += static_cast<float>(xRoads.GetRoadCellCount()) * 0.5f;

	xStats.m_fIncomePerTick   = fIncome;
	xStats.m_fExpensesPerTick = fExpenses;
	xStats.m_fTreasury       += (fIncome - fExpenses);
}
