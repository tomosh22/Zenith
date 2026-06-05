#pragma once

#include "CityBuilder/Source/CB_CityGrid.h"
#include "CityBuilder/Source/CB_RoadNetwork.h"
#include "CityBuilder/Source/CB_BuildingManager.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include <cstdint>

// ============================================================================
// CB_ToolSystem — player build tools. Reads input, picks the ground cell under
// the cursor (camera unproject -> ground plane), and applies the active tool on
// left-click / drag. Number keys 1-9 switch tools.
//
//   1 Residential  2 Commercial  3 Industrial  4 Park  5 Road
//   6 Police       7 Power       8 Water       9 Bulldoze   T Terraform
// ============================================================================

enum CB_ETool : uint8_t
{
	CB_TOOL_NONE        = 0,
	CB_TOOL_ZONE_RES    = 1,
	CB_TOOL_ZONE_COM    = 2,
	CB_TOOL_ZONE_IND    = 3,
	CB_TOOL_ZONE_PARK   = 4,
	CB_TOOL_ROAD        = 5,
	CB_TOOL_POLICE      = 6,
	CB_TOOL_POWER       = 7,
	CB_TOOL_WATER       = 8,
	CB_TOOL_BULLDOZE    = 9,
	CB_TOOL_TERRAFORM   = 10,   // hold left = raise / right = lower the terrain under the cursor
	CB_TOOL_DISTRICT    = 11,   // left-click paints a district; F1-F4 toggle its policies
	CB_TOOL_TRANSIT     = 12,   // left-click adds a transit stop to the current line; right starts a new line
	CB_TOOL_CONDUIT     = 13,   // left-click lays a utility conduit (extends power/water reach)
	CB_TOOL_COUNT
};

class CB_ToolSystem
{
public:
	void     SetTool(CB_ETool eTool) { m_eTool = eTool; }
	CB_ETool GetTool() const         { return m_eTool; }
	void     SetBrushRadius(uint32_t u) { m_uBrushRadius = u; }

	// Read input (tool keys + click), pick the ground cell, apply the tool.
	void Update(CB_CityGrid& xGrid, CB_RoadNetwork& xRoads, CB_BuildingManager& xBuildings, CB_TerrainHeightfield& xTerrain);

	// Unproject the mouse onto the ground plane and convert to a grid cell.
	bool PickGroundCell(const CB_CityGrid& xGrid, uint32_t& uOutX, uint32_t& uOutZ) const;

	// Unproject the mouse onto the ground plane (y=0); returns the world hit point.
	// Used by the free-form spline road tool (no grid).
	bool PickGroundPoint(float& fOutX, float& fOutZ) const;

	// Apply the active tool at a grid cell (also callable by tests).
	void ApplyToolAt(uint32_t uX, uint32_t uZ, CB_CityGrid& xGrid, CB_RoadNetwork& xRoads,
	                 CB_BuildingManager& xBuildings, CB_TerrainHeightfield& xTerrain);

	static const char* ToolName(CB_ETool eTool);

private:
	CB_ETool m_eTool       = CB_TOOL_NONE;
	uint32_t m_uBrushRadius = 2;
};
