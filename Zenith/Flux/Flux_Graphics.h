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
	static Flux_TargetSetup s_xFinalRenderTarget_NoDepth;

	static struct Flux_RenderAttachment s_xDepthBuffer;

	static Flux_Sampler s_xDefaultSampler;

	static Flux_MeshGeometry s_xQuadMesh;

	static Flux_DynamicConstantBuffer s_xFrameConstantsBuffer;

	static Flux_Texture* s_pxBlankTexture2D;
	static Flux_MeshGeometry s_xBlankMesh;
	static class Flux_Material* s_pxBlankMaterial;

	static ColourFormat s_aeMRTFormats[MRT_INDEX_COUNT];

	static const Zenith_Maths::Vector3& GetCameraPosition();
	static Flux_Texture& GetGBufferTexture(MRTIndex eIndex);
	static Flux_Texture& GetDepthStencilTexture();

	static Zenith_Maths::Matrix4 GetViewProjMatrix() { return s_xFrameConstants.m_xViewProjMat; }
	static Zenith_Maths::Matrix4 GetInvViewProjMatrix() { return s_xFrameConstants.m_xInvViewProjMat; }
	static Zenith_Maths::Matrix4 GetViewMatrix() { return s_xFrameConstants.m_xViewMat; }
	static Zenith_Maths::Vector3 GetSunDir() { return s_xFrameConstants.m_xSunDir_Pad; }
	static float GetNearPlane();
	static float GetFarPlane();
	static float GetFOV();
	static float GetAspectRatio();

	static Flux_DescriptorSetLayout s_xFrameConstantsLayout;
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
		u_int m_uQuadUtilisationAnalysis;
		u_int m_uTargetPixelsPerTri;
	};
	static FrameConstants s_xFrameConstants;
};