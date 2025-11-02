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
	static Flux_TargetSetup s_xNullTargetSetup;  // For compute passes without render targets

	static struct Flux_RenderAttachment s_xDepthBuffer;

	static Flux_Sampler s_xRepeatSampler;
	static Flux_Sampler s_xClampSampler;

	static Flux_MeshGeometry s_xQuadMesh;

	static Flux_DynamicConstantBuffer s_xFrameConstantsBuffer;

	struct BlankTexture
	{
		uint32_t m_uVRAMHandle;
		Flux_ShaderResourceView m_xSRV;
	};

	static BlankTexture s_xWhiteBlankTexture2D;
	static BlankTexture s_xBlackBlankTexture2D;
	
	static Flux_MeshGeometry s_xBlankMesh;
	static class Flux_Material* s_pxBlankMaterial;

	static TextureFormat s_aeMRTFormats[MRT_INDEX_COUNT];

	static const Zenith_Maths::Vector3& GetCameraPosition();
	static Flux_Texture& GetGBufferTexture(MRTIndex eIndex);
	static Flux_Texture& GetDepthStencilTexture();
	
	// View accessors for resource binding
	static Flux_ShaderResourceView* GetGBufferSRV(MRTIndex eIndex);
	static Flux_ShaderResourceView* GetDepthStencilSRV();
	static Flux_RenderTargetView* GetGBufferRTV(MRTIndex eIndex);
	static Flux_DepthStencilView* GetDepthStencilDSV();

	static Zenith_Maths::Matrix4 GetViewProjMatrix() { return s_xFrameConstants.m_xViewProjMat; }
	static Zenith_Maths::Matrix4 GetInvViewProjMatrix() { return s_xFrameConstants.m_xInvViewProjMat; }
	static Zenith_Maths::Matrix4 GetViewMatrix() { return s_xFrameConstants.m_xViewMat; }
	static Zenith_Maths::Vector3 GetSunDir() { return s_xFrameConstants.m_xSunDir_Pad; }
	static float GetNearPlane();
	static float GetFarPlane();
	static float GetFOV();
	static float GetAspectRatio();

	static Flux_DescriptorSetLayout s_xFrameConstantsLayout;

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
private:
	
};