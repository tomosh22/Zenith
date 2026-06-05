#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "CityBuilder/Source/CB_Spline.h"
#include <cstdint>

// ============================================================================
// CB_RoadGraph — a Cities: Skylines-style road network: NODES (intersections /
// endpoints, free world XZ positions) connected by SEGMENTS (each a CB_Spline
// curve with a width). No grid. Segments are soft-deleted (m_bActive) so indices
// stay stable for the mesh cache + node ref-counts; nodes are reference-counted
// and freed when their last segment goes. Pure logic — headless-testable.
// ============================================================================

class Zenith_DataStream;

enum CB_ERoadClass : uint8_t
{
	CB_ROADCLASS_SMALL  = 0,  // 2-lane local
	CB_ROADCLASS_MEDIUM = 1,  // 4-lane avenue
	CB_ROADCLASS_LARGE  = 2,  // 6-lane highway
	CB_ROADCLASS_COUNT
};

struct CB_RoadNode
{
	Zenith_Maths::Vector2 m_xPos    = Zenith_Maths::Vector2(0.0f, 0.0f);  // world XZ
	uint32_t              m_uRefs   = 0;      // active segments referencing this node
	bool                  m_bActive = false;
};

struct CB_RoadSegment
{
	uint32_t      m_uNodeA  = 0xFFFFFFFFu;
	uint32_t      m_uNodeB  = 0xFFFFFFFFu;
	CB_Spline     m_xSpline;                       // P0 == nodeA pos, P3 == nodeB pos
	float         m_fWidth  = 8.0f;                // total carriageway width (world units)
	CB_ERoadClass m_eClass  = CB_ROADCLASS_SMALL;
	bool          m_bActive = false;
};

class CB_RoadGraph
{
public:
	static constexpr uint32_t INVALID = 0xFFFFFFFFu;

	static float ClassWidth(CB_ERoadClass eClass)
	{
		switch (eClass)
		{
		case CB_ROADCLASS_SMALL:  return 8.0f;
		case CB_ROADCLASS_MEDIUM: return 12.0f;
		case CB_ROADCLASS_LARGE:  return 16.0f;
		default:                  return 8.0f;
		}
	}

	// --- nodes ---
	uint32_t AddNode(const Zenith_Maths::Vector2& xPos);
	// Nearest active node within fRadius, or INVALID.
	uint32_t FindNodeNear(const Zenith_Maths::Vector2& xPos, float fRadius) const;
	// Snap to an existing node within fRadius, else create one.
	uint32_t FindOrAddNode(const Zenith_Maths::Vector2& xPos, float fSnapRadius);

	// --- segments ---
	// Links nodeA/nodeB (ref-counts them) and stores the spline + width-by-class.
	// Returns the segment index (stable). The spline's P0/P3 should be the node
	// positions; this is not enforced.
	uint32_t AddSegment(uint32_t uNodeA, uint32_t uNodeB, const CB_Spline& xSpline, CB_ERoadClass eClass);
	// Soft-delete: deactivates the segment, decrements node refs, frees orphan nodes.
	void     RemoveSegment(uint32_t uSegment);
	// Nearest active segment whose centreline passes within fMaxDist of (wx,wz), or INVALID.
	uint32_t FindNearestSegment(float fWorldX, float fWorldZ, float fMaxDist) const;

	// --- queries ---
	uint32_t GetNodeSlotCount()    const { return m_axNodes.GetSize(); }
	uint32_t GetSegmentSlotCount() const { return m_axSegments.GetSize(); }
	uint32_t GetActiveSegmentCount() const { return m_uActiveSegments; }
	uint32_t GetActiveNodeCount()    const { return m_uActiveNodes; }

	// Total XZ arc length of all active segments — a proxy for road-network capacity.
	float    GetTotalActiveLength()  const;

	const CB_RoadNode&    GetNode(uint32_t i)    const { return m_axNodes.Get(i); }
	const CB_RoadSegment& GetSegment(uint32_t i) const { return m_axSegments.Get(i); }
	CB_RoadSegment&       GetSegmentMutable(uint32_t i) { return m_axSegments.Get(i); }

	// How many active segments touch a node (for junction rendering: >= 2 = junction).
	uint32_t CountSegmentsAtNode(uint32_t uNode) const;

	// Minimum distance (XZ) from a world point to ANY active road centreline
	// (INF if no roads). Used by terrain carving + zoning in later phases.
	float MinDistanceToAnyRoad(float fWorldX, float fWorldZ) const;

	void Clear();

	// Serialize the whole graph (nodes + segments + active counts). POD elements.
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

private:
	Zenith_Vector<CB_RoadNode>    m_axNodes;
	Zenith_Vector<CB_RoadSegment> m_axSegments;
	uint32_t m_uActiveSegments = 0;
	uint32_t m_uActiveNodes    = 0;
};
