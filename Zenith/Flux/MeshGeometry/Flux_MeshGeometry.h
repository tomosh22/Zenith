#pragma once
#include "Flux/Flux_Types.h"
#include "Flux/Flux_Buffers.h"

// Forward declarations
class Flux_AnimationController;

#define MAX_BONES_PER_VERTEX 4

class Flux_MeshGeometry
{
public:
	using IndexType = uint32_t;

	enum VertexAttribute : uint8_t
	{
		FLUX_VERTEX_ATTRIBUTE__POSITION = 0,
		FLUX_VERTEX_ATTRIBUTE__NORMAL,
		FLUX_VERTEX_ATTRIBUTE__TANGENT,
		FLUX_VERTEX_ATTRIBUTE__BITANGENT,
		FLUX_VERTEX_ATTRIBUTE__COLOR,
		FLUX_VERTEX_ATTRIBUTE__MATERIAL_LERP,
		FLUX_VERTEX_ATTRIBUTE__BONE_IDS,
		FLUX_VERTEX_ATTRIBUTE__BONE_WEIGHTS,
		FLUX_VERTEX_ATTRIBUTE__COUNT
	};

	struct MeshBone
	{
		uint32_t m_uID = ~0u;
		Zenith_Maths::Matrix4 m_xOffsetMat = glm::identity<glm::mat4>();
	};

	~Flux_MeshGeometry()
	{
		Reset();
	}

	// Non-copyable and non-movable - VRAM handles require special handling
	Flux_MeshGeometry(const Flux_MeshGeometry&) = delete;
	Flux_MeshGeometry& operator=(const Flux_MeshGeometry&) = delete;
	Flux_MeshGeometry(Flux_MeshGeometry&&) = delete;
	Flux_MeshGeometry& operator=(Flux_MeshGeometry&&) = delete;

	// Default constructor
	Flux_MeshGeometry() = default;

	void Reset()
	{
		m_xBufferLayout.Reset();

		if(m_pVertexData) Zenith_MemoryManagement::Deallocate(m_pVertexData);
		if(m_puIndices) Zenith_MemoryManagement::Deallocate(m_puIndices);
		if(m_pxPositions) Zenith_MemoryManagement::Deallocate(m_pxPositions);
		if(m_pxUVs) Zenith_MemoryManagement::Deallocate(m_pxUVs);
		if(m_pxNormals) Zenith_MemoryManagement::Deallocate(m_pxNormals);
		if(m_pxTangents) Zenith_MemoryManagement::Deallocate(m_pxTangents);
		if(m_pxBitangents) Zenith_MemoryManagement::Deallocate(m_pxBitangents);
		if(m_pxColors) Zenith_MemoryManagement::Deallocate(m_pxColors);
		if(m_pfMaterialLerps) Zenith_MemoryManagement::Deallocate(m_pfMaterialLerps);
		if(m_puBoneIDs) Zenith_MemoryManagement::Deallocate(m_puBoneIDs);
		if(m_pfBoneWeights) Zenith_MemoryManagement::Deallocate(m_pfBoneWeights);

		// Destroy GPU buffers if they were allocated
		if (m_xVertexBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		{
			Flux_MemoryManager::DestroyVertexBuffer(m_xVertexBuffer);
		}
		m_xVertexBuffer.Reset();

		if (m_xIndexBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		{
			Flux_MemoryManager::DestroyIndexBuffer(m_xIndexBuffer);
		}
		m_xIndexBuffer.Reset();

		m_uNumBones = 0;
		m_xBoneNameToIdAndOffset.clear();
		m_strSourcePath.clear();
	}

	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut);
	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut, Zenith_Maths::Matrix4 xTransform);
	static void GenerateUnitCube(Flux_MeshGeometry& xGeometryOut);
	static void LoadFromFile(const char* szPath, Flux_MeshGeometry& xGeometryOut, u_int uRetainAttributeBits = 0, const bool bUploadToGPU = true);
	static void Combine(Flux_MeshGeometry& xDst, const Flux_MeshGeometry& xSrc);

	const void* GetVertexData() const { return m_pVertexData; }
	const uint64_t GetVertexDataSize() const { return m_uNumVerts * m_xBufferLayout.GetStride(); }
	const IndexType* GetIndexData() const { return m_puIndices; }
	const uint64_t GetIndexDataSize() const { return m_uNumIndices * sizeof(IndexType); }
	const uint32_t GetNumVerts() const { return m_uNumVerts; }
	const uint32_t GetNumIndices() const { return m_uNumIndices; }
	const uint32_t GetNumBones() const { return m_uNumBones; }
	void SetNumBones(const uint32_t uNumBones) { m_uNumBones = uNumBones; }

	const Flux_VertexBuffer& GetVertexBuffer() const { return m_xVertexBuffer; }
	Flux_VertexBuffer& GetVertexBuffer() { return m_xVertexBuffer; }
	const Flux_IndexBuffer& GetIndexBuffer() const { return m_xIndexBuffer; }
	Flux_IndexBuffer& GetIndexBuffer() { return m_xIndexBuffer; }

	const Flux_BufferLayout& GetBufferLayout() const { return m_xBufferLayout; }

#ifdef ZENITH_TOOLS
	void Export(const char* szFilename);
#endif

	friend class Zenith_ColliderComponent;
	friend class Zenith_PhysicsMeshGenerator;
	void GenerateLayoutAndVertexData();

	uint32_t m_uNumVerts = 0;
	uint32_t m_uNumIndices = 0;
	uint32_t m_uNumBones = 0;

#ifndef ZENITH_TOOLS
private:
#endif
	void GenerateNormals();
	void GenerateTangents();
	void GenerateBitangents();
	Zenith_Maths::Vector3 GenerateTangent(uint32_t uA, uint32_t uB, uint32_t uC);

	Flux_BufferLayout m_xBufferLayout;

	//#TO_TODO: move to private
public:
	std::unordered_map<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>> m_xBoneNameToIdAndOffset;

	IndexType* m_puIndices = nullptr;

	Zenith_Maths::Vector3* m_pxPositions = nullptr;
	Zenith_Maths::Vector2* m_pxUVs = nullptr;
	Zenith_Maths::Vector3* m_pxNormals = nullptr;
	Zenith_Maths::Vector3* m_pxTangents = nullptr;
	Zenith_Maths::Vector3* m_pxBitangents = nullptr;
	Zenith_Maths::Vector4* m_pxColors = nullptr;
	float* m_pfMaterialLerps = nullptr;
	uint32_t* m_puBoneIDs = nullptr;
	float* m_pfBoneWeights = nullptr;

	Zenith_Maths::Vector4 m_xMaterialColor = Zenith_Maths::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

	// Source path for serialization (set when loaded from file)
	std::string m_strSourcePath;

	u_int8* m_pVertexData = nullptr;

	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;

	u_int64 m_ulReservedVertexDataSize = 0;
	u_int64 m_ulReservedIndexDataSize = 0;
	u_int64 m_ulReservedPositionDataSize = 0;


	class Flux_MeshAnimation* m_pxAnimation = nullptr;

	// New animation system - AnimationController provides state machines, blending, and IK
	// When both are present, prefer the new controller for rendering
	Flux_AnimationController* m_pxAnimationController = nullptr;

	// Check if this mesh has any animation system available
	bool HasAnimation() const { return m_pxAnimation != nullptr || m_pxAnimationController != nullptr; }

	// Get bone buffer for rendering - prefers new controller if available
	const Flux_DynamicConstantBuffer* GetBoneBuffer() const;
};