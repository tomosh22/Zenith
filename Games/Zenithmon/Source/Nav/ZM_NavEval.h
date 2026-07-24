#pragma once

#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"

#include <cstdint>

// ============================================================================
// ZM_NavEval -- S7 item 3 SC1: a PURE, HEADLESS spike that answers one
// question -- can the engine's navmesh generator
// (Zenith_NavMeshGenerator::GenerateFromGeometry) ingest a Dawnmere-bounded
// ground surface and return a walkable navmesh?
//
// It is DELIBERATELY headless. It never constructs or deserialises a live
// Zenith_TerrainComponent (that asserts "Invalid buffer VRAM handle" with no
// GPU -- open engine gap Q-2026-07-21-001) and never touches a baked terrain
// asset on disk (those are gitignored, so absent on CI). Instead it harvests a
// synthetic FLAT coverage grid at Dawnmere's sampled ground height over the
// recipe's 1024 m export sub-rect and feeds the raw triangle soup straight to
// the leaf generator.
//
// Why a flat grid and not the real terrain surface: Dawnmere's ground height
// is produced by the engine's procedural terrain generator (driven through
// Zenith_EditorAutomation::AddStep_TerrainGenerateProcedural on a live
// Zenith_TerrainComponent -- see ZM_TerrainAuthoring.cpp). There is NO pure
// height(x,z) recipe function reachable without the terrain component / a GPU,
// so this spike takes the sanctioned fallback: a flat coverage grid at the
// TownCenter-sampled ground height over the 1024 m rect. That is sufficient to
// evaluate whether the generator accepts a Dawnmere-scale ground surface.
//
// No persistence this SC: the .znavmesh write + runtime routing are DEFERRED
// (Q-2026-07-24-002 Q-A). This is an EVALUATION spike only.
// ============================================================================

// The generator's hard voxel-grid clamp (Zenith_NavMeshGenerator::ComputeBounds
// clamps m_iWidth / m_iHeight / m_iDepth to iMaxDim = 1024). A cell size finer
// than domain/this makes the far domain collapse onto the border columns
// (RasterizeTriangle clamps out-of-range cells rather than dropping them), so
// the harvester refuses it.
constexpr u_int uZM_NAV_GENERATOR_MAX_GRID_DIM = 1024u;

// The agent-radius padding the generator adds to the bounds on every side
// (NavMeshGenerationConfig::m_fAgentRadius default = 0.4). Mirrored here so the
// harvester can predict the generator's grid dimension for its fail-closed
// guard without reaching into the engine config. The evaluator leaves the
// config's agent radius at this default, so the two stay in step.
constexpr float fZM_NAV_AGENT_RADIUS_PAD = 0.4f;

// The Dawnmere export sub-rect in world XZ + a representative ground height.
// A flat coverage grid is harvested over [m_fMinX,m_fMaxX] x [m_fMinZ,m_fMaxZ].
struct ZM_NavEvalRect
{
	float m_fMinX;
	float m_fMinZ;
	float m_fMaxX;
	float m_fMaxZ;
	float m_fGroundHeight;
};

// The Dawnmere nav rect: bounds pulled from ZM_GetDawnmereTerrainRecipe() (the
// real 1024 m export sub-rect of the engine's fixed 4096 m terrain grid) and a
// ground height sampled at the TownCenter spawn. Pure + headless.
ZM_NavEvalRect ZM_GetDawnmereNavRect();

// Outcome of building a coverage grid. m_bBuilt is false (fail-closed) when the
// requested cell size is non-finite / non-positive, or so fine that the
// generator's voxel grid would be driven past its hard iMaxDim clamp (whereupon
// the far half of the domain would collapse onto the border columns). Nothing
// is emitted into the output vectors in the fail-closed case.
struct ZM_NavEvalGrid
{
	bool  m_bBuilt;
	u_int m_uQuadsPerSide;      // soup resolution actually emitted (0 when not built)
	u_int m_uGeneratorGridDim;  // ceil((domain + 2*pad)/cellSize) the generator WOULD use
};

// Build the flat coverage soup over xRect at fCellSize. Two triangles per quad.
// bUpwardNormals=true winds every quad for an upward (walkable) normal;
// bUpwardNormals=false emits VERTICAL wall quads (horizontal normals) so the
// whole surface is non-walkable -- used to prove walkability is actually
// evaluated by the generator, not assumed.
//
// TOTAL: never asserts on its inputs. Fail-closed (returns m_bBuilt=false and
// emits nothing) if fCellSize is non-finite, <= 0, the rect is degenerate, or
// the cell size is too fine for the generator's grid clamp.
ZM_NavEvalGrid ZM_BuildCoverageGrid(
	const ZM_NavEvalRect& xRect,
	float fCellSize,
	bool bUpwardNormals,
	Zenith_Vector<Zenith_Maths::Vector3>& axVerticesOut,
	Zenith_Vector<uint32_t>& auIndicesOut);

// The evaluation verdict -- a small POD the orchestrator can quote in a report.
struct ZM_NavEvalResult
{
	bool  m_bAttempted;             // false => harvester fail-closed on fCellSize
	bool  m_bSuccess;               // generator returned a non-null navmesh
	bool  m_bWalkable;              // navmesh carries > 0 upward-facing polygons
	u_int m_uPolygonCount;          // total polygons in the returned navmesh
	u_int m_uVertexCount;           // total vertices
	u_int m_uWalkablePolygonCount;  // polygons whose normal.y clears the up-threshold
	u_int m_uQuadsPerSide;          // coverage soup resolution actually emitted
	u_int m_uGeneratorGridDim;      // voxel grid dim the generator WOULD use (pre-clamp)
	float m_fMinSafeCellSize;       // smallest cell size that stays under the clamp
};

// Run the whole spike for the Dawnmere rect at fCellSize: build the flat soup,
// call GenerateFromGeometry, read back polygon / vertex counts + per-polygon
// walkability (via the returned mesh's normals), free the navmesh, and return
// the verdict. Pure + headless. bUpwardNormals=false runs the degenerate
// all-vertical case (which must yield zero walkable polygons).
ZM_NavEvalResult ZM_EvaluateDawnmereNavGeneration(float fCellSize, bool bUpwardNormals);
