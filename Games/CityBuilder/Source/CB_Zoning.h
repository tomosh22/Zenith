#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_Zones.h"   // CB_EZoneType
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

	// Render "ghost" markers (colour-keeping ring outlines, in eActiveZone's colour) on every
	// AVAILABLE placement lot — active, unzoned, unbuilt frontage — so when an R/C/I tool is
	// selected the player sees exactly where a zone can be placed. Returns the number of ghosts
	// drawn (telemetry). Windowed (immediate-mode primitives). Filled lit slabs wash to white,
	// so rings (AddCircle) are used to keep the hue (memory reference-screen-capture-...-winding).
	uint32_t RenderPlacementGhosts(CB_EZoneType eActiveZone) const;

	// Pure count of available placement lots (active, unzoned, unbuilt) — headless-safe; the
	// number of ghosts RenderPlacementGhosts would draw. Lets a logic test assert availability
	// without the GPU.
	uint32_t CountAvailableLots() const;

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
	void AddSegmentLots(uint32_t uSeg, const CB_RoadSegment& xSeg, const CB_RoadGraph& xGraph, const CB_TerrainHeightfield& xField);
	void RemoveSegmentLots(uint32_t uSeg);
	// True if a candidate lot centre is clear of EVERY road carriageway (by fRoadClear beyond each
	// road's half-width) AND of every already-placed active lot (by fMinLotDist). Stops lots
	// overlapping the road / intersections / each other at junctions + between close parallel roads.
	bool IsLotPositionClear(const Zenith_Maths::Vector2& xPos, const CB_RoadGraph& xGraph,
	                        float fRoadClear, float fMinLotDist) const;

	Zenith_Vector<CB_Lot> m_axLots;
	Zenith_Vector<bool>   m_abSegHasLots;   // indexed by segment slot
	uint32_t              m_uActiveLots = 0;
};
