#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#define ZENITH_FLUX_NUM_CSMS 4
#define ZENITH_FLUX_CSM_RESOLUTION 2048

// CSM depth format — exposed here so subsystems that build shadow pipelines at
// Initialise() time can reference it without going through a graph-owned
// transient accessor (which requires the graph to exist).
static constexpr TextureFormat CSM_FORMAT = TEXTURE_FORMAT_D32_SFLOAT;

// ---------------------------------------------------------------------------
// Per-cascade data the lighting pass needs to sample the shadow maps. Computed
// once per frame in UpdateShadowMatrices() and consumed by Flux_DeferredShading
// (cascade selection by view depth, normal-offset bias, PCSS penumbra estimate).
// One float4 per field keeps the GPU mirror std140-clean (HLSL arrays pad each
// element to a float4 anyway).
// ---------------------------------------------------------------------------
struct Flux_ShadowCascadeSamplingData
{
	// x..w = the camera-view-space far depth (linear, +Z forward) of cascades 0..3.
	// A fragment selects cascade i = first index whose view depth <= split[i].
	Zenith_Maths::Vector4 m_xCascadeSplitViewDepth = Zenith_Maths::Vector4(0.f);
	// World units covered by one shadow texel for cascades 0..3 (2*radius/res).
	// Drives the normal-offset bias and the PCSS world->texel conversion.
	Zenith_Maths::Vector4 m_xCascadeWorldPerTexel = Zenith_Maths::Vector4(1.f);
	// Light-space depth range (far-near, world units) of cascades 0..3. Ortho
	// depth is linear, so NDC depth deltas scale by this to recover world depth.
	Zenith_Maths::Vector4 m_xCascadeDepthRange = Zenith_Maths::Vector4(1.f);
};

// Global (not per-cascade) shadow-sampling tunables. Backed by debug variables
// so they can be dialled at runtime; mirrored verbatim into the deferred shader.
struct Flux_ShadowSamplingConfig
{
	float m_fResolution        = float(ZENITH_FLUX_CSM_RESOLUTION);
	float m_fRcpResolution     = 1.f / float(ZENITH_FLUX_CSM_RESOLUTION);
	float m_fNormalOffsetTexels = 2.5f;   // world normal offset, in texels, at grazing angles
	// (depth bias is fixed-function: vkCmdSetDepthBias on the caster pipelines)
	float m_fPCFRadiusTexels    = 2.0f;    // base penumbra radius in texels
	float m_fSunAngularRadius   = 0.018f;  // ~1° half-angle; drives PCSS softening
	float m_fCascadeBlendFraction = 0.12f; // fraction of a cascade's far split used to cross-fade
	u_int m_uPCFTapCount        = 16u;     // Vogel-disk taps for the filter kernel
	u_int m_bPCSSEnabled        = 1u;      // contact-hardening blocker search
};

// GPU mirror of the shadow-sampling parameters — uploaded to a dynamic constant
// buffer each frame and bound to the deferred shader's ShadowSampling CB. Pure
// float4s so HLSL/std140 packing matches byte-for-byte (see ShadowSamplingLayout
// in Flux_DeferredShading.slang).
struct Flux_ShadowSamplingGPU
{
	// Defaults are SANE (not zero): the buffer is seeded with these at Initialise,
	// so even if a shadows-disabled boot never calls UpdateShadowMatrices, the
	// shader reads a valid tap count (w of m_xParams1 >= 1) rather than garbage
	// VRAM — guards the PCF loop bound against a GPU hang / 0-divide.
	Zenith_Maths::Vector4 m_xCascadeSplitViewDepth = Zenith_Maths::Vector4(1e9f);            // huge -> always selects cascade 0
	Zenith_Maths::Vector4 m_xCascadeWorldPerTexel  = Zenith_Maths::Vector4(0.01f);
	Zenith_Maths::Vector4 m_xCascadeDepthRange     = Zenith_Maths::Vector4(1.f);
	Zenith_Maths::Vector4 m_xParams0               = Zenith_Maths::Vector4(float(ZENITH_FLUX_CSM_RESOLUTION), 1.f / float(ZENITH_FLUX_CSM_RESOLUTION), 2.5f, 0.f); // res, rcpRes, normalOffsetTexels, (spare)
	Zenith_Maths::Vector4 m_xParams1               = Zenith_Maths::Vector4(2.f, 0.013f, 0.12f, 16.f); // pcfRadiusTexels, sunAngularRadius, cascadeBlendFraction, tapCount
	Zenith_Maths::Vector4 m_xParams2               = Zenith_Maths::Vector4(1.f, 0.f, 0.f, 0.f);        // pcssEnabled, spare...
};

// Phase 9: state + behaviour for shadows subsystem.
class Flux_ShadowsImpl
{
public:
	Flux_ShadowsImpl() = default;
	~Flux_ShadowsImpl() = default;

	Flux_ShadowsImpl(const Flux_ShadowsImpl&) = delete;
	Flux_ShadowsImpl& operator=(const Flux_ShadowsImpl&) = delete;

	void Initialise();
	void Shutdown();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	Flux_RenderAttachment* GetCSMTargetSetup(const uint32_t uIndex, uint32_t& uNumColour, Flux_RenderAttachment*& pxDepthStencil);
	Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex) { return m_axSunViewProjMats[uIndex]; }
	Flux_ShaderResourceView& GetCSMSRV(const uint32_t u);
	Flux_DynamicConstantBuffer& GetShadowMatrixBuffer(const uint32_t u) { return m_xShadowMatrixBuffers[u]; }

	void UpdateShadowMatrices();

	// Per-cascade + global sampling data for the lighting pass. Valid after
	// UpdateShadowMatrices() has run for the current frame.
	const Flux_ShadowCascadeSamplingData& GetCascadeSamplingData() const { return m_xCascadeSamplingData; }
	const Flux_ShadowSamplingConfig&      GetSamplingConfig()      const { return m_xSamplingConfig; }
	// Dynamic CB holding the GPU mirror of the sampling params (bound by the
	// deferred lighting pass as the ShadowSampling constant buffer).
	Flux_DynamicConstantBuffer&           GetShadowSamplingBuffer() { return m_xShadowSamplingBuffer; }

	Flux_TransientHandle       m_axCSMHandles[ZENITH_FLUX_NUM_CSMS];
	Flux_RenderGraph*          m_pxGraph = nullptr;
	Flux_DynamicConstantBuffer m_xShadowMatrixBuffers[ZENITH_FLUX_NUM_CSMS];
	Flux_DynamicConstantBuffer m_xShadowSamplingBuffer;
	Zenith_Maths::Matrix4      m_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];

private:
	Flux_RenderAttachment& GetCSM(u_int uIndex);

	Flux_ShadowCascadeSamplingData m_xCascadeSamplingData;
	Flux_ShadowSamplingConfig      m_xSamplingConfig;
};
