#pragma once

#include "Collections/Zenith_Vector.h"
#include "DataStream/Zenith_DataStream.h"
#include <cstdint>

// ============================================================================
// CB_CityGrid — the spatial data model. A regular grid of 16-byte cells holding
// zone, road, building, service and derived-data state. World<->grid conversion
// is the single mapping every gameplay system uses to go between the player's
// world-space clicks and the discrete simulation grid. Pure data + logic; no
// engine/rendering dependency, so it unit-tests headlessly and serializes.
// ============================================================================

enum CB_EZoneType : uint8_t
{
	CB_ZONE_NONE        = 0,
	CB_ZONE_RESIDENTIAL = 1,
	CB_ZONE_COMMERCIAL  = 2,
	CB_ZONE_INDUSTRIAL  = 3,
	CB_ZONE_PARK        = 4,
	CB_ZONE_COUNT
};

enum CB_ERoadType : uint8_t
{
	CB_ROAD_NONE   = 0,
	CB_ROAD_SMALL  = 1,
	CB_ROAD_MEDIUM = 2,
	CB_ROAD_LARGE  = 3,
	CB_ROAD_TYPE_COUNT
};

// Neighbour-connection bits for the road piece bitmask (which sides connect).
static constexpr uint8_t CB_ROAD_DIR_NORTH = 1 << 0;  // -Z
static constexpr uint8_t CB_ROAD_DIR_EAST  = 1 << 1;  // +X
static constexpr uint8_t CB_ROAD_DIR_SOUTH = 1 << 2;  // +Z
static constexpr uint8_t CB_ROAD_DIR_WEST  = 1 << 3;  // -X

static constexpr uint16_t CB_NO_BUILDING = 0xFFFF;

struct CB_Cell
{
	uint8_t  m_uZoneAndFlags      = 0;             // [0:3]=zone [4:5]=density [6]=power [7]=water
	uint8_t  m_uRoadAndFlags      = 0;             // [0:3]=neighbour bitmask [4:5]=road type [6]=road access [7]=reserved
	uint16_t m_uBuildingType      = 0xFFFF;        // CB_EBuildingType cache (0xFFFF = none)
	uint32_t m_uBuildingRecordP1  = 0;             // building-manager record index + 1 (0 = none; u32 supports >65k buildings)
	float    m_fTerrainHeight     = 0.0f;
	uint8_t  m_uLandValue         = 0;
	uint8_t  m_uPollution         = 0;
	uint8_t  m_uCrimeRate         = 0;
	uint8_t  m_uNoiseLevel        = 0;

	// ---- Zone byte ----
	CB_EZoneType GetZone() const           { return static_cast<CB_EZoneType>(m_uZoneAndFlags & 0x0F); }
	void         SetZone(CB_EZoneType e)   { m_uZoneAndFlags = static_cast<uint8_t>((m_uZoneAndFlags & 0xF0) | (static_cast<uint8_t>(e) & 0x0F)); }
	uint8_t      GetDensity() const        { return static_cast<uint8_t>((m_uZoneAndFlags >> 4) & 0x03); }
	void         SetDensity(uint8_t d)     { m_uZoneAndFlags = static_cast<uint8_t>((m_uZoneAndFlags & ~0x30) | ((d & 0x03) << 4)); }
	bool         HasPower() const          { return (m_uZoneAndFlags & 0x40) != 0; }
	void         SetPower(bool b)          { m_uZoneAndFlags = b ? (m_uZoneAndFlags | 0x40) : (m_uZoneAndFlags & ~0x40); }
	bool         HasWater() const          { return (m_uZoneAndFlags & 0x80) != 0; }
	void         SetWater(bool b)          { m_uZoneAndFlags = b ? (m_uZoneAndFlags | 0x80) : (m_uZoneAndFlags & ~0x80); }

	// ---- Road byte ----
	uint8_t      GetRoadMask() const       { return static_cast<uint8_t>(m_uRoadAndFlags & 0x0F); }
	void         SetRoadMask(uint8_t m)    { m_uRoadAndFlags = static_cast<uint8_t>((m_uRoadAndFlags & 0xF0) | (m & 0x0F)); }
	CB_ERoadType GetRoadType() const       { return static_cast<CB_ERoadType>((m_uRoadAndFlags >> 4) & 0x03); }
	void         SetRoadType(CB_ERoadType e){ m_uRoadAndFlags = static_cast<uint8_t>((m_uRoadAndFlags & ~0x30) | ((static_cast<uint8_t>(e) & 0x03) << 4)); }
	bool         HasRoad() const           { return GetRoadType() != CB_ROAD_NONE; }
	bool         HasRoadAccess() const     { return (m_uRoadAndFlags & 0x40) != 0; }
	void         SetRoadAccess(bool b)     { m_uRoadAndFlags = b ? (m_uRoadAndFlags | 0x40) : (m_uRoadAndFlags & ~0x40); }

	// ---- Building link (record index + cached type) ----
	bool     HasBuilding() const                          { return m_uBuildingRecordP1 != 0; }
	void     SetBuilding(uint32_t uRecord, uint8_t uType) { m_uBuildingRecordP1 = uRecord + 1; m_uBuildingType = uType; }
	void     ClearBuilding()                              { m_uBuildingRecordP1 = 0; m_uBuildingType = 0xFFFF; }
	uint32_t GetBuildingRecord() const                    { return m_uBuildingRecordP1 - 1; }
	uint8_t  GetBuildingType() const                      { return static_cast<uint8_t>(m_uBuildingType); }
};
static_assert(sizeof(CB_Cell) == 16, "CB_Cell must be 16 bytes");

class CB_CityGrid
{
public:
	void Initialize(uint32_t uWidth, uint32_t uHeight, float fCellSize, float fOriginX, float fOriginZ);
	void Shutdown();

	bool     IsInitialized() const { return m_uWidth > 0 && m_uHeight > 0; }
	uint32_t GetWidth()      const { return m_uWidth; }
	uint32_t GetHeight()     const { return m_uHeight; }
	float    GetCellSize()   const { return m_fCellSize; }
	float    GetOriginX()    const { return m_fOriginX; }
	float    GetOriginZ()    const { return m_fOriginZ; }

	bool IsInBounds(uint32_t uX, uint32_t uZ) const { return uX < m_uWidth && uZ < m_uHeight; }

	CB_Cell&       GetCell(uint32_t uX, uint32_t uZ);
	const CB_Cell& GetCell(uint32_t uX, uint32_t uZ) const;

	// World<->grid. WorldToGrid returns false if the point is outside the grid.
	bool WorldToGrid(float fWorldX, float fWorldZ, uint32_t& uOutX, uint32_t& uOutZ) const;
	void GridToWorld(uint32_t uX, uint32_t uZ, float& fOutX, float& fOutZ) const;  // cell centre

	// Zone painting over a square half-extent brush (uRadius=0 => single cell).
	// Cells already carrying a road are skipped (roads aren't zoned).
	void PaintZone(uint32_t uCentreX, uint32_t uCentreZ, uint32_t uRadius, CB_EZoneType eZone, uint8_t uDensity);
	void ClearZone(uint32_t uCentreX, uint32_t uCentreZ, uint32_t uRadius);

	// Flood-fill road access from every road cell: cells orthogonally adjacent to
	// a road (or themselves a road) are marked HasRoadAccess. Cheap full sweep.
	void RecalculateRoadAccess();

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_Vector<CB_Cell> m_axCells;
	uint32_t m_uWidth   = 0;
	uint32_t m_uHeight  = 0;
	float    m_fCellSize = 4.0f;
	float    m_fOriginX  = 0.0f;
	float    m_fOriginZ  = 0.0f;
};
