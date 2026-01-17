#pragma once

#include "Flux/Flux.h"

class Zenith_TextureAsset;

/*
 * Flux_VolumeFog - Shared Infrastructure for Volumetric Fog Techniques
 *
 * Provides common resources used across all volumetric fog rendering techniques:
 * - 3D noise textures (Perlin-Worley combined)
 * - Blue noise texture for temporal dithering
 * - Froxel grid (camera-aligned 3D texture)
 * - History buffers for temporal reprojection
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
	// LPV
	VOLFOG_DEBUG_LPV_INJECTION_POINTS,     // 13: VPL injection locations
	VOLFOG_DEBUG_LPV_PROPAGATION_ITER,     // 14: Light after N propagation steps
	VOLFOG_DEBUG_LPV_CASCADE_BOUNDS,       // 15: Cascade bounding boxes
	VOLFOG_DEBUG_LPV_SH_COEFFICIENTS,      // 16: SH coefficient magnitude
	// Temporal
	VOLFOG_DEBUG_TEMPORAL_MOTION_VECTORS,  // 17: Reprojection motion vectors
	VOLFOG_DEBUG_TEMPORAL_HISTORY_WEIGHT,  // 18: History vs current blend weight
	VOLFOG_DEBUG_TEMPORAL_JITTER_OFFSET,   // 19: Current frame jitter pattern
	VOLFOG_DEBUG_TEMPORAL_DISOCCLUSION,    // 20: Disoccluded pixels (history invalid)
	// God Rays
	VOLFOG_DEBUG_GODRAYS_LIGHT_MASK,       // 21: Light source screen-space mask
	VOLFOG_DEBUG_GODRAYS_OCCLUSION,        // 22: Depth-based occlusion test
	VOLFOG_DEBUG_GODRAYS_RADIAL_WEIGHTS,   // 23: Sample weights along ray
	VOLFOG_DEBUG_MAX
};

// Volumetric fog technique selection
enum VolumetricFogTechnique
{
	VOLFOG_TECHNIQUE_SIMPLE = 0,     // Original exponential fog
	VOLFOG_TECHNIQUE_FROXEL,         // Froxel-based volumetric
	VOLFOG_TECHNIQUE_RAYMARCH,       // Ray marching with noise
	VOLFOG_TECHNIQUE_LPV,            // Light Propagation Volumes
	VOLFOG_TECHNIQUE_TEMPORAL,       // Froxel + temporal reprojection
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
	float m_fPad0;
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

	// History buffer ping-pong
	static Flux_RenderAttachment& GetCurrentHistory();
	static Flux_RenderAttachment& GetPreviousHistory();
	static void SwapHistoryBuffers();

	// Debug output
	static Flux_RenderAttachment& GetDebugOutput() { return s_xDebugOutput; }

	// Configuration
	static Flux_VolumeFogConstants& GetSharedConstants() { return s_xSharedConstants; }
	static Flux_FroxelConfig& GetFroxelConfig() { return s_xFroxelConfig; }

	// Temporal jitter
	static Zenith_Maths::Vector2 GetCurrentJitter();
	static u_int GetJitterIndex() { return s_uJitterIndex; }

private:
	static void GenerateNoiseTexture3D();
	static void GenerateBlueNoiseTexture();
	static void CreateFroxelGrids();
	static void CreateHistoryBuffers();
	static void CreateDebugOutput();
	static void RegisterDebugVariables();

	// Shared textures
	static Zenith_TextureAsset* s_pxNoiseTexture3D;      // 128^3 Perlin-Worley noise
	static Zenith_TextureAsset* s_pxBlueNoiseTexture;    // 64x64 blue noise

	// Froxel grids (3D render targets)
	static Flux_RenderAttachment s_xFroxelDensityGrid;   // RGBA16F: density + scattering
	static Flux_RenderAttachment s_xFroxelLightingGrid;  // RGBA16F: lit color + transmittance

	// Temporal history buffers (ping-pong)
	static Flux_RenderAttachment s_axHistoryBuffers[2];
	static u_int s_uCurrentHistoryIndex;

	// Debug visualization output
	static Flux_RenderAttachment s_xDebugOutput;

	// Shared configuration
	static Flux_VolumeFogConstants s_xSharedConstants;
	static Flux_FroxelConfig s_xFroxelConfig;

	// Temporal jitter
	static u_int s_uJitterIndex;
	static const u_int s_uJitterSequenceLength = 16;
};
