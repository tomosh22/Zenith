#include "Zenith.h"
#include "Flux_MeshGeometry.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux.h"

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
	if (strString == "UInt4") return SHADER_DATA_TYPE_UINT4;
	Zenith_Assert(false, "Unrecognized data type");
	return SHADER_DATA_TYPE_NONE;
}

void Flux_MeshGeometry::LoadFromFile(const char* szPath, Flux_MeshGeometry& xGeometryOut, const bool bRetainPositionsAndNormals)
{
	FILE* pxFile = fopen(szPath, "rb");

	if (!pxFile)
	{
		Zenith_Log("Mesh file doesn't exist");
		return;
	}
	fseek(pxFile, 0, SEEK_END);
	size_t ulFileSize = ftell(pxFile);
	fseek(pxFile, 0, SEEK_SET);
	char* pcData = new char[ulFileSize + 1];
	fread(pcData, ulFileSize, 1, pxFile);
	pcData[ulFileSize] = '\0';
	fclose(pxFile);

	size_t ulCursor = 0;

	uint32_t uNumElements = atoi(pcData + ulCursor);
	ulCursor += 2;
	for (uint32_t i = 0; i < uNumElements; i++)
	{
		std::string strType(pcData + ulCursor);
		ulCursor += strType.length() + 1;
		ShaderDataType eType = StringToShaderDataType(strType);
		xGeometryOut.m_xBufferLayout.GetElements().push_back({ eType });
	}
	xGeometryOut.m_xBufferLayout.CalculateOffsetsAndStrides();

	size_t ulNumVerts = atoi(pcData + ulCursor);
	ulCursor += std::to_string(ulNumVerts).length() + 1;

	size_t ulNumIndices = atoi(pcData + ulCursor);
	ulCursor += std::to_string(ulNumIndices).length() + 1;

	size_t ulVertBufferLen = atoi(pcData + ulCursor);
	ulCursor += std::to_string(ulVertBufferLen).length() + 1;

	size_t ulIndexBufferLen = atoi(pcData + ulCursor);
	ulCursor += std::to_string(ulIndexBufferLen).length() + 1;

	xGeometryOut.m_uNumVerts = ulNumVerts;
	xGeometryOut.m_uNumIndices = ulNumIndices;

	Zenith_Assert(ulVertBufferLen == xGeometryOut.m_uNumVerts * xGeometryOut.m_xBufferLayout.GetStride(), "Vertex buffer is wrong size");
	Zenith_Assert(ulIndexBufferLen == xGeometryOut.m_uNumIndices * sizeof(IndexType), "Index buffer is wrong size");

	xGeometryOut.m_pVertexData = new char[ulVertBufferLen];
	xGeometryOut.m_puIndices = new IndexType[xGeometryOut.m_uNumIndices];
	if (bRetainPositionsAndNormals)
	{
		xGeometryOut.m_pxPositions = new glm::highp_vec3[xGeometryOut.m_uNumVerts];
		xGeometryOut.m_pxNormals = new glm::vec3[xGeometryOut.m_uNumVerts];
	}

	memcpy(xGeometryOut.m_pVertexData, pcData + ulCursor, ulVertBufferLen);
	ulCursor += ulVertBufferLen;

	memcpy(xGeometryOut.m_puIndices, pcData + ulCursor, ulIndexBufferLen);
	ulCursor += ulIndexBufferLen;

	if (bRetainPositionsAndNormals)
	{
		memcpy(xGeometryOut.m_pxPositions, pcData + ulCursor, sizeof(xGeometryOut.m_pxPositions[0]) * xGeometryOut.m_uNumVerts);
	}
	ulCursor += sizeof(glm::highp_vec3) * xGeometryOut.m_uNumVerts;

	if (bRetainPositionsAndNormals)
	{
		memcpy(xGeometryOut.m_pxNormals, pcData + ulCursor, sizeof(xGeometryOut.m_pxNormals[0]) * xGeometryOut.m_uNumVerts);
	}
	ulCursor += sizeof(glm::vec3) * xGeometryOut.m_uNumVerts;

	delete[] pcData;

	Flux_MemoryManager::InitialiseVertexBuffer(xGeometryOut.GetVertexData(), xGeometryOut.GetVertexDataSize(), xGeometryOut.m_xVertexBuffer);
	Flux_MemoryManager::InitialiseIndexBuffer(xGeometryOut.GetIndexData(), xGeometryOut.GetIndexDataSize(), xGeometryOut.m_xIndexBuffer);
}

#ifdef ZENITH_TOOLS
static std::string ShaderDataTypeToString(ShaderDataType eType)
{
	switch (eType)
	{
	case SHADER_DATA_TYPE_FLOAT:
		return "Float";
	case SHADER_DATA_TYPE_FLOAT2:
		return "Float2";
	case SHADER_DATA_TYPE_FLOAT3:
		return "Float3";
	case SHADER_DATA_TYPE_FLOAT4:
		return "Float4";
	case SHADER_DATA_TYPE_UINT4:
		return "UInt4";
	default:
		Zenith_Assert(false, "Unknown data type");
		return "";
	}
}
void Flux_MeshGeometry::Export(const char* szFilename)
{
	FILE* pxFile = fopen(szFilename, "wb");
	Zenith_Assert(pxFile, "Failed to open file %s", szFilename);
	char cNull = '\0';

	fputs(std::to_string(m_xBufferLayout.GetElements().size()).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);
	for (Flux_BufferElement& xElement : m_xBufferLayout.GetElements())
	{
		fputs(ShaderDataTypeToString(xElement._Type).c_str(), pxFile);
		fwrite(&cNull, 1, 1, pxFile);
	}

	fputs(std::to_string(m_uNumVerts).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fputs(std::to_string(m_uNumIndices).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fputs(std::to_string(m_uNumVerts * m_xBufferLayout.GetStride()).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fputs(std::to_string(m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType)).c_str(), pxFile);
	fwrite(&cNull, 1, 1, pxFile);

	fwrite(m_pVertexData, m_uNumVerts * m_xBufferLayout.GetStride(), 1, pxFile);

	fwrite(m_puIndices, m_uNumIndices * sizeof(Flux_MeshGeometry::IndexType), 1, pxFile);

	fwrite(m_pxPositions, m_uNumVerts * sizeof(m_pxPositions[0]), 1, pxFile);

	fwrite(m_pxNormals, m_uNumVerts * sizeof(m_pxNormals[0]), 1, pxFile);

	fclose(pxFile);
}
#endif

void Flux_MeshGeometry::GenerateLayoutAndVertexData()
{
	uint32_t uNumFloats = 0;
	if (m_pxPositions != nullptr)
	{
		m_xBufferLayout.GetElements().push_back({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pxUVs != nullptr)
	{
		m_xBufferLayout.GetElements().push_back({ SHADER_DATA_TYPE_FLOAT2 });
		uNumFloats += 2;
	}
	if (m_pxNormals != nullptr)
	{
		m_xBufferLayout.GetElements().push_back({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pxTangents != nullptr)
	{
		m_xBufferLayout.GetElements().push_back({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pxBitangents != nullptr)
	{
		m_xBufferLayout.GetElements().push_back({ SHADER_DATA_TYPE_FLOAT3 });
		uNumFloats += 3;
	}
	if (m_pfMaterialLerps != nullptr)
	{
		m_xBufferLayout.GetElements().push_back({ SHADER_DATA_TYPE_FLOAT });
		uNumFloats += 1;
	}

	m_pVertexData = new float[m_uNumVerts * uNumFloats];

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