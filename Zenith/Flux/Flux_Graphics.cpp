#include "Zenith.h"
#include "Core/Zenith_Engine.h"

// State lives on Flux_GraphicsImpl, reachable via g_xEngine.FluxGraphics().
// Static methods in Flux_Graphics are thin forwards (post-Phase-6a-1). See
// Flux/Flux.h for the broader migration note.

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Flux_BackendTypes.h"
#include "Core/Zenith_RenderGather.h" // Wave 3: main camera comes through the neutral gather
#include "Core/Zenith_GraphicsOptions.h" // m_bShadowsEnabled (cascade render-view activity sync)
#include "Flux/DynamicLights/Flux_LightClusteringImpl.h" // IsInitialised (main-view cluster-lights flag)
#include "DebugVariables/Zenith_DebugVariables.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Profiling/Zenith_Profiling.h"
#include <filesystem>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

// Phase 6a-2: graphics state moved onto Flux_GraphicsImpl held by
// Zenith_Engine. The MRT format defaults that used to be initialiser-
// list-set on the static array now live in a small initialiser routine
// called from Flux_Graphics::Initialise.

Zenith_Maths::Matrix4 Flux_GraphicsImpl::GetViewProjMatrix()    { return m_xFrameConstants.m_xViewProjMat; }
Zenith_Maths::Matrix4 Flux_GraphicsImpl::GetInvViewProjMatrix() { return m_xFrameConstants.m_xInvViewProjMat; }

void Flux_GraphicsImpl::BindFullscreenQuad(Flux_CommandBuffer& xCmd, Flux_Pipeline& xPipeline)
{
	xCmd.SetPipeline(&xPipeline);
	xCmd.SetVertexBuffer(m_xQuadMesh.GetVertexBuffer());
	xCmd.SetIndexBuffer(m_xQuadMesh.GetIndexBuffer());
}
Zenith_Maths::Matrix4 Flux_GraphicsImpl::GetViewMatrix()        { return m_xFrameConstants.m_xViewMat; }
Zenith_Maths::Vector3 Flux_GraphicsImpl::GetSunDir()            { return m_xFrameConstants.m_xSunDir_Pad; }

// Default sun DIRECTION (the way the light travels, into the scene). Normalised
// at upload. This was near-vertical { 0.1, -1, 0.1 } — a "high noon" key that
// fully lights up-facing surfaces (terrain) but leaves vertical surfaces
// (characters, walls) at NdotL ~= 0, so they fall to ambient-only and read as
// near-black silhouettes. An angled key (~45 deg elevation, lighting +x/+z-facing
// surfaces) gives form-defining shading on upright geometry. Games that need a
// specific sun set their own via SetSunOverride / the Sun Direction debug var.
DEBUGVAR Zenith_Maths::Vector3 dbg_SunDir = { -0.4, -0.7, -0.55 };
// Sun radiance (.a) is the direct key-light illuminance. It was 1.0 while the
// sky/IBL ambient bakes from the atmosphere at sun-intensity 20 (see
// Flux_SkyboxImpl::fSUN_INTENSITY + Flux_IBL irradiance/prefilter capture),
// so ambient fill drowned the key -> flat, washed "ambient soup" with no
// form-defining contrast or visible shadows. Raising it to ~12 makes the sun
// the dominant key (auto-exposure normalises absolute brightness; what matters
// is the key:fill ratio). The chromaticity is warm ~5500K daylight white; the
// cool-blue sky ambient stays cool, giving a realistic warm-key/cool-fill split.
DEBUGVAR Zenith_Maths::Vector4 dbg_SunColour = { 1.0, 0.94, 0.86, 3.4f };

DEBUGVAR bool dbg_bQuadUtilisationAnalysis = false;
DEBUGVAR u_int dbg_uTargetPixelsPerTri = 10;

DEBUGVAR bool dbg_bOverrideViewProjMat = false;
DEBUGVAR u_int dbg_uOverrideViewProjMatIndex = 0;

void Flux_GraphicsImpl::InitialiseSamplers()
{
	Flux_Sampler::InitialiseRepeat(m_xRepeatSampler);
	Flux_Sampler::InitialiseClamp(m_xClampSampler);
	Flux_Sampler::InitialisePointClamp(m_xPointSampler);
}

void Flux_GraphicsImpl::Initialise()
{
	// MRT format defaults -- previously a static-init initialiser list,
	// now set at engine-init time on the engine-owned Impl.
	m_aeMRTFormats[MRT_INDEX_DIFFUSE]        = MRT_FORMAT_DIFFUSE;
	m_aeMRTFormats[MRT_INDEX_NORMALSAMBIENT] = MRT_FORMAT_NORMALSAMBIENT;
	m_aeMRTFormats[MRT_INDEX_MATERIAL]       = MRT_FORMAT_MATERIAL;
	m_aeMRTFormats[MRT_INDEX_EMISSIVE]       = MRT_FORMAT_EMISSIVE;

	// Bindless-slot allocator: capacity = the backend's device-clamped table size
	// (set during backend init, before this first-feature Initialise runs). Must be
	// ready before any texture is made bindless below / by assets.
	m_xBindlessAllocator.Initialise(g_xEngine.FluxBackend().GetBindlessTableSize());

	// GPU material table (g_axMaterials, GLOBAL set). Per-frame-in-flight host-coherent
	// buffer; must be ready before any material is registered or drawn.
	m_xMaterialTable.Initialise();

	Flux_SurfaceInfo xTexInfo;
	xTexInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xTexInfo.m_uWidth = 1;
	xTexInfo.m_uHeight = 1;
	xTexInfo.m_uDepth = 1;
	xTexInfo.m_uNumMips = 1;
	xTexInfo.m_uNumLayers = 1;
	xTexInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	u_int8 aucWhiteBlankTexData[] = { 255,255,255,255 };

	// Create white texture (pinned via handle so UnloadUnused never frees it)
	if (Zenith_TextureAsset* pxWhite = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
	{
		pxWhite->CreateFromData(aucWhiteBlankTexData, xTexInfo, false);
		m_xWhiteTexture.Set(pxWhite);
	}

	u_int8 aucBlackBlankTexData[] = { 0,0,0,0 };

	// Create black texture (pinned)
	if (Zenith_TextureAsset* pxBlack = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
	{
		pxBlack->CreateFromData(aucBlackBlankTexData, xTexInfo, false);
		m_xBlackTexture.Set(pxBlack);
	}

	// 64x64 checkerboard pattern with 32x32 quadrants
	// Light grey (200) in top-left and bottom-right, dark grey (150) in top-right and bottom-left
	u_int8 aucGridTexData[64 * 64 * 4];
	for (u_int uY = 0; uY < 64; ++uY)
	{
		for (u_int uX = 0; uX < 64; ++uX)
		{
			bool bTopHalf = (uY < 32);
			bool bLeftHalf = (uX < 32);
			bool bLightQuadrant = (bTopHalf == bLeftHalf);

			u_int8 uGreyValue = bLightQuadrant ? 200 : 150;
			u_int uIdx = (uY * 64 + uX) * 4;
			aucGridTexData[uIdx + 0] = uGreyValue;
			aucGridTexData[uIdx + 1] = uGreyValue;
			aucGridTexData[uIdx + 2] = uGreyValue;
			aucGridTexData[uIdx + 3] = 255;
		}
	}

	// Self-heal: write Grid.ztxtr if missing, then assign the handle by *path* so it
	// carries the stable engine: prefix when copied into materials. Set(ptr) would
	// clear the path (it's the procedural-asset entry point), and any material that
	// later receives this handle would lose the texture's identity at serialization.
	const char* szGridAssetPath = "engine:Grid" ZENITH_TEXTURE_EXT;
	const std::string strGridDiskPath = Zenith_AssetRegistry::ResolvePath(szGridAssetPath);
	if (!std::filesystem::exists(strGridDiskPath))
	{
		Zenith_DataStream xStream;
		xStream << static_cast<int32_t>(64);
		xStream << static_cast<int32_t>(64);
		xStream << static_cast<int32_t>(1);
		xStream << static_cast<TextureFormat>(TEXTURE_FORMAT_RGBA8_UNORM);
		xStream << static_cast<size_t>(sizeof(aucGridTexData));
		xStream.WriteData(aucGridTexData, sizeof(aucGridTexData));
		xStream.WriteToFile(strGridDiskPath.c_str());
	}

	m_xGridTexture = TextureHandle(szGridAssetPath);
	Zenith_Assert(m_xGridTexture.Resolve() != nullptr,
		"Failed to load engine grid texture from %s", szGridAssetPath);

	// Create blank material for use as fallback throughout the engine (pinned).
	// Material will use blank white textures by default (GetXXXTexture returns blank if path not set).
	if (Zenith_MaterialAsset* pxBlankMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>())
	{
		pxBlankMat->SetName("BlankMaterial");
		m_xBlankMaterial.Set(pxBlankMat);
	}

	Flux_MeshGeometry::GenerateFullscreenQuad(m_xQuadMesh);
	auto& xEngine = g_xEngine;
	Flux_MemoryManager& xVulkanMemory = xEngine.FluxMemory();
	xVulkanMemory.InitialiseVertexBuffer(m_xQuadMesh.GetVertexData(), m_xQuadMesh.GetVertexDataSize(), m_xQuadMesh.GetVertexBuffer());
	xVulkanMemory.InitialiseIndexBuffer(m_xQuadMesh.GetIndexData(), m_xQuadMesh.GetIndexDataSize(), m_xQuadMesh.GetIndexBuffer());
	xVulkanMemory.InitialiseDynamicConstantBuffer(nullptr, sizeof(GlobalConstants), m_xGlobalConstantsBuffer);
	// One VIEW CB per fixed render-view slot. An inactive slot's buffer is a
	// valid descriptor (written into its persistent VIEW set each frame) whose
	// contents are never read — no pass ever binds an inactive view's set.
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		xVulkanMemory.InitialiseDynamicConstantBuffer(nullptr, sizeof(ViewConstants), m_axViewConstantsBuffers[u]);
	}

	// Persistent preview-view LDR output — survives graph rebuilds so the SRV
	// the editor registers with ImGui stays valid (transients would dangle it).
	// FINAL_RT_FORMAT so the HDR feature's existing tonemap pipeline (built
	// against the final-RT format) can render straight into it.
	{
		Flux_RenderAttachmentBuilder xBuilder;
		xBuilder.m_uWidth       = kuFLUX_PREVIEW_VIEW_SIZE;
		xBuilder.m_uHeight      = kuFLUX_PREVIEW_VIEW_SIZE;
		xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
		xBuilder.m_eFormat      = FINAL_RT_FORMAT;
		xBuilder.BuildColour(m_xPreviewLDR, "Preview View LDR");
	}

	// Render targets are graph-owned transients, created in SetupTransients.
	// No resize callback needed — the graph re-creates them on every
	// SetupRenderGraph pass, which is already a resize callback.

#ifdef ZENITH_DEBUG_VARIABLES
	xEngine.DebugVariables().AddVector3({ "Render", "Sun Direction" }, dbg_SunDir, -1, 1.);
	xEngine.DebugVariables().AddVector4({ "Render", "Sun Colour" }, dbg_SunColour, 0, 1.);

	xEngine.DebugVariables().AddBoolean({ "Render", "Quad Utilisation Analysis" }, dbg_bQuadUtilisationAnalysis);
	xEngine.DebugVariables().AddUInt32({ "Render", "Target Pixels Per Tri" }, dbg_uTargetPixelsPerTri, 1, 32);

	xEngine.DebugVariables().AddBoolean({ "Render", "Shadows", "Override ViewProj Mat" }, dbg_bOverrideViewProjMat);
	xEngine.DebugVariables().AddUInt32({ "Render", "Shadows", "Override ViewProj Mat Index" }, dbg_uOverrideViewProjMatIndex, 0, ZENITH_FLUX_NUM_CSMS);
#endif

	{
		Flux_BindingGroupEntry& xEntry = m_xFrameConstantsLayout.m_axBindings[0];
		xEntry.m_eKind            = FLUX_RESOURCE_KIND_CONSTANT_BUFFER;
		xEntry.m_uDescriptorCount = 1;
		xEntry.m_bPresent         = true;
	}

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics Initialised");
}

bool Flux_GraphicsImpl::BuildCameraMatrices(FrameConstants& xConstants)
{
#ifdef ZENITH_TOOLS
	auto& xEditor = g_xEngine.Editor();
	if (xEditor.GetEditorMode() != EditorMode::Playing)
	{
		xEditor.BuildViewMatrix(xConstants.m_xViewMat);
		xEditor.BuildProjectionMatrix(xConstants.m_xProjMat);
		xEditor.GetCameraPosition(xConstants.m_xCamPos_Pad);
		return true;
	}
#endif
	Zenith_CameraRenderData xCam;
	if (g_pfnZenithCameraGather) g_pfnZenithCameraGather(xCam);
	if (xCam.m_bValid)
	{
		xConstants.m_xViewMat = xCam.m_xViewMatrix;
		xConstants.m_xProjMat = xCam.m_xProjMatrix;
		xConstants.m_xCamPos_Pad = xCam.m_xPositionPad;
		return true;
	}
	return false;
}

void Flux_GraphicsImpl::UploadFrameConstants()
{
	ZENITH_PROFILE_SCOPE("Flux Upload Frame Constants");
	bool bCameraValid = BuildCameraMatrices(m_xFrameConstants);
	m_bCameraValid = bCameraValid;   // exposed via IsCameraValid() for the scene-graph snapshot frustum

	if (bCameraValid)
	{
		if (dbg_bOverrideViewProjMat)
		{
			// Debug-only (default-off) sun-cascade inspector: the WHOLE frame's view-proj —
			// and hence the scene-graph snapshot's camera frustum (stamped via IsCameraValid()
			// from GetViewProjMatrix()) plus the world reconstruction below — is replaced by the
			// sun's. So under this override the geometry consumers intentionally cull to the SUN
			// frustum, consistent with viewing the frame from the light. Not a camera path.
			m_xFrameConstants.m_xViewProjMat = g_xEngine.Shadows().GetSunViewProjMatrix(dbg_uOverrideViewProjMatIndex);
		}
		else
		{
			m_xFrameConstants.m_xViewProjMat = m_xFrameConstants.m_xProjMat * m_xFrameConstants.m_xViewMat;
		}
		m_xFrameConstants.m_xInvViewProjMat = glm::inverse(m_xFrameConstants.m_xViewProjMat);
		m_xFrameConstants.m_xInvViewMat = glm::inverse(m_xFrameConstants.m_xViewMat);
		m_xFrameConstants.m_xInvProjMat = glm::inverse(m_xFrameConstants.m_xProjMat);
	}

	const Zenith_Maths::Vector3 xSunDir = m_bSunOverride ? m_xSunDirOverride : Zenith_Maths::Vector3(dbg_SunDir.x, dbg_SunDir.y, dbg_SunDir.z);
	const Zenith_Maths::Vector4 xSunCol = m_bSunOverride ? m_xSunColourOverride : dbg_SunColour;
	m_xFrameConstants.m_xSunDir_Pad = glm::normalize(Zenith_Maths::Vector4(xSunDir.x, xSunDir.y, xSunDir.z, 0.));
	m_xFrameConstants.m_xSunColour_Pad = { xSunCol.x, xSunCol.y, xSunCol.z, xSunCol.w };
	int32_t iWidth, iHeight;
	Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
	m_xFrameConstants.m_xScreenDims = { static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight) };
	// Prevent division by zero when window is minimized or has zero dimensions
	m_xFrameConstants.m_xRcpScreenDims = {
		(m_xFrameConstants.m_xScreenDims.x > 0) ? 1.f / m_xFrameConstants.m_xScreenDims.x : 1.f,
		(m_xFrameConstants.m_xScreenDims.y > 0) ? 1.f / m_xFrameConstants.m_xScreenDims.y : 1.f
	};
#ifdef ZENITH_TOOLS
	m_xFrameConstants.m_uQuadUtilisationAnalysis = dbg_bQuadUtilisationAnalysis;
	m_xFrameConstants.m_uTargetPixelsPerTri = dbg_uTargetPixelsPerTri;
#endif
	m_xFrameConstants.m_xCameraNearFar = { GetNearPlane(), GetFarPlane() };

	// Spine: mirror the relevant fields from the CPU-side m_xFrameConstants into the
	// GLOBAL (view-invariant) + VIEW (per-camera) buffers — the only frame-constant
	// buffers the GPU sees (m_xFrameConstants itself is no longer uploaded; its GPU
	// buffer was removed with Common/Frame.slang, but it stays as the CPU camera-
	// matrix source for GetViewProjMatrix()/etc. and the mirror source here).
	m_xGlobalConstantsData.m_uFrameIndex    = g_xEngine.Frame().GetFrameIndex();
	m_xGlobalConstantsData.m_fTimeSeconds   = static_cast<float>(g_xEngine.Frame().GetTimePassed());

	m_xViewConstantsData.m_xViewMat        = m_xFrameConstants.m_xViewMat;
	m_xViewConstantsData.m_xProjMat        = m_xFrameConstants.m_xProjMat;
	m_xViewConstantsData.m_xViewProjMat    = m_xFrameConstants.m_xViewProjMat;
	m_xViewConstantsData.m_xInvViewProjMat = m_xFrameConstants.m_xInvViewProjMat;
	m_xViewConstantsData.m_xInvViewMat     = m_xFrameConstants.m_xInvViewMat;
	m_xViewConstantsData.m_xInvProjMat     = m_xFrameConstants.m_xInvProjMat;
	m_xViewConstantsData.m_xCamPos_Pad     = m_xFrameConstants.m_xCamPos_Pad;
	m_xViewConstantsData.m_xScreenDims     = m_xFrameConstants.m_xScreenDims;
	m_xViewConstantsData.m_xRcpScreenDims  = m_xFrameConstants.m_xRcpScreenDims;
	m_xViewConstantsData.m_xCameraNearFar  = m_xFrameConstants.m_xCameraNearFar;
#ifdef ZENITH_TOOLS
	m_xViewConstantsData.m_uQuadUtilisationAnalysis = m_xFrameConstants.m_uQuadUtilisationAnalysis;
	m_xViewConstantsData.m_uTargetPixelsPerTri      = m_xFrameConstants.m_uTargetPixelsPerTri;
#endif
	// Per-view sun + flags (main view): the scene sun, with the global feature
	// toggles folded into this view's flag word each frame. Lit shaders read
	// these from g_xView — a secondary view (the material preview) carries its
	// own sun and zeroed shadow/cluster flags instead.
	m_xViewConstantsData.m_xSunDir_Pad     = m_xFrameConstants.m_xSunDir_Pad;
	m_xViewConstantsData.m_xSunColour_Pad  = m_xFrameConstants.m_xSunColour_Pad;
	m_xViewConstantsData.m_uViewFlags      =
		  (Zenith_GraphicsOptions::Get().m_bShadowsEnabled ? FLUX_VIEW_FLAG_SHADOWS_ENABLED : 0u)
		| (g_xEngine.LightClustering().IsInitialised() ? FLUX_VIEW_FLAG_CLUSTER_LIGHTS_ENABLED : 0u)
		| FLUX_VIEW_FLAG_SCENE_CONTENT;
	m_xViewConstantsData.m_uViewSlot       = kuFluxViewSlotMain;
	g_xEngine.FluxMemory().UploadBufferData(m_xGlobalConstantsBuffer.GetBuffer().m_xVRAMHandle, &m_xGlobalConstantsData, sizeof(GlobalConstants));
	g_xEngine.FluxMemory().UploadBufferData(m_axViewConstantsBuffers[kuFluxViewSlotMain].GetBuffer().m_xVRAMHandle, &m_xViewConstantsData, sizeof(ViewConstants));

	// Keep the registry's MAIN payload in sync — it is the single CPU source of
	// truth per view (cascade/preview owners stage theirs the same way).
	m_xRenderViews.View(kuFluxViewSlotMain).m_xConstants = m_xViewConstantsData;
}

void Flux_GraphicsImpl::UploadRenderViewConstants()
{
	// Main thread, once per frame, before the persistent VIEW sets are written
	// (Flux_RendererImpl::RecordFrame). Uploads every ACTIVE non-main view's
	// staged ViewConstants payload into its frame-indexed CB. Slot 0 (main) is
	// uploaded by UploadFrameConstants, which also mirrors into the registry.

	// Cascade views activate where their constants are staged (UpdateShadowMatrices)
	// — but that never runs while shadows are OFF, so the toggle-off side is synced
	// here: deactivate the cascade slots so the active-view mask reflects reality
	// (their passes early-out; their sets are never bound while inactive).
	if (!Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		for (u_int u = 0; u < kuFluxViewNumShadowSlots; u++)
		{
			m_xRenderViews.SetViewActive(kuFluxViewSlotShadowFirst + u, false);
		}
	}

	for (u_int u = kuFluxViewSlotMain + 1u; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		if (!m_xRenderViews.IsViewActive(u)) { continue; }
		g_xEngine.FluxMemory().UploadBufferData(m_axViewConstantsBuffers[u].GetBuffer().m_xVRAMHandle,
			&m_xRenderViews.View(u).m_xConstants, sizeof(ViewConstants));
	}
}

TextureFormat Flux_GraphicsImpl::GetMRTFormat(MRTIndex eIndex)
{
	return m_aeMRTFormats[eIndex];
}

const Zenith_Maths::Vector3& Flux_GraphicsImpl::GetCameraPosition()
{
	// Return the xyz components of the camera position (w is padding)
	// Note: This is safe because Vector4's memory layout is {x, y, z, w} contiguously
	// and Vector3 is {x, y, z}, so we can reinterpret the first 3 components
	return *reinterpret_cast<const Zenith_Maths::Vector3*>(&m_xFrameConstants.m_xCamPos_Pad);
}

Flux_ShaderResourceView* Flux_GraphicsImpl::GetGBufferSRV(MRTIndex eIndex, u_int uViewSlot)
{
	return &GetMRTAttachment(eIndex, uViewSlot).SRV();
}

Flux_ShaderResourceView* Flux_GraphicsImpl::GetDepthStencilSRV(u_int uViewSlot)
{
	return &GetDepthAttachment(uViewSlot).SRV();
}

Flux_RenderTargetView* Flux_GraphicsImpl::GetGBufferRTV(MRTIndex eIndex)
{
	return &GetMRTAttachment(eIndex).RTV();
}

Flux_DepthStencilView* Flux_GraphicsImpl::GetDepthStencilDSV()
{
	return &GetDepthAttachment().DSV();
}

float Flux_GraphicsImpl::GetNearPlane()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraNearPlane();
#else
	Zenith_CameraRenderData xCam;
	if (g_pfnZenithCameraGather) g_pfnZenithCameraGather(xCam);
	return xCam.m_bValid ? xCam.m_fNearPlane : 0.1f;
#endif
}
float Flux_GraphicsImpl::GetFarPlane()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraFarPlane();
#else
	Zenith_CameraRenderData xCam;
	if (g_pfnZenithCameraGather) g_pfnZenithCameraGather(xCam);
	return xCam.m_bValid ? xCam.m_fFarPlane : 1000.0f;
#endif
}

float Flux_GraphicsImpl::GetFOV()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraFOV();
#else
	Zenith_CameraRenderData xCam;
	if (g_pfnZenithCameraGather) g_pfnZenithCameraGather(xCam);
	return xCam.m_bValid ? xCam.m_fFOV : 1.0472f;
#endif
}

float Flux_GraphicsImpl::GetAspectRatio()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraAspectRatio();
#else
	Zenith_CameraRenderData xCam;
	if (g_pfnZenithCameraGather) g_pfnZenithCameraGather(xCam);
	return xCam.m_bValid ? xCam.m_fAspectRatio : 1.7778f;
#endif
}

// ---- Transient resource setup -----------------------------------------------

void Flux_GraphicsImpl::SetupTransients(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	const u_int uWidth  = g_xEngine.FluxSwapchain().GetWidth();
	const u_int uHeight = g_xEngine.FluxSwapchain().GetHeight();
	Zenith_Assert(uWidth > 0 && uHeight > 0,
		"Flux_Graphics::SetupTransients: swapchain dimensions are %ux%u — window minimised or swapchain not yet created",
		uWidth, uHeight);

	// G-buffer MRTs / depth / HDR scene are created PER ACTIVE FULL-PIPELINE VIEW
	// (slot 0 = main camera at swapchain dims, always; the preview slot at its own
	// dims when its view is active). Transients for inactive slots are NOT created
	// — no feature declares passes on them, so the graph's unused-transient
	// validation stays clean. A view (de)activation triggers a graph rebuild,
	// which re-runs this walk.
	for (u_int uView = 0; uView < FLUX_MAX_RENDER_VIEWS; uView++)
	{
		const Flux_RenderView& xView = m_xRenderViews.View(uView);
		if (!xView.m_bActive || !xView.m_bFullPipeline) { continue; }
		const u_int uViewW = (uView == kuFluxViewSlotMain) ? uWidth  : xView.m_xTargetDims.x;
		const u_int uViewH = (uView == kuFluxViewSlotMain) ? uHeight : xView.m_xTargetDims.y;
		Zenith_Assert(uViewW > 0 && uViewH > 0,
			"Flux_Graphics::SetupTransients: view slot %u has zero target dims", uView);

		// MRT colour attachments (sRGB diffuse, RGBA16F normals+AO, RGBA8 material).
		for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
		{
			Flux_TransientTextureDesc xDesc;
			xDesc.m_uWidth       = uViewW;
			xDesc.m_uHeight      = uViewH;
			xDesc.m_eFormat      = m_aeMRTFormats[u];
			xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
			m_aaxMRTHandles[uView][u] = xGraph.CreateTransient(xDesc);
		}

		// Depth buffer.
		{
			Flux_TransientTextureDesc xDesc;
			xDesc.m_uWidth          = uViewW;
			xDesc.m_uHeight         = uViewH;
			xDesc.m_eFormat         = DEPTH_FORMAT;
			xDesc.m_uMemoryFlags    = (1u << MEMORY_FLAGS__SHADER_READ);
			xDesc.m_bIsDepthStencil = true;
			m_axDepthHandles[uView] = xGraph.CreateTransient(xDesc);
		}

		// HDR scene target — the shared scene-colour buffer. Created here (with the
		// other shared targets) so it exists before the first feature that writes it
		// (DeferredShading and the post-lighting passes); the HDR feature
		// reads/tonemaps it and owns only its private bloom chain.
		{
			Flux_TransientTextureDesc xDesc;
			xDesc.m_uWidth       = uViewW;
			xDesc.m_uHeight      = uViewH;
			xDesc.m_eFormat      = HDR_SCENE_FORMAT;
			xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
			m_axHDRSceneTargetHandles[uView] = xGraph.CreateTransient(xDesc);
		}
	}

	// Final render target — main view only (the preview tonemaps into the
	// persistent m_xPreviewLDR instead).
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth       = uWidth;
		xDesc.m_uHeight      = uHeight;
		xDesc.m_eFormat      = FINAL_RT_FORMAT;
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		m_xFinalRTHandle = xGraph.CreateTransient(xDesc);
	}
}

#ifdef ZENITH_TOOLS
// Debug-variable callbacks: resolve the current MRT transient's SRV at each
// ImGui draw. Registered once via g_xEngine.DebugVariables().AddTextureCallback —
// the callback returns the live SRV pointer from the (possibly just-rebuilt)
// graph, so rebuilds that invalidate the old SRV don't leave a stale binding
// in the tree. Returns nullptr if the graph isn't set up yet (safe in early
// startup before the first SetupTransients call).
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_MRTDiffuse()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_aaxMRTHandles[kuFluxViewSlotMain][MRT_INDEX_DIFFUSE]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_MRTNormalsAO()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_aaxMRTHandles[kuFluxViewSlotMain][MRT_INDEX_NORMALSAMBIENT]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_MRTMaterial()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_aaxMRTHandles[kuFluxViewSlotMain][MRT_INDEX_MATERIAL]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_Depth()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_axDepthHandles[kuFluxViewSlotMain]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_HDRScene()
{
	if (m_pxGraph == nullptr) return nullptr;
	return &m_pxGraph->GetTransientAttachment(m_axHDRSceneTargetHandles[kuFluxViewSlotMain]).SRV();
}
#endif

// Render target getters — always resolve through the graph's transient slot.
// Per-view: the slot's transients exist only while its view is active (the
// handle assert below fires on a stale/inactive slot).
Flux_RenderAttachment& Flux_GraphicsImpl::GetMRTAttachment(MRTIndex eIndex, u_int uViewSlot)
{
	Zenith_Assert(eIndex < MRT_INDEX_COUNT, "Flux_Graphics::GetMRTAttachment: index %u out of range", static_cast<u_int>(eIndex));
	Zenith_Assert(uViewSlot < FLUX_MAX_RENDER_VIEWS, "Flux_Graphics::GetMRTAttachment: view slot %u out of range", uViewSlot);
	Zenith_Assert(m_pxGraph, "Flux_Graphics::GetMRTAttachment: graph pointer is null — call SetupTransients first");
	return m_pxGraph->GetTransientAttachment(m_aaxMRTHandles[uViewSlot][eIndex]);
}

Flux_RenderAttachment& Flux_GraphicsImpl::GetDepthAttachment(u_int uViewSlot)
{
	Zenith_Assert(uViewSlot < FLUX_MAX_RENDER_VIEWS, "Flux_Graphics::GetDepthAttachment: view slot %u out of range", uViewSlot);
	Zenith_Assert(m_pxGraph, "Flux_Graphics::GetDepthAttachment: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_axDepthHandles[uViewSlot]);
}

Flux_RenderAttachment& Flux_GraphicsImpl::GetFinalRenderTarget()
{
	Zenith_Assert(m_pxGraph, "Flux_Graphics::GetFinalRenderTarget: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_xFinalRTHandle);
}

Flux_RenderAttachment& Flux_GraphicsImpl::GetHDRSceneTarget(u_int uViewSlot)
{
	Zenith_Assert(uViewSlot < FLUX_MAX_RENDER_VIEWS, "Flux_Graphics::GetHDRSceneTarget: view slot %u out of range", uViewSlot);
	Zenith_Assert(m_pxGraph, "Flux_Graphics::GetHDRSceneTarget: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_axHDRSceneTargetHandles[uViewSlot]);
}

Flux_ShaderResourceView& Flux_GraphicsImpl::GetHDRSceneSRV()
{
	return GetHDRSceneTarget().SRV();
}

void Flux_GraphicsImpl::GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = nullptr;
}

void Flux_GraphicsImpl::GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil)
{
	apxColourAttachments[0] = &GetHDRSceneTarget();
	uNumColour = 1;
	pxDepthStencil = &GetDepthAttachment();
}

void Flux_GraphicsImpl::ReleaseAssetReferences()
{
	// Drop refs to all engine bootstrap defaults so the asset registry can delete
	// them in its own Shutdown. Called from Flux::ReleaseAssetReferences before
	// Zenith_AssetRegistry::Shutdown — putting these inside Flux_Graphics::Shutdown
	// would run too late (Flux::Shutdown executes after the registry is gone).
	m_xWhiteTexture.Clear();
	m_xBlackTexture.Clear();
	m_xGridTexture.Clear();
	m_xBlankMaterial.Clear();
	m_xCubemapTexture.Clear();
	m_xWaterNormalTexture.Clear();
}

void Flux_GraphicsImpl::Shutdown()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shutting down...");

	m_pxGraph = nullptr;

	// Destroy quad mesh buffers
	Flux_MemoryManager& xVulkanMemory = g_xEngine.FluxMemory();
	xVulkanMemory.DestroyVertexBuffer(m_xQuadMesh.GetVertexBuffer());
	xVulkanMemory.DestroyIndexBuffer(m_xQuadMesh.GetIndexBuffer());

	// Destroy the GLOBAL/VIEW spine constant buffers (one VIEW CB per view slot).
	xVulkanMemory.DestroyDynamicConstantBuffer(m_xGlobalConstantsBuffer);
	for (u_int u = 0; u < FLUX_MAX_RENDER_VIEWS; u++)
	{
		xVulkanMemory.DestroyDynamicConstantBuffer(m_axViewConstantsBuffers[u]);
	}

	// Destroy the persistent preview-view LDR.
	Flux_RenderAttachmentBuilder::Destroy(m_xPreviewLDR);

	// Destroy the GPU material table buffer.
	m_xMaterialTable.Shutdown();

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shut down");
}
