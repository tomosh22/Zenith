#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_CityGrid.h"   // CB_EZoneType
#include <cstdint>

// ============================================================================
// CB_Zoning — Cities: Skylines-style road-relative zoning. Each road segment
// generates a row of buildable LOTS along both frontages (free world positions,
// rotated to face the road). The player paints R/C/I/park onto lots; buildings
// (G4) grow in zoned, unoccupied lots facing the road. Lots are tied to their
// segment (created when the road is drawn, removed on bulldoze) so painted zones
// persist across road edits — SyncToGraph reconciles incrementally each frame.
// ============================================================================

struct CB_Lot
{
	Zenith_Maths::Vector2 m_xPos     = Zenith_Maths::Vector2(0.0f, 0.0f);  // world XZ plot centre
	Zenith_Maths::Vector2 m_xFaceDir = Zenith_Maths::Vector2(0.0f, 1.0f);  // unit dir toward the road (building faces this)
	float        m_fWorldY     = 0.0f;
	CB_EZoneType m_eZone       = CB_ZONE_NONE;
	uint8_t      m_uDensity    = 0;             // 0..2 RCI density target
	uint32_t     m_uBuildingId = 0xFFFFFFFFu;   // occupant building record (INVALID = empty)
	uint32_t     m_uSegment    = 0xFFFFFFFFu;   // owning road segment
	bool         m_bActive     = false;
};

class CB_Zoning
{
public:
	static constexpr float    LOT_SPACING = 14.0f;   // along-road spacing between lot centres
	static constexpr float    LOT_DEPTH   = 14.0f;   // plot depth (back from the road edge)
	static constexpr uint32_t INVALID     = 0xFFFFFFFFu;

	void Reset();

	// Reconcile lots with the graph: add lots for newly-active segments, remove
	// lots for gone segments. Cheap (diffs the per-segment "has lots" flags).
	void SyncToGraph(const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField);

	// Paint a zone over active lots within fRadius of (wx,wz). Returns count painted.
	uint32_t PaintZone(float fWorldX, float fWorldZ, float fRadius, CB_EZoneType eZone, uint8_t uDensity);

	// Render the zone overlay (flat colour-coded quads on zoned lots) — windowed.
	void RenderOverlay() const;

	uint32_t      GetLotSlotCount() const { return m_axLots.GetSize(); }
	const CB_Lot& GetLot(uint32_t i) const { return m_axLots.Get(i); }
	CB_Lot&       GetLotMutable(uint32_t i) { return m_axLots.Get(i); }
	uint32_t      GetActiveLotCount() const { return m_uActiveLots; }
	uint32_t      CountZonedLots(CB_EZoneType eZone) const;

	// Serialize all lots (+ the per-segment flag array). Zenith_DataStream is
	// forward-declared via CB_RoadGraph.h. POD elements.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	void AddSegmentLots(uint32_t uSeg, const CB_RoadSegment& xSeg, const CB_TerrainHeightfield& xField);
	void RemoveSegmentLots(uint32_t uSeg);

	Zenith_Vector<CB_Lot> m_axLots;
	Zenith_Vector<bool>   m_abSegHasLots;   // indexed by segment slot
	uint32_t              m_uActiveLots = 0;
};
