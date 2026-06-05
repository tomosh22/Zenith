#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_RoadGraph.h"
#include "CityBuilder/Source/CB_TerrainHeightfield.h"
#include "CityBuilder/Source/CB_ToolSystem.h"
#include "CityBuilder/Source/CB_Zoning.h"
#include "CityBuilder/Source/CB_BuildingPlacement.h"

// ============================================================================
// CB_RoadController — the Cities: Skylines free-form road system: owns the
// CB_RoadGraph, the interactive curved-road draw tool (click points → smooth
// tangent-continuous spline segments, snapping to existing nodes for junctions),
// bulldoze, and the terrain-following ribbon rendering (G1: flat triangles via
// Flux_Primitives). The CityManager owns one of these and drives Update/Render.
// ============================================================================

class CB_RoadController
{
public:
	static constexpr float ROAD_Y_OFFSET = 0.08f;   // ribbon sits just above the terrain (G1; G2 carves)
	static constexpr float SNAP_RADIUS    = 7.0f;    // click within this of a node → snap (junction)
	static constexpr float MIN_SEGMENT    = 2.0f;    // ignore segments shorter than this

	void Reset();

	static constexpr float ZONE_BRUSH_RADIUS = 22.0f;

	// Read input for the active tool: road = draw curved segments, zone tools =
	// paint zones onto frontage lots, service/utility tools = place buildings at
	// the cursor, bulldoze = remove nearest road. Rebuilds the ribbon cache on
	// change. Windowed only.
	void Update(const CB_ToolSystem& xTools, const CB_TerrainHeightfield& xField, CB_Zoning& xZoning, CB_BuildingPlacement& xBuild);

	// Submit the road ribbons to Flux_Primitives this frame. Windowed only.
	void Render() const;

	// Commit a clicked world point to the in-progress road (also callable by tests).
	void HandleClick(float fWorldX, float fWorldZ);
	// End the in-progress road (right-click / Esc / tool change).
	void EndRoad();
	// Remove the nearest segment to a world point.
	void BulldozeAt(float fWorldX, float fWorldZ);

	void SetRoadClass(CB_ERoadClass eClass) { m_eClass = eClass; }
	CB_ERoadClass GetRoadClass() const      { return m_eClass; }

	CB_RoadGraph&       GetGraph()       { return m_xGraph; }
	const CB_RoadGraph& GetGraph() const { return m_xGraph; }
	bool                IsDrawing() const { return m_uPendingNode != CB_RoadGraph::INVALID; }

	// Current services-category sub-type (police/fire/hospital/school), cycled by
	// re-pressing the services tool key. Power/water are their own tools.
	CB_EBuildingType GetServiceType() const { return m_eServiceType; }
	// Set the services-category sub-type directly (the UI toolbar has a button per service).
	void SetServiceType(CB_EBuildingType eType) { m_eServiceType = eType; }

	// Rebuild the ribbon triangle cache from the graph (call after edits).
	void RebuildMesh(const CB_TerrainHeightfield& xField);

private:
	CB_RoadGraph m_xGraph;

	// In-progress road state.
	uint32_t              m_uPendingNode = CB_RoadGraph::INVALID;
	Zenith_Maths::Vector2 m_xLastDir     = Zenith_Maths::Vector2(0.0f, 0.0f);
	bool                  m_bHaveLastDir = false;
	CB_ERoadClass         m_eClass       = CB_ROADCLASS_SMALL;

	// Render cache: flat ribbon triangles (3 verts each), world space.
	Zenith_Vector<Zenith_Maths::Vector3> m_xRoadTris;

	// Edge-trigger latches for the simulator's frame-held button state.
	bool m_bPrevLeft  = false;
	bool m_bPrevRight = false;

	// Services-category placement state.
	CB_EBuildingType m_eServiceType     = CB_BUILDING_POLICE;  // cycles police→fire→hospital→school
	bool             m_bWasServiceTool  = false;               // was the services tool active last frame
};
