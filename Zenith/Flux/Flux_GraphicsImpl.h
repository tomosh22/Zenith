#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_BindlessAllocator.h"
#include "Flux/Flux_MaterialTable.h"
#include "Flux/Flux_ViewConstants.h"
#include "Flux/RenderViews/Flux_RenderViews.h"
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

	// The spine constant payloads + the CPU camera struct live in
	// Flux/Flux_ViewConstants.h (extracted so the render-view registry can carry
	// a per-view ViewConstants payload). The nested names below are aliases so
	// existing Flux_GraphicsImpl::FrameConstants/... references compile unchanged.
	using FrameConstants  = Flux_FrameConstants;
	using GlobalConstants = Flux_GlobalConstants;
	using ViewConstants   = Flux_ViewConstants;

	void InitialiseSamplers();
	void Initialise();
	void ReleaseAssetReferences();
	void Shutdown();
	void UploadFrameConstants();

	// Phase 5.1: the spine GLOBAL/VIEW constant-buffer descriptor handles for THIS frame,
	// written into the persistent descriptor sets each frame (Zenith_Vulkan::Prepare-
	// PersistentSets). GetCBV() resolves the current frame-in-flight buffer. VIEW is
	// per-render-view (one frame-indexed CB per fixed view slot); slot 0 = main camera.
	Flux_BufferDescriptorHandle GetGlobalConstantsBufferHandle() const { return m_xGlobalConstantsBuffer.GetCBV().m_xBufferDescHandle; }
	Flux_BufferDescriptorHandle GetViewConstantsBufferHandle(u_int uViewSlot = kuFluxViewSlotMain) const
	{
		Zenith_Assert(uViewSlot < FLUX_MAX_RENDER_VIEWS, "GetViewConstantsBufferHandle: view slot %u out of range", uViewSlot);
		return m_axViewConstantsBuffers[uViewSlot].GetCBV().m_xBufferDescHandle;
	}

	// The render-view registry (fixed slots: 0 main / 1..N cascades / preview).
	// Owners stage each view's ViewConstants + frustum planes here per frame;
	// UploadRenderViewConstants copies every ACTIVE non-main slot's payload into
	// its frame-indexed CB (slot 0 is uploaded by UploadFrameConstants).
	Flux_RenderViewRegistry&       RenderViews()       { return m_xRenderViews; }
	const Flux_RenderViewRegistry& RenderViews() const { return m_xRenderViews; }
	void UploadRenderViewConstants();

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

	// Fullscreen-quad pass prologue shared by the screen-space effects: bind the given
	// pipeline + the shared fullscreen quad's vertex/index buffers. The caller then binds
	// its per-effect SRVs/constants and issues DrawIndexed(6). Centralises the m_xQuadMesh
	// fetch + the "6 indices" quad knowledge so each Execute callback reads as just its
	// pipeline + binds. (Deliberately on FluxGraphics — which owns m_xQuadMesh — not on the
	// SSR/SSGI CRTP base, so non-CRTP effects like SSAO can use it too.)
	void BindFullscreenQuad(Flux_CommandBuffer& xCmd, Flux_Pipeline& xPipeline);

	// Per-view target accessors: each active FULL-PIPELINE view (main + preview)
	// owns its own G-buffer/depth/HDR transients at its own dims. uViewSlot
	// defaults to the main camera so single-view callers stay unchanged; per-view
	// pass record callbacks pass Flux_RenderGraph::GetCurrentRecordingPassViewSlot().
	Flux_RenderAttachment& GetMRTAttachment(MRTIndex eIndex, u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetDepthAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderAttachment& GetFinalRenderTarget();

	// HDR scene target — the shared scene-colour buffer many features write and the
	// HDR tonemap reads. Owned here (alongside the other shared targets) so it exists
	// before the first writer; the HDR feature keeps only its private bloom chain.
	// (Was Flux_HDRImpl's, created by the removed @SetupTransients:HDR step.)
	Flux_RenderAttachment&   GetHDRSceneTarget(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_ShaderResourceView& GetHDRSceneSRV();
	void GetHDRSceneTargetSetup(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);
	void GetHDRSceneTargetSetupWithDepth(Flux_RenderAttachment* apxColourAttachments[], uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);

	TextureFormat GetMRTFormat(MRTIndex eIndex);

	const Zenith_Maths::Vector3& GetCameraPosition();

	Flux_ShaderResourceView* GetGBufferSRV(MRTIndex eIndex, u_int uViewSlot = kuFluxViewSlotMain);
	Flux_ShaderResourceView* GetDepthStencilSRV(u_int uViewSlot = kuFluxViewSlotMain);
	Flux_RenderTargetView*   GetGBufferRTV(MRTIndex eIndex);
	Flux_DepthStencilView*   GetDepthStencilDSV();

	// The material-preview view's persistent LDR output (512², RGBA8). Persistent —
	// NOT a transient — so the SRV handed to ImGui stays valid across graph
	// rebuilds. Written by the HDR feature's preview tonemap pass.
	Flux_RenderAttachment& GetPreviewLDR() { return m_xPreviewLDR; }

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

	// True iff UploadFrameConstants resolved a valid main camera this frame (else
	// m_xViewProjMat is stale/identity). The scene-graph snapshot reads this so it can skip
	// frustum culling against a bogus matrix until the camera resolves (e.g. first boot frame
	// in a non-tools/Playing build before the camera entity exists).
	bool IsCameraValid() const { return m_bCameraValid; }
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

	// Dense allocator for bindless-table slots (set 2, g_axTextures[]). Initialised
	// in Initialise() from the backend's clamped table size; advanced once per frame.
	Flux_BindlessAllocator& BindlessAllocator() { return m_xBindlessAllocator; }

	// GPU material record store (g_axMaterials, GLOBAL set). Registered per-frame from
	// each material subsystem's gather; uploaded once at RecordFrame.
	Flux_MaterialTable& MaterialTable() { return m_xMaterialTable; }

	// Samplers (initialised in InitialiseSamplers).
	Flux_Sampler                m_xRepeatSampler;
	Flux_Sampler                m_xClampSampler;
	Flux_BindlessAllocator      m_xBindlessAllocator;
	Flux_MaterialTable          m_xMaterialTable;
	// NEAREST/no-aniso/clamp — for data textures that must be read per-texel
	// exactly (VAT position textures). Bound explicitly via BindSRV's sampler arg.
	Flux_Sampler                m_xPointSampler;

	// Shared geometry / per-frame UBO.
	Flux_MeshGeometry           m_xQuadMesh;
	// GLOBAL (set 0) + VIEW (set 1) spine constant buffers — the only frame-constant
	// buffers the GPU sees. Filled each frame from the CPU m_xFrameConstants (see
	// UploadFrameConstants); bound by every spine shader. (The former
	// m_xFrameConstantsBuffer GPU upload was deleted with Common/Frame.slang — no
	// shader bound it. The CPU m_xFrameConstants struct stays as the camera-matrix
	// source for CPU systems + the mirror source for these two.)
	Flux_DynamicConstantBuffer  m_xGlobalConstantsBuffer;
	// One VIEW CB per fixed render-view slot (all allocated up front — tiny).
	// Slot 0 (main) is filled by UploadFrameConstants; other ACTIVE slots by
	// UploadRenderViewConstants from the registry payloads. Every slot's CB is
	// written into its persistent VIEW descriptor set each frame, so inactive
	// slots still carry a VALID (identity-initialised) buffer descriptor.
	Flux_DynamicConstantBuffer  m_axViewConstantsBuffers[FLUX_MAX_RENDER_VIEWS];

	// The render-view registry (see RenderViews() accessor).
	Flux_RenderViewRegistry     m_xRenderViews;

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
	// CPU-side spine constants (subsets of m_xFrameConstants), uploaded to the
	// GLOBAL/VIEW buffers each frame.
	GlobalConstants             m_xGlobalConstantsData;
	ViewConstants               m_xViewConstantsData;
	// Set each UploadFrameConstants to whether a valid main camera was resolved this frame.
	bool                        m_bCameraValid = false;
	// Per-scene sun override (see SetSunOverride). Default OFF.
	bool                        m_bSunOverride = false;
	Zenith_Maths::Vector3       m_xSunDirOverride = { 0.0f, -1.0f, 0.0f };
	Zenith_Maths::Vector4       m_xSunColourOverride = { 1.0f, 1.0f, 1.0f, 1.0f };
	Flux_BindingGroupLayout     m_xFrameConstantsLayout;

	// Graph-owned transient render-target handles + graph back-ref. G-buffer/
	// depth/HDR are PER FULL-PIPELINE VIEW SLOT (created in SetupTransients only
	// for slots active this compile — slot 0 always, at swapchain dims; the
	// preview slot at its own dims when its view is active). Final RT is
	// main-view-only (the preview tonemaps into the persistent m_xPreviewLDR).
	Flux_TransientHandle        m_aaxMRTHandles[FLUX_MAX_RENDER_VIEWS][MRT_INDEX_COUNT];
	Flux_TransientHandle        m_xFinalRTHandle;
	Flux_TransientHandle        m_axDepthHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_TransientHandle        m_axHDRSceneTargetHandles[FLUX_MAX_RENDER_VIEWS];
	Flux_RenderGraph*           m_pxGraph = nullptr;

	// Persistent preview LDR output (see GetPreviewLDR).
	Flux_RenderAttachment       m_xPreviewLDR;
};
