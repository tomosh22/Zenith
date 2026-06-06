#pragma once

#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include <cstdint>

// ============================================================================
// CB_ToolSystem — the player's active build tool + the terrain-aware mouse picker.
// Update() reads the tool-selection hotkeys (1-9/0/T/B/L/K); the free-form tools
// themselves are applied by CB_RoadController (road/zone/service/bulldoze) which
// reads GetTool(). PickGroundPoint unprojects the cursor and ray-marches it onto
// the hilly terrain surface — the world point every free-form tool builds at.
//
//   1 Residential  2 Commercial  3 Industrial  4 Park  5 Road
//   6 Services(cycle)  7 Power  8 Water  9 Bulldoze  0 None
//   T Terraform  B District  L Transit  K Conduit
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

	// Heightfield used for terrain-aware picking — the cursor ray is intersected with the
	// hilly terrain SURFACE, not a flat y=0 plane (which drifts tens of metres on hills).
	// Set once by the manager; null → PickGroundPoint falls back to the y=0 plane.
	void     SetTerrainField(const CB_TerrainHeightfield* pxField) { m_pxTerrainField = pxField; }

	// Read the tool-selection hotkeys; dispatches CB_OnToolSelected on a change. The free-form
	// tools are applied by CB_RoadController (which reads GetTool()).
	void Update();

	// Unproject the mouse + ray-march onto the terrain SURFACE (the hilly heightfield set via
	// SetTerrainField); returns the world hit point. Falls back to the flat y=0 plane if no
	// terrain is set. Used by every free-form tool (road/zone/service/terraform/...).
	bool PickGroundPoint(float& fOutX, float& fOutZ) const;

	static const char* ToolName(CB_ETool eTool);

private:
	CB_ETool m_eTool = CB_TOOL_NONE;
	const CB_TerrainHeightfield* m_pxTerrainField = nullptr;   // for terrain-aware picking
};
