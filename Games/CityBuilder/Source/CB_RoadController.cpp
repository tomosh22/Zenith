#include "Zenith.h"

#include "CityBuilder/Source/CB_RoadController.h"
#include "CityBuilder/Source/CB_RoadMesh.h"
#include "Input/Zenith_Input.h"
#include "Flux/Primitives/Flux_PrimitivesImpl.h"

namespace
{
	// The services tool cycles through the coverage buildings on each re-press.
	CB_EBuildingType NextServiceType(CB_EBuildingType eType)
	{
		switch (eType)
		{
		case CB_BUILDING_POLICE:       return CB_BUILDING_FIRE;
		case CB_BUILDING_FIRE:         return CB_BUILDING_HOSPITAL;
		case CB_BUILDING_HOSPITAL:     return CB_BUILDING_SCHOOL;
		case CB_BUILDING_SCHOOL:       return CB_BUILDING_LANDFILL;
		case CB_BUILDING_LANDFILL:     return CB_BUILDING_SEWAGE_PLANT;
		case CB_BUILDING_SEWAGE_PLANT: return CB_BUILDING_BUS_DEPOT;
		case CB_BUILDING_BUS_DEPOT:    return CB_BUILDING_POST_OFFICE;
		default:                       return CB_BUILDING_POLICE;
		}
	}
}

void CB_RoadController::Reset()
{
	m_xGraph.Clear();
	m_uPendingNode = CB_RoadGraph::INVALID;
	m_bHaveLastDir = false;
	m_eClass       = CB_ROADCLASS_SMALL;
	m_xRoadTris.Clear();
	m_bPrevLeft       = false;
	m_bPrevRight      = false;
	m_eServiceType    = CB_BUILDING_POLICE;
	m_bWasServiceTool = false;
}

void CB_RoadController::HandleClick(float fWorldX, float fWorldZ)
{
	const Zenith_Maths::Vector2 xP(fWorldX, fWorldZ);
	const uint32_t uNode = m_xGraph.FindOrAddNode(xP, SNAP_RADIUS);

	if (m_uPendingNode == CB_RoadGraph::INVALID)
	{
		// First click: anchor the road's start.
		m_uPendingNode = uNode;
		return;
	}
	if (uNode == m_uPendingNode)
	{
		return;  // clicked the same node — ignore
	}

	const Zenith_Maths::Vector2 xA = m_xGraph.GetNode(m_uPendingNode).m_xPos;
	const Zenith_Maths::Vector2 xB = m_xGraph.GetNode(uNode).m_xPos;
	const float fChordLen = CB_Spline::Distance(xA, xB);
	if (fChordLen < MIN_SEGMENT)
	{
		return;
	}

	const Zenith_Maths::Vector2 xChord = (xB - xA) * (1.0f / fChordLen);
	// Leave A along the previous road direction (tangent continuity), arrive at B
	// heading toward B (CB_Spline::Curved arrives in -dirB, so dirB = A->B reversed).
	const Zenith_Maths::Vector2 xDirA = m_bHaveLastDir ? m_xLastDir : xChord;
	const Zenith_Maths::Vector2 xDirB = (xA - xB) * (1.0f / fChordLen);
	const CB_Spline xSpline = CB_Spline::Curved(xA, xDirA, xB, xDirB);

	m_xGraph.AddSegment(m_uPendingNode, uNode, xSpline, m_eClass);
	m_xLastDir     = xChord;
	m_bHaveLastDir = true;
	m_uPendingNode = uNode;  // continue the road from B
}

void CB_RoadController::EndRoad()
{
	m_uPendingNode = CB_RoadGraph::INVALID;
	m_bHaveLastDir = false;
}

void CB_RoadController::BulldozeAt(float fWorldX, float fWorldZ)
{
	const uint32_t uSeg = m_xGraph.FindNearestSegment(fWorldX, fWorldZ, 12.0f);
	if (uSeg != CB_RoadGraph::INVALID)
	{
		m_xGraph.RemoveSegment(uSeg);
	}
}

void CB_RoadController::Update(const CB_ToolSystem& xTools, const CB_TerrainHeightfield& xField, CB_Zoning& xZoning, CB_BuildingPlacement& xBuild)
{
	Zenith_Input& xInput = g_xEngine.Input();
	const CB_ETool eTool = xTools.GetTool();

	const bool bLeftHeld  = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_LEFT);
	const bool bRightHeld = xInput.IsMouseButtonHeld(ZENITH_MOUSE_BUTTON_RIGHT);
	const bool bLeftClick  = bLeftHeld  && !m_bPrevLeft;   // rising edge
	const bool bRightClick = bRightHeld && !m_bPrevRight;
	m_bPrevLeft  = bLeftHeld;
	m_bPrevRight = bRightHeld;

	const uint32_t uActiveBefore = m_xGraph.GetActiveSegmentCount();

	if (eTool == CB_TOOL_ROAD)
	{
		if (bRightClick)
		{
			EndRoad();
		}
		if (bLeftClick)
		{
			float fWX = 0.0f, fWZ = 0.0f;
			if (xTools.PickGroundPoint(fWX, fWZ))
			{
				HandleClick(fWX, fWZ);
			}
		}
	}
	else if (eTool >= CB_TOOL_ZONE_RES && eTool <= CB_TOOL_ZONE_IND)
	{
		if (IsDrawing()) { EndRoad(); }
		if (bLeftHeld)   // continuous paint while dragging the zone brush
		{
			float fWX = 0.0f, fWZ = 0.0f;
			if (xTools.PickGroundPoint(fWX, fWZ))
			{
				// CB_TOOL_ZONE_* (1..4) align 1:1 with CB_ZONE_* (1..4).
				xZoning.PaintZone(fWX, fWZ, ZONE_BRUSH_RADIUS, static_cast<CB_EZoneType>(eTool), 2);
			}
		}
	}
	else if (eTool == CB_TOOL_POLICE || eTool == CB_TOOL_POWER || eTool == CB_TOOL_WATER || eTool == CB_TOOL_ZONE_PARK)
	{
		if (IsDrawing()) { EndRoad(); }
		// Re-pressing the services key cycles its sub-type (police→fire→hospital→school).
		if (eTool == CB_TOOL_POLICE && m_bWasServiceTool && xInput.WasKeyPressedThisFrame(ZENITH_KEY_6))
		{
			m_eServiceType = NextServiceType(m_eServiceType);
		}
		if (bLeftClick)
		{
			float fWX = 0.0f, fWZ = 0.0f;
			if (xTools.PickGroundPoint(fWX, fWZ))
			{
				const CB_EBuildingType eSvc = (eTool == CB_TOOL_POWER)     ? CB_BUILDING_POWER_PLANT
				                            : (eTool == CB_TOOL_WATER)     ? CB_BUILDING_WATER_TOWER
				                            : (eTool == CB_TOOL_ZONE_PARK) ? CB_BUILDING_PARK
				                            : m_eServiceType;
				xBuild.PlaceService(eSvc, fWX, fWZ, xField.GetHeightAt(fWX, fWZ));
			}
		}
	}
	else
	{
		if (IsDrawing())
		{
			EndRoad();  // left the road/zone tools — finish the in-progress road
		}
		if (eTool == CB_TOOL_BULLDOZE && bLeftClick)
		{
			float fWX = 0.0f, fWZ = 0.0f;
			if (xTools.PickGroundPoint(fWX, fWZ))
			{
				BulldozeAt(fWX, fWZ);
			}
		}
	}

	m_bWasServiceTool = (eTool == CB_TOOL_POLICE);

	if (m_xGraph.GetActiveSegmentCount() != uActiveBefore)
	{
		RebuildMesh(xField);
	}
}

void CB_RoadController::RebuildMesh(const CB_TerrainHeightfield& xField)
{
	m_xRoadTris.Clear();
	for (uint32_t i = 0; i < m_xGraph.GetSegmentSlotCount(); ++i)
	{
		const CB_RoadSegment& xSeg = m_xGraph.GetSegment(i);
		if (!xSeg.m_bActive)
		{
			continue;
		}
		CB_RoadMesh::BuildRibbon(xSeg, xField, ROAD_Y_OFFSET, m_xRoadTris);
	}
}

void CB_RoadController::Render() const
{
	const Zenith_Maths::Vector3 xAsphalt(0.17f, 0.17f, 0.19f);
	const uint32_t uTriCount = m_xRoadTris.GetSize() / 3u;
	for (uint32_t t = 0; t < uTriCount; ++t)
	{
		g_xEngine.Primitives().AddTriangle(
			m_xRoadTris.Get(t * 3u + 0u),
			m_xRoadTris.Get(t * 3u + 1u),
			m_xRoadTris.Get(t * 3u + 2u),
			xAsphalt);
	}
}
