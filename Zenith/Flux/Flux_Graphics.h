#pragma once
#include "Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"

class Flux_Graphics
{
public:
	Flux_Graphics() = delete;
	~Flux_Graphics() = delete;

	static void Initialise();
	static void InitialiseRenderTargets();

	static void UploadFrameConstants();

	static Flux_TargetSetup s_xMRTTarget;
	static Flux_TargetSetup s_xFinalRenderTarget;

	static struct Flux_RenderAttachment s_xDepthBuffer;

	static Flux_Sampler s_xDefaultSampler;

	static Flux_MeshGeometry s_xQuadMesh;

	static Flux_ConstantBuffer s_xFrameConstantsBuffer;

	static Flux_Texture s_xBlankTexture2D;
	static Flux_MeshGeometry s_xBlankMesh;

	static ColourFormat s_aeMRTFormats[MRT_INDEX_COUNT];

	static Flux_Texture& GetGBufferTexture(MRTIndex eIndex);
	static Flux_Texture& GetDepthStencilTexture();

	static Zenith_Maths::Matrix4 GetViewProjMatrix() { return s_xFrameConstants.m_xViewProjMat; }
	static Zenith_Maths::Matrix4 GetInvViewProjMatrix() { return s_xFrameConstants.m_xInvViewProjMat; }
private:
	struct FrameConstants
	{
		Zenith_Maths::Matrix4 m_xViewMat;
		Zenith_Maths::Matrix4 m_xProjMat;
		Zenith_Maths::Matrix4 m_xViewProjMat;
		Zenith_Maths::Matrix4 m_xInvViewProjMat;
		Zenith_Maths::Vector4 m_xCamPos_Pad;
		Zenith_Maths::Vector4 m_xSunDir_Pad;
		Zenith_Maths::Vector4 m_xSunColour_Pad;
		Zenith_Maths::UVector2 m_xScreenDims;
		Zenith_Maths::Vector2 m_xRcpScreenDims;
	};
	static FrameConstants s_xFrameConstants;
};