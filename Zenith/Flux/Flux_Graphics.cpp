#include "Zenith.h"

#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Shadows/Flux_Shadows.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Zenith_OS_Include.h"
#include "AssetHandling/Zenith_AssetHandler.h"

Flux_TargetSetup Flux_Graphics::s_xMRTTarget;
Flux_TargetSetup Flux_Graphics::s_xFinalRenderTarget;
Flux_TargetSetup Flux_Graphics::s_xFinalRenderTarget_NoDepth;
Flux_RenderAttachment Flux_Graphics::s_xDepthBuffer;
Flux_Sampler Flux_Graphics::s_xDefaultSampler;
Flux_MeshGeometry Flux_Graphics::s_xQuadMesh;
Flux_ConstantBuffer Flux_Graphics::s_xFrameConstantsBuffer;
Flux_Texture* Flux_Graphics::s_pxBlankTexture2D;
Flux_MeshGeometry Flux_Graphics::s_xBlankMesh;
Flux_Graphics::FrameConstants Flux_Graphics::s_xFrameConstants;
Flux_DescriptorSetLayout Flux_Graphics::s_xFrameConstantsLayout;

ColourFormat Flux_Graphics::s_aeMRTFormats[MRT_INDEX_COUNT]
{
	COLOUR_FORMAT_RGBA8_UNORM, //MRT_INDEX_DIFFUSE
	COLOUR_FORMAT_R16G16B16A16_SFLOAT, //MRT_INDEX_NORMALSAMBIENT
	COLOUR_FORMAT_RGBA8_UNORM, //MRT_INDEX_MATERIAL
	COLOUR_FORMAT_R32G32B32A32_SFLOAT //MRT_INDEX_WORLDPOS #TO unused alpha
};

#ifdef ZENITH_DEBUG_VARIABLES
DEBUGVAR Zenith_Maths::Vector3 dbg_SunDir = { 0.,-0.6, -0.8 };
DEBUGVAR Zenith_Maths::Vector4 dbg_SunColour = { 0.7, 0.4,0.2, 1.f };

DEBUGVAR bool dbg_bOverrideViewProjMat = false;
DEBUGVAR uint32_t dbg_uOverrideViewProjMatIndex = 0;
#endif

void Flux_Graphics::Initialise()
{
	Flux_Sampler::InitialiseDefault(s_xDefaultSampler);

	float afBlankTexData[] = { 1.f,1.f,1.f,1.f };
	s_pxBlankTexture2D = Zenith_AssetHandler::AddTexture2D("Flux Graphics Blank Texture", afBlankTexData, 1, 1, 1, COLOUR_FORMAT_RGBA8_UNORM, DEPTHSTENCIL_FORMAT_NONE, false);

	Flux_MeshGeometry::GenerateFullscreenQuad(s_xQuadMesh);
	Flux_MemoryManager::InitialiseVertexBuffer(s_xQuadMesh.GetVertexData(), s_xQuadMesh.GetVertexDataSize(), s_xQuadMesh.GetVertexBuffer());
	Flux_MemoryManager::InitialiseIndexBuffer(s_xQuadMesh.GetIndexData(), s_xQuadMesh.GetIndexDataSize(), s_xQuadMesh.GetIndexBuffer());
	Flux_MemoryManager::InitialiseConstantBuffer(nullptr, sizeof(FrameConstants), s_xFrameConstantsBuffer);

	InitialiseRenderTargets();
	Flux::AddResChangeCallback(InitialiseRenderTargets);

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddVector3({ "Render", "Sun Direction" }, dbg_SunDir, -1, 1.);
	Zenith_DebugVariables::AddVector4({ "Render", "Sun Colour" }, dbg_SunColour, 0, 1.);

	Zenith_DebugVariables::AddTexture({ "Render", "Debug", "MRT Diffuse" }, *s_xMRTTarget.m_axColourAttachments[MRT_INDEX_DIFFUSE].m_pxTargetTexture);

	Zenith_DebugVariables::AddBoolean({ "Shadows", "Override ViewProj Mat" }, dbg_bOverrideViewProjMat);
	Zenith_DebugVariables::AddUInt32({ "Shadows", "Override ViewProj Mat Index" }, dbg_uOverrideViewProjMatIndex, 0, ZENITH_FLUX_NUM_CSMS);
#endif

	s_xFrameConstantsLayout.m_axBindings[0].m_eType = DESCRIPTOR_TYPE_BUFFER;

	Zenith_Log("Flux_Graphics Initialised");
}

void Flux_Graphics::InitialiseRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = Flux_Swapchain::GetWidth();
	xBuilder.m_uHeight = Flux_Swapchain::GetHeight();

	xBuilder.m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_D32_SFLOAT;
	xBuilder.Build(s_xDepthBuffer, RENDER_TARGET_TYPE_DEPTHSTENCIL, "Flux Graphics Depth Buffer");
	Zenith_Log("Depth Buffer: %p", s_xDepthBuffer.m_pxTargetTexture->GetImage());

	{
		for (uint32_t u = 0; u < MRT_INDEX_COUNT; u++)
		{
			xBuilder.m_eColourFormat = s_aeMRTFormats[u];
			xBuilder.Build(s_xMRTTarget.m_axColourAttachments[u], RENDER_TARGET_TYPE_COLOUR, "Flux Graphics MRT " + std::to_string(u));
		}

		s_xMRTTarget.AssignDepthStencil(&s_xDepthBuffer);
	}

	{
		xBuilder.m_eColourFormat = COLOUR_FORMAT_R16G16B16A16_UNORM;
		xBuilder.Build(s_xFinalRenderTarget.m_axColourAttachments[0], RENDER_TARGET_TYPE_COLOUR, "Flux Graphics Final Render Target");

		s_xFinalRenderTarget.AssignDepthStencil(&s_xDepthBuffer);

		s_xFinalRenderTarget_NoDepth.m_axColourAttachments[0] = s_xFinalRenderTarget.m_axColourAttachments[0];
	}
}

void Flux_Graphics::UploadFrameConstants()
{
	Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	xCamera.BuildViewMatrix(s_xFrameConstants.m_xViewMat);
	xCamera.BuildProjectionMatrix(s_xFrameConstants.m_xProjMat);
	if (dbg_bOverrideViewProjMat)
	{
		s_xFrameConstants.m_xViewProjMat = Flux_Shadows::GetSunViewProjMatrix(dbg_uOverrideViewProjMatIndex);
	}
	else
	{
		s_xFrameConstants.m_xViewProjMat = s_xFrameConstants.m_xProjMat * s_xFrameConstants.m_xViewMat;
	}
	s_xFrameConstants.m_xInvViewProjMat = glm::inverse(s_xFrameConstants.m_xViewProjMat);
	xCamera.GetPosition(s_xFrameConstants.m_xCamPos_Pad);
	s_xFrameConstants.m_xSunDir_Pad = glm::normalize(Zenith_Maths::Vector4(dbg_SunDir.x, dbg_SunDir.y, dbg_SunDir.z, 0.));
	s_xFrameConstants.m_xSunColour_Pad = { dbg_SunColour.x, dbg_SunColour.y, dbg_SunColour.z, dbg_SunColour.w };
	int32_t iWidth, iHeight;
	Zenith_Window::GetInstance()->GetSize(iWidth, iHeight);
	s_xFrameConstants.m_xScreenDims = { static_cast<uint32_t>(iWidth), static_cast<uint32_t>(iHeight) };
	s_xFrameConstants.m_xRcpScreenDims = { 1.f / s_xFrameConstants.m_xScreenDims.x, 1.f / s_xFrameConstants.m_xScreenDims.y };
	Flux_MemoryManager::UploadBufferData(s_xFrameConstantsBuffer.GetBuffer(), &s_xFrameConstants, sizeof(FrameConstants));
}

const Zenith_Maths::Vector3& Flux_Graphics::GetCameraPosition()
{
	return s_xFrameConstants.m_xCamPos_Pad;
}

Flux_Texture& Flux_Graphics::GetGBufferTexture(MRTIndex eIndex)
{
	return *s_xMRTTarget.m_axColourAttachments[eIndex].m_pxTargetTexture;
}

Flux_Texture& Flux_Graphics::GetDepthStencilTexture()
{
	return *s_xDepthBuffer.m_pxTargetTexture;
}

float Flux_Graphics::GetNearPlane()
{
	return Zenith_Scene::GetCurrentScene().GetMainCamera().GetNearPlane();

}
float Flux_Graphics::GetFarPlane()
{
	return Zenith_Scene::GetCurrentScene().GetMainCamera().GetFarPlane();
}

float Flux_Graphics::GetFOV()
{
	return Zenith_Scene::GetCurrentScene().GetMainCamera().GetFOV();
}

float Flux_Graphics::GetAspectRatio()
{
	return Zenith_Scene::GetCurrentScene().GetMainCamera().GetAspectRatio();
}
