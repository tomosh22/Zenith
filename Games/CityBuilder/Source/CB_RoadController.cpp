#include "Zenith.h"
#include "Core/Zenith_Engine.h"

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
	m_bHaveHover      = false;
	m_bHoverOverride  = false;
	m_xPreviewTris.Clear();
	m_xPreviewEdgeL.Clear();
	m_xPreviewEdgeR.Clear();
	m_bPreviewSnap        = false;
	m_bPreviewStartValid  = false;
	m_bPreviewEndValid    = false;
}

void CB_RoadController::HandleClick(float fWorldX, float fWorldZ)
{
	const Zenith_Maths::Vector2 xP(fWorldX, fWorldZ);
	// Snap to an existing node, else T-junction onto a road the click lands on, else a fresh node.
	const uint32_t uNode = m_xGraph.FindOrSplitNodeAt(xP, SNAP_RADIUS);

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

	// Auto-junction: where this road crosses existing roads, split both at the crossing + insert a
	// shared junction node, so the network is always a connected graph (SimCity / Cities: Skylines).
	m_xGraph.AddSegmentWithJunctions(m_uPendingNode, uNode, xSpline, m_eClass);
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
		// Hover the cursor onto the ground EVERY frame so the ghost preview tracks the
		// mouse (the override lets tests drive the hover without the unproject picker).
		if (!m_bHoverOverride)
		{
			float fHX = 0.0f, fHZ = 0.0f;
			m_bHaveHover = xTools.PickGroundPoint(fHX, fHZ);
			if (m_bHaveHover) { m_xHoverPos = Zenith_Maths::Vector2(fHX, fHZ); }
		}
		if (bRightClick)
		{
			EndRoad();
		}
		if (bLeftClick && m_bHaveHover)
		{
			HandleClick(m_xHoverPos.x, m_xHoverPos.y);
		}
		RebuildPreview(xField);   // ghost from the anchored start (if any) to the cursor
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

	if (eTool != CB_TOOL_ROAD)   // no ghost preview unless the road tool is active
	{
		m_bHaveHover         = false;
		m_bPreviewStartValid = false;
		m_bPreviewEndValid   = false;
		m_xPreviewTris.Clear();
		m_xPreviewEdgeL.Clear();
		m_xPreviewEdgeR.Clear();
	}

	if (m_xGraph.GetActiveSegmentCount() != uActiveBefore)
	{
		RebuildMesh(xField);
	}
}

void CB_RoadController::RebuildPreview(const CB_TerrainHeightfield& xField)
{
	m_xPreviewTris.Clear();
	m_xPreviewEdgeL.Clear();
	m_xPreviewEdgeR.Clear();
	m_bPreviewSnap       = false;
	m_bPreviewStartValid = false;
	m_bPreviewEndValid   = false;
	if (!m_bHaveHover)
	{
		return;
	}

	// Snap the hovered endpoint to a nearby existing node (preview the junction), just
	// like HandleClick's FindOrAddNode does on commit.
	Zenith_Maths::Vector2 xEnd = m_xHoverPos;
	const uint32_t uSnap = m_xGraph.FindNodeNear(m_xHoverPos, SNAP_RADIUS);
	if (uSnap != CB_RoadGraph::INVALID && uSnap != m_uPendingNode)
	{
		xEnd           = m_xGraph.GetNode(uSnap).m_xPos;
		m_bPreviewSnap = true;
	}

	// End marker: where the road would go (or start, before the first click).
	const float fGhostY = ROAD_Y_OFFSET + 0.35f;
	m_xPreviewEnd      = Zenith_Maths::Vector3(xEnd.x, xField.GetHeightAt(xEnd.x, xEnd.y) + fGhostY, xEnd.y);
	m_bPreviewEndValid = true;

	if (m_uPendingNode == CB_RoadGraph::INVALID)
	{
		return;   // not drawing yet: only the cursor/start marker, no ribbon
	}

	// Start marker = the anchored node.
	const Zenith_Maths::Vector2 xA = m_xGraph.GetNode(m_uPendingNode).m_xPos;
	m_xPreviewStart      = Zenith_Maths::Vector3(xA.x, xField.GetHeightAt(xA.x, xA.y) + fGhostY, xA.y);
	m_bPreviewStartValid = true;

	const float fLen = CB_Spline::Distance(xA, xEnd);
	if (fLen < MIN_SEGMENT)
	{
		return;   // too short to draw a ribbon — keep the two markers only
	}

	// Same spline math as HandleClick: leave A along the previous road direction
	// (tangent continuity), arrive at the cursor.
	const Zenith_Maths::Vector2 xChord = (xEnd - xA) * (1.0f / fLen);
	const Zenith_Maths::Vector2 xDirA  = m_bHaveLastDir ? m_xLastDir : xChord;
	const Zenith_Maths::Vector2 xDirB  = (xA - xEnd) * (1.0f / fLen);
	const CB_Spline xSpline = CB_Spline::Curved(xA, xDirA, xEnd, xDirB);

	CB_RoadSegment xGhostSeg;
	xGhostSeg.m_uNodeA  = m_uPendingNode;
	xGhostSeg.m_xSpline = xSpline;
	xGhostSeg.m_fWidth  = CB_RoadGraph::ClassWidth(m_eClass);
	xGhostSeg.m_eClass  = m_eClass;
	xGhostSeg.m_bActive = true;
	CB_RoadMesh::BuildRibbon(xGhostSeg, xField, fGhostY, m_xPreviewTris);

	// Footprint outline: bright edge polylines. (Lines keep their colour; the flat
	// fill washes toward white when lit, so the outline carries the "ghost" read.)
	const uint32_t uSamples = CB_RoadMesh::SampleCount(xGhostSeg);
	const float    fHalf    = xGhostSeg.m_fWidth * 0.5f;
	for (uint32_t i = 0; i <= uSamples; ++i)
	{
		const float fTt = static_cast<float>(i) / static_cast<float>(uSamples);
		const Zenith_Maths::Vector2 xCc  = xSpline.Evaluate(fTt);
		const Zenith_Maths::Vector2 xTan = xSpline.UnitTangent(fTt);
		const Zenith_Maths::Vector2 xPerp(-xTan.y, xTan.x);
		const Zenith_Maths::Vector2 xLp = xCc + xPerp * fHalf;
		const Zenith_Maths::Vector2 xRp = xCc - xPerp * fHalf;
		m_xPreviewEdgeL.PushBack(Zenith_Maths::Vector3(xLp.x, xField.GetHeightAt(xLp.x, xLp.y) + fGhostY + 0.1f, xLp.y));
		m_xPreviewEdgeR.PushBack(Zenith_Maths::Vector3(xRp.x, xField.GetHeightAt(xRp.x, xRp.y) + fGhostY + 0.1f, xRp.y));
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

	// --- Ghost preview (the player's intended road, before they confirm with a click) ---
	// Saturated colours (low R/B channels) keep their hue under the bright lit debug
	// primitives — a balanced colour like (0.4,0.95,0.45) just washes toward white.
	// Green = would drop a fresh endpoint; cyan = would snap to an existing node.
	const Zenith_Maths::Vector3 xGreen(0.05f, 0.85f, 0.12f);
	const Zenith_Maths::Vector3 xCyan (0.00f, 0.65f, 1.00f);
	const Zenith_Maths::Vector3 xGhostCol = m_bPreviewSnap ? xCyan : xGreen;

	// Filled footprint (the ghost road surface).
	const uint32_t uGhostTris = m_xPreviewTris.GetSize() / 3u;
	for (uint32_t t = 0; t < uGhostTris; ++t)
	{
		g_xEngine.Primitives().AddTriangle(
			m_xPreviewTris.Get(t * 3u + 0u),
			m_xPreviewTris.Get(t * 3u + 1u),
			m_xPreviewTris.Get(t * 3u + 2u),
			xGhostCol);
	}

	// Bright footprint outline as a row of small spheres down each edge. Flat ribbons +
	// lines wash to white under the bright lit primitives (highlight desaturation); spheres
	// keep their hue (their shadowed side isn't a highlight), so the ghost reads in colour.
	const uint32_t uEdge = m_xPreviewEdgeL.GetSize();
	for (uint32_t i = 0; i < uEdge; i += 2u)   // every ~4 world units
	{
		g_xEngine.Primitives().AddSphere(m_xPreviewEdgeL.Get(i), 1.5f, xGhostCol);
		g_xEngine.Primitives().AddSphere(m_xPreviewEdgeR.Get(i), 1.5f, xGhostCol);
	}

	// Anchored start marker (yellow — low B keeps the hue).
	if (m_bPreviewStartValid)
	{
		g_xEngine.Primitives().AddSphere(m_xPreviewStart, 2.8f, Zenith_Maths::Vector3(0.95f, 0.80f, 0.05f));
	}
	// Cursor / snap marker — a node dot + a ground ring (snap radius when over a junction).
	if (m_bPreviewEndValid)
	{
		g_xEngine.Primitives().AddSphere(m_xPreviewEnd, 2.8f, xGhostCol);
		g_xEngine.Primitives().AddCircle(m_xPreviewEnd, m_bPreviewSnap ? SNAP_RADIUS : 4.0f, xGhostCol);
	}
}
