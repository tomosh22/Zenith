#include "Zenith.h"
#include "Flux/Vegetation/Flux_GrassImpl.h"
#include "Flux/Vegetation/Flux_Grass_Shaders.h"

#include "Core/Zenith_Engine.h"
#include "Core/Zenith_GraphicsOptions.h"
#include "Flux/Flux_BackendTypes.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Shaders/Generated/Vegetation.h"  // the grass programs' baked vertex layouts (pinned empty below)
#include "Flux/Shadows/Flux_ShadowsImpl.h"      // CSM_FORMAT (shadow caster pipeline)
#include "Flux/Terrain/Flux_TerrainConfig.h"    // TERRAIN_SIZE (the .ztxtr map footprint)
#include "AssetHandling/Zenith_TextureAsset.h"  // LoadCPUData (the ONE .ztxtr parser)
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_GrassTypeTableAsset.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#ifdef ZENITH_TOOLS
#include "DebugVariables/Zenith_DebugVariables.h"
#endif

//=============================================================================
// Flux_Grass — lifecycle, GPU resources, map feeding and the CPU query surface.
// The per-frame gather, the render-graph declaration and the record callbacks
// live in Flux_Grass_Frame.cpp.
//=============================================================================

namespace
{
	// Map textures are created ONCE at these extents and updated in place, so the
	// render graph's barrier keys (and the descriptor the placement CS binds) never
	// move. A Build* supplying different dimensions is rejected rather than silently
	// resampled — a resample would misalign the CPU queries against the GPU texels.
	constexpr u_int uGRASS_HEIGHT_TEXTURE_SIZE   = 4096u;
	constexpr u_int uGRASS_COVERAGE_TEXTURE_SIZE = 1024u;
	constexpr u_int uGRASS_TYPE_TEXTURE_SIZE     = 1024u;
	// Neutral 1x1 zero field, bound in place of the real 256^2 trail map whenever
	// displacement is off. The placement CS samples its displacement input
	// unconditionally (a branch there would diverge a whole wave for a field that is
	// usually inert), so "no push" has to be expressible as DATA.
	constexpr u_int uGRASS_DISPLACEMENT_TEXTURE_SIZE = 1u;
	constexpr TextureFormat eGRASS_DISPLACEMENT_FORMAT = TEXTURE_FORMAT_R16G16_SFLOAT;

	// Coarse min/max height band per cell, used only to give each tile an AABB worth
	// culling. A flat band over hilly ground culls tiles that are plainly visible.
	constexpr u_int uGRASS_HEIGHT_GRID_CELLS = 64u;

	// Height.ztxtr stores a NORMALIZED heightfield; this is the metre range it spans.
	// Authoring twin: Zenith_TerrainEditor::fTERRAIN_MAX_HEIGHT — tools-only, so it
	// cannot be included from a runtime feature.
	constexpr float fGRASS_TERRAIN_HEIGHT_RANGE = 512.0f;

	Flux_SurfaceInfo MakeMapSurfaceInfo(TextureFormat eFormat, u_int uSize)
	{
		Flux_SurfaceInfo xInfo;
		xInfo.m_eFormat      = eFormat;
		xInfo.m_uWidth       = uSize;
		xInfo.m_uHeight      = uSize;
		xInfo.m_uDepth       = 1u;
		xInfo.m_uNumMips     = 1u;
		xInfo.m_uNumLayers   = 1u;
		xInfo.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		return xInfo;
	}

	// A square map with data, and a positive world footprint to divide by.
	bool MapSourceIsUsable(const void* pData, u_int uSize, u_int uExpectedSize)
	{
		return pData != nullptr && uSize == uExpectedSize;
	}
}

//=============================================================================
// Pipelines
//=============================================================================

void Flux_GrassImpl::BuildPipelines()
{
	// --- compute: reset -> placement -> fixup, and the displacement field the
	// placement pass samples (it runs LAST in the frame; see SetupRenderGraph) ---
	m_xResetShader.Initialise(Flux_GrassShaders::xGrass_Reset);
	Flux_RootSigBuilder::FromReflection(m_xResetRootSig, m_xResetShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xResetPipeline, m_xResetShader, m_xResetRootSig);

	m_xPlacementShader.Initialise(Flux_GrassShaders::xGrass_Placement);
	Flux_RootSigBuilder::FromReflection(m_xPlacementRootSig, m_xPlacementShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xPlacementPipeline, m_xPlacementShader, m_xPlacementRootSig);

	m_xFixupShader.Initialise(Flux_GrassShaders::xGrass_IndirectFixup);
	Flux_RootSigBuilder::FromReflection(m_xFixupRootSig, m_xFixupShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xFixupPipeline, m_xFixupShader, m_xFixupRootSig);

	m_xDisplacementShader.Initialise(Flux_GrassShaders::xGrass_Displacement);
	Flux_RootSigBuilder::FromReflection(m_xDisplacementRootSig, m_xDisplacementShader.GetReflection());
	Flux_ComputePipelineBuilder::BuildFromShader(m_xDisplacementPipeline, m_xDisplacementShader, m_xDisplacementRootSig);

	// --- graphics: the blade draws ---
	// NO VERTEX INPUT on any of them. The draw is INDEXED against the 48-entry blade
	// index table, so SV_VertexID delivers the fetched index value; a vertex layout
	// here would declare bindings nothing ever sets, so all three specs below leave
	// m_pxVertexLayout null — the canonical "no vertex input" spelling.
	//
	// Pinned against the generated tables rather than left as a comment: the three
	// draw programs are the only ones here that COULD grow a VsIn, and if one did,
	// the CPU side would keep passing null and the pipeline would fetch from a
	// binding nothing binds. An empty generated layout is what makes the null legal.
	static_assert(Flux_Generated_Vegetation::Grass_ToGBuffer::kVertexLayout == Flux_VertexLayoutDesc{},
		"Grass_ToGBuffer must declare NO vertex input — the blade draw binds no vertex buffer");
	static_assert(Flux_Generated_Vegetation::Grass_ToGBufferVelocity::kVertexLayout == Flux_VertexLayoutDesc{},
		"Grass_ToGBufferVelocity must declare NO vertex input — the blade draw binds no vertex buffer");
	static_assert(Flux_Generated_Vegetation::Grass_ToShadowmap::kVertexLayout == Flux_VertexLayoutDesc{},
		"Grass_ToShadowmap must declare NO vertex input — the caster draw binds no vertex buffer");

	{
		m_xGBufferShader.Initialise(Flux_GrassShaders::xGrass_ToGBuffer);

		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE]        = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL]       = MRT_FORMAT_MATERIAL;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE]       = MRT_FORMAT_EMISSIVE;
		xSpec.m_uNumColourAttachments = uFLUX_MRT_CORE_COUNT;
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_pxShader = &m_xGBufferShader;
		xSpec.m_eTopology = MESH_TOPOLOGY_NONE;
		// Blades are OPAQUE G-buffer geometry with the SUBSURFACE shading tag, not a
		// forward blended overlay: they test AND write depth.
		xSpec.m_bDepthTestEnabled = true;
		xSpec.m_bDepthWriteEnabled = true;
		// A blade is a two-sided sliver; the fragment stage flips the lighting normal
		// on back faces rather than dropping them.
		xSpec.m_eCullMode = CULL_MODE_NONE;
		// Flux_BlendState defaults to blend ENABLED with src-alpha/one-minus-src-alpha,
		// which would alpha-blend the G-buffer targets. Disabled EXPLICITLY on every
		// target rather than relied on being off.
		for (Flux_BlendState& xBlend : xSpec.m_axBlendStates)
		{
			xBlend.m_bBlendEnabled = false;
			xBlend.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlend.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
		}
		m_xGBufferShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(m_xGBufferPipeline, xSpec);

	}

	// Velocity variant: identical state, 5 attachments, and its OWN program — the
	// base's GBufferOut is shared with 4-attachment passes and must not gain a
	// fifth target. Built from a fresh spec so the reflected layout is this
	// program's, not the base's with an extra populate on top.
	{
		m_xGBufferVelocityShader.Initialise(Flux_GrassShaders::xGrass_ToGBufferVelocity);

		Flux_PipelineSpecification xSpec;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE]        = MRT_FORMAT_DIFFUSE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL]       = MRT_FORMAT_MATERIAL;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_EMISSIVE]       = MRT_FORMAT_EMISSIVE;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_VELOCITY]       = MRT_FORMAT_VELOCITY;
		xSpec.m_uNumColourAttachments = MRT_INDEX_COUNT;
		xSpec.m_eDepthStencilFormat = DEPTH_FORMAT;
		xSpec.m_pxShader = &m_xGBufferVelocityShader;
		xSpec.m_eTopology = MESH_TOPOLOGY_NONE;
		xSpec.m_bDepthTestEnabled = true;
		xSpec.m_bDepthWriteEnabled = true;
		xSpec.m_eCullMode = CULL_MODE_NONE;
		for (Flux_BlendState& xBlend : xSpec.m_axBlendStates)
		{
			xBlend.m_bBlendEnabled = false;
			xBlend.m_eSrcBlendFactor = BLEND_FACTOR_ONE;
			xBlend.m_eDstBlendFactor = BLEND_FACTOR_ZERO;
		}
		m_xGBufferVelocityShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(m_xGBufferVelocityPipeline, xSpec);
	}

	{
		m_xShadowShader.Initialise(Flux_GrassShaders::xGrass_ToShadowmap);

		Flux_PipelineSpecification xSpec;
		xSpec.m_uNumColourAttachments = 0u;
		xSpec.m_eDepthStencilFormat = CSM_FORMAT;
		xSpec.m_pxShader = &m_xShadowShader;
		xSpec.m_eTopology = MESH_TOPOLOGY_NONE;
		xSpec.m_eCullMode = CULL_MODE_NONE;
		// Dynamic bias: the cascade pass sets its own per-cascade value, matching
		// every other caster.
		xSpec.m_bDepthBias = true;
		xSpec.m_bDynamicDepthBias = true;
		xSpec.m_fDepthBiasConstant = 1.75f;
		xSpec.m_fDepthBiasSlope = 3.0f;
		m_xShadowShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);
		Flux_PipelineBuilder::FromSpecification(m_xShadowPipeline, xSpec);
	}
}

//=============================================================================
// GPU resources
//=============================================================================

void Flux_GrassImpl::CreateGPUResources()
{
	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();

	xMem.InitialiseReadWriteBuffer(nullptr,
		static_cast<size_t>(uFLUX_GRASS_BLADE_POOL_CAPACITY) * sizeof(Flux_GrassBladeInstance), m_xBladePoolBuffer);
	xMem.InitialiseReadWriteBuffer(nullptr,
		static_cast<size_t>(uFLUX_GRASS_VISIBLE_INDEX_CAPACITY) * sizeof(u_int), m_xVisibleIndexBuffer);
	xMem.InitialiseReadWriteBuffer(nullptr, sizeof(u_int), m_xBladeCounterBuffer);
	xMem.InitialiseIndirectBuffer(uFLUX_GRASS_INDIRECT_SLOT_COUNT * uFLUX_GRASS_INDIRECT_STRIDE, m_xIndirectArgsBuffer);

	// The index table VERBATIM — the shader's kauGRASS_INDEX_TABLE mirrored into
	// Flux_GrassConfig::auBLADE_INDEX_TABLE. The reset CS's firstIndex/indexCount
	// address ranges inside exactly these 48 entries.
	xMem.InitialiseIndexBuffer(Flux_GrassConfig::auBLADE_INDEX_TABLE,
		sizeof(Flux_GrassConfig::auBLADE_INDEX_TABLE), m_xBladeIndexBuffer);

	xMem.InitialiseDynamicReadWriteBuffer(nullptr,
		static_cast<size_t>(uFLUX_GRASS_MAX_TYPES) * sizeof(Flux_GrassTypeParamsGPU), m_xTypeParamsBuffer);
	xMem.InitialiseDynamicReadWriteBuffer(nullptr,
		static_cast<size_t>(Flux_GrassConfig::uMAX_TILES) * sizeof(Flux_GrassTileGPU), m_xTileBuffer);
	// Full cap, not the live prefix: the buffer is allocated once and only the live
	// movers are uploaded into it each frame.
	xMem.InitialiseDynamicReadWriteBuffer(nullptr,
		static_cast<size_t>(uFLUX_GRASS_MAX_MOVERS) * sizeof(Flux_GrassMoverGPU), m_xMoverBuffer);

	xMem.InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_GrassPlacementConstantsGPU), m_xPlacementConstantsBuffer);
	xMem.InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_GrassDrawConstantsGPU), m_xDrawConstantsBuffer);
	xMem.InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_GrassWindBlockGPU), m_xPrevWindConstantsBuffer);
	xMem.InitialiseDynamicConstantBuffer(nullptr, sizeof(Flux_GrassDisplacementConstantsGPU), m_xDisplacementConstantsBuffer);

	// Zero the indirect block once at allocation. The per-frame reset CS rewrites
	// every live word, but a garbage VkDrawIndexedIndirectCommand read on the very
	// first frame (before the reset has run) is the classic GPU-TDR landmine.
	Flux_GrassDrawIndexedIndirectArgs axZeroArgs[uFLUX_GRASS_INDIRECT_SLOT_COUNT] = {};
	xMem.UploadBufferData(m_xIndirectArgsBuffer.GetBuffer().m_xVRAMHandle, axZeroArgs, sizeof(axZeroArgs));
}

void Flux_GrassImpl::DestroyGPUResources()
{
	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();
	xMem.DestroyReadWriteBuffer(m_xBladePoolBuffer);
	xMem.DestroyReadWriteBuffer(m_xVisibleIndexBuffer);
	xMem.DestroyReadWriteBuffer(m_xBladeCounterBuffer);
	xMem.DestroyIndirectBuffer(m_xIndirectArgsBuffer);
	xMem.DestroyIndexBuffer(m_xBladeIndexBuffer);
	xMem.DestroyDynamicReadWriteBuffer(m_xTypeParamsBuffer);
	xMem.DestroyDynamicReadWriteBuffer(m_xTileBuffer);
	xMem.DestroyDynamicReadWriteBuffer(m_xMoverBuffer);
	xMem.DestroyDynamicConstantBuffer(m_xPlacementConstantsBuffer);
	xMem.DestroyDynamicConstantBuffer(m_xDrawConstantsBuffer);
	xMem.DestroyDynamicConstantBuffer(m_xPrevWindConstantsBuffer);
	xMem.DestroyDynamicConstantBuffer(m_xDisplacementConstantsBuffer);
}

void Flux_GrassImpl::CreateDisplacementMaps()
{
	// Built in EVERY config, headless included — unlike the 34 MB map set there is no
	// staging cost to skip, and the render-graph declaration references these objects
	// by address, so a config where they did not exist would need a different graph.
	//
	// UNORDERED_ACCESS for the displacement pass (which loads AND stores through
	// storage-image views, so the ping-pong needs no sampler on the write side) plus
	// SHADER_READ for the placement CS's bilinear sample of the finished field.
	for (u_int u = 0; u < 2u; u++)
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth  = Flux_GrassConfig::uDISPLACEMENT_RESOLUTION;
		xBuilder.m_uHeight = Flux_GrassConfig::uDISPLACEMENT_RESOLUTION;
		xBuilder.m_eFormat = eGRASS_DISPLACEMENT_FORMAT;
		xBuilder.m_uMemoryFlags = (1u << MEMORY_FLAGS__UNORDERED_ACCESS) | (1u << MEMORY_FLAGS__SHADER_READ);
		xBuilder.BuildColour(m_axDisplacementMaps[u], u == 0u ? "Grass Displacement A" : "Grass Displacement B");
	}
}

void Flux_GrassImpl::DestroyDisplacementMaps()
{
	for (u_int u = 0; u < 2u; u++)
	{
		Flux_RenderAttachmentBuilder::Destroy(m_axDisplacementMaps[u]);
	}
}

void Flux_GrassImpl::CreateMapTextures()
{
	if (Zenith_IsNullRenderer())
	{
		// The GPU-less backend's texture calls are no-ops and nothing ever samples
		// these descriptors, so the 34 MB of zero staging below would be pure cost.
		// Every CPU-side consequence of a build (maps, tile schedule, stats) still
		// happens — only the device copy is skipped.
		return;
	}

	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();

	// Created from zeroed staging rather than left undefined: an unbuilt world must
	// sample zero coverage (no grass), not whatever the allocation happened to hold.
	const Flux_SurfaceInfo xHeightInfo = MakeMapSurfaceInfo(TEXTURE_FORMAT_R16_UNORM, uGRASS_HEIGHT_TEXTURE_SIZE);
	const Flux_SurfaceInfo xCoverageInfo = MakeMapSurfaceInfo(TEXTURE_FORMAT_R8_UNORM, uGRASS_COVERAGE_TEXTURE_SIZE);
	const Flux_SurfaceInfo xTypeInfo = MakeMapSurfaceInfo(TEXTURE_FORMAT_R8_UNORM, uGRASS_TYPE_TEXTURE_SIZE);
	const Flux_SurfaceInfo xDisplacementInfo = MakeMapSurfaceInfo(TEXTURE_FORMAT_R16G16_SFLOAT, uGRASS_DISPLACEMENT_TEXTURE_SIZE);

	Zenith_Vector<u_int8> aucZeros;
	aucZeros.Resize(uGRASS_HEIGHT_TEXTURE_SIZE * uGRASS_HEIGHT_TEXTURE_SIZE * 2u, static_cast<u_int8>(0));
	const void* pZeros = aucZeros.GetDataPointer();

	m_xHeightTexture.m_xSurfaceInfo = xHeightInfo;
	m_xHeightTexture.m_xVRAMHandle = xMem.CreateTextureVRAM(pZeros, xHeightInfo, TEXTURE_MIPS_NONE);
	m_xHeightTexture.m_xSRV = xMem.CreateShaderResourceView(m_xHeightTexture.m_xVRAMHandle, xHeightInfo, 0u, 1u);

	m_xCoverageTexture.m_xSurfaceInfo = xCoverageInfo;
	m_xCoverageTexture.m_xVRAMHandle = xMem.CreateTextureVRAM(pZeros, xCoverageInfo, TEXTURE_MIPS_NONE);
	m_xCoverageTexture.m_xSRV = xMem.CreateShaderResourceView(m_xCoverageTexture.m_xVRAMHandle, xCoverageInfo, 0u, 1u);

	m_xTypeTexture.m_xSurfaceInfo = xTypeInfo;
	m_xTypeTexture.m_xVRAMHandle = xMem.CreateTextureVRAM(pZeros, xTypeInfo, TEXTURE_MIPS_NONE);
	m_xTypeTexture.m_xSRV = xMem.CreateShaderResourceView(m_xTypeTexture.m_xVRAMHandle, xTypeInfo, 0u, 1u);

	m_xDisplacementTexture.m_xSurfaceInfo = xDisplacementInfo;
	m_xDisplacementTexture.m_xVRAMHandle = xMem.CreateTextureVRAM(pZeros, xDisplacementInfo, TEXTURE_MIPS_NONE);
	m_xDisplacementTexture.m_xSRV = xMem.CreateShaderResourceView(m_xDisplacementTexture.m_xVRAMHandle, xDisplacementInfo, 0u, 1u);
}

void Flux_GrassImpl::DestroyMapTextures()
{
	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();
	Flux_Texture* apxTextures[4] = { &m_xHeightTexture, &m_xCoverageTexture, &m_xTypeTexture, &m_xDisplacementTexture };
	for (Flux_Texture* pxTexture : apxTextures)
	{
		if (pxTexture->m_xVRAMHandle.IsValid())
		{
			xMem.QueueVRAMDeletion(pxTexture->m_xVRAMHandle,
				Flux_ImageViewHandle(), Flux_ImageViewHandle(), pxTexture->m_xSRV.m_xImageViewHandle);
		}
		*pxTexture = Flux_Texture();
	}
}

void Flux_GrassImpl::UploadMapTextures()
{
	// Headless has no allocator behind the texture handles, so the re-upload is a
	// no-op that still walks 34 MB of staging. Skipped explicitly: the CPU maps ARE
	// the query surface, and they are already populated.
	if (Zenith_IsNullRenderer() || !m_bGPUResourcesReady)
	{
		return;
	}

	Flux_MemoryManager& xMem = g_xEngine.FluxMemory();
	xMem.UpdateTextureVRAM(m_xHeightTexture.m_xVRAMHandle, m_auHeightTexels.GetDataPointer(), m_xHeightTexture.m_xSurfaceInfo);
	xMem.UpdateTextureVRAM(m_xCoverageTexture.m_xVRAMHandle, m_aucCoverageTexels.GetDataPointer(), m_xCoverageTexture.m_xSurfaceInfo);
	xMem.UpdateTextureVRAM(m_xTypeTexture.m_xVRAMHandle, m_aucTypeTexels.GetDataPointer(), m_xTypeTexture.m_xSurfaceInfo);
}

//=============================================================================
// Lifecycle
//=============================================================================

void Flux_GrassImpl::Initialise()
{
	BuildPipelines();
	CreateGPUResources();
	CreateMapTextures();
	CreateDisplacementMaps();
	m_bGPUResourcesReady = true;

	// The type table is global authored content, not scene state: seeded once here
	// and preserved across every ClearSceneData. The game's authored table, when it
	// ships one, replaces the seeded defaults right here — before the first gather,
	// so no frame is ever placed against a table the game did not author.
	m_xTypeTable = Flux_GrassTypeTable::Defaults();
	LoadAuthoredTypeTable();

	ClearSceneData();

#ifdef ZENITH_TOOLS
	RegisterDebugVariables();
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass initialised (GPU-driven, %.1f MB)", GetBufferUsageMB());
}

void Flux_GrassImpl::Shutdown()
{
	if (m_bGPUResourcesReady)
	{
		DestroyGPUResources();
		DestroyMapTextures();
		DestroyDisplacementMaps();
		m_bGPUResourcesReady = false;
	}

	m_xResetPipeline.Reset();
	m_xPlacementPipeline.Reset();
	m_xFixupPipeline.Reset();
	m_xDisplacementPipeline.Reset();
	m_xGBufferPipeline.Reset();
	m_xGBufferVelocityPipeline.Reset();
	m_xShadowPipeline.Reset();

	m_xResetShader.Reset();
	m_xPlacementShader.Reset();
	m_xFixupShader.Reset();
	m_xDisplacementShader.Reset();
	m_xGBufferShader.Reset();
	m_xGBufferVelocityShader.Reset();
	m_xShadowShader.Reset();

	ClearSceneData();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass shut down");
}

void Flux_GrassImpl::Reset()
{
	// Engine scene-lifecycle hook. It DOUBLE-FIRES at boot and may run before
	// Initialise, so the whole body has to be idempotent CPU work — which is exactly
	// what ClearSceneData is.
	ClearSceneData();
}

void Flux_GrassImpl::ClearSceneData()
{
	// Scene-owned state only. The type table, the wind state and every GPU
	// allocation deliberately survive: the first is authored content, the second is
	// a global atmosphere setting, and the third is engine-owned until Shutdown.
	m_auHeightTexels.Clear();
	m_aucCoverageTexels.Clear();
	m_aucTypeTexels.Clear();
	m_uHeightSize = 0u;
	m_uCoverageSize = 0u;
	m_uTypeSize = 0u;
	m_fMapWorldSize = 0.0f;
	m_fHeightScale = 1.0f;
	m_fHeightBias = 0.0f;

	m_afTileMinY.Clear();
	m_afTileMaxY.Clear();
	m_uHeightGridCells = 0u;
	m_fHeightGridCellSize = 0.0f;

	m_xTileList.Clear();
	m_axMovers.Clear();
	m_uMoverOverflowCount = 0u;

	m_uScheduledInstanceCount = 0u;
	m_uVisibleTileCount = 0u;
	m_uTileCount = 0u;
	m_uSubmittedDrawCount = 0u;
	m_uCascadeFrustaCount = 0u;

	m_bBuilt = false;
}

//=============================================================================
// Feeding
//=============================================================================

bool Flux_GrassImpl::QuantizeHeightMap(const float* pfHeight, u_int uSize, float fWorldSize)
{
	// The fixed-point range tracks the terrain actually supplied rather than a
	// global constant, so a shallow map keeps its full 16 bits of resolution.
	float fMinY = FLT_MAX;
	float fMaxY = -FLT_MAX;
	const u_int uTexelCount = uSize * uSize;
	for (u_int u = 0; u < uTexelCount; u++)
	{
		fMinY = glm::min(fMinY, pfHeight[u]);
		fMaxY = glm::max(fMaxY, pfHeight[u]);
	}
	if (!(fMinY <= fMaxY))
	{
		return false;   // every texel was NaN
	}

	m_fHeightBias = fMinY;
	// A perfectly flat map would divide by zero; 1 m of range quantizes it to the
	// single value it already is.
	m_fHeightScale = (fMaxY - fMinY) > 1.0e-4f ? (fMaxY - fMinY) : 1.0f;
	const float fInvScale = 1.0f / m_fHeightScale;

	// Cleared first: Resize only value-fills NEW elements, so a rebuild over a
	// previous map would otherwise keep its tail.
	m_auHeightTexels.Clear();
	m_auHeightTexels.Resize(uTexelCount, static_cast<u_int16>(0));
	u_int16* puTexels = m_auHeightTexels.GetDataPointer();
	for (u_int u = 0; u < uTexelCount; u++)
	{
		const float fNormalized = Zenith_Maths::Clamp((pfHeight[u] - m_fHeightBias) * fInvScale, 0.0f, 1.0f);
		puTexels[u] = static_cast<u_int16>(fNormalized * 65535.0f + 0.5f);
	}
	m_uHeightSize = uSize;
	m_fMapWorldSize = fWorldSize;
	return true;
}

void Flux_GrassImpl::QuantizeCoverageMap(const float* pfCoverage, u_int uSize)
{
	const u_int uTexelCount = uSize * uSize;
	m_aucCoverageTexels.Clear();
	m_aucCoverageTexels.Resize(uTexelCount, static_cast<u_int8>(0));
	u_int8* pucTexels = m_aucCoverageTexels.GetDataPointer();
	for (u_int u = 0; u < uTexelCount; u++)
	{
		const float fValue = Zenith_Maths::Clamp(pfCoverage[u], 0.0f, 1.0f);
		pucTexels[u] = static_cast<u_int8>(fValue * 255.0f + 0.5f);
	}
	m_uCoverageSize = uSize;
}

void Flux_GrassImpl::CopyTypeMap(const u_int8* pucType, u_int uSize)
{
	// An absent type map is normal content, not an error: a terrain set baked before
	// the map existed simply has no file, and all-zero reads as type 0 everywhere.
	const u_int uTexelCount = uSize * uSize;
	m_aucTypeTexels.Clear();
	m_aucTypeTexels.Resize(uTexelCount, static_cast<u_int8>(0));
	if (pucType != nullptr)
	{
		memcpy(m_aucTypeTexels.GetDataPointer(), pucType, uTexelCount);
	}
	m_uTypeSize = uSize;
}

void Flux_GrassImpl::BuildTileHeightGrid(const float* pfHeight, u_int uSize, float fWorldSize)
{
	const u_int uCells = uGRASS_HEIGHT_GRID_CELLS;
	// Cleared first — the accumulation below starts from the sentinels, and a
	// rebuild that inherited a previous map's extremes would widen every band.
	m_afTileMinY.Clear();
	m_afTileMaxY.Clear();
	m_afTileMinY.Resize(uCells * uCells, FLT_MAX);
	m_afTileMaxY.Resize(uCells * uCells, -FLT_MAX);
	m_uHeightGridCells = uCells;
	m_fHeightGridCellSize = fWorldSize / static_cast<float>(uCells);

	float* pfMin = m_afTileMinY.GetDataPointer();
	float* pfMax = m_afTileMaxY.GetDataPointer();
	// uSize is validated a multiple of uCells at Build*; the guard keeps a future
	// smaller map from dividing by zero rather than trusting that validation.
	const u_int uTexelsPerCell = glm::max(uSize / uCells, 1u);
	for (u_int uZ = 0; uZ < uSize; uZ++)
	{
		const u_int uCellZ = glm::min(uZ / uTexelsPerCell, uCells - 1u);
		const float* pfRow = pfHeight + static_cast<size_t>(uZ) * uSize;
		for (u_int uX = 0; uX < uSize; uX++)
		{
			const u_int uCell = uCellZ * uCells + glm::min(uX / uTexelsPerCell, uCells - 1u);
			pfMin[uCell] = glm::min(pfMin[uCell], pfRow[uX]);
			pfMax[uCell] = glm::max(pfMax[uCell], pfRow[uX]);
		}
	}
}

bool Flux_GrassImpl::BuildFromMaps(const MapSet& xMaps, const BuildParams& xParams)
{
	// Everything is validated BEFORE anything is written: a rejected build must
	// leave the previous world's grass exactly as it was.
	const bool bHeightOk = MapSourceIsUsable(xMaps.pHeight, xMaps.uHeightSize, uGRASS_HEIGHT_TEXTURE_SIZE);
	const bool bCoverageOk = MapSourceIsUsable(xMaps.pCoverage, xMaps.uCoverageSize, uGRASS_COVERAGE_TEXTURE_SIZE);
	const bool bTypeOk = xMaps.pType == nullptr || xMaps.uTypeSize == uGRASS_TYPE_TEXTURE_SIZE;
	if (!bHeightOk || !bCoverageOk || !bTypeOk || !(xMaps.fWorldSize > 0.0f))
	{
		Zenith_Log(LOG_CATEGORY_RENDERER,
			"Flux_Grass: rejected build (height %ux%u want %u, coverage %ux%u want %u, world %.1f m) — prior state kept",
			xMaps.uHeightSize, xMaps.uHeightSize, uGRASS_HEIGHT_TEXTURE_SIZE,
			xMaps.uCoverageSize, xMaps.uCoverageSize, uGRASS_COVERAGE_TEXTURE_SIZE, xMaps.fWorldSize);
		return false;
	}

	if (!QuantizeHeightMap(xMaps.pHeight, xMaps.uHeightSize, xMaps.fWorldSize))
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: heightfield has no finite texel — prior state kept");
		return false;
	}
	QuantizeCoverageMap(xMaps.pCoverage, xMaps.uCoverageSize);
	CopyTypeMap(xMaps.pType, uGRASS_TYPE_TEXTURE_SIZE);
	BuildTileHeightGrid(xMaps.pHeight, xMaps.uHeightSize, xMaps.fWorldSize);

	SetDensityScale(xParams.m_fDensityScale);
	UploadMapTextures();
	m_bBuilt = true;

	Zenith_Log(LOG_CATEGORY_RENDERER,
		"Flux_Grass: built over %.0f m (height %u^2 in [%.1f, %.1f] m, coverage %u^2, type %u^2)",
		m_fMapWorldSize, m_uHeightSize, m_fHeightBias, m_fHeightBias + m_fHeightScale, m_uCoverageSize, m_uTypeSize);
	return true;
}

bool Flux_GrassImpl::BuildFromTerrainTextures(const std::string& strDir, float fWorldSize, const BuildParams& xParams)
{
	// Height + GrassDensity are REQUIRED; GrassType is optional content.
	Flux_SurfaceInfo xHeightInfo;
	Zenith_Vector<uint8_t> aucHeightBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strDir + "Height" + ZENITH_TEXTURE_EXT, xHeightInfo, aucHeightBytes).IsOk()
		|| xHeightInfo.m_eFormat != TEXTURE_FORMAT_R32_SFLOAT)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: no usable Height%s in '%s'", ZENITH_TEXTURE_EXT, strDir.c_str());
		return false;
	}

	Flux_SurfaceInfo xCoverageInfo;
	Zenith_Vector<uint8_t> aucCoverageBytes;
	if (!Zenith_TextureAsset::LoadCPUData(strDir + "GrassDensity" + ZENITH_TEXTURE_EXT, xCoverageInfo, aucCoverageBytes).IsOk()
		|| xCoverageInfo.m_eFormat != TEXTURE_FORMAT_R32_SFLOAT)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: no usable GrassDensity%s in '%s'", ZENITH_TEXTURE_EXT, strDir.c_str());
		return false;
	}

	Flux_SurfaceInfo xTypeInfo;
	Zenith_Vector<uint8_t> aucTypeBytes;
	const bool bHasType = Zenith_TextureAsset::LoadCPUData(strDir + "GrassType" + ZENITH_TEXTURE_EXT, xTypeInfo, aucTypeBytes).IsOk()
		&& xTypeInfo.m_eFormat == TEXTURE_FORMAT_R8_UNORM
		&& xTypeInfo.m_uWidth == uGRASS_TYPE_TEXTURE_SIZE;

	// Height.ztxtr is NORMALIZED; BuildFromMaps takes metres. Scaled IN PLACE inside
	// the loaded buffer so a 4096^2 map does not need a second 64 MB copy.
	float* pfHeight = reinterpret_cast<float*>(aucHeightBytes.GetDataPointer());
	const u_int uHeightTexels = xHeightInfo.m_uWidth * xHeightInfo.m_uHeight;
	for (u_int u = 0; u < uHeightTexels; u++)
	{
		pfHeight[u] *= fGRASS_TERRAIN_HEIGHT_RANGE;
	}

	MapSet xMaps;
	xMaps.pHeight = pfHeight;
	xMaps.uHeightSize = xHeightInfo.m_uWidth;
	xMaps.pCoverage = reinterpret_cast<const float*>(aucCoverageBytes.GetDataPointer());
	xMaps.uCoverageSize = xCoverageInfo.m_uWidth;
	xMaps.pType = bHasType ? aucTypeBytes.GetDataPointer() : nullptr;
	xMaps.uTypeSize = bHasType ? xTypeInfo.m_uWidth : 0u;
	// The OWNING TERRAIN's square authoring domain, supplied by the caller. The
	// maps in this directory were authored over exactly that extent.
	xMaps.fWorldSize = fWorldSize;
	return BuildFromMaps(xMaps, xParams);
}

//=============================================================================
// Types
//=============================================================================

void Flux_GrassImpl::SetTypeTable(const Flux_GrassTypeTable& xTable)
{
	m_xTypeTable = xTable;
	m_xTypeTable.Validate();
	ResolveTypeTextures();
	// The GPU block is re-uploaded by the next gather (it uploads unconditionally —
	// 2 KB a frame is far cheaper than tracking a dirty flag across frames in
	// flight), so there is nothing device-side to do here.
}

u_int Flux_GrassImpl::ResolveTypeTexture(const std::string& strPath, FluxGrassTextureSlot eSlot, void* pUser)
{
	Flux_GrassImpl* pxImpl = static_cast<Flux_GrassImpl*>(pUser);
	TextureHandle xHandle(strPath);
	Zenith_TextureAsset* pxTexture = xHandle.Resolve();
	if (pxTexture == nullptr)
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Flux_Grass: grass texture '%s' (slot %d) failed to load — the slot stays unbound",
			strPath.c_str(), static_cast<int>(eSlot));
		return uFLUX_GRASS_BINDLESS_UNBOUND;
	}
	// The gloss lookup runs past u = 1 (height * GlossRepeat) and the vein map is
	// blade detail, so both REPEAT; the ramp is indexed by two [0,1] hashes and
	// CLAMPs so a clump hash of exactly 1 cannot wrap to the first column. A
	// texture is used in one role, so one sampler per slot is correct. An SRV that
	// is not valid leaves the index at uFLUX_INVALID_BINDLESS_INDEX, which is the
	// same bit pattern as UNBOUND — the shader short-circuits either way.
	pxTexture->MarkAsBindless(/*bRepeatAddressing*/ eSlot != FLUX_GRASS_TEXTURE_RAMP);
	pxImpl->m_axTypeTextureHandles.PushBack(xHandle);
	return pxTexture->m_xSRV.m_uBindlessIndex;
}

void Flux_GrassImpl::ResolveTypeTextures()
{
	// The previous table's pins drop FIRST, so a re-applied table that no longer
	// names a texture lets it unload rather than accumulating handles per Apply.
	m_axTypeTextureHandles.Clear();
	m_xTypeTable.ResolveTextureIndices(&Flux_GrassImpl::ResolveTypeTexture, this);
}

void Flux_GrassImpl::ReleaseAssetReferences()
{
	m_axTypeTextureHandles.Clear();
}

void Flux_GrassImpl::LoadAuthoredTypeTable()
{
	// Anchors the asset TU (and its static asset-type registrar) against /OPT:REF —
	// see Zenith_GrassTypeTableAsset_ForceLink.
	static const bool ls_bAssetLinked = Zenith_GrassTypeTableAsset_ForceLink();
	(void)ls_bAssetLinked;

	// The existence check comes FIRST because absence is the normal state: routing a
	// missing file through the registry would log two load failures on every boot of
	// every game for a condition that is not one.
	const std::string strResolved = Zenith_AssetRegistry::ResolvePath(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	if (!Zenith_FileAccess::FileExists(strResolved.c_str()))
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: no authored type table at '%s' — using the %u built-in types",
			szZENITH_GRASS_TYPE_TABLE_ASSET_PATH, m_xTypeTable.GetCount());
		return;
	}

	// A non-owning view is enough: the table is copied out below and nothing here
	// holds the asset past this call.
	const Zenith_GrassTypeTableAsset* pxAsset =
		Zenith_AssetRegistry::GetView<Zenith_GrassTypeTableAsset>(szZENITH_GRASS_TYPE_TABLE_ASSET_PATH);
	if (pxAsset == nullptr || !pxAsset->LoadedOk())
	{
		Zenith_Warning(LOG_CATEGORY_RENDERER, "Flux_Grass: '%s' is present but unreadable — keeping the %u built-in types",
			szZENITH_GRASS_TYPE_TABLE_ASSET_PATH, m_xTypeTable.GetCount());
		return;
	}

	// SetTypeTable copies and validates, so a hand-edited file cannot push an
	// out-of-range parameter into the placement CS — and resolves the table's
	// texture paths to bindless slots, which is the only route a file has to them.
	SetTypeTable(pxAsset->GetTable());
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Grass: loaded %u authored grass types from '%s' (%u texture slots bound)",
		m_xTypeTable.GetCount(), szZENITH_GRASS_TYPE_TABLE_ASSET_PATH, m_xTypeTable.CountBoundTextures());
}

//=============================================================================
// Tuning
//=============================================================================

void Flux_GrassImpl::SetDensityScale(float fScale)
{
	m_fDensityScale = Zenith_Maths::Clamp(fScale, 0.0f, 4.0f);
}

void Flux_GrassImpl::SetMaxDistance(float fDistance)
{
	m_fMaxDistance = Flux_GrassClampMaxDistance(fDistance);
}

void Flux_GrassImpl::SetWindDirection(float fYawRad)
{
	// The YAW is the stored truth and the direction vector is derived each frame:
	// two representations would let the debug slider and a game-code setter disagree.
	m_fWindYawDeg = glm::degrees(fYawRad);
}

float Flux_GrassImpl::GetWindYawRadians() const
{
	return glm::radians(m_fWindYawDeg);
}

void Flux_GrassImpl::SetWindStrength(float fStrength)
{
	m_xWind.m_fStrength = Zenith_Maths::Clamp(fStrength, 0.0f, 10.0f);
}

//=============================================================================
// Displacement movers
//=============================================================================

void Flux_GrassImpl::SubmitMover(const Mover& xMover)
{
	if (m_axMovers.GetSize() >= uFLUX_GRASS_MAX_MOVERS)
	{
		// Dropped, not grown: the list is consumed every frame, so an unbounded one
		// would let a runaway submitter grow without limit inside a single frame.
		m_uMoverOverflowCount++;
		return;
	}
	m_axMovers.PushBack(xMover);
}

//=============================================================================
// CPU queries
//=============================================================================

Flux_GrassMap Flux_GrassImpl::MakeHeightMapView() const
{
	Flux_GrassMap xMap;
	xMap.m_pData = m_auHeightTexels.GetDataPointer();
	xMap.m_uWidth = m_uHeightSize;
	xMap.m_uHeight = m_uHeightSize;
	xMap.m_fWorldSize = m_fMapWorldSize;
	xMap.m_eFormat = Flux_GrassMapFormat::U16;
	xMap.m_fScale = m_fHeightScale;
	return xMap;
}

Flux_GrassMap Flux_GrassImpl::MakeCoverageMapView() const
{
	Flux_GrassMap xMap;
	xMap.m_pData = m_aucCoverageTexels.GetDataPointer();
	xMap.m_uWidth = m_uCoverageSize;
	xMap.m_uHeight = m_uCoverageSize;
	xMap.m_fWorldSize = m_fMapWorldSize;
	xMap.m_eFormat = Flux_GrassMapFormat::U8;
	return xMap;
}

Flux_GrassMap Flux_GrassImpl::MakeTypeMapView() const
{
	Flux_GrassMap xMap;
	xMap.m_pData = m_aucTypeTexels.GetDataPointer();
	xMap.m_uWidth = m_uTypeSize;
	xMap.m_uHeight = m_uTypeSize;
	xMap.m_fWorldSize = m_fMapWorldSize;
	xMap.m_eFormat = Flux_GrassMapFormat::U8;
	return xMap;
}

float Flux_GrassImpl::SampleGrassCoverage(float fWorldX, float fWorldZ) const
{
	return Flux_GrassSampleMapBilinear(MakeCoverageMapView(), fWorldX, fWorldZ);
}

u_int8 Flux_GrassImpl::SampleGrassType(float fWorldX, float fWorldZ) const
{
	return static_cast<u_int8>(Flux_GrassSampleTypeIndex(MakeTypeMapView(), fWorldX, fWorldZ));
}

float Flux_GrassImpl::SampleGrassHeight(float fWorldX, float fWorldZ) const
{
	// The bias is only meaningful on top of a real sample: an unbuilt map samples
	// zero, and adding the (stale or default) bias to it would report a height.
	if (!m_bBuilt)
	{
		return 0.0f;
	}
	return Flux_GrassSampleMapBilinear(MakeHeightMapView(), fWorldX, fWorldZ) + m_fHeightBias;
}

//=============================================================================
// Stats
//=============================================================================

float Flux_GrassImpl::GetBufferUsageMB() const
{
	if (!m_bGPUResourcesReady)
	{
		return 0.0f;
	}
	// The whole persistent footprint, which is CONSTANT after Initialise — nothing
	// here grows with scene content or with any toggle.
	size_t ulBytes = 0;
	ulBytes += static_cast<size_t>(uFLUX_GRASS_BLADE_POOL_CAPACITY) * sizeof(Flux_GrassBladeInstance);
	ulBytes += static_cast<size_t>(uFLUX_GRASS_VISIBLE_INDEX_CAPACITY) * sizeof(u_int);
	ulBytes += uFLUX_GRASS_INDIRECT_SLOT_COUNT * uFLUX_GRASS_INDIRECT_STRIDE;
	ulBytes += sizeof(u_int);
	ulBytes += sizeof(Flux_GrassConfig::auBLADE_INDEX_TABLE);
	ulBytes += static_cast<size_t>(uFLUX_GRASS_MAX_TYPES) * sizeof(Flux_GrassTypeParamsGPU);
	// Both halves of the displacement ping-pong, RG16F.
	ulBytes += 2u * static_cast<size_t>(Flux_GrassConfig::uDISPLACEMENT_RESOLUTION)
		* Flux_GrassConfig::uDISPLACEMENT_RESOLUTION * 4u;
	ulBytes += static_cast<size_t>(uGRASS_HEIGHT_TEXTURE_SIZE) * uGRASS_HEIGHT_TEXTURE_SIZE * 2u;
	ulBytes += static_cast<size_t>(uGRASS_COVERAGE_TEXTURE_SIZE) * uGRASS_COVERAGE_TEXTURE_SIZE;
	ulBytes += static_cast<size_t>(uGRASS_TYPE_TEXTURE_SIZE) * uGRASS_TYPE_TEXTURE_SIZE;
	return static_cast<float>(ulBytes) / (1024.0f * 1024.0f);
}

u_int Flux_GrassImpl::ReadbackVisibleBladeCount()
{
	if (!m_bGPUResourcesReady)
	{
		return 0u;
	}
	// Explicit slow path — DownloadBufferData drains staged writes and idles the
	// device, so this must never be called from a frame path. Headless it returns
	// zeroes by construction, which is why it is windowed-only truth.
	Flux_GrassDrawIndexedIndirectArgs axArgs[uFLUX_GRASS_INDIRECT_SLOT_COUNT] = {};
	g_xEngine.FluxMemory().DownloadBufferData(m_xIndirectArgsBuffer.GetBuffer().m_xVRAMHandle, axArgs, sizeof(axArgs));

	u_int uTotal = 0u;
	for (u_int u = 0; u < uFLUX_GRASS_INDIRECT_SLOT_COUNT; u++)
	{
		uTotal += axArgs[u].m_uInstanceCount;
	}
	return uTotal;
}

bool Flux_GrassImpl::IsShadowCastingEnabled() const
{
	const Zenith_GraphicsOptions& xOpts = Zenith_GraphicsOptions::Get();
	// Engine-wide shadows AND the per-feature grass-shadow option AND the debug
	// escape hatch: grass casters are the one caster class cheap enough to keep and
	// expensive enough to want dropped on its own, so it answers to all three.
	return xOpts.m_bShadowsEnabled && xOpts.m_bGrassShadowsEnabled && !m_bDisableShadowCasting;
}

//=============================================================================
// Debug variables
//
// Every one BINDS A MEMBER BY REFERENCE. The retired implementation copied its
// debug globals over the members inside the record callback each frame, which
// silently reverted anything game code had set and is what forced Zenithmon to
// grow a read-back seam to detect the stomp.
//=============================================================================

#ifdef ZENITH_TOOLS
void Flux_GrassImpl::RegisterDebugVariables()
{
	Zenith_DebugVariables& xVars = g_xEngine.DebugVariables();
	// 0 None / 1 TypeIndex / 2 Clump / 3 HeightT / 4 LODMesh / 5 LatticeClass /
	// 6 Normals — the kGRASS_DEBUG_* set in Flux_GrassCommon.slang, which is the
	// authority. The value rides the draw CB verbatim; Flux_GrassDebugColour tests
	// 1-5 explicitly and FALLS THROUGH to the world-normal view, so 6 and every
	// value above it (the slider's 7 included) all read as normals.
	xVars.AddUInt32({ "Flux", "Grass", "DebugMode" }, m_uDebugMode, 0u, 7u);
	xVars.AddFloat({ "Flux", "Grass", "DensityScale" }, m_fDensityScale, 0.0f, 4.0f);
	xVars.AddFloat({ "Flux", "Grass", "MaxDistance" }, m_fMaxDistance,
		Flux_GrassConfig::fMIN_MAX_DISTANCE, Flux_GrassConfig::fMAX_MAX_DISTANCE);
	xVars.AddFloat({ "Flux", "Grass", "WindStrength" }, m_xWind.m_fStrength, 0.0f, 10.0f);
	xVars.AddFloat({ "Flux", "Grass", "WindYawDeg" }, m_fWindYawDeg, -180.0f, 180.0f);
	xVars.AddBoolean({ "Flux", "Grass", "FreezeCulling" }, m_bFreezeCulling);
	// Storage only: the tile outlines belong on the gameplay-safe debug
	// primitives channel, which is not wired to this flag yet.
	xVars.AddBoolean({ "Flux", "Grass", "ShowTileGrid" }, m_bShowTileGrid);
	// Live A/B on the cascade casters: it drops the shadow partitions out of the
	// active-slot mask, so it removes the placement work as well as the two draws.
	xVars.AddBoolean({ "Flux", "Grass", "DisableShadowCasting" }, m_bDisableShadowCasting);
	xVars.AddBoolean({ "Flux", "Grass", "ForceLoBlades" }, m_bForceLoBlades);
	// Live: while on, every gather submits one synthetic mover circling the camera's
	// ground point, so the trail field can be inspected without a game body to walk.
	// Binds the SAME member SetDebugOrbitDisplacer writes.
	xVars.AddBoolean({ "Flux", "Grass", "DebugOrbitDisplacer" }, m_bDebugOrbitDisplacer);
}
#endif

//=============================================================================
// Partition activation
//=============================================================================

u_int Flux_GrassImpl::ComputeActiveSlotMask() const
{
	// The camera partitions are always live. A cascade partition needs BOTH inputs:
	// casting enabled, and a real cascade frustum staged for it this frame. The frusta
	// count is the second half because a slot whose planes are still the camera's
	// duplicate would populate the partition with the wrong blades — visibly worse
	// than an empty cascade, and harder to spot. Disabling casting therefore stops
	// GENERATION, not just the draw.
	u_int uMask = (1u << uFLUX_GRASS_SLOT_CAMERA_HI) | (1u << uFLUX_GRASS_SLOT_CAMERA_LO);
	if (IsShadowCastingEnabled())
	{
		for (u_int u = 0; u < m_uCascadeFrustaCount && u < uFLUX_GRASS_MAX_CASCADES; u++)
		{
			uMask |= 1u << (uFLUX_GRASS_SLOT_CASCADE_0 + u);
		}
	}
	return uMask;
}

// Unit tests for the pure grass definitions (Flux_GrassTypes.h) plus the feature's
// scene-state lifecycle. Hosted at the bottom of this always-linked feature TU —
// the same feature-owned idiom as Flux_RenderViews.cpp / Flux_AnimationClip.cpp.
// The ZENITH_TEST macros self-noop when ZENITH_TESTING is undefined, so this
// include stays unconditional (matching every other Flux .Tests.inl host).
#include "Flux/Vegetation/Flux_Grass.Tests.inl"
