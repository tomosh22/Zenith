#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_Buffers.h"
#include "Flux/Flux_RenderTargets.h"
#include "Flux/RenderGraph/Flux_RenderGraph.h"
#include "Core/Zenith_EnvironmentAuthority.h"
#include "AssetHandling/Zenith_AssetHandle.h"

namespace AtmosphereConfig
{
	constexpr float fEARTH_RADIUS = 6360000.0f;
	constexpr float fATMOSPHERE_RADIUS = 6420000.0f;
	constexpr float fATMOSPHERE_HEIGHT = fATMOSPHERE_RADIUS - fEARTH_RADIUS;

	constexpr float afRAYLEIGH_SCATTER[3] = { 5.8e-6f, 13.5e-6f, 33.1e-6f };
	constexpr float fRAYLEIGH_SCALE_HEIGHT = 8000.0f;

	constexpr float fMIE_SCATTER = 3.996e-6f;
	constexpr float fMIE_ABSORB = 4.4e-6f;
	constexpr float fMIE_SCALE_HEIGHT = 1200.0f;
	constexpr float fMIE_G = 0.76f;

	constexpr float fSUN_ANGULAR_RADIUS = 0.00935f;
	// ★ THE ENGINE'S ONE RADIOMETRIC ANCHOR: top-of-atmosphere solar
	// irradiance, in engine linear-HDR units. Everything luminous derives from
	// it — the sky (single-scatter inscatter * this), the IBL irradiance +
	// prefilter cubes (convolved from that sky, including the virtual-ground
	// bounce), and the direct sun key (this * per-channel transmittance, see
	// Flux_AtmosphereTransmittance.h). Its ABSOLUTE value is a unit choice
	// (exposure normalises it away; only ratios are physical); changing it
	// rescales every absolute HDR constant in the tree (bloom threshold,
	// exposure clamps, histogram domain — Flux_HDRImpl derives its histogram
	// top bin from this). It is NOT a look knob: do not touch it to brighten
	// or darken a scene — that is the exposure system's job.
	//
	// TODO(exposure-locality): because the anchor is fixed and there is
	// deliberately no look override, EVERY "make this area brighter" request
	// routes through the GLOBAL auto-exposure -- a region cannot be lifted
	// without changing the metering everywhere. The principled fix is LOCAL
	// exposure (a spatially varying exposure term in Flux/HDR), NOT a per-scene
	// anchor: an anchor knob would re-break the sun/sky/ambient agreement this
	// constant exists to guarantee.
	constexpr float fSUN_INTENSITY = 7.0f;

	constexpr u_int uDEFAULT_SKY_SAMPLES = 16;
	constexpr u_int uDEFAULT_LIGHT_SAMPLES = 8;

	constexpr u_int uTRANSMITTANCE_LUT_WIDTH = 256;
	constexpr u_int uTRANSMITTANCE_LUT_HEIGHT = 64;

	// Hillaire multiple-scattering LUT. Tiny (32x32) because the quantity is
	// smooth in both axes (altitude, sun-zenith cosine); the cost is in the
	// per-texel sphere integral, not the resolution. Baked on the same cadence
	// as the transmittance LUT (medium changes only), never per frame.
	constexpr u_int uMULTISCATTER_LUT_SIZE = 32;

	// Low-res sky-view LUT (lat-long). The per-frame raymarch runs at this
	// resolution instead of full screen; the fullscreen sky pass samples it.
	constexpr u_int uSKYVIEW_LUT_WIDTH = 192;
	constexpr u_int uSKYVIEW_LUT_HEIGHT = 108;
}

// Solid colour override constants
struct SkyboxOverrideConstants
{
	Zenith_Maths::Vector4 m_xColour;
};

// Atmosphere constants buffer structure
struct AtmosphereConstants
{
	// Scattering coefficients
	Zenith_Maths::Vector4 m_xRayleighScatter;      // RGB coefficients, W = scale height
	Zenith_Maths::Vector4 m_xMieScatter;           // RGB = scatter, W = scale height

	// Planet parameters
	float m_fPlanetRadius;
	float m_fAtmosphereRadius;
	float m_fMieG;                                   // Henyey-Greenstein asymmetry
	float m_fSunIntensity;

	// Configuration
	float m_fRayleighScale;
	float m_fMieScale;
	u_int m_uDebugMode;

	// Ray march settings
	u_int m_uSkySamples;
	u_int m_uLightSamples;
	// 1 = fold the Hillaire multiple-scattering LUT into the sky integral.
	u_int m_uMultiScatteringEnabled;
	float m_fPad;
};

enum Skybox_DebugMode : u_int
{
	SKYBOX_DEBUG_NONE,
	SKYBOX_DEBUG_RAYLEIGH_ONLY,
	SKYBOX_DEBUG_MIE_ONLY,
	SKYBOX_DEBUG_TRANSMITTANCE,
	SKYBOX_DEBUG_SCATTER_DIRECTION,
	SKYBOX_DEBUG_SUN_DISK,
	SKYBOX_DEBUG_LUT_PREVIEW,
	SKYBOX_DEBUG_RAY_STEPS,
	SKYBOX_DEBUG_PHASE_FUNCTION,
	SKYBOX_DEBUG_COUNT
};

// Phase 9: state + behaviour for Skybox subsystem.
//
// Cross-subsystem dependencies (FluxGraphics for the fullscreen quad + frame
// constants + MRT and final-RT attachments, VulkanMemory for the constant-buffer
// + upload paths) are reached via g_xEngine at point of use. The non-capturing
// fn-pointer trampolines (the Execute*/PreExecuteSkybox graph callbacks and
// the ZENITH_TOOLS hot-reload callback) cannot capture state, so they re-enter
// via g_xEngine.Skybox() to reach this singleton instance.
class Flux_SkyboxImpl
{
public:
	Flux_SkyboxImpl() = default;
	~Flux_SkyboxImpl() = default;

	Flux_SkyboxImpl(const Flux_SkyboxImpl&) = delete;
	Flux_SkyboxImpl& operator=(const Flux_SkyboxImpl&) = delete;

	void Initialise();
	void ReleaseAssetReferences();
	void Shutdown();
	void Reset();
	void BuildPipelines();

	void SetupRenderGraph(Flux_RenderGraph& xGraph);

	// Toggles the transmittance-LUT generation pass per frame. Called from
	// Flux_RendererImpl::ApplySubsystemGraphSelections BEFORE Compile (like IBL):
	// force-enables the writer on a dirty compile so the validator sees a writer
	// for the LUT the Skybox pass reads, and re-enables it when atmosphere params
	// that affect transmittance change.
	void UpdateGraphPassEnables(Flux_RenderGraph& xGraph);

	// Consume the resolved scene environment. Drives the atmosphere-model
	// members from the single authoritative environment snapshot (sun geometry
	// is consumed by Flux_Graphics; this carries the atmosphere medium). The
	// radiometric anchor (m_fSunIntensity) is POLICY and is intentionally NOT
	// touched here -- it is never scene-authored. Changing the scales here
	// lets UpdateGraphPassEnables detect the LUT invalidation next compile.
	void ApplyEnvironment(const Zenith_EnvironmentAuthorityData& xEnv);

	bool IsAtmosphereEnabled() const;
	// The radiometric anchor exposed read-only. It is the engine's single
	// light-energy source (AtmosphereConfig::fSUN_INTENSITY) -- never mutated
	// by gameplay or scene authoring; exposure handles presentation brightness.
	float GetSunIntensity() const                       { return m_fSunIntensity; }
	float GetRayleighScale() const                      { return m_fRayleighScale; }
	float GetMieScale() const                           { return m_fMieScale; }
	float GetMieG() const                               { return m_fMieG; }
	float GetRayleighScaleHeight() const                { return m_fRayleighScaleHeight; }
	float GetMieScaleHeight() const                     { return m_fMieScaleHeight; }
	// Multiple scattering is a QUALITY option, not a scene property: it changes
	// how completely the same medium is integrated, not what the medium is.
	static bool IsMultiScatteringEnabled();

	Flux_ShaderResourceView& GetTransmittanceLUTSRV();
	Flux_ShaderResourceView& GetMultiScatterLUTSRV();

#ifdef ZENITH_DEBUG_VARIABLES
	void RegisterDebugVariables();
#endif

	void CreateRenderTargets();
	void DestroyRenderTargets();

	TextureHandle              m_xCubemapTexture;

	Flux_RenderAttachment      m_xTransmittanceLUT;
	// Hillaire multiple-scattering LUT. Same invalidation cadence as the
	// transmittance LUT (both depend only on the medium), and written by the
	// same "LUT needs update" flag so the pair can never disagree about which
	// atmosphere they describe.
	Flux_RenderAttachment      m_xMultiScatterLUT;
	bool                       m_bLUTNeedsUpdate = true;

	// Sky-view LUT. Persistent (not a graph transient): the "Skybox" pass reads
	// it unconditionally (so no mode-change rebuild), and in cubemap/solid mode
	// the sky-view writer is disabled in steady state while the read still needs
	// a stably-allocated target. ~192x108 RGBA16F (~166 KB) — trivial VRAM.
	Flux_RenderAttachment      m_xSkyViewLUT;

	// Preview-view sky-view LUT (S5c). The LUT is camera+sun dependent and the
	// preview view owns its OWN sun, so it cannot share the main LUT. Same
	// dims/format; persistent for the same reasons as m_xSkyViewLUT (and cheap
	// enough to build unconditionally rather than churn on preview toggles).
	// Written by "Skybox Sky-View LUT (Preview)" (whose .View(preview) makes the
	// shader read the PREVIEW slot's g_xView sun); sampled by "Skybox (Preview)".
	Flux_RenderAttachment      m_xPreviewSkyViewLUT;

	Flux_Pipeline              m_xCubemapPipeline;
	Flux_Pipeline              m_xAtmospherePipeline;
	Flux_Pipeline              m_xSolidColourPipeline;
	Flux_Pipeline              m_xTransmittanceLUTPipeline;
	Flux_Pipeline              m_xMultiScatterLUTPipeline;
	Flux_Pipeline              m_xSkyViewLUTPipeline;
	// TAA sky motion vectors. ONE pipeline for all three sky shading modes (the
	// velocity of a point at infinity depends only on the view ray), built
	// unconditionally and bound only by the "Skybox Velocity" pass, which exists
	// only while the velocity latch is on.
	Flux_Pipeline              m_xVelocityPipeline;

	Flux_Shader                m_xCubemapShader;
	Flux_Shader                m_xAtmosphereShader;
	Flux_Shader                m_xSolidColourShader;
	Flux_Shader                m_xTransmittanceLUTShader;
	Flux_Shader                m_xMultiScatterLUTShader;
	Flux_Shader                m_xSkyViewLUTShader;
	Flux_Shader                m_xVelocityShader;

	// Transmittance-LUT generation pass. Enabled only when the LUT needs a
	// refresh (m_bLUTNeedsUpdate); the graph floats this handle's enable bit via
	// UpdateGraphPassEnables. Cached scales detect the param changes that
	// invalidate the LUT (Rayleigh/Mie scale only — NOT sun intensity / Mie-G /
	// sky samples, which don't affect transmittance).
	Flux_PassHandle            m_xTransmittanceLUTPassHandle = {};
	Flux_PassHandle            m_xMultiScatterLUTPassHandle  = {};
	Flux_PassHandle            m_xSkyViewLUTPassHandle       = {};
	// Preview sky-view LUT writer. Reset to invalid at the top of every
	// SetupRenderGraph and (re)assigned only while the preview view is active
	// that compile — UpdateGraphPassEnables gates on IsValid(), so it never
	// touches a stale handle after the preview view deactivates.
	Flux_PassHandle            m_xPreviewSkyViewLUTPassHandle = {};
	float                      m_fLastLUTRayleighScale      = 1.0f;
	float                      m_fLastLUTMieScale           = 1.0f;
	float                      m_fLastLUTRayleighScaleHeight = AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT;
	float                      m_fLastLUTMieScaleHeight     = AtmosphereConfig::fMIE_SCALE_HEIGHT;
	float                      m_fLastLUTGroundAlbedo       = 0.25f;

	Flux_DynamicConstantBuffer m_xAtmosphereConstantsBuffer;
	Flux_DynamicConstantBuffer m_xSolidColourConstantsBuffer;

	// CB scratch (formerly module-scope statics in Flux_Skybox.cpp). Filled in
	// Initialise (for initial sizing/fill) and per-frame in PreExecuteSkybox.
	AtmosphereConstants        m_xAtmosphereConstants;
	SkyboxOverrideConstants    m_xSolidColourConstants;

	float                      m_fSunIntensity              = AtmosphereConfig::fSUN_INTENSITY;
	float                      m_fRayleighScale             = 1.0f;
	float                      m_fMieScale                  = 1.0f;
	float                      m_fMieG                      = AtmosphereConfig::fMIE_G;
	float                      m_fRayleighScaleHeight       = AtmosphereConfig::fRAYLEIGH_SCALE_HEIGHT;
	float                      m_fMieScaleHeight            = AtmosphereConfig::fMIE_SCALE_HEIGHT;
	// Capture-only in the sky sense (the visible sky pass uses 0), but the
	// multiple-scattering bake needs it: the ground bounce is a real part of the
	// multiply-scattered field.
	float                      m_fGroundAlbedo              = 0.25f;
};
