#include "Zenith.h"

#include "Flux/Fog/Flux_VolumeFog.h"

#include "Flux/Flux_Graphics.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "DebugVariables/Zenith_DebugVariables.h"
#include "Maths/Zenith_Maths.h"

#include <cmath>
#include <random>
#include <vector>

// Noise generation helpers
namespace
{
	// Simple hash function for noise
	float Hash(int n)
	{
		n = (n << 13) ^ n;
		return 1.0f - ((n * (n * n * 15731 + 789221) + 1376312589) & 0x7fffffff) / 1073741824.0f;
	}

	// 3D gradient noise (simplified Perlin)
	float GradientNoise3D(float x, float y, float z)
	{
		int ix = static_cast<int>(std::floor(x));
		int iy = static_cast<int>(std::floor(y));
		int iz = static_cast<int>(std::floor(z));

		float fx = x - ix;
		float fy = y - iy;
		float fz = z - iz;

		// Smooth interpolation
		float ux = fx * fx * (3.0f - 2.0f * fx);
		float uy = fy * fy * (3.0f - 2.0f * fy);
		float uz = fz * fz * (3.0f - 2.0f * fz);

		// Hash corners
		auto hash = [](int x, int y, int z) {
			return Hash(x + y * 57 + z * 113);
		};

		// Trilinear interpolation
		float n000 = hash(ix, iy, iz);
		float n100 = hash(ix + 1, iy, iz);
		float n010 = hash(ix, iy + 1, iz);
		float n110 = hash(ix + 1, iy + 1, iz);
		float n001 = hash(ix, iy, iz + 1);
		float n101 = hash(ix + 1, iy, iz + 1);
		float n011 = hash(ix, iy + 1, iz + 1);
		float n111 = hash(ix + 1, iy + 1, iz + 1);

		float nx00 = n000 * (1 - ux) + n100 * ux;
		float nx10 = n010 * (1 - ux) + n110 * ux;
		float nx01 = n001 * (1 - ux) + n101 * ux;
		float nx11 = n011 * (1 - ux) + n111 * ux;

		float nxy0 = nx00 * (1 - uy) + nx10 * uy;
		float nxy1 = nx01 * (1 - uy) + nx11 * uy;

		return nxy0 * (1 - uz) + nxy1 * uz;
	}

	// Fractal Brownian Motion noise
	float FBMNoise3D(float x, float y, float z, int octaves = 4)
	{
		float value = 0.0f;
		float amplitude = 1.0f;
		float frequency = 1.0f;
		float maxValue = 0.0f;

		for (int i = 0; i < octaves; i++)
		{
			value += amplitude * GradientNoise3D(x * frequency, y * frequency, z * frequency);
			maxValue += amplitude;
			amplitude *= 0.5f;
			frequency *= 2.0f;
		}

		return value / maxValue;
	}
}

// Static member definitions
// Note: Use {} initialization to trigger default member initializers
Zenith_TextureAsset* Flux_VolumeFog::s_pxNoiseTexture3D = nullptr;
Zenith_TextureAsset* Flux_VolumeFog::s_pxBlueNoiseTexture = nullptr;
Flux_RenderAttachment Flux_VolumeFog::s_xFroxelDensityGrid;
Flux_RenderAttachment Flux_VolumeFog::s_xFroxelLightingGrid;
Flux_RenderAttachment Flux_VolumeFog::s_xDebugOutput;
Flux_VolumeFogConstants Flux_VolumeFog::s_xSharedConstants{};
Flux_FroxelConfig Flux_VolumeFog::s_xFroxelConfig{};

void Flux_VolumeFog::Initialise()
{
	// Generate shared textures
	GenerateNoiseTexture3D();
	GenerateBlueNoiseTexture();

#ifdef ZENITH_DEBUG_VARIABLES
	// Master controls
	Zenith_DebugVariables::AddVector3({ "Render", "Volumetric Fog", "Shared", "Colour" }, s_xSharedConstants.m_xFogColour, 0.f, 1.f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Shared", "Density" }, s_xSharedConstants.m_fDensity, 0.f, 0.01f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Shared", "Scattering" }, s_xSharedConstants.m_fScatteringCoeff, 0.f, 1.f);
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Shared", "Absorption" }, s_xSharedConstants.m_fAbsorptionCoeff, 0.f, 1.f);
	// Ambient irradiance ratio: fraction of sky light vs direct sun contribution to fog
	// Physical basis: Clear sky ~0.15-0.25, overcast ~0.4-0.6
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Shared", "Ambient Irradiance Ratio" }, s_xSharedConstants.m_fAmbientIrradianceRatio, 0.f, 1.f);
	// Noise world scale: maps world-space coordinates to noise texture UV
	// Smaller values = larger fog features, larger values = denser noise detail
	Zenith_DebugVariables::AddFloat({ "Render", "Volumetric Fog", "Shared", "Noise World Scale" }, s_xSharedConstants.m_fNoiseWorldScale, 0.001f, 0.1f);
#endif

	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_VolumeFog initialised");
}

void Flux_VolumeFog::Shutdown()
{
	Zenith_Log(LOG_CATEGORY_RENDERER, "Flux_VolumeFog shutdown");
}

void Flux_VolumeFog::Reset()
{
	// Spatial-only fog - no history buffers to reset
}

void Flux_VolumeFog::GenerateNoiseTexture3D()
{
	constexpr u_int uSize = 64;  // 64^3 texture
	constexpr u_int uNumPixels = uSize * uSize * uSize;

	// Allocate RGBA8 data
	u_int8* pData = static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(uNumPixels * 4));

	// Generate FBM noise
	for (u_int z = 0; z < uSize; z++)
	{
		for (u_int y = 0; y < uSize; y++)
		{
			for (u_int x = 0; x < uSize; x++)
			{
				// Normalize coordinates to 0-1 and scale for noise frequency
				float fx = static_cast<float>(x) / uSize * 4.0f;
				float fy = static_cast<float>(y) / uSize * 4.0f;
				float fz = static_cast<float>(z) / uSize * 4.0f;

				// Generate FBM noise, map from [-1,1] to [0,1]
				float noise = (FBMNoise3D(fx, fy, fz, 4) + 1.0f) * 0.5f;

				// Clamp and convert to byte
				u_int8 uValue = static_cast<u_int8>(std::clamp(noise * 255.0f, 0.0f, 255.0f));

				u_int uIndex = (z * uSize * uSize + y * uSize + x) * 4;
				pData[uIndex + 0] = uValue;  // R
				pData[uIndex + 1] = uValue;  // G
				pData[uIndex + 2] = uValue;  // B
				pData[uIndex + 3] = 255;     // A
			}
		}
	}

	// Create texture via registry
	Flux_SurfaceInfo xSurfaceInfo;
	xSurfaceInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_3D;
	xSurfaceInfo.m_uWidth = uSize;
	xSurfaceInfo.m_uHeight = uSize;
	xSurfaceInfo.m_uDepth = uSize;
	xSurfaceInfo.m_uNumMips = 1;
	xSurfaceInfo.m_uNumLayers = 1;
	xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	s_pxNoiseTexture3D = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	s_pxNoiseTexture3D->CreateFromData(pData, xSurfaceInfo, false);

	Zenith_MemoryManagement::Deallocate(pData);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Generated 3D noise texture (%ux%ux%u)", uSize, uSize, uSize);
}

void Flux_VolumeFog::GenerateBlueNoiseTexture()
{
	constexpr u_int uSize = 64;  // 64x64 texture
	constexpr u_int uNumPixels = uSize * uSize;

	// Allocate RGBA8 data
	u_int8* pData = static_cast<u_int8*>(Zenith_MemoryManagement::Allocate(uNumPixels * 4));

	// Generate blue noise using simple void-and-cluster approximation
	// For actual production, you'd load a precomputed blue noise texture
	std::mt19937 rng(42);  // Fixed seed for reproducibility
	std::uniform_real_distribution<float> dist(0.0f, 1.0f);

	// Initialize with white noise
	for (u_int i = 0; i < uNumPixels; i++)
	{
		u_int8 uValue = static_cast<u_int8>(dist(rng) * 255.0f);
		pData[i * 4 + 0] = uValue;
		pData[i * 4 + 1] = uValue;
		pData[i * 4 + 2] = uValue;
		pData[i * 4 + 3] = 255;
	}

	// Simple spatial filtering to approximate blue noise characteristics
	// This applies a high-pass filter to reduce low-frequency content
	std::vector<float> fTemp(uNumPixels);
	for (u_int y = 0; y < uSize; y++)
	{
		for (u_int x = 0; x < uSize; x++)
		{
			float fSum = 0.0f;
			float fCenter = pData[(y * uSize + x) * 4] / 255.0f;

			// Sample neighbors with wrapping
			for (int dy = -1; dy <= 1; dy++)
			{
				for (int dx = -1; dx <= 1; dx++)
				{
					u_int nx = (x + dx + uSize) % uSize;
					u_int ny = (y + dy + uSize) % uSize;
					fSum += pData[(ny * uSize + nx) * 4] / 255.0f;
				}
			}

			// High-pass: center - average
			float fAvg = fSum / 9.0f;
			fTemp[y * uSize + x] = fCenter + (fCenter - fAvg) * 0.5f;
		}
	}

	// Write back normalized
	for (u_int i = 0; i < uNumPixels; i++)
	{
		u_int8 uValue = static_cast<u_int8>(std::clamp(fTemp[i] * 255.0f, 0.0f, 255.0f));
		pData[i * 4 + 0] = uValue;
		pData[i * 4 + 1] = uValue;
		pData[i * 4 + 2] = uValue;
	}

	// Create texture via registry
	Flux_SurfaceInfo xSurfaceInfo;
	xSurfaceInfo.m_eFormat = TEXTURE_FORMAT_RGBA8_UNORM;
	xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_2D;
	xSurfaceInfo.m_uWidth = uSize;
	xSurfaceInfo.m_uHeight = uSize;
	xSurfaceInfo.m_uDepth = 1;
	xSurfaceInfo.m_uNumMips = 1;
	xSurfaceInfo.m_uNumLayers = 1;
	xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	s_pxBlueNoiseTexture = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
	s_pxBlueNoiseTexture->CreateFromData(pData, xSurfaceInfo, false);

	Zenith_MemoryManagement::Deallocate(pData);

	Zenith_Log(LOG_CATEGORY_RENDERER, "Generated blue noise texture (%ux%u)", uSize, uSize);
}

void Flux_VolumeFog::CreateFroxelGrids()
{
	// STUB - not implemented
}

void Flux_VolumeFog::CreateDebugOutput()
{
	// STUB - not implemented
}

void Flux_VolumeFog::RegisterDebugVariables()
{
	// Done in Initialise
}
