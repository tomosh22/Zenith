#include "Core/Zenith_TestFramework.h"

#ifdef ZENITH_TESTING

#include <cmath>
#include <cstring>

// ============================================================================
// Zenith_TerrainComponent ground-height query (Shortfalls E8 TIER 1)
//
// What these pin:
//   * an interior point gets the triangle's PLANE height, not the nearest
//     vertex's height;
//   * failure is the BOOL, never the value -- an XZ outside the footprint and
//     absent physics geometry both report false and leave the out-param alone,
//     while a genuine height of 0.0 reports TRUE. A caller that could not tell
//     those apart would author geometry into a hole (ZM-D-173);
//   * a point on a shared edge or vertex resolves to ONE answer, whichever
//     triangle the scan happens to reach first;
//   * malformed input (degenerate, out-of-range, truncated) is skipped, not
//     read past;
//   * (ZEN-2) THE CONSUMER of a chunk-span table -- the chunk-granularity
//     reject TryGetGroundHeightAt now runs ahead of the per-triangle scan,
//     TryGetGroundHeightFromTrianglesChunked. Four cases: it finds a span other
//     than the first, agrees with itself regardless of which of two
//     seam-adjacent spans is tried first, preserves the exact rim boundary the
//     per-triangle reject already established, and answers false over a hole
//     left by a chunk that never loaded. Each of those four HAND-WRITES its
//     span array and passes it in, so what they pin is how the reject READS a
//     table -- they say nothing whatsoever about how one is produced;
//   * (ZEN-2) and, separately, THE PRODUCER of that table --
//     CombineTerrainChunkGridCore's span recorder. One case, named
//     RecorderSkipsAHoleAndRecordsTruePostSkipOffsets, drives the combine core
//     itself over a 3x3 grid whose middle chunk deliberately fails to load, and
//     asserts the table that comes back: no entry at all for the skipped chunk,
//     every later span at the offset it ACTUALLY landed at rather than one a
//     grid coordinate would compute, and the whole table tiling the combined
//     index buffer exactly. That case is what fails if the recorder is changed
//     to arithmetic; the four consumer cases above would all stay green.
//     See the two dedicated blocks near the bottom of this file.
//
// These are PURE units: no scene, no g_xEngine, no file I/O. Geometry is built
// in the test rather than loaded, so nothing here depends on a bake.
//
// WHAT THEY CANNOT PIN, AND IT IS STRUCTURAL. Every case below builds its OWN
// triangles, so the one property that decides whether a caller may trust the
// answer -- that the shipped physics mesh is a FOUR-METRE grid while the mesh the
// player sees is a ONE-METRE grid, leaving the query a chord across the visible
// ground (see the block above TryGetGroundHeightAt) -- never enters a test here,
// and would not enter one written the same way. That gap is held by the CONTRACT
// in the header and by Zenith_TerrainChunkLayout's own static_asserts on the two
// vertex counts, not by anything in this file. Do not read a green run here as
// evidence that the number is the rendered ground.
//
// NOTE ON INCLUDES: this .inl is compiled as part of Zenith_TerrainComponent.cpp
// and deliberately does NOT include Flux/MeshGeometry/Flux_MeshGeometry.h. The
// full type arrives transitively through that .cpp's already-allow-listed Flux
// includes; a direct include here would add a new EntityComponent => Flux edge
// that the layering gate scans for. See the include-block note at the top of
// the .cpp.
// ============================================================================

namespace
{
	// A poison value no test geometry ever produces. Every failing query is
	// checked against it, so "returned false" and "left the out-param alone" are
	// asserted together -- a query that wrote 0.0 and returned false would still
	// be a trap for a caller reading the value first.
	constexpr float fTERRAIN_GROUND_QUERY_POISON = -999.0f;

	// A CPU-only Flux_MeshGeometry carrying exactly the two streams the ground
	// query reads. The streams are allocated through Zenith_MemoryManagement
	// because that is what Flux_MeshGeometry::Reset() frees them with -- mixing
	// new[] with Deallocate is documented heap corruption (Core/CLAUDE.md,
	// "Allocation Consistency"). No GPU buffer is ever created, so both VRAM
	// handles stay invalid and the destructor never reaches g_xEngine.FluxMemory().
	struct TerrainGroundQueryGeometry
	{
		Flux_MeshGeometry m_xGeometry;

		TerrainGroundQueryGeometry(const Zenith_Maths::Vector3* pxPositions, uint32_t uVertexCount,
			const uint32_t* puIndices, uint32_t uIndexCount)
		{
			const size_t ulPositionBytes = static_cast<size_t>(uVertexCount) * sizeof(Zenith_Maths::Vector3);
			const size_t ulIndexBytes = static_cast<size_t>(uIndexCount) * sizeof(Flux_MeshGeometry::IndexType);
			m_xGeometry.m_pxPositions =
				static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(ulPositionBytes));
			m_xGeometry.m_puIndices =
				static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(ulIndexBytes));

			// Allocate is malloc underneath and may return null. memcpy into null is
			// undefined behaviour that would take the WHOLE suite down instead of
			// failing one case, so the null is reported as an assertion and the copy
			// is skipped. The counts stay 0 on that path, which leaves the fixture in
			// the "geometry with no CPU streams" shape the query already handles --
			// the test then fails on its own expectations rather than crashing.
			ZENITH_ASSERT_NOT_NULL(m_xGeometry.m_pxPositions,
				"test fixture could not allocate its position stream");
			ZENITH_ASSERT_NOT_NULL(m_xGeometry.m_puIndices,
				"test fixture could not allocate its index stream");
			if (m_xGeometry.m_pxPositions == nullptr || m_xGeometry.m_puIndices == nullptr)
				return;

			std::memcpy(m_xGeometry.m_pxPositions, pxPositions, ulPositionBytes);
			std::memcpy(m_xGeometry.m_puIndices, puIndices, ulIndexBytes);
			m_xGeometry.m_uNumVerts = uVertexCount;
			m_xGeometry.m_uNumIndices = uIndexCount;
		}
	};
}

// One steep triangle with three well-separated vertex heights. The probe sits at
// barycentric (0.5, 0.25, 0.25), which is world XZ (1, 1) and plane height 7 --
// a value equal to NO vertex, so a nearest-vertex implementation (which would
// answer 10, vertex A being closest) cannot pass this.
ZENITH_TEST(TerrainGroundQuery, InteriorPointInterpolatesThePlane)
{
	const Zenith_Maths::Vector3 axPositions[3] =
	{
		Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f),
		Zenith_Maths::Vector3(4.0f,  2.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  6.0f, 4.0f)
	};
	const uint32_t auIndices[3] = { 0u, 1u, 2u };

	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auIndices, 3u, 1.0f, 1.0f, fHeight),
		"a point inside the triangle must resolve");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 7.0f, 1.0e-4f, "barycentric plane height at (1, 1)");
	ZENITH_ASSERT_GT(std::fabs(fHeight - 10.0f), 1.0f,
		"the NEAREST vertex is A at height 10 -- answering it would be the bug this pins");

	// The centroid is the other value only a real interpolation produces: the
	// mean of the three vertex heights.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auIndices, 3u, 4.0f / 3.0f, 4.0f / 3.0f, fHeight),
		"the centroid is inside");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 6.0f, 1.0e-4f, "(10 + 2 + 6) / 3");

	// Winding must not matter. The terrain exporter emits clockwise XZ triangles;
	// this reversed copy is the same surface and must give the same answer.
	const uint32_t auReversedIndices[3] = { 0u, 2u, 1u };
	float fReversedHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auReversedIndices, 3u, 1.0f, 1.0f, fReversedHeight),
		"the lookup is winding-agnostic");
	ZENITH_ASSERT_EQ_FLOAT(fReversedHeight, 7.0f, 1.0e-4f, "reversed winding, same plane");
}

// Failure is the RETURN VALUE, never the height. The second half of this test is
// the one that matters: a triangle lying flat at y = 0 answers TRUE with 0.0, so
// 0.0 can never be read as "no answer".
ZENITH_TEST(TerrainGroundQuery, OutsideTheFootprintFailsWithoutAnsweringZero)
{
	const Zenith_Maths::Vector3 axPositions[3] =
	{
		Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f),
		Zenith_Maths::Vector3(4.0f,  2.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  6.0f, 4.0f)
	};
	const uint32_t auIndices[3] = { 0u, 1u, 2u };

	// Well outside the XZ bounding box.
	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auIndices, 3u, 5.0f, 5.0f, fHeight),
		"outside the footprint has no answer");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f,
		"a failed query must leave the out-param untouched, NOT write 0.0");

	// Inside the bounding box but beyond the hypotenuse -- the case a naive
	// box-only test would wrongly answer.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auIndices, 3u, 3.0f, 3.0f, fHeight),
		"inside the AABB is not inside the triangle");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f,
		"still untouched");

	// ...and 0.0 IS a legal height. This is the whole reason the query reports
	// through a bool: sea level is a real answer.
	const Zenith_Maths::Vector3 axFlatPositions[3] =
	{
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(4.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 4.0f)
	};
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axFlatPositions, 3u, auIndices, 3u, 1.0f, 1.0f, fHeight),
		"a flat triangle at y = 0 still has an answer");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 0.0f, 0.0f, "0.0 is a HEIGHT here, reported with true");
}

// The HasPhysicsGeometry() case the component header warns about: physics
// geometry is absent whenever every chunk's source mesh failed to load, and the
// query is reachable from code that has not checked. It must not dereference.
ZENITH_TEST(TerrainGroundQuery, AbsentPhysicsGeometryFailsAndDoesNotCrash)
{
	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		nullptr, 0.0f, 0.0f, fHeight),
		"a null geometry is the absent-physics-geometry case");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f,
		"absent geometry must not be reported as height 0.0");

	// A geometry that EXISTS but retained no CPU streams (GPU-only, or reset)
	// takes the same path and must answer the same way.
	Flux_MeshGeometry xEmptyGeometry;
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		&xEmptyGeometry, 0.0f, 0.0f, fHeight),
		"a geometry with no position/index streams has nothing to look up");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "still untouched");

	// The raw-array entry point rejects the same shapes.
	const uint32_t auIndices[3] = { 0u, 1u, 2u };
	const Zenith_Maths::Vector3 xPosition(0.0f, 0.0f, 0.0f);
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		nullptr, 0u, auIndices, 3u, 0.0f, 0.0f, fHeight), "no positions");
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		&xPosition, 1u, nullptr, 0u, 0.0f, 0.0f, fHeight), "no indices");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "still untouched");
}

// Two triangles meeting along the v0-v2 diagonal of one quad. A point ON that
// diagonal is contained by BOTH, so the answer would depend on iteration order
// if the two disagreed. They cannot: along the shared edge each interpolates the
// same two shared vertices. Pinned by running the SAME quad with the triangle
// pair listed in both orders.
ZENITH_TEST(TerrainGroundQuery, SharedEdgeAndVertexAreTriangleOrderIndependent)
{
	const Zenith_Maths::Vector3 axPositions[4] =
	{
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),   // v0
		Zenith_Maths::Vector3(2.0f, 4.0f, 0.0f),   // v1
		Zenith_Maths::Vector3(2.0f, 6.0f, 2.0f),   // v2
		Zenith_Maths::Vector3(0.0f, 1.0f, 2.0f)    // v3
	};
	const uint32_t auOrderA[6] = { 0u, 1u, 2u, 0u, 2u, 3u };
	const uint32_t auOrderB[6] = { 0u, 2u, 3u, 0u, 1u, 2u };

	// Midpoint of the shared diagonal: halfway between v0 (y = 0) and v2 (y = 6).
	float fHeightA = fTERRAIN_GROUND_QUERY_POISON;
	float fHeightB = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 4u, auOrderA, 6u, 1.0f, 1.0f, fHeightA), "on the edge is INSIDE");
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 4u, auOrderB, 6u, 1.0f, 1.0f, fHeightB), "on the edge is INSIDE");
	ZENITH_ASSERT_EQ_FLOAT(fHeightA, 3.0f, 1.0e-4f, "midpoint of the v0-v2 edge");
	ZENITH_ASSERT_EQ_FLOAT(fHeightA, fHeightB, 1.0e-6f,
		"listing the two triangles in the other order must not move the answer");

	// A shared VERTEX: v2 belongs to both triangles.
	fHeightA = fTERRAIN_GROUND_QUERY_POISON;
	fHeightB = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 4u, auOrderA, 6u, 2.0f, 2.0f, fHeightA), "a shared vertex resolves");
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 4u, auOrderB, 6u, 2.0f, 2.0f, fHeightB), "a shared vertex resolves");
	ZENITH_ASSERT_EQ_FLOAT(fHeightA, 6.0f, 1.0e-4f, "v2's own height");
	ZENITH_ASSERT_EQ_FLOAT(fHeightA, fHeightB, 1.0e-6f, "order-independent at a vertex too");

	// v0 sits at height 0 and is also shared -- another proof that 0.0 arrives
	// with a true, not as a failure sentinel.
	fHeightA = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 4u, auOrderA, 6u, 0.0f, 0.0f, fHeightA), "the quad corner resolves");
	ZENITH_ASSERT_EQ_FLOAT(fHeightA, 0.0f, 1.0e-4f, "v0's own height, reported with true");
}

// The geometry-level entry point, over a 3x3 vertex grid (four quads, eight
// triangles) with deliberately asymmetric heights so no two triangles share a
// centroid height -- picking the wrong triangle produces a wrong number rather
// than the right one by luck. This is what pins that the vertex COUNT and index
// COUNT are read from the right accessors.
ZENITH_TEST(TerrainGroundQuery, PhysicsMeshGeometryStreamsFeedTheQuery)
{
	const Zenith_Maths::Vector3 axPositions[9] =
	{
		Zenith_Maths::Vector3(0.0f,  0.0f, 0.0f),  // 0
		Zenith_Maths::Vector3(1.0f,  2.0f, 0.0f),  // 1
		Zenith_Maths::Vector3(2.0f,  6.0f, 0.0f),  // 2
		Zenith_Maths::Vector3(0.0f,  1.0f, 1.0f),  // 3
		Zenith_Maths::Vector3(1.0f,  4.0f, 1.0f),  // 4
		Zenith_Maths::Vector3(2.0f,  9.0f, 1.0f),  // 5
		Zenith_Maths::Vector3(0.0f,  3.0f, 2.0f),  // 6
		Zenith_Maths::Vector3(1.0f, 12.0f, 2.0f),  // 7
		Zenith_Maths::Vector3(2.0f, 21.0f, 2.0f)   // 8
	};
	// Each quad split along its (x, z) -> (x+1, z+1) diagonal.
	const uint32_t auIndices[24] =
	{
		0u, 1u, 4u,  0u, 4u, 3u,
		1u, 2u, 5u,  1u, 5u, 4u,
		3u, 4u, 7u,  3u, 7u, 6u,
		4u, 5u, 8u,  4u, 8u, 7u
	};

	TerrainGroundQueryGeometry xFixture(axPositions, 9u, auIndices, 24u);

	// Centroid of the FIRST triangle: heights 0, 2, 4.
	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		&xFixture.m_xGeometry, 2.0f / 3.0f, 1.0f / 3.0f, fHeight), "inside quad (0,0)");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 2.0f, 1.0e-4f, "(0 + 2 + 4) / 3");

	// Centroid of the second triangle of the same quad: heights 0, 4, 1. Its
	// answer differs from the first, so the two are distinguishable.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		&xFixture.m_xGeometry, 1.0f / 3.0f, 2.0f / 3.0f, fHeight), "the other half of quad (0,0)");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 5.0f / 3.0f, 1.0e-4f, "(0 + 4 + 1) / 3");

	// The LAST quad, so a truncated scan cannot pass: heights 4, 9, 21.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		&xFixture.m_xGeometry, 5.0f / 3.0f, 4.0f / 3.0f, fHeight), "inside quad (1,1)");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 34.0f / 3.0f, 1.0e-4f, "(4 + 9 + 21) / 3");

	// An interior grid vertex reports its own height.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		&xFixture.m_xGeometry, 1.0f, 1.0f, fHeight), "the centre vertex resolves");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 4.0f, 1.0e-4f, "vertex 4's own height");

	// ...and off the edge of the grid there is still no answer.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromGeometry(
		&xFixture.m_xGeometry, 2.5f, 0.5f, fHeight), "beyond the grid's +X edge");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f,
		"outside a real mesh is still not height 0.0");
}

// Robustness against the shapes a damaged or partially-loaded physics mesh can
// present. None of these may read past the end of a stream, and none may answer.
ZENITH_TEST(TerrainGroundQuery, MalformedTrianglesAreSkippedNotRead)
{
	// Collinear in XZ (all three on the line x == z): zero projected area, so
	// there is no height to report for an XZ column even though the point is
	// inside the bounding box.
	const Zenith_Maths::Vector3 axDegenerate[3] =
	{
		Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(2.0f, 5.0f, 2.0f),
		Zenith_Maths::Vector3(4.0f, 3.0f, 4.0f)
	};
	const uint32_t auIndices[3] = { 0u, 1u, 2u };
	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axDegenerate, 3u, auIndices, 3u, 2.0f, 2.0f, fHeight),
		"a triangle with zero XZ area is not a ground surface");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched");

	const Zenith_Maths::Vector3 axPositions[3] =
	{
		Zenith_Maths::Vector3(0.0f, 10.0f, 0.0f),
		Zenith_Maths::Vector3(4.0f,  2.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  6.0f, 4.0f)
	};

	// An index past the end of the position stream is skipped, never fetched.
	const uint32_t auOutOfRange[3] = { 0u, 1u, 99u };
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auOutOfRange, 3u, 1.0f, 1.0f, fHeight),
		"an out-of-range index drops its triangle");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched");

	// A trailing PARTIAL triple is dropped: five indices are one triangle, not
	// one and two-thirds. The complete triangle still answers.
	const uint32_t auTruncated[5] = { 0u, 1u, 2u, 0u, 1u };
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auTruncated, 5u, 1.0f, 1.0f, fHeight),
		"the one complete triangle is still used");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 7.0f, 1.0e-4f, "same plane height as the whole-triple case");

	// Fewer than three indices is not a triangle at all.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 3u, auTruncated, 2u, 1.0f, 1.0f, fHeight),
		"two indices describe nothing");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched");
}

// ============================================================================
// Chunk-granularity acceleration (ZEN-2): TryGetGroundHeightFromTrianglesChunked
//
// These pin the NEW coarse per-chunk XZ reject that TryGetGroundHeightAt now
// runs ahead of the per-triangle scan pinned above, which stays UNCHANGED --
// every case below cross-checks the chunked entry point against
// TryGetGroundHeightFromTriangles over the identical arrays, so a divergence
// between them fails here rather than only showing up against a live bake.
//
// Each fixture is TWO physics-chunk-shaped 3x3 grids side by side in world
// space (world X in [0, 2] and [2, 4], both spanning world Z in [0, 2]) --
// small enough to hand-build and hand-verify, but with a REAL shared seam and
// a REAL second (non-zero-indexed) span, the minimum shape able to
// distinguish "narrows to the right chunk" from "always scans span 0" or
// "never narrows at all". Each chunk reuses the 3x3/two-triangles-per-cell
// pattern from PhysicsMeshGeometryStreamsFeedTheQuery above, offset by 9 for
// the second chunk.
//
// Heights follow h(x, z) = 10x + z + 1 everywhere, so a vertex SHARED between
// the two chunks (the world X = 2 column) gets the IDENTICAL height in both
// chunks' own position streams purely because it is a function of world
// position -- exactly how the real exporter's stitched edge is bit-identical
// across a chunk boundary (Flux/Terrain/CLAUDE.md, "every chunk closes on its
// neighbour"), so the fixtures below do not have to hand-duplicate values to
// make a seam agree; it agrees by construction.
// ============================================================================

// A probe strictly inside the SECOND span (world X in (2, 4)), away from
// either span's own boundary. A lookup that only ever tried span[0], or that
// stopped once span[0] rejected instead of continuing to span[1], cannot pass
// this.
ZENITH_TEST(TerrainGroundQuery, ChunkedLookupFindsANonZeroIndexedSpan)
{
	const Zenith_Maths::Vector3 axPositions[18] =
	{
		Zenith_Maths::Vector3(0.0f,  1.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 11.0f, 0.0f), Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  2.0f, 1.0f), Zenith_Maths::Vector3(1.0f, 12.0f, 1.0f), Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f,  3.0f, 2.0f), Zenith_Maths::Vector3(1.0f, 13.0f, 2.0f), Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f),
		// Chunk B, world X in [2, 4] -- its own X=2 column (indices 9, 12, 15)
		// matches chunk A's X=2 column (indices 2, 5, 8) exactly.
		Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f), Zenith_Maths::Vector3(3.0f, 31.0f, 0.0f), Zenith_Maths::Vector3(4.0f, 41.0f, 0.0f),
		Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f), Zenith_Maths::Vector3(3.0f, 32.0f, 1.0f), Zenith_Maths::Vector3(4.0f, 42.0f, 1.0f),
		Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f), Zenith_Maths::Vector3(3.0f, 33.0f, 2.0f), Zenith_Maths::Vector3(4.0f, 43.0f, 2.0f)
	};
	const uint32_t auIndices[48] =
	{
		0u,1u,4u, 0u,4u,3u, 1u,2u,5u, 1u,5u,4u, 3u,4u,7u, 3u,7u,6u, 4u,5u,8u, 4u,8u,7u,                          // chunk A, [0, 24)
		9u,10u,13u, 9u,13u,12u, 10u,11u,14u, 10u,14u,13u, 12u,13u,16u, 12u,16u,15u, 13u,14u,17u, 13u,17u,16u     // chunk B, [24, 48)
	};
	const Zenith_TerrainComponent::PhysicsChunkSpan axSpans[2] =
	{
		{ 0.0f, 2.0f, 0.0f, 2.0f, 0u, 24u },   // chunk A -- span[0]
		{ 2.0f, 4.0f, 0.0f, 2.0f, 24u, 24u }   // chunk B -- span[1], not span[0]
	};

	// Centroid of chunk B's first triangle (verts 9, 10, 13 -- world (2, 0),
	// (3, 0), (3, 1), heights 21, 31, 32): X = 8/3 is strictly inside (2, 4),
	// so only span[1] can contain it.
	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 8.0f / 3.0f, 1.0f / 3.0f, fHeight),
		"a point strictly inside the second span must resolve");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 28.0f, 1.0e-4f, "(21 + 31 + 32) / 3");

	// The unrestricted, unaccelerated core over the SAME arrays must agree --
	// the chunk reject changes only which triangles get tried, never the maths.
	float fUnrestrictedHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 18u, auIndices, 48u, 8.0f / 3.0f, 1.0f / 3.0f, fUnrestrictedHeight),
		"the unrestricted scan finds the same triangle");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fUnrestrictedHeight, 1.0e-6f,
		"chunked and unrestricted must agree exactly");

	// A point outside every span's bounds still correctly fails.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 10.0f, 10.0f, fHeight),
		"outside every span's bounds has no answer");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched");
}

// A probe exactly on the boundary line between two chunks -- both spans' real
// vertex-data bounds contain it (the bounds are inclusive and touch exactly at
// world X = 2), so the answer must not depend on which span the scan reaches
// first. Pinned both ways, exactly like SharedEdgeAndVertexAreTriangleOrderIndependent
// above but one level up, at the SPAN level rather than the triangle level.
ZENITH_TEST(TerrainGroundQuery, ChunkedLookupAgreesAcrossASeamRegardlessOfSpanOrder)
{
	const Zenith_Maths::Vector3 axPositions[18] =
	{
		Zenith_Maths::Vector3(0.0f,  1.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 11.0f, 0.0f), Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  2.0f, 1.0f), Zenith_Maths::Vector3(1.0f, 12.0f, 1.0f), Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f,  3.0f, 2.0f), Zenith_Maths::Vector3(1.0f, 13.0f, 2.0f), Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f),
		Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f), Zenith_Maths::Vector3(3.0f, 31.0f, 0.0f), Zenith_Maths::Vector3(4.0f, 41.0f, 0.0f),
		Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f), Zenith_Maths::Vector3(3.0f, 32.0f, 1.0f), Zenith_Maths::Vector3(4.0f, 42.0f, 1.0f),
		Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f), Zenith_Maths::Vector3(3.0f, 33.0f, 2.0f), Zenith_Maths::Vector3(4.0f, 43.0f, 2.0f)
	};
	const uint32_t auIndices[48] =
	{
		0u,1u,4u, 0u,4u,3u, 1u,2u,5u, 1u,5u,4u, 3u,4u,7u, 3u,7u,6u, 4u,5u,8u, 4u,8u,7u,
		9u,10u,13u, 9u,13u,12u, 10u,11u,14u, 10u,14u,13u, 12u,13u,16u, 12u,16u,15u, 13u,14u,17u, 13u,17u,16u
	};
	const Zenith_TerrainComponent::PhysicsChunkSpan axSpansAThenB[2] =
	{
		{ 0.0f, 2.0f, 0.0f, 2.0f, 0u, 24u },
		{ 2.0f, 4.0f, 0.0f, 2.0f, 24u, 24u }
	};
	const Zenith_TerrainComponent::PhysicsChunkSpan axSpansBThenA[2] =
	{
		{ 2.0f, 4.0f, 0.0f, 2.0f, 24u, 24u },
		{ 0.0f, 2.0f, 0.0f, 2.0f, 0u, 24u }
	};

	// The shared VERTEX at world (2, 1): chunk A's own vertex 5 and chunk B's
	// own vertex 12 -- height 22 in both streams, by construction (see the
	// fixture-block comment above).
	float fHeightAB = fTERRAIN_GROUND_QUERY_POISON;
	float fHeightBA = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpansAThenB, 2u, 2.0f, 1.0f, fHeightAB),
		"a shared vertex is on the boundary of both spans");
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpansBThenA, 2u, 2.0f, 1.0f, fHeightBA),
		"...regardless of which span is tried first");
	ZENITH_ASSERT_EQ_FLOAT(fHeightAB, 22.0f, 1.0e-4f, "the shared vertex's own height");
	ZENITH_ASSERT_EQ_FLOAT(fHeightAB, fHeightBA, 1.0e-6f, "span order must not move the answer");

	// The MIDPOINT of the shared edge, world (2, 0.5) -- not a grid vertex, so
	// this is a genuine barycentric interpolation on each side of the seam, not
	// a nearest-vertex coincidence: halfway between h(2,0)=21 and h(2,1)=22.
	fHeightAB = fTERRAIN_GROUND_QUERY_POISON;
	fHeightBA = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpansAThenB, 2u, 2.0f, 0.5f, fHeightAB),
		"the edge midpoint is on the boundary of both spans");
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpansBThenA, 2u, 2.0f, 0.5f, fHeightBA),
		"...regardless of order");
	ZENITH_ASSERT_EQ_FLOAT(fHeightAB, 21.5f, 1.0e-4f, "midpoint of the h=21 / h=22 edge");
	ZENITH_ASSERT_EQ_FLOAT(fHeightAB, fHeightBA, 1.0e-6f, "still order-independent");
}

// The mesh RIM: a probe exactly at the far edge of the last chunk's own real
// bounds must still resolve (inclusive), and a probe just beyond it must not --
// pinning that the new coarse per-chunk reject does not move the boundary the
// existing per-triangle reject already established. Cross-checked against the
// unrestricted scan at both points so this is a genuine "unchanged", not
// merely "the chunked path is internally consistent".
ZENITH_TEST(TerrainGroundQuery, ChunkedLookupPreservesRimBehaviour)
{
	const Zenith_Maths::Vector3 axPositions[18] =
	{
		Zenith_Maths::Vector3(0.0f,  1.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 11.0f, 0.0f), Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  2.0f, 1.0f), Zenith_Maths::Vector3(1.0f, 12.0f, 1.0f), Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f,  3.0f, 2.0f), Zenith_Maths::Vector3(1.0f, 13.0f, 2.0f), Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f),
		Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f), Zenith_Maths::Vector3(3.0f, 31.0f, 0.0f), Zenith_Maths::Vector3(4.0f, 41.0f, 0.0f),
		Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f), Zenith_Maths::Vector3(3.0f, 32.0f, 1.0f), Zenith_Maths::Vector3(4.0f, 42.0f, 1.0f),
		Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f), Zenith_Maths::Vector3(3.0f, 33.0f, 2.0f), Zenith_Maths::Vector3(4.0f, 43.0f, 2.0f)
	};
	const uint32_t auIndices[48] =
	{
		0u,1u,4u, 0u,4u,3u, 1u,2u,5u, 1u,5u,4u, 3u,4u,7u, 3u,7u,6u, 4u,5u,8u, 4u,8u,7u,
		9u,10u,13u, 9u,13u,12u, 10u,11u,14u, 10u,14u,13u, 12u,13u,16u, 12u,16u,15u, 13u,14u,17u, 13u,17u,16u
	};
	const Zenith_TerrainComponent::PhysicsChunkSpan axSpans[2] =
	{
		{ 0.0f, 2.0f, 0.0f, 2.0f, 0u, 24u },
		{ 2.0f, 4.0f, 0.0f, 2.0f, 24u, 24u }
	};

	// world (4, 1) is chunk B's own vertex 14 -- its maxX, the mesh's rim.
	float fChunkedHeight = fTERRAIN_GROUND_QUERY_POISON;
	float fUnrestrictedHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 4.0f, 1.0f, fChunkedHeight),
		"exactly at the rim is INSIDE");
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 18u, auIndices, 48u, 4.0f, 1.0f, fUnrestrictedHeight),
		"...and the unrestricted scan agrees");
	ZENITH_ASSERT_EQ_FLOAT(fChunkedHeight, 42.0f, 1.0e-4f, "vertex 14's own height");
	ZENITH_ASSERT_EQ_FLOAT(fChunkedHeight, fUnrestrictedHeight, 1.0e-6f, "identical to the unrestricted answer");

	// Just beyond the rim: no span's real bounds reach X = 4.5, so this must
	// fail exactly as the unrestricted per-triangle box already would.
	fChunkedHeight = fTERRAIN_GROUND_QUERY_POISON;
	fUnrestrictedHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 4.5f, 1.0f, fChunkedHeight),
		"beyond every span's bounds is beyond the rim");
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 18u, auIndices, 48u, 4.5f, 1.0f, fUnrestrictedHeight),
		"...and the unrestricted scan agrees it has no answer either");
	ZENITH_ASSERT_EQ_FLOAT(fChunkedHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched");
	ZENITH_ASSERT_EQ_FLOAT(fUnrestrictedHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched, same as the chunked path");
}

// A SPARSE bake, from the CONSUMER's side. The middle chunk of a row of three
// never loaded, so the two surviving chunks sit back-to-back in the combined
// mesh with NOTHING reserved for the missing one, and the second span's
// firstIndex is 24 rather than 48.
//
// READ WHAT THIS DOES AND DOES NOT PIN. The span table below is HAND-WRITTEN:
// the 24 is supplied as INPUT, not asserted as the recorder's output. So this
// case pins that TryGetGroundHeightFromTrianglesChunked handles a
// sparse-shaped table correctly -- false over the hole, and the post-hole span
// still resolving against its own triangles -- and it would stay green if
// CombineTerrainChunkGridCore computed its offsets from a grid coordinate.
// The PRODUCER side is pinned separately, by
// RecorderSkipsAHoleAndRecordsTruePostSkipOffsets in the next block, which
// drives the combine core and asserts the offsets it actually writes.
ZENITH_TEST(TerrainGroundQuery, ChunkedLookupReturnsFalseOverASparseHole)
{
	const Zenith_Maths::Vector3 axPositions[18] =
	{
		// Chunk A: world X in [0, 2].
		Zenith_Maths::Vector3(0.0f,  1.0f, 0.0f), Zenith_Maths::Vector3(1.0f, 11.0f, 0.0f), Zenith_Maths::Vector3(2.0f, 21.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f,  2.0f, 1.0f), Zenith_Maths::Vector3(1.0f, 12.0f, 1.0f), Zenith_Maths::Vector3(2.0f, 22.0f, 1.0f),
		Zenith_Maths::Vector3(0.0f,  3.0f, 2.0f), Zenith_Maths::Vector3(1.0f, 13.0f, 2.0f), Zenith_Maths::Vector3(2.0f, 23.0f, 2.0f),
		// Chunk C: world X in [4, 6]. Chunk B (world X in [2, 4]) never loaded
		// and contributes NOTHING -- not even a reserved slot.
		Zenith_Maths::Vector3(4.0f, 41.0f, 0.0f), Zenith_Maths::Vector3(5.0f, 51.0f, 0.0f), Zenith_Maths::Vector3(6.0f, 61.0f, 0.0f),
		Zenith_Maths::Vector3(4.0f, 42.0f, 1.0f), Zenith_Maths::Vector3(5.0f, 52.0f, 1.0f), Zenith_Maths::Vector3(6.0f, 62.0f, 1.0f),
		Zenith_Maths::Vector3(4.0f, 43.0f, 2.0f), Zenith_Maths::Vector3(5.0f, 53.0f, 2.0f), Zenith_Maths::Vector3(6.0f, 63.0f, 2.0f)
	};
	const uint32_t auIndices[48] =
	{
		0u,1u,4u, 0u,4u,3u, 1u,2u,5u, 1u,5u,4u, 3u,4u,7u, 3u,7u,6u, 4u,5u,8u, 4u,8u,7u,                          // chunk A, [0, 24)
		9u,10u,13u, 9u,13u,12u, 10u,11u,14u, 10u,14u,13u, 12u,13u,16u, 12u,16u,15u, 13u,14u,17u, 13u,17u,16u     // chunk C, [24, 48) -- NOT [48, 72)
	};
	const Zenith_TerrainComponent::PhysicsChunkSpan axSpans[2] =
	{
		{ 0.0f, 2.0f, 0.0f, 2.0f, 0u, 24u },    // chunk A
		{ 4.0f, 6.0f, 0.0f, 2.0f, 24u, 24u }    // chunk C, at its TRUE post-skip offset
	};

	// The hole itself: world X in (2, 4) belongs to neither span.
	float fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 3.0f, 1.0f, fHeight),
		"a probe over the missing chunk has no answer");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f, "untouched");
	// The unrestricted scan over the SAME (already-sparse) arrays agrees --
	// there genuinely is no triangle there, not merely one the chunk reject hid.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_FALSE(Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
		axPositions, 18u, auIndices, 48u, 3.0f, 1.0f, fHeight),
		"...because no triangle covers it at all");

	// Both surviving chunks still resolve correctly around the hole.
	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 1.0f, 1.0f, fHeight),
		"chunk A, before the hole, is unaffected");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 12.0f, 1.0e-4f, "vertex 4's own height");

	fHeight = fTERRAIN_GROUND_QUERY_POISON;
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
		axPositions, 18u, auIndices, 48u, axSpans, 2u, 5.0f, 1.0f, fHeight),
		"chunk C, after the hole, resolves at its TRUE (post-skip) offset");
	ZENITH_ASSERT_EQ_FLOAT(fHeight, 52.0f, 1.0e-4f, "vertex 13's own height");
}

// ============================================================================
// The chunk-span RECORDER (ZEN-2): CombineTerrainChunkGridCore's span table
//
// Everything above this line hands a HAND-WRITTEN span array to the query. That
// leaves the thing this ticket added -- the recorder inside the combine core --
// with no coverage at all: swap its RecordChunkSpan(xChunkGeometry,
// uPreviousIndexCount) for any grid-coordinate arithmetic and every one of
// those cases stays green, because none of them ever calls the combine.
//
// This block drives the PRODUCER. It builds a 3x3 chunk grid in memory, fails
// the load of exactly one MIDDLE chunk, and asserts the table
// CombineTerrainChunkGridCore writes.
//
// WHY THIS TEST FAILS IF THE RECORDER IS CHANGED TO ARITHMETIC, concretely.
// Chunks are appended in the core's own (uX, uY) order with (0,0) seeded first,
// so with (1,1) missing the eight surviving chunks land at index offsets
// 0/6/12/18/24/30/36/42 -- the append ordinal times six. A grid-coordinate
// mapping ((uX * 3 + uY) * 6) agrees for every chunk BEFORE the hole and is
// wrong by exactly six for every chunk after it:
//
//   append  chunk    TRUE offset    arithmetic offset
//   4       (1,2)    24             30   -> scans chunk (2,0)'s triangles
//   5       (2,0)    30             36   -> scans chunk (2,1)'s triangles
//   6       (2,1)    36             42   -> scans chunk (2,2)'s triangles
//   7       (2,2)    42             48   == GetNumIndices(), so the malformed-
//                                          span guard in the chunked query
//                                          SILENTLY continues past it
//
// Three assertions below each independently catch that. (1) The offsets are
// asserted against the running sum of the preceding spans' index counts, so an
// arithmetic offset breaks the tiling at span 4. (2) Span 4's offset is
// asserted EQUAL to 24 and NOT EQUAL to the arithmetic 30, computed in the test
// from the same grid coordinate a wrong implementation would use. (3) The
// end-to-end probe loop queries through the RECORDED table and requires the
// same answer as the unrestricted scan for all nine cells -- under arithmetic
// the four post-hole chunks answer FALSE over ground the unrestricted scan
// finds, which is precisely the production symptom (a wrong ground height, or
// "no ground" over ground that exists).
//
// This is a PURE unit like the rest of the file: the load callback synthesises
// geometry in memory, so there is no bake, no file I/O, no scene and no
// g_xEngine. It reaches CombineTerrainChunkGridCore (private) through the
// friend declaration on Zenith_TerrainComponent.
// ============================================================================
struct Zenith_TerrainChunkSpanRecorderTests
{
	// A 3x3 grid of single-quad chunks. Three is the smallest grid size with a
	// cell that is genuinely in the MIDDLE of the append order -- with 2x2 the
	// only skippable cells are the last two, where an arithmetic offset is still
	// right for everything recorded before it and the bug hides.
	static constexpr uint32_t uGRID_SIZE = 3u;
	static constexpr uint32_t uCELL_COUNT = uGRID_SIZE * uGRID_SIZE;
	static constexpr uint32_t uVERTS_PER_CHUNK = 4u;
	static constexpr uint32_t uINDICES_PER_CHUNK = 6u;
	static constexpr float fCHUNK_SIZE = 2.0f;
	// (1,1) is the 5th of nine in the core's (uX, uY) append order, so four
	// chunks are recorded before the hole and four after it.
	static constexpr uint32_t uHOLE_X = 1u;
	static constexpr uint32_t uHOLE_Y = 1u;
	// Append ordinal of chunk (1,2), the first chunk appended AFTER the hole.
	static constexpr uint32_t uFIRST_POST_HOLE_SPAN = 4u;

	struct LoadContext
	{
		uint32_t m_uFailX;
		uint32_t m_uFailY;
	};

	// A single global plane, so every chunk's corner heights and every
	// neighbour's copy of a shared edge agree by construction -- the same
	// property the real exporter's stitched chunk edges have. Because it is
	// planar, a barycentric interpolation anywhere inside a chunk returns this
	// function exactly, which is what lets a probe assert an expected height
	// rather than merely "some number".
	static float HeightAt(float fX, float fZ) { return 10.0f * fX + fZ + 1.0f; }

	static bool BuildChunk(void* pContext, uint32_t uX, uint32_t uY, Flux_MeshGeometry& xGeometryOut);
	static void Run();
};

// The TerrainChunkLoadCallback the combine core calls once per grid cell. One
// quad per chunk, split along its (minX,minZ)->(maxX,maxZ) diagonal, occupying
// world X in [uX*2, uX*2+2] and world Z in [uY*2, uY*2+2].
bool Zenith_TerrainChunkSpanRecorderTests::BuildChunk(void* pContext, uint32_t uX, uint32_t uY,
	Flux_MeshGeometry& xGeometryOut)
{
	const LoadContext& xContext = *static_cast<const LoadContext*>(pContext);
	// The whole point of the fixture. Returning false here is exactly what a
	// missing or rejected chunk .zmesh does to the real loader, and it is the
	// branch that makes the combined mesh sparse. xGeometryOut is left untouched.
	if (uX == xContext.m_uFailX && uY == xContext.m_uFailY)
		return false;

	const float fMinX = static_cast<float>(uX) * fCHUNK_SIZE;
	const float fMinZ = static_cast<float>(uY) * fCHUNK_SIZE;
	const float fMaxX = fMinX + fCHUNK_SIZE;
	const float fMaxZ = fMinZ + fCHUNK_SIZE;
	const Zenith_Maths::Vector3 axPositions[uVERTS_PER_CHUNK] =
	{
		Zenith_Maths::Vector3(fMinX, HeightAt(fMinX, fMinZ), fMinZ),   // 0
		Zenith_Maths::Vector3(fMaxX, HeightAt(fMaxX, fMinZ), fMinZ),   // 1
		Zenith_Maths::Vector3(fMinX, HeightAt(fMinX, fMaxZ), fMaxZ),   // 2
		Zenith_Maths::Vector3(fMaxX, HeightAt(fMaxX, fMaxZ), fMaxZ)    // 3
	};
	const Flux_MeshGeometry::IndexType auIndices[uINDICES_PER_CHUNK] = { 0u, 1u, 3u, 0u, 3u, 2u };

	// Zenith_MemoryManagement::Allocate, NOT new[]: Flux_MeshGeometry::Reset()
	// frees these with Deallocate, and the combine core Reallocate()s them.
	// Mixing the two strategies on one pointer is documented heap corruption
	// (Core/CLAUDE.md, "Allocation Consistency").
	const size_t ulPositionBytes = sizeof(axPositions);
	const size_t ulIndexBytes = sizeof(auIndices);
	Zenith_Maths::Vector3* pxPositions =
		static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(ulPositionBytes));
	Flux_MeshGeometry::IndexType* puIndices =
		static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(ulIndexBytes));
	if (pxPositions == nullptr || puIndices == nullptr)
	{
		// Allocate is malloc underneath and may return null. Report the same way
		// the loader does -- as a failed load -- rather than memcpy-ing into null
		// and taking the whole suite down; the test then fails on its own
		// expectations below.
		if (pxPositions != nullptr) Zenith_MemoryManagement::Deallocate(pxPositions);
		if (puIndices != nullptr) Zenith_MemoryManagement::Deallocate(puIndices);
		return false;
	}
	std::memcpy(pxPositions, axPositions, ulPositionBytes);
	std::memcpy(puIndices, auIndices, ulIndexBytes);

	xGeometryOut.m_pxPositions = pxPositions;
	xGeometryOut.m_puIndices = puIndices;
	xGeometryOut.m_uNumVerts = uVERTS_PER_CHUNK;
	xGeometryOut.m_uNumIndices = uINDICES_PER_CHUNK;
	// A DECLARED LAYOUT IS NOT OPTIONAL HERE, even though nothing in this test
	// draws. The combine core sizes its one up-front reallocation as
	// uTotalVerts * GetBufferLayout().GetStride(), and Flux_MeshGeometry::Combine
	// both asserts the two layouts match and memcpys GetVertexDataSize() bytes
	// per chunk. A layout-less chunk would give a ZERO-byte reallocation, which
	// realloc answers with null, and the combine would bail before recording
	// anything. GenerateLayoutAndVertexData is the shared derived-float builder
	// every non-.zmesh producer uses; with only a position stream present it
	// declares one FLOAT3 element (stride 12) and the interleaved bytes are a
	// verbatim copy of the positions.
	xGeometryOut.GenerateLayoutAndVertexData();
	xGeometryOut.m_ulReservedVertexDataSize = xGeometryOut.GetVertexDataSize();
	xGeometryOut.m_ulReservedIndexDataSize = ulIndexBytes;
	xGeometryOut.m_ulReservedPositionDataSize = ulPositionBytes;
	return xGeometryOut.m_pVertexData != nullptr;
}

ZENITH_TEST(TerrainChunkSpans, RecorderSkipsAHoleAndRecordsTruePostSkipOffsets)
{
	Zenith_TerrainChunkSpanRecorderTests::Run();
}

void Zenith_TerrainChunkSpanRecorderTests::Run()
{
	LoadContext xContext{ uHOLE_X, uHOLE_Y };
	Zenith_TerrainComponent::TerrainSparseLoadDiagnostics xDiagnostics;
	Flux_MeshGeometry* pxCombined = nullptr;
	Zenith_TerrainComponent::PhysicsChunkSpan axSpans[uCELL_COUNT] = {};
	// Deliberately NOT zero: the core documents that it RESETS the count, and a
	// recorder that only ever incremented would leave this poison in place.
	uint32_t uSpanCount = 0xFFFFFFFFu;

	// uTotalVerts / uTotalIndices are the DENSE upper bound, exactly as
	// LoadCombinedPhysicsGeometryCore computes them -- the core reserves for a
	// full grid and the sparse result simply uses less of it.
	ZENITH_ASSERT_TRUE(Zenith_TerrainComponent::CombineTerrainChunkGridCore(
		uGRID_SIZE, uVERTS_PER_CHUNK * uCELL_COUNT, uINDICES_PER_CHUNK * uCELL_COUNT,
		&Zenith_TerrainChunkSpanRecorderTests::BuildChunk, &xContext, nullptr,
		pxCombined, xDiagnostics, axSpans, uCELL_COUNT, &uSpanCount),
		"a grid whose (0,0) anchor loads must still combine with one middle chunk missing");
	ZENITH_ASSERT_NOT_NULL(pxCombined, "a successful sparse combine must return owned geometry");
	if (pxCombined == nullptr)
		return;   // everything below dereferences it

	ZENITH_ASSERT_TRUE(xDiagnostics.m_bAnchorLoaded, "the (0,0) anchor loaded");
	ZENITH_ASSERT_EQ(xDiagnostics.m_uSkippedCount, 1u, "exactly one chunk refused to load");
	ZENITH_ASSERT_EQ(xDiagnostics.m_auSampleX[0], uHOLE_X, "the skipped coordinate's X");
	ZENITH_ASSERT_EQ(xDiagnostics.m_auSampleY[0], uHOLE_Y, "the skipped coordinate's Y");

	// (1) NO PLACEHOLDER FOR THE HOLE. Eight cells loaded, so there are eight
	// spans -- not nine with one blank, and not nine with one zero-length. A
	// reserved slot is the shape that makes an arithmetic offset look correct.
	ZENITH_ASSERT_EQ(uSpanCount, uCELL_COUNT - 1u,
		"the skipped chunk must contribute NO span at all, not a placeholder");
	// A failed assertion RECORDS a failure and lets the test carry on, so a wrong
	// count here would index this fixed-size array out of bounds below and take
	// the whole suite down instead of failing one case. Report and stop.
	if (uSpanCount != uCELL_COUNT - 1u)
	{
		delete pxCombined;
		return;
	}
	ZENITH_ASSERT_EQ(pxCombined->GetNumIndices(), uINDICES_PER_CHUNK * (uCELL_COUNT - 1u),
		"the combined index buffer holds only the surviving chunks' triangles");
	ZENITH_ASSERT_EQ(pxCombined->GetNumVerts(), uVERTS_PER_CHUNK * (uCELL_COUNT - 1u),
		"...and only their vertices");

	// (2) THE TABLE TILES [0, GetNumIndices()) EXACTLY. Each span begins where
	// the previous one ended -- strictly increasing, no gap, no overlap, nothing
	// left over. An arithmetic offset breaks this at the first post-hole span.
	uint32_t uRunningFirstIndex = 0u;
	for (uint32_t uSpan = 0u; uSpan < uSpanCount; ++uSpan)
	{
		const Zenith_TerrainComponent::PhysicsChunkSpan& xSpan = axSpans[uSpan];
		ZENITH_ASSERT_EQ(xSpan.m_uFirstIndex, uRunningFirstIndex,
			"span %u must begin exactly where the previous span ended (the recorded "
			"offset is the one the chunk ACTUALLY landed at)", uSpan);
		ZENITH_ASSERT_EQ(xSpan.m_uIndexCount, uINDICES_PER_CHUNK,
			"span %u must cover exactly its own chunk's triangles", uSpan);
		uRunningFirstIndex += xSpan.m_uIndexCount;
	}
	ZENITH_ASSERT_EQ(uRunningFirstIndex, pxCombined->GetNumIndices(),
		"the spans must account for EVERY index in the combined buffer");

	// (3) THE ANCHOR, which is seeded before the append loop and recorded from
	// the combined geometry rather than from a separate chunk geometry -- a
	// different code path from every other span, and the one a recorder written
	// only for the loop body would drop.
	ZENITH_ASSERT_EQ(axSpans[0].m_uFirstIndex, 0u, "the anchor chunk (0,0) starts at index 0");
	ZENITH_ASSERT_EQ_FLOAT(axSpans[0].m_fMinX, 0.0f, 1.0e-4f, "chunk (0,0)'s own real-vertex-data minX");
	ZENITH_ASSERT_EQ_FLOAT(axSpans[0].m_fMaxX, fCHUNK_SIZE, 1.0e-4f, "...its maxX");
	ZENITH_ASSERT_EQ_FLOAT(axSpans[0].m_fMinZ, 0.0f, 1.0e-4f, "...its minZ");
	ZENITH_ASSERT_EQ_FLOAT(axSpans[0].m_fMaxZ, fCHUNK_SIZE, 1.0e-4f, "...its maxZ");

	// (4) THE DECISIVE PAIR. Span 4 is chunk (1,2), the first chunk appended
	// after the hole. Its TRUE offset is its append ordinal times six; the
	// arithmetic offset is computed here from the same grid coordinate a wrong
	// implementation would use. They differ by exactly one chunk's worth of
	// indices, which is the skip.
	const uint32_t uTrueFirstIndex = uFIRST_POST_HOLE_SPAN * uINDICES_PER_CHUNK;
	const uint32_t uArithmeticFirstIndex = (1u * uGRID_SIZE + 2u) * uINDICES_PER_CHUNK;
	ZENITH_ASSERT_EQ(axSpans[uFIRST_POST_HOLE_SPAN].m_uFirstIndex, uTrueFirstIndex,
		"the span after the hole must carry its TRUE post-skip offset");
	ZENITH_ASSERT_NE(axSpans[uFIRST_POST_HOLE_SPAN].m_uFirstIndex, uArithmeticFirstIndex,
		"...which is NOT what (uX * gridSize + uY) * indicesPerChunk computes -- this "
		"assertion is the one that fails if the recorder is changed to arithmetic");
	// ...and it really is chunk (1,2)'s own footprint, not a neighbour's.
	ZENITH_ASSERT_EQ_FLOAT(axSpans[uFIRST_POST_HOLE_SPAN].m_fMinX, 1.0f * fCHUNK_SIZE, 1.0e-4f, "chunk (1,2) minX");
	ZENITH_ASSERT_EQ_FLOAT(axSpans[uFIRST_POST_HOLE_SPAN].m_fMinZ, 2.0f * fCHUNK_SIZE, 1.0e-4f, "chunk (1,2) minZ");

	// The LAST chunk is the case that would fail SILENTLY rather than loudly:
	// its arithmetic offset is exactly GetNumIndices(), which
	// TryGetGroundHeightFromTrianglesChunked's malformed-span guard skips with a
	// bare `continue` -- no assert, no log, just "no ground" over solid ground.
	const uint32_t uLastArithmeticFirstIndex = (2u * uGRID_SIZE + 2u) * uINDICES_PER_CHUNK;
	ZENITH_ASSERT_EQ(uLastArithmeticFirstIndex, pxCombined->GetNumIndices(),
		"fixture check: the arithmetic offset for the last chunk really is off the end of the buffer");
	ZENITH_ASSERT_LT(axSpans[uSpanCount - 1u].m_uFirstIndex, pxCombined->GetNumIndices(),
		"the RECORDED offset for the last chunk is inside the buffer");

	// (5) END TO END, through the recorded table. One probe strictly inside each
	// of the nine cells (off the quad diagonal so it lies in exactly one
	// triangle, and away from every chunk boundary so exactly one span's bounds
	// contain it). The chunked answer must match the unrestricted scan over the
	// same combined arrays -- value AND bool -- for all nine.
	for (uint32_t uX = 0u; uX < uGRID_SIZE; ++uX)
	{
		for (uint32_t uY = 0u; uY < uGRID_SIZE; ++uY)
		{
			const float fProbeX = static_cast<float>(uX) * fCHUNK_SIZE + 1.5f;
			const float fProbeZ = static_cast<float>(uY) * fCHUNK_SIZE + 0.5f;

			float fChunkedHeight = fTERRAIN_GROUND_QUERY_POISON;
			const bool bChunked = Zenith_TerrainComponent::TryGetGroundHeightFromTrianglesChunked(
				pxCombined->m_pxPositions, pxCombined->GetNumVerts(),
				pxCombined->m_puIndices, pxCombined->GetNumIndices(),
				axSpans, uSpanCount, fProbeX, fProbeZ, fChunkedHeight);

			float fUnrestrictedHeight = fTERRAIN_GROUND_QUERY_POISON;
			const bool bUnrestricted = Zenith_TerrainComponent::TryGetGroundHeightFromTriangles(
				pxCombined->m_pxPositions, pxCombined->GetNumVerts(),
				pxCombined->m_puIndices, pxCombined->GetNumIndices(),
				fProbeX, fProbeZ, fUnrestrictedHeight);

			ZENITH_ASSERT_EQ(bChunked, bUnrestricted,
				"chunk (%u,%u): the RECORDED span table must never change WHETHER the "
				"scan finds ground", uX, uY);

			if (uX == uHOLE_X && uY == uHOLE_Y)
			{
				ZENITH_ASSERT_FALSE(bChunked,
					"chunk (%u,%u) never loaded, so no triangle covers its XZ", uX, uY);
				ZENITH_ASSERT_EQ_FLOAT(fChunkedHeight, fTERRAIN_GROUND_QUERY_POISON, 0.0f,
					"a failed query leaves the out-param alone");
				continue;
			}

			ZENITH_ASSERT_TRUE(bChunked,
				"chunk (%u,%u) loaded, so the offset the RECORDER wrote for it must reach "
				"its own triangles", uX, uY);
			ZENITH_ASSERT_EQ_FLOAT(fChunkedHeight, HeightAt(fProbeX, fProbeZ), 1.0e-2f,
				"chunk (%u,%u): the recorded span must reach ITS OWN triangles, not a "
				"neighbour's", uX, uY);
			ZENITH_ASSERT_EQ_FLOAT(fChunkedHeight, fUnrestrictedHeight, 1.0e-6f,
				"chunk (%u,%u): chunked and unrestricted must agree exactly", uX, uY);
		}
	}

	delete pxCombined;
}

// ============================================================================
// CleanupPriorGenerationForRegenerate frees the physics-geometry table (ZEN-4
// fix-forward). The reviewer verified the ZEN-2 fix by hand-tracing every
// discard/transfer site and found no defect -- what it flagged instead was
// that the fixed line has ZERO execution coverage: the one existing
// caller-side test (Zenith_UnitTests.Tests.inl's preflight-rejection block,
// ~line 15598) explicitly asserts the regenerate operation is REJECTED before
// cleanup ever runs, so it pins the opposite of what would need to be true for
// it to notice a use-after-free or double-free here. This block is that
// coverage: it drives CleanupPriorGenerationForRegenerate() directly.
//
// UNLIKE EVERY TEST ABOVE THIS LINE, THIS ONE IS NOT A PURE UNIT, and that is
// a fact about the function under test, not a choice made here.
// CleanupPriorGenerationForRegenerate() unconditionally calls
// g_xEngine.TerrainStreaming().UnregisterTerrainBuffers(...) and
// g_xEngine.FluxMemory().Destroy{Vertex,Index}Buffer(...) -- there is no way
// to reach it without a live g_xEngine. It also lives in a ZENITH_TOOLS-only
// TU (Zenith_TerrainComponent_Editor.cpp), so both the friend declaration in
// the header and this entire block are ZENITH_TOOLS-gated: omitting the guard
// would not merely compile this test out of a ToolsEnabled=False
// configuration, it would break that configuration's BUILD outright, for
// every game, because CleanupPriorGenerationForRegenerate would not exist as
// a member to call.
//
// WHY THIS IS SAFE TO CALL HERE, traced rather than assumed, because a crash
// here would take the whole suite down with it, not just this one case:
//   * g_xEngine IS fully initialised by the time this runs. The boot
//     ZENITH_TEST batch executes from inside Zenith_Engine::InitialiseProject,
//     which is the LAST step of Zenith_Engine::Initialise -- every Flux
//     subsystem, including TerrainStreaming and FluxMemory, was allocated
//     several steps earlier (AllocateFluxSubsystems) and is live by the time
//     any ZENITH_TEST body runs;
//   * DestroyCullingResources() early-returns on
//     m_pxStreamingState->m_bCullingResourcesInitialized, which the
//     never-rendered component built below leaves false (InitializeCullingResources
//     is only ever called from InitializeRenderResources, which this test does
//     not invoke);
//   * TerrainStreaming().UnregisterTerrainBuffers is a registry search-and-remove
//     that is a documented no-op when the state was never registered (or was
//     already removed) -- RegisterTerrainBuffers is, like culling init, never
//     called by the constructor this test uses;
//   * FluxMemory().Destroy{Vertex,Index}Buffer both early-return on
//     Flux_VRAMHandle::IsValid() (Vulkan/Zenith_Vulkan_MemoryManager_Buffers.cpp,
//     DestroySimpleBuffer) -- this component's unified buffers were never
//     created (InitializeUnifiedBuffers is likewise never invoked) -- and are
//     unconditional no-ops on the Null backend the pinned gate actually runs
//     (Zenith_Null_MemoryManager.h), regardless of handle state;
//   * consequently, the SAME sequence runs again, harmlessly, when the test's
//     local xTerrain destructs at the end of Run() below (its destructor
//     repeats DestroyCullingResources / UnregisterTerrainBuffers /
//     Destroy{Vertex,Index}Buffer unconditionally) -- m_pxPhysicsGeometry is
//     already null by then, so the destructor's own `delete` and
//     FreePhysicsChunkSpans() calls are no-ops too.
// So the only thing CleanupPriorGenerationForRegenerate meaningfully touches
// in this fixture is exactly the physics-geometry pair this test exists to
// pin.
//
// The component itself uses the lighter of its two constructors --
// Zenith_TerrainComponent(Zenith_Entity&), the "deserialization" one -- built
// over a DEFAULT-CONSTRUCTED (invalid, scene-less) Zenith_Entity. Its body
// heap-allocates m_pxStreamingState and reads a compile-time asset-directory
// fallback (m_strTerrainAssetSet starts empty, which TryResolveTerrainAssetDirectory
// treats as the legacy default rather than an error); it never dereferences
// the entity, never touches a file, and never touches g_xEngine. That is the
// narrowest way to get a real `this` for the private instance methods below.
// ============================================================================
#ifdef ZENITH_TOOLS
struct Zenith_TerrainCleanupPhysicsGeometryTests
{
	// One always-succeeding 4-vertex/6-index quad "chunk", reused as the sole
	// cell of a uGridSize=1 grid in Run() below: CombineTerrainChunkGridCore's
	// grid loop only ever considers (0,0) (the anchor, loaded before the loop
	// starts; a 1x1 grid's only loop iteration IS (0,0), and the loop's own
	// `if (uX==0 && uY==0) continue;` skips it), so this fixture round-trips
	// through Zenith_MemoryManagement::Allocate for both the combined geometry
	// and the span table without needing the sparse/multi-chunk machinery
	// Zenith_TerrainChunkSpanRecorderTests::BuildChunk above exercises -- this
	// test's whole point is the FREE, not the combine, so the fixture is kept
	// as small as one that still genuinely allocates both.
	static bool BuildChunk(void* pContext, uint32_t uX, uint32_t uY, Flux_MeshGeometry& xGeometryOut);
	static void Run();
};

bool Zenith_TerrainCleanupPhysicsGeometryTests::BuildChunk(void* pContext, uint32_t uX, uint32_t uY,
	Flux_MeshGeometry& xGeometryOut)
{
	(void)pContext; (void)uX; (void)uY;
	constexpr uint32_t uVERTS = 4u;
	constexpr uint32_t uINDICES = 6u;
	const Zenith_Maths::Vector3 axPositions[uVERTS] =
	{
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		Zenith_Maths::Vector3(2.0f, 2.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 3.0f, 2.0f),
		Zenith_Maths::Vector3(2.0f, 4.0f, 2.0f)
	};
	const Flux_MeshGeometry::IndexType auIndices[uINDICES] = { 0u, 1u, 3u, 0u, 3u, 2u };

	// Zenith_MemoryManagement::Allocate, NOT new[]: Flux_MeshGeometry::Reset()
	// frees these with Deallocate, and CombineTerrainChunkGridCore Reallocate()s
	// them. Mixing new[] with either is documented heap corruption
	// (Core/CLAUDE.md, "Allocation Consistency") -- same discipline as every
	// other in-memory fixture in this file.
	const size_t ulPositionBytes = sizeof(axPositions);
	const size_t ulIndexBytes = sizeof(auIndices);
	Zenith_Maths::Vector3* pxPositions =
		static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(ulPositionBytes));
	Flux_MeshGeometry::IndexType* puIndices =
		static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(ulIndexBytes));
	if (pxPositions == nullptr || puIndices == nullptr)
	{
		// Allocate is malloc underneath and may return null. Report the same way
		// the real loader does -- as a failed load -- rather than memcpy-ing into
		// null and taking the whole suite down; the test then fails on its own
		// expectations in Run() below.
		ZENITH_ASSERT_NOT_NULL(pxPositions, "test fixture could not allocate its position stream");
		ZENITH_ASSERT_NOT_NULL(puIndices, "test fixture could not allocate its index stream");
		if (pxPositions != nullptr) Zenith_MemoryManagement::Deallocate(pxPositions);
		if (puIndices != nullptr) Zenith_MemoryManagement::Deallocate(puIndices);
		return false;
	}
	std::memcpy(pxPositions, axPositions, ulPositionBytes);
	std::memcpy(puIndices, auIndices, ulIndexBytes);

	xGeometryOut.m_pxPositions = pxPositions;
	xGeometryOut.m_puIndices = puIndices;
	xGeometryOut.m_uNumVerts = uVERTS;
	xGeometryOut.m_uNumIndices = uINDICES;
	// Required even though nothing here draws -- CombineTerrainChunkGridCore
	// sizes its up-front Reallocate as uTotalVerts * GetBufferLayout().GetStride(),
	// and a layout-less chunk gives a ZERO-byte reallocation (realloc answers
	// that with null, and the combine bails before recording anything). See
	// Zenith_TerrainChunkSpanRecorderTests::BuildChunk above for the fuller
	// explanation -- this call is load-bearing, not decorative.
	xGeometryOut.GenerateLayoutAndVertexData();
	xGeometryOut.m_ulReservedVertexDataSize = xGeometryOut.GetVertexDataSize();
	xGeometryOut.m_ulReservedIndexDataSize = ulIndexBytes;
	xGeometryOut.m_ulReservedPositionDataSize = ulPositionBytes;
	return xGeometryOut.m_pVertexData != nullptr;
}

void Zenith_TerrainCleanupPhysicsGeometryTests::Run()
{
	// Default-constructed: no scene, invalid ID, never dereferenced by the
	// constructor below (traced in the block comment above) -- this is what
	// keeps CONSTRUCTION scene-free even though the function under test is not
	// g_xEngine-free.
	Zenith_Entity xInertEntity;
	Zenith_TerrainComponent xTerrain(xInertEntity);

	Zenith_TerrainComponent::TerrainSparseLoadDiagnostics xDiagnostics;
	const bool bCombined = xTerrain.LoadCombinedPhysicsGeometryCore(1u,
		&Zenith_TerrainCleanupPhysicsGeometryTests::BuildChunk, nullptr, xDiagnostics);
	ZENITH_ASSERT_TRUE(bCombined, "a single always-succeeding chunk must combine");

	// THE ASSERTION THAT MATTERS MOST. Without proving the fixture genuinely
	// populated both members here, the after-cleanup null checks below could
	// pass VACUOUSLY against a fixture that silently allocated nothing -- the
	// exact gap a hand-assigned pointer (rather than this real
	// LoadCombinedPhysicsGeometryCore call) would have been unable to rule out.
	ZENITH_ASSERT_NOT_NULL(xTerrain.m_pxPhysicsGeometry,
		"fixture must populate physics geometry before cleanup runs");
	ZENITH_ASSERT_NOT_NULL(xTerrain.m_pxPhysicsChunkSpans,
		"fixture must populate the chunk-span table before cleanup runs");
	ZENITH_ASSERT_GT(xTerrain.m_uPhysicsChunkSpanCount, 0u,
		"the populated span table must carry at least the anchor chunk's own span");

	xTerrain.CleanupPriorGenerationForRegenerate();

	ZENITH_ASSERT_NULL(xTerrain.m_pxPhysicsGeometry,
		"cleanup must free and null the physics geometry");
	ZENITH_ASSERT_NULL(xTerrain.m_pxPhysicsChunkSpans,
		"cleanup must free and null the chunk-span table in the same call");
	ZENITH_ASSERT_EQ(xTerrain.m_uPhysicsChunkSpanCount, 0u,
		"...and reset the count alongside it");

	// xTerrain now destructs normally, which is deliberately left to happen
	// rather than short-circuited: see the block comment above for why running
	// the destructor's own cleanup sequence a second time here is safe.
}

ZENITH_TEST(TerrainPhysicsGeometryCleanup, CleanupPriorGenerationForRegenerateFreesGeometryAndSpans)
{
	Zenith_TerrainCleanupPhysicsGeometryTests::Run();
}
#endif // ZENITH_TOOLS

#endif // ZENITH_TESTING
