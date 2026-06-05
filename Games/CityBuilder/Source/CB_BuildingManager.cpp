#include "Zenith.h"

#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_TerrainModifier.h"
#include "CityBuilder/Source/CB_Events.h"
#include "ZenithECS/Zenith_EventSystem.h"

void CB_BuildingManager::Initialize(CB_CityGrid* pxGrid)
{
	m_pxGrid = pxGrid;
	m_axRecords.Clear();
	m_auFreeList.Clear();
	m_uActiveCount  = 0;
	m_uGrowthCursor = 0;
}

void CB_BuildingManager::Shutdown()
{
	m_axRecords.Clear();
	m_auFreeList.Clear();
	m_pxGrid = nullptr;
	m_uActiveCount = 0;
}

uint32_t CB_BuildingManager::AllocRecord()
{
	if (m_auFreeList.GetSize() > 0)
	{
		const uint32_t uIdx = m_auFreeList.GetBack();
		m_auFreeList.PopBack();
		return uIdx;
	}
	CB_BuildingRecord xRec;
	m_axRecords.PushBack(xRec);
	return m_axRecords.GetSize() - 1;
}

uint32_t CB_BuildingManager::SpawnBuilding(CB_EBuildingType eType, uint32_t uX, uint32_t uZ)
{
	if (!m_pxGrid || !m_pxGrid->IsInBounds(uX, uZ) || eType >= CB_BUILDING_COUNT)
	{
		return INVALID;
	}
	CB_Cell& xCell = m_pxGrid->GetCell(uX, uZ);
	if (xCell.HasBuilding() || xCell.HasRoad())
	{
		return INVALID;
	}

	const CB_BuildingDef& xDef = CB_BuildingDefs::Get(eType);

	const uint32_t uRecord = AllocRecord();
	CB_BuildingRecord& xRec = m_axRecords.Get(uRecord);
	xRec.m_eType      = eType;
	xRec.m_uGridX     = uX;
	xRec.m_uGridZ     = uZ;
	xRec.m_uOccupants = xDef.m_uMaxOccupants;
	xRec.m_uLevel     = 0;
	xRec.m_bPowered   = false;
	xRec.m_bWatered   = false;
	xRec.m_bActive    = true;
	xRec.m_uInstanceID = 0;

	xCell.SetBuilding(uRecord, static_cast<uint8_t>(eType));

	// Sit the footprint on (and flatten) the terrain.
	float fWorldX, fWorldZ;
	m_pxGrid->GridToWorld(uX, uZ, fWorldX, fWorldZ);
	xCell.m_fTerrainHeight = CB_TerrainModifier::GetHeightAt(fWorldX, fWorldZ);
	CB_TerrainModifier::FlattenForBuilding(fWorldX, fWorldZ, m_pxGrid->GetCellSize() * 0.5f);

	++m_uActiveCount;
	return uRecord;
}

bool CB_BuildingManager::RemoveBuilding(uint32_t uRecordIndex)
{
	if (uRecordIndex >= m_axRecords.GetSize())
	{
		return false;
	}
	CB_BuildingRecord& xRec = m_axRecords.Get(uRecordIndex);
	if (!xRec.m_bActive)
	{
		return false;
	}
	if (m_pxGrid && m_pxGrid->IsInBounds(xRec.m_uGridX, xRec.m_uGridZ))
	{
		m_pxGrid->GetCell(xRec.m_uGridX, xRec.m_uGridZ).ClearBuilding();
	}
	xRec.m_bActive = false;
	xRec.m_eType = CB_BUILDING_NONE;
	m_auFreeList.PushBack(uRecordIndex);
	if (m_uActiveCount > 0)
	{
		--m_uActiveCount;
	}
	return true;
}

bool CB_BuildingManager::RemoveBuildingAtCell(uint32_t uX, uint32_t uZ)
{
	if (!m_pxGrid || !m_pxGrid->IsInBounds(uX, uZ))
	{
		return false;
	}
	const CB_Cell& xCell = m_pxGrid->GetCell(uX, uZ);
	if (!xCell.HasBuilding())
	{
		return false;
	}
	return RemoveBuilding(xCell.GetBuildingRecord());
}

uint32_t CB_BuildingManager::ProcessGrowth(float fResDemand, float fComDemand, float fIndDemand,
                                           uint32_t uMaxSpawns, uint32_t uMaxScan)
{
	if (!m_pxGrid)
	{
		return 0;
	}
	const uint32_t uTotal = m_pxGrid->GetWidth() * m_pxGrid->GetHeight();
	if (uTotal == 0)
	{
		return 0;
	}

	uint32_t uSpawned = 0;
	uint32_t uScanned = 0;
	while (uScanned < uMaxScan && uSpawned < uMaxSpawns)
	{
		const uint32_t uIndex = m_uGrowthCursor % uTotal;
		const uint32_t uX = uIndex % m_pxGrid->GetWidth();
		const uint32_t uZ = uIndex / m_pxGrid->GetWidth();
		m_uGrowthCursor = (m_uGrowthCursor + 1) % uTotal;
		++uScanned;

		const CB_Cell& xCell = m_pxGrid->GetCell(uX, uZ);
		if (xCell.HasBuilding() || xCell.HasRoad() || !xCell.HasRoadAccess())
		{
			continue;
		}
		const CB_EZoneType eZone = xCell.GetZone();
		float fDemand = 0.0f;
		if (eZone == CB_ZONE_RESIDENTIAL)      { fDemand = fResDemand; }
		else if (eZone == CB_ZONE_COMMERCIAL)  { fDemand = fComDemand; }
		else if (eZone == CB_ZONE_INDUSTRIAL)  { fDemand = fIndDemand; }
		else                                   { continue; }

		if (fDemand <= 0.0f)
		{
			continue;
		}
		const CB_EBuildingType eType = CB_BuildingDefs::TypeForZone(eZone, xCell.GetDensity());
		if (eType == CB_BUILDING_NONE)
		{
			continue;
		}
		if (SpawnBuilding(eType, uX, uZ) != INVALID)
		{
			++uSpawned;
			Zenith_EventDispatcher::Get().Dispatch(CB_OnBuildingGrew{
				uX, uZ, static_cast<uint8_t>(eType), CB_BuildingDefs::Get(eType).m_uMaxOccupants });
		}
	}
	return uSpawned;
}

uint32_t CB_BuildingManager::GetTotalPopulation() const
{
	uint32_t uPop = 0;
	for (uint32_t u = 0; u < m_axRecords.GetSize(); ++u)
	{
		const CB_BuildingRecord& xRec = m_axRecords.Get(u);
		if (xRec.m_bActive && CB_BuildingDefs::IsResidential(xRec.m_eType))
		{
			uPop += xRec.m_uOccupants;
		}
	}
	return uPop;
}

uint32_t CB_BuildingManager::GetTotalJobs() const
{
	uint32_t uJobs = 0;
	for (uint32_t u = 0; u < m_axRecords.GetSize(); ++u)
	{
		const CB_BuildingRecord& xRec = m_axRecords.Get(u);
		if (xRec.m_bActive && (CB_BuildingDefs::IsCommercial(xRec.m_eType) || CB_BuildingDefs::IsIndustrial(xRec.m_eType)))
		{
			uJobs += xRec.m_uOccupants;
		}
	}
	return uJobs;
}

void CB_BuildingManager::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_uActiveCount;
	const uint32_t uCount = m_axRecords.GetSize();
	xStream << uCount;
	for (uint32_t u = 0; u < uCount; ++u)
	{
		xStream << m_axRecords.Get(u);   // trivially copyable
	}
	const uint32_t uFree = m_auFreeList.GetSize();
	xStream << uFree;
	for (uint32_t u = 0; u < uFree; ++u)
	{
		xStream << m_auFreeList.Get(u);
	}
}

void CB_BuildingManager::ReadFromDataStream(Zenith_DataStream& xStream)
{
	m_axRecords.Clear();
	m_auFreeList.Clear();
	xStream >> m_uActiveCount;
	uint32_t uCount = 0;
	xStream >> uCount;
	for (uint32_t u = 0; u < uCount; ++u)
	{
		CB_BuildingRecord xRec;
		xStream >> xRec;
		m_axRecords.PushBack(xRec);
	}
	uint32_t uFree = 0;
	xStream >> uFree;
	for (uint32_t u = 0; u < uFree; ++u)
	{
		uint32_t uIdx = 0;
		xStream >> uIdx;
		m_auFreeList.PushBack(uIdx);
	}
}
