#include "Zenith.h"

#include "Flux_Grass.h"
#include "Flux/Flux.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/HDR/Flux_HDR.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "Vulkan/Zenith_Vulkan_MemoryManager.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Maths/Zenith_FrustumCulling.h"
#include <random>
#include <algorithm>

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Task for async rendering
static Zenith_Task g_xRenderTask(ZENITH_PROFILE_INDEX__FLUX_GRASS, Flux_Grass::Render, nullptr);

// Command list
static Flux_CommandList g_xCommandList("Grass");

// Static member definitions
Flux_Pipeline Flux_Grass::s_xGrassPipeline;
Flux_Shader Flux_Grass::s_xGrassShader;

Flux_ReadWriteBuffer Flux_Grass::s_xInstanceBuffer;
u_int Flux_Grass::s_uAllocatedInstances = 0;

Zenith_Vector<GrassChunk> Flux_Grass::s_axChunks;
u_int Flux_Grass::s_uVisibleBladeCount = 0;
u_int Flux_Grass::s_uActiveChunkCount = 0;

bool Flux_Grass::s_bEnabled = true;
float Flux_Grass::s_fDensityScale = 1.0f;
float Flux_Grass::s_fMaxDistance = GrassConfig::fMAX_DISTANCE;
bool Flux_Grass::s_bWindEnabled = true;
float Flux_Grass::s_fWindStrength = 1.0f;
Zenith_Maths::Vector2 Flux_Grass::s_xWindDirection = glm::normalize(Zenith_Maths::Vector2(1.0f, 0.2f));

Flux_DynamicConstantBuffer Flux_Grass::s_xGrassConstantsBuffer;

// Cached binding handles
static Flux_BindingHandle s_xGrassFrameConstantsBinding;
static Flux_BindingHandle s_xGrassParamsBinding;
static Flux_BindingHandle s_xGrassInstanceBinding;

// Grass constants buffer structure
struct GrassConstants
{
	Zenith_Maths::Vector4 m_xWindParams;     // XY = direction, Z = strength, W = time
	Zenith_Maths::Vector4 m_xGrassParams;    // X = density scale, Y = max distance, Z = debug mode, W = pad
	Zenith_Maths::Vector4 m_xLODDistances;   // LOD0, LOD1, LOD2, MAX distances
};
static GrassConstants s_xGrassConstants;

// Debug variables
DEBUGVAR bool dbg_bGrassEnable = true;
DEBUGVAR u_int dbg_uGrassDebugMode = GRASS_DEBUG_NONE;
DEBUGVAR float dbg_fGrassDensityScale = 1.0f;
DEBUGVAR float dbg_fGrassMaxDistance = GrassConfig::fMAX_DISTANCE;
DEBUGVAR bool dbg_bGrassWindEnabled = true;
DEBUGVAR float dbg_fGrassWindStrength = 1.0f;
DEBUGVAR bool dbg_bGrassCullingEnabled = true;
DEBUGVAR bool dbg_bGrassShowChunkGrid = false;
DEBUGVAR bool dbg_bGrassFreezeLOD = false;
DEBUGVAR u_int dbg_uGrassForcedLOD = 0;

// Read-only stats
DEBUGVAR u_int dbg_uGrassBladeCount = 0;
DEBUGVAR u_int dbg_uGrassActiveChunks = 0;
DEBUGVAR float dbg_fGrassBufferUsageMB = 0.0f;

// CPU-side instance storage (populated during generation, uploaded to GPU)
static Zenith_Vector<GrassBladeInstance> s_axAllInstances;
static bool s_bInstancesGenerated = false;
static bool s_bInstancesUploaded = false;

// Simple grass blade mesh (quad with 3 segments)
struct GrassBladeMesh
{
	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
};
static GrassBladeMesh s_xGrassBladeMesh;

void CreateGrassBladeMesh()
{
	// Simple grass blade: 4 vertices, 2 triangles
	// Oriented along Y-axis, centered at base
	struct GrassVertex
	{
		Zenith_Maths::Vector3 m_xPosition;
		Zenith_Maths::Vector2 m_xUV;
	};

	GrassVertex axVertices[] = {
		// Bottom-left
		{ Zenith_Maths::Vector3(-0.5f, 0.0f, 0.0f), Zenith_Maths::Vector2(0.0f, 0.0f) },
		// Bottom-right
		{ Zenith_Maths::Vector3(0.5f, 0.0f, 0.0f), Zenith_Maths::Vector2(1.0f, 0.0f) },
		// Top-left (tip - narrower)
		{ Zenith_Maths::Vector3(-0.2f, 1.0f, 0.0f), Zenith_Maths::Vector2(0.0f, 1.0f) },
		// Top-right (tip - narrower)
		{ Zenith_Maths::Vector3(0.2f, 1.0f, 0.0f), Zenith_Maths::Vector2(1.0f, 1.0f) }
	};

	u_int auIndices[] = {
		0, 1, 2,
		1, 3, 2
	};

	Zenith_Vulkan_MemoryManager::InitialiseVertexBuffer(axVertices, sizeof(axVertices), s_xGrassBladeMesh.m_xVertexBuffer);
	Zenith_Vulkan_MemoryManager::InitialiseIndexBuffer(auIndices, sizeof(auIndices), s_xGrassBladeMesh.m_xIndexBuffer);
}

void Flux_Grass::Initialise()
{
	CreateGrassBladeMesh();
	CreateBuffers();

	// Initialize grass shader
	s_xGrassShader.Initialise("Vegetation/Flux_Grass.vert", "Vegetation/Flux_Grass.frag");

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(Flux_BufferElement(SHADER_DATA_TYPE_FLOAT3));  // POSITION
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(Flux_BufferElement(SHADER_DATA_TYPE_FLOAT2));  // TEXCOORD
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	// Grass uses forward rendering with direct lighting in the fragment shader,
	// so it renders to HDR target (after deferred shading) rather than G-Buffer.
	// This allows proper depth testing against deferred-rendered geometry.
	xPipelineSpec.m_pxTargetSetup = &Flux_HDR::GetHDRSceneTargetSetupWithDepth();
	xPipelineSpec.m_pxShader = &s_xGrassShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;
	xPipelineSpec.m_bDepthTestEnabled = true;
	xPipelineSpec.m_bDepthWriteEnabled = true;
	xPipelineSpec.m_eCullMode = CULL_MODE_NONE;  // Grass is double-sided

	Flux_PipelineLayout& xLayout = xPipelineSpec.m_xPipelineLayout;
	xLayout.m_uNumDescriptorSets = 1;
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Frame constants
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[1].m_eType = DESCRIPTOR_TYPE_BUFFER;  // Grass params
	xLayout.m_axDescriptorSetLayouts[0].m_axBindings[2].m_eType = DESCRIPTOR_TYPE_STORAGE_BUFFER;  // Instance buffer

	Flux_PipelineBuilder::FromSpecification(s_xGrassPipeline, xPipelineSpec);

	// Cache binding handles
	const Flux_ShaderReflection& xReflection = s_xGrassShader.GetReflection();
	s_xGrassFrameConstantsBinding = xReflection.GetBinding("FrameConstants");
	s_xGrassParamsBinding = xReflection.GetBinding("GrassConstants");
	s_xGrassInstanceBinding = xReflection.GetBinding("g_xInstances");

	// Initialize constants buffer
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(&s_xGrassConstants, sizeof(GrassConstants), s_xGrassConstantsBuffer);

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass Initialised");
}

void Flux_Grass::Shutdown()
{
	DestroyBuffers();
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xGrassConstantsBuffer);
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass shut down");
}

void Flux_Grass::Reset()
{
	g_xCommandList.Reset(true);
	s_axChunks.Clear();
	s_uVisibleBladeCount = 0;
	s_uActiveChunkCount = 0;
}

void Flux_Grass::CreateBuffers()
{
	// Create instance buffer for grass blade data
	u_int uBufferSize = GrassConfig::uMAX_TOTAL_INSTANCES * sizeof(GrassBladeInstance);

	Zenith_Vulkan_MemoryManager::InitialiseReadWriteBuffer(nullptr, uBufferSize, s_xInstanceBuffer);
	s_uAllocatedInstances = GrassConfig::uMAX_TOTAL_INSTANCES;
}

void Flux_Grass::DestroyBuffers()
{
	if (s_xInstanceBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_MemoryManager::DestroyReadWriteBuffer(s_xInstanceBuffer);
	}
}

void Flux_Grass::SubmitRenderTask()
{
	Zenith_TaskSystem::SubmitTask(&g_xRenderTask);
}

void Flux_Grass::WaitForRenderTask()
{
	g_xRenderTask.WaitUntilComplete();
}

void Flux_Grass::Render(void*)
{
	if (!dbg_bGrassEnable || !s_bInstancesUploaded)
	{
		return;
	}

	// Sync debug variables
	s_bEnabled = dbg_bGrassEnable;
	s_fDensityScale = dbg_fGrassDensityScale;
	s_fMaxDistance = dbg_fGrassMaxDistance;
	s_bWindEnabled = dbg_bGrassWindEnabled;
	s_fWindStrength = dbg_fGrassWindStrength;

	// Update visibility and LOD each frame
	UpdateVisibleChunks();

	if (s_uVisibleBladeCount == 0)
	{
		return;
	}

	// Update constants
	// Float32 has 24-bit mantissa, giving ~7 decimal digits of precision
	// Wind frequencies max at 4.1x, so after 10 hours (36000s) we have 147600 which is fine
	// No need to wrap - doing so causes visible phase jumps in all sine waves
	double dTime = Zenith_Core::GetTimePassed();
	float fTime = static_cast<float>(dTime);

	s_xGrassConstants.m_xWindParams = Zenith_Maths::Vector4(
		s_xWindDirection.x,
		s_xWindDirection.y,
		s_bWindEnabled ? s_fWindStrength : 0.0f,
		fTime);

	s_xGrassConstants.m_xGrassParams = Zenith_Maths::Vector4(
		s_fDensityScale,
		s_fMaxDistance,
		static_cast<float>(dbg_uGrassDebugMode),
		0.0f);

	s_xGrassConstants.m_xLODDistances = Zenith_Maths::Vector4(
		GrassConfig::fLOD0_DISTANCE,
		GrassConfig::fLOD1_DISTANCE,
		GrassConfig::fLOD2_DISTANCE,
		s_fMaxDistance);

	Flux_MemoryManager::UploadBufferData(s_xGrassConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xGrassConstants, sizeof(GrassConstants));

	g_xCommandList.Reset(false);

	g_xCommandList.AddCommand<Flux_CommandSetPipeline>(&s_xGrassPipeline);
	g_xCommandList.AddCommand<Flux_CommandSetVertexBuffer>(&s_xGrassBladeMesh.m_xVertexBuffer);
	g_xCommandList.AddCommand<Flux_CommandSetIndexBuffer>(&s_xGrassBladeMesh.m_xIndexBuffer);

	{
		Flux_ShaderBinder xBinder(g_xCommandList);
		xBinder.BindCBV(s_xGrassFrameConstantsBinding, &Flux_Graphics::s_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(s_xGrassParamsBinding, &s_xGrassConstantsBuffer.GetCBV());
		xBinder.BindUAV_Buffer(s_xGrassInstanceBinding, &s_xInstanceBuffer.GetUAV());
	}

	// Draw instanced grass (6 indices per blade, s_uVisibleBladeCount instances)
	g_xCommandList.AddCommand<Flux_CommandDrawIndexed>(6, s_uVisibleBladeCount);

	// Submit to HDR target - grass is forward-rendered after deferred shading
	Flux::SubmitCommandList(&g_xCommandList, Flux_HDR::GetHDRSceneTargetSetupWithDepth(), RENDER_ORDER_FOLIAGE);

	// Update stats
	dbg_uGrassBladeCount = s_uVisibleBladeCount;
	dbg_uGrassActiveChunks = s_uActiveChunkCount;
	dbg_fGrassBufferUsageMB = (s_uVisibleBladeCount * sizeof(GrassBladeInstance)) / (1024.0f * 1024.0f);
}

void Flux_Grass::GenerateGrassForChunk(GrassChunk& xChunk, const Zenith_Maths::Vector3& xCenter)
{
	// This function is called by OnTerrainChunkLoaded for individual chunks
	// For full terrain generation, use GenerateFromTerrain() instead
	xChunk.m_xCenter = xCenter;
	xChunk.m_fRadius = GrassConfig::fCHUNK_SIZE * 0.707f;  // Diagonal
	xChunk.m_uInstanceOffset = 0;
	xChunk.m_uInstanceCount = 0;  // Will be populated by GenerateFromTerrain
	xChunk.m_uLOD = 0;
	xChunk.m_bVisible = false;
}

void Flux_Grass::UpdateVisibleChunks()
{
	s_uVisibleBladeCount = 0;
	s_uActiveChunkCount = 0;

	if (s_axChunks.GetSize() == 0)
	{
		return;
	}

	// Get camera position and frustum for culling
	const Zenith_Maths::Vector3& xCamPos = Flux_Graphics::GetCameraPosition();
	Zenith_Maths::Matrix4 xViewProj = Flux_Graphics::GetViewProjMatrix();

	// Extract frustum planes for culling
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProj);

	for (u_int i = 0; i < s_axChunks.GetSize(); ++i)
	{
		GrassChunk& xChunk = s_axChunks.Get(i);

		if (xChunk.m_uInstanceCount == 0)
		{
			xChunk.m_bVisible = false;
			continue;
		}

		// Distance culling
		float fDistSq = glm::distance2(xCamPos, xChunk.m_xCenter);
		float fMaxDistSq = s_fMaxDistance * s_fMaxDistance;

		if (dbg_bGrassCullingEnabled && fDistSq > fMaxDistSq)
		{
			xChunk.m_bVisible = false;
			continue;
		}

		// Frustum culling using AABB converted from bounding sphere
		bool bInFrustum = true;
		if (dbg_bGrassCullingEnabled)
		{
			// Convert sphere to AABB for frustum testing
			Zenith_AABB xChunkAABB;
			xChunkAABB.m_xMin = xChunk.m_xCenter - Zenith_Maths::Vector3(xChunk.m_fRadius);
			xChunkAABB.m_xMax = xChunk.m_xCenter + Zenith_Maths::Vector3(xChunk.m_fRadius);
			bInFrustum = Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xChunkAABB);
		}

		if (!bInFrustum)
		{
			xChunk.m_bVisible = false;
			continue;
		}

		// LOD selection based on distance (using squared distances to avoid sqrt)
		if (!dbg_bGrassFreezeLOD)
		{
			constexpr float fLOD0DistSq = GrassConfig::fLOD0_DISTANCE * GrassConfig::fLOD0_DISTANCE;
			constexpr float fLOD1DistSq = GrassConfig::fLOD1_DISTANCE * GrassConfig::fLOD1_DISTANCE;
			constexpr float fLOD2DistSq = GrassConfig::fLOD2_DISTANCE * GrassConfig::fLOD2_DISTANCE;

			if (fDistSq < fLOD0DistSq)
			{
				xChunk.m_uLOD = 0;
			}
			else if (fDistSq < fLOD1DistSq)
			{
				xChunk.m_uLOD = 1;
			}
			else if (fDistSq < fLOD2DistSq)
			{
				xChunk.m_uLOD = 2;
			}
			else
			{
				xChunk.m_uLOD = 3;
			}
		}
		else
		{
			xChunk.m_uLOD = dbg_uGrassForcedLOD;
		}

		xChunk.m_bVisible = true;

		// Apply LOD-based density reduction
		// LOD0 = 100%, LOD1 = 50%, LOD2 = 25%, LOD3 = 12.5%
		u_int uLODInstanceCount = xChunk.m_uInstanceCount;
		switch (xChunk.m_uLOD)
		{
		case 0: break;  // Full density
		case 1: uLODInstanceCount = xChunk.m_uInstanceCount / 2; break;
		case 2: uLODInstanceCount = xChunk.m_uInstanceCount / 4; break;
		case 3: uLODInstanceCount = xChunk.m_uInstanceCount / 8; break;
		default: uLODInstanceCount = xChunk.m_uInstanceCount / 8; break;
		}

		s_uVisibleBladeCount += uLODInstanceCount;
		s_uActiveChunkCount++;
	}
}

void Flux_Grass::UploadInstanceData()
{
	if (!s_bInstancesGenerated || s_axAllInstances.GetSize() == 0)
	{
		return;
	}

	if (!s_xInstanceBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Instance buffer not valid, cannot upload");
		return;
	}

	// Calculate upload size
	u_int uUploadSize = static_cast<u_int>(s_axAllInstances.GetSize() * sizeof(GrassBladeInstance));
	u_int uBufferSize = s_uAllocatedInstances * sizeof(GrassBladeInstance);

	if (uUploadSize > uBufferSize)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Instance data (%u bytes) exceeds buffer size (%u bytes), truncating",
			uUploadSize, uBufferSize);
		uUploadSize = uBufferSize;
	}

	// Upload to GPU
	Zenith_Vulkan_MemoryManager::UploadBufferData(
		s_xInstanceBuffer.GetBuffer().m_xVRAMHandle,
		s_axAllInstances.GetDataPointer(),
		static_cast<size_t>(uUploadSize));

	s_bInstancesUploaded = true;
	s_uVisibleBladeCount = static_cast<u_int>(s_axAllInstances.GetSize());

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Uploaded %u instances (%.2f MB)",
		s_axAllInstances.GetSize(), uUploadSize / (1024.0f * 1024.0f));
}

void Flux_Grass::GenerateFromTerrain(const Flux_MeshGeometry& xTerrainMesh)
{
	// Validate terrain mesh has required data
	if (!xTerrainMesh.m_pxPositions || !xTerrainMesh.m_pxNormals || !xTerrainMesh.m_puIndices)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Cannot generate - terrain mesh missing position/normal/index data");
		return;
	}

	const u_int uNumIndices = xTerrainMesh.GetNumIndices();
	const u_int uNumTriangles = uNumIndices / 3;

	if (uNumTriangles == 0)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Cannot generate - terrain mesh has no triangles");
		return;
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Generating grass from terrain mesh (%u triangles)...", uNumTriangles);

	// Clear existing instances
	s_axAllInstances.Clear();
	s_axChunks.Clear();
	s_bInstancesGenerated = false;
	s_bInstancesUploaded = false;

	// Terrain data pointers
	const Zenith_Maths::Vector3* pxPositions = xTerrainMesh.m_pxPositions;
	const Zenith_Maths::Vector3* pxNormals = xTerrainMesh.m_pxNormals;
	const float* pfMaterialLerps = xTerrainMesh.m_pfMaterialLerps;
	const Flux_MeshGeometry::IndexType* puIndices = xTerrainMesh.m_puIndices;

	// Random number generator for grass placement
	std::mt19937 xRng(42);  // Fixed seed for reproducibility
	std::uniform_real_distribution<float> xUniform01(0.0f, 1.0f);
	std::uniform_real_distribution<float> xRotationDist(0.0f, 6.28318f);  // 0 to 2*PI
	std::uniform_real_distribution<float> xHeightDist(0.3f, 0.8f);  // Blade height variation
	std::uniform_real_distribution<float> xWidthDist(0.02f, 0.05f);  // Blade width
	std::uniform_real_distribution<float> xBendDist(0.0f, 0.3f);  // Initial bend
	std::uniform_int_distribution<u_int> xGreenDist(180, 255);   // Primary green variation
	std::uniform_int_distribution<u_int> xColorOffset(0, 40);    // Color variation offset
	std::uniform_real_distribution<float> xColorBlend(0.0f, 1.0f);  // For blending healthy/dry grass

	// Grass density threshold - only place grass where MaterialLerp < this value
	// MaterialLerp = 0 means 100% material 0 (grass), MaterialLerp = 1 means 100% material 1 (rock/dirt)
	constexpr float fGrassThreshold = 0.5f;

	// Target blades per square meter (adjusted by density scale)
	const float fBladesPerSqm = static_cast<float>(GrassConfig::uBLADES_PER_SQM) * s_fDensityScale;

	u_int uTotalTrianglesProcessed = 0;
	u_int uTotalBladesGenerated = 0;

	// Process each triangle
	for (u_int uTri = 0; uTri < uNumTriangles; ++uTri)
	{
		const u_int uIdx0 = puIndices[uTri * 3 + 0];
		const u_int uIdx1 = puIndices[uTri * 3 + 1];
		const u_int uIdx2 = puIndices[uTri * 3 + 2];

		const Zenith_Maths::Vector3& xPos0 = pxPositions[uIdx0];
		const Zenith_Maths::Vector3& xPos1 = pxPositions[uIdx1];
		const Zenith_Maths::Vector3& xPos2 = pxPositions[uIdx2];

		// Get material lerp values (if available)
		float fLerp0 = pfMaterialLerps ? pfMaterialLerps[uIdx0] : 0.0f;
		float fLerp1 = pfMaterialLerps ? pfMaterialLerps[uIdx1] : 0.0f;
		float fLerp2 = pfMaterialLerps ? pfMaterialLerps[uIdx2] : 0.0f;

		// Average material lerp for triangle
		float fAvgLerp = (fLerp0 + fLerp1 + fLerp2) / 3.0f;

		// Skip triangles that are mostly non-grass material
		if (fAvgLerp > fGrassThreshold)
		{
			continue;
		}

		// Calculate triangle area using cross product
		Zenith_Maths::Vector3 xEdge1 = xPos1 - xPos0;
		Zenith_Maths::Vector3 xEdge2 = xPos2 - xPos0;
		Zenith_Maths::Vector3 xCross = glm::cross(xEdge1, xEdge2);
		float fArea = glm::length(xCross) * 0.5f;

		// Skip degenerate triangles
		if (fArea < 0.001f)
		{
			continue;
		}

		// Calculate number of blades based on area
		// Reduce density based on material lerp (less grass as we approach threshold)
		float fDensityMultiplier = 1.0f - (fAvgLerp / fGrassThreshold);
		u_int uNumBlades = static_cast<u_int>(fArea * fBladesPerSqm * fDensityMultiplier);

		// Cap blades per triangle to prevent overgeneration
		uNumBlades = std::min(uNumBlades, 100u);

		// Get triangle normals for interpolation
		const Zenith_Maths::Vector3& xNorm0 = pxNormals[uIdx0];
		const Zenith_Maths::Vector3& xNorm1 = pxNormals[uIdx1];
		const Zenith_Maths::Vector3& xNorm2 = pxNormals[uIdx2];

		// Generate blades using random barycentric coordinates
		for (u_int uBlade = 0; uBlade < uNumBlades; ++uBlade)
		{
			// Stop if we've hit the instance limit
			if (s_axAllInstances.GetSize() >= GrassConfig::uMAX_TOTAL_INSTANCES)
			{
				Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Hit instance limit (%u)", GrassConfig::uMAX_TOTAL_INSTANCES);
				goto generation_complete;
			}

			// Random barycentric coordinates
			float fU = xUniform01(xRng);
			float fV = xUniform01(xRng);
			if (fU + fV > 1.0f)
			{
				fU = 1.0f - fU;
				fV = 1.0f - fV;
			}
			float fW = 1.0f - fU - fV;

			// Interpolate position
			Zenith_Maths::Vector3 xPosition = xPos0 * fW + xPos1 * fU + xPos2 * fV;

			// Interpolate normal
			Zenith_Maths::Vector3 xNormal = glm::normalize(xNorm0 * fW + xNorm1 * fU + xNorm2 * fV);

			// Interpolate material lerp and use for additional filtering
			float fLocalLerp = fLerp0 * fW + fLerp1 * fU + fLerp2 * fV;
			if (fLocalLerp > fGrassThreshold)
			{
				continue;  // Skip this blade if local lerp is too high
			}

			// Offset position slightly along normal to prevent z-fighting
			xPosition += xNormal * 0.01f;

			// Create blade instance
			GrassBladeInstance xInstance;
			xInstance.m_xPosition = xPosition;
			xInstance.m_fRotation = xRotationDist(xRng);
			xInstance.m_fHeight = xHeightDist(xRng);
			xInstance.m_fWidth = xWidthDist(xRng);
			xInstance.m_fBend = xBendDist(xRng);

			// Pack color tint with natural grass color variation
			// Blend between healthy green and dry/yellowed grass for realism
			float fBlend = xColorBlend(xRng);
			bool bDryGrass = fBlend < 0.15f;  // ~15% of blades are dry/yellow

			u_int uR, uG, uB;
			if (bDryGrass)
			{
				// Dry/yellowed grass: more yellow-brown tones
				uR = 140 + xColorOffset(xRng);    // 140-180
				uG = 150 + xColorOffset(xRng);    // 150-190
				uB = 50 + xColorOffset(xRng) / 2; // 50-70
			}
			else
			{
				// Healthy green grass with natural variation
				u_int uBaseGreen = xGreenDist(xRng);  // 180-255
				uR = 40 + xColorOffset(xRng);         // 40-80 (some red for warmth)
				uG = uBaseGreen;                       // Primary green channel
				uB = 20 + xColorOffset(xRng) / 2;     // 20-40 (low blue for grass)
			}
			xInstance.m_uColorTint = (255 << 24) | (uB << 16) | (uG << 8) | uR;

			s_axAllInstances.PushBack(xInstance);
			uTotalBladesGenerated++;
		}

		uTotalTrianglesProcessed++;
	}

generation_complete:
	s_bInstancesGenerated = true;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Generated %u blades from %u triangles",
		uTotalBladesGenerated, uTotalTrianglesProcessed);

	// Shuffle instances to ensure even spatial distribution for LOD
	// Without shuffling, instances are ordered by terrain triangle traversal,
	// so drawing only the first N/4 instances (for LOD2) would show grass
	// only in one area of the terrain. Shuffling ensures LOD reduction
	// removes blades evenly across the entire terrain.
	if (s_axAllInstances.GetSize() > 1)
	{
		std::shuffle(s_axAllInstances.GetDataPointer(),
			s_axAllInstances.GetDataPointer() + s_axAllInstances.GetSize(),
			xRng);
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Shuffled instances for even LOD distribution");
	}

	// Create a single chunk containing all instances for now
	// (Future: subdivide into spatial chunks for better culling)
	if (s_axAllInstances.GetSize() > 0)
	{
		// Calculate bounding sphere
		Zenith_Maths::Vector3 xMinBounds(FLT_MAX);
		Zenith_Maths::Vector3 xMaxBounds(-FLT_MAX);

		for (u_int i = 0; i < s_axAllInstances.GetSize(); ++i)
		{
			const Zenith_Maths::Vector3& xPos = s_axAllInstances.Get(i).m_xPosition;
			xMinBounds = glm::min(xMinBounds, xPos);
			xMaxBounds = glm::max(xMaxBounds, xPos);
		}

		GrassChunk xChunk;
		xChunk.m_xCenter = (xMinBounds + xMaxBounds) * 0.5f;
		xChunk.m_fRadius = glm::length(xMaxBounds - xMinBounds) * 0.5f;
		xChunk.m_uInstanceOffset = 0;
		xChunk.m_uInstanceCount = static_cast<u_int>(s_axAllInstances.GetSize());
		xChunk.m_uLOD = 0;
		xChunk.m_bVisible = true;

		s_axChunks.PushBack(xChunk);
	}

	// Upload to GPU
	UploadInstanceData();

	// Initialize visibility
	UpdateVisibleChunks();
}

// Setters with input validation
void Flux_Grass::SetEnabled(bool bEnabled) { s_bEnabled = bEnabled; dbg_bGrassEnable = bEnabled; }
void Flux_Grass::SetDensityScale(float fScale) { s_fDensityScale = std::clamp(fScale, 0.0f, 10.0f); dbg_fGrassDensityScale = s_fDensityScale; }
void Flux_Grass::SetMaxDistance(float fDistance) { s_fMaxDistance = std::clamp(fDistance, 10.0f, 1000.0f); dbg_fGrassMaxDistance = s_fMaxDistance; }
void Flux_Grass::SetWindEnabled(bool bEnabled) { s_bWindEnabled = bEnabled; dbg_bGrassWindEnabled = bEnabled; }
void Flux_Grass::SetWindStrength(float fStrength) { s_fWindStrength = std::clamp(fStrength, 0.0f, 10.0f); dbg_fGrassWindStrength = s_fWindStrength; }
void Flux_Grass::SetWindDirection(const Zenith_Maths::Vector2& xDirection)
{
	float fLenSq = glm::dot(xDirection, xDirection);
	if (fLenSq > 0.0001f)
	{
		s_xWindDirection = xDirection / sqrtf(fLenSq);
	}
	else
	{
		s_xWindDirection = Zenith_Maths::Vector2(1.0f, 0.0f);
	}
}

// Getters
bool Flux_Grass::IsEnabled() { return s_bEnabled; }
float Flux_Grass::GetDensityScale() { return s_fDensityScale; }
float Flux_Grass::GetMaxDistance() { return s_fMaxDistance; }
bool Flux_Grass::IsWindEnabled() { return s_bWindEnabled; }
float Flux_Grass::GetWindStrength() { return s_fWindStrength; }
const Zenith_Maths::Vector2& Flux_Grass::GetWindDirection() { return s_xWindDirection; }

// Stats
u_int Flux_Grass::GetVisibleBladeCount() { return s_uVisibleBladeCount; }
u_int Flux_Grass::GetActiveChunkCount() { return s_uActiveChunkCount; }
float Flux_Grass::GetBufferUsageMB() { return (s_uVisibleBladeCount * sizeof(GrassBladeInstance)) / (1024.0f * 1024.0f); }

#ifdef ZENITH_TOOLS
void Flux_Grass::RegisterDebugVariables()
{
	Zenith_DebugVariables::AddBoolean({ "Flux", "Grass", "Enable" }, dbg_bGrassEnable);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Grass", "DebugMode" }, dbg_uGrassDebugMode, 0, GRASS_DEBUG_COUNT - 1);
	Zenith_DebugVariables::AddFloat({ "Flux", "Grass", "DensityScale" }, dbg_fGrassDensityScale, 0.0f, 5.0f);
	Zenith_DebugVariables::AddFloat({ "Flux", "Grass", "MaxDistance" }, dbg_fGrassMaxDistance, 50.0f, 500.0f);
	Zenith_DebugVariables::AddBoolean({ "Flux", "Grass", "WindEnabled" }, dbg_bGrassWindEnabled);
	Zenith_DebugVariables::AddFloat({ "Flux", "Grass", "WindStrength" }, dbg_fGrassWindStrength, 0.0f, 5.0f);
	Zenith_DebugVariables::AddBoolean({ "Flux", "Grass", "CullingEnabled" }, dbg_bGrassCullingEnabled);
	Zenith_DebugVariables::AddBoolean({ "Flux", "Grass", "ShowChunkGrid" }, dbg_bGrassShowChunkGrid);
	Zenith_DebugVariables::AddBoolean({ "Flux", "Grass", "FreezeLOD" }, dbg_bGrassFreezeLOD);
	Zenith_DebugVariables::AddUInt32({ "Flux", "Grass", "ForcedLOD" }, dbg_uGrassForcedLOD, 0, 3);

	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Flux", "Grass", "Stats", "BladeCount" }, dbg_uGrassBladeCount);
	Zenith_DebugVariables::AddUInt32_ReadOnly({ "Flux", "Grass", "Stats", "ActiveChunks" }, dbg_uGrassActiveChunks);
	Zenith_DebugVariables::AddFloat_ReadOnly({ "Flux", "Grass", "Stats", "BufferUsageMB" }, dbg_fGrassBufferUsageMB);
}
#endif
