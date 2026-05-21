#include "Zenith.h"

#include "Flux/Decals/Flux_Decals.h"
#include "Flux/Flux_Graphics.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/Slang/Flux_ShaderBinder.h"
#include "Flux/Primitives/Flux_Primitives.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#ifdef ZENITH_TOOLS
#include "Flux/Slang/Flux_ShaderHotReload.h"
#endif

#include <algorithm>
#include <cmath>
#include <cstring>

// =====================================================================
// Flux_Decals — deferred screen-space box decal renderer.
//
// See Flux_Decals.h and Decals/CLAUDE.md for the full design rationale.
// Two-pass: NormalsCopy clones the live normalsAmbient MRT into a transient
// (so Apply can read pre-decal normals while writing the live MRT under
// blend), Apply rasterizes one cube per active decal and writes blended
// values into all three G-buffer MRTs. Pre-existing G-buffer alpha
// channels (AO on MRT1, unused on MRT2) are preserved via per-attachment
// color write masks.
// =====================================================================

// ===== INSTANCE STRUCT (mirrors Flux_Decals_Apply.slang) =====

// 160 bytes. xyz/w packing keeps std430 alignment clean. Reserved slots
// are intentional — adding fields without breaking the size assert is a
// readability win when iterating.
struct DecalInstance
{
	Zenith_Maths::Matrix4 m_xWorld;          // 64 — unit-cube-local -> world (T * R * S)
	Zenith_Maths::Matrix4 m_xWorldInverse;   // 64 — world -> unit-cube-local
	// xyz = unit-length projection axis (surface normal at hit); w = fade opacity.
	// Packed into one float4 for std430 alignment, not because the two are one concept.
	Zenith_Maths::Vector4 m_xAxisOpacity;    // 16
	Zenith_Maths::Vector4 m_xParams;         // 16 — x = normal-alignment threshold; yzw reserved
};
static_assert(sizeof(DecalInstance) == 160, "DecalInstance must match Flux_Decals_Apply.slang");

// ===== STATIC STATE =====

bool Flux_Decals::s_bInitialised = false;

// CPU-side pool. Ring buffer for slot recycling — m_uNextSlot tracks the
// next slot to overwrite when SpawnDecal is called.
struct CpuDecalSlot
{
	bool                  m_bActive            = false;
	Zenith_Maths::Vector3 m_xPosition;
	Zenith_Maths::Vector3 m_xNormal;           // unit-length projection axis
	float                 m_fSize              = 0.0f;
	float                 m_fInitialLifetime   = 0.0f;
	float                 m_fRemainingLifetime = 0.0f;
	DecalInstance         m_xInstance{};
};
static CpuDecalSlot s_axDecalSlots[Flux_Decals::uMAX_DECALS];
static u_int        s_uNextSlot          = 0;
static u_int        s_uActiveDecalCount  = 0;

// Dense GPU staging — packed at upload time so SV_InstanceID indexes
// 0..uActiveDecalCount-1 contiguously regardless of CPU ring layout.
static DecalInstance s_axDecalStaging[Flux_Decals::uMAX_DECALS];

// Default tuning. The normal-alignment threshold gates leakage onto
// perpendicular surfaces inside the decal volume — 0.3 ≈ 70°, beyond
// which the decal would smear too much anyway.
static constexpr float k_fDefaultNormalThreshold = 0.3f;

// Anisotropic scale: visible XY = fSize, projection-axis depth thinner
// to limit leakage onto nearby parallel surfaces.
static constexpr float k_fDecalDepthFraction = 0.25f;
static constexpr float k_fDecalDepthCap      = 0.04f;

// G-buffer normalsAmbient is RGBA16F; clone format must match.
static constexpr TextureFormat k_eNormalsCopyFormat = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;

// Debug: render a wireframe sphere at every active decal's hit point.
// Off by default; toggle live via Render/Decals/Debug Spheres.
DEBUGVAR bool dbg_bDecalDebugSpheres = false;

// Render-graph state.
static Flux_RenderGraph*    s_pxGraph = nullptr;
static Flux_TransientHandle s_xNormalsCopyHandle;
static Flux_PassHandle      s_xNormalsCopyPass;
static Flux_PassHandle      s_xApplyPass;

// Pipelines and shaders.
static Flux_Shader   s_xNormalsCopyShader;
static Flux_Shader   s_xApplyShader;
static Flux_Pipeline s_xNormalsCopyPipeline;
static Flux_Pipeline s_xApplyPipeline;

// GPU buffers. The decal SRV is bound at Record time (not via the graph)
// because frame-indexed dynamic buffers in this renderer are deliberately
// untracked — same pattern as Flux_DynamicLights' light buffer.
static Flux_DynamicReadWriteBuffer s_xDecalBuffer;

// Decal-pass index buffer holding 36 indices (0..35). The Apply VS uses
// SV_VertexID to look up cube corners from a static const table, but
// Flux exposes only indexed-instanced draws so the IB exists only to
// satisfy DrawIndexed. Its values are ignored by the shader.
static Flux_IndexBuffer s_xDecalIndexBuffer;

// ===== HELPERS =====

static void BuildDecalBasis(const Zenith_Maths::Vector3& xNormal,
                            Zenith_Maths::Vector3& xTangent,
                            Zenith_Maths::Vector3& xBitangent)
{
	// Pick a stable up reference that isn't parallel to the normal.
	Zenith_Maths::Vector3 xUp = (fabsf(xNormal.y) < 0.9f)
		? Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f)
		: Zenith_Maths::Vector3(1.0f, 0.0f, 0.0f);

	xTangent   = glm::normalize(glm::cross(xUp, xNormal));
	xBitangent = glm::cross(xNormal, xTangent);
}

static void BuildDecalInstance(const Zenith_Maths::Vector3& xPosition,
                               const Zenith_Maths::Vector3& xNormal,
                               float                        fSize,
                               float                        fOpacity,
                               DecalInstance&               xOut)
{
	Zenith_Maths::Vector3 xTangent;
	Zenith_Maths::Vector3 xBitangent;
	BuildDecalBasis(xNormal, xTangent, xBitangent);

	const float fDepth = std::min(fSize * k_fDecalDepthFraction, k_fDecalDepthCap);

	// Translate * Rotate * Scale. Engine convention; matches
	// Zenith_TransformComponent::BuildModelMatrix.
	Zenith_Maths::Matrix4 xT = glm::translate(Zenith_Maths::Matrix4(1.0f), xPosition);
	Zenith_Maths::Matrix4 xR(1.0f);
	xR[0] = Zenith_Maths::Vector4(xTangent,   0.0f);
	xR[1] = Zenith_Maths::Vector4(xBitangent, 0.0f);
	xR[2] = Zenith_Maths::Vector4(xNormal,    0.0f);
	xR[3] = Zenith_Maths::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
	Zenith_Maths::Matrix4 xS = glm::scale(Zenith_Maths::Matrix4(1.0f),
		Zenith_Maths::Vector3(fSize, fSize, fDepth));

	xOut.m_xWorld        = xT * xR * xS;
	xOut.m_xWorldInverse = glm::inverse(xOut.m_xWorld);
	xOut.m_xAxisOpacity  = Zenith_Maths::Vector4(xNormal.x, xNormal.y, xNormal.z, fOpacity);
	xOut.m_xParams       = Zenith_Maths::Vector4(k_fDefaultNormalThreshold, 0.0f, 0.0f, 0.0f);
}

// Build the dense GPU staging array from active CPU slots, ticking
// per-slot lifetimes by fDt and deactivating expired slots. Returns the
// number of active decals after the tick. Also queues a debug sphere
// per active decal via Flux_Primitives — primitives are cleared each
// frame, so re-queuing per-frame is the right pattern.
static u_int TickAndPackDense(float fDt)
{
	u_int uActive = 0;
	for (u_int u = 0; u < Flux_Decals::uMAX_DECALS; ++u)
	{
		CpuDecalSlot& xSlot = s_axDecalSlots[u];
		if (!xSlot.m_bActive)
			continue;

		xSlot.m_fRemainingLifetime -= fDt;
		if (xSlot.m_fRemainingLifetime <= 0.0f)
		{
			xSlot.m_bActive = false;
			continue;
		}

		// Fade opacity over the last 10% of lifetime.
		const float fFadeWindow = std::max(0.001f, xSlot.m_fInitialLifetime * 0.1f);
		const float fFadeT      = std::min(1.0f, xSlot.m_fRemainingLifetime / fFadeWindow);
		xSlot.m_xInstance.m_xAxisOpacity.w = fFadeT;

		// Debug sphere bracketing the decal — bright yellow at the decal's
		// XY half-extent so it's obvious where each decal landed.
		// Gated by Render/Decals/Debug Spheres so it stays off in normal play.
#ifdef ZENITH_DEBUG_VARIABLES
		if (dbg_bDecalDebugSpheres)
		{
			Flux_Primitives::AddSphere(
				xSlot.m_xPosition,
				xSlot.m_fSize * 0.5f,
				Zenith_Maths::Vector3(1.0f, 1.0f, 0.0f));
		}
#endif

		s_axDecalStaging[uActive] = xSlot.m_xInstance;
		++uActive;
	}
	return uActive;
}

// ===== INDEX BUFFER (36 indices, 0..35) =====

static void InitialiseDecalIndexBuffer()
{
	// Flux's command-buffer backend binds index buffers as uint32
	// (Zenith_Vulkan_CommandBuffer::SetIndexBuffer uses vk::IndexType::eUint32),
	// so 16-bit indices would trip the validator with a "size×(firstIndex+
	// indexCount) > buffer size" diagnostic.
	uint32_t auIndices[36];
	for (uint32_t u = 0; u < 36; ++u)
		auIndices[u] = u;
	Flux_MemoryManager::InitialiseIndexBuffer(auIndices, sizeof(auIndices), s_xDecalIndexBuffer);
}

// ===== PIPELINES =====

void Flux_Decals::BuildPipelines()
{
	// NormalsCopy — vanilla fullscreen-quad pass writing the transient.
	Flux_PipelineHelper::BuildFullscreenPipeline(
		s_xNormalsCopyShader,
		s_xNormalsCopyPipeline,
		FluxShaderProgram::Decals_NormalsCopy,
		k_eNormalsCopyFormat);

	// Apply — instanced cube into 3 G-buffer MRTs with per-attachment
	// alpha blend and color write mask.
	{
		s_xApplyShader.Initialise(FluxShaderProgram::Decals_Apply);

		Flux_PipelineSpecification xSpec;
		xSpec.m_pxShader = &s_xApplyShader;

		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_DIFFUSE]        = TEXTURE_FORMAT_RGBA8_UNORM;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_NORMALSAMBIENT] = TEXTURE_FORMAT_R16G16B16A16_SFLOAT;
		xSpec.m_aeColourAttachmentFormats[MRT_INDEX_MATERIAL]       = TEXTURE_FORMAT_RGBA8_UNORM;
		xSpec.m_uNumColourAttachments = 3;

		xSpec.m_eDepthStencilFormat = TEXTURE_FORMAT_NONE;
		xSpec.m_bDepthTestEnabled   = false;
		xSpec.m_bDepthWriteEnabled  = false;
		xSpec.m_eDepthCompareFunc   = DEPTH_COMPARE_FUNC_ALWAYS;

		// Cull FRONT — render the cube's back faces only. Guarantees
		// fragment coverage when the camera sits inside the decal volume;
		// back-face culling would clip the visible front faces away.
		xSpec.m_eCullMode = CULL_MODE_FRONT;

		// Per-attachment blend: alpha-blend RGB into each MRT, leave alpha
		// untouched so the AO and unused-alpha channels survive. Belt-and-
		// suspenders: SrcA=ZERO+DstA=ONE makes the alpha math a no-op AND
		// the write mask drops the alpha bit on MRTs 1 and 2.
		for (u_int u = 0; u < 3; ++u)
		{
			Flux_BlendState& xBlend = xSpec.m_axBlendStates[u];
			xBlend.m_bBlendEnabled        = true;
			xBlend.m_eSrcBlendFactor      = BLEND_FACTOR_SRCALPHA;
			xBlend.m_eDstBlendFactor      = BLEND_FACTOR_ONEMINUSSRCALPHA;
			xBlend.m_eSrcAlphaBlendFactor = BLEND_FACTOR_ZERO;
			xBlend.m_eDstAlphaBlendFactor = BLEND_FACTOR_ONE;
			xBlend.m_uColorWriteMask      = 0x7;  // R | G | B (no A)
		}

		// No vertex bindings — VS emits cube corners from SV_VertexID.
		xSpec.m_xVertexInputDesc.m_eTopology = MESH_TOPOLOGY_NONE;

		s_xApplyShader.GetReflection().PopulateLayout(xSpec.m_xPipelineLayout);

		Flux_PipelineBuilder::FromSpecification(s_xApplyPipeline, xSpec);
	}
}

// ===== INIT / SHUTDOWN =====

void Flux_Decals::Initialise()
{
	BuildPipelines();

	// One frame-indexed structured buffer for all decals.
	const u_int64 ulBufferSize = uMAX_DECALS * sizeof(DecalInstance);
	Zenith_Vector<DecalInstance> xZeroed(uMAX_DECALS);
	for (u_int u = 0; u < uMAX_DECALS; ++u) xZeroed.EmplaceBack();
	Flux_MemoryManager::InitialiseDynamicReadWriteBuffer(
		xZeroed.GetDataPointer(), ulBufferSize, s_xDecalBuffer);

	InitialiseDecalIndexBuffer();

	// CPU pool starts empty.
	for (u_int u = 0; u < uMAX_DECALS; ++u)
		s_axDecalSlots[u].m_bActive = false;
	s_uNextSlot         = 0;
	s_uActiveDecalCount = 0;

#ifdef ZENITH_TOOLS
	static const FluxShaderProgram s_axPrograms[] = {
		FluxShaderProgram::Decals_NormalsCopy,
		FluxShaderProgram::Decals_Apply,
	};
	Flux_ShaderHotReload::RegisterSubsystem(&Flux_Decals::BuildPipelines,
		s_axPrograms, sizeof(s_axPrograms) / sizeof(s_axPrograms[0]));
#endif

#ifdef ZENITH_DEBUG_VARIABLES
	Zenith_DebugVariables::AddBoolean({ "Render", "Decals", "Debug Spheres" },
		dbg_bDecalDebugSpheres);
#endif

	s_bInitialised = true;
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Decals initialised (deferred screen-space box, max %u decals)", uMAX_DECALS);
}

void Flux_Decals::Shutdown()
{
	if (!s_bInitialised)
		return;

	Flux_MemoryManager::DestroyDynamicReadWriteBuffer(s_xDecalBuffer);
	Flux_MemoryManager::DestroyIndexBuffer(s_xDecalIndexBuffer);

	// Release pipeline + shader GPU resources eagerly while the Vulkan
	// device is still alive. Static destructors run after Flux::Shutdown
	// has torn the device down, so leaving cleanup to the implicit dtor
	// would be a use-after-free on the device handle.
	s_xApplyPipeline.Reset();
	s_xNormalsCopyPipeline.Reset();
	s_xApplyShader.Reset();
	s_xNormalsCopyShader.Reset();

	s_pxGraph           = nullptr;
	s_uActiveDecalCount = 0;
	s_bInitialised      = false;

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Decals shut down");
}

// ===== GAME-SIDE API =====

void Flux_Decals::SpawnDecal(const Zenith_Maths::Vector3& xPosition,
                             const Zenith_Maths::Vector3& xNormal,
                             Zenith_TextureAsset* /*pxTexture*/,
                             float                        fSize,
                             float                        fLifetime)
{
	// Defensive: callers pass raycast hit normals which are unit-length,
	// but the public API can receive anything. A near-zero normal would
	// produce a NaN basis — log + bail.
	if (glm::length2(xNormal) < 1e-6f)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Decals::SpawnDecal: ignoring near-zero normal");
		return;
	}
	if (fSize <= 0.0f || fLifetime <= 0.0f)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_Decals::SpawnDecal: ignoring non-positive size/lifetime");
		return;
	}

	const Zenith_Maths::Vector3 xUnitNormal = glm::normalize(xNormal);

	// Pick the next slot in the ring. If the slot was active, we're
	// recycling — the active count stays the same. Otherwise we grow.
	const u_int uSlot = s_uNextSlot;
	CpuDecalSlot& xSlot = s_axDecalSlots[uSlot];
	const bool bWasActive = xSlot.m_bActive;

	xSlot.m_bActive            = true;
	xSlot.m_xPosition          = xPosition;
	xSlot.m_xNormal            = xUnitNormal;
	xSlot.m_fSize              = fSize;
	xSlot.m_fInitialLifetime   = fLifetime;
	xSlot.m_fRemainingLifetime = fLifetime;
	BuildDecalInstance(xPosition, xUnitNormal, fSize, 1.0f, xSlot.m_xInstance);

	s_uNextSlot = (uSlot + 1) % uMAX_DECALS;
	if (!bWasActive && s_uActiveDecalCount < uMAX_DECALS)
		++s_uActiveDecalCount;

	Zenith_Log(LOG_CATEGORY_RENDERER,
		"[DECAL] SpawnDecal slot=%u recycled=%d activeCount=%u "
		"pos=(%.2f,%.2f,%.2f) normal=(%.2f,%.2f,%.2f) size=%.3f lifetime=%.2f initialised=%d",
		uSlot, bWasActive ? 1 : 0, s_uActiveDecalCount,
		xPosition.x, xPosition.y, xPosition.z,
		xUnitNormal.x, xUnitNormal.y, xUnitNormal.z,
		fSize, fLifetime, s_bInitialised ? 1 : 0);
}

// ===== RECORD CALLBACKS =====

static void ExecuteNormalsCopy(Flux_CommandList* pxCommandList, void*)
{
	// No logging here — runs on a worker thread at 60Hz; per-frame log
	// from each of the parallel-recorder threads serialises them on the
	// logger mutex and starves the engine.
	if (s_uActiveDecalCount == 0)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xNormalsCopyPipeline);
	pxCommandList->AddCommand<Flux_CommandSetVertexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetVertexBuffer());
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&g_xEngine.FluxGraphics().m_xQuadMesh.GetIndexBuffer());

	Flux_ShaderBinder xBinder(*pxCommandList);
	xBinder.BindSRV(s_xNormalsCopyShader, "g_xNormalsTex",
		Flux_Graphics::GetGBufferSRV(MRT_INDEX_NORMALSAMBIENT));

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(6);
}

static void ExecuteApply(Flux_CommandList* pxCommandList, void*)
{
	if (s_uActiveDecalCount == 0)
		return;

	pxCommandList->AddCommand<Flux_CommandSetPipeline>(&s_xApplyPipeline);
	// Index-only draw — VS emits cube corners from SV_VertexID. The IB
	// values are unused but the command is required (Flux has no
	// non-indexed instanced draw).
	pxCommandList->AddCommand<Flux_CommandSetIndexBuffer>(&s_xDecalIndexBuffer);

	Flux_ShaderBinder xBinder(*pxCommandList);

	xBinder.BindCBV(s_xApplyShader, "FrameConstants", &g_xEngine.FluxGraphics().m_xFrameConstantsBuffer.GetCBV());

	xBinder.BindSRV(s_xApplyShader, "g_xDepthTex",       Flux_Graphics::GetDepthStencilSRV());
	xBinder.BindSRV(s_xApplyShader, "g_xNormalsCopyTex", &s_pxGraph->GetTransientAttachment(s_xNormalsCopyHandle).SRV());
	xBinder.BindSRV_Buffer(s_xApplyShader, "DecalBuffer", s_xDecalBuffer.GetSRV());

	pxCommandList->AddCommand<Flux_CommandDrawIndexed>(36, s_uActiveDecalCount);
}

// ===== PREPARE CALLBACK =====

static void PrepareDecals(void*)
{
	const float fDt = g_xEngine.Frame().GetDt();
	const u_int uPriorActive = s_uActiveDecalCount;

	// Tick lifetimes + pack active decals into the dense staging array.
	const u_int uActive = TickAndPackDense(fDt);
	s_uActiveDecalCount = uActive;

	if (uActive > 0)
	{
		Flux_MemoryManager::UploadBufferData(
			s_xDecalBuffer.GetBuffer().m_xVRAMHandle,
			s_axDecalStaging,
			uActive * sizeof(DecalInstance));
	}

	// Log only on count transitions — every-frame logging spams the console
	// at 60Hz and (combined with worker-thread record callbacks) caused
	// logger contention severe enough to wedge the engine in earlier
	// debug iterations.
	if (uActive != uPriorActive)
	{
		Zenith_Log(LOG_CATEGORY_RENDERER,
			"[DECAL] Prepare: %u -> %u active (%llu bytes uploaded this frame)",
			uPriorActive, uActive,
			(unsigned long long)(uActive * sizeof(DecalInstance)));
	}
}

// ===== GRAPH SETUP =====

void Flux_Decals::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	s_pxGraph = &xGraph;

	Flux_TransientTextureDesc xDesc;
	xDesc.m_uWidth       = Flux_Swapchain::GetWidth();
	xDesc.m_uHeight      = Flux_Swapchain::GetHeight();
	xDesc.m_eFormat      = k_eNormalsCopyFormat;
	xDesc.m_uMemoryFlags = (1u << MEMORY_FLAGS__SHADER_READ);
	s_xNormalsCopyHandle = xGraph.CreateTransient(xDesc);

	// NormalsCopy — clones live normalsAmbient into the transient. The
	// attached Prepare callback also runs the per-frame lifetime tick,
	// dense-packs active slots into the GPU staging array, and uploads.
	s_xNormalsCopyPass = xGraph.AddPass("Decal Normals Copy", ExecuteNormalsCopy)
		.Prepare(PrepareDecals)
		.ClearTargets()
		.Reads          (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_READ_SRV)
		.WritesTransient(s_xNormalsCopyHandle,                                       RESOURCE_ACCESS_WRITE_RTV);

	// Apply — instanced cube into all 3 G-buffer MRTs. Reads depth + the
	// cloned normals; writes diffuse / normalsAmbient / material under
	// per-attachment blend.
	s_xApplyPass = xGraph.AddPass("Decal Apply", ExecuteApply)
		.Reads         (Flux_Graphics::GetDepthAttachment(),                      RESOURCE_ACCESS_READ_SRV)
		.ReadsTransient(s_xNormalsCopyHandle,                                      RESOURCE_ACCESS_READ_SRV)
		.Writes        (Flux_Graphics::GetMRTAttachment(MRT_INDEX_DIFFUSE),        RESOURCE_ACCESS_WRITE_RTV)
		.Writes        (Flux_Graphics::GetMRTAttachment(MRT_INDEX_NORMALSAMBIENT), RESOURCE_ACCESS_WRITE_RTV)
		.Writes        (Flux_Graphics::GetMRTAttachment(MRT_INDEX_MATERIAL),       RESOURCE_ACCESS_WRITE_RTV);

	// Both passes are always-enabled. The render graph skips Prepare
	// callbacks for disabled passes (Flux_RenderGraph_Execution.cpp:144),
	// so we cannot gate these via SetEnabled — that would prevent the
	// active count from ever lifting off zero. Idle-frame cost is bounded
	// by the early-out at the top of each Execute callback (no commands
	// recorded when active==0; only the render-pass setup runs).
}

// ===== TEST HOOKS =====

#ifdef ZENITH_TESTING
u_int Flux_Decals::GetActiveCountForTest()
{
	return s_uActiveDecalCount;
}

Flux_Decals::TestSlotView Flux_Decals::GetSlotForTest(u_int uSlotIndex)
{
	Zenith_Assert(uSlotIndex < uMAX_DECALS, "Flux_Decals::GetSlotForTest: index %u >= %u", uSlotIndex, uMAX_DECALS);
	const CpuDecalSlot& xSlot = s_axDecalSlots[uSlotIndex];
	TestSlotView xView;
	xView.m_xPosition          = xSlot.m_xPosition;
	xView.m_xNormal            = xSlot.m_xNormal;
	xView.m_fRemainingLifetime = xSlot.m_fRemainingLifetime;
	xView.m_bActive            = xSlot.m_bActive;
	return xView;
}

void Flux_Decals::ResetForTest()
{
	for (u_int u = 0; u < uMAX_DECALS; ++u)
		s_axDecalSlots[u].m_bActive = false;
	s_uNextSlot         = 0;
	s_uActiveDecalCount = 0;
}
#endif
