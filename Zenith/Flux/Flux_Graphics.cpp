#include "Zenith.h"

#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Zenith_OS_Include.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"
#include <filesystem>

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

// Graph-owned transients — backing Flux_RenderAttachments are allocated and
// destroyed by the render graph, sized from the descriptors set in
// SetupTransients. All G-buffer / depth / final-RT access flows through
// GetMRTAttachment / GetDepthAttachment / GetFinalRenderTarget.
Flux_TransientHandle Flux_Graphics::s_axMRTHandles[MRT_INDEX_COUNT];
Flux_TransientHandle Flux_Graphics::s_xFinalRTHandle;
Flux_TransientHandle Flux_Graphics::s_xDepthHandle;
Flux_RenderGraph* Flux_Graphics::s_pxGraph = nullptr;
Flux_Sampler Flux_Graphics::s_xRepeatSampler;
Flux_Sampler Flux_Graphics::s_xClampSampler;
Flux_MeshGeometry Flux_Graphics::s_xQuadMesh;
Flux_DynamicConstantBuffer Flux_Graphics::s_xFrameConstantsBuffer;
TextureHandle Flux_Graphics::s_xWhiteTexture;
TextureHandle Flux_Graphics::s_xBlackTexture;
TextureHandle Flux_Graphics::s_xGridTexture;
Flux_MeshGeometry Flux_Graphics::s_xBlankMesh;
MaterialHandle Flux_Graphics::s_xBlankMaterial;
TextureHandle Flux_Graphics::s_xCubemapTexture;
TextureHandle Flux_Graphics::s_xWaterNormalTexture;
Flux_Graphics::FrameConstants Flux_Graphics::s_xFrameConstants;
Flux_BindingGroupLayout Flux_Graphics::s_xFrameConstantsLayout;

TextureFormat Flux_Graphics::s_aeMRTFormats[MRT_INDEX_COUNT]
{
	TEXTURE_FORMAT_RGBA8_UNORM, //MRT_INDEX_DIFFUSE
	TEXTURE_FORMAT_R16G16B16A16_SFLOAT, //MRT_INDEX_NORMALSAMBIENT
	TEXTURE_FORMAT_RGBA8_UNORM, //MRT_INDEX_MATERIAL
};

DEBUGVAR Zenith_Maths::Vector3 dbg_SunDir = { 0.1,-1.0, 0.1 };
DEBUGVAR Zenith_Maths::Vector4 dbg_SunColour = { 0.9, 0.8,0.7, 1.f };

DEBUGVAR bool dbg_bQuadUtilisationAnalysis = false;
DEBUGVAR u_int dbg_uTargetPixelsPerTri = 10;

DEBUGVAR bool dbg_bOverrideViewProjMat = false;
DEBUGVAR u_int dbg_uOverrideViewProjMatIndex = 0;

void Flux_Graphics::InitialiseSamplers()
{
	Flux_Sampler::InitialiseRepeat(s_xRepeatSampler);
	Flux_Sampler::InitialiseClamp(s_xClampSampler);
}

void Flux_Graphics::Initialise()
{
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
		s_xWhiteTexture.Set(pxWhite);
	}

	u_int8 aucBlackBlankTexData[] = { 0,0,0,0 };

	// Create black texture (pinned)
	if (Zenith_TextureAsset* pxBlack = Zenith_AssetRegistry::Create<Zenith_TextureAsset>())
	{
		pxBlack->CreateFromData(aucBlackBlankTexData, xTexInfo, false);
		s_xBlackTexture.Set(pxBlack);
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

	s_xGridTexture = TextureHandle(szGridAssetPath);
	Zenith_Assert(s_xGridTexture.Resolve() != nullptr,
		"Failed to load engine grid texture from %s", szGridAssetPath);

	// Create blank material for use as fallback throughout the engine (pinned).
	// Material will use blank white textures by default (GetXXXTexture returns blank if path not set).
	if (Zenith_MaterialAsset* pxBlankMat = Zenith_AssetRegistry::Create<Zenith_MaterialAsset>())
	{
		pxBlankMat->SetName("BlankMaterial");
		s_xBlankMaterial.Set(pxBlankMat);
	}

	Flux_MeshGeometry::GenerateFullscreenQuad(s_xQuadMesh);
	Flux_MemoryManager::InitialiseVertexBuffer(s_xQuadMesh.GetVertexData(), s_xQuadMesh.GetVertexDataSize(), s_xQuadMesh.GetVertexBuffer());
	Flux_MemoryManager::InitialiseIndexBuffer(s_xQuadMesh.GetIndexData(), s_xQuadMesh.GetIndexDataSize(), s_xQuadMesh.GetIndexBuffer());
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(FrameConstants), s_xFrameConstantsBuffer);

	// Render targets are graph-owned transients, created in SetupTransients.
	// No resize callback needed — the graph re-creates them on every
	// SetupRenderGraph pass, which is already a resize callback.

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddVector3({ "Render", "Sun Direction" }, dbg_SunDir, -1, 1.);
	Zenith_DebugVariables::AddVector4({ "Render", "Sun Colour" }, dbg_SunColour, 0, 1.);

	Zenith_DebugVariables::AddBoolean({ "Render", "Quad Utilisation Analysis" }, dbg_bQuadUtilisationAnalysis);
	Zenith_DebugVariables::AddUInt32({ "Render", "Target Pixels Per Tri" }, dbg_uTargetPixelsPerTri, 1, 32);

	Zenith_DebugVariables::AddBoolean({ "Render", "Shadows", "Override ViewProj Mat" }, dbg_bOverrideViewProjMat);
	Zenith_DebugVariables::AddUInt32({ "Render", "Shadows", "Override ViewProj Mat Index" }, dbg_uOverrideViewProjMatIndex, 0, ZENITH_FLUX_NUM_CSMS);
#endif

	s_xFrameConstantsLayout.m_axBindings[0].m_eType = BINDING_TYPE_BUFFER;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics Initialised");
}

bool Flux_Graphics::BuildCameraMatrices(FrameConstants& xConstants)
{
#ifdef ZENITH_TOOLS
	if (Zenith_Editor::GetEditorMode() != EditorMode::Playing)
	{
		Zenith_Editor::BuildViewMatrix(xConstants.m_xViewMat);
		Zenith_Editor::BuildProjectionMatrix(xConstants.m_xProjMat);
		Zenith_Editor::GetCameraPosition(xConstants.m_xCamPos_Pad);
		return true;
	}
#endif
	Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
	if (pxCamera)
	{
		pxCamera->BuildViewMatrix(xConstants.m_xViewMat);
		pxCamera->BuildProjectionMatrix(xConstants.m_xProjMat);
		pxCamera->GetPosition(xConstants.m_xCamPos_Pad);
		return true;
	}
	return false;
}

void Flux_Graphics::UploadFrameConstants()
{
	bool bCameraValid = BuildCameraMatrices(s_xFrameConstants);

	if (bCameraValid)
	{
		if (dbg_bOverrideViewProjMat)
		{
			s_xFrameConstants.m_xViewProjMat = Flux_Shadows::GetSunViewProjMatrix(dbg_uOverrideViewProjMatIndex);
		}
		else
		{
			s_xFrameConstants.m_xViewProjMat = s_xFrameConstants.m_xProjMat * s_xFrameConstants.m_xViewMat;
		}
		s_xFrameConstants.m_xInvViewProjMat = glm::inverse(s_xFrameConstants.m_xViewProjMat);
		s_xFrameConstants.m_xInvViewMat = glm::inverse(s_xFrameConstants.m_xViewMat);
		s_xFrameConstants.m_xInvProjMat = glm::inverse(s_xFrameConstants.m_xProjMat);
	}

	s_xFrameConstants.m_xSunDir_Pad = glm::normalize(Zenith_Maths::Vector4(dbg_SunDir.x, dbg_SunDir.y, dbg_SunDir.z, 0.));
	s_xFrameConstants.m_xSunColour_Pad = { dbg_SunColour.x, dbg_SunColour.y, dbg_SunColour.z, dbg_SunColour.w };
	int32_t iWidth, iHeight;
	Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
	s_xFrameConstants.m_xScreenDims = { static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight) };
	// Prevent division by zero when window is minimized or has zero dimensions
	s_xFrameConstants.m_xRcpScreenDims = {
		(s_xFrameConstants.m_xScreenDims.x > 0) ? 1.f / s_xFrameConstants.m_xScreenDims.x : 1.f,
		(s_xFrameConstants.m_xScreenDims.y > 0) ? 1.f / s_xFrameConstants.m_xScreenDims.y : 1.f
	};
#ifdef ZENITH_TOOLS
	s_xFrameConstants.m_uQuadUtilisationAnalysis = dbg_bQuadUtilisationAnalysis;
	s_xFrameConstants.m_uTargetPixelsPerTri = dbg_uTargetPixelsPerTri;
#endif
	s_xFrameConstants.m_xCameraNearFar = { GetNearPlane(), GetFarPlane() };
	Flux_MemoryManager::UploadBufferData(s_xFrameConstantsBuffer.GetBuffer().m_xVRAMHandle, &s_xFrameConstants, sizeof(FrameConstants));
}

TextureFormat Flux_Graphics::GetMRTFormat(MRTIndex eIndex)
{
	return s_aeMRTFormats[eIndex];
}

const Zenith_Maths::Vector3& Flux_Graphics::GetCameraPosition()
{
	// Return the xyz components of the camera position (w is padding)
	// Note: This is safe because Vector4's memory layout is {x, y, z, w} contiguously
	// and Vector3 is {x, y, z}, so we can reinterpret the first 3 components
	return *reinterpret_cast<const Zenith_Maths::Vector3*>(&s_xFrameConstants.m_xCamPos_Pad);
}

Flux_ShaderResourceView* Flux_Graphics::GetGBufferSRV(MRTIndex eIndex)
{
	return &GetMRTAttachment(eIndex).SRV();
}

Flux_ShaderResourceView* Flux_Graphics::GetDepthStencilSRV()
{
	return &GetDepthAttachment().SRV();
}

Flux_RenderTargetView* Flux_Graphics::GetGBufferRTV(MRTIndex eIndex)
{
	return &GetMRTAttachment(eIndex).RTV();
}

Flux_DepthStencilView* Flux_Graphics::GetDepthStencilDSV()
{
	return &GetDepthAttachment().DSV();
}

float Flux_Graphics::GetNearPlane()
{
#ifdef ZENITH_TOOLS
	return Zenith_Editor::GetCameraNearPlane();
#else
	Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetNearPlane() : 0.1f;
#endif
}
float Flux_Graphics::GetFarPlane()
{
#ifdef ZENITH_TOOLS
	return Zenith_Editor::GetCameraFarPlane();
#else
	Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetFarPlane() : 1000.0f;
#endif
}

float Flux_Graphics::GetFOV()
{
#ifdef ZENITH_TOOLS
	return Zenith_Editor::GetCameraFOV();
#else
	Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetFOV() : 1.0472f;
#endif
}

float Flux_Graphics::GetAspectRatio()
{
#ifdef ZENITH_TOOLS
	return Zenith_Editor::GetCameraAspectRatio();
#else
	Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
	return pxCamera ? pxCamera->GetAspectRatio() : 1.7778f;
#endif
}

// ---- Transient resource setup -----------------------------------------------

void Flux_Graphics::SetupTransients(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;

	const u_int uWidth  = Flux_Swapchain::GetWidth();
	const u_int uHeight = Flux_Swapchain::GetHeight();
	Zenith_Assert(uWidth > 0 && uHeight > 0,
		"Flux_Graphics::SetupTransients: swapchain dimensions are %ux%u — window minimised or swapchain not yet created",
		uWidth, uHeight);

	// MRT colour attachments (sRGB diffuse, RGBA16F normals+AO, RGBA8 material).
	for (u_int u = 0; u < MRT_INDEX_COUNT; u++)
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth       = uWidth;
		xDesc.m_uHeight      = uHeight;
		xDesc.m_eFormat      = s_aeMRTFormats[u];
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		s_axMRTHandles[u] = xGraph.CreateTransient(xDesc);
	}

	// Depth buffer.
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth          = uWidth;
		xDesc.m_uHeight         = uHeight;
		xDesc.m_eFormat         = DEPTH_FORMAT;
		xDesc.m_uMemoryFlags    = (1u << MEMORY_FLAGS__SHADER_READ);
		xDesc.m_bIsDepthStencil = true;
		s_xDepthHandle = xGraph.CreateTransient(xDesc);
	}

	// Final render target.
	{
		Flux_TransientTextureDesc xDesc;
		xDesc.m_uWidth       = uWidth;
		xDesc.m_uHeight      = uHeight;
		xDesc.m_eFormat      = FINAL_RT_FORMAT;
		xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
		s_xFinalRTHandle = xGraph.CreateTransient(xDesc);
	}
}

#ifdef ZENITH_TOOLS
// Debug-variable callbacks: resolve the current MRT transient's SRV at each
// ImGui draw. Registered once via Zenith_DebugVariables::AddTextureCallback —
// the callback returns the live SRV pointer from the (possibly just-rebuilt)
// graph, so rebuilds that invalidate the old SRV don't leave a stale binding
// in the tree. Returns nullptr if the graph isn't set up yet (safe in early
// startup before the first SetupTransients call).
const Flux_ShaderResourceView* Flux_Graphics::GetDebugSRV_MRTDiffuse()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &s_pxGraph->GetTransientAttachment(s_axMRTHandles[MRT_INDEX_DIFFUSE]).SRV();
}
const Flux_ShaderResourceView* Flux_Graphics::GetDebugSRV_MRTNormalsAO()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &s_pxGraph->GetTransientAttachment(s_axMRTHandles[MRT_INDEX_NORMALSAMBIENT]).SRV();
}
const Flux_ShaderResourceView* Flux_Graphics::GetDebugSRV_MRTMaterial()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &s_pxGraph->GetTransientAttachment(s_axMRTHandles[MRT_INDEX_MATERIAL]).SRV();
}
const Flux_ShaderResourceView* Flux_Graphics::GetDebugSRV_Depth()
{
	if (s_pxGraph == nullptr) return nullptr;
	return &s_pxGraph->GetTransientAttachment(s_xDepthHandle).SRV();
}
#endif

// Render target getters — always resolve through the graph's transient slot.
Flux_RenderAttachment& Flux_Graphics::GetMRTAttachment(MRTIndex eIndex)
{
	Zenith_Assert(eIndex < MRT_INDEX_COUNT, "Flux_Graphics::GetMRTAttachment: index %u out of range", static_cast<u_int>(eIndex));
	Zenith_Assert(s_pxGraph, "Flux_Graphics::GetMRTAttachment: graph pointer is null — call SetupTransients first");
	return s_pxGraph->GetTransientAttachment(s_axMRTHandles[eIndex]);
}

Flux_RenderAttachment& Flux_Graphics::GetDepthAttachment()
{
	Zenith_Assert(s_pxGraph, "Flux_Graphics::GetDepthAttachment: graph pointer is null");
	return s_pxGraph->GetTransientAttachment(s_xDepthHandle);
}

Flux_RenderAttachment& Flux_Graphics::GetFinalRenderTarget()
{
	Zenith_Assert(s_pxGraph, "Flux_Graphics::GetFinalRenderTarget: graph pointer is null");
	return s_pxGraph->GetTransientAttachment(s_xFinalRTHandle);
}

void Flux_Graphics::ReleaseAssetReferences()
{
	// Drop refs to all engine bootstrap defaults so the asset registry can delete
	// them in its own Shutdown. Called from Flux::ReleaseAssetReferences before
	// Zenith_AssetRegistry::Shutdown — putting these inside Flux_Graphics::Shutdown
	// would run too late (Flux::Shutdown executes after the registry is gone).
	s_xWhiteTexture.Clear();
	s_xBlackTexture.Clear();
	s_xGridTexture.Clear();
	s_xBlankMaterial.Clear();
	s_xCubemapTexture.Clear();
	s_xWaterNormalTexture.Clear();
}

void Flux_Graphics::Shutdown()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shutting down...");

	s_pxGraph = nullptr;

	// Destroy quad mesh buffers
	Flux_MemoryManager::DestroyVertexBuffer(s_xQuadMesh.GetVertexBuffer());
	Flux_MemoryManager::DestroyIndexBuffer(s_xQuadMesh.GetIndexBuffer());

	// Destroy frame constants buffer
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xFrameConstantsBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shut down");
}
