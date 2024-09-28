#pragma once
#include "Flux/Flux_Types.h"
#include "Flux/Flux_Buffers.h"

#define MAX_BONES_PER_VERTEX 4

class Flux_MeshGeometry
{
public:
	using IndexType = uint32_t;

	struct Bone
	{
		uint32_t m_uID = ~0u;
		Zenith_Maths::Matrix4 m_xOffsetMat = glm::identity<glm::mat4>();
	};

	~Flux_MeshGeometry()
	{
		Reset();
	}

	void Reset()
	{
		m_xBufferLayout.Reset();

		if(m_pVertexData) delete[] m_pVertexData;
		if(m_puIndices) delete[] m_puIndices;
		if(m_pxPositions) delete[] m_pxPositions;
		if(m_pxUVs) delete[] m_pxUVs;
		if(m_pxNormals) delete[] m_pxNormals;
		if(m_pxTangents) delete[] m_pxTangents;
		if(m_pxBitangents) delete[] m_pxBitangents;
		if(m_pfMaterialLerps) delete[] m_pfMaterialLerps;

		Zenith_Log("Resetting vertex buffer");
		m_xVertexBuffer.Reset();

		Zenith_Log("Resetting index buffer");
		m_xIndexBuffer.Reset();
	}

	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut);
	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut, Zenith_Maths::Matrix4 xTransform);
	static void LoadFromFile(const char* szPath, Flux_MeshGeometry& xGeometryOut, const bool bRetainPositionsAndNormals = false);

	const void* GetVertexData() const { return m_pVertexData; }
	const uint64_t GetVertexDataSize() const { return m_uNumVerts * m_xBufferLayout.GetStride(); }
	const IndexType* GetIndexData() const { return m_puIndices; }
	const uint64_t GetIndexDataSize() const { return m_uNumIndices * sizeof(IndexType); }
	const uint32_t GetNumVerts() const { return m_uNumVerts; }
	const uint32_t GetNumIndices() const { return m_uNumIndices; }
	const uint32_t GetNumBones() const { return m_uNumBones; }

	const Flux_VertexBuffer& GetVertexBuffer() const { return m_xVertexBuffer; }
	Flux_VertexBuffer& GetVertexBuffer() { return m_xVertexBuffer; }
	const Flux_IndexBuffer& GetIndexBuffer() const { return m_xIndexBuffer; }
	Flux_IndexBuffer& GetIndexBuffer() { return m_xIndexBuffer; }

#ifdef ZENITH_TOOLS
	void Export(const char* szFilename);
#endif

#ifndef ZENITH_TOOLS
private:
#endif
	friend class Zenith_ColliderComponent;
	void GenerateLayoutAndVertexData();

	void GenerateNormals();
	void GenerateTangents();
	void GenerateBitangents();
	Zenith_Maths::Vector3 GenerateTangent(uint32_t uA, uint32_t uB, uint32_t uC);

	Flux_BufferLayout m_xBufferLayout;

	uint32_t m_uNumVerts = 0;
	uint32_t m_uNumIndices = 0;
	uint32_t m_uNumBones = 0;

	std::unordered_map<std::string, uint32_t> m_xBoneNameToID;

	IndexType* m_puIndices = nullptr;

	Zenith_Maths::Vector3* m_pxPositions = nullptr;
	Zenith_Maths::Vector2* m_pxUVs = nullptr;
	Zenith_Maths::Vector3* m_pxNormals = nullptr;
	Zenith_Maths::Vector3* m_pxTangents = nullptr;
	Zenith_Maths::Vector3* m_pxBitangents = nullptr;
	float* m_pfMaterialLerps = nullptr;
	uint32_t* m_puBoneIDs = nullptr;
	float* m_pfBoneWeights = nullptr;

	//#TO_TODO: move this to a separate skeleton class
	Bone* m_pxBones = nullptr;

	void* m_pVertexData = nullptr;

	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
};