#pragma once

#include "Flux/Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "AssetHandling/Zenith_AssetHandle.h"

// Cross-subsystem dependencies injected into Initialise (aggressive DI pass):
// engine-infra + Shadows reaches that used to be g_xEngine.X() lookups inside
// instance methods are now passed in and stored as member pointers. Forward-
// declared here; full headers are pulled in by Flux_Graphics.cpp.
class Zenith_Vulkan_MemoryManager;
class Zenith_Vulkan_Swapchain;
class Flux_ShadowsImpl;

// ---- Core render-target format constants --------------------------------
static constexpr TextureFormat MRT_FORMAT_DIFFUSE         = TEXTURE_FORMAT_RGBA8_UNORM;
static constexpr TextureFormat MRT_FORMAT_NORMALSAMBIENT  = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
static constexpr TextureFormat MRT_FORMAT_MATERIAL        = TEXTURE_FORMAT_RGBA8_UNORM;
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
	void Initialise(Zenith_Vulkan_MemoryManager& xVulkanMemory, Zenith_Vulkan_Swapchain& xVulkanSwapchain, Flux_ShadowsImpl& xShadows);
	void ReleaseAssetReferences();
	void Shutdown();
	void UploadFrameConstants();

	void SetupTransients(Flux_RenderGraph& xGraph);

	Flux_RenderAttachment& GetMRTAttachment(MRTIndex eIndex);
	Flux_RenderAttachment& GetDepthAttachment();
	Flux_RenderAttachment& GetFinalRenderTarget();

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
#endif

	Zenith_Maths::Matrix4  GetViewProjMatrix();
	Zenith_Maths::Matrix4  GetInvViewProjMatrix();
	Zenith_Maths::Matrix4  GetViewMatrix();
	Zenith_Maths::Vector3  GetSunDir();
	float GetNearPlane();
	float GetFarPlane();
	float GetFOV();
	float GetAspectRatio();

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
	Flux_BindingGroupLayout     m_xFrameConstantsLayout;

	// Graph-owned transient render-target handles + graph back-ref.
	Flux_TransientHandle        m_axMRTHandles[MRT_INDEX_COUNT];
	Flux_TransientHandle        m_xFinalRTHandle;
	Flux_TransientHandle        m_xDepthHandle;
	Flux_RenderGraph*           m_pxGraph = nullptr;

	// Injected cross-subsystem deps (set in Initialise, nulled in Shutdown).
	Zenith_Vulkan_MemoryManager* m_pxVulkanMemory = nullptr;
	Zenith_Vulkan_Swapchain*     m_pxVulkanSwapchain = nullptr;
	Flux_ShadowsImpl*            m_pxShadows = nullptr;
};
