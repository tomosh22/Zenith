#pragma once
#include "Maths/Zenith_Maths.h"
#include "Flux/Flux_Types.h"

class Flux_MeshGeometry
{
public:
	using IndexType = uint32_t;

	~Flux_MeshGeometry()
	{
		delete[] m_pVertexData;
		delete[] m_puIndices;
		delete[] m_pxPositions;
		delete[] m_pxUVs;
	}

	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut);

	const void* GetVertexData() const { return m_pVertexData; }
	const uint64_t GetVertexDataSize() const { return m_uNumVerts * m_xBufferLayout.GetStride(); }
	const IndexType* GetIndexData() const { return m_puIndices; }
	const uint64_t GetIndexDataSize() const { return m_uNumIndices * sizeof(IndexType); }
private:
	void GenerateLayoutAndVertexData();

	Flux_BufferLayout m_xBufferLayout;

	uint32_t m_uNumVerts = 0;
	uint32_t m_uNumIndices = 0;

	IndexType* m_puIndices = nullptr;

	Zenith_Maths::Vector3* m_pxPositions = nullptr;
	Zenith_Maths::Vector2* m_pxUVs = nullptr;

	void* m_pVertexData = nullptr;
};