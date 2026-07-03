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
static constexpr TextureFormat MRT_FORMAT_VELOCITY       = TEXTURE_FORMAT_R16G16_SFLOAT;			// TAA motion vectors (uvCurrent - uvPrev, UV space); optional 5th MRT
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

	// --- Temporal upscaling (Stage 5) — render/output resolution split ------------
	// GetOutputDims() = the swapchain/window resolution (what Present, the FinalRT and the
	// UI/text overlay use — never changed by upscaling). GetRenderDims() = the resolution the
	// MAIN-view scene chain (G-buffer/depth/HDR + HiZ/SSAO/SSR/SSGI/Decals/histogram) renders
	// at; it EQUALS GetOutputDims() unless temporal upscaling is active (TAA on + Upscaling on
	// + RenderScale<1), when it is the even-quantised round_to_even(RenderScale * output)
	// latched at the last graph build (Flux_TAAComputeRenderDims). Returning the LATCHED value
	// (not the live scale) keeps every per-frame caller consistent with the current graph's
	// transient sizes. OFF path returns GetOutputDims() verbatim so it can never perturb the
	// byte-identical-off gate.
	Zenith_Maths::UVector2 GetOutputDims() const;
	Zenith_Maths::UVector2 GetRenderDims() const;
	// The render dims the CURRENT frame's graph WILL be (re)built at — derived from the *requested*
	// upscaling state, i.e. what SetupTransients is about to latch. UploadFrameConstants stages the
	// slot-0 VIEW CB (screen dims + jitter) BEFORE the rebuild's SetupTransients runs, so it must use
	// THIS (not the latched GetRenderDims(), which still holds the previous build's dims) — otherwise
	// the slot-0 CB lags the freshly-resized transients by one frame on an upscaling/scale toggle
	// (decals/translucency/SSR-denoise map SV_Position×g_xRcpScreenDims → a one-frame edge glitch).
	// The poll that updates *Requested runs AFTER UploadFrameConstants and AFTER SetupTransients in
	// the same frame, so requested-now == what SetupTransients latches. Equals GetRenderDims() on
	// every non-transition frame; equals GetOutputDims() whenever upscaling is (pending-)off.
	Zenith_Maths::UVector2 GetPendingRenderDims() const;
	u_int GetRenderWidth()  const { return GetRenderDims().x; }
	u_int GetRenderHeight() const { return GetRenderDims().y; }
	bool  IsUpscalingActive() const { return m_bUpscalingActive; }
	float GetRenderScale()    const { return m_fRenderScaleActive; }

	// TAA velocity MRT (optional 5th G-buffer target). Materialised only for the MAIN
	// view, only while the velocity latch is on. Reached via this DISTINCT accessor —
	// GetMRTAttachment asserts core-only so velocity can never leak through the normal
	// G-buffer getters. IsVelocityMRTActive() is the single latched flag both the pass
	// declarations and the record-time pipeline selection read.
	Flux_RenderAttachment& GetVelocityAttachment(u_int uViewSlot = kuFluxViewSlotMain);
	bool IsVelocityMRTActive() const { return m_bVelocityMRTActive; }
	// Polled each frame (ApplySubsystemGraphSelections): flips velocity 4↔5-MRT and drives
	// the Stage-2 jitter enable in lockstep. Returns TRUE when the requested state diverged
	// from the latched one — the caller then issues the full graph rebuild (the MRT count
	// changes). Returning the signal keeps the renderer reach out of this graphics method.
	bool UpdateVelocityTargetSelection(Flux_RenderGraph& xGraph);

	// Runtime TAA-enable override for automation/tests (TAAToggleStress). -1 = no override
	// (the debug var / --taa CLI decide — byte-identical default); 0/1 = force the requested
	// state, applied LAST in UpdateVelocityTargetSelection so a test can flip TAA mid-run (and
	// trigger the graph rebuild) without the ImGui panel. g_xEngine.TAA().SetEnabled(bool) forwards here.
	void SetTAAEnableOverride(int iState) { m_iTAARuntimeEnableOverride = iState; }

	// HDR scene target — the shared scene-colour buffer many features write and the
	// HDR tonemap reads. Owned here (alongside the other shared targets) so it exists
	// before the first writer; the HDR feature keeps only its private bloom chain.
	// (Was Flux_HDRImpl's, created by the removed @SetupTransients:HDR step.)
	Flux_RenderAttachment&   GetHDRSceneTarget(u_int uViewSlot = kuFluxViewSlotMain);
	// The scene colour the HDR post-FX (bloom threshold + main tonemap) should read.
	// Returns the TAA-resolved output when TAA is active on the MAIN view, else the raw
	// HDR scene. Histogram/auto-exposure + the material-preview tonemap deliberately keep
	// reading GetHDRSceneTarget (raw). TAA-off => identical to GetHDRSceneTarget
	// everywhere (the byte-identical pre-TAA path).
	Flux_RenderAttachment&   GetSceneColourForPostFX(u_int uViewSlot = kuFluxViewSlotMain);
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

	// --- TAA sub-pixel jitter state (Stage 2 plumbing; the {"Render","TAA"} toggle
	// wires m_bTAAJitterEnabled in Stage 3). Jitter is injected ONLY into the slot-0
	// GPU CB payload (m_xViewConstantsData) — m_xFrameConstants stays unjittered so
	// culling / CSM / preview / terrain streaming are jitter-free by construction.
	// While disabled the offset is (0,0), which Flux_ApplyJitterToProjection returns
	// byte-for-byte, so the whole GPU payload is byte-identical to the no-TAA frame.
	// m_xPrevFrameViewProjNoJitter / m_xPrevJitterUV feed the velocity reprojection's
	// previous-frame terms (staged into the slot-0 payload each frame).
	bool                        m_bTAAJitterEnabled          = false;
	u_int                       m_uTAAJitterPhase            = 0u;
	u_int                       m_uTAAJitterPhaseCount       = 8u;
	Zenith_Maths::Matrix4       m_xPrevFrameViewProjNoJitter = Zenith_Maths::Matrix4(1.0f);
	Zenith_Maths::Vector2       m_xPrevJitterUV              = Zenith_Maths::Vector2(0.0f);

	// --- TAA velocity MRT toggle/latch (Stage 3). m_bVelocityMRTRequested is set by the
	// per-frame poll; SetupTransients latches it into m_bVelocityMRTActive at the start of
	// each graph build, and EVERYTHING else in that build (velocity transient creation, the
	// geometry pass .Writes, the record-time pipeline selection) reads m_bVelocityMRTActive —
	// so they can never disagree within a frame. Default OFF ⇒ 4-MRT frame, byte-identical.
	bool                        m_bVelocityMRTRequested      = false;
	bool                        m_bVelocityMRTActive         = false;
	int                         m_iTAARuntimeEnableOverride  = -1;   // -1=none, 0/1=forced (test/automation hook)

	// --- TAA temporal upscaling toggle/latch (Stage 5). *Requested is set by the per-frame
	// poll; SetupTransients latches Active + the even-quantised m_xRenderDimsThisBuild at the
	// start of each graph build (parallel to the velocity latch), so every consumer this build
	// agrees on the render dims. m_bUpscalingActive is gated on m_bVelocityMRTActive: upscaling
	// can NEVER engage while TAA is off (there is no resolve pass to reconstruct output res, so
	// the scene must render at output res). Default OFF / scale 1.0 ⇒ render == output ⇒
	// byte-identical. Both are STRUCTURAL (they resize the slot-0 scene transients) so a change
	// requests a full graph rebuild, not a MarkDirty. m_xRenderDimsThisBuild is read by
	// GetRenderDims() only while active (it starts (0,0) but is set before any consumer runs).
	bool                        m_bUpscalingRequested        = false;
	bool                        m_bUpscalingActive           = false;
	float                       m_fRenderScaleRequested      = 1.0f;
	float                       m_fRenderScaleActive         = 1.0f;
	Zenith_Maths::UVector2      m_xRenderDimsThisBuild       = Zenith_Maths::UVector2(0u, 0u);
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
