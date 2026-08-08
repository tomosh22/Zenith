#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Flux_MeshInstance.h"
#include "Flux_MeshGeometry.h"
#include "Flux/MeshGeometry/Flux_VertexPacker.h"          // Flux_PackVertices + the canonical defaults
#include "Flux/Shaders/Generated/UnifiedMesh.h"           // THE reflected 72-byte static-mesh layout
#include "Flux/UnifiedMesh/Flux_Skinning.h"               // Flux_SkinInputVertex (the 104-byte skin-input contract)
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"


// Helper function to apply bind pose skinning transformation
// This applies the full skinning equation at bind pose:
//   skinnedPos = boneBindPoseModel * inverseBindPose * meshLocalPos
// For properly set up skeletons, boneBindPoseModel * inverseBindPose = identity at bind pose,
// so vertices stay at their mesh-local positions.
static Zenith_Maths::Vector3 ApplyBindPoseSkinning(
	const Zenith_Maths::Vector3& xOriginalPos,
	const glm::uvec4& xBoneIndices,
	const glm::vec4& xBoneWeights,
	const Zenith_SkeletonAsset* pxSkeleton)
{
	float fTotalWeight = 0.0f;
	Zenith_Maths::Vector3 xSkinnedPos(0.0f);

	for (int i = 0; i < 4; i++)
	{
		float fWeight = xBoneWeights[i];
		if (fWeight <= 0.0f)
		{
			continue;
		}

		uint32_t uBoneIndex = xBoneIndices[i];
		if (uBoneIndex >= pxSkeleton->GetNumBones())
		{
			continue;
		}

		fTotalWeight += fWeight;

		const Zenith_SkeletonAsset::Bone& xBone = pxSkeleton->GetBone(uBoneIndex);

		// Apply full bind pose skinning equation: boneBindPoseModel * inverseBindPose * localPos
		// This properly positions vertices from mesh-local space to world space at bind pose
		Zenith_Maths::Matrix4 xSkinningMatrix = xBone.m_xBindPoseModel * xBone.m_xInverseBindPose;
		Zenith_Maths::Vector4 xTransformed = xSkinningMatrix * Zenith_Maths::Vector4(xOriginalPos, 1.0f);
		xSkinnedPos += fWeight * Zenith_Maths::Vector3(xTransformed);
	}

	// If no valid bones contributed, return original position unchanged
	if (fTotalWeight <= 0.0f)
	{
		return xOriginalPos;
	}

	return xSkinnedPos;
}

Flux_MeshInstance::~Flux_MeshInstance()
{
	Destroy();
}

Flux_MeshInstance::Flux_MeshInstance(Flux_MeshInstance&& xOther)
	: m_xVertexBuffer(std::move(xOther.m_xVertexBuffer))
	, m_xIndexBuffer(std::move(xOther.m_xIndexBuffer))
	, m_xBufferLayout(std::move(xOther.m_xBufferLayout))
	, m_uNumVerts(xOther.m_uNumVerts)
	, m_uNumIndices(xOther.m_uNumIndices)
	, m_xSourceAsset(std::move(xOther.m_xSourceAsset))
	, m_pxProceduralGeometry(xOther.m_pxProceduralGeometry)
	, m_bInitialized(xOther.m_bInitialized)
{
	xOther.m_uNumVerts = 0;
	xOther.m_uNumIndices = 0;
	xOther.m_pxProceduralGeometry = nullptr;
	xOther.m_bInitialized = false;
}

Flux_MeshInstance& Flux_MeshInstance::operator=(Flux_MeshInstance&& xOther)
{
	if (this != &xOther)
	{
		Destroy();

		m_xVertexBuffer = std::move(xOther.m_xVertexBuffer);
		m_xIndexBuffer = std::move(xOther.m_xIndexBuffer);
		m_xBufferLayout = std::move(xOther.m_xBufferLayout);
		m_uNumVerts = xOther.m_uNumVerts;
		m_uNumIndices = xOther.m_uNumIndices;
		m_xSourceAsset = std::move(xOther.m_xSourceAsset);
		m_pxProceduralGeometry = xOther.m_pxProceduralGeometry;
		m_bInitialized = xOther.m_bInitialized;

		xOther.m_uNumVerts = 0;
		xOther.m_uNumIndices = 0;
		xOther.m_pxProceduralGeometry = nullptr;
		xOther.m_bInitialized = false;
	}
	return *this;
}

uint32_t Flux_MeshInstance::GetNumVerts() const
{
	if (m_pxProceduralGeometry)
	{
		return m_pxProceduralGeometry->GetNumVerts();
	}
	return m_uNumVerts;
}

uint32_t Flux_MeshInstance::GetNumIndices() const
{
	if (m_pxProceduralGeometry)
	{
		return m_pxProceduralGeometry->GetNumIndices();
	}
	return m_uNumIndices;
}

const Flux_BufferLayout& Flux_MeshInstance::GetBufferLayout() const
{
	if (m_pxProceduralGeometry)
	{
		return m_pxProceduralGeometry->GetBufferLayout();
	}
	return m_xBufferLayout;
}

const Flux_VertexBuffer& Flux_MeshInstance::GetVertexBuffer() const
{
	if (m_pxProceduralGeometry)
	{
		return m_pxProceduralGeometry->GetVertexBuffer();
	}
	return m_xVertexBuffer;
}

const Flux_IndexBuffer& Flux_MeshInstance::GetIndexBuffer() const
{
	if (m_pxProceduralGeometry)
	{
		return m_pxProceduralGeometry->GetIndexBuffer();
	}
	return m_xIndexBuffer;
}

Flux_MeshInstance* Flux_MeshInstance::CreateFromGeometry(Flux_MeshGeometry* pxGeometry)
{
	Zenith_Assert(pxGeometry != nullptr, "Cannot create mesh instance from null geometry");
	if (!pxGeometry)
	{
		return nullptr;
	}

	if (pxGeometry->GetNumVerts() == 0 || pxGeometry->GetNumIndices() == 0)
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Cannot create mesh instance from empty geometry");
		return nullptr;
	}

	Flux_MeshInstance* pxInstance = new Flux_MeshInstance();
	pxInstance->m_pxProceduralGeometry = pxGeometry;
	// m_bInitialized stays false - the geometry owns the buffers, so Destroy()
	// must not tear them down here.
	return pxInstance;
}

namespace
{
	// A mesh asset's CPU attribute arrays are ALREADY tight SoA float streams — a
	// Zenith_Vector<Vector3> is a float3 stream — so a packer source view is a
	// pointer plus a lane count, with no repacking anywhere. These asserts are what
	// make that reinterpret honest: a padded gentype would break the stride the
	// packer walks, silently.
	static_assert(sizeof(Zenith_Maths::Vector2) == 2 * sizeof(float), "attribute streams must be tight: Vector2");
	static_assert(sizeof(Zenith_Maths::Vector3) == 3 * sizeof(float), "attribute streams must be tight: Vector3");
	static_assert(sizeof(Zenith_Maths::Vector4) == 4 * sizeof(float), "attribute streams must be tight: Vector4");

	// The array's floats, or nullptr when it cannot cover uNumVerts vertices.
	// "Too short must simply not be passed" is the packer's contract for routing an
	// attribute onto its canonical default, and `GetSize() >= uNumVerts` is exactly
	// the presence test every loop replaced here already applied.
	template<typename T>
	const float* SourceFloatsOrNull(const Zenith_Vector<T>& xArray, uint32_t uNumVerts)
	{
		static_assert(sizeof(T) % sizeof(float) == 0, "an attribute stream must be whole floats");
		return (xArray.GetSize() >= uNumVerts) ? reinterpret_cast<const float*>(xArray.GetDataPointer()) : nullptr;
	}

	// xyz of a semantic's canonical default — the one vocabulary, shared by the
	// packer-driven static path and the hand-written skin-input builder below, so
	// "a mesh with no normals" cannot come to mean two different things.
	Zenith_Maths::Vector3 CanonicalDefault3(FluxVertexSemantic eSemantic)
	{
		return Zenith_Maths::Vector3(Flux_CanonicalVertexDefault(eSemantic));
	}
}

void Flux_PackStaticMeshVertices(
	void* pDst,
	const Zenith_MeshAsset& xAsset,
	uint32_t uNumVerts,
	const Zenith_Maths::Vector3* pxPositionOverride)
{
	// The bind-pose baked path pre-skins the positions and passes the RESULT; every
	// other caller reads the asset's own (which may be absent).
	const float* const pfPositions = (pxPositionOverride != nullptr)
		? reinterpret_cast<const float*>(pxPositionOverride)
		: SourceFloatsOrNull(xAsset.m_xPositions, uNumVerts);

	const Flux_VertexSourceView axSources[] =
	{
		{ FLUX_VERTEX_SEMANTIC_POSITION, 0u, pfPositions,                                            3u },
		{ FLUX_VERTEX_SEMANTIC_TEXCOORD, 0u, SourceFloatsOrNull(xAsset.m_xUVs,        uNumVerts),    2u },
		{ FLUX_VERTEX_SEMANTIC_NORMAL,   0u, SourceFloatsOrNull(xAsset.m_xNormals,    uNumVerts),    3u },
		{ FLUX_VERTEX_SEMANTIC_TANGENT,  0u, SourceFloatsOrNull(xAsset.m_xTangents,   uNumVerts),    3u },
		{ FLUX_VERTEX_SEMANTIC_BINORMAL, 0u, SourceFloatsOrNull(xAsset.m_xBitangents, uNumVerts),    3u },
		{ FLUX_VERTEX_SEMANTIC_COLOR,    0u, SourceFloatsOrNull(xAsset.m_xColors,     uNumVerts),    4u },
	};

	Flux_PackVertices(
		pDst,
		Flux_Generated_UnifiedMesh::UnifiedMesh_ToGBuffer::kVertexLayout,
		axSources, static_cast<u_int>(sizeof(axSources) / sizeof(axSources[0])), uNumVerts);
}

void Flux_BuildSkinInputVertices(Flux_SkinInputVertex* paxDst, const Zenith_MeshAsset& xAsset, u_int uNumVerts)
{
	Zenith_Assert(paxDst != nullptr || uNumVerts == 0u, "Flux_BuildSkinInputVertices: null destination for %u vertices", uNumVerts);
	if (paxDst == nullptr)
	{
		return;
	}

	const bool bHasPositions   = xAsset.m_xPositions.GetSize()   >= uNumVerts;
	const bool bHasUVs         = xAsset.m_xUVs.GetSize()         >= uNumVerts;
	const bool bHasNormals     = xAsset.m_xNormals.GetSize()     >= uNumVerts;
	const bool bHasTangents    = xAsset.m_xTangents.GetSize()    >= uNumVerts;
	const bool bHasBitangents  = xAsset.m_xBitangents.GetSize()  >= uNumVerts;
	const bool bHasColors      = xAsset.m_xColors.GetSize()      >= uNumVerts;
	const bool bHasBoneIndices = xAsset.m_xBoneIndices.GetSize() >= uNumVerts;
	const bool bHasBoneWeights = xAsset.m_xBoneWeights.GetSize() >= uNumVerts;

	for (u_int uVertex = 0; uVertex < uNumVerts; uVertex++)
	{
		Flux_SkinInputVertex& xVertex = paxDst[uVertex];

		xVertex.m_xPosition  = bHasPositions  ? xAsset.m_xPositions.Get(uVertex)  : CanonicalDefault3(FLUX_VERTEX_SEMANTIC_POSITION);
		xVertex.m_xUV        = bHasUVs        ? xAsset.m_xUVs.Get(uVertex)        : Zenith_Maths::Vector2(Flux_CanonicalVertexDefault(FLUX_VERTEX_SEMANTIC_TEXCOORD));
		xVertex.m_xNormal    = bHasNormals    ? xAsset.m_xNormals.Get(uVertex)    : CanonicalDefault3(FLUX_VERTEX_SEMANTIC_NORMAL);
		xVertex.m_xTangent   = bHasTangents   ? xAsset.m_xTangents.Get(uVertex)   : CanonicalDefault3(FLUX_VERTEX_SEMANTIC_TANGENT);
		xVertex.m_xBitangent = bHasBitangents ? xAsset.m_xBitangents.Get(uVertex) : CanonicalDefault3(FLUX_VERTEX_SEMANTIC_BINORMAL);
		xVertex.m_xColor     = bHasColors     ? xAsset.m_xColors.Get(uVertex)     : Flux_CanonicalVertexDefault(FLUX_VERTEX_SEMANTIC_COLOR);

		// Bone lanes have no vertex SEMANTIC: an unweighted vertex is bone 0 with
		// weight 0, which the skinning kernel reads as "no influence".
		const glm::uvec4 xBoneIndices = bHasBoneIndices ? xAsset.m_xBoneIndices.Get(uVertex) : glm::uvec4(0u, 0u, 0u, 0u);
		xVertex.m_auBoneIDs[0] = xBoneIndices.x;
		xVertex.m_auBoneIDs[1] = xBoneIndices.y;
		xVertex.m_auBoneIDs[2] = xBoneIndices.z;
		xVertex.m_auBoneIDs[3] = xBoneIndices.w;

		xVertex.m_xBoneWeights = bHasBoneWeights ? xAsset.m_xBoneWeights.Get(uVertex) : glm::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	}
}

Flux_MeshInstance* Flux_MeshInstance::CreateFromAsset(Zenith_MeshAsset* pxAsset)
{
	Zenith_Assert(pxAsset != nullptr, "Cannot create mesh instance from null asset");
	if (!pxAsset)
	{
		return nullptr;
	}

	const uint32_t uNumVerts = pxAsset->GetNumVerts();
	const uint32_t uNumIndices = pxAsset->GetNumIndices();

	if (uNumVerts == 0 || uNumIndices == 0)
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Cannot create mesh instance from empty asset");
		return nullptr;
	}

	Flux_MeshInstance* pxInstance = new Flux_MeshInstance();
	pxInstance->m_xSourceAsset.Set(pxAsset);
	pxInstance->m_uNumVerts = uNumVerts;
	pxInstance->m_uNumIndices = uNumIndices;

	// Build buffer layout matching the static mesh vertex stride (72 bytes):
	// Position (12) + UV (8) + Normal (12) + Tangent (12) + Bitangent (12) + Color (16)
	Flux_BufferLayout& xLayout = pxInstance->m_xBufferLayout;

	// Position - Vector3 (12 bytes)
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
	// UV - Vector2 (8 bytes)
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT2 });
	// Normal - Vector3 (12 bytes)
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
	// Tangent - Vector3 (12 bytes)
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
	// Bitangent - Vector3 (12 bytes)
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
	// Color - Vector4 (16 bytes)
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT4 });

	xLayout.CalculateOffsetsAndStrides();

	// Sanity check the stride
	Zenith_Assert(xLayout.GetStride() == 72, "Mesh instance vertex stride mismatch! Expected 72, got %u", xLayout.GetStride());

	// Generate interleaved vertex data
	// Each vertex is 72 bytes: pos(12) + uv(8) + normal(12) + tangent(12) + bitangent(12) + color(16)
	const size_t uVertexDataSize = static_cast<size_t>(uNumVerts) * xLayout.GetStride();
	uint8_t* pVertexData = new uint8_t[uVertexDataSize];

	// Pack the standard 72-byte static vertex, shaped by the shader-reflected layout.
	Flux_PackStaticMeshVertices(pVertexData, *pxAsset, uNumVerts);

	// Create GPU vertex buffer
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.InitialiseVertexBuffer(
		pVertexData,
		uVertexDataSize,
		pxInstance->m_xVertexBuffer
	);

	// Create GPU index buffer
	const size_t uIndexDataSize = static_cast<size_t>(uNumIndices) * sizeof(uint32_t);
	xVulkanMemory.InitialiseIndexBuffer(
		pxAsset->m_xIndices.GetDataPointer(),
		uIndexDataSize,
		pxInstance->m_xIndexBuffer
	);

	// Clean up temporary vertex data
	delete[] pVertexData;

	pxInstance->m_bInitialized = true;

	return pxInstance;
}

void Flux_MeshInstance::Destroy()
{
	if (m_bInitialized)
	{
		Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
		xVulkanMemory.DestroyVertexBuffer(m_xVertexBuffer);
		xVulkanMemory.DestroyIndexBuffer(m_xIndexBuffer);
		m_xBufferLayout.Reset();
		m_uNumVerts = 0;
		m_uNumIndices = 0;
		m_xSourceAsset.Clear();
		m_bInitialized = false;
	}
	// Procedural instances don't own any GPU resources, just clear the back-reference.
	m_pxProceduralGeometry = nullptr;
}

bool Flux_MeshInstance::HasSkinning() const
{
	if (Zenith_MeshAsset* pxSource = m_xSourceAsset.GetDirect())
	{
		return pxSource->HasSkinning();
	}
	return false;
}

const Zenith_AABB& Flux_MeshInstance::GetLocalBounds() const
{
	if (m_bLocalBoundsValid)
	{
		return m_xLocalBounds;
	}

	if (Zenith_MeshAsset* pxSource = m_xSourceAsset.GetDirect())
	{
		// Asset-backed: the bounds were baked at import time. A Zenith_MeshAsset that was
		// never baked defaults to a zero-volume point (min == max == 0,0,0) — which would
		// pass IsValid() and falsely cull the model the instant the entity origin leaves the
		// frustum. Treat a degenerate point as NO usable bounds (leave the default-invalid
		// box) so such an entity stays conservatively unculled.
		const Zenith_Maths::Vector3 xMin = pxSource->GetBoundsMin();
		const Zenith_Maths::Vector3 xMax = pxSource->GetBoundsMax();
		if (xMin != xMax)
		{
			m_xLocalBounds = Zenith_AABB(xMin, xMax);
		}
		else
		{
			m_xLocalBounds = Zenith_AABB();   // invalid -> "no bounds"
		}
	}
	else if (m_pxProceduralGeometry && m_pxProceduralGeometry->m_pxPositions && m_pxProceduralGeometry->GetNumVerts() > 0)
	{
		// Procedural: compute the tight AABB over the CPU positions.
		m_xLocalBounds = Zenith_FrustumCulling::GenerateAABBFromVertices(
			m_pxProceduralGeometry->m_pxPositions, m_pxProceduralGeometry->GetNumVerts());
	}
	else
	{
		// No source (empty/degenerate instance): leave the default-invalid AABB so a
		// frustum test treats it conservatively (an invalid AABB has min > max — callers
		// of the model union skip empty mesh bounds).
		m_xLocalBounds = Zenith_AABB();
	}

	m_bLocalBoundsValid = true;
	return m_xLocalBounds;
}

Flux_MeshInstance* Flux_MeshInstance::CreateSkinnedFromAsset(Zenith_MeshAsset* pxAsset)
{
	Zenith_Assert(pxAsset != nullptr, "Cannot create skinned mesh instance from null asset");
	if (!pxAsset)
	{
		return nullptr;
	}

	const uint32_t uNumVerts = pxAsset->GetNumVerts();
	const uint32_t uNumIndices = pxAsset->GetNumIndices();

	if (uNumVerts == 0 || uNumIndices == 0)
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Cannot create skinned mesh instance from empty asset");
		return nullptr;
	}

	if (!pxAsset->HasSkinning())
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Cannot create skinned mesh instance from asset without skinning data, use CreateFromAsset() instead");
		return nullptr;
	}

	// Debug logging to verify skinning data
	static bool s_bLoggedSkinningData = false;
	if (!s_bLoggedSkinningData)
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[MeshInstance] Creating skinned mesh: %u verts, %u indices", uNumVerts, uNumIndices);

		// Log first vertex's skinning data
		if (pxAsset->m_xBoneIndices.GetSize() > 0 && pxAsset->m_xBoneWeights.GetSize() > 0)
		{
			const glm::uvec4& xIdx = pxAsset->m_xBoneIndices.Get(0);
			const glm::vec4& xWgt = pxAsset->m_xBoneWeights.Get(0);
			Zenith_Log(LOG_CATEGORY_MESH, "[MeshInstance]   Vertex 0: BoneIdx=(%u,%u,%u,%u) Weights=(%.3f,%.3f,%.3f,%.3f)",
				xIdx.x, xIdx.y, xIdx.z, xIdx.w,
				xWgt.x, xWgt.y, xWgt.z, xWgt.w);
		}

		// Log first vertex position
		if (pxAsset->m_xPositions.GetSize() > 0)
		{
			const Zenith_Maths::Vector3& xPos = pxAsset->m_xPositions.Get(0);
			Zenith_Log(LOG_CATEGORY_MESH, "[MeshInstance]   Vertex 0: Position=(%.3f,%.3f,%.3f)", xPos.x, xPos.y, xPos.z);
		}

		s_bLoggedSkinningData = true;
	}

	Flux_MeshInstance* pxInstance = new Flux_MeshInstance();
	pxInstance->m_xSourceAsset.Set(pxAsset);
	pxInstance->m_uNumVerts = uNumVerts;
	pxInstance->m_uNumIndices = uNumIndices;

	// Build buffer layout for skinned mesh vertex stride (104 bytes):
	// Position (12) + UV (8) + Normal (12) + Tangent (12) + Bitangent (12) + Color (16) + BoneIndices (16) + BoneWeights (16)
	Flux_BufferLayout& xLayout = pxInstance->m_xBufferLayout;
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Position
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT2 }); // UV
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Normal
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Tangent
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Bitangent
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT4 }); // Color
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_UINT4 });  // BoneIndices
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT4 }); // BoneWeights
	xLayout.CalculateOffsetsAndStrides();

	Zenith_Assert(xLayout.GetStride() == 104, "Skinned mesh instance vertex stride mismatch! Expected 104, got %u", xLayout.GetStride());

	// Generate interleaved vertex data
	const size_t uVertexDataSize = static_cast<size_t>(uNumVerts) * xLayout.GetStride();
	uint8_t* pVertexData = new uint8_t[uVertexDataSize];

	// Build the 104-byte skin-INPUT vertex (bone ids are uint lanes, so this stream
	// is the Flux_Skinning.h contract's own builder, not the vertex packer).
	Flux_BuildSkinInputVertices(reinterpret_cast<Flux_SkinInputVertex*>(pVertexData), *pxAsset, uNumVerts);

	// Create GPU vertex buffer
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.InitialiseVertexBuffer(
		pVertexData,
		uVertexDataSize,
		pxInstance->m_xVertexBuffer
	);

	// Create GPU index buffer
	const size_t uIndexDataSize = static_cast<size_t>(uNumIndices) * sizeof(uint32_t);
	xVulkanMemory.InitialiseIndexBuffer(
		pxAsset->m_xIndices.GetDataPointer(),
		uIndexDataSize,
		pxInstance->m_xIndexBuffer
	);

	delete[] pVertexData;

	pxInstance->m_bInitialized = true;

	return pxInstance;
}

Flux_MeshInstance* Flux_MeshInstance::CreateFromAsset(Zenith_MeshAsset* pxAsset, Zenith_SkeletonAsset* pxSkeleton)
{
	Zenith_Assert(pxAsset != nullptr, "Cannot create mesh instance from null asset");
	if (!pxAsset)
	{
		return nullptr;
	}

	// If no skeleton or mesh doesn't have skinning, delegate to the simple version
	if (!pxSkeleton || !pxAsset->HasSkinning())
	{
		Zenith_Log(LOG_CATEGORY_MESH, "[MeshInstance] CreateFromAsset: No skeleton or no skinning, delegating to simple version");
		return CreateFromAsset(pxAsset);
	}

	Zenith_Log(LOG_CATEGORY_MESH, "[MeshInstance] CreateFromAsset with skeleton: %u bones, mesh has %u verts",
		pxSkeleton->GetNumBones(), pxAsset->GetNumVerts());

	const uint32_t uNumVerts = pxAsset->GetNumVerts();
	const uint32_t uNumIndices = pxAsset->GetNumIndices();

	if (uNumVerts == 0 || uNumIndices == 0)
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Cannot create mesh instance from empty asset");
		return nullptr;
	}

	Flux_MeshInstance* pxInstance = new Flux_MeshInstance();
	pxInstance->m_xSourceAsset.Set(pxAsset);
	pxInstance->m_uNumVerts = uNumVerts;
	pxInstance->m_uNumIndices = uNumIndices;

	// Build buffer layout matching the static mesh vertex stride (72 bytes)
	Flux_BufferLayout& xLayout = pxInstance->m_xBufferLayout;
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Position
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT2 }); // UV
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Normal
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Tangent
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 }); // Bitangent
	xLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT4 }); // Color
	xLayout.CalculateOffsetsAndStrides();

	Zenith_Assert(xLayout.GetStride() == 72, "Mesh instance vertex stride mismatch! Expected 72, got %u", xLayout.GetStride());

	// Generate interleaved vertex data with bind pose skinning applied
	// For skinned meshes, vertices are stored in mesh-local space (centered at origin).
	// We need to apply bind pose skinning to position them at their correct world locations.
	// skinningMatrix = boneBindPoseModel * inverseBindPose
	// At bind pose, this positions vertices at their bind pose world locations.

	const size_t uVertexDataSize = static_cast<size_t>(uNumVerts) * xLayout.GetStride();
	uint8_t* pVertexData = new uint8_t[uVertexDataSize];

	// What is SPECIAL about this path is the bind pose, and nothing else — so it
	// pre-skins the POSITIONS into its own stream and hands that to the shared
	// packer as the position source. Every other attribute (and the byte layout
	// itself) comes from exactly where the plain static path gets it.
	const bool bHasPositions = pxAsset->m_xPositions.GetSize() >= uNumVerts;
	const bool bHasSkinning = pxAsset->m_xBoneIndices.GetSize() >= uNumVerts &&
		pxAsset->m_xBoneWeights.GetSize() >= uNumVerts;

	Zenith_Vector<Zenith_Maths::Vector3> xBindPosePositions;
	xBindPosePositions.Reserve(uNumVerts);
	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		// An absent position array reads as the canonical origin — the same value the
		// packer's own default would have written.
		Zenith_Maths::Vector3 xPos = bHasPositions
			? pxAsset->m_xPositions.Get(i)
			: CanonicalDefault3(FLUX_VERTEX_SEMANTIC_POSITION);

		// Apply bind pose skinning to position the vertex at its correct world location:
		// this transforms vertices from mesh-local space to their bind pose world positions.
		if (bHasSkinning)
		{
			xPos = ApplyBindPoseSkinning(xPos, pxAsset->m_xBoneIndices.Get(i), pxAsset->m_xBoneWeights.Get(i), pxSkeleton);
		}
		xBindPosePositions.PushBack(xPos);
	}

	// Normals/tangents/bitangents are NOT bind-pose transformed (pre-existing
	// behaviour, unchanged here): they pass through as authored.
	Flux_PackStaticMeshVertices(pVertexData, *pxAsset, uNumVerts, xBindPosePositions.GetDataPointer());

	// Create GPU vertex buffer
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.InitialiseVertexBuffer(
		pVertexData,
		uVertexDataSize,
		pxInstance->m_xVertexBuffer
	);

	// Create GPU index buffer
	const size_t uIndexDataSize = static_cast<size_t>(uNumIndices) * sizeof(uint32_t);
	xVulkanMemory.InitialiseIndexBuffer(
		pxAsset->m_xIndices.GetDataPointer(),
		uIndexDataSize,
		pxInstance->m_xIndexBuffer
	);

	delete[] pVertexData;

	pxInstance->m_bInitialized = true;

	return pxInstance;
}

// Stage-0b mesh-geometry registry tests, hosted in this always-linked TU so the
// test-body calls pull the otherwise-inert Flux_MeshGeometryRegistry.obj into the
// link (same pattern + rationale as the bottom of Flux_GPUScene.cpp).
#include "Flux/Flux_MeshGeometryRegistry.Tests.inl"

// Mesh-family vertex-stream tests — hosted here (the same TU as both writers) so
// the always-linked .obj carries them. They memcmp Flux_PackStaticMeshVertices and
// Flux_BuildSkinInputVertices against a frozen transcription of the interleave loop
// they replaced, which is what pins the 72/104-byte bytes and the attribute defaults.
#include "Flux/MeshGeometry/Flux_VertexInterleave.Tests.inl"
