#pragma once

#include "Flux/Flux.h"
#include "Flux/Flux_RenderTargets.h"
#include "AssetHandling/Zenith_AssetHandle.h"
#include "Maths/Zenith_Maths.h"

class Zenith_TextureAsset;

// Debug visualization modes for volumetric fog
enum VolumetricFogDebugMode
{
	VOLFOG_DEBUG_NONE = 0,
	VOLFOG_DEBUG_NOISE_3D_SLICE,
	VOLFOG_DEBUG_BLUE_NOISE,
	VOLFOG_DEBUG_FROXEL_DENSITY_SLICE,
	VOLFOG_DEBUG_FROXEL_DENSITY_MAX,
	VOLFOG_DEBUG_FROXEL_LIGHTING_SLICE,
	VOLFOG_DEBUG_FROXEL_SCATTERING,
	VOLFOG_DEBUG_FROXEL_EXTINCTION,
	VOLFOG_DEBUG_FROXEL_SHADOW_SAMPLES,
	VOLFOG_DEBUG_RAYMARCH_STEP_COUNT,
	VOLFOG_DEBUG_RAYMARCH_ACCUMULATED_DENSITY,
	VOLFOG_DEBUG_RAYMARCH_NOISE_SAMPLE,
	VOLFOG_DEBUG_RAYMARCH_JITTER_PATTERN,
	VOLFOG_DEBUG_GODRAYS_LIGHT_MASK,
	VOLFOG_DEBUG_GODRAYS_OCCLUSION,
	VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS,
	VOLFOG_DEBUG_MAX
};

enum VolumetricFogTechnique
{
	VOLFOG_TECHNIQUE_SIMPLE = 0,
	VOLFOG_TECHNIQUE_FROXEL,
	VOLFOG_TECHNIQUE_RAYMARCH,
	VOLFOG_TECHNIQUE_GODRAYS,
	VOLFOG_TECHNIQUE_MAX
};

struct Flux_VolumeFogConstants
{
	Zenith_Maths::Vector4 m_xFogColour = { 0.5f, 0.6f, 0.7f, 1.0f };
	float m_fDensity = 0.0001f;
	float m_fScatteringCoeff = 0.1f;
	float m_fAbsorptionCoeff = 0.05f;
	float m_fAmbientIrradianceRatio = 0.25f;
	float m_fNoiseWorldScale = 0.01f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };
};

struct Flux_FroxelConfig
{
	u_int m_uGridWidth = 160;
	u_int m_uGridHeight = 90;
	u_int m_uGridDepth = 64;
	float m_fNearSlice = 0.1f;
	float m_fFarSlice = 500.0f;
};

// Phase 9: state + behaviour for VolumeFog (shared fog infra) subsystem.
class Flux_VolumeFogImpl
{
public:
	Flux_VolumeFogImpl() = default;
	~Flux_VolumeFogImpl() = default;

	Flux_VolumeFogImpl(const Flux_VolumeFogImpl&) = delete;
	Flux_VolumeFogImpl& operator=(const Flux_VolumeFogImpl&) = delete;

	void Initialise();

	void ReleaseAssetReferences();

	void Shutdown();
	void Reset();

	Zenith_TextureAsset*    GetNoiseTexture3D();
	Zenith_TextureAsset*    GetBlueNoiseTexture();
	Flux_RenderAttachment&  GetFroxelDensityGrid()  { return m_xFroxelDensityGrid; }
	Flux_RenderAttachment&  GetFroxelLightingGrid() { return m_xFroxelLightingGrid; }

	Flux_RenderAttachment&  GetDebugOutput()        { return m_xDebugOutput; }

	Flux_VolumeFogConstants& GetSharedConstants()   { return m_xSharedConstants; }
	Flux_FroxelConfig&       GetFroxelConfig()      { return m_xFroxelConfig; }

	// Private internals (declared here so the .cpp's member-function
	// definitions can reach them).
	void GenerateNoiseTexture3D();
	void GenerateBlueNoiseTexture();
	void CreateFroxelGrids();
	void CreateDebugOutput();
	void RegisterDebugVariables();

	TextureHandle             m_xNoiseTexture3D;
	TextureHandle             m_xBlueNoiseTexture;
	Flux_RenderAttachment     m_xFroxelDensityGrid;
	Flux_RenderAttachment     m_xFroxelLightingGrid;
	Flux_RenderAttachment     m_xDebugOutput;
	Flux_VolumeFogConstants   m_xSharedConstants;
	Flux_FroxelConfig         m_xFroxelConfig;
};
