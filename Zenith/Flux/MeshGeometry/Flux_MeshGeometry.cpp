#include "Zenith.h"
#include "Flux_MeshGeometry.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"
#include "DataStream/Zenith_DataStream.h"

void Flux_MeshGeometry::GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut)
{
	xGeometryOut.m_uNumVerts = 4;
	xGeometryOut.m_uNumIndices = 6;
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[xGeometryOut.m_uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[xGeometryOut.m_uNumVerts];

	xGeometryOut.m_puIndices = new IndexType[xGeometryOut.m_uNumIndices]{ 0, 1, 2, 2, 1, 3 };

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
	xGeometryOut.m_pxPositions = new Zenith_Maths::Vector3[xGeometryOut.m_uNumVerts];
	xGeometryOut.m_pxUVs = new Zenith_Maths::Vector2[xGeometryOut.m_uNumVerts];

	xGeometryOut.m_puIndices = new IndexType[xGeometryOut.m_uNumIndices]{ 0, 1, 2, 2, 1, 3 };

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
		ptData = new T[uDataSize / sizeof(T)];
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

void Flux_MeshGeometry::LoadFromFile(const char* szPath, Flux_MeshGeometry& xGeometryOut, const bool bRetainAttributes /*= false*/, const bool bUploadToGPU /*= true*/)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	xStream >> xGeometryOut.m_xBufferLayout.GetElements();
	xGeometryOut.m_xBufferLayout.CalculateOffsetsAndStrides();
	xStream >> xGeometryOut.m_uNumVerts;
	xStream >> xGeometryOut.m_uNumIndices;
	xStream >> xGeometryOut.m_uNumBones;
	xStream >> xGeometryOut.m_xBoneNameToIdAndOffset;

	ReadAttribute(xGeometryOut.m_pVertexData, xStream, xGeometryOut.m_uNumVerts * xGeometryOut.m_xBufferLayout.GetStride());
	ReadAttribute(xGeometryOut.m_puIndices, xStream, xGeometryOut.m_uNumIndices * sizeof(m_puIndices[0]));
	if (bRetainAttributes)
	{
		ReadAttribute(xGeometryOut.m_pxPositions, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxPositions[0]));
		ReadAttribute(xGeometryOut.m_pxNormals, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxNormals[0]));
		ReadAttribute(xGeometryOut.m_pxTangents, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxTangents[0]));
		ReadAttribute(xGeometryOut.m_pxBitangents, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxBitangents[0]));
		ReadAttribute(xGeometryOut.m_puBoneIDs, xStream, xGeometryOut.m_uNumVerts * sizeof(m_puBoneIDs[0]));
		ReadAttribute(xGeometryOut.m_pfBoneWeights, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pfBoneWeights[0]));
	}
	else
	{
		SkipAttribute(xGeometryOut.m_pxPositions, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxPositions[0]));
		SkipAttribute(xGeometryOut.m_pxNormals, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxNormals[0]));
		SkipAttribute(xGeometryOut.m_pxTangents, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxTangents[0]));
		SkipAttribute(xGeometryOut.m_pxBitangents, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pxBitangents[0]));
		SkipAttribute(xGeometryOut.m_puBoneIDs, xStream, xGeometryOut.m_uNumVerts * sizeof(m_puBoneIDs[0]));
		SkipAttribute(xGeometryOut.m_pfBoneWeights, xStream, xGeometryOut.m_uNumVerts * sizeof(m_pfBoneWeights[0]));
	}

	

	if(bUploadToGPU)
	{
		Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
		Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
	}
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
	xStream << m_xBoneNameToIdAndOffset;
	ExportAttribute(m_pVertexData, xStream, m_uNumVerts * m_xBufferLayout.GetStride());
	ExportAttribute(m_puIndices, xStream, m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType));
	ExportAttribute(m_pxPositions, xStream, m_uNumVerts * sizeof(m_pxPositions[0]));
	ExportAttribute(m_pxNormals, xStream, m_uNumVerts * sizeof(m_pxNormals[0]));
	ExportAttribute(m_pxTangents, xStream, m_uNumVerts * sizeof(m_pxTangents[0]));
	ExportAttribute(m_pxBitangents, xStream, m_uNumVerts * sizeof(m_pxBitangents[0]));
	ExportAttribute(m_puBoneIDs, xStream, m_uNumVerts * sizeof(m_puBoneIDs[0]));
	ExportAttribute(m_pfBoneWeights, xStream, m_uNumVerts * sizeof(m_pfBoneWeights[0]));

	xStream.WriteToFile(szFilename);
}
#endif

void Flux_MeshGeometry::GenerateLayoutAndVertexData()
{
	uint32_t uNumFloats = 0;
	if (m_pxPositions != nullptr)
	{
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pxUVs != nullptr)
	{
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT2 });
		uNumFloats += 2;
	}
	if (m_pxNormals != nullptr)
	{
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pxTangents != nullptr)
	{
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pxBitangents != nullptr)
	{
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pfMaterialLerps != nullptr)
	{
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT });
		uNumFloats += 1;
	}

	if (m_puBoneIDs != nullptr)
	{
		Zenith_Assert(m_pfBoneWeights != nullptr, "How have we wound up with bone IDs but no weights");
		static_assert(MAX_BONES_PER_VERTEX == 4, "data type needs changing");
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_UINT4 });
		m_xBufferLayout.GetElements().PushBack({ SHADER_DATA_TYPE_FLOAT4 });
		uNumFloats += MAX_BONES_PER_VERTEX * 2;
	}

	m_pVertexData = new u_int8[m_uNumVerts * uNumFloats * 4];

	size_t index = 0;
	for (uint32_t i = 0; i < m_uNumVerts; i++)
	{
		if (m_pxPositions != nullptr)
		{
			((float*)m_pVertexData)[index++] = m_pxPositions[i].x;
			((float*)m_pVertexData)[index++] = m_pxPositions[i].y;
			((float*)m_pVertexData)[index++] = m_pxPositions[i].z;
		}

		if (m_pxUVs != nullptr)
		{
			((float*)m_pVertexData)[index++] = m_pxUVs[i].x;
			((float*)m_pVertexData)[index++] = m_pxUVs[i].y;
		}
		if (m_pxNormals != nullptr)
		{
			((float*)m_pVertexData)[index++] = m_pxNormals[i].x;
			((float*)m_pVertexData)[index++] = m_pxNormals[i].y;
			((float*)m_pVertexData)[index++] = m_pxNormals[i].z;
		}
		if (m_pxTangents != nullptr)
		{
			((float*)m_pVertexData)[index++] = m_pxTangents[i].x;
			((float*)m_pVertexData)[index++] = m_pxTangents[i].y;
			((float*)m_pVertexData)[index++] = m_pxTangents[i].z;
		}
		if (m_pxBitangents != nullptr)
		{
			((float*)m_pVertexData)[index++] = m_pxBitangents[i].x;
			((float*)m_pVertexData)[index++] = m_pxBitangents[i].y;
			((float*)m_pVertexData)[index++] = m_pxBitangents[i].z;
		}
		if (m_pfMaterialLerps != nullptr)
		{
			((float*)m_pVertexData)[index++] = m_pfMaterialLerps[i];
		}
		if (m_puBoneIDs != nullptr)
		{
			//we've already asserted that weights isn't null
			((uint32_t*)m_pVertexData)[index++] = m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 0];
			((uint32_t*)m_pVertexData)[index++] = m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 1];
			((uint32_t*)m_pVertexData)[index++] = m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 2];
			((uint32_t*)m_pVertexData)[index++] = m_puBoneIDs[i * MAX_BONES_PER_VERTEX + 3];
			((float*)m_pVertexData)[index++] = m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 0];
			((float*)m_pVertexData)[index++] = m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 1];
			((float*)m_pVertexData)[index++] = m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 2];
			((float*)m_pVertexData)[index++] = m_pfBoneWeights[i * MAX_BONES_PER_VERTEX + 3];
		}
	}

	m_xBufferLayout.CalculateOffsetsAndStrides();
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