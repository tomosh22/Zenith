#include "Zenith.h"

#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Core/Zenith_Engine.h"
#include "Core/FrameContext.h"
#include "Flux/Flux_BackendTypes.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/HDR/Flux_HDRImpl.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Terrain/Flux_TerrainConfig.h"
#include "TaskSystem/Zenith_TaskSystem.h"
#include "Maths/Zenith_FrustumCulling.h"
#include "Core/Zenith_GraphicsOptions.h"
#include <random>
#include <algorithm>

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

// Phase 7g: subsystem state moved to Flux_GrassImpl held by Zenith_Engine.

// Grass constants buffer structure — type is .cpp-local, instance kept file-static.
struct GrassConstants
{
	Zenith_Maths::Vector4 m_xWindParams;     // XY = direction, Z = strength, W = time
	Zenith_Maths::Vector4 m_xGrassParams;    // X = density scale, Y = max distance, Z = debug mode, W = pad
	Zenith_Maths::Vector4 m_xLODDistances;   // LOD0, LOD1, LOD2, MAX distances
};
static GrassConstants s_xGrassConstants;

// Debug variables
DEBUGVAR u_int dbg_uGrassDebugMode = GRASS_DEBUG_NONE;
DEBUGVAR float dbg_fGrassDensityScale = 1.0f;
DEBUGVAR float dbg_fGrassMaxDistance = GrassConfig::fMAX_DISTANCE;
DEBUGVAR float dbg_fGrassWindStrength = 1.0f;
DEBUGVAR bool dbg_bGrassShowChunkGrid = false;
DEBUGVAR bool dbg_bGrassFreezeLOD = false;
DEBUGVAR u_int dbg_uGrassForcedLOD = 0;

// Simple grass blade mesh (quad with 3 segments)
struct GrassBladeMesh
{
	Flux_VertexBuffer m_xVertexBuffer;
	Flux_IndexBuffer m_xIndexBuffer;
};
static GrassBladeMesh s_xGrassBladeMesh;

void Flux_GrassImpl::CreateGrassBladeMesh()
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

	g_xEngine.FluxMemory().InitialiseVertexBuffer(axVertices, sizeof(axVertices), s_xGrassBladeMesh.m_xVertexBuffer);
	g_xEngine.FluxMemory().InitialiseIndexBuffer(auIndices, sizeof(auIndices), s_xGrassBladeMesh.m_xIndexBuffer);
}

static void ExecuteRender(Flux_CommandBuffer* pxCmdList, void* pUserData);

void Flux_GrassImpl::BuildPipelines()
{
	// Initialize grass shader
	m_xGrassShader.Initialise(FluxShaderProgram::Grass);

	Flux_VertexInputDescription xVertexDesc;
	xVertexDesc.m_eTopology = MESH_TOPOLOGY_TRIANGLES;
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(Flux_BufferElement(SHADER_DATA_TYPE_FLOAT3));  // POSITION
	xVertexDesc.m_xPerVertexLayout.GetElements().PushBack(Flux_BufferElement(SHADER_DATA_TYPE_FLOAT2));  // TEXCOORD
	xVertexDesc.m_xPerVertexLayout.CalculateOffsetsAndStrides();

	Flux_PipelineSpecification xPipelineSpec;
	xPipelineSpec.m_aeColourAttachmentFormats[0] = HDR_SCENE_FORMAT;
	xPipelineSpec.m_uNumColourAttachments = 1;
	xPipelineSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
	xPipelineSpec.m_pxShader = &m_xGrassShader;
	xPipelineSpec.m_xVertexInputDesc = xVertexDesc;
	xPipelineSpec.m_bDepthTestEnabled = true;
	// Depth WRITE must stay off: the graph binds the scene depth as a
	// READ-ONLY attachment for this pass (WRITE_DSV + READ_DEPTH declaration
	// => bDepthReadOnly render pass) — blades depth-test against the opaque
	// scene but never write it.
	xPipelineSpec.m_bDepthWriteEnabled = false;
	xPipelineSpec.m_eCullMode = CULL_MODE_NONE;  // Grass is double-sided

	m_xGrassShader.GetReflection().PopulateLayout(xPipelineSpec.m_xPipelineLayout);

	Flux_PipelineBuilder::FromSpecification(m_xGrassPipeline, xPipelineSpec);
}

void Flux_GrassImpl::Initialise()
{
	CreateGrassBladeMesh();
	CreateBuffers();

	BuildPipelines();

	// Initialize constants buffer
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&s_xGrassConstants, sizeof(GrassConstants), m_xGrassConstantsBuffer);

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass Initialised");
}

void Flux_GrassImpl::Shutdown()
{
	DestroyBuffers();
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyDynamicConstantBuffer(m_xGrassConstantsBuffer);
	xVulkanMemory.DestroyVertexBuffer(s_xGrassBladeMesh.m_xVertexBuffer);
	xVulkanMemory.DestroyIndexBuffer(s_xGrassBladeMesh.m_xIndexBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass shut down");
}

void Flux_GrassImpl::Reset()
{
	// Reset is handled by the render graph
	m_axChunks.Clear();
	m_uVisibleBladeCount = 0;
	m_uActiveChunkCount = 0;
}

void Flux_GrassImpl::CreateBuffers()
{
	// Create instance buffer for grass blade data
	u_int uBufferSize = GrassConfig::uMAX_TOTAL_INSTANCES * sizeof(GrassBladeInstance);

	g_xEngine.FluxMemory().InitialiseReadWriteBuffer(nullptr, uBufferSize, m_xInstanceBuffer);
	m_uAllocatedInstances = GrassConfig::uMAX_TOTAL_INSTANCES;
}

void Flux_GrassImpl::DestroyBuffers()
{
	if (m_xInstanceBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		g_xEngine.FluxMemory().DestroyReadWriteBuffer(m_xInstanceBuffer);
	}
}

void Flux_GrassImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	// Forward pass over the lit HDR scene (the setup walk runs this AFTER
	// DeferredShading, so the HDR clear + lighting have already happened).
	// READ_DEPTH binds the scene depth as a READ-ONLY depth attachment: the
	// blades depth-test against the opaque scene without writing depth.
	// Do NOT clear: both attachments carry live scene contents.
	xGraph.AddPass("Grass", ExecuteRender)
		.Writes(g_xEngine.HDR().GetHDRSceneTarget(),                 RESOURCE_ACCESS_WRITE_RTV)
		.Reads (g_xEngine.FluxGraphics().GetDepthAttachment(),       RESOURCE_ACCESS_READ_DEPTH);
}

static void ExecuteRender(Flux_CommandBuffer* pxCmdList, void*)
{
	// Static graph trampoline: recover the subsystem once; other engine reaches
	// (VulkanMemory / Frame / FluxGraphics) go via g_xEngine at point of use.
	Flux_GrassImpl& xGrass = g_xEngine.Grass();

	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	if (!xOpts.m_bGrassEnabled || !xGrass.m_bInstancesUploaded)
	{
		return;
	}

	// Sync debug variables (continuous parameters)
	xGrass.m_fDensityScale = dbg_fGrassDensityScale;
	xGrass.m_fMaxDistance = dbg_fGrassMaxDistance;
	xGrass.m_fWindStrength = dbg_fGrassWindStrength;

	// Update visibility and LOD each frame
	xGrass.UpdateVisibleChunks();

	if (xGrass.m_uVisibleBladeCount == 0)
	{
		return;
	}

	// Update constants
	// Float32 has 24-bit mantissa, giving ~7 decimal digits of precision
	// Wind frequencies max at 4.1x, so after 10 hours (36000s) we have 147600 which is fine
	// No need to wrap - doing so causes visible phase jumps in all sine waves
	double dTime = g_xEngine.Frame().GetTimePassed();
	float fTime = static_cast<float>(dTime);

	s_xGrassConstants.m_xWindParams = Zenith_Maths::Vector4(
		xGrass.m_xWindDirection.x,
		xGrass.m_xWindDirection.y,
		xOpts.m_bGrassWindEnabled ? xGrass.m_fWindStrength : 0.0f,
		fTime);

	s_xGrassConstants.m_xGrassParams = Zenith_Maths::Vector4(
		xGrass.m_fDensityScale,
		xGrass.m_fMaxDistance,
		static_cast<float>(dbg_uGrassDebugMode),
		0.0f);

	s_xGrassConstants.m_xLODDistances = Zenith_Maths::Vector4(
		GrassConfig::fLOD0_DISTANCE,
		GrassConfig::fLOD1_DISTANCE,
		GrassConfig::fLOD2_DISTANCE,
		xGrass.m_fMaxDistance);

	g_xEngine.FluxMemory().UploadBufferData(xGrass.m_xGrassConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xGrassConstants, sizeof(GrassConstants));

	pxCmdList->SetPipeline(&xGrass.m_xGrassPipeline);
	pxCmdList->SetVertexBuffer(s_xGrassBladeMesh.m_xVertexBuffer);
	pxCmdList->SetIndexBuffer(s_xGrassBladeMesh.m_xIndexBuffer);

	{
		Flux_ShaderBinder xBinder(*pxCmdList);
		xBinder.BindCBV(xGrass.m_xGrassShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());
		xBinder.BindCBV(xGrass.m_xGrassShader, "GrassConstants", &xGrass.m_xGrassConstantsBuffer.GetCBV());
		xBinder.BindUAV_Buffer(xGrass.m_xGrassShader, "InstanceBuffer", &xGrass.m_xInstanceBuffer.GetUAV());
	}

	// Draw instanced grass (6 indices per blade, xGrass.m_uVisibleBladeCount instances)
	pxCmdList->DrawIndexed(6, xGrass.m_uVisibleBladeCount);
}

void Flux_GrassImpl::GenerateGrassForChunk(GrassChunk& xChunk, const Zenith_Maths::Vector3& xCenter)
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

static bool Flux_Grass_IsChunkInFrustum(const GrassChunk& xChunk, const Zenith_Frustum& xFrustum)
{
	// Convert the chunk's bounding sphere into an AABB and test against the
	// frustum. Kept as its own helper so UpdateVisibleChunks stays focused on
	// the culling *policy* rather than the geometry plumbing.
	Zenith_AABB xChunkAABB;
	xChunkAABB.m_xMin = xChunk.m_xCenter - Zenith_Maths::Vector3(xChunk.m_fRadius);
	xChunkAABB.m_xMax = xChunk.m_xCenter + Zenith_Maths::Vector3(xChunk.m_fRadius);
	return Zenith_FrustumCulling::TestAABBFrustum(xFrustum, xChunkAABB);
}

static u_int Flux_Grass_PickChunkLOD(float fDistSq)
{
	// Pure function: distance squared to LOD index. Forced-LOD override is
	// applied by the caller so this stays trivially testable.
	constexpr float fLOD0DistSq = GrassConfig::fLOD0_DISTANCE * GrassConfig::fLOD0_DISTANCE;
	constexpr float fLOD1DistSq = GrassConfig::fLOD1_DISTANCE * GrassConfig::fLOD1_DISTANCE;
	constexpr float fLOD2DistSq = GrassConfig::fLOD2_DISTANCE * GrassConfig::fLOD2_DISTANCE;

	if (fDistSq < fLOD0DistSq) return 0;
	if (fDistSq < fLOD1DistSq) return 1;
	if (fDistSq < fLOD2DistSq) return 2;
	return 3;
}

static u_int Flux_Grass_InstanceCountForLOD(u_int uTotal, u_int uLOD)
{
	// LOD0 = 100%, LOD1 = 50%, LOD2 = 25%, LOD3+ = 12.5%.
	switch (uLOD)
	{
	case 0: return uTotal;
	case 1: return uTotal / 2;
	case 2: return uTotal / 4;
	default: return uTotal / 8;
	}
}

void Flux_GrassImpl::UpdateVisibleChunks()
{
	m_uVisibleBladeCount = 0;
	m_uActiveChunkCount = 0;

	if (m_axChunks.GetSize() == 0)
	{
		return;
	}

	const Zenith_Maths::Vector3& xCamPos = g_xEngine.FluxGraphics().GetCameraPosition();
	const Zenith_Maths::Matrix4 xViewProj = g_xEngine.FluxGraphics().GetViewProjMatrix();
	Zenith_Frustum xFrustum;
	xFrustum.ExtractFromViewProjection(xViewProj);

	const float fMaxDistSq = m_fMaxDistance * m_fMaxDistance;

	for (u_int i = 0; i < m_axChunks.GetSize(); ++i)
	{
		GrassChunk& xChunk = m_axChunks.Get(i);

		if (xChunk.m_uInstanceCount == 0)
		{
			xChunk.m_bVisible = false;
			continue;
		}

		const float fDistSq = glm::distance2(xCamPos, xChunk.m_xCenter);

		// Distance + frustum culling — both gated on the GraphicsOptions toggle so
		// forcing it off keeps everything visible for debug viewing.
		if (Zenith_GraphicsOptions::Get().m_bGrassCullingEnabled)
		{
			if (fDistSq > fMaxDistSq || !Flux_Grass_IsChunkInFrustum(xChunk, xFrustum))
			{
				xChunk.m_bVisible = false;
				continue;
			}
		}

		xChunk.m_uLOD = dbg_bGrassFreezeLOD ? dbg_uGrassForcedLOD : Flux_Grass_PickChunkLOD(fDistSq);
		xChunk.m_bVisible = true;

		m_uVisibleBladeCount += Flux_Grass_InstanceCountForLOD(xChunk.m_uInstanceCount, xChunk.m_uLOD);
		m_uActiveChunkCount++;
	}
}

void Flux_GrassImpl::UploadInstanceData()
{
	if (!m_bInstancesGenerated || m_axAllInstances.GetSize() == 0)
	{
		return;
	}

	if (!m_xInstanceBuffer.GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Instance buffer not valid, cannot upload");
		return;
	}

	// Calculate upload size
	u_int uUploadSize = static_cast<u_int>(m_axAllInstances.GetSize() * sizeof(GrassBladeInstance));
	u_int uBufferSize = m_uAllocatedInstances * sizeof(GrassBladeInstance);

	if (uUploadSize > uBufferSize)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Instance data (%u bytes) exceeds buffer size (%u bytes), truncating",
			uUploadSize, uBufferSize);
		uUploadSize = uBufferSize;
	}

	// Upload to GPU
	g_xEngine.FluxMemory().UploadBufferData(
		m_xInstanceBuffer.GetBuffer().m_xVRAMHandle,
		m_axAllInstances.GetDataPointer(),
		static_cast<size_t>(uUploadSize));

	m_bInstancesUploaded = true;
	m_uVisibleBladeCount = static_cast<u_int>(m_axAllInstances.GetSize());

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Uploaded %u instances (%.2f MB)",
		m_axAllInstances.GetSize(), uUploadSize / (1024.0f * 1024.0f));
}

namespace
{
	// RNG bundle — one fixed-seed mt19937 shared across all per-blade distributions
	// so calls that re-generate with the same terrain produce identical output.
	struct GrassGenRng
	{
		std::mt19937 xRng{42};
		std::uniform_real_distribution<float> xUniform01{0.0f, 1.0f};
		std::uniform_real_distribution<float> xRotationDist{0.0f, 6.28318f};
		std::uniform_real_distribution<float> xHeightDist{0.3f, 0.8f};
		std::uniform_real_distribution<float> xWidthDist{0.02f, 0.05f};
		std::uniform_real_distribution<float> xBendDist{0.0f, 0.3f};
		std::uniform_int_distribution<u_int> xGreenDist{180, 255};
		std::uniform_int_distribution<u_int> xColorOffset{0, 40};
		std::uniform_real_distribution<float> xColorBlend{0.0f, 1.0f};
	};

	struct TriangleSamples
	{
		Zenith_Maths::Vector3 xPos0;
		Zenith_Maths::Vector3 xPos1;
		Zenith_Maths::Vector3 xPos2;
		Zenith_Maths::Vector3 xNorm0;
		Zenith_Maths::Vector3 xNorm1;
		Zenith_Maths::Vector3 xNorm2;
		float fLerp0;
		float fLerp1;
		float fLerp2;
	};

	// Grass density threshold - only place grass where MaterialLerp < this value.
	// MaterialLerp = 0 means 100% material 0 (grass), 1 means 100% material 1 (rock/dirt).
	constexpr float fGRASS_THRESHOLD = 0.5f;
}

// Generates blades for one triangle. Returns false once the global instance
// limit is hit so the outer triangle loop can stop early.
static bool GenerateBladesForTriangle(
	const TriangleSamples& xTri,
	u_int uNumBlades,
	GrassGenRng& xRng,
	Zenith_Vector<GrassBladeInstance>& xAllInstances,
	u_int& uTotalBladesGenerated)
{
	for (u_int uBlade = 0; uBlade < uNumBlades; ++uBlade)
	{
		if (xAllInstances.GetSize() >= GrassConfig::uMAX_TOTAL_INSTANCES)
		{
			Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Hit instance limit (%u)", GrassConfig::uMAX_TOTAL_INSTANCES);
			return false;
		}

		// Random barycentric coordinates (reflect if outside the triangle).
		float fU = xRng.xUniform01(xRng.xRng);
		float fV = xRng.xUniform01(xRng.xRng);
		if (fU + fV > 1.0f)
		{
			fU = 1.0f - fU;
			fV = 1.0f - fV;
		}
		float fW = 1.0f - fU - fV;

		Zenith_Maths::Vector3 xPosition = xTri.xPos0 * fW + xTri.xPos1 * fU + xTri.xPos2 * fV;
		Zenith_Maths::Vector3 xNormal = glm::normalize(xTri.xNorm0 * fW + xTri.xNorm1 * fU + xTri.xNorm2 * fV);

		const float fLocalLerp = xTri.fLerp0 * fW + xTri.fLerp1 * fU + xTri.fLerp2 * fV;
		if (fLocalLerp > fGRASS_THRESHOLD)
			continue;

		// Offset along normal to prevent z-fighting.
		xPosition += xNormal * 0.01f;

		GrassBladeInstance xInstance;
		xInstance.m_xPosition = xPosition;
		xInstance.m_fRotation = xRng.xRotationDist(xRng.xRng);
		xInstance.m_fHeight = xRng.xHeightDist(xRng.xRng);
		xInstance.m_fWidth = xRng.xWidthDist(xRng.xRng);
		xInstance.m_fBend = xRng.xBendDist(xRng.xRng);

		// ~15% of blades use a dry/yellowed palette for variety.
		const float fBlend = xRng.xColorBlend(xRng.xRng);
		const bool bDryGrass = fBlend < 0.15f;

		u_int uR, uG, uB;
		if (bDryGrass)
		{
			uR = 140 + xRng.xColorOffset(xRng.xRng);
			uG = 150 + xRng.xColorOffset(xRng.xRng);
			uB = 50 + xRng.xColorOffset(xRng.xRng) / 2;
		}
		else
		{
			const u_int uBaseGreen = xRng.xGreenDist(xRng.xRng);
			uR = 40 + xRng.xColorOffset(xRng.xRng);
			uG = uBaseGreen;
			uB = 20 + xRng.xColorOffset(xRng.xRng) / 2;
		}
		xInstance.m_uColorTint = (255 << 24) | (uB << 16) | (uG << 8) | uR;

		xAllInstances.PushBack(xInstance);
		uTotalBladesGenerated++;
	}
	return true;
}

void Flux_GrassImpl::GenerateFromTerrain(const Flux_MeshGeometry& xTerrainMesh)
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

	m_axAllInstances.Clear();
	m_axChunks.Clear();
	m_bInstancesGenerated = false;
	m_bInstancesUploaded = false;

	const Zenith_Maths::Vector3* pxPositions = xTerrainMesh.m_pxPositions;
	const Zenith_Maths::Vector3* pxNormals = xTerrainMesh.m_pxNormals;
	const float* pfMaterialLerps = xTerrainMesh.m_pfMaterialLerps;
	const Flux_MeshGeometry::IndexType* puIndices = xTerrainMesh.m_puIndices;

	GrassGenRng xRng;

	// Target blades per square meter (adjusted by density scale)
	const float fBladesPerSqm = static_cast<float>(GrassConfig::uBLADES_PER_SQM) * m_fDensityScale;

	u_int uTotalTrianglesProcessed = 0;
	u_int uTotalBladesGenerated = 0;

	for (u_int uTri = 0; uTri < uNumTriangles; ++uTri)
	{
		const u_int uIdx0 = puIndices[uTri * 3 + 0];
		const u_int uIdx1 = puIndices[uTri * 3 + 1];
		const u_int uIdx2 = puIndices[uTri * 3 + 2];

		TriangleSamples xTri;
		xTri.xPos0 = pxPositions[uIdx0];
		xTri.xPos1 = pxPositions[uIdx1];
		xTri.xPos2 = pxPositions[uIdx2];
		xTri.xNorm0 = pxNormals[uIdx0];
		xTri.xNorm1 = pxNormals[uIdx1];
		xTri.xNorm2 = pxNormals[uIdx2];
		xTri.fLerp0 = pfMaterialLerps ? pfMaterialLerps[uIdx0] : 0.0f;
		xTri.fLerp1 = pfMaterialLerps ? pfMaterialLerps[uIdx1] : 0.0f;
		xTri.fLerp2 = pfMaterialLerps ? pfMaterialLerps[uIdx2] : 0.0f;

		const float fAvgLerp = (xTri.fLerp0 + xTri.fLerp1 + xTri.fLerp2) / 3.0f;
		if (fAvgLerp > fGRASS_THRESHOLD)
			continue;

		const Zenith_Maths::Vector3 xEdge1 = xTri.xPos1 - xTri.xPos0;
		const Zenith_Maths::Vector3 xEdge2 = xTri.xPos2 - xTri.xPos0;
		const float fArea = glm::length(glm::cross(xEdge1, xEdge2)) * 0.5f;

		if (fArea < 0.001f)
			continue;

		// Density falls off linearly as fAvgLerp approaches the threshold.
		float fDensityMultiplier = 1.0f - (fAvgLerp / fGRASS_THRESHOLD);

		// Painted density map (terrain editor): sampled at the triangle
		// centroid and multiplied into the placement density.
		if (HasDensityMap())
		{
			const Zenith_Maths::Vector3 xCentroid = (xTri.xPos0 + xTri.xPos1 + xTri.xPos2) / 3.0f;
			fDensityMultiplier *= SampleDensityMap(xCentroid.x, xCentroid.z);
		}

		if (fDensityMultiplier <= 0.0f)
			continue;

		u_int uNumBlades = static_cast<u_int>(fArea * fBladesPerSqm * fDensityMultiplier);
		uNumBlades = std::min(uNumBlades, 100u);

		if (!GenerateBladesForTriangle(xTri, uNumBlades, xRng, m_axAllInstances, uTotalBladesGenerated))
			break;

		uTotalTrianglesProcessed++;
	}

	m_bInstancesGenerated = true;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Generated %u blades from %u triangles",
		uTotalBladesGenerated, uTotalTrianglesProcessed);

	// Shuffle so LOD reduction (draw first N/4) spreads across the terrain
	// instead of clustering into a corner.
	if (m_axAllInstances.GetSize() > 1)
	{
		std::shuffle(m_axAllInstances.GetDataPointer(),
			m_axAllInstances.GetDataPointer() + m_axAllInstances.GetSize(),
			xRng.xRng);
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: Shuffled instances for even LOD distribution");
	}

	// Single chunk spanning every instance (future: subdivide for culling).
	if (m_axAllInstances.GetSize() > 0)
	{
		Zenith_Maths::Vector3 xMinBounds(FLT_MAX);
		Zenith_Maths::Vector3 xMaxBounds(-FLT_MAX);
		for (u_int i = 0; i < m_axAllInstances.GetSize(); ++i)
		{
			const Zenith_Maths::Vector3& xPos = m_axAllInstances.Get(i).m_xPosition;
			xMinBounds = glm::min(xMinBounds, xPos);
			xMaxBounds = glm::max(xMaxBounds, xPos);
		}

		GrassChunk xChunk;
		xChunk.m_xCenter = (xMinBounds + xMaxBounds) * 0.5f;
		xChunk.m_fRadius = glm::length(xMaxBounds - xMinBounds) * 0.5f;
		xChunk.m_uInstanceOffset = 0;
		xChunk.m_uInstanceCount = static_cast<u_int>(m_axAllInstances.GetSize());
		xChunk.m_uLOD = 0;
		xChunk.m_bVisible = true;
		m_axChunks.PushBack(xChunk);
	}

	UploadInstanceData();
	UpdateVisibleChunks();
}

void Flux_GrassImpl::SetDensityMap(const float* pfData, u_int uWidth, u_int uHeight, float fWorldSize)
{
	m_xDensityMap.Clear();
	m_uDensityMapWidth = 0;
	m_uDensityMapHeight = 0;
	if (pfData == nullptr || uWidth == 0 || uHeight == 0 || fWorldSize <= 0.0f)
	{
		return;
	}
	const u_int uCount = uWidth * uHeight;
	m_xDensityMap.Reserve(uCount);
	for (u_int u = 0; u < uCount; u++)
	{
		m_xDensityMap.PushBack(pfData[u]);
	}
	m_uDensityMapWidth = uWidth;
	m_uDensityMapHeight = uHeight;
	m_fDensityMapWorldSize = fWorldSize;
}

float Flux_GrassImpl::SampleDensityMap(float fWorldX, float fWorldZ) const
{
	if (!HasDensityMap())
	{
		return 1.0f;
	}
	const float fScaleX = static_cast<float>(m_uDensityMapWidth) / m_fDensityMapWorldSize;
	const float fScaleZ = static_cast<float>(m_uDensityMapHeight) / m_fDensityMapWorldSize;
	float fPX = std::clamp(fWorldX * fScaleX, 0.0f, static_cast<float>(m_uDensityMapWidth - 1));
	float fPZ = std::clamp(fWorldZ * fScaleZ, 0.0f, static_cast<float>(m_uDensityMapHeight - 1));
	const u_int uX0 = static_cast<u_int>(fPX);
	const u_int uZ0 = static_cast<u_int>(fPZ);
	const u_int uX1 = std::min(uX0 + 1, m_uDensityMapWidth - 1);
	const u_int uZ1 = std::min(uZ0 + 1, m_uDensityMapHeight - 1);
	const float fTX = fPX - static_cast<float>(uX0);
	const float fTZ = fPZ - static_cast<float>(uZ0);
	const float* pfMap = m_xDensityMap.GetDataPointer();
	const float fTop = pfMap[uZ0 * m_uDensityMapWidth + uX0] * (1.0f - fTX) + pfMap[uZ0 * m_uDensityMapWidth + uX1] * fTX;
	const float fBottom = pfMap[uZ1 * m_uDensityMapWidth + uX0] * (1.0f - fTX) + pfMap[uZ1 * m_uDensityMapWidth + uX1] * fTX;
	return fTop * (1.0f - fTZ) + fBottom * fTZ;
}

// Setters with input validation (continuous parameters; on/off lives in Zenith_GraphicsOptions)
void Flux_GrassImpl::SetDensityScale(float fScale)
{
	m_fDensityScale = std::clamp(fScale, 0.0f, 10.0f);
}
void Flux_GrassImpl::SetMaxDistance(float fDistance)
{
	m_fMaxDistance = std::clamp(fDistance, 10.0f, 1000.0f);
}
void Flux_GrassImpl::SetWindStrength(float fStrength)
{
	m_fWindStrength = std::clamp(fStrength, 0.0f, 10.0f);
}
void Flux_GrassImpl::SetWindDirection(const Zenith_Maths::Vector2& xDirection)
{
	float fLenSq = glm::dot(xDirection, xDirection);
	if (fLenSq > 0.0001f)
	{
		m_xWindDirection = xDirection / sqrtf(fLenSq);
	}
	else
	{
		m_xWindDirection = Zenith_Maths::Vector2(1.0f, 0.0f);
	}
}

// Getters
bool Flux_GrassImpl::IsEnabled() const { return Zenith_GraphicsOptions::Get().m_bGrassEnabled; }
bool Flux_GrassImpl::IsWindEnabled() const { return Zenith_GraphicsOptions::Get().m_bGrassWindEnabled; }

// Stats
float Flux_GrassImpl::GetBufferUsageMB() const { return (m_uVisibleBladeCount * sizeof(GrassBladeInstance)) / (1024.0f * 1024.0f); }

#ifdef ZENITH_TOOLS
void Flux_GrassImpl::RegisterDebugVariables()
{
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Grass", "DebugMode" }, dbg_uGrassDebugMode, 0, GRASS_DEBUG_COUNT - 1);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Grass", "DensityScale" }, dbg_fGrassDensityScale, 0.0f, 5.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Grass", "MaxDistance" }, dbg_fGrassMaxDistance, 50.0f, 500.0f);
	g_xEngine.DebugVariables().AddFloat({ "Flux", "Grass", "WindStrength" }, dbg_fGrassWindStrength, 0.0f, 5.0f);
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "Grass", "ShowChunkGrid" }, dbg_bGrassShowChunkGrid);
	g_xEngine.DebugVariables().AddBoolean({ "Flux", "Grass", "FreezeLOD" }, dbg_bGrassFreezeLOD);
	g_xEngine.DebugVariables().AddUInt32({ "Flux", "Grass", "ForcedLOD" }, dbg_uGrassForcedLOD, 0, 3);
}
#endif
