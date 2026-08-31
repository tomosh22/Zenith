#pragma once
#include "Core/Zenith_Engine.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_Buffers.h"
#include "Collections/Zenith_HashMap.h"

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
		if(m_puBoneIDs) Zenith_MemoryManagement::Deallocate(m_puBoneIDs);
		if(m_pfBoneWeights) Zenith_MemoryManagement::Deallocate(m_pfBoneWeights);

		// Destroy GPU buffers if they were allocated
		if (m_xVertexBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		{
			g_xEngine.FluxMemory().DestroyVertexBuffer(m_xVertexBuffer);
		}
		m_xVertexBuffer.Reset();

		if (m_xIndexBuffer.GetBuffer().m_xVRAMHandle.IsValid())
		{
			g_xEngine.FluxMemory().DestroyIndexBuffer(m_xIndexBuffer);
		}
		m_xIndexBuffer.Reset();

		m_uNumBones = 0;
		m_xBoneNameToIdAndOffset.Clear();
		m_strSourcePath.clear();
	}

	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut);
	static void GenerateFullscreenQuad(Flux_MeshGeometry& xGeometryOut, Zenith_Maths::Matrix4 xTransform);
	static void GenerateUnitCube(Flux_MeshGeometry& xGeometryOut);
	// The general box GenerateUnitCube is a special case of: centred on the origin,
	// spanning -half..+half per axis, with the same 24-vertex/36-index layout,
	// winding, per-face UVs and tangent basis. For a caller that needs a renderable
	// block of a stated SIZE and cannot express it as a transform scale (an entity
	// whose scale already belongs to a model, say).
	static void GenerateBox(Flux_MeshGeometry& xGeometryOut,
		const Zenith_Maths::Vector3& xHalfExtents);
	// Procedural primitives. GenerateCapsule builds a sphere stretched along Y by
	// fHeight (a true cylinder+hemispheres capsule is approximated); GenerateCone
	// builds a base ring + apex + base centre. Both fill the geometry, build the
	// layout, and upload to the GPU — same contract as GenerateUnitCube. Moved out
	// of per-game code so any game can reuse them.
	static void GenerateCapsule(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices, uint32_t uStacks);
	static void GenerateCone(Flux_MeshGeometry& xGeometryOut, float fRadius, float fHeight, uint32_t uSlices);
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
	// Serializes the geometry's CURRENT layout + interleaved bytes verbatim — the
	// right call when those ARE the file format (the terrain exporter's chunk
	// meshes carry the terrain table). Asserts if the live layout is the reflected
	// mesh-pipeline table: the .zgeom format has no version field, so its one
	// stability guarantee is the element table, and a reflected table changes
	// whenever a shader annotation does — that form must never reach disk. A
	// geometry living on the packed form exports with ExportDerivedFloatLayout.
	void Export(const char* szFilename);
	// Serializes the DERIVED float32 form (the stable .zgeom shape) regardless of
	// which form the live geometry carries, without touching the live layout,
	// bytes, or GPU buffers — for callers that draw the packed mesh-pipeline form
	// but persist the asset (Combat's tools-boot capsule/cube/cone stamps).
	void ExportDerivedFloatLayout(const char* szFilename);
private:
	// Shared serializer: the layout + interleaved bytes are parameters, everything
	// else (counts, bone map, SoA attribute streams) comes from the members.
	void ExportWithLayout(const Flux_BufferLayout& xLayout, const u_int8* pVertexData, const char* szFilename);
public:
#endif

	friend class Zenith_ColliderComponent;
	friend class Zenith_PhysicsMeshGenerator;
	// Terrain's bounded .zgeom decoder constructs retained CPU geometry from
	// one already-validated file snapshot. Friendship keeps the general mesh
	// loader API unchanged while avoiding a second pathname open through its
	// assertion-based DataStream path.
	friend class Zenith_TerrainComponent;

	// Interleave m_pVertexData, and declare the layout that describes it.
	//
	// DERIVED: one tight float32 element per attribute STREAM this geometry carries,
	// in the fixed order the serialized .zgeom element table declares them. That table
	// IS the file format (Export writes it verbatim), and it is also the 20-byte
	// position+UV shape the Quads / Text / Particles programs fetch from the shared
	// unit quad — so this is the form for geometry that is serialized, consumed
	// CPU-side, or drawn by a program with its own VsIn.
	void GenerateLayoutAndVertexData();
	// The body of GenerateLayoutAndVertexData, writing into caller storage instead
	// of the members — so ExportDerivedFloatLayout can build the stable float form
	// without disturbing a live packed geometry. Appends to xLayoutOut exactly as
	// GenerateLayoutAndVertexData appends to m_xBufferLayout; pVertexDataOut is
	// (re)pointed at a fresh allocation the caller owns.
	void BuildDerivedFloatLayoutAndVertexData(Flux_BufferLayout& xLayoutOut, u_int8*& pVertexDataOut) const;

	// MESH-PIPELINE: the reflected static-mesh table (Flux_DeclareMeshVertexLayout),
	// the same packed 24-byte vertex an imported mesh asset is packed into. Any
	// geometry DRAWN as a mesh must use this one — the unified opaque pipeline binds
	// this buffer and fetches it at that stride whatever the bytes happen to be, so a
	// float32 stream would decode as garbage rather than fail. Bone streams are not
	// part of that table and are ignored; procedural geometry has no skinning path.
	void GenerateMeshPipelineVertexData();
	// Upload this geometry's current vertex/index data to GPU buffers. Single
	// upload path shared by every generator + LoadFromFile (keeps the g_xEngine
	// reach in one place).
	void UploadToGPU();

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
	Zenith_HashMap<std::string, std::pair<uint32_t, Zenith_Maths::Matrix4>> m_xBoneNameToIdAndOffset;

	IndexType* m_puIndices = nullptr;

	Zenith_Maths::Vector3* m_pxPositions = nullptr;
	Zenith_Maths::Vector2* m_pxUVs = nullptr;
	Zenith_Maths::Vector3* m_pxNormals = nullptr;
	Zenith_Maths::Vector3* m_pxTangents = nullptr;
	Zenith_Maths::Vector3* m_pxBitangents = nullptr;
	Zenith_Maths::Vector4* m_pxColors = nullptr;
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
};
