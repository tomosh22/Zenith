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

void Flux_MeshGeometry::LoadFromFile(const char* szPath, Flux_MeshGeometry& xGeometryOut)
{

	FILE* pxFile = fopen(szPath, "rb");
	Zenith_Assert(pxFile, "Mesh file doesn't exist");
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
	xGeometryOut.m_pxPositions = new glm::highp_vec3[xGeometryOut.m_uNumVerts];
	xGeometryOut.m_pxNormals = new glm::vec3[xGeometryOut.m_uNumVerts];

	memcpy(xGeometryOut.m_pVertexData, pcData + ulCursor, ulVertBufferLen);
	ulCursor += ulVertBufferLen;

	memcpy(xGeometryOut.m_puIndices, pcData + ulCursor, ulIndexBufferLen);
	ulCursor += ulIndexBufferLen;

	memcpy(xGeometryOut.m_pxPositions, pcData + ulCursor, sizeof(xGeometryOut.m_pxPositions[0]) * xGeometryOut.m_uNumVerts);
	ulCursor += sizeof(xGeometryOut.m_pxPositions[0]) * xGeometryOut.m_uNumVerts;

	memcpy(xGeometryOut.m_pxNormals, pcData + ulCursor, sizeof(xGeometryOut.m_pxNormals[0]) * xGeometryOut.m_uNumVerts);
	ulCursor += sizeof(xGeometryOut.m_pxNormals[0]) * xGeometryOut.m_uNumVerts;

	delete[] pcData;
}

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
	}

	m_xBufferLayout.CalculateOffsetsAndStrides();
}