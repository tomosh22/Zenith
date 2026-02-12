#include "Zenith.h"

#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_SceneManager.h"
#include "EntityComponent/Zenith_SceneData.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Zenith_OS_Include.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_MaterialAsset.h"
#include "AssetHandling/Zenith_TextureAsset.h"

#ifdef ZENITH_TOOLS
#include "Editor/Zenith_Editor.h"
#endif

Flux_TargetSetup Flux_Graphics::s_xMRTTarget;
Flux_TargetSetup Flux_Graphics::s_xFinalRenderTarget;
Flux_TargetSetup Flux_Graphics::s_xFinalRenderTarget_NoDepth;
Flux_TargetSetup Flux_Graphics::s_xNullTargetSetup;  // For compute passes without render targets
Flux_RenderAttachment Flux_Graphics::s_xDepthBuffer;
Flux_Sampler Flux_Graphics::s_xRepeatSampler;
Flux_Sampler Flux_Graphics::s_xClampSampler;
Flux_MeshGeometry Flux_Graphics::s_xQuadMesh;
Flux_DynamicConstantBuffer Flux_Graphics::s_xFrameConstantsBuffer;
Zenith_TextureAsset* Flux_Graphics::s_pxWhiteTexture = nullptr;
Zenith_TextureAsset* Flux_Graphics::s_pxBlackTexture = nullptr;
Zenith_TextureAsset* Flux_Graphics::s_pxGridTexture = nullptr;
Flux_MeshGeometry Flux_Graphics::s_xBlankMesh;
Zenith_MaterialAsset* Flux_Graphics::s_pxBlankMaterial = nullptr;
Zenith_TextureAsset* Flux_Graphics::s_pxCubemapTexture = nullptr;
Zenith_TextureAsset* Flux_Graphics::s_pxWaterNormalTexture = nullptr;
Flux_Graphics::FrameConstants Flux_Graphics::s_xFrameConstants;
Flux_DescriptorSetLayout Flux_Graphics::s_xFrameConstantsLayout;

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

void Flux_Graphics::Initialise()
{
	Flux_Sampler::InitialiseRepeat(s_xRepeatSampler);
	Flux_Sampler::InitialiseClamp(s_xClampSampler);

	Flux_SurfaceInfo xTexInfo;
	xTexInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xTexInfo.m_uWidth = 1;
	xTexInfo.m_uHeight = 1;
	xTexInfo.m_uDepth = 1;
	xTexInfo.m_uNumMips = 1;
	xTexInfo.m_uNumLayers = 1;
	xTexInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	u_int8 aucWhiteBlankTexData[] = { 255,255,255,255 };

	// Create white texture
	s_pxWhiteTexture = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	if (s_pxWhiteTexture)
	{
		s_pxWhiteTexture->CreateFromData(aucWhiteBlankTexData, xTexInfo, false);
	}

	u_int8 aucBlackBlankTexData[] = { 0,0,0,0 };

	// Create black texture
	s_pxBlackTexture = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	if (s_pxBlackTexture)
	{
		s_pxBlackTexture->CreateFromData(aucBlackBlankTexData, xTexInfo, false);
	}

	// Create 64x64 greyscale grid pattern texture for procedural materials
	// This allows materials to use BaseColor for tinting instead of colored 1x1 textures
	// Each 32x32 quadrant is a uniform color, creating a checkerboard pattern
	Flux_SurfaceInfo xGridTexInfo;
	xGridTexInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xGridTexInfo.m_uWidth = 64;
	xGridTexInfo.m_uHeight = 64;
	xGridTexInfo.m_uDepth = 1;
	xGridTexInfo.m_uNumMips = 1;
	xGridTexInfo.m_uNumLayers = 1;
	xGridTexInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	// 64x64 checkerboard pattern with 32x32 quadrants
	// Light grey (200) in top-left and bottom-right, dark grey (150) in top-right and bottom-left
	u_int8 aucGridTexData[64 * 64 * 4];
	for (u_int uY = 0; uY < 64; ++uY)
	{
		for (u_int uX = 0; uX < 64; ++uX)
		{
			// Determine quadrant: top-left/bottom-right = light, top-right/bottom-left = dark
			bool bTopHalf = (uY < 32);
			bool bLeftHalf = (uX < 32);
			bool bLightQuadrant = (bTopHalf == bLeftHalf);  // TL or BR

			u_int8 uGreyValue = bLightQuadrant ? 200 : 150;
			u_int uIdx = (uY * 64 + uX) * 4;
			aucGridTexData[uIdx + 0] = uGreyValue;  // R
			aucGridTexData[uIdx + 1] = uGreyValue;  // G
			aucGridTexData[uIdx + 2] = uGreyValue;  // B
			aucGridTexData[uIdx + 3] = 255;         // A
		}
	}

	// Create grid texture
	s_pxGridTexture = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	if (s_pxGridTexture)
	{
		s_pxGridTexture->CreateFromData(aucGridTexData, xGridTexInfo, false);
	}

	// Create blank material for use as fallback throughout the engine
	s_pxBlankMaterial = Zenith_AssetRegistry::Get().Create<Zenith_MaterialAsset>();
	if (s_pxBlankMaterial)
	{
		s_pxBlankMaterial->SetName("BlankMaterial");
	}
	// Ensure it has white textures set
	if (s_pxBlankMaterial)
	{
		// Material will use blank white textures by default (GetXXXTexture returns blank if path not set)
	}

	Flux_MeshGeometry::GenerateFullscreenQuad(s_xQuadMesh);
	Flux_MemoryManager::InitialiseVertexBuffer(s_xQuadMesh.GetVertexData(), s_xQuadMesh.GetVertexDataSize(), s_xQuadMesh.GetVertexBuffer());
	Flux_MemoryManager::InitialiseIndexBuffer(s_xQuadMesh.GetIndexData(), s_xQuadMesh.GetIndexDataSize(), s_xQuadMesh.GetIndexBuffer());
	Flux_MemoryManager::InitialiseDynamicConstantBuffer(nullptr, sizeof(FrameConstants), s_xFrameConstantsBuffer);

	InitialiseRenderTargets();
	Flux::AddResChangeCallback(InitialiseRenderTargets);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddVector3({ "Render", "Sun Direction" }, dbg_SunDir, -1, 1.);
	Zenith_DebugVariables::AddVector4({ "Render", "Sun Colour" }, dbg_SunColour, 0, 1.);

	Zenith_DebugVariables::AddTexture({ "Render", "Debug", "MRT Diffuse" }, s_xMRTTarget.m_axColourAttachments[MRT_INDEX_DIFFUSE].m_pxSRV);

	Zenith_DebugVariables::AddBoolean({ "Render", "Quad Utilisation Analysis" }, dbg_bQuadUtilisationAnalysis);
	Zenith_DebugVariables::AddUInt32({ "Render", "Target Pixels Per Tri" }, dbg_uTargetPixelsPerTri, 1, 32);

	Zenith_DebugVariables::AddBoolean({ "Render", "Shadows", "Override ViewProj Mat" }, dbg_bOverrideViewProjMat);
	Zenith_DebugVariables::AddUInt32({ "Render", "Shadows", "Override ViewProj Mat Index" }, dbg_uOverrideViewProjMatIndex, 0, ZENITH_FLUX_NUM_CSMS);
#endif

	s_xFrameConstantsLayout.m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics Initialised");
}

void Flux_Graphics::InitialiseRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = Flux_Swapchain::GetWidth();
	xBuilder.m_uHeight = Flux_Swapchain::GetHeight();
	xBuilder.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;

	xBuilder.m_eFormat = TEXTURE_FORMAT_D32_SFLOAT;
	xBuilder.BuildDepthStencil(s_xDepthBuffer, "Flux Graphics Depth Buffer");
	Zenith_Vulkan_VRAM* pxDepthVRAM = Zenith_Vulkan::GetVRAM(s_xDepthBuffer.m_xVRAMHandle);
	Zenith_Log(LOG_CATEGORY_RENDERER, "DEBUG: Main depth buffer VRAM=%u VkImage=0x%llx",
		s_xDepthBuffer.m_xVRAMHandle.AsUInt(), (unsigned long long)(VkImage)pxDepthVRAM->GetImage());

	{
		for (uint32_t u = 0; u < MRT_INDEX_COUNT; u++)
		{
			xBuilder.m_eFormat = s_aeMRTFormats[u];
			xBuilder.BuildColour(s_xMRTTarget.m_axColourAttachments[u], "Flux Graphics MRT " + std::to_string(u));
		}

		s_xMRTTarget.AssignDepthStencil(&s_xDepthBuffer);
	}

	{
		xBuilder.m_eFormat = TEXTURE_FORMAT_R16G16B16A16_UNORM;
		xBuilder.BuildColour(s_xFinalRenderTarget.m_axColourAttachments[0], "Flux Graphics Final Render Target");

		s_xFinalRenderTarget.AssignDepthStencil(&s_xDepthBuffer);

		s_xFinalRenderTarget_NoDepth.m_axColourAttachments[0] = s_xFinalRenderTarget.m_axColourAttachments[0];
	}
}

void Flux_Graphics::UploadFrameConstants()
{
#ifdef ZENITH_TOOLS
	if(Zenith_Editor::GetEditorMode() != EditorMode::Playing)
	{
		Zenith_Editor::BuildViewMatrix(s_xFrameConstants.m_xViewMat);
		Zenith_Editor::BuildProjectionMatrix(s_xFrameConstants.m_xProjMat);
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
		Zenith_Editor::GetCameraPosition(s_xFrameConstants.m_xCamPos_Pad);
	}
	else
#endif
	{
		// Search all loaded scenes for the main camera (supports multi-scene with persistent camera)
		Zenith_CameraComponent* pxCamera = Zenith_SceneManager::FindMainCameraAcrossScenes();
		if (pxCamera)
		{
			pxCamera->BuildViewMatrix(s_xFrameConstants.m_xViewMat);
			pxCamera->BuildProjectionMatrix(s_xFrameConstants.m_xProjMat);
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
			pxCamera->GetPosition(s_xFrameConstants.m_xCamPos_Pad);
		}
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
	return &s_xMRTTarget.m_axColourAttachments[eIndex].m_pxSRV;
}

Flux_ShaderResourceView* Flux_Graphics::GetDepthStencilSRV()
{
	return &s_xDepthBuffer.m_pxSRV;
}

Flux_RenderTargetView* Flux_Graphics::GetGBufferRTV(MRTIndex eIndex)
{
	return &s_xMRTTarget.m_axColourAttachments[eIndex].m_pxRTV;
}

Flux_DepthStencilView* Flux_Graphics::GetDepthStencilDSV()
{
	return &s_xDepthBuffer.m_pxDSV;
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

void Flux_Graphics::Shutdown()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shutting down...");

	// Helper lambda to destroy a render attachment's VRAM and views
	auto DestroyRenderAttachment = [](Flux_RenderAttachment& xAttachment)
	{
		if (xAttachment.m_xVRAMHandle.IsValid())
		{
			Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(xAttachment.m_xVRAMHandle);
			Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, xAttachment.m_xVRAMHandle,
				xAttachment.m_pxRTV.m_xImageViewHandle, xAttachment.m_pxDSV.m_xImageViewHandle,
				xAttachment.m_pxSRV.m_xImageViewHandle, xAttachment.m_pxUAV.m_xImageViewHandle);
			xAttachment.m_xVRAMHandle = Flux_VRAMHandle();
		}
	};

	// Destroy MRT render targets
	for (uint32_t u = 0; u < MRT_INDEX_COUNT; u++)
	{
		DestroyRenderAttachment(s_xMRTTarget.m_axColourAttachments[u]);
	}

	// Destroy final render target
	DestroyRenderAttachment(s_xFinalRenderTarget.m_axColourAttachments[0]);

	// Destroy depth buffer
	DestroyRenderAttachment(s_xDepthBuffer);

	// Destroy quad mesh buffers
	Flux_MemoryManager::DestroyVertexBuffer(s_xQuadMesh.GetVertexBuffer());
	Flux_MemoryManager::DestroyIndexBuffer(s_xQuadMesh.GetIndexBuffer());

	// Destroy frame constants buffer
	Flux_MemoryManager::DestroyDynamicConstantBuffer(s_xFrameConstantsBuffer);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Graphics shut down");
}
