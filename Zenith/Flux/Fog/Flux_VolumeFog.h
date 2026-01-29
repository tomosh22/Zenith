#pragma once

#include "Flux/Flux.h"

class Zenith_TextureAsset;

/*
 * Flux_VolumeFog - Shared Infrastructure for Volumetric Fog Techniques
 *
 * Provides common resources used across all volumetric fog rendering techniques:
 * - 3D noise textures (Perlin-Worley combined)
 * - Blue noise texture for spatial dithering
 * - Froxel grid (camera-aligned 3D texture)
 *
 * All techniques are spatial-only (no temporal effects, history buffers, or reprojection).
 *
 * Debug Modes: 1-2 (noise 3D slice, blue noise)
 */

// Debug visualization modes for volumetric fog
enum VolumetricFogDebugMode
{
	VOLFOG_DEBUG_NONE = 0,
	// Shared
	VOLFOG_DEBUG_NOISE_3D_SLICE,           // 1: Visualize 3D noise texture slice
	VOLFOG_DEBUG_BLUE_NOISE,               // 2: Show blue noise texture
	// Froxel
	VOLFOG_DEBUG_FROXEL_DENSITY_SLICE,     // 3: Single Z-slice of density grid
	VOLFOG_DEBUG_FROXEL_DENSITY_MAX,       // 4: Max projection of density through Z
	VOLFOG_DEBUG_FROXEL_LIGHTING_SLICE,    // 5: Single Z-slice of lit froxels
	VOLFOG_DEBUG_FROXEL_SCATTERING,        // 6: In-scattering amount per froxel
	VOLFOG_DEBUG_FROXEL_EXTINCTION,        // 7: Extinction/transmittance per froxel
	VOLFOG_DEBUG_FROXEL_SHADOW_SAMPLES,    // 8: Shadow map sampling visualization
	// Raymarch
	VOLFOG_DEBUG_RAYMARCH_STEP_COUNT,      // 9: Heat map of steps taken per pixel
	VOLFOG_DEBUG_RAYMARCH_ACCUMULATED_DENSITY, // 10: Density before lighting
	VOLFOG_DEBUG_RAYMARCH_NOISE_SAMPLE,    // 11: Raw noise values sampled
	VOLFOG_DEBUG_RAYMARCH_JITTER_PATTERN,  // 12: Blue noise jitter offsets
	// God Rays
	VOLFOG_DEBUG_GODRAYS_LIGHT_MASK,       // 13: Light source screen-space mask
	VOLFOG_DEBUG_GODRAYS_OCCLUSION,        // 14: Depth-based occlusion test
	VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS,   // 15: Sample weights along ray
	VOLFOG_DEBUG_MAX
};

// Volumetric fog technique selection (all spatial-only, no temporal effects)
enum VolumetricFogTechnique
{
	VOLFOG_TECHNIQUE_SIMPLE = 0,     // Original exponential fog
	VOLFOG_TECHNIQUE_FROXEL,         // Froxel-based volumetric
	VOLFOG_TECHNIQUE_RAYMARCH,       // Ray marching with noise
	VOLFOG_TECHNIQUE_GODRAYS,        // Screen-space god rays
	VOLFOG_TECHNIQUE_MAX
};

// Shared constants for volumetric fog
struct Flux_VolumeFogConstants
{
	Zenith_Maths::Vector4 m_xFogColour = { 0.5f, 0.6f, 0.7f, 1.0f };
	float m_fDensity = 0.0001f;
	float m_fScatteringCoeff = 0.1f;
	float m_fAbsorptionCoeff = 0.05f;
	// Ambient irradiance ratio: fraction of sky light vs direct sun contribution to fog
	// Physical basis: Clear sky ambient ~0.15-0.25, overcast ~0.4-0.6
	// Use 0.25 as balanced default for typical outdoor scenes
	float m_fAmbientIrradianceRatio = 0.25f;
	// Noise coordinate scale: Maps world-space to noise texture UV
	// 0.01 = fog features ~100 units wide (suitable for large outdoor scenes)
	// Smaller values = larger fog features, larger values = smaller/denser noise detail
	// Note: Shaders should read from uniform buffer; VolumetricCommon.fxh has fallback const
	float m_fNoiseWorldScale = 0.01f;
	float m_fPad[3] = { 0.f, 0.f, 0.f };  // Padding for std140 alignment
};

// Froxel grid configuration
struct Flux_FroxelConfig
{
	u_int m_uGridWidth = 160;
	u_int m_uGridHeight = 90;
	u_int m_uGridDepth = 64;
	float m_fNearSlice = 0.1f;
	float m_fFarSlice = 500.0f;
};

class Flux_VolumeFog
{
public:
	static void Initialise();
	static void Shutdown();
	static void Reset();  // Clear state when scene resets

	// Shared resources accessors
	static Zenith_TextureAsset* GetNoiseTexture3D() { return s_pxNoiseTexture3D; }
	static Zenith_TextureAsset* GetBlueNoiseTexture() { return s_pxBlueNoiseTexture; }
	static Flux_RenderAttachment& GetFroxelDensityGrid() { return s_xFroxelDensityGrid; }
	static Flux_RenderAttachment& GetFroxelLightingGrid() { return s_xFroxelLightingGrid; }

	// Debug output
	static Flux_RenderAttachment& GetDebugOutput() { return s_xDebugOutput; }

	// Configuration
	static Flux_VolumeFogConstants& GetSharedConstants() { return s_xSharedConstants; }
	static Flux_FroxelConfig& GetFroxelConfig() { return s_xFroxelConfig; }

private:
	static void GenerateNoiseTexture3D();
	static void GenerateBlueNoiseTexture();
	static void CreateFroxelGrids();
	static void CreateDebugOutput();
	static void RegisterDebugVariables();

	// Shared textures
	static Zenith_TextureAsset* s_pxNoiseTexture3D;      // 64^3 Perlin-Worley noise
	static Zenith_TextureAsset* s_pxBlueNoiseTexture;    // 64x64 blue noise

	// Froxel grids (3D render targets)
	static Flux_RenderAttachment s_xFroxelDensityGrid;   // RGBA16F: density + scattering
	static Flux_RenderAttachment s_xFroxelLightingGrid;  // RGBA16F: lit color + transmittance

	// Debug visualization output
	static Flux_RenderAttachment s_xDebugOutput;

	// Shared configuration
	static Flux_VolumeFogConstants s_xSharedConstants;
	static Flux_FroxelConfig s_xFroxelConfig;
};
