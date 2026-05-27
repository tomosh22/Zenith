#include "Zenith.h"

#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include <filesystem>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

// Phase 6a-2: graphics state moved onto Flux_GraphicsImpl held by
// Zenith_Engine. The MRT format defaults that used to be initialiser-
// list-set on the static array now live in a small initialiser routine
// called from Flux_Graphics::Initialise.

Zenith_Maths::Matrix4 Flux_GraphicsImpl::GetViewProjMatrix()    { return g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewProjMat; }
Zenith_Maths::Matrix4 Flux_GraphicsImpl::GetInvViewProjMatrix() { return g_xEngine.FluxGraphics().m_xFrameConstants.m_xInvViewProjMat; }
Zenith_Maths::Matrix4 Flux_GraphicsImpl::GetViewMatrix()        { return g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewMat; }
Zenith_Maths::Vector3 Flux_GraphicsImpl::GetSunDir()            { return g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunDir_Pad; }

DEBUGVAR Zenith_Maths::Vector3 dbg_SunDir = { 0.1,-1.0, 0.1 };
DEBUGVAR Zenith_Maths::Vector4 dbg_SunColour = { 0.9, 0.8,0.7, 1.f };

DEBUGVAR bool dbg_bQuadUtilisationAnalysis = false;
DEBUGVAR u_int dbg_uTargetPixelsPerTri = 10;

DEBUGVAR bool dbg_bOverrideViewProjMat = false;
DEBUGVAR u_int dbg_uOverrideViewProjMatIndex = 0;

void Flux_GraphicsImpl::InitialiseSamplers()
{
	Flux_Sampler::InitialiseRepeat(g_xEngine.FluxGraphics().m_xRepeatSampler);
	Flux_Sampler::InitialiseClamp(g_xEngine.FluxGraphics().m_xClampSampler);
}

void Flux_GraphicsImpl::Initialise()
{
	// MRT format defaults -- previously a static-init initialiser list,
	// now set at engine-init time on the engine-owned Impl.
	g_xEngine.FluxGraphics().m_aeMRTFormats[MRT_INDEX_DIFFUSE]        = TEXTURE_FORMAT_RGBA8_UNORM;
	g_xEngine.FluxGraphics().m_aeMRTFormats[MRT_INDEX_NORMALSAMBIENT] = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
	g_xEngine.FluxGraphics().m_aeMRTFormats[MRT_INDEX_MATERIAL]       = TEXTURE_FORMAT_RGBA8_UNORM;

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
		g_xEngine.FluxGraphics().m_xWhiteTexture.Set(pxWhite);
	}

	u_int8 aucBlackBlankTexData[] = { 0,0,0,0 };

	// Create black texture (pinned)
	if (Zenith_TextureAsset* pxBlack = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
	{
		pxBlack->CreateFromData(aucBlackBlankTexData, xTexInfo, false);
		g_xEngine.FluxGraphics().m_xBlackTexture.Set(pxBlack);
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

	g_xEngine.FluxGraphics().m_xGridTexture = TextureHandle(szGridAssetPath);
	Zenith_Assert(g_xEngine.FluxGraphics().m_xGridTexture.Resolve() != nullptr,
		"Failed to load engine grid texture from %s", szGridAssetPath);

	// Create blank material for use as fallback throughout the engine (pinned).
	// Material will use blank white textures by default (GetXXXTexture returns blank if path not set).
	if (Zenith_MaterialAsset* pxBlankMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>())
	{
		pxBlankMat->SetName("BlankMaterial");
		g_xEngine.FluxGraphics().m_xBlankMaterial.Set(pxBlankMat);
	}

	Flux_MeshGeometry::GenerateFullscreenQuad(g_xEngine.FluxGraphics().m_xQuadMesh);
	g_xEngine.VulkanMemory().InitialiseVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexData(), g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexDataSize(), g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	g_xEngine.VulkanMemory().InitialiseIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexData(), g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexDataSize(), g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());
	g_xEngine.VulkanMemory().InitialiseDynamicConstantBuffer(nullptr, sizeof(FrameConstants), g_xEngine.FluxGraphics().m_xFrameConstantsBuffer);

	// Render targets are graph-owned transients, created in SetupTransients.
	// No resize callback needed — the graph re-creates them on every
	// SetupRenderGraph pass, which is already a resize callback.

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddVector3({ "Render", "Sun Direction" }, dbg_SunDir, -1, 1.);
	g_xEngine.DebugVariables().AddVector4({ "Render", "Sun Colour" }, dbg_SunColour, 0, 1.);

	g_xEngine.DebugVariables().AddBoolean({ "Render", "Quad Utilisation Analysis" }, dbg_bQuadUtilisationAnalysis);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Target Pixels Per Tri" }, dbg_uTargetPixelsPerTri, 1, 32);

	g_xEngine.DebugVariables().AddBoolean({ "Render", "Shadows", "Override ViewProj Mat" }, dbg_bOverrideViewProjMat);
	g_xEngine.DebugVariables().AddUInt32({ "Render", "Shadows", "Override ViewProj Mat Index" }, dbg_uOverrideViewProjMatIndex, 0, ZENITH_FLUX_NUM_CSMS);
#endif

	g_xEngine.FluxGraphics().m_xFrameConstantsLayout.m_axBindings[0].m_eType = BINDING_TYPE_BUFFER;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics Initialised");
}

bool Flux_GraphicsImpl::BuildCameraMatrices(FrameConstants& xConstants)
{
#ifdef ZENITH_TOOLS
	if (g_xEngine.Editor().GetEditorMode() != EditorMode::Playing)
	{
		g_xEngine.Editor().BuildViewMatrix(xConstants.m_xViewMat);
		g_xEngine.Editor().BuildProjectionMatrix(xConstants.m_xProjMat);
		g_xEngine.Editor().GetCameraPosition(xConstants.m_xCamPos_Pad);
		return true;
	}
#endif
	Zenith_CameraComponent* pxCamera = g_xEngine.SceneRegistry().FindMainCameraAcrossScenes();
	if (pxCamera)
	{
		pxCamera->BuildViewMatrix(xConstants.m_xViewMat);
		pxCamera->BuildProjectionMatrix(xConstants.m_xProjMat);
		pxCamera->GetPosition(xConstants.m_xCamPos_Pad);
		return true;
	}
	return false;
}

void Flux_GraphicsImpl::UploadFrameConstants()
{
	bool bCameraValid = BuildCameraMatrices(g_xEngine.FluxGraphics().m_xFrameConstants);

	if (bCameraValid)
	{
		if (dbg_bOverrideViewProjMat)
		{
			g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewProjMat = g_xEngine.Shadows().GetSunViewProjMatrix(dbg_uOverrideViewProjMatIndex);
		}
		else
		{
			g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewProjMat = g_xEngine.FluxGraphics().m_xFrameConstants.m_xProjMat * g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewMat;
		}
		g_xEngine.FluxGraphics().m_xFrameConstants.m_xInvViewProjMat = glm::inverse(g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewProjMat);
		g_xEngine.FluxGraphics().m_xFrameConstants.m_xInvViewMat = glm::inverse(g_xEngine.FluxGraphics().m_xFrameConstants.m_xViewMat);
		g_xEngine.FluxGraphics().m_xFrameConstants.m_xInvProjMat = glm::inverse(g_xEngine.FluxGraphics().m_xFrameConstants.m_xProjMat);
	}

	g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunDir_Pad = glm::normalize(Zenith_Maths::Vector4(dbg_SunDir.x, dbg_SunDir.y, dbg_SunDir.z, 0.));
	g_xEngine.FluxGraphics().m_xFrameConstants.m_xSunColour_Pad = { dbg_SunColour.x, dbg_SunColour.y, dbg_SunColour.z, dbg_SunColour.w };
	int32_t iWidth, iHeight;
	Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
	g_xEngine.FluxGraphics().m_xFrameConstants.m_xScreenDims = { static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight) };
	// Prevent division by zero when window is minimized or has zero dimensions
	g_xEngine.FluxGraphics().m_xFrameConstants.m_xRcpScreenDims = {
		(g_xEngine.FluxGraphics().m_xFrameConstants.m_xScreenDims.x > 0) ? 1.f / g_xEngine.FluxGraphics().m_xFrameConstants.m_xScreenDims.x : 1.f,
		(g_xEngine.FluxGraphics().m_xFrameConstants.m_xScreenDims.y > 0) ? 1.f / g_xEngine.FluxGraphics().m_xFrameConstants.m_xScreenDims.y : 1.f
	};
#ifdef ZENITH_TOOLS
	g_xEngine.FluxGraphics().m_xFrameConstants.m_uQuadUtilisationAnalysis = dbg_bQuadUtilisationAnalysis;
	g_xEngine.FluxGraphics().m_xFrameConstants.m_uTargetPixelsPerTri = dbg_uTargetPixelsPerTri;
#endif
	g_xEngine.FluxGraphics().m_xFrameConstants.m_xCameraNearFar = { GetNearPlane(), GetFarPlane() };
	g_xEngine.VulkanMemory().UploadBufferData(g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetBuffer().m_xVRAMHandle, &g_xEngine.FluxGraphics().m_xFrameConstants, sizeof(FrameConstants));
}

TextureFormat Flux_GraphicsImpl::GetMRTFormat(MRTIndex eIndex)
{
	return g_xEngine.FluxGraphics().m_aeMRTFormats[eIndex];
}

const Zenith_Maths::Vector3& Flux_GraphicsImpl::GetCameraPosition()
{
	// Return the xyz components of the camera position (w is padding)
	// Note: This is safe because Vector4's memory layout is {x, y, z, w} contiguously
	// and Vector3 is {x, y, z}, so we can reinterpret the first 3 components
	return *reinterpret_cast<const Zenith_Maths::Vector3*>(&g_xEngine.FluxGraphics().m_xFrameConstants.m_xCamPos_Pad);
}

Flux_ShaderResourceView* Flux_GraphicsImpl::GetGBufferSRV(MRTIndex eIndex)
{
	return &GetMRTAttachment(eIndex).SRV();
}

Flux_ShaderResourceView* Flux_GraphicsImpl::GetDepthStencilSRV()
{
	return &GetDepthAttachment().SRV();
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
	Zenith_CameraComponent* pxCamera = g_xEngine.SceneRegistry().FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetNearPlane() : 0.1f;
#endif
}
float Flux_GraphicsImpl::GetFarPlane()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraFarPlane();
#else
	Zenith_CameraComponent* pxCamera = g_xEngine.SceneRegistry().FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetFarPlane() : 1000.0f;
#endif
}

float Flux_GraphicsImpl::GetFOV()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraFOV();
#else
	Zenith_CameraComponent* pxCamera = g_xEngine.SceneRegistry().FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetFOV() : 1.0472f;
#endif
}

float Flux_GraphicsImpl::GetAspectRatio()
{
#ifdef ZENITH_TOOLS
	return g_xEngine.Editor().GetCameraAspectRatio();
#else
	Zenith_CameraComponent* pxCamera = g_xEngine.SceneRegistry().FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetAspectRatio() : 1.7778f;
#endif
}

// ---- Transient resource setup -----------------------------------------------

void Flux_GraphicsImpl::SetupTransients(Flux_RenderGraph& xGraph)
{
	g_xEngine.FluxGraphics().m_pxGraph = &xGraph;

	const u_int uWidth  = g_xEngine.VulkanSwapchain().GetWidth();
	const u_int uHeight = g_xEngine.VulkanSwapchain().GetHeight();
	Zenith_Assert(uWidth > 0 && uHeight > 0,
		"Flux_Graphics::SetupTransients: swapchain dimensions are %ux%u — window minimised or swapchain not yet created",
		uWidth, uHeight);

	// MRT colour attachments (sRGB diffuse, RGBA16F normals+AO, RGBA8 material).
	for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth       = uWidth;
		xDesc.m_uHeight      = uHeight;
		xDesc.m_eFormat      = g_xEngine.FluxGraphics().m_aeMRTFormats[u];
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		g_xEngine.FluxGraphics().m_axMRTHandles[u] = xGraph.CreateTransient(xDesc);
	}

	// Depth buffer.
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth          = uWidth;
		xDesc.m_uHeight         = uHeight;
		xDesc.m_eFormat         = DEPTH_FORMAT;
		xDesc.m_uMemoryFlags    = (1u << MEMORY_FLAGS__SHADER_READ);
		xDesc.m_bIsDepthStencil = true;
		g_xEngine.FluxGraphics().m_xDepthHandle = xGraph.CreateTransient(xDesc);
	}

	// Final render target.
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth       = uWidth;
		xDesc.m_uHeight      = uHeight;
		xDesc.m_eFormat      = FINAL_RT_FORMAT;
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		g_xEngine.FluxGraphics().m_xFinalRTHandle = xGraph.CreateTransient(xDesc);
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
	if (g_xEngine.FluxGraphics().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_axMRTHandles[MRT_INDEX_DIFFUSE]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_MRTNormalsAO()
{
	if (g_xEngine.FluxGraphics().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_axMRTHandles[MRT_INDEX_NORMALSAMBIENT]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_MRTMaterial()
{
	if (g_xEngine.FluxGraphics().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_axMRTHandles[MRT_INDEX_MATERIAL]).SRV();
}
const Flux_ShaderResourceView* Flux_GraphicsImpl::GetDebugSRV_Depth()
{
	if (g_xEngine.FluxGraphics().m_pxGraph == nullptr) return nullptr;
	return &g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_xDepthHandle).SRV();
}
#endif

// Render target getters — always resolve through the graph's transient slot.
Flux_RenderAttachment& Flux_GraphicsImpl::GetMRTAttachment(MRTIndex eIndex)
{
	Zenith_Assert(eIndex < MRT_INDEX_COUNT, "Flux_Graphics::GetMRTAttachment: index %u out of range", static_cast<u_int>(eIndex));
	Zenith_Assert(g_xEngine.FluxGraphics().m_pxGraph, "Flux_Graphics::GetMRTAttachment: graph pointer is null — call SetupTransients first");
	return g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_axMRTHandles[eIndex]);
}

Flux_RenderAttachment& Flux_GraphicsImpl::GetDepthAttachment()
{
	Zenith_Assert(g_xEngine.FluxGraphics().m_pxGraph, "Flux_Graphics::GetDepthAttachment: graph pointer is null");
	return g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_xDepthHandle);
}

Flux_RenderAttachment& Flux_GraphicsImpl::GetFinalRenderTarget()
{
	Zenith_Assert(g_xEngine.FluxGraphics().m_pxGraph, "Flux_Graphics::GetFinalRenderTarget: graph pointer is null");
	return g_xEngine.FluxGraphics().m_pxGraph->GetTransientAttachment(g_xEngine.FluxGraphics().m_xFinalRTHandle);
}

void Flux_GraphicsImpl::ReleaseAssetReferences()
{
	// Drop refs to all engine bootstrap defaults so the asset registry can delete
	// them in its own Shutdown. Called from Flux::ReleaseAssetReferences before
	// Zenith_AssetRegistry::Shutdown — putting these inside Flux_Graphics::Shutdown
	// would run too late (Flux::Shutdown executes after the registry is gone).
	g_xEngine.FluxGraphics().m_xWhiteTexture.Clear();
	g_xEngine.FluxGraphics().m_xBlackTexture.Clear();
	g_xEngine.FluxGraphics().m_xGridTexture.Clear();
	g_xEngine.FluxGraphics().m_xBlankMaterial.Clear();
	g_xEngine.FluxGraphics().m_xCubemapTexture.Clear();
	g_xEngine.FluxGraphics().m_xWaterNormalTexture.Clear();
}

void Flux_GraphicsImpl::Shutdown()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shutting down...");

	g_xEngine.FluxGraphics().m_pxGraph = nullptr;

	// Destroy quad mesh buffers
	g_xEngine.VulkanMemory().DestroyVertexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	g_xEngine.VulkanMemory().DestroyIndexBuffer(g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	// Destroy frame constants buffer
	g_xEngine.VulkanMemory().DestroyDynamicConstantBuffer(g_xEngine.FluxGraphics().m_xFrameConstantsBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shut down");
}
