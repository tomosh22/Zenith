#pragma once

#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_BuildingDefs.h"
#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include <cstdint>

// ============================================================================
// CB_BuildingManager — building lifecycle over the grid. Buildings occupy a
// cell (record index stored on the cell), provide population/jobs/services, and
// auto-grow on zoned + road-connected cells when there is demand. No physics
// bodies — placement is grid-occupancy. Rendering (one instanced batch per
// type) is layered on in the visual pass.
// ============================================================================

struct CB_BuildingRecord
{
	CB_EBuildingType m_eType      = CB_BUILDING_NONE;
	uint32_t         m_uGridX     = 0;
	uint32_t         m_uGridZ     = 0;
	uint16_t         m_uOccupants = 0;
	uint8_t          m_uLevel     = 0;       // upgrade level (visual scale)
	bool             m_bPowered   = false;
	bool             m_bWatered   = false;
	bool             m_bActive    = false;
	uint32_t         m_uInstanceID = 0;      // render instance (visual; unused for now)
};

class CB_BuildingManager
{
public:
	static constexpr uint32_t INVALID = 0xFFFFFFFF;

	void Initialize(CB_CityGrid* pxGrid);
	void Shutdown();
	bool IsInitialized() const { return m_pxGrid != nullptr; }

	// Spawn a building on a cell. Returns its record index, or INVALID if the
	// cell is out of bounds / occupied / a road. Flattens the footprint.
	uint32_t SpawnBuilding(CB_EBuildingType eType, uint32_t uX, uint32_t uZ);
	bool     RemoveBuilding(uint32_t uRecordIndex);
	bool     RemoveBuildingAtCell(uint32_t uX, uint32_t uZ);

	// Auto-grow RCI buildings on eligible cells given current demand. Spawns up
	// to uMaxSpawns, scanning at most uMaxScan cells from a rolling cursor so a
	// huge grid never costs a full sweep in one call. Returns spawn count.
	uint32_t ProcessGrowth(float fResDemand, float fComDemand, float fIndDemand,
	                       uint32_t uMaxSpawns, uint32_t uMaxScan);

	uint32_t                 GetRecordCount() const { return m_axRecords.GetSize(); }
	const CB_BuildingRecord& GetRecord(uint32_t u) const { return m_axRecords.Get(u); }

	// Set powered/watered on an active record (called by the service manager).
	void SetServiceFlags(uint32_t uRecord, bool bPowered, bool bWatered)
	{
		if (uRecord < m_axRecords.GetSize())
		{
			CB_BuildingRecord& xRec = m_axRecords.Get(uRecord);
			if (xRec.m_bActive)
			{
				xRec.m_bPowered = bPowered;
				xRec.m_bWatered = bWatered;
			}
		}
	}
	uint32_t                 GetActiveCount() const { return m_uActiveCount; }
	uint32_t                 GetTotalPopulation() const;
	uint32_t                 GetTotalJobs() const;

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	uint32_t AllocRecord();

	CB_CityGrid*                     m_pxGrid = nullptr;
	Zenith_Vector<CB_BuildingRecord> m_axRecords;
	Zenith_Vector<uint32_t>          m_auFreeList;
	uint32_t                         m_uActiveCount  = 0;
	uint32_t                         m_uGrowthCursor = 0;
};
