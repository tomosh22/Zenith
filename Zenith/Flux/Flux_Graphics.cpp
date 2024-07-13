#include "Zenith.h"

#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_Types.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

Flux_TargetSetup Flux_Graphics::s_xFinalRenderTarget;
Flux_Sampler Flux_Graphics::s_xDefaultSampler;
Flux_MeshGeometry Flux_Graphics::s_xQuadMesh;
Flux_VertexBuffer Flux_Graphics::s_xQuadVertexBuffer;
Flux_IndexBuffer Flux_Graphics::s_xQuadIndexBuffer;

void Flux_Graphics::Initialise()
{

	Flux_Sampler::InitialiseDefault(s_xDefaultSampler);
	Flux_MeshGeometry::GenerateFullscreenQuad(s_xQuadMesh);


	Flux_RenderAttachmentBuilder xBuilder;
	xBuilder.m_uWidth = Flux_Swapchain::GetWidth();
	xBuilder.m_uHeight = Flux_Swapchain::GetHeight();

	Flux_RenderAttachment xColour;
	xBuilder.m_eColourFormat = COLOUR_FORMAT_BGRA8_SRGB;
	xBuilder.Build(xColour, RENDER_TARGET_TYPE_COLOUR);

	Flux_RenderAttachment xDepthStencil;
	xBuilder.m_eDepthStencilFormat = DEPTHSTENCIL_FORMAT_D32_SFLOAT;
	xBuilder.Build(xDepthStencil, RENDER_TARGET_TYPE_DEPTHSTENCIL);

	s_xFinalRenderTarget.m_axColourAttachments[0] = xColour;
	s_xFinalRenderTarget.m_xDepthStencil = xDepthStencil;

	Zenith_Log("Flux_Graphics Initialised");
}