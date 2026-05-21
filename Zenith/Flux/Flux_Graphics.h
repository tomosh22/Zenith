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

	// Phase 6a-2: the 17 file-static data members previously declared
	// here moved onto Flux_GraphicsImpl (held by Zenith_Engine). The
	// static facade methods below keep their signatures; bodies route
	// through g_xEngine.FluxGraphics().m_xXxx.

	static TextureFormat GetMRTFormat(MRTIndex eIndex);

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

	static Zenith_Maths::Matrix4 GetViewProjMatrix();
	static Zenith_Maths::Matrix4 GetInvViewProjMatrix();
	static Zenith_Maths::Matrix4 GetViewMatrix();
	static Zenith_Maths::Vector3 GetSunDir();
	static float GetNearPlane();
	static float GetFarPlane();
	static float GetFOV();
	static float GetAspectRatio();

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
private:
	static bool BuildCameraMatrices(FrameConstants& xConstants);
};