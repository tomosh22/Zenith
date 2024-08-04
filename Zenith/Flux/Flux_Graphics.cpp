#include "Zenith.h"

#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"

Flux_TargetSetup Flux_Graphics::s_xMRTTarget;
Flux_TargetSetup Flux_Graphics::s_xFinalRenderTarget;
Flux_RenderAttachment Flux_Graphics::s_xDepthBuffer;
Flux_Sampler Flux_Graphics::s_xDefaultSampler;
Flux_MeshGeometry Flux_Graphics::s_xQuadMesh;
Flux_ConstantBuffer Flux_Graphics::s_xFrameConstantsBuffer;
Flux_Texture Flux_Graphics::s_xBlankTexture2D;
Flux_MeshGeometry Flux_Graphics::s_xBlankMesh;

ColourFormat Flux_Graphics::s_aeMRTFormats[MRT_INDEX_COUNT]
{
	COLOUR_FORMAT_RGBA8_UNORM, //MRT_INDEX_DIFFUSE
	COLOUR_FORMAT_RGBA8_UNORM, //MRT_INDEX_NORMALSAMBIENT
	COLOUR_FORMAT_RGBA8_UNORM, //MRT_INDEX_MATERIAL
	COLOUR_FORMAT_R16G16B16A16_SFLOAT //MRT_INDEX_WORLDPOS
};

void Flux_Graphics::Initialise()
{
	Flux_Sampler::InitialiseDefault(s_xDefaultSampler);

	Flux_MeshGeometry::GenerateFullscreenQuad(s_xQuadMesh);
	Flux_MemoryManager::InitialiseVertexBuffer(s_xQuadMesh.GetVertexData(), s_xQuadMesh.GetVertexDataSize(), s_xQuadMesh.GetVertexBuffer());
	Flux_MemoryManager::InitialiseIndexBuffer(s_xQuadMesh.GetIndexData(), s_xQuadMesh.GetIndexDataSize(), s_xQuadMesh.GetIndexBuffer());
	Flux_MemoryManager::InitialiseConstantBuffer(nullptr, sizeof(Zenith_FrameConstants), s_xFrameConstantsBuffer);

	InitialiseRenderTargets();
	Flux::AddResChangeCallback(InitialiseRenderTargets);

	Zenith_Log("Flux_Graphics Initialised");
}

void Flux_Graphics::InitialiseRenderTargets()
{
	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = Flux_Swapchain::GetWidth();
	xBuilder.m_uHeight = Flux_Swapchain::GetHeight();

	xBuilder.m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_D32_SFLOAT;
	xBuilder.Build(s_xDepthBuffer, RENDER_TARGET_TYPE_DEPTHSTENCIL);

	{
		for (uint32_t u = 0; u < MRT_INDEX_COUNT; u++)
		{
			xBuilder.m_eColourFormat = s_aeMRTFormats[u];
			xBuilder.Build(s_xMRTTarget.m_axColourAttachments[u], RENDER_TARGET_TYPE_COLOUR);
		}

		s_xMRTTarget.AssignDepthStencil(&s_xDepthBuffer);
	}

	{
		xBuilder.m_eColourFormat = COLOUR_FORMAT_BGRA8_SRGB;
		xBuilder.Build(s_xFinalRenderTarget.m_axColourAttachments[0], RENDER_TARGET_TYPE_COLOUR);

		s_xFinalRenderTarget.AssignDepthStencil(&s_xDepthBuffer);
	}
}

void Flux_Graphics::UploadFrameConstants()
{
	Zenith_CameraComponent& xCamera = Zenith_Scene::GetCurrentScene().GetMainCamera();

	Zenith_FrameConstants xConstants;
	xCamera.BuildViewMatrix(xConstants.m_xViewMat);
	xCamera.BuildProjectionMatrix(xConstants.m_xProjMat);
	xConstants.m_xViewProjMat = xConstants.m_xProjMat * xConstants.m_xViewMat;
	xCamera.GetPosition(xConstants.m_xCamPos_Pad);
	Flux_MemoryManager::UploadData(&s_xFrameConstantsBuffer, &xConstants, sizeof(Zenith_FrameConstants));
}

Flux_Texture& Flux_Graphics::GetGBufferTexture(MRTIndex eIndex)
{
	return s_xMRTTarget.m_axColourAttachments[eIndex].m_axTargetTextures[Flux_Swapchain::GetCurrentFrameIndex()];
}
