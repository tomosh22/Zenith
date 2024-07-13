#include "Zenith.h"

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