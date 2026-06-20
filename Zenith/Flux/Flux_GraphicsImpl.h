#pragma once

#include "Flux/Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "AssetHandling/Zenith_AssetHandle.h"

// ---- Core render-target format constants --------------------------------
static constexpr TextureFormat MRT_FORMAT_DIFFUSE         = TEXTURE_FORMAT_RGBA8_UNORM;
static constexpr TextureFormat MRT_FORMAT_NORMALSAMBIENT  = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
static constexpr TextureFormat MRT_FORMAT_MATERIAL        = TEXTURE_FORMAT_RGBA8_UNORM;
static constexpr TextureFormat MRT_FORMAT_EMISSIVE        = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;	// HDR emissive (feeds bloom) + clear-coat roughness in A
static constexpr TextureFormat DEPTH_FORMAT               = TEXTURE_FORMAT_D32_SFLOAT;
static constexpr TextureFormat FINAL_RT_FORMAT            = TEXTURE_FORMAT_R16G16B16A16_UNORM;
static constexpr TextureFormat HDR_SCENE_FORMAT           = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Phase 9: state + behaviour for Flux_Graphics subsystem.
class Flux_GraphicsImpl
{
public:
	Flux_GraphicsImpl() = default;
	~Flux_GraphicsImpl() = default;

	Flux_GraphicsImpl(const Flux_GraphicsImpl&) = delete;
	Flux_GraphicsImpl& operator=(const Flux_GraphicsImpl&) = delete;

	struct FrameConstants
	{
		Zenith_Maths::Matrix4 m_xViewMat;
		Zenith_Maths::Matrix4 m_xProjMat;
		Zenith_Maths::Matrix4 m_xViewProjMat;
		Zenith_Maths::Matrix4 m_xInvViewProjMat;
		Zenith_Maths::Matrix4 m_xInvViewMat;
		Zenith_Maths::Matrix4 m_xInvProjMat;
		Zenith_Maths::Vector4 m_xCamPos_Pad;
		Zenith_Maths::Vector4 m_xSunDir_Pad;
		Zenith_Maths::Vector4 m_xSunColour_Pad;
		Zenith_Maths::UVector2 m_xScreenDims;
		Zenith_Maths::Vector2 m_xRcpScreenDims;
#ifdef ZENITH_TOOLS
		u_int m_uQuadUtilisationAnalysis;
		u_int m_uTargetPixelsPerTri;
#else
		u_int m_uPad0;
		u_int m_uPad1;
#endif
		Zenith_Maths::Vector2 m_xCameraNearFar;
	};

	void InitialiseSamplers();
	void Initialise();
	void ReleaseAssetReferences();
	void Shutdown();
	void UploadFrameConstants();

	void SetupTransients(Flux_RenderGraph& xGraph);

	// FluxGraphics creates all the shared cross-feature render-graph transients
	// (G-buffer / depth / final-RT / HDR scene) via SetupTransients. It is registered
	// FIRST, so its SetupRenderGraph runs before any feature declares a pass on those
	// targets — which is why this is real work, not a no-op (it replaced the former
	// @SetupTransients:FluxGraphics raw step).
	void SetupRenderGraph(Flux_RenderGraph& xGraph) { SetupTransients(xGraph); }
	// No-op: FluxGraphics owns no shader programs, so there are no pipelines to build
	// or hot-reload. Present only to satisfy the uniform FluxRenderFeature interface.
	void BuildPipelines() {}

	Flux_RenderAttachment& GetMRTAttachment(MRTIndex eIndex);
	Flux_RenderAttachment& GetDepthAttachment();
	Flux_RenderAttachment& GetFinalRenderTarget();

	// HDR scene target — the shared scene-colour buffer many features write and the
	// HDR tonemap reads. Owned here (alongside the other shared targets) so it exists
	// before the first writer; the HDR feature keeps only its private bloom chain.
	// (Was Flux_HDRImpl's, created by the removed @SetupTransients:HDR step.)
	Flux_RenderAttachment&   GetHDRSceneTarget();
	Flux_ShaderResourceView& GetHDRSceneSRV();
	void GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);
	void GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);

	TextureFormat GetMRTFormat(MRTIndex eIndex);

	const Zenith_Maths::Vector3& GetCameraPosition();

	Flux_ShaderResourceView* GetGBufferSRV(MRTIndex eIndex);
	Flux_ShaderResourceView* GetDepthStencilSRV();
	Flux_RenderTargetView*   GetGBufferRTV(MRTIndex eIndex);
	Flux_DepthStencilView*   GetDepthStencilDSV();

#ifdef ZENITH_TOOLS
	const Flux_ShaderResourceView* GetDebugSRV_MRTDiffuse();
	const Flux_ShaderResourceView* GetDebugSRV_MRTNormalsAO();
	const Flux_ShaderResourceView* GetDebugSRV_MRTMaterial();
	const Flux_ShaderResourceView* GetDebugSRV_Depth();
	const Flux_ShaderResourceView* GetDebugSRV_HDRScene();
#endif

	Zenith_Maths::Matrix4  GetViewProjMatrix();
	Zenith_Maths::Matrix4  GetInvViewProjMatrix();
	Zenith_Maths::Matrix4  GetViewMatrix();
	Zenith_Maths::Vector3  GetSunDir();
	float GetNearPlane();
	float GetFarPlane();
	float GetFOV();
	float GetAspectRatio();

	// Opt-in per-scene directional-sun override (default OFF → games use the global
	// dbg_SunDir/dbg_SunColour unchanged). xColourRadiance.w is the HDR radiance
	// scalar. Lets a scene pose a real cinematic key/fill without changing globals.
	void SetSunOverride(const Zenith_Maths::Vector3& xDir, const Zenith_Maths::Vector4& xColourRadiance)
	{
		m_bSunOverride = true;
		m_xSunDirOverride = xDir;
		m_xSunColourOverride = xColourRadiance;
	}
	void ClearSunOverride() { m_bSunOverride = false; }

	bool BuildCameraMatrices(FrameConstants& xConstants);

	// Samplers (initialised in InitialiseSamplers).
	Flux_Sampler                m_xRepeatSampler;
	Flux_Sampler                m_xClampSampler;

	// Shared geometry / per-frame UBO.
	Flux_MeshGeometry           m_xQuadMesh;
	Flux_DynamicConstantBuffer  m_xFrameConstantsBuffer;

	// Fallback assets.
	TextureHandle               m_xWhiteTexture;
	TextureHandle               m_xBlackTexture;
	TextureHandle               m_xGridTexture;
	Flux_MeshGeometry           m_xBlankMesh;
	MaterialHandle              m_xBlankMaterial;

	// Scene textures.
	TextureHandle               m_xCubemapTexture;
	TextureHandle               m_xWaterNormalTexture;

	// MRT formats.
	TextureFormat               m_aeMRTFormats[MRT_INDEX_COUNT] = {};

	// Per-frame CPU-side constants struct + binding-group layout.
	FrameConstants              m_xFrameConstants;
	// Per-scene sun override (see SetSunOverride). Default OFF.
	bool                        m_bSunOverride = false;
	Zenith_Maths::Vector3       m_xSunDirOverride = { 0.0f, -1.0f, 0.0f };
	Zenith_Maths::Vector4       m_xSunColourOverride = { 1.0f, 1.0f, 1.0f, 1.0f };
	Flux_BindingGroupLayout     m_xFrameConstantsLayout;

	// Graph-owned transient render-target handles + graph back-ref.
	Flux_TransientHandle        m_axMRTHandles[MRT_INDEX_COUNT];
	Flux_TransientHandle        m_xFinalRTHandle;
	Flux_TransientHandle        m_xDepthHandle;
	Flux_TransientHandle        m_xHDRSceneTargetHandle;
	Flux_RenderGraph*           m_pxGraph = nullptr;
};
