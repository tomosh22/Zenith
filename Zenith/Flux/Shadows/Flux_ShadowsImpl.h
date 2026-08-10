#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"

#define ZENITH_FLUX_NUM_CSMS 4
#define ZENITH_FLUX_CSM_RESOLUTION 2048

// The render-view registry reserves one fixed view slot per cascade (slots
// 1..N; see Flux/RenderViews/Flux_RenderViews.h). Its dependency-light local
// count must track the cascade count here.
#include "Flux/RenderViews/Flux_RenderViews.h"
static_assert(kuFluxViewNumShadowSlots == ZENITH_FLUX_NUM_CSMS,
	"kuFluxViewNumShadowSlots (Flux_RenderViews.h) must equal ZENITH_FLUX_NUM_CSMS");

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

// Runtime sampling flags packed numerically into g_xParams2.y. Small integer
// values round-trip exactly through float; keep bit values mirrored by
// SHADOW_FLAG_* in Common/ShadowSampling.slang.
inline constexpr u_int FLUX_SHADOW_FLAG_PCSS_CASCADE0_ONLY     = 1u << 0u;
inline constexpr u_int FLUX_SHADOW_FLAG_CHEAP_FAR_CASCADES     = 1u << 1u;
inline constexpr u_int FLUX_SHADOW_FLAG_ROUGHNESS_GATED_PCSS   = 1u << 2u;
inline constexpr u_int FLUX_SHADOW_FLAG_CASCADE_FALLBACK_BLEND = 1u << 3u;
inline constexpr u_int FLUX_SHADOW_FLAG_CONTACT_SHADOWS        = 1u << 4u;
inline constexpr u_int FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS =
	FLUX_SHADOW_FLAG_PCSS_CASCADE0_ONLY |
	FLUX_SHADOW_FLAG_CHEAP_FAR_CASCADES |
	FLUX_SHADOW_FLAG_CASCADE_FALLBACK_BLEND |
	FLUX_SHADOW_FLAG_CONTACT_SHADOWS;

// Global (not per-cascade) shadow-sampling tunables. Backed by debug variables
// so they can be dialled at runtime; packed into the deferred shader's float4s.
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
	u_int m_uQualityFlags       = FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS;
	float m_fPCSSRoughnessThreshold = 0.6f;
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
	Zenith_Maths::Vector4 m_xParams2               = Zenith_Maths::Vector4(1.f, float(FLUX_SHADOW_DEFAULT_SAMPLING_FLAGS), 0.6f, 0.f); // pcssEnabled, qualityFlags, roughnessThreshold, spare
};
// Must stay 6× float4 (96B) to match ShadowSamplingLayout in
// Shaders/DeferredShading/Flux_DeferredShading.slang byte-for-byte (std140, no
// scalar straddling). A silent drift here corrupts every shadow sample — guard it
// like the other GPU-mirror structs (MeshDrawConstants / TerrainMaterialDrawConstants).
static_assert(sizeof(Flux_ShadowSamplingGPU) == 96, "Flux_ShadowSamplingGPU must be 6x float4 (96B) to match ShadowSamplingLayout");

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
	// No-op: Shadows owns no shader programs of its own — the caster pipelines live in
	// the mesh subsystems (StaticMesh_ToShadowmap etc.), so there is nothing to
	// build/hot-reload. Present only to satisfy the uniform FluxRenderFeature interface.
	void BuildPipelines() {}

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// uIndex is a CASCADE index in [0, ZENITH_FLUX_NUM_CSMS). The debug-variable
	// override path (Render/Shadows/Override ViewProj Mat Index) feeds this
	// straight from a slider, so an off-by-one in the registered bound used to
	// read one matrix past the array — the assert makes that a break, not a
	// silent garbage view matrix.
	Zenith_Maths::Matrix4 GetSunViewProjMatrix(const uint32_t uIndex)
	{
		Zenith_Assert(uIndex < ZENITH_FLUX_NUM_CSMS,
			"Cascade index %u out of range (valid: 0..%u)", uIndex, ZENITH_FLUX_NUM_CSMS - 1);
		return m_axSunViewProjMats[uIndex];
	}
	// The CSM depth-array transient handle — for consumers (DeferredShading /
	// Translucency) to declare a graph Read() spanning all cascade layers so the
	// WRITE_DSV → SHADER_READ barrier covers every layer. Valid after SetupRenderGraph.
	Flux_TransientHandle GetCSMArrayHandle() const { return m_xCSMArrayHandle; }
	// Whole-array SRV of the 4-cascade CSM depth array (Sampler2DArray in shaders).
	// Phase 4b collapse — replaces the per-cascade GetCSMSRV(u). Always valid (the
	// transient is graph-allocated even when shadows are disabled; the cascade
	// passes then just clear it to far depth → no shadow).
	Flux_ShaderResourceView& GetCSMArraySRV();
	// All 4 cascade sun view×proj matrices live in one StructuredBuffer<float4x4>
	// (Phase 4a collapse). Casters read ShadowMatrices[cascade] (cascade index via
	// MeshDrawConstants); lit/fog passes read all four.
	Flux_ShaderResourceView_Buffer& GetShadowMatricesSRV() { return m_xShadowMatricesBuffer.GetSRV(); }

	void UpdateShadowMatrices();

	// Per-cascade + global sampling data for the lighting pass. Valid after
	// UpdateShadowMatrices() has run for the current frame.
	const Flux_ShadowCascadeSamplingData& GetCascadeSamplingData() const { return m_xCascadeSamplingData; }
	const Flux_ShadowSamplingConfig&      GetSamplingConfig()      const { return m_xSamplingConfig; }
	// Dynamic CB holding the GPU mirror of the sampling params (bound by the
	// deferred lighting pass as the ShadowSampling constant buffer).
	Flux_DynamicConstantBuffer&           GetShadowSamplingBuffer() { return m_xShadowSamplingBuffer; }

	// One 4-layer depth-array transient holding all cascades (Phase 4b collapse);
	// each cascade pass writes layer u, the lit/fog passes sample it as a
	// Sampler2DArray. Replaces the old m_axCSMHandles[ZENITH_FLUX_NUM_CSMS].
	Flux_TransientHandle       m_xCSMArrayHandle;
	Flux_RenderGraph*          m_pxGraph = nullptr;
	// One StructuredBuffer<float4x4> holding all 4 cascade matrices (Phase 4a).
	Flux_DynamicReadWriteBuffer m_xShadowMatricesBuffer;
	Flux_DynamicConstantBuffer m_xShadowSamplingBuffer;
	Zenith_Maths::Matrix4      m_axSunViewProjMats[ZENITH_FLUX_NUM_CSMS];

private:
	Flux_RenderAttachment& GetCSMArray();

	Flux_ShadowCascadeSamplingData m_xCascadeSamplingData;
	Flux_ShadowSamplingConfig      m_xSamplingConfig;
};
