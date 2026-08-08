#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux_MeshGeometry.h"
#include "Flux/MeshGeometry/Flux_VertexPacker.h"   // Flux_PackVertices — the one interleaver
// The three programs that FETCH the unit quad this file generates. Included for the
// baked layouts the contract below compares — nothing here calls into them.
#include "Flux/Shaders/Generated/Quads.h"
#include "Flux/Shaders/Generated/Text.h"
#include "Flux/Shaders/Generated/Particles.h"

// ============================================================================
// THE shared unit-quad contract.
//
// GenerateFullscreenQuad below produces the ONE 20-byte vertex buffer that Quads,
// Text and Particles all bind at binding 0 (Flux_GraphicsImpl::m_xQuadMesh — see the
// `SetVertexBuffer(xGraphics.m_xQuadMesh.GetVertexBuffer(), 0)` in each of the three
// record callbacks). Each of those programs describes that one buffer with its OWN
// generated layout, and each pins only its own table, beside its own per-instance
// struct (Flux_QuadsImpl.h / Flux_TextImpl.h / Flux_ParticleData.h). Three
// separately-correct consumer pins can still describe three DIFFERENT buffers, and
// no consumer is positioned to notice — the shared thing is the PRODUCER, so the
// contract is pinned at the producer.
//
// Because all three are compared against the SAME table, the comparison is
// transitive: it states both "each program fetches pos3@0 + uv2@12, stride 20" and
// "the three agree with one another". Taking the route the plan offered as the
// alternative — giving each consumer its own quad VB — would have made the three
// layouts independent instead of merely unpinned, at the cost of three buffers.
// ============================================================================
static constexpr u_int uFLUX_UNIT_QUAD_ELEMENT_COUNT = 2u;
static constexpr u_int uFLUX_UNIT_QUAD_STRIDE        = 20u;
static constexpr Flux_VertexLayoutElement kaxFLUX_UNIT_QUAD_BINDING0[uFLUX_UNIT_QUAD_ELEMENT_COUNT] =
{
	{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, SHADER_DATA_TYPE_FLOAT3, 0u,  0u },
	{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SHADER_DATA_TYPE_FLOAT2, 0u, 12u },
};

// True iff xLayout's binding-0 PREFIX is the shared unit quad. The prefix only: each
// consumer carries its own per-instance elements on binding 1 after these two, and
// those are that consumer's business (and already pinned there, full-table).
static constexpr bool Flux_FetchesSharedUnitQuad(const Flux_VertexLayoutDesc& xLayout)
{
	if (xLayout.m_uElementCount < uFLUX_UNIT_QUAD_ELEMENT_COUNT) return false;
	if (xLayout.m_auStrides[0] != uFLUX_UNIT_QUAD_STRIDE) return false;
	for (u_int u = 0; u < uFLUX_UNIT_QUAD_ELEMENT_COUNT; u++)
	{
		if (!(xLayout.m_paxElements[u] == kaxFLUX_UNIT_QUAD_BINDING0[u])) return false;
	}
	return true;
}

static_assert(Flux_FetchesSharedUnitQuad(Flux_Generated_Quads::Quads::kVertexLayout),
	"The Quads program stopped fetching the shared unit quad GenerateFullscreenQuad writes — give it its own vertex buffer, or put the VsIn prefix back");
static_assert(Flux_FetchesSharedUnitQuad(Flux_Generated_Text::Text::kVertexLayout),
	"The Text program stopped fetching the shared unit quad GenerateFullscreenQuad writes — give it its own vertex buffer, or put the VsIn prefix back");
static_assert(Flux_FetchesSharedUnitQuad(Flux_Generated_Particles::Particles::kVertexLayout),
	"The Particles program stopped fetching the shared unit quad GenerateFullscreenQuad writes — give it its own vertex buffer, or put the VsIn prefix back");

// ...and the PRODUCER half: that the generator really writes those offsets.
// GenerateFullscreenQuad fills exactly m_pxPositions + m_pxUVs, and
// GenerateLayoutAndVertexData declares its rows POSITION-then-TEXCOORD and
// tight-packs them, so the two source streams' element types ARE the offsets and the
// stride. Spelled in sizeof rather than read back out of Flux_BufferLayout because
// Flux_ShaderDataTypeSize is not constexpr — the layout's own answer only exists at
// run time, and the T3.b interleave goldens are what compare it byte-for-byte.
static_assert(sizeof(Zenith_Maths::Vector3) == kaxFLUX_UNIT_QUAD_BINDING0[1].m_uOffset,
	"The unit quad's UV no longer starts where a tight float3 position ends");
static_assert(sizeof(Zenith_Maths::Vector3) + sizeof(Zenith_Maths::Vector2) == uFLUX_UNIT_QUAD_STRIDE,
	"The unit quad's position+UV no longer tight-pack to the stride the three programs fetch");

void Flux_MeshGeometry::GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut)
{
	xGeometryOut.m_uNumVerts = 4;
	xGeometryOut.m_uNumIndices = 6;
	xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(xGeometryOut.m_uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(xGeometryOut.m_uNumVerts * sizeof(Zenith_Maths::Vector2)));

	xGeometryOut.m_puIndices = static_cast<IndexType*>(Zenith_MemoryManagement::Allocate(xGeometryOut.m_uNumIndices * sizeof(IndexType)));
	xGeometryOut.m_puIndices[0] = 0; xGeometryOut.m_puIndices[1] = 1; xGeometryOut.m_puIndices[2] = 2;
	xGeometryOut.m_puIndices[3] = 2; xGeometryOut.m_puIndices[4] = 1; xGeometryOut.m_puIndices[5] = 3;

	xGeometryOut.m_pxPositions[0] = { 1,1,0 };
	xGeometryOut.m_pxPositions[1] = { 1,-1,0 };
	xGeometryOut.m_pxPositions[2] = { -1,1,0 };
	xGeometryOut.m_pxPositions[3] = { -1,-1,0 };

	xGeometryOut.m_pxUVs[0] = { 1,0 };
	xGeometryOut.m_pxUVs[1] = { 1,1 };
	xGeometryOut.m_pxUVs[2] = { 0,0 };
	xGeometryOut.m_pxUVs[3] = { 0,1 };

	xGeometryOut.GenerateLayoutAndVertexData();
}

void Flux_MeshGeometry::GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut, Zenith_Maths::Matrix4 xTransform)
{
	xGeometryOut.m_uNumVerts = 4;
	xGeometryOut.m_uNumIndices = 6;
	xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(xGeometryOut.m_uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(xGeometryOut.m_uNumVerts * sizeof(Zenith_Maths::Vector2)));

	xGeometryOut.m_puIndices = static_cast<IndexType*>(Zenith_MemoryManagement::Allocate(xGeometryOut.m_uNumIndices * sizeof(IndexType)));
	xGeometryOut.m_puIndices[0] = 0; xGeometryOut.m_puIndices[1] = 1; xGeometryOut.m_puIndices[2] = 2;
	xGeometryOut.m_puIndices[3] = 2; xGeometryOut.m_puIndices[4] = 1; xGeometryOut.m_puIndices[5] = 3;

	xGeometryOut.m_pxPositions[0] = { 1,1,0 };
	xGeometryOut.m_pxPositions[1] = { 1,-1,0 };
	xGeometryOut.m_pxPositions[2] = { -1,1,0 };
	xGeometryOut.m_pxPositions[3] = { -1,-1,0 };

	Zenith_Maths::Vector4 xTemp0 = { xGeometryOut.m_pxPositions[0].x, xGeometryOut.m_pxPositions[0].y, xGeometryOut.m_pxPositions[0].z, 1. };
	Zenith_Maths::Vector4 xTemp1 = { xGeometryOut.m_pxPositions[1].x, xGeometryOut.m_pxPositions[1].y, xGeometryOut.m_pxPositions[1].z, 1. };
	Zenith_Maths::Vector4 xTemp2 = { xGeometryOut.m_pxPositions[2].x, xGeometryOut.m_pxPositions[2].y, xGeometryOut.m_pxPositions[2].z, 1. };
	Zenith_Maths::Vector4 xTemp3 = { xGeometryOut.m_pxPositions[3].x, xGeometryOut.m_pxPositions[3].y, xGeometryOut.m_pxPositions[3].z, 1. };

	xTemp0 = xTransform * xTemp0;
	xTemp1 = xTransform * xTemp1;
	xTemp2 = xTransform * xTemp2;
	xTemp3 = xTransform * xTemp3;

	xGeometryOut.m_pxPositions[0] = { xTemp0.x, xTemp0.y, xTemp0.z };
	xGeometryOut.m_pxPositions[1] = { xTemp1.x, xTemp1.y, xTemp1.z };
	xGeometryOut.m_pxPositions[2] = { xTemp2.x, xTemp2.y, xTemp2.z };
	xGeometryOut.m_pxPositions[3] = { xTemp3.x, xTemp3.y, xTemp3.z };

	xGeometryOut.m_pxUVs[0] = { 1,0 };
	xGeometryOut.m_pxUVs[1] = { 1,1 };
	xGeometryOut.m_pxUVs[2] = { 0,0 };
	xGeometryOut.m_pxUVs[3] = { 0,1 };

	xGeometryOut.GenerateLayoutAndVertexData();
}

void Flux_MeshGeometry::GenerateUnitCube(Flux_MeshGeometry& xGeometryOut)
{
	// The unit cube IS a box of half-extent 0.5 on every axis. Forwarding rather
	// than duplicating keeps the two from drifting in winding, UVs or tangents.
	GenerateBox(xGeometryOut, Zenith_Maths::Vector3(0.5f, 0.5f, 0.5f));
}

void Flux_MeshGeometry::GenerateBox(Flux_MeshGeometry& xGeometryOut,
	const Zenith_Maths::Vector3& xHalfExtents)
{
	// 24 vertices (4 per face for proper normals)
	// 36 indices (6 faces * 2 triangles * 3 indices)
	xGeometryOut.m_uNumVerts = 24;
	xGeometryOut.m_uNumIndices = 36;

	xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(24 * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(24 * sizeof(Zenith_Maths::Vector2)));
	xGeometryOut.m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(24 * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(24 * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(24 * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(24 * sizeof(Zenith_Maths::Vector4)));
	xGeometryOut.m_puIndices = static_cast<IndexType*>(Zenith_MemoryManagement::Allocate(36 * sizeof(IndexType)));

	uint32_t uVert = 0;
	uint32_t uIdx = 0;

	// Helper to add a face with 4 vertices and 6 indices
	auto AddFace = [&](
		const Zenith_Maths::Vector3& p0, const Zenith_Maths::Vector3& p1,
		const Zenith_Maths::Vector3& p2, const Zenith_Maths::Vector3& p3,
		const Zenith_Maths::Vector3& normal,
		const Zenith_Maths::Vector3& tangent,
		const Zenith_Maths::Vector3& bitangent)
	{
		uint32_t uBase = uVert;

		// 4 vertices per face
		xGeometryOut.m_pxPositions[uVert] = p0;
		xGeometryOut.m_pxNormals[uVert] = normal;
		xGeometryOut.m_pxTangents[uVert] = tangent;
		xGeometryOut.m_pxBitangents[uVert] = bitangent;
		xGeometryOut.m_pxUVs[uVert] = { 0.f, 0.f };
		xGeometryOut.m_pxColors[uVert] = { 1.f, 1.f, 1.f, 1.f };
		uVert++;

		xGeometryOut.m_pxPositions[uVert] = p1;
		xGeometryOut.m_pxNormals[uVert] = normal;
		xGeometryOut.m_pxTangents[uVert] = tangent;
		xGeometryOut.m_pxBitangents[uVert] = bitangent;
		xGeometryOut.m_pxUVs[uVert] = { 1.f, 0.f };
		xGeometryOut.m_pxColors[uVert] = { 1.f, 1.f, 1.f, 1.f };
		uVert++;

		xGeometryOut.m_pxPositions[uVert] = p2;
		xGeometryOut.m_pxNormals[uVert] = normal;
		xGeometryOut.m_pxTangents[uVert] = tangent;
		xGeometryOut.m_pxBitangents[uVert] = bitangent;
		xGeometryOut.m_pxUVs[uVert] = { 0.f, 1.f };
		xGeometryOut.m_pxColors[uVert] = { 1.f, 1.f, 1.f, 1.f };
		uVert++;

		xGeometryOut.m_pxPositions[uVert] = p3;
		xGeometryOut.m_pxNormals[uVert] = normal;
		xGeometryOut.m_pxTangents[uVert] = tangent;
		xGeometryOut.m_pxBitangents[uVert] = bitangent;
		xGeometryOut.m_pxUVs[uVert] = { 1.f, 1.f };
		xGeometryOut.m_pxColors[uVert] = { 1.f, 1.f, 1.f, 1.f };
		uVert++;

		// Two triangles with counter-clockwise winding for Vulkan: 0-2-1 and 1-2-3
		xGeometryOut.m_puIndices[uIdx++] = uBase + 0;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 2;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 1;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 1;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 2;
		xGeometryOut.m_puIndices[uIdx++] = uBase + 3;
	};

	// Box centred on the origin, spanning -half..+half on each axis. With
	// xHalfExtents == 0.5 these are bit-for-bit the old unit-cube literals.
	const float fX = xHalfExtents.x;
	const float fY = xHalfExtents.y;
	const float fZ = xHalfExtents.z;

	// +Z face (front)
	AddFace(
		{ -fX, -fY,  fZ }, {  fX, -fY,  fZ },
		{ -fX,  fY,  fZ }, {  fX,  fY,  fZ },
		{ 0.f, 0.f, 1.f }, { 1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }
	);
	// -Z face (back)
	AddFace(
		{  fX, -fY, -fZ }, { -fX, -fY, -fZ },
		{  fX,  fY, -fZ }, { -fX,  fY, -fZ },
		{ 0.f, 0.f, -1.f }, { -1.f, 0.f, 0.f }, { 0.f, 1.f, 0.f }
	);
	// +Y face (top)
	AddFace(
		{ -fX,  fY,  fZ }, {  fX,  fY,  fZ },
		{ -fX,  fY, -fZ }, {  fX,  fY, -fZ },
		{ 0.f, 1.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f, -1.f }
	);
	// -Y face (bottom)
	AddFace(
		{ -fX, -fY, -fZ }, {  fX, -fY, -fZ },
		{ -fX, -fY,  fZ }, {  fX, -fY,  fZ },
		{ 0.f, -1.f, 0.f }, { 1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }
	);
	// +X face (right)
	AddFace(
		{  fX, -fY,  fZ }, {  fX, -fY, -fZ },
		{  fX,  fY,  fZ }, {  fX,  fY, -fZ },
		{ 1.f, 0.f, 0.f }, { 0.f, 0.f, -1.f }, { 0.f, 1.f, 0.f }
	);
	// -X face (left)
	AddFace(
		{ -fX, -fY, -fZ }, { -fX, -fY,  fZ },
		{ -fX,  fY, -fZ }, { -fX,  fY,  fZ },
		{ -1.f, 0.f, 0.f }, { 0.f, 0.f, 1.f }, { 0.f, 1.f, 0.f }
	);

	xGeometryOut.GenerateLayoutAndVertexData();
	xGeometryOut.UploadToGPU();
}

// Single GPU-upload path shared by every generator + LoadFromFile.
void Flux_MeshGeometry::UploadToGPU()
{
	g_xEngine.FluxMemory().InitialiseVertexBuffer(GetVertexData(), GetVertexDataSize(), m_xVertexBuffer);
	g_xEngine.FluxMemory().InitialiseIndexBuffer(GetIndexData(), GetIndexDataSize(), m_xIndexBuffer);
}

// Capsule = sphere stretched along Y by fHeight (cylinder body + hemisphere caps
// approximated by stretching). Moved verbatim from Combat game code.
void Flux_MeshGeometry::GenerateCapsule(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices, uint32_t uStacks)
{
	float fCylinderHalfHeight = fHeight * 0.5f;

	uint32_t uNumVerts = (uStacks + 1) * (uSlices + 1);
	uint32_t uNumIndices = uStacks * uSlices * 6;

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	xGeometryOut.m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	xGeometryOut.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));

	uint32_t uVertIdx = 0;

	// Capsule vertices (sphere stretched along Y)
	for (uint32_t uStack = 0; uStack <= uStacks; uStack++)
	{
		float fPhi = static_cast<float>(uStack) / static_cast<float>(uStacks) * 3.14159265f;
		float fSphereY = cos(fPhi);  // -1 to 1
		float fStackRadius = sin(fPhi) * fRadius;

		// Stretch Y based on hemisphere, inserting the cylinder half-height gap.
		float fY = (fSphereY > 0.0f)
			? fSphereY * fRadius + fCylinderHalfHeight
			: fSphereY * fRadius - fCylinderHalfHeight;

		for (uint32_t uSlice = 0; uSlice <= uSlices; uSlice++)
		{
			float fTheta = static_cast<float>(uSlice) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
			float fX = cos(fTheta) * fStackRadius;
			float fZ = sin(fTheta) * fStackRadius;

			Zenith_Maths::Vector3 xPos(fX, fY, fZ);

			// Normal is the un-stretched sphere normal.
			Zenith_Maths::Vector3 xNormal(fX, cos(fPhi) * fRadius, fZ);
			if (glm::length(xNormal) > 0.001f)
			{
				xNormal = glm::normalize(xNormal);
			}
			else
			{
				xNormal = Zenith_Maths::Vector3(0.0f, fSphereY > 0.0f ? 1.0f : -1.0f, 0.0f);
			}

			xGeometryOut.m_pxPositions[uVertIdx] = xPos;
			xGeometryOut.m_pxNormals[uVertIdx] = xNormal;
			xGeometryOut.m_pxUVs[uVertIdx] = Zenith_Maths::Vector2(
				static_cast<float>(uSlice) / static_cast<float>(uSlices),
				static_cast<float>(uStack) / static_cast<float>(uStacks)
			);

			Zenith_Maths::Vector3 xTangent(-sin(fTheta), 0.0f, cos(fTheta));
			xGeometryOut.m_pxTangents[uVertIdx] = xTangent;
			xGeometryOut.m_pxBitangents[uVertIdx] = glm::cross(xNormal, xTangent);
			xGeometryOut.m_pxColors[uVertIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

			uVertIdx++;
		}
	}

	uint32_t uIdxIdx = 0;
	for (uint32_t uStack = 0; uStack < uStacks; uStack++)
	{
		for (uint32_t uSlice = 0; uSlice < uSlices; uSlice++)
		{
			uint32_t uCurrent = uStack * (uSlices + 1) + uSlice;
			uint32_t uNext = uCurrent + uSlices + 1;

			// Counter-clockwise winding for Vulkan.
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;

			xGeometryOut.m_puIndices[uIdxIdx++] = uCurrent + 1;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
			xGeometryOut.m_puIndices[uIdxIdx++] = uNext + 1;
		}
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	xGeometryOut.UploadToGPU();
}

// Cone = base ring + apex + base centre. Moved verbatim from Combat game code.
void Flux_MeshGeometry::GenerateCone(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices)
{
	uint32_t uNumVerts = uSlices + 2;     // base ring + apex + base centre
	uint32_t uNumIndices = uSlices * 6;   // side triangles + base triangles

	xGeometryOut.m_uNumVerts = uNumVerts;
	xGeometryOut.m_uNumIndices = uNumIndices;
	xGeometryOut.m_pxPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxNormals = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxUVs = static_cast<Zenith_Maths::Vector2*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector2)));
	xGeometryOut.m_pxTangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxBitangents = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector3)));
	xGeometryOut.m_pxColors = static_cast<Zenith_Maths::Vector4*>(Zenith_MemoryManagement::Allocate(uNumVerts * sizeof(Zenith_Maths::Vector4)));
	xGeometryOut.m_puIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Allocate(uNumIndices * sizeof(Flux_MeshGeometry::IndexType)));

	// Base ring vertices (indices 0 .. uSlices-1).
	for (uint32_t i = 0; i < uSlices; i++)
	{
		float fTheta = static_cast<float>(i) / static_cast<float>(uSlices) * 2.0f * 3.14159265f;
		float fX = cos(fTheta) * fRadius;
		float fZ = sin(fTheta) * fRadius;

		xGeometryOut.m_pxPositions[i] = Zenith_Maths::Vector3(fX, 0.0f, fZ);

		// Normal points outward and slightly up.
		float fNormalY = fRadius / fHeight;
		Zenith_Maths::Vector3 xNormal = glm::normalize(Zenith_Maths::Vector3(cos(fTheta), fNormalY, sin(fTheta)));
		xGeometryOut.m_pxNormals[i] = xNormal;

		xGeometryOut.m_pxUVs[i] = Zenith_Maths::Vector2(static_cast<float>(i) / static_cast<float>(uSlices), 0.0f);
		xGeometryOut.m_pxTangents[i] = Zenith_Maths::Vector3(-sin(fTheta), 0.0f, cos(fTheta));
		xGeometryOut.m_pxBitangents[i] = glm::cross(xNormal, xGeometryOut.m_pxTangents[i]);
		xGeometryOut.m_pxColors[i] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// Apex (index uSlices).
	uint32_t uApexIdx = uSlices;
	xGeometryOut.m_pxPositions[uApexIdx] = Zenith_Maths::Vector3(0.0f, fHeight, 0.0f);
	xGeometryOut.m_pxNormals[uApexIdx] = Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f);
	xGeometryOut.m_pxUVs[uApexIdx] = Zenith_Maths::Vector2(0.5f, 1.0f);
	xGeometryOut.m_pxTangents[uApexIdx] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxBitangents[uApexIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	xGeometryOut.m_pxColors[uApexIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// Base centre (index uSlices+1).
	uint32_t uBaseCenterIdx = uSlices + 1;
	xGeometryOut.m_pxPositions[uBaseCenterIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxNormals[uBaseCenterIdx] = Zenith_Maths::Vector3(0.0f, -1.0f, 0.0f);
	xGeometryOut.m_pxUVs[uBaseCenterIdx] = Zenith_Maths::Vector2(0.5f, 0.5f);
	xGeometryOut.m_pxTangents[uBaseCenterIdx] = Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);
	xGeometryOut.m_pxBitangents[uBaseCenterIdx] = Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f);
	xGeometryOut.m_pxColors[uBaseCenterIdx] = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	uint32_t uIdxIdx = 0;
	// Side triangles (base ring -> apex). Outward winding: cross(C-A,B-A) faces
	// out+up. The prior i,apex,next order was inverted and back-face culled.
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uNext = (i + 1) % uSlices;
		xGeometryOut.m_puIndices[uIdxIdx++] = i;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
		xGeometryOut.m_puIndices[uIdxIdx++] = uApexIdx;
	}
	// Base triangles (base ring -> centre), faces -Y (was inverted too).
	for (uint32_t i = 0; i < uSlices; i++)
	{
		uint32_t uNext = (i + 1) % uSlices;
		xGeometryOut.m_puIndices[uIdxIdx++] = uNext;
		xGeometryOut.m_puIndices[uIdxIdx++] = i;
		xGeometryOut.m_puIndices[uIdxIdx++] = uBaseCenterIdx;
	}

	xGeometryOut.GenerateLayoutAndVertexData();
	xGeometryOut.UploadToGPU();
}

ShaderDataType StringToShaderDataType(const std::string& strString)
{
	if (strString == "Float") return SHADER_DATA_TYPE_FLOAT;
	if (strString == "Float2") return SHADER_DATA_TYPE_FLOAT2;
	if (strString == "Float3") return SHADER_DATA_TYPE_FLOAT3;
	if (strString == "Float4") return SHADER_DATA_TYPE_FLOAT4;
	if (strString == "UInt") return SHADER_DATA_TYPE_UINT;
	if (strString == "UInt4") return SHADER_DATA_TYPE_UINT4;
	Zenith_Assert(false, "Unrecognized data type");
	return SHADER_DATA_TYPE_NONE;
}

template<typename T>
void ReadAttribute(T*& ptData, Zenith_DataStream& xStream, u_int uDataSize)
{
	bool bRead;
	xStream >> bRead;

	if (bRead)
	{
		ptData = static_cast<T*>(Zenith_MemoryManagement::Allocate(uDataSize));
		xStream.ReadData(ptData, uDataSize);
	}
	else
	{
		ptData = nullptr;
	}
}
template<typename T>
void SkipAttribute(T*& ptData, Zenith_DataStream& xStream, u_int uDataSize)
{
	bool bRead;
	xStream >> bRead;

	if (bRead)
	{
		ptData = nullptr;
		xStream.SkipBytes(uDataSize);
	}
	else
	{
		ptData = nullptr;
	}
}

#define READ_ATTR(uAttribute, pOutData, uSize)\
if (uRetainAttributeBits & 1 << uAttribute)\
{\
	ReadAttribute(pOutData, xStream, uSize);\
}\
else\
{\
	SkipAttribute(pOutData, xStream, uSize);\
}

void Flux_MeshGeometry::LoadFromFile(const char* szPath, Flux_MeshGeometry& xGeometryOut, u_int uRetainAttributeBits /*= 0*/, const bool bUploadToGPU /*= true*/)
{
	Zenith_Profiling::ScopeZone xProfileScope(ZENITH_PROFILE_ZONE("Flux Mesh Geometry Load From File"));
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	xStream >> xGeometryOut.m_xBufferLayout.GetElements();
	xGeometryOut.m_xBufferLayout.CalculateOffsetsAndStrides();
	xStream >> xGeometryOut.m_uNumVerts;
	xStream >> xGeometryOut.m_uNumIndices;
	xStream >> xGeometryOut.m_uNumBones;
	xGeometryOut.m_xBoneNameToIdAndOffset.ReadFromDataStream(xStream);
	xStream >> xGeometryOut.m_xMaterialColor;

	ReadAttribute(xGeometryOut.m_pVertexData, xStream, xGeometryOut.m_uNumVerts * xGeometryOut.m_xBufferLayout.GetStride());
	ReadAttribute(xGeometryOut.m_puIndices, xStream, xGeometryOut.m_uNumIndices * sizeof(m_puIndices[0]));

	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__POSITION, xGeometryOut.m_pxPositions, xGeometryOut.m_uNumVerts * sizeof(m_pxPositions[0]));
	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__NORMAL, xGeometryOut.m_pxNormals, xGeometryOut.m_uNumVerts * sizeof(m_pxNormals[0]));
	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__TANGENT, xGeometryOut.m_pxTangents, xGeometryOut.m_uNumVerts * sizeof(m_pxTangents[0]));
	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__BITANGENT, xGeometryOut.m_pxBitangents, xGeometryOut.m_uNumVerts * sizeof(m_pxBitangents[0]));
	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__COLOR, xGeometryOut.m_pxColors, xGeometryOut.m_uNumVerts * sizeof(m_pxColors[0]));
	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__BONE_IDS, xGeometryOut.m_puBoneIDs, xGeometryOut.m_uNumVerts * MAX_BONES_PER_VERTEX * sizeof(m_puBoneIDs[0]));
	READ_ATTR(FLUX_VERTEX_ATTRIBUTE__BONE_WEIGHTS, xGeometryOut.m_pfBoneWeights, xGeometryOut.m_uNumVerts * MAX_BONES_PER_VERTEX * sizeof(m_pfBoneWeights[0]));

	if(bUploadToGPU)
	{
		xGeometryOut.UploadToGPU();
	}
}

// Reallocate pxDst to hold (uDstCount + uSrcCount) Ts and append uSrcCount
// entries from pxSrc starting at the uDstCount offset. Returns false on
// allocation failure (error already logged). Used by Combine() to collapse
// the five near-identical non-position attribute stanzas.
template<typename T>
static bool EnsureAndCopyAttributeStream(T*& pxDst, u_int uDstCount,
                                        const T* pxSrc, u_int uSrcCount,
                                        const char* szName)
{
	// Cast to u_int64 before addition to prevent uint32_t overflow
	u_int64 ulNewSize = (static_cast<u_int64>(uDstCount) + uSrcCount) * sizeof(T);
	T* pxNew = static_cast<T*>(Zenith_MemoryManagement::Reallocate(pxDst, ulNewSize));
	if (pxNew == nullptr)
	{
		Zenith_Error(LOG_CATEGORY_MESH, "%s buffer reallocation failed", szName);
		return false;
	}
	pxDst = pxNew;
	memcpy(pxDst + uDstCount, pxSrc, uSrcCount * sizeof(T));
	return true;
}

void Flux_MeshGeometry::Combine(Flux_MeshGeometry& xDst, const Flux_MeshGeometry& xSrc)
{
	Zenith_Assert(xDst.m_xBufferLayout == xSrc.m_xBufferLayout, "Incompatible meshes");

	if(xDst.m_ulReservedVertexDataSize < xDst.GetVertexDataSize() + xSrc.GetVertexDataSize())
	{
		u_int64 newSize = xDst.GetVertexDataSize() + xSrc.GetVertexDataSize();
		Zenith_Log(LOG_CATEGORY_MESH, "WARNING: Vertex buffer reallocation required! Reserved: %llu, Needed: %llu", xDst.m_ulReservedVertexDataSize, newSize);
		u_int8* pNewData = static_cast<u_int8*>(Zenith_MemoryManagement::Reallocate(xDst.m_pVertexData, newSize));
		if (pNewData == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_MESH, "Vertex buffer reallocation failed");
			return;
		}
		xDst.m_pVertexData = pNewData;
		xDst.m_ulReservedVertexDataSize = newSize;
	}
	memcpy(xDst.m_pVertexData + xDst.GetVertexDataSize(), xSrc.m_pVertexData, xSrc.GetVertexDataSize());

	if(xDst.m_ulReservedIndexDataSize < xDst.GetIndexDataSize() + xSrc.GetIndexDataSize())
	{
		u_int64 newSize = xDst.GetIndexDataSize() + xSrc.GetIndexDataSize();
		Zenith_Log(LOG_CATEGORY_MESH, "WARNING: Index buffer reallocation required! Reserved: %llu, Needed: %llu", xDst.m_ulReservedIndexDataSize, newSize);
		Flux_MeshGeometry::IndexType* pNewIndices = static_cast<Flux_MeshGeometry::IndexType*>(Zenith_MemoryManagement::Reallocate(xDst.m_puIndices, newSize));
		if (pNewIndices == nullptr)
		{
			Zenith_Error(LOG_CATEGORY_MESH, "Index buffer reallocation failed");
			return;
		}
		xDst.m_puIndices = pNewIndices;
		xDst.m_ulReservedIndexDataSize = newSize;
	}
	memcpy(xDst.m_puIndices + xDst.m_uNumIndices, xSrc.m_puIndices, xSrc.GetIndexDataSize());
	for (u_int u = 0; u < xSrc.m_uNumIndices; u++)
	{
		xDst.m_puIndices[xDst.m_uNumIndices + u] += xDst.m_uNumVerts;
	}

	if (xDst.m_pxPositions)
	{
		// Cast to u_int64 before addition to prevent uint32_t overflow
		u_int64 ulTotalVerts = static_cast<u_int64>(xDst.m_uNumVerts) + xSrc.m_uNumVerts;
		if(xDst.m_ulReservedPositionDataSize < ulTotalVerts * sizeof(Zenith_Maths::Vector3))
		{
			u_int64 newSize = ulTotalVerts * sizeof(Zenith_Maths::Vector3);
			Zenith_Log(LOG_CATEGORY_MESH, "WARNING: Position buffer reallocation required! Reserved: %llu, Needed: %llu", xDst.m_ulReservedPositionDataSize, newSize);
			Zenith_Maths::Vector3* pNewPositions = static_cast<Zenith_Maths::Vector3*>(Zenith_MemoryManagement::Reallocate(xDst.m_pxPositions, newSize));
			if (pNewPositions == nullptr)
			{
				Zenith_Error(LOG_CATEGORY_MESH, "Position buffer reallocation failed");
				return;
			}
			xDst.m_pxPositions = pNewPositions;
			xDst.m_ulReservedPositionDataSize = newSize;
		}
		memcpy(xDst.m_pxPositions + xDst.m_uNumVerts, xSrc.m_pxPositions, xSrc.m_uNumVerts * sizeof(Zenith_Maths::Vector3));
	}

	if (xDst.m_pxNormals && !EnsureAndCopyAttributeStream(xDst.m_pxNormals, xDst.m_uNumVerts, xSrc.m_pxNormals, xSrc.m_uNumVerts, "Normal"))
		return;

	if (xDst.m_pxTangents && !EnsureAndCopyAttributeStream(xDst.m_pxTangents, xDst.m_uNumVerts, xSrc.m_pxTangents, xSrc.m_uNumVerts, "Tangent"))
		return;

	if (xDst.m_pxBitangents && !EnsureAndCopyAttributeStream(xDst.m_pxBitangents, xDst.m_uNumVerts, xSrc.m_pxBitangents, xSrc.m_uNumVerts, "Bitangent"))
		return;

	if (xDst.m_pxColors && !EnsureAndCopyAttributeStream(xDst.m_pxColors, xDst.m_uNumVerts, xSrc.m_pxColors, xSrc.m_uNumVerts, "Color"))
		return;

	xDst.m_uNumVerts += xSrc.m_uNumVerts;
	xDst.m_uNumIndices += xSrc.m_uNumIndices;
	xDst.m_uNumBones += xSrc.m_uNumBones;
	for (Zenith_HashMap<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>>::Iterator xIt(xSrc.m_xBoneNameToIdAndOffset); !xIt.Done(); xIt.Next())
	{
		Zenith_Assert(!xDst.m_xBoneNameToIdAndOffset.Contains(xIt.GetKey()), "Bone name collision when combining meshes");
		xDst.m_xBoneNameToIdAndOffset.Insert(xIt.GetKey(), xIt.GetValue());
	}
	//#TO just ignore material color
}

#ifdef ZENITH_TOOLS
template<typename T>
void ExportAttribute(T* ptData, Zenith_DataStream& xStream, u_int uSize)
{
	if (ptData)
	{
		xStream << bool(true);
		xStream.WriteData(ptData, uSize);
	}
	else
	{
		xStream << bool(false);
	}
}
void Flux_MeshGeometry::Export(const char* szFilename)
{
	Zenith_DataStream xStream;
	xStream << m_xBufferLayout.GetElements();
	xStream << m_uNumVerts;
	xStream << m_uNumIndices;
	xStream << m_uNumBones;
	m_xBoneNameToIdAndOffset.WriteToDataStream(xStream);
	xStream << m_xMaterialColor;
	ExportAttribute(m_pVertexData, xStream, m_uNumVerts * m_xBufferLayout.GetStride());
	ExportAttribute(m_puIndices, xStream, m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType));
	ExportAttribute(m_pxPositions, xStream, m_uNumVerts * sizeof(m_pxPositions[0]));
	ExportAttribute(m_pxNormals, xStream, m_uNumVerts * sizeof(m_pxNormals[0]));
	ExportAttribute(m_pxTangents, xStream, m_uNumVerts * sizeof(m_pxTangents[0]));
	ExportAttribute(m_pxBitangents, xStream, m_uNumVerts * sizeof(m_pxBitangents[0]));
	ExportAttribute(m_pxColors, xStream, m_uNumVerts * sizeof(m_pxColors[0]));
	ExportAttribute(m_puBoneIDs, xStream, m_uNumVerts * MAX_BONES_PER_VERTEX * sizeof(m_puBoneIDs[0]));
	ExportAttribute(m_pfBoneWeights, xStream, m_uNumVerts * MAX_BONES_PER_VERTEX * sizeof(m_pfBoneWeights[0]));

	xStream.WriteToFile(szFilename);
}
#endif

void Flux_MeshGeometry::GenerateLayoutAndVertexData()
{
	// One row per FLOAT attribute this geometry can carry, in the order the
	// SERIALIZED element table declares them — that order is the .zmesh file format
	// (Export writes m_xBufferLayout.GetElements() verbatim), so it must not be
	// rearranged. Presence is "the stream exists", and an element is declared only
	// for a stream that does.
	struct AttributeRow
	{
		const void*        m_pvStream;
		ShaderDataType     m_eType;
		FluxVertexSemantic m_eSemantic;
		u_int              m_uLanes;
	};
	const AttributeRow axRows[] =
	{
		{ m_pxPositions,  SHADER_DATA_TYPE_FLOAT3, FLUX_VERTEX_SEMANTIC_POSITION, 3u },
		{ m_pxUVs,        SHADER_DATA_TYPE_FLOAT2, FLUX_VERTEX_SEMANTIC_TEXCOORD, 2u },
		{ m_pxNormals,    SHADER_DATA_TYPE_FLOAT3, FLUX_VERTEX_SEMANTIC_NORMAL,   3u },
		{ m_pxTangents,   SHADER_DATA_TYPE_FLOAT3, FLUX_VERTEX_SEMANTIC_TANGENT,  3u },
		{ m_pxBitangents, SHADER_DATA_TYPE_FLOAT3, FLUX_VERTEX_SEMANTIC_BINORMAL, 3u },
		{ m_pxColors,     SHADER_DATA_TYPE_FLOAT4, FLUX_VERTEX_SEMANTIC_COLOR,    4u },
	};
	constexpr u_int uMAX_FLOAT_ELEMENTS = static_cast<u_int>(sizeof(axRows) / sizeof(axRows[0]));

	// Where THIS call's elements begin. It is 0 for the only supported shape — a
	// geometry whose layout has not been declared yet — but reading the offsets back
	// relative to it, rather than from absolute 0, is what keeps the bytes consistent
	// with the stride GetVertexDataSize()/UploadToGPU() will use if a caller ever
	// generates into a geometry that was not Reset.
	const u_int uElementBase = m_xBufferLayout.GetElements().GetSize();

	Flux_VertexLayoutElement axElements[uMAX_FLOAT_ELEMENTS] = {};
	Flux_VertexSourceView    axSources[uMAX_FLOAT_ELEMENTS]  = {};
	u_int uNumFloatElements = 0;
	for (const AttributeRow& xRow : axRows)
	{
		if (xRow.m_pvStream == nullptr)
		{
			continue;
		}
		m_xBufferLayout.GetElements().PushBack({ xRow.m_eType });
		// The offset is filled in from the layout below — it is the layout's answer,
		// not something recomputed here.
		axElements[uNumFloatElements] = { xRow.m_eSemantic, 0u, xRow.m_eType, 0u, 0u };
		axSources[uNumFloatElements]  = { xRow.m_eSemantic, 0u, static_cast<const float*>(xRow.m_pvStream), xRow.m_uLanes };
		uNumFloatElements++;
	}

	const bool bHasBones = (m_puBoneIDs != nullptr);
	if (bHasBones)
	{
		Zenith_Assert(m_pfBoneWeights != nullptr, "How have we wound up with bone IDs but no weights");
		static_assert(MAX_BONES_PER_VERTEX == 4, "data type needs changing");
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_UINT4 });
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT4 });
	}

	// Offsets + stride are computed ONCE, here, from the element table that was just
	// declared — and every writer below reads them back out of it, so the bytes and
	// the table the .zmesh carries cannot disagree.
	m_xBufferLayout.CalculateOffsetsAndStrides();
	const u_int uStride = m_xBufferLayout.GetStride();
	for (u_int u = 0; u < uNumFloatElements; u++)
	{
		axElements[u].m_uOffset = m_xBufferLayout.GetElements().Get(uElementBase + u).m_uOffset;
	}

	m_pVertexData = static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(static_cast<size_t>(m_uNumVerts) * uStride));

	const Flux_VertexLayoutDesc xVertexLayout{ axElements, uNumFloatElements, { uStride, 0u } };
	Flux_PackVertices(m_pVertexData, xVertexLayout, axSources, uNumFloatElements, m_uNumVerts);

	if (bHasBones)
	{
		// The bone tail is deliberately NOT packer work: a uint4 palette index has no
		// vertex semantic, and the packer refuses the integer families by design (see
		// Flux_VertexPacker.h). It is copied straight in at the offsets the same
		// layout named, one 4-lane block each.
		const u_int uBoneIDOffset     = m_xBufferLayout.GetElements().Get(uElementBase + uNumFloatElements).m_uOffset;
		const u_int uBoneWeightOffset = m_xBufferLayout.GetElements().Get(uElementBase + uNumFloatElements + 1u).m_uOffset;
		for (uint32_t i = 0; i < m_uNumVerts; i++)
		{
			u_int8* const pVertex = m_pVertexData + static_cast<size_t>(i) * uStride;
			const size_t uBoneBase = static_cast<size_t>(i) * MAX_BONES_PER_VERTEX;
			memcpy(pVertex + uBoneIDOffset, m_puBoneIDs + uBoneBase, MAX_BONES_PER_VERTEX * sizeof(m_puBoneIDs[0]));
			memcpy(pVertex + uBoneWeightOffset, m_pfBoneWeights + uBoneBase, MAX_BONES_PER_VERTEX * sizeof(m_pfBoneWeights[0]));
		}
	}
}

void Flux_MeshGeometry::GenerateNormals()
{
	for (size_t i = 0; i < m_uNumIndices / 3; i++)
	{
		IndexType uA = m_puIndices[i * 3];
		IndexType uB = m_puIndices[i * 3 + 1];
		IndexType uC = m_puIndices[i * 3 + 2];

		Zenith_Maths::Vector3 xPosA = m_pxPositions[uA];
		Zenith_Maths::Vector3 xPosB = m_pxPositions[uB];
		Zenith_Maths::Vector3 xPosC = m_pxPositions[uC];

		Zenith_Maths::Vector3 xNormal = glm::cross(xPosB - xPosA, xPosC - xPosA);
		m_pxNormals[uA] += xNormal;
		m_pxNormals[uB] += xNormal;
		m_pxNormals[uC] += xNormal;
	}

	for (size_t i = 0; i < m_uNumVerts; i++)
	{
		m_pxNormals[i] = glm::normalize(m_pxNormals[i]);
	}
}

void Flux_MeshGeometry::GenerateTangents()
{
	for (uint32_t i = 0; i < m_uNumIndices / 3; i++)
	{
		uint32_t uA = m_puIndices[i * 3];
		uint32_t uB = m_puIndices[i * 3 + 1];
		uint32_t uC = m_puIndices[i * 3 + 2];
		Zenith_Maths::Vector3 tangent = GenerateTangent(uA, uB, uC);
		m_pxTangents[uA] += tangent;
		m_pxTangents[uB] += tangent;
		m_pxTangents[uC] += tangent;
	}

	for (uint32_t i = 0; i < m_uNumVerts; i++)
	{
		m_pxTangents[i] = glm::normalize(m_pxTangents[i]);
	}
}

void Flux_MeshGeometry::GenerateBitangents()
{
	for (uint32_t i = 0; i < m_uNumVerts; i++)
	{
		m_pxBitangents[i] = glm::cross(m_pxNormals[i], m_pxTangents[i]);
	}
}

Zenith_Maths::Vector3 Flux_MeshGeometry::GenerateTangent(uint32_t a, uint32_t b, uint32_t c) {
	Zenith_Maths::Vector3 xBa = m_pxPositions[b] - m_pxPositions[a];
	Zenith_Maths::Vector3 xCa = m_pxPositions[c] - m_pxPositions[a];
	Zenith_Maths::Vector2 xTba = m_pxUVs[b] - m_pxUVs[a];
	Zenith_Maths::Vector2 xTca = m_pxUVs[c] - m_pxUVs[a];

	Zenith_Maths::Matrix2 xTexMatrix(xTba, xTca);
	xTexMatrix = glm::inverse(xTexMatrix);

	Zenith_Maths::Vector3 xTangent = xBa * xTexMatrix[0][0] + xCa * xTexMatrix[0][1];
	Zenith_Maths::Vector3 xBinormal = xBa * xTexMatrix[1][0] + xCa * xTexMatrix[1][1];

	Zenith_Maths::Vector3 xNormal = glm::cross(xBa, xCa);
	Zenith_Maths::Vector3 biCross = glm::cross(xTangent, xNormal);

	return std::move(Zenith_Maths::Vector3(xTangent.x, xTangent.y, xTangent.z));
}