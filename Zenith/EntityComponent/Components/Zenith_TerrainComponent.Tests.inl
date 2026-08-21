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
//     read past.
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

#endif // ZENITH_TESTING
