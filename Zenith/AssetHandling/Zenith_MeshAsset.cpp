#include "Zenith.h"
#include "AssetHandling/Zenith_MeshAsset.h"

//------------------------------------------------------------------------------
// Helper Functions for Serialization
//------------------------------------------------------------------------------

// Helper template for writing vertex arrays
template<typename T>
static void WriteVertexArray(Zenith_DataStream& xStream, const Zenith_Vector<T>& xArray, const char* /*szName*/)
{
	bool bHasData = xArray.GetSize() > 0;
	xStream << bHasData;
	if (bHasData)
	{
		xStream.WriteData(xArray.GetDataPointer(), xArray.GetSize() * sizeof(T));
	}
}

// Helper template for reading vertex arrays
template<typename T>
static void ReadVertexArray(Zenith_DataStream& xStream, Zenith_Vector<T>& xArray, uint32_t uCount)
{
	bool bHasData;
	xStream >> bHasData;
	if (bHasData)
	{
		xArray.Reserve(uCount);
		for (uint32_t u = 0; u < uCount; u++)
		{
			T xValue;
			xStream.ReadData(&xValue, sizeof(xValue));
			xArray.PushBack(xValue);
		}
	}
}

//------------------------------------------------------------------------------
// Submesh Serialization
//------------------------------------------------------------------------------

void Zenith_MeshAsset::Submesh::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_uStartIndex;
	xStream << m_uIndexCount;
	xStream << m_uMaterialIndex;
}

void Zenith_MeshAsset::Submesh::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_uStartIndex;
	xStream >> m_uIndexCount;
	xStream >> m_uMaterialIndex;
}

//------------------------------------------------------------------------------
// Constructor / Destructor
//------------------------------------------------------------------------------

Zenith_MeshAsset::~Zenith_MeshAsset()
{
	Reset();
}

Zenith_MeshAsset::Zenith_MeshAsset(Zenith_MeshAsset&& xOther)
{
	m_xPositions = std::move(xOther.m_xPositions);
	m_xNormals = std::move(xOther.m_xNormals);
	m_xUVs = std::move(xOther.m_xUVs);
	m_xTangents = std::move(xOther.m_xTangents);
	m_xBitangents = std::move(xOther.m_xBitangents);
	m_xColors = std::move(xOther.m_xColors);
	m_xIndices = std::move(xOther.m_xIndices);
	m_xSubmeshes = std::move(xOther.m_xSubmeshes);
	m_strSkeletonPath = std::move(xOther.m_strSkeletonPath);
	m_xBoneIndices = std::move(xOther.m_xBoneIndices);
	m_xBoneWeights = std::move(xOther.m_xBoneWeights);
	m_xBoundsMin = xOther.m_xBoundsMin;
	m_xBoundsMax = xOther.m_xBoundsMax;
	m_strSourcePath = std::move(xOther.m_strSourcePath);
	m_xMaterialColor = xOther.m_xMaterialColor;
	m_uNumVerts = xOther.m_uNumVerts;
	m_uNumIndices = xOther.m_uNumIndices;

	xOther.m_uNumVerts = 0;
	xOther.m_uNumIndices = 0;
}

Zenith_MeshAsset& Zenith_MeshAsset::operator=(Zenith_MeshAsset&& xOther)
{
	if (this != &xOther)
	{
		Reset();

		m_xPositions = std::move(xOther.m_xPositions);
		m_xNormals = std::move(xOther.m_xNormals);
		m_xUVs = std::move(xOther.m_xUVs);
		m_xTangents = std::move(xOther.m_xTangents);
		m_xBitangents = std::move(xOther.m_xBitangents);
		m_xColors = std::move(xOther.m_xColors);
		m_xIndices = std::move(xOther.m_xIndices);
		m_xSubmeshes = std::move(xOther.m_xSubmeshes);
		m_strSkeletonPath = std::move(xOther.m_strSkeletonPath);
		m_xBoneIndices = std::move(xOther.m_xBoneIndices);
		m_xBoneWeights = std::move(xOther.m_xBoneWeights);
		m_xBoundsMin = xOther.m_xBoundsMin;
		m_xBoundsMax = xOther.m_xBoundsMax;
		m_strSourcePath = std::move(xOther.m_strSourcePath);
		m_xMaterialColor = xOther.m_xMaterialColor;
		m_uNumVerts = xOther.m_uNumVerts;
		m_uNumIndices = xOther.m_uNumIndices;

		xOther.m_uNumVerts = 0;
		xOther.m_uNumIndices = 0;
	}
	return *this;
}

//------------------------------------------------------------------------------
// Loading and Saving
//------------------------------------------------------------------------------

Zenith_MeshAsset* Zenith_MeshAsset::LoadFromFile(const char* szPath)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	// Validate file was loaded successfully
	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_MESH, "LoadFromFile: Failed to read file '%s'", szPath);
		return nullptr;
	}

	Zenith_MeshAsset* pxAsset = new Zenith_MeshAsset();
	pxAsset->ReadFromDataStream(xStream);
	pxAsset->m_strSourcePath = szPath;

	// Debug: Log mesh bounds and first few vertex positions
	Zenith_Log(LOG_CATEGORY_MESH, "Loaded %s: %u verts, bounds=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)",
		szPath, pxAsset->GetNumVerts(),
		pxAsset->m_xBoundsMin.x, pxAsset->m_xBoundsMin.y, pxAsset->m_xBoundsMin.z,
		pxAsset->m_xBoundsMax.x, pxAsset->m_xBoundsMax.y, pxAsset->m_xBoundsMax.z);

	// Log first 3 vertex positions to see if they're at different locations
	for (uint32_t i = 0; i < 3 && i < pxAsset->m_xPositions.GetSize(); i++)
	{
		const Zenith_Maths::Vector3& xPos = pxAsset->m_xPositions.Get(i);
		Zenith_Log(LOG_CATEGORY_MESH, "  Vertex %u: pos=(%.3f, %.3f, %.3f)", i, xPos.x, xPos.y, xPos.z);
	}

	return pxAsset;
}

void Zenith_MeshAsset::Export(const char* szPath) const
{
	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(szPath);
}

void Zenith_MeshAsset::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Version
	xStream << static_cast<uint32_t>(ZENITH_MESH_ASSET_VERSION);

	// Counts
	xStream << m_uNumVerts;
	xStream << m_uNumIndices;

	// Submeshes
	uint32_t uNumSubmeshes = static_cast<uint32_t>(m_xSubmeshes.GetSize());
	xStream << uNumSubmeshes;
	for (uint32_t u = 0; u < uNumSubmeshes; u++)
	{
		m_xSubmeshes.Get(u).WriteToDataStream(xStream);
	}

	// Bounds
	xStream << m_xBoundsMin.x;
	xStream << m_xBoundsMin.y;
	xStream << m_xBoundsMin.z;
	xStream << m_xBoundsMax.x;
	xStream << m_xBoundsMax.y;
	xStream << m_xBoundsMax.z;

	// Material color
	xStream << m_xMaterialColor.x;
	xStream << m_xMaterialColor.y;
	xStream << m_xMaterialColor.z;
	xStream << m_xMaterialColor.w;

	// Skinning info
	bool bHasSkinning = HasSkinning();
	xStream << bHasSkinning;
	if (bHasSkinning)
	{
		xStream << m_strSkeletonPath;
	}

	// Vertex data - write presence flags and data
	WriteVertexArray(xStream, m_xPositions, "Positions");
	WriteVertexArray(xStream, m_xNormals, "Normals");
	WriteVertexArray(xStream, m_xUVs, "UVs");
	WriteVertexArray(xStream, m_xTangents, "Tangents");
	WriteVertexArray(xStream, m_xBitangents, "Bitangents");
	WriteVertexArray(xStream, m_xColors, "Colors");

	// Indices
	xStream.WriteData(m_xIndices.GetDataPointer(), m_uNumIndices * sizeof(uint32_t));

	// Skinning data
	if (bHasSkinning)
	{
		xStream.WriteData(m_xBoneIndices.GetDataPointer(), m_uNumVerts * sizeof(glm::uvec4));
		xStream.WriteData(m_xBoneWeights.GetDataPointer(), m_uNumVerts * sizeof(glm::vec4));
	}
}

void Zenith_MeshAsset::ReadFromDataStream(Zenith_DataStream& xStream)
{
	Reset();

	// Version
	uint32_t uVersion;
	xStream >> uVersion;

	if (uVersion != ZENITH_MESH_ASSET_VERSION)
	{
		Zenith_Log(LOG_CATEGORY_MESH, "Version mismatch: expected %u, got %u", ZENITH_MESH_ASSET_VERSION, uVersion);
	}

	// Counts
	xStream >> m_uNumVerts;
	xStream >> m_uNumIndices;

	// Submeshes
	uint32_t uNumSubmeshes;
	xStream >> uNumSubmeshes;
	for (uint32_t u = 0; u < uNumSubmeshes; u++)
	{
		Submesh xSubmesh;
		xSubmesh.ReadFromDataStream(xStream);
		m_xSubmeshes.PushBack(xSubmesh);
	}

	// Bounds
	xStream >> m_xBoundsMin.x;
	xStream >> m_xBoundsMin.y;
	xStream >> m_xBoundsMin.z;
	xStream >> m_xBoundsMax.x;
	xStream >> m_xBoundsMax.y;
	xStream >> m_xBoundsMax.z;

	// Material color
	xStream >> m_xMaterialColor.x;
	xStream >> m_xMaterialColor.y;
	xStream >> m_xMaterialColor.z;
	xStream >> m_xMaterialColor.w;

	// Skinning info
	bool bHasSkinning;
	xStream >> bHasSkinning;
	if (bHasSkinning)
	{
		xStream >> m_strSkeletonPath;
	}

	// Read vertex arrays
	ReadVertexArray(xStream, m_xPositions, m_uNumVerts);
	ReadVertexArray(xStream, m_xNormals, m_uNumVerts);
	ReadVertexArray(xStream, m_xUVs, m_uNumVerts);
	ReadVertexArray(xStream, m_xTangents, m_uNumVerts);
	ReadVertexArray(xStream, m_xBitangents, m_uNumVerts);
	ReadVertexArray(xStream, m_xColors, m_uNumVerts);

	// Indices
	m_xIndices.Reserve(m_uNumIndices);
	for (uint32_t u = 0; u < m_uNumIndices; u++)
	{
		uint32_t uIndex;
		xStream.ReadData(&uIndex, sizeof(uIndex));
		m_xIndices.PushBack(uIndex);
	}

	// Skinning data - read as blocks (matching WriteToDataStream format)
	if (bHasSkinning)
	{
		// Read all bone indices first
		m_xBoneIndices.Reserve(m_uNumVerts);
		for (uint32_t u = 0; u < m_uNumVerts; u++)
		{
			glm::uvec4 xBoneIdx;
			xStream.ReadData(&xBoneIdx, sizeof(xBoneIdx));
			m_xBoneIndices.PushBack(xBoneIdx);
		}
		// Then read all bone weights
		m_xBoneWeights.Reserve(m_uNumVerts);
		for (uint32_t u = 0; u < m_uNumVerts; u++)
		{
			glm::vec4 xBoneWeight;
			xStream.ReadData(&xBoneWeight, sizeof(xBoneWeight));
			m_xBoneWeights.PushBack(xBoneWeight);
		}
	}
}

//------------------------------------------------------------------------------
// Mesh Building
//------------------------------------------------------------------------------

void Zenith_MeshAsset::Reserve(uint32_t uNumVerts, uint32_t uNumIndices)
{
	m_xPositions.Reserve(uNumVerts);
	m_xNormals.Reserve(uNumVerts);
	m_xUVs.Reserve(uNumVerts);
	m_xTangents.Reserve(uNumVerts);
	m_xBitangents.Reserve(uNumVerts);
	m_xColors.Reserve(uNumVerts);
	m_xIndices.Reserve(uNumIndices);
}

void Zenith_MeshAsset::AddVertex(
	const Zenith_Maths::Vector3& xPosition,
	const Zenith_Maths::Vector3& xNormal,
	const Zenith_Maths::Vector2& xUV,
	const Zenith_Maths::Vector3& xTangent,
	const Zenith_Maths::Vector4& xColor)
{
	m_xPositions.PushBack(xPosition);
	m_xNormals.PushBack(xNormal);
	m_xUVs.PushBack(xUV);
	m_xTangents.PushBack(xTangent);
	m_xColors.PushBack(xColor);
	m_uNumVerts++;
}

void Zenith_MeshAsset::AddTriangle(uint32_t uA, uint32_t uB, uint32_t uC)
{
	m_xIndices.PushBack(uA);
	m_xIndices.PushBack(uB);
	m_xIndices.PushBack(uC);
	m_uNumIndices += 3;
}

void Zenith_MeshAsset::AddSubmesh(uint32_t uStartIndex, uint32_t uIndexCount, uint32_t uMaterialIndex)
{
	Submesh xSubmesh;
	xSubmesh.m_uStartIndex = uStartIndex;
	xSubmesh.m_uIndexCount = uIndexCount;
	xSubmesh.m_uMaterialIndex = uMaterialIndex;
	m_xSubmeshes.PushBack(xSubmesh);
}

void Zenith_MeshAsset::SetVertexSkinning(
	uint32_t uVertexIndex,
	const glm::uvec4& xBoneIndices,
	const glm::vec4& xBoneWeights)
{
	// Ensure skinning arrays are sized correctly
	while (m_xBoneIndices.GetSize() <= uVertexIndex)
	{
		m_xBoneIndices.PushBack(glm::uvec4(0));
		m_xBoneWeights.PushBack(glm::vec4(0.0f));
	}

	m_xBoneIndices.Get(uVertexIndex) = xBoneIndices;
	m_xBoneWeights.Get(uVertexIndex) = xBoneWeights;
}

void Zenith_MeshAsset::ComputeBounds()
{
	if (m_xPositions.GetSize() == 0)
	{
		m_xBoundsMin = Zenith_Maths::Vector3(0);
		m_xBoundsMax = Zenith_Maths::Vector3(0);
		return;
	}

	m_xBoundsMin = m_xPositions.Get(0);
	m_xBoundsMax = m_xPositions.Get(0);

	for (uint32_t u = 1; u < m_xPositions.GetSize(); u++)
	{
		const Zenith_Maths::Vector3& xPos = m_xPositions.Get(u);
		m_xBoundsMin = glm::min(m_xBoundsMin, xPos);
		m_xBoundsMax = glm::max(m_xBoundsMax, xPos);
	}
}

void Zenith_MeshAsset::GenerateNormals()
{
	// Clear existing normals
	m_xNormals.Clear();
	m_xNormals.Reserve(m_uNumVerts);
	for (uint32_t u = 0; u < m_uNumVerts; u++)
	{
		m_xNormals.PushBack(Zenith_Maths::Vector3(0));
	}

	// Accumulate face normals
	for (uint32_t u = 0; u < m_uNumIndices / 3; u++)
	{
		uint32_t uA = m_xIndices.Get(u * 3 + 0);
		uint32_t uB = m_xIndices.Get(u * 3 + 1);
		uint32_t uC = m_xIndices.Get(u * 3 + 2);

		Zenith_Maths::Vector3 xPosA = m_xPositions.Get(uA);
		Zenith_Maths::Vector3 xPosB = m_xPositions.Get(uB);
		Zenith_Maths::Vector3 xPosC = m_xPositions.Get(uC);

		Zenith_Maths::Vector3 xNormal = glm::cross(xPosB - xPosA, xPosC - xPosA);
		m_xNormals.Get(uA) += xNormal;
		m_xNormals.Get(uB) += xNormal;
		m_xNormals.Get(uC) += xNormal;
	}

	// Normalize
	for (uint32_t u = 0; u < m_uNumVerts; u++)
	{
		Zenith_Maths::Vector3& xNormal = m_xNormals.Get(u);
		float fLength = glm::length(xNormal);
		if (fLength > 0.0001f)
		{
			xNormal = glm::normalize(xNormal);
		}
	}
}

void Zenith_MeshAsset::GenerateTangents()
{
	// Clear existing tangents
	m_xTangents.Clear();
	m_xBitangents.Clear();
	m_xTangents.Reserve(m_uNumVerts);
	m_xBitangents.Reserve(m_uNumVerts);
	for (uint32_t u = 0; u < m_uNumVerts; u++)
	{
		m_xTangents.PushBack(Zenith_Maths::Vector3(0));
		m_xBitangents.PushBack(Zenith_Maths::Vector3(0));
	}

	// Accumulate tangents per face
	for (uint32_t u = 0; u < m_uNumIndices / 3; u++)
	{
		uint32_t uA = m_xIndices.Get(u * 3 + 0);
		uint32_t uB = m_xIndices.Get(u * 3 + 1);
		uint32_t uC = m_xIndices.Get(u * 3 + 2);

		Zenith_Maths::Vector3 xPosA = m_xPositions.Get(uA);
		Zenith_Maths::Vector3 xPosB = m_xPositions.Get(uB);
		Zenith_Maths::Vector3 xPosC = m_xPositions.Get(uC);

		Zenith_Maths::Vector2 xUVA = m_xUVs.Get(uA);
		Zenith_Maths::Vector2 xUVB = m_xUVs.Get(uB);
		Zenith_Maths::Vector2 xUVC = m_xUVs.Get(uC);

		Zenith_Maths::Vector3 xEdge1 = xPosB - xPosA;
		Zenith_Maths::Vector3 xEdge2 = xPosC - xPosA;
		Zenith_Maths::Vector2 xDeltaUV1 = xUVB - xUVA;
		Zenith_Maths::Vector2 xDeltaUV2 = xUVC - xUVA;

		float fDet = xDeltaUV1.x * xDeltaUV2.y - xDeltaUV2.x * xDeltaUV1.y;
		if (std::fabsf(fDet) < 0.0001f)
		{
			continue;
		}

		float fInvDet = 1.0f / fDet;
		Zenith_Maths::Vector3 xTangent = fInvDet * (xDeltaUV2.y * xEdge1 - xDeltaUV1.y * xEdge2);
		Zenith_Maths::Vector3 xBitangent = fInvDet * (-xDeltaUV2.x * xEdge1 + xDeltaUV1.x * xEdge2);

		m_xTangents.Get(uA) += xTangent;
		m_xTangents.Get(uB) += xTangent;
		m_xTangents.Get(uC) += xTangent;

		m_xBitangents.Get(uA) += xBitangent;
		m_xBitangents.Get(uB) += xBitangent;
		m_xBitangents.Get(uC) += xBitangent;
	}

	// Normalize and orthogonalize
	for (uint32_t u = 0; u < m_uNumVerts; u++)
	{
		Zenith_Maths::Vector3& xTangent = m_xTangents.Get(u);
		Zenith_Maths::Vector3& xBitangent = m_xBitangents.Get(u);
		const Zenith_Maths::Vector3& xNormal = m_xNormals.Get(u);

		// Gram-Schmidt orthogonalize
		xTangent = glm::normalize(xTangent - xNormal * glm::dot(xNormal, xTangent));

		// Compute bitangent from normal and tangent
		xBitangent = glm::cross(xNormal, xTangent);
	}
}

void Zenith_MeshAsset::Reset()
{
	m_xPositions.Clear();
	m_xNormals.Clear();
	m_xUVs.Clear();
	m_xTangents.Clear();
	m_xBitangents.Clear();
	m_xColors.Clear();
	m_xIndices.Clear();
	m_xSubmeshes.Clear();
	m_strSkeletonPath.clear();
	m_xBoneIndices.Clear();
	m_xBoneWeights.Clear();
	m_xBoundsMin = Zenith_Maths::Vector3(0);
	m_xBoundsMax = Zenith_Maths::Vector3(0);
	m_strSourcePath.clear();
	m_xMaterialColor = Zenith_Maths::Vector4(1, 1, 1, 1);
	m_uNumVerts = 0;
	m_uNumIndices = 0;
}
