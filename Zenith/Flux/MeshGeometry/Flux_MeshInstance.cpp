#include "Zenith.h"
#include "Flux_MeshInstance.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "Flux/Flux.h"


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
	, m_pxSourceAsset(xOther.m_pxSourceAsset)
	, m_bInitialized(xOther.m_bInitialized)
{
	xOther.m_uNumVerts = 0;
	xOther.m_uNumIndices = 0;
	xOther.m_pxSourceAsset = nullptr;
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
		m_pxSourceAsset = xOther.m_pxSourceAsset;
		m_bInitialized = xOther.m_bInitialized;

		xOther.m_uNumVerts = 0;
		xOther.m_uNumIndices = 0;
		xOther.m_pxSourceAsset = nullptr;
		xOther.m_bInitialized = false;
	}
	return *this;
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
	pxInstance->m_pxSourceAsset = pxAsset;
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

	// Check which attributes are available from the asset
	const bool bHasPositions = pxAsset->m_xPositions.GetSize() >= uNumVerts;
	const bool bHasUVs = pxAsset->m_xUVs.GetSize() >= uNumVerts;
	const bool bHasNormals = pxAsset->m_xNormals.GetSize() >= uNumVerts;
	const bool bHasTangents = pxAsset->m_xTangents.GetSize() >= uNumVerts;
	const bool bHasBitangents = pxAsset->m_xBitangents.GetSize() >= uNumVerts;
	const bool bHasColors = pxAsset->m_xColors.GetSize() >= uNumVerts;

	// Default values for missing attributes
	const Zenith_Maths::Vector3 xDefaultPosition(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector2 xDefaultUV(0.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultNormal(0.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultTangent(1.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultBitangent(0.0f, 0.0f, 1.0f);
	const Zenith_Maths::Vector4 xDefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

	// Interleave vertex data
	size_t uFloatIndex = 0;
	float* pFloatData = reinterpret_cast<float*>(pVertexData);

	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		// Position (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xPos = bHasPositions ? pxAsset->m_xPositions.Get(i) : xDefaultPosition;
		pFloatData[uFloatIndex++] = xPos.x;
		pFloatData[uFloatIndex++] = xPos.y;
		pFloatData[uFloatIndex++] = xPos.z;

		// UV (2 floats = 8 bytes)
		const Zenith_Maths::Vector2& xUV = bHasUVs ? pxAsset->m_xUVs.Get(i) : xDefaultUV;
		pFloatData[uFloatIndex++] = xUV.x;
		pFloatData[uFloatIndex++] = xUV.y;

		// Normal (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xNormal = bHasNormals ? pxAsset->m_xNormals.Get(i) : xDefaultNormal;
		pFloatData[uFloatIndex++] = xNormal.x;
		pFloatData[uFloatIndex++] = xNormal.y;
		pFloatData[uFloatIndex++] = xNormal.z;

		// Tangent (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xTangent = bHasTangents ? pxAsset->m_xTangents.Get(i) : xDefaultTangent;
		pFloatData[uFloatIndex++] = xTangent.x;
		pFloatData[uFloatIndex++] = xTangent.y;
		pFloatData[uFloatIndex++] = xTangent.z;

		// Bitangent (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xBitangent = bHasBitangents ? pxAsset->m_xBitangents.Get(i) : xDefaultBitangent;
		pFloatData[uFloatIndex++] = xBitangent.x;
		pFloatData[uFloatIndex++] = xBitangent.y;
		pFloatData[uFloatIndex++] = xBitangent.z;

		// Color (4 floats = 16 bytes)
		const Zenith_Maths::Vector4& xColor = bHasColors ? pxAsset->m_xColors.Get(i) : xDefaultColor;
		pFloatData[uFloatIndex++] = xColor.x;
		pFloatData[uFloatIndex++] = xColor.y;
		pFloatData[uFloatIndex++] = xColor.z;
		pFloatData[uFloatIndex++] = xColor.w;
	}

	// Create GPU vertex buffer
	Flux_MemoryManager::InitialiseVertexBuffer(
		pVertexData,
		uVertexDataSize,
		pxInstance->m_xVertexBuffer
	);

	// Create GPU index buffer
	const size_t uIndexDataSize = static_cast<size_t>(uNumIndices) * sizeof(uint32_t);
	Flux_MemoryManager::InitialiseIndexBuffer(
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
		Flux_MemoryManager::DestroyVertexBuffer(m_xVertexBuffer);
		Flux_MemoryManager::DestroyIndexBuffer(m_xIndexBuffer);
		m_xBufferLayout.Reset();
		m_uNumVerts = 0;
		m_uNumIndices = 0;
		m_pxSourceAsset = nullptr;
		m_bInitialized = false;
	}
}

bool Flux_MeshInstance::HasSkinning() const
{
	if (m_pxSourceAsset)
	{
		return m_pxSourceAsset->HasSkinning();
	}
	return false;
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
	pxInstance->m_pxSourceAsset = pxAsset;
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

	// Check which attributes are available
	const bool bHasPositions = pxAsset->m_xPositions.GetSize() >= uNumVerts;
	const bool bHasUVs = pxAsset->m_xUVs.GetSize() >= uNumVerts;
	const bool bHasNormals = pxAsset->m_xNormals.GetSize() >= uNumVerts;
	const bool bHasTangents = pxAsset->m_xTangents.GetSize() >= uNumVerts;
	const bool bHasBitangents = pxAsset->m_xBitangents.GetSize() >= uNumVerts;
	const bool bHasColors = pxAsset->m_xColors.GetSize() >= uNumVerts;
	const bool bHasBoneIndices = pxAsset->m_xBoneIndices.GetSize() >= uNumVerts;
	const bool bHasBoneWeights = pxAsset->m_xBoneWeights.GetSize() >= uNumVerts;

	// Default values
	const Zenith_Maths::Vector3 xDefaultPosition(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector2 xDefaultUV(0.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultNormal(0.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultTangent(1.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultBitangent(0.0f, 0.0f, 1.0f);
	const Zenith_Maths::Vector4 xDefaultColor(1.0f, 1.0f, 1.0f, 1.0f);
	const glm::uvec4 xDefaultBoneIndices(0, 0, 0, 0);
	const glm::vec4 xDefaultBoneWeights(0.0f, 0.0f, 0.0f, 0.0f);

	// Interleave vertex data
	uint8_t* pCurrentVertex = pVertexData;

	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		float* pFloatData = reinterpret_cast<float*>(pCurrentVertex);
		size_t uFloatIndex = 0;

		// Position (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xPos = bHasPositions ? pxAsset->m_xPositions.Get(i) : xDefaultPosition;
		pFloatData[uFloatIndex++] = xPos.x;
		pFloatData[uFloatIndex++] = xPos.y;
		pFloatData[uFloatIndex++] = xPos.z;

		// UV (2 floats = 8 bytes)
		const Zenith_Maths::Vector2& xUV = bHasUVs ? pxAsset->m_xUVs.Get(i) : xDefaultUV;
		pFloatData[uFloatIndex++] = xUV.x;
		pFloatData[uFloatIndex++] = xUV.y;

		// Normal (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xNormal = bHasNormals ? pxAsset->m_xNormals.Get(i) : xDefaultNormal;
		pFloatData[uFloatIndex++] = xNormal.x;
		pFloatData[uFloatIndex++] = xNormal.y;
		pFloatData[uFloatIndex++] = xNormal.z;

		// Tangent (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xTangent = bHasTangents ? pxAsset->m_xTangents.Get(i) : xDefaultTangent;
		pFloatData[uFloatIndex++] = xTangent.x;
		pFloatData[uFloatIndex++] = xTangent.y;
		pFloatData[uFloatIndex++] = xTangent.z;

		// Bitangent (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xBitangent = bHasBitangents ? pxAsset->m_xBitangents.Get(i) : xDefaultBitangent;
		pFloatData[uFloatIndex++] = xBitangent.x;
		pFloatData[uFloatIndex++] = xBitangent.y;
		pFloatData[uFloatIndex++] = xBitangent.z;

		// Color (4 floats = 16 bytes)
		const Zenith_Maths::Vector4& xColor = bHasColors ? pxAsset->m_xColors.Get(i) : xDefaultColor;
		pFloatData[uFloatIndex++] = xColor.x;
		pFloatData[uFloatIndex++] = xColor.y;
		pFloatData[uFloatIndex++] = xColor.z;
		pFloatData[uFloatIndex++] = xColor.w;

		// BoneIndices (4 uints = 16 bytes) at offset 72
		const glm::uvec4& xBoneIndices = bHasBoneIndices ? pxAsset->m_xBoneIndices.Get(i) : xDefaultBoneIndices;
		uint32_t* pUintData = reinterpret_cast<uint32_t*>(pCurrentVertex + 72);
		pUintData[0] = xBoneIndices.x;
		pUintData[1] = xBoneIndices.y;
		pUintData[2] = xBoneIndices.z;
		pUintData[3] = xBoneIndices.w;

		// BoneWeights (4 floats = 16 bytes) at offset 88
		const glm::vec4& xBoneWeights = bHasBoneWeights ? pxAsset->m_xBoneWeights.Get(i) : xDefaultBoneWeights;
		float* pWeightData = reinterpret_cast<float*>(pCurrentVertex + 88);
		pWeightData[0] = xBoneWeights.x;
		pWeightData[1] = xBoneWeights.y;
		pWeightData[2] = xBoneWeights.z;
		pWeightData[3] = xBoneWeights.w;

		pCurrentVertex += 104;
	}

	// Create GPU vertex buffer
	Flux_MemoryManager::InitialiseVertexBuffer(
		pVertexData,
		uVertexDataSize,
		pxInstance->m_xVertexBuffer
	);

	// Create GPU index buffer
	const size_t uIndexDataSize = static_cast<size_t>(uNumIndices) * sizeof(uint32_t);
	Flux_MemoryManager::InitialiseIndexBuffer(
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
	pxInstance->m_pxSourceAsset = pxAsset;
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

	// Check which attributes are available
	const bool bHasPositions = pxAsset->m_xPositions.GetSize() >= uNumVerts;
	const bool bHasUVs = pxAsset->m_xUVs.GetSize() >= uNumVerts;
	const bool bHasNormals = pxAsset->m_xNormals.GetSize() >= uNumVerts;
	const bool bHasTangents = pxAsset->m_xTangents.GetSize() >= uNumVerts;
	const bool bHasBitangents = pxAsset->m_xBitangents.GetSize() >= uNumVerts;
	const bool bHasColors = pxAsset->m_xColors.GetSize() >= uNumVerts;

	// Default values
	const Zenith_Maths::Vector3 xDefaultPosition(0.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector2 xDefaultUV(0.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultNormal(0.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultTangent(1.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xDefaultBitangent(0.0f, 0.0f, 1.0f);
	const Zenith_Maths::Vector4 xDefaultColor(1.0f, 1.0f, 1.0f, 1.0f);

	// Check if mesh has skinning data
	const bool bHasSkinning = pxAsset->m_xBoneIndices.GetSize() >= uNumVerts &&
		pxAsset->m_xBoneWeights.GetSize() >= uNumVerts;

	// Interleave vertex data with bind pose skinning
	size_t uFloatIndex = 0;
	float* pFloatData = reinterpret_cast<float*>(pVertexData);

	for (uint32_t i = 0; i < uNumVerts; i++)
	{
		// Get original position from mesh asset
		const Zenith_Maths::Vector3& xOriginalPos = bHasPositions ? pxAsset->m_xPositions.Get(i) : xDefaultPosition;

		// Apply bind pose skinning to position the vertex at its correct world location
		// This transforms vertices from mesh-local space to their bind pose world positions
		Zenith_Maths::Vector3 xSkinnedPos = xOriginalPos;
		if (bHasSkinning)
		{
			const glm::uvec4& xBoneIndices = pxAsset->m_xBoneIndices.Get(i);
			const glm::vec4& xBoneWeights = pxAsset->m_xBoneWeights.Get(i);
			xSkinnedPos = ApplyBindPoseSkinning(xOriginalPos, xBoneIndices, xBoneWeights, pxSkeleton);
		}

		// Position (3 floats = 12 bytes)
		pFloatData[uFloatIndex++] = xSkinnedPos.x;
		pFloatData[uFloatIndex++] = xSkinnedPos.y;
		pFloatData[uFloatIndex++] = xSkinnedPos.z;

		// UV (2 floats = 8 bytes)
		const Zenith_Maths::Vector2& xUV = bHasUVs ? pxAsset->m_xUVs.Get(i) : xDefaultUV;
		pFloatData[uFloatIndex++] = xUV.x;
		pFloatData[uFloatIndex++] = xUV.y;

		// Normal (3 floats = 12 bytes) - should also be transformed but for now leave as-is
		const Zenith_Maths::Vector3& xNormal = bHasNormals ? pxAsset->m_xNormals.Get(i) : xDefaultNormal;
		pFloatData[uFloatIndex++] = xNormal.x;
		pFloatData[uFloatIndex++] = xNormal.y;
		pFloatData[uFloatIndex++] = xNormal.z;

		// Tangent (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xTangent = bHasTangents ? pxAsset->m_xTangents.Get(i) : xDefaultTangent;
		pFloatData[uFloatIndex++] = xTangent.x;
		pFloatData[uFloatIndex++] = xTangent.y;
		pFloatData[uFloatIndex++] = xTangent.z;

		// Bitangent (3 floats = 12 bytes)
		const Zenith_Maths::Vector3& xBitangent = bHasBitangents ? pxAsset->m_xBitangents.Get(i) : xDefaultBitangent;
		pFloatData[uFloatIndex++] = xBitangent.x;
		pFloatData[uFloatIndex++] = xBitangent.y;
		pFloatData[uFloatIndex++] = xBitangent.z;

		// Color (4 floats = 16 bytes)
		const Zenith_Maths::Vector4& xColor = bHasColors ? pxAsset->m_xColors.Get(i) : xDefaultColor;
		pFloatData[uFloatIndex++] = xColor.x;
		pFloatData[uFloatIndex++] = xColor.y;
		pFloatData[uFloatIndex++] = xColor.z;
		pFloatData[uFloatIndex++] = xColor.w;
	}

	// Create GPU vertex buffer
	Flux_MemoryManager::InitialiseVertexBuffer(
		pVertexData,
		uVertexDataSize,
		pxInstance->m_xVertexBuffer
	);

	// Create GPU index buffer
	const size_t uIndexDataSize = static_cast<size_t>(uNumIndices) * sizeof(uint32_t);
	Flux_MemoryManager::InitialiseIndexBuffer(
		pxAsset->m_xIndices.GetDataPointer(),
		uIndexDataSize,
		pxInstance->m_xIndexBuffer
	);

	delete[] pVertexData;

	pxInstance->m_bInitialized = true;

	return pxInstance;
}
