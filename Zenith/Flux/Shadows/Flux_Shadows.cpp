#include "Zenith.h"
#include "Core/Zenith_Engine.h"

#include "Flux/Shadows/Flux_ShadowsImpl.h"
#include "Flux/Shadows/Flux_ShadowsImpl.h"

#include "Core/Zenith_GraphicsOptions.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_BackendTypes.h"
#include "Profiling/Zenith_Profiling.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/AnimatedMeshes/Flux_AnimatedMeshesImpl.h"
#include "Flux/Terrain/Flux_TerrainImpl.h"
#include "Flux/UnifiedMesh/Flux_UnifiedMeshImpl.h"   // Stage 2: unified GPU-driven shadow casters

// Graph-owned transient — backing Flux_RenderAttachment is allocated and
// destroyed by the render graph, sized from the descriptor in SetupRenderGraph.
// CSM_FORMAT is declared in Flux_Shadows.h so subsystems that build shadow
// pipelines at Initialise() time can reference it.




// ---------------------------------------------------------------------------
// Cascade-fit + sampling tunables. The cascade fit is a stabilised
// bounding-sphere scheme (rotation/translation invariant) with texel snapping;
// the sampling tunables are mirrored into the deferred lighting shader.
// ---------------------------------------------------------------------------

// PSSM practical-split blend: 0 = uniform splits, 1 = logarithmic. ~0.85 is the
// usual sweet spot (tight near cascades, coarse far ones).
DEBUGVAR float dbg_fSplitLambda        = 0.85f;
// Far end of the shadowed range (m). Cascades pack their resolution into
// [camera-near, this], capped by the camera far plane. Concentrating cascades
// near the viewer is what makes 2048² maps look high-res up close.
DEBUGVAR float dbg_fShadowDistance     = 300.f;
// How far (in cascade radii) to push the light origin back past the bounding
// sphere so off-frustum occluders still rasterise in. Replaces the old 17x
// depth-range inflation that destroyed depth precision.
DEBUGVAR float dbg_fCasterExtendRadii  = 1.0f;
// Acne control is split between two mechanisms:
//  - Fixed-function slope-scaled depth bias on the caster pipelines (the actual
//    "depth bias", applied by the rasterizer via vkCmdSetDepthBias). Slope-scaled
//    bias works on D32 float depth where a constant bias is unreliable, so it
//    carries the load; the constant term is a small top-up.
//  - Normal-offset (texels of world-space push along the surface normal, in the
//    receiver). This is NOT depth bias — it shifts the sample position, has no
//    fixed-function equivalent, and handles grazing-angle acne without the
//    peter-panning a large depth bias would cause.
DEBUGVAR float dbg_fNormalOffsetTexels      = 2.5f;
DEBUGVAR float dbg_fShadowDepthBiasConstant = 1.75f; // vkCmdSetDepthBias constant factor
DEBUGVAR float dbg_fShadowDepthBiasSlope    = 3.0f;  // vkCmdSetDepthBias slope factor
// Base PCF kernel radius (texels) and tap count of the Vogel disk.
DEBUGVAR float dbg_fPCFRadiusTexels    = 2.0f;
DEBUGVAR uint32_t dbg_uPCFTapCount     = 16u;
// PCSS contact hardening: estimate penumbra from a blocker search so shadows
// sharpen at contact and soften with distance. Sun half-angle drives the width.
DEBUGVAR bool  dbg_bPCSSEnabled        = true;
DEBUGVAR float dbg_fSunAngularRadius   = 0.013f;
// Fraction of a cascade's far split over which it cross-fades into the next
// cascade, hiding the seam.
DEBUGVAR float dbg_fCascadeBlendFraction = 0.12f;

Flux_RenderAttachment& Flux_ShadowsImpl::GetCSMArray()
{
	Zenith_Assert(m_pxGraph, "Flux_ShadowsImpl::GetCSMArray: graph pointer is null");
	return m_pxGraph->GetTransientAttachment(m_xCSMArrayHandle);
}

struct FrustumCorners
{
	const Zenith_Maths::Vector3 GetCenter() const
	{
		Zenith_Maths::Vector3 xRet(0,0,0);
		for (uint32_t u = 0; u < 8; u++) xRet += m_axCorners[u];
		xRet /= 8;
		return xRet;
	}
	Zenith_Maths::Vector3 m_axCorners[8];
};

static FrustumCorners WorldSpaceFrustumCornersFromInverseViewProjMatrix(const Zenith_Maths::Matrix4& xInvViewProjMat)
{
	FrustumCorners xRet;
	uint32_t uCount = 0;
	for (uint32_t uX = 0; uX < 2; uX++)
	{
		for (uint32_t uY = 0; uY < 2; uY++)
		{
			for (uint32_t uZ = 0; uZ < 2; uZ++)
			{
				Zenith_Maths::Vector4 xCorner = xInvViewProjMat * Zenith_Maths::Vector4(2.f * uX - 1.f, 2.f * uY - 1.f, uZ ? 1.f : 0.f, 1.f);
				xRet.m_axCorners[uCount++] = Zenith_Maths::Vector3(xCorner) / xCorner.w;
			}
		}
	}

	return xRet;
}

void Flux_ShadowsImpl::Initialise()
{
	// One StructuredBuffer<float4x4> holding all 4 cascade sun view×proj matrices
	// (Phase 4a collapse — replaces the 4 separate per-cascade constant buffers).
	g_xEngine.FluxMemory().InitialiseDynamicReadWriteBuffer(nullptr, ZENITH_FLUX_NUM_CSMS * sizeof(Zenith_Maths::Matrix4), m_xShadowMatricesBuffer);
	// Seed with sane defaults (not nullptr): a shadows-disabled boot never runs
	// UpdateShadowMatrices, and the deferred shader still reads the tap count from
	// this CB to bound its PCF loop. Garbage VRAM there = unbounded loop / 0-divide.
	const Flux_ShadowSamplingGPU xDefaultSampling;
	g_xEngine.FluxMemory().InitialiseDynamicConstantBuffer(&xDefaultSampling, sizeof(Flux_ShadowSamplingGPU), m_xShadowSamplingBuffer);

#ifdef ZENITH_DEBUG_VARIABLES
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Split Lambda"},        dbg_fSplitLambda,        0.f, 1.f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Shadow Distance"},     dbg_fShadowDistance,     10.f, 2000.f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Caster Extend Radii"}, dbg_fCasterExtendRadii,  0.f, 8.f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Normal Offset Texels"},dbg_fNormalOffsetTexels, 0.f, 8.f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Depth Bias Constant"}, dbg_fShadowDepthBiasConstant, 0.f, 16.f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Depth Bias Slope"},    dbg_fShadowDepthBiasSlope,    0.f, 16.f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "PCF Radius Texels"},   dbg_fPCFRadiusTexels,    0.f, 8.f);
	g_xEngine.DebugVariables().AddUInt32 ({"Render", "Shadows", "PCF Tap Count"},       dbg_uPCFTapCount,        1u, 32u);
	g_xEngine.DebugVariables().AddBoolean({"Render", "Shadows", "PCSS Enabled"},        dbg_bPCSSEnabled);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Sun Angular Radius"},  dbg_fSunAngularRadius,   0.f, 0.1f);
	g_xEngine.DebugVariables().AddFloat  ({"Render", "Shadows", "Cascade Blend Frac"},  dbg_fCascadeBlendFraction, 0.f, 0.5f);
#endif
}

void Flux_ShadowsImpl::Shutdown()
{
	g_xEngine.FluxMemory().DestroyDynamicReadWriteBuffer(m_xShadowMatricesBuffer);
	g_xEngine.FluxMemory().DestroyDynamicConstantBuffer(m_xShadowSamplingBuffer);

	m_pxGraph = nullptr;
}

// Persistent pass names (W1: prevents dangling stack-buffer pointers passed to AddPass).
static const char* const s_aszShadowCascadePassNames[ZENITH_FLUX_NUM_CSMS] =
{
	"Shadow Cascade 0",
	"Shadow Cascade 1",
	"Shadow Cascade 2",
	"Shadow Cascade 3",
};
static_assert(ZENITH_FLUX_NUM_CSMS == 4, "s_aszShadowCascadePassNames must match ZENITH_FLUX_NUM_CSMS");

// Cascade index is passed through the graph's typed user-data slot — see
// Flux_PassBuilder::UserData<T> / Flux_UnpackUserData<T> in Flux_RenderGraph.h.

static void PreExecuteShadowMatrices(void*)
{
	// Ensures the UNCULLLED static/animated geometry shadow packets once per frame on the main
	// thread before parallel cascade recording (Phase 3 backstop: off-screen casters still cast
	// even when the mesh G-buffer passes — which would otherwise build the packets — are
	// force-disabled). Generation-guarded, so a no-op when the G-buffer prepares already built them.
	//
	// NOTE: the cascade view×proj matrices are NO LONGER computed here. UpdateShadowMatrices was
	// hoisted to the main-thread seam in Zenith_Core (right after the snapshot rebuild) so the
	// unified mesh cull's .Prepare — which runs earlier in topological order once the cascade
	// passes read its cull-output buffers (Stage 2) — sees up-to-date cascade frustums. The
	// matrices are still ready well before any cascade records.
	if (!Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		return;
	}
	g_xEngine.Shadows().EnsureGeometryShadowPackets();
}

static void ExecuteShadowCascade(Flux_CommandBuffer* pxCommandList, void* pUserData)
{
	if (!Zenith_GraphicsOptions::Get().m_bShadowsEnabled)
	{
		return;
	}

	const uint32_t u = Flux_UnpackUserData<uint32_t>(pUserData);

	// Fixed-function slope-scaled depth bias for shadow acne. The caster pipelines
	// declare depth-bias + dynamic-depth-bias state; this sets it per cascade
	// command list (dynamic state is per-command-buffer). Bias is applied entirely
	// by the rasterizer here — never in the sampling shader.
	pxCommandList->SetDepthBias(dbg_fShadowDepthBiasConstant, dbg_fShadowDepthBiasSlope, 0.f);

	// All casters read the cascade matrix from the persistent VIEW set's all-cascade
	// g_xShadowMatrices SSBO (Phase 5.4), selecting element `u` via the per-draw cascade index.

	// Skeletal-animated casters keep their own per-bone shadow path (Stage 5 territory).
	auto& xAnimatedMeshes = g_xEngine.AnimatedMeshes();
	pxCommandList->SetPipeline(&xAnimatedMeshes.GetShadowPipeline());

	// RenderToShadowMap handles all bindings via shader reflection
	xAnimatedMeshes.RenderToShadowMap(*pxCommandList, u);

	// Unified GPU-driven opaque casters (Stage 4): the single path for opaque statics AND instanced
	// foliage (the old StaticMeshes/InstancedMeshes shadow loops were retired here). Binds its own
	// depth-only pipeline + draws cascade view (u+1) of the shared cull-output buffers (per-cascade
	// frustum-culled). The cascade pass declares the cull-output ReadBuffer (see SetupRenderGraph)
	// so the reset->cull->cascade barrier is synthesised.
	g_xEngine.UnifiedMesh().RenderToShadowMap(*pxCommandList, u);

	// #TODO: Enable terrain shadow casting
	// g_xEngine.Terrain().RenderToShadowMap(*pxCommandList, u);
}

void Flux_ShadowsImpl::SetupRenderGraph(Flux_RenderGraph& xGraph)
{
	m_pxGraph = &xGraph;

	// One 4-layer depth-array transient holds all cascades (Phase 4b collapse).
	// Each cascade pass writes its own layer; the lit/fog passes sample the whole
	// array as a Sampler2DArray. The 4 cascade passes have no GPU dependency on
	// each other (disjoint layers) so they still record in parallel.
	Flux_TransientTextureDesc xCSMDesc;
	xCSMDesc.m_uWidth = ZENITH_FLUX_CSM_RESOLUTION;
	xCSMDesc.m_uHeight = ZENITH_FLUX_CSM_RESOLUTION;
	xCSMDesc.m_eFormat = CSM_FORMAT;
	xCSMDesc.m_uNumLayers = ZENITH_FLUX_NUM_CSMS;
	xCSMDesc.m_uMemoryFlags = 1u << MEMORY_FLAGS__SHADER_READ;
	xCSMDesc.m_bIsDepthStencil = true;
	m_xCSMArrayHandle = xGraph.CreateTransient(xCSMDesc);

	// Stage 2: the unified GPU-driven path draws static shadow casters from its compute-culled
	// per-(view,bucket) cull-output buffers. Each cascade pass must READ those persistent buffers
	// so the graph orders reset->cull->cascade and synthesises the WRITE_UAV->READ barrier. The
	// buffers exist after UnifiedMesh::Initialise (all Initialise precede all SetupRenderGraph).
	// Declared UNCONDITIONALLY (not gated on the runtime toggle): the cull pass always writes
	// them, the read is harmless when the unified path is off (the cascade draw early-outs), and
	// keeping the declaration static means toggling the path needs no graph rebuild for shadows.
	Flux_UnifiedMeshImpl& xUnified = g_xEngine.UnifiedMesh();
	const bool bUnifiedResources = xUnified.m_bResourcesReady;

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		// CSM targets are depth textures — declared as per-layer DSV writes (mip 0,
		// layer u). Cascade 0 owns the CPU-side geometry-packet ensure via its pre-execute
		// callback; the disjoint-layer writes record in parallel.
		const Flux_PassHandle xPass = xGraph.AddPass(s_aszShadowCascadePassNames[u], ExecuteShadowCascade)
			.UserData(u)
			.ClearTargets()
			.WritesTransient(m_xCSMArrayHandle, RESOURCE_ACCESS_WRITE_DSV, 0, 1, u, 1);
		if (u == 0)
			xGraph.SetPrepare(xPass, PreExecuteShadowMatrices);

		if (bUnifiedResources)
		{
			xGraph.ReadBuffer(xPass, xUnified.m_xVisibleIndexBuffer.GetBuffer(), RESOURCE_ACCESS_READ_BUFFER_SRV);
			xGraph.ReadBuffer(xPass, xUnified.m_xIndirectBuffer.GetBuffer(),     RESOURCE_ACCESS_READ_INDIRECT_ARG);
		}
	}
}

Flux_ShaderResourceView& Flux_ShadowsImpl::GetCSMArraySRV()
{
	// Always the array SRV (Sampler2DArray) — valid even when shadows are disabled:
	// the cascade passes still clear the array to far depth, so sampling yields
	// "not in shadow". A 2D white-texture fallback would be the wrong descriptor
	// type for a Sampler2DArray, so there is no disabled-path substitute here.
	return GetCSMArray().SRV();
}


void Flux_ShadowsImpl::EnsureGeometryShadowPackets()
{
	// Ensure the UNCULLLED animated geometry shadow packet (cascade-0 Prepare). Generation-guarded
	// inside the consumer, so this is a no-op when the animated G-buffer Prepare already built it
	// this frame; it's the backstop when that G-buffer pass is force-disabled. (Opaque statics +
	// instanced foliage now cast via the GPU-driven unified path, which needs no CPU packet.)
	if (m_pxAnimatedMeshes) m_pxAnimatedMeshes->EnsureAnimatedPacket();
}

void Flux_ShadowsImpl::UpdateShadowMatrices()
{
	g_xEngine.Profiling().BeginProfileZone(ZENITH_PROFILE_ZONE("Flux Shadows Update Matrices"));
	Flux_GraphicsImpl& xGraphics = g_xEngine.FluxGraphics();
	const Zenith_Maths::Matrix4& xViewMat = xGraphics.GetViewMatrix();

	// GetFOV() returns radians for the game camera but DEGREES for the editor
	// camera while Stopped/Paused (Zenith_Editor::GetCameraFOV). PerspectiveProjection
	// expects radians; feeding it 45 "radians" makes the reconstructed frustum (and
	// every cascade derived from it) garbage. Normalise defensively: a real vertical
	// FOV is always < PI radians, so anything larger must be degrees.
	float fFOV = xGraphics.GetFOV();
	if (fFOV > 3.14159265f) fFOV = glm::radians(fFOV);
	const float fAspect   = xGraphics.GetAspectRatio();
	const float fCamNear  = xGraphics.GetNearPlane();
	const float fCamFar   = xGraphics.GetFarPlane();
	const float fResolution = float(ZENITH_FLUX_CSM_RESOLUTION);

	// Pack cascade resolution into [near, shadow-distance] (capped by far plane).
	const float fShadowNear = fCamNear;
	const float fShadowFar  = glm::max(fShadowNear + 1.f, glm::min(fCamFar, dbg_fShadowDistance));

	// ---- Practical split scheme (Zhang et al.): blend logarithmic + uniform. ----
	float afSplits[ZENITH_FLUX_NUM_CSMS + 1];
	afSplits[0] = fShadowNear;
	afSplits[ZENITH_FLUX_NUM_CSMS] = fShadowFar;
	for (uint32_t i = 1; i < ZENITH_FLUX_NUM_CSMS; i++)
	{
		const float fSI  = float(i) / float(ZENITH_FLUX_NUM_CSMS);
		const float fLog = fShadowNear * powf(fShadowFar / fShadowNear, fSI);
		const float fUni = fShadowNear + (fShadowFar - fShadowNear) * fSI;
		afSplits[i] = glm::mix(fUni, fLog, glm::clamp(dbg_fSplitLambda, 0.f, 1.f));
	}

	// Sun direction points light -> scene; the light sits opposite. Guard the
	// up-vector so a near-vertical sun doesn't degenerate glm::lookAt.
	const Zenith_Maths::Vector3 xSunDir = glm::normalize(xGraphics.GetSunDir());
	const Zenith_Maths::Vector3 xUp = (fabsf(xSunDir.y) > 0.99f) ? Zenith_Maths::Vector3(0, 0, 1)
																  : Zenith_Maths::Vector3(0, 1, 0);

	for (uint32_t u = 0; u < ZENITH_FLUX_NUM_CSMS; u++)
	{
		const float fSliceNear = afSplits[u];
		const float fSliceFar  = afSplits[u + 1];

		// World-space corners of this frustum slice.
		const Zenith_Maths::Matrix4 xSliceProj    = Zenith_Maths::PerspectiveProjection(fFOV, fAspect, fSliceNear, fSliceFar);
		const Zenith_Maths::Matrix4 xInvViewProj  = glm::inverse(xSliceProj * xViewMat);
		const FrustumCorners        xCorners      = WorldSpaceFrustumCornersFromInverseViewProjMatrix(xInvViewProj);

		// Bounding sphere of the slice. Its RADIUS depends only on FOV/aspect/split
		// (not on camera orientation or position), so the cascade extent — and
		// therefore the texel footprint — is invariant as the camera turns. This is
		// what eliminates the edge shimmer the AABB-in-light-space fit suffered.
		const Zenith_Maths::Vector3 xCenter = xCorners.GetCenter();
		float fRadius = 0.f;
		for (uint32_t c = 0; c < 8; c++)
			fRadius = glm::max(fRadius, glm::length(xCorners.m_axCorners[c] - xCenter));
		// Quantise the radius so sub-pixel float wobble in the corner positions
		// can't perturb world-units-per-texel between frames.
		fRadius = ceilf(fRadius * 16.f) / 16.f;

		const float fWorldPerTexel = (2.f * fRadius) / fResolution;

		// Push the light origin back past the sphere by a bounded margin so
		// occluders between the light and the slice still rasterise into the map.
		const float fCasterExtend = fRadius * glm::max(0.f, dbg_fCasterExtendRadii);
		const float fBackDist     = fRadius + fCasterExtend;
		const float fDepthRange   = 2.f * fRadius + fCasterExtend;

		const Zenith_Maths::Vector3 xLightEye  = xCenter - xSunDir * fBackDist;
		const Zenith_Maths::Matrix4 xLightView = glm::lookAt(xLightEye, xCenter, xUp);
		Zenith_Maths::Matrix4 xLightProj = Zenith_Maths::OrthographicProjection(-fRadius, fRadius, -fRadius, fRadius, 0.f, fDepthRange);

		// ---- Texel snapping: lock the sampling grid to whole shadow texels in
		// world space so the shadow pattern doesn't crawl as the camera moves. ----
		const Zenith_Maths::Matrix4 xUnsnapped = xLightProj * xLightView;
		Zenith_Maths::Vector4 xOrigin = xUnsnapped * Zenith_Maths::Vector4(0.f, 0.f, 0.f, 1.f);
		xOrigin *= fResolution * 0.5f;
		const Zenith_Maths::Vector2 xRounded(glm::round(xOrigin.x), glm::round(xOrigin.y));
		const Zenith_Maths::Vector2 xSnapOffset = (xRounded - Zenith_Maths::Vector2(xOrigin.x, xOrigin.y)) * (2.f / fResolution);
		xLightProj[3][0] += xSnapOffset.x;
		xLightProj[3][1] += xSnapOffset.y;

		m_axSunViewProjMats[u] = xLightProj * xLightView;

		// Sampling data consumed by the deferred lighting pass.
		m_xCascadeSamplingData.m_xCascadeSplitViewDepth[u] = fSliceFar;
		m_xCascadeSamplingData.m_xCascadeWorldPerTexel[u]  = fWorldPerTexel;
		m_xCascadeSamplingData.m_xCascadeDepthRange[u]     = fDepthRange;
	}

	// Upload all 4 cascade matrices in one write to the single StructuredBuffer
	// (Phase 4a). m_axSunViewProjMats is a contiguous Matrix4[ZENITH_FLUX_NUM_CSMS].
	g_xEngine.FluxMemory().UploadBufferData(m_xShadowMatricesBuffer.GetBuffer().m_xVRAMHandle, m_axSunViewProjMats, sizeof(m_axSunViewProjMats));

	// Mirror runtime-tunable sampling config for the shader (cheap; lets the
	// debug variables take effect live).
	m_xSamplingConfig.m_fResolution           = fResolution;
	m_xSamplingConfig.m_fRcpResolution        = 1.f / fResolution;
	m_xSamplingConfig.m_fNormalOffsetTexels   = dbg_fNormalOffsetTexels;
	m_xSamplingConfig.m_fPCFRadiusTexels      = dbg_fPCFRadiusTexels;
	m_xSamplingConfig.m_fSunAngularRadius     = dbg_fSunAngularRadius;
	m_xSamplingConfig.m_fCascadeBlendFraction = dbg_fCascadeBlendFraction;
	m_xSamplingConfig.m_uPCFTapCount          = glm::clamp(dbg_uPCFTapCount, 1u, 32u);
	m_xSamplingConfig.m_bPCSSEnabled          = dbg_bPCSSEnabled ? 1u : 0u;

	// Pack + upload the GPU mirror for the deferred lighting pass.
	Flux_ShadowSamplingGPU xGPU;
	xGPU.m_xCascadeSplitViewDepth = m_xCascadeSamplingData.m_xCascadeSplitViewDepth;
	xGPU.m_xCascadeWorldPerTexel  = m_xCascadeSamplingData.m_xCascadeWorldPerTexel;
	xGPU.m_xCascadeDepthRange     = m_xCascadeSamplingData.m_xCascadeDepthRange;
	xGPU.m_xParams0 = Zenith_Maths::Vector4(m_xSamplingConfig.m_fResolution, m_xSamplingConfig.m_fRcpResolution, m_xSamplingConfig.m_fNormalOffsetTexels, 0.f);
	xGPU.m_xParams1 = Zenith_Maths::Vector4(m_xSamplingConfig.m_fPCFRadiusTexels, m_xSamplingConfig.m_fSunAngularRadius, m_xSamplingConfig.m_fCascadeBlendFraction, float(m_xSamplingConfig.m_uPCFTapCount));
	xGPU.m_xParams2 = Zenith_Maths::Vector4(float(m_xSamplingConfig.m_bPCSSEnabled), 0.f, 0.f, 0.f);
	g_xEngine.FluxMemory().UploadBufferData(m_xShadowSamplingBuffer.GetBuffer().m_xVRAMHandle, &xGPU, sizeof(xGPU));

	g_xEngine.Profiling().EndProfileZone(ZENITH_PROFILE_ZONE("Flux Shadows Update Matrices"));
}
