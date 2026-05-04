#pragma once
#include "Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "AssetHandling/Zenith_AssetHandle.h"

// ---- Core render-target format constants --------------------------------
// Pipeline building needs these at init time (before transients exist).
// Extracted from Flux_Graphics::InitialiseRenderTargets().
static constexpr TextureFormat MRT_FORMAT_DIFFUSE         = TEXTURE_FORMAT_RGBA8_UNORM;
static constexpr TextureFormat MRT_FORMAT_NORMALSAMBIENT  = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
static constexpr TextureFormat MRT_FORMAT_MATERIAL        = TEXTURE_FORMAT_RGBA8_UNORM;
static constexpr TextureFormat DEPTH_FORMAT               = TEXTURE_FORMAT_D32_SFLOAT;
static constexpr TextureFormat FINAL_RT_FORMAT            = TEXTURE_FORMAT_R16G16B16A16_UNORM;
static constexpr TextureFormat HDR_SCENE_FORMAT           = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

class Flux_Graphics
{
public:
	Flux_Graphics() = delete;
	~Flux_Graphics() = delete;

	static void InitialiseSamplers();
	static void Initialise();

	// Drop refs to all Flux_Graphics-owned asset handles (default textures, blank
	// material, grid texture). Called from Flux::ReleaseAssetReferences before the
	// asset registry shuts down. Do NOT free GPU resources here — that's Shutdown().
	static void ReleaseAssetReferences();

	static void Shutdown();

	static void UploadFrameConstants();

	//----------------------------------------------------------------------
	// Transient resource setup (called from Flux::SetupRenderGraph BEFORE
	// any other subsystem registers passes).
	//----------------------------------------------------------------------
	static void SetupTransients(Flux_RenderGraph& xGraph);

	//----------------------------------------------------------------------
	// Render Target Accessors (prefer these over direct member access)
	//----------------------------------------------------------------------
	static Flux_RenderAttachment& GetMRTAttachment(MRTIndex eIndex);
	static Flux_RenderAttachment& GetDepthAttachment();
	static Flux_RenderAttachment& GetFinalRenderTarget();

	//----------------------------------------------------------------------
	// Samplers
	//----------------------------------------------------------------------
	static Flux_Sampler s_xRepeatSampler;
	static Flux_Sampler s_xClampSampler;

	//----------------------------------------------------------------------
	// Shared Geometry & Buffers
	//----------------------------------------------------------------------
	static Flux_MeshGeometry s_xQuadMesh;
	static Flux_DynamicConstantBuffer s_xFrameConstantsBuffer;

	//----------------------------------------------------------------------
	// Fallback Assets (engine defaults for missing/loading assets).
	// Stored as handles so they hold a permanent ref while initialised — without
	// that, Zenith_AssetRegistry::UnloadUnused() would free them out from under
	// every consumer that uses them as a fallback.
	//----------------------------------------------------------------------
	static TextureHandle s_xWhiteTexture;
	static TextureHandle s_xBlackTexture;
	static TextureHandle s_xGridTexture;
	static Flux_MeshGeometry s_xBlankMesh;
	static MaterialHandle s_xBlankMaterial;

	//----------------------------------------------------------------------
	// Scene Textures (set during initialization in Zenith_Main.cpp)
	//----------------------------------------------------------------------
	static TextureHandle s_xCubemapTexture;
	static TextureHandle s_xWaterNormalTexture;

	static TextureFormat GetMRTFormat(MRTIndex eIndex);
	static TextureFormat s_aeMRTFormats[MRT_INDEX_COUNT];

	static const Zenith_Maths::Vector3& GetCameraPosition();
	
	// View accessors for resource binding
	static Flux_ShaderResourceView* GetGBufferSRV(MRTIndex eIndex);
	static Flux_ShaderResourceView* GetDepthStencilSRV();
	static Flux_RenderTargetView* GetGBufferRTV(MRTIndex eIndex);
	static Flux_DepthStencilView* GetDepthStencilDSV();

#ifdef ZENITH_TOOLS
	// Debug-variable callbacks — return the live SRV of each G-buffer/depth
	// transient at ImGui display time. Used via Zenith_DebugVariables::AddTextureCallback
	// so the editor's texture preview survives render-graph rebuilds that
	// invalidate previously-captured SRV pointers. Returns nullptr pre-SetupTransients.
	static const Flux_ShaderResourceView* GetDebugSRV_MRTDiffuse();
	static const Flux_ShaderResourceView* GetDebugSRV_MRTNormalsAO();
	static const Flux_ShaderResourceView* GetDebugSRV_MRTMaterial();
	static const Flux_ShaderResourceView* GetDebugSRV_Depth();
#endif

	static Zenith_Maths::Matrix4 GetViewProjMatrix() { return s_xFrameConstants.m_xViewProjMat; }
	static Zenith_Maths::Matrix4 GetInvViewProjMatrix() { return s_xFrameConstants.m_xInvViewProjMat; }
	static Zenith_Maths::Matrix4 GetViewMatrix() { return s_xFrameConstants.m_xViewMat; }
	static Zenith_Maths::Vector3 GetSunDir() { return s_xFrameConstants.m_xSunDir_Pad; }
	static float GetNearPlane();
	static float GetFarPlane();
	static float GetFOV();
	static float GetAspectRatio();

	static Flux_BindingGroupLayout s_xFrameConstantsLayout;

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
		Zenith_Maths::Vector2 m_xCameraNearFar;  // x = near plane, y = far plane
	};
	static FrameConstants s_xFrameConstants;
private:
	static bool BuildCameraMatrices(FrameConstants& xConstants);

	// Graph-owned transient handles — backing Flux_RenderAttachments are
	// allocated and destroyed by the render graph, sized from the descriptors
	// set in SetupTransients. Access the resolved attachments via the
	// Get*Attachment / GetFinalRenderTarget* getters above.
	static Flux_TransientHandle s_axMRTHandles[MRT_INDEX_COUNT];
	static Flux_TransientHandle s_xFinalRTHandle;
	static Flux_TransientHandle s_xDepthHandle;
	static Flux_RenderGraph* s_pxGraph;
};