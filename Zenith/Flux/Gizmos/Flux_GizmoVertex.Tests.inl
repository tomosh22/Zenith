#include "UnitTests/Zenith_UnitTests.h"

#include <cstring>   // std::memcpy — reading the packed lane back out of the raw bytes

// ============================================================================
// Flux_GizmoVertex packed-lane contract (compressed-vertex Phase 6).
//
// TOOLS-ONLY, like the whole Gizmos feature — this file is included from inside
// Flux_Gizmos.cpp's ZENITH_TOOLS block, so these registrations exist only in a
// tools build (the same arrangement Flux_MaterialPreviewController.Tests.inl has).
// Every pinned unit baseline is measured on a *_True configuration.
//
// Gizmos has no instance stream: both lanes are per-vertex on binding 0. The
// colour is unorm8x4 STORAGE behind a float3 shader field, so the fetch supplies a
// fourth byte the declared float3 discards — which is why the packer's alpha is
// inert and the decode drops it.
// ============================================================================

ZENITH_TEST(GizmoVertex, SetColourWritesThePackedWordAtTheGeneratedOffset)
{
	Flux_GizmoVertex xVertex{};
	xVertex.m_xPosition = Zenith_Maths::Vector3(0.25f, -1.5f, 8.0f);
	xVertex.SetColour(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));

	u_int uWord = 0u;
	std::memcpy(&uWord,
		reinterpret_cast<const unsigned char*>(&xVertex)
			+ Flux_Generated_Gizmos::Gizmos::kaxVertexAttribs[1].m_uOffset,
		sizeof(uWord));
	ZENITH_ASSERT_EQ(uWord, Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f)),
		"the colour lane must hold the unorm8x4 word at the generated COLOR offset");

	// ...and the position lane, three raw floats, must be undisturbed beside it.
	float fPositionZ = 0.0f;
	std::memcpy(&fPositionZ,
		reinterpret_cast<const unsigned char*>(&xVertex)
			+ Flux_Generated_Gizmos::Gizmos::kaxVertexAttribs[0].m_uOffset + 8u,
		sizeof(fPositionZ));
	ZENITH_ASSERT_EQ_FLOAT(fPositionZ, 8.0f, 0.0f, "the position lane must stay full float");
}

ZENITH_TEST(GizmoVertex, AxisHandleColoursRoundTripExactly)
{
	// The authored handle tints are the three unit axis colours. Every channel is a
	// 0 or a 1, both of which unorm8 represents exactly, so these are identity
	// round-trips rather than tolerance comparisons.
	const Zenith_Maths::Vector3 axAxisColours[3] =
	{
		Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f),
		Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f),
	};
	for (u_int u = 0u; u < 3u; ++u)
	{
		Flux_GizmoVertex xVertex{};
		xVertex.SetColour(axAxisColours[u]);
		const Zenith_Maths::Vector3 xOut = xVertex.GetColour();
		ZENITH_ASSERT_EQ_FLOAT(xOut.x, axAxisColours[u].x, 0.0f, "axis colour %u red", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.y, axAxisColours[u].y, 0.0f, "axis colour %u green", u);
		ZENITH_ASSERT_EQ_FLOAT(xOut.z, axAxisColours[u].z, 0.0f, "axis colour %u blue", u);
	}
}

ZENITH_TEST(GizmoVertex, InterleaveWritesPackedColoursForEveryVertex)
{
	// The one production writer. It is the only path gizmo geometry reaches the GPU
	// through, so this covers the packing where it actually happens rather than only
	// at the setter.
	Zenith_Vector<Zenith_Maths::Vector3> xPositions;
	Zenith_Vector<Zenith_Maths::Vector3> xColours;
	xPositions.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f));
	xPositions.PushBack(Zenith_Maths::Vector3(1.0f, 2.0f, 3.0f));
	xColours.PushBack(Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f));
	xColours.PushBack(Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));

	Flux_GizmosImpl xGizmos;
	Zenith_Vector<Flux_GizmoVertex> xOut;
	xGizmos.InterleaveVertexData(xOut, xPositions, xColours);

	ZENITH_ASSERT_EQ(xOut.GetSize(), 2u, "one vertex per supplied position");
	ZENITH_ASSERT_EQ(xOut.Get(0).m_uColour, Flux_PackUnorm8x4(Zenith_Maths::Vector4(1.0f, 0.0f, 0.0f, 1.0f)),
		"vertex 0 must carry the packed red handle tint");
	ZENITH_ASSERT_EQ(xOut.Get(1).m_uColour, Flux_PackUnorm8x4(Zenith_Maths::Vector4(0.0f, 0.0f, 1.0f, 1.0f)),
		"vertex 1 must carry the packed blue handle tint");
	ZENITH_ASSERT_EQ_FLOAT(xOut.Get(1).m_xPosition.y, 2.0f, 0.0f, "positions pass through untouched");
}
