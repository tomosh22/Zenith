#pragma once

#include "CityBuilder/Source/CB_CityGrid.h"
#include <cstdint>

// ============================================================================
// CB_RoadNetwork — road placement + auto-piece selection over the city grid.
//
// Roads live on grid cells (road type + a 4-bit N/E/S/W neighbour bitmask).
// DeterminePieceFromMask maps the bitmask to one of 11 visual piece types so a
// placed road auto-connects to its neighbours. Placement/removal refreshes the
// cell and its four neighbours' masks and updates road access. Rendering (one
// instanced batch per piece) is layered on top in the visual pass; the logic
// here is pure + headless-testable.
// ============================================================================

enum CB_ERoadPiece : uint8_t
{
	CB_ROAD_PIECE_STRAIGHT_NS  = 0,
	CB_ROAD_PIECE_STRAIGHT_EW  = 1,
	CB_ROAD_PIECE_CORNER_NE    = 2,
	CB_ROAD_PIECE_CORNER_SE    = 3,
	CB_ROAD_PIECE_CORNER_SW    = 4,
	CB_ROAD_PIECE_CORNER_NW    = 5,
	CB_ROAD_PIECE_T_N          = 6,  // stem points North (bar E-W)
	CB_ROAD_PIECE_T_E          = 7,  // stem points East  (bar N-S)
	CB_ROAD_PIECE_T_S          = 8,  // stem points South (bar E-W)
	CB_ROAD_PIECE_T_W          = 9,  // stem points West  (bar N-S)
	CB_ROAD_PIECE_INTERSECTION = 10,
	CB_ROAD_PIECE_COUNT        = 11
};

class CB_RoadNetwork
{
public:
	void Initialize(CB_CityGrid* pxGrid);
	void Shutdown();

	bool IsInitialized() const { return m_pxGrid != nullptr; }

	// Place / remove a road on a cell. Returns false if out of bounds or the cell
	// is occupied by a building. Placing clears any zone on the cell.
	bool PlaceRoad(uint32_t uX, uint32_t uZ, CB_ERoadType eType);
	bool RemoveRoad(uint32_t uX, uint32_t uZ);

	// Neighbour bitmask (CB_ROAD_DIR_*) of road-carrying orthogonal neighbours.
	uint8_t ComputeNeighbourMask(uint32_t uX, uint32_t uZ) const;

	// Visual piece for a cell, derived from its stored neighbour mask.
	CB_ERoadPiece GetPieceAt(uint32_t uX, uint32_t uZ) const;

	// Pure mask -> piece mapping (the 16-entry truth table).
	static CB_ERoadPiece DeterminePieceFromMask(uint8_t uMask);

	uint32_t GetRoadCellCount() const { return m_uRoadCellCount; }

	// Flat list of road cell indices (uZ*GetWidth() + uX), maintained on place/
	// remove — lets the renderer iterate roads without scanning the whole grid.
	const Zenith_Vector<uint32_t>& GetRoadCells() const { return m_axRoadCells; }

	// Shortest path over road cells (4-connected, uniform step cost => BFS gives
	// the optimum). Fills xOutPath with cell indices (uZ*GetWidth() + uX) from
	// start to end inclusive. Returns false if either endpoint isn't a road, no
	// path exists, or the search exceeds its node budget.
	bool FindPath(uint32_t uStartX, uint32_t uStartZ, uint32_t uEndX, uint32_t uEndZ,
	              Zenith_Vector<uint32_t>& xOutPath) const;

private:
	// Recompute the stored neighbour mask for a cell (if it has a road) and for
	// each of its four orthogonal neighbours (their masks change too).
	void RefreshMasks(uint32_t uX, uint32_t uZ);
	void UpdateLocalRoadAccess(uint32_t uX, uint32_t uZ);

	CB_CityGrid*            m_pxGrid = nullptr;
	uint32_t                m_uRoadCellCount = 0;
	Zenith_Vector<uint32_t> m_axRoadCells;
};
