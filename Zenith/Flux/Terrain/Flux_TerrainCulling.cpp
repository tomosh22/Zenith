#include "Zenith.h"

#include "Flux/Terrain/Flux_TerrainCulling.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Buffers.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "Vulkan/Zenith_Vulkan_Pipeline.h"
#include "EntityComponent/Components/Zenith_TerrainComponent.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include <fstream>

// Static GPU resources
static Flux_ReadWriteBuffer g_xChunkDataBuffer;
static Flux_IndirectBuffer g_xIndirectDrawBuffer;
static Flux_DynamicConstantBuffer g_xFrustumPlanesBuffer;
static Flux_IndirectBuffer g_xVisibleCountBuffer;  // Used as indirect count buffer for DrawIndexedIndirectCount
static Flux_ReadWriteBuffer g_xLODLevelBuffer;  // New: stores LOD level for each draw call

static Zenith_Vulkan_Pipeline g_xCullingPipeline;
static Zenith_Vulkan_Shader g_xCullingShader;
static Zenith_Vulkan_RootSig g_xCullingRootSig;

static constexpr uint32_t TERRAIN_CHUNK_COUNT = TERRAIN_EXPORT_DIMS * TERRAIN_EXPORT_DIMS;

static constexpr float LOD_DISTANCES_SQ[TERRAIN_LOD_COUNT] = {
	400000.0f,
	1000000.0f,
	2000000.0f,
	FLT_MAX
};

void Flux_TerrainCulling::Initialise()
{
	Zenith_Log("Flux_TerrainCulling::Initialise() - Starting GPU-driven terrain culling with LOD support");

	// NOTE: BuildChunkData() is called by Zenith_TerrainComponent constructor
	// AFTER all terrain meshes have been loaded

	// ========== CREATE GPU BUFFERS ==========

	// Frustum planes buffer (6 planes, updated per frame)
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(
		nullptr,
		sizeof(Flux_CameraDataGPU),
		g_xFrustumPlanesBuffer
	);

	// Indirect draw command buffer (one command per chunk, max)
	// Structure: VkDrawIndexedIndirectCommand (5 uint32_t values)
	// Initialize to zeros so all commands start with indexCount=0
	const size_t indirectBufferSize = sizeof(uint32_t) * 5 * TERRAIN_CHUNK_COUNT;
	uint32_t* pZeroBuffer = new uint32_t[5 * TERRAIN_CHUNK_COUNT];
	memset(pZeroBuffer, 0, indirectBufferSize);
	
	Flux_MemoryManager::InitialiseIndirectBuffer(
		indirectBufferSize,
		g_xIndirectDrawBuffer
	);
	
	// Upload the zero-initialized data
	Flux_MemoryManager::UploadBufferData(g_xIndirectDrawBuffer.GetBuffer().m_xVRAMHandle, pZeroBuffer, indirectBufferSize);
	delete[] pZeroBuffer;

	// Visible chunk counter (single atomic uint32_t)
	Flux_MemoryManager::InitialiseIndirectBuffer(
		sizeof(uint32_t),
		g_xVisibleCountBuffer
	);

	// LOD level buffer (one uint32_t per potential draw call)
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		nullptr,
		sizeof(uint32_t) * TERRAIN_CHUNK_COUNT,
		g_xLODLevelBuffer
	);

	// ========== CREATE COMPUTE PIPELINE ==========

	g_xCullingShader.InitialiseCompute("Terrain/Flux_TerrainCulling.comp");
	Zenith_Log("Flux_TerrainCulling - Loaded compute shader");

	// Build compute root signature
	Flux_PipelineLayout xCullingLayout;
	xCullingLayout.m_uNumDescriptorSets = 1;
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Chunk data (read)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frustum planes (read)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Indirect commands (write)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[3].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Visible count (read/write atomic)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[4].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // LOD levels (write)
	xCullingLayout.m_axDescriptorSetLayouts[0].m_axBindings[5].m_eType = DESCRIPTOR_TYPE_MAX;
	
	Zenith_Vulkan_RootSigBuilder::FromSpecification(g_xCullingRootSig, xCullingLayout);

	// Build compute pipeline
	Zenith_Vulkan_ComputePipelineBuilder xCullingBuilder;
	xCullingBuilder.WithShader(g_xCullingShader)
		.WithLayout(g_xCullingRootSig.m_xLayout)
		.Build(g_xCullingPipeline);

	g_xCullingPipeline.m_xRootSig = g_xCullingRootSig;

	Zenith_Log("Flux_TerrainCulling - Built compute pipeline");
	Zenith_Log("Flux_TerrainCulling - Initialized with %u terrain chunks, %u LOD levels", TERRAIN_CHUNK_COUNT, TERRAIN_LOD_COUNT);
	Zenith_Log("Flux_TerrainCulling - LOD distances: LOD0<%.1f, LOD1<%.1f, LOD2<%.1f, LOD3<inf",
		sqrtf(LOD_DISTANCES_SQ[0]), sqrtf(LOD_DISTANCES_SQ[1]), sqrtf(LOD_DISTANCES_SQ[2]));
}

void Flux_TerrainCulling::Shutdown()
{
	// Cleanup GPU resources
	//Flux_MemoryManager::DestroyBuffer(g_xChunkDataBuffer);
	//Flux_MemoryManager::DestroyBuffer(g_xFrustumPlanesBuffer);
	//Flux_MemoryManager::DestroyBuffer(g_xIndirectDrawBuffer);
	//Flux_MemoryManager::DestroyBuffer(g_xVisibleCountBuffer);
}

void Flux_TerrainCulling::ExtractFrustumPlanes(const Zenith_Maths::Matrix4& xViewProjMatrix, Flux_FrustumPlaneGPU* pxOutPlanes)
{
	// Use existing frustum extraction code
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);

	// Convert to GPU format
	for (int i = 0; i < 6; ++i)
	{
		pxOutPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance
		);
	}
}

void Flux_TerrainCulling::DispatchCulling(const Zenith_Maths::Matrix4& xViewProjMatrix)
{
	// Extract frustum planes and camera position, upload to GPU
	Flux_CameraDataGPU xCameraData;
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProjMatrix);
	
	// Convert frustum to GPU format
	for (int i = 0; i < 6; ++i)
	{
		xCameraData.m_axFrustumPlanes[i].m_xNormalAndDistance = Zenith_Maths::Vector4(
			xFrustum.m_axPlanes[i].m_xNormal,
			xFrustum.m_axPlanes[i].m_fDistance
		);
	}
	
	// Add camera position for distance-based sorting and LOD selection
	Zenith_Maths::Vector3 xCameraPos = Flux_Graphics::GetCameraPosition();
	xCameraData.m_xCameraPosition = Zenith_Maths::Vector4(xCameraPos, 0.0f);
	
	// Debug: Log camera position once every 60 frames
	static int frameCount = 0;
	if (frameCount++ % 60 == 0)
	{
		Zenith_Log("Camera position: (%.1f, %.1f, %.1f)", xCameraPos.x, xCameraPos.y, xCameraPos.z);
	}
	
	Flux_MemoryManager::UploadBufferData(g_xFrustumPlanesBuffer.GetBuffer().m_xVRAMHandle, &xCameraData, sizeof(Flux_CameraDataGPU));

	// Reset visible chunk counter to 0
	uint32_t uZero = 0;
	Flux_MemoryManager::UploadBufferData(g_xVisibleCountBuffer.GetBuffer().m_xVRAMHandle, &uZero, sizeof(uint32_t));

	// Create and submit compute command list for terrain culling
	static Flux_CommandList s_xCullingCommandList("Terrain Culling Compute");
	s_xCullingCommandList.Reset(false);  // No render targets to clear

	// Bind compute pipeline
	s_xCullingCommandList.AddCommand<Flux_CommandBindComputePipeline>(&g_xCullingPipeline);

	// Bind descriptor set 0 with all buffers
	// Note: We bind using CBV but the descriptor type STORAGE_BUFFER in the pipeline layout tells Vulkan to treat them as storage buffers
	s_xCullingCommandList.AddCommand<Flux_CommandBeginBind>(0);
	s_xCullingCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&g_xChunkDataBuffer.GetUAV(), 0);          // Chunk data (read)
	s_xCullingCommandList.AddCommand<Flux_CommandBindCBV>(&g_xFrustumPlanesBuffer.GetCBV(), 1);     // Frustum planes (read)
	s_xCullingCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&g_xIndirectDrawBuffer.GetUAV(), 2);      // Indirect commands (write)
	s_xCullingCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&g_xVisibleCountBuffer.GetUAV(), 3);      // Visible count (read/write atomic)
	s_xCullingCommandList.AddCommand<Flux_CommandBindUAV_Buffer>(&g_xLODLevelBuffer.GetUAV(), 4);          // LOD levels (write)

	// Dispatch compute shader
	// We have 64x64 = 4096 chunks, with local_size_x=64 we need (4096 + 63) / 64 = 64 workgroups
	uint32_t uNumWorkgroups = (TERRAIN_CHUNK_COUNT + 63) / 64;
	s_xCullingCommandList.AddCommand<Flux_CommandDispatch>(uNumWorkgroups, 1, 1);

	// Submit to RENDER_ORDER_SKYBOX (which runs before terrain in the current ordering)
	// This ensures the culling compute completes before terrain rendering begins
	Flux::SubmitCommandList(&s_xCullingCommandList, Flux_Graphics::s_xNullTargetSetup, RENDER_ORDER_TERRAIN_CULLING);
}

const Flux_IndirectBuffer& Flux_TerrainCulling::GetIndirectDrawBuffer()
{
	return g_xIndirectDrawBuffer;
}

const Flux_IndirectBuffer& Flux_TerrainCulling::GetVisibleCountBuffer()
{
	return g_xVisibleCountBuffer;
}

uint32_t Flux_TerrainCulling::GetMaxDrawCount()
{
	return TERRAIN_CHUNK_COUNT;
}

Flux_ReadWriteBuffer& Flux_TerrainCulling::GetLODLevelBuffer()
{
	return g_xLODLevelBuffer;
}

void Flux_TerrainCulling::BuildChunkData()
{
	Zenith_Log("Flux_TerrainCulling::BuildChunkData() - Building chunk AABBs and LOD data from terrain meshes");

	// Temporary CPU-side buffer for chunk data
	Flux_TerrainChunkData* pxChunkData = new Flux_TerrainChunkData[TERRAIN_CHUNK_COUNT];

	const char* LOD_SUFFIXES[4] = { "", "_LOD1", "_LOD2", "_LOD3" };

	uint32_t uCurrentIndexOffset = 0;

	// IMPORTANT: This iteration order MUST match Zenith_TerrainComponent::Zenith_TerrainComponent
	// which combines chunks in x (outer), y (inner), LOD (innermost) order, skipping chunk (0,0) LOD0
	
	// First, calculate AABBs for all chunks using LOD0 geometry
	// NOTE: Meshes should already be loaded by TerrainComponent constructor
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
		{
			// Chunk index for x (outer), y (inner) iteration
			uint32_t uCurrentChunk = x * TERRAIN_EXPORT_DIMS + y;

			// Get LOD0 mesh for this chunk (should already be loaded)
			// Mesh name format: "Terrain_Render" + x + "_" + y (no LOD suffix for LOD0)
			std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
			std::string strChunkName = "Terrain_Render" + strSuffix;

			if (!Zenith_AssetHandler::MeshExists(strChunkName))
			{
				Zenith_Log("ERROR: LOD0 mesh '%s' not loaded yet - TerrainComponent should load meshes before BuildChunkData", strChunkName.c_str());
				Zenith_Assert(false, "BuildChunkData called before meshes were loaded");
			}

			Flux_MeshGeometry& xChunkMesh = Zenith_AssetHandler::GetMesh(strChunkName);

			// Validate that positions were loaded
			if (!xChunkMesh.m_pxPositions || xChunkMesh.m_uNumVerts == 0)
			{
				Zenith_Log("ERROR: Chunk (%u,%u) has no position data! m_pxPositions=%p, m_uNumVerts=%u",
					x, y, xChunkMesh.m_pxPositions, xChunkMesh.m_uNumVerts);
				Zenith_Assert(false, "Terrain chunk has no vertex positions");
			}

			// Generate AABB from vertex positions (use LOD0 for AABB)
			Zenith_AABB xChunkAABB = Zenith_FrustumCulling::GenerateAABBFromVertices(
				xChunkMesh.m_pxPositions,
				xChunkMesh.m_uNumVerts
			);

			// Store chunk AABB
			pxChunkData[uCurrentChunk].m_xAABBMin = Zenith_Maths::Vector4(xChunkAABB.m_xMin, 0.0f);
			pxChunkData[uCurrentChunk].m_xAABBMax = Zenith_Maths::Vector4(xChunkAABB.m_xMax, 0.0f);

			// Initialize LOD distance thresholds for this chunk (same for all chunks)
			for (uint32_t uLOD = 0; uLOD < TERRAIN_LOD_COUNT; ++uLOD)
			{
				pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_fMaxDistance = LOD_DISTANCES_SQ[uLOD];
			}
		}
	}

	// Now extract LOD data from all meshes in the EXACT order they are combined
	for (uint32_t x = 0; x < TERRAIN_EXPORT_DIMS; ++x)
	{
		for (uint32_t y = 0; y < TERRAIN_EXPORT_DIMS; ++y)
		{
			uint32_t uCurrentChunk = x * TERRAIN_EXPORT_DIMS + y;

			for (uint32_t uLOD = 0; uLOD < TERRAIN_LOD_COUNT; ++uLOD)
			{
				std::string strSuffix = std::to_string(x) + "_" + std::to_string(y);
				std::string strLODMeshName = std::string("Terrain_Render") + LOD_SUFFIXES[uLOD] + strSuffix;

				if (!Zenith_AssetHandler::MeshExists(strLODMeshName))
				{
					Zenith_Log("ERROR: Mesh '%s' not loaded yet", strLODMeshName.c_str());
					Zenith_Assert(false, "BuildChunkData called before meshes were loaded");
				}

				Flux_MeshGeometry& xLODMesh = Zenith_AssetHandler::GetMesh(strLODMeshName);

				// Skip chunk (0,0) LOD0 as it's the base mesh
				if (x == 0 && y == 0 && uLOD == 0)
				{
					// Store LOD data (this is the first mesh so offset is 0)
					pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_uFirstIndex = 0;
					pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_uIndexCount = xLODMesh.m_uNumIndices;
					pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_uVertexOffset = 0;
					
					// Update index offset for the NEXT mesh
					uCurrentIndexOffset = xLODMesh.m_uNumIndices;
					continue;
				}

				// Store LOD data
				pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_uFirstIndex = uCurrentIndexOffset;
				pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_uIndexCount = xLODMesh.m_uNumIndices;
				pxChunkData[uCurrentChunk].m_axLODs[uLOD].m_uVertexOffset = 0;
				
				// Update index offset
				uCurrentIndexOffset += xLODMesh.m_uNumIndices;
			}
		}
	}

	Zenith_Log("Flux_TerrainCulling - Total indices across all LODs: %u", uCurrentIndexOffset);

	// Upload chunk data to GPU
	Flux_MemoryManager::InitialiseReadWriteBuffer(
		pxChunkData,
		sizeof(Flux_TerrainChunkData) * TERRAIN_CHUNK_COUNT,
		g_xChunkDataBuffer
	);

	// Cleanup CPU data
	delete[] pxChunkData;

	Zenith_Log("Flux_TerrainCulling - Chunk data with %u LOD levels uploaded to GPU", TERRAIN_LOD_COUNT);
}
