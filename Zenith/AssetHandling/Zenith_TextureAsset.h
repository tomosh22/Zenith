#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Flux/Flux.h"

/**
 * Zenith_TextureAsset - GPU texture asset
 *
 * This replaces the old Flux_Texture struct with a proper asset class.
 * Contains both the surface info and GPU resources (VRAM, SRV).
 *
 * Usage:
 *   // Load from file
 *   Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Get().Get<Zenith_TextureAsset>("Assets/tex.ztex");
 *
 *   // Create procedural
 *   Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Get().Create<Zenith_TextureAsset>();
 *   pTex->CreateFromData(pData, xSurfaceInfo);
 */
class Zenith_TextureAsset : public Zenith_Asset
{
public:
	Zenith_TextureAsset();
	~Zenith_TextureAsset();

	// Non-copyable
	Zenith_TextureAsset(const Zenith_TextureAsset&) = delete;
	Zenith_TextureAsset& operator=(const Zenith_TextureAsset&) = delete;

	//--------------------------------------------------------------------------
	// Loading
	//--------------------------------------------------------------------------

	/**
	 * Load texture data from an image file
	 * @param strPath Path to image file (PNG, JPG, etc.)
	 * @param bCreateMips Generate mipmaps
	 * @return true on success
	 */
	bool LoadFromFile(const std::string& strPath, bool bCreateMips = true);

	/**
	 * Create texture from raw data (for procedural textures)
	 * @param pData Pointer to pixel data
	 * @param xSurfaceInfo Surface format and dimensions
	 * @param bCreateMips Generate mipmaps
	 * @return true on success
	 */
	bool CreateFromData(const void* pData, const Flux_SurfaceInfo& xSurfaceInfo, bool bCreateMips = false);

	/**
	 * Create cubemap texture from 6 face images
	 * @param apFaceData Array of 6 face data pointers (PX, NX, PY, NY, PZ, NZ)
	 * @param xSurfaceInfo Surface format and dimensions (for one face)
	 * @return true on success
	 */
	bool CreateCubemap(const void* apFaceData[6], const Flux_SurfaceInfo& xSurfaceInfo);

	/**
	 * Load cubemap from 6 separate texture files
	 * @param szPathPX Path to positive X face
	 * @param szPathNX Path to negative X face
	 * @param szPathPY Path to positive Y face
	 * @param szPathNY Path to negative Y face
	 * @param szPathPZ Path to positive Z face
	 * @param szPathNZ Path to negative Z face
	 * @return true on success
	 */
	bool LoadCubemapFromFiles(
		const char* szPathPX, const char* szPathNX,
		const char* szPathPY, const char* szPathNY,
		const char* szPathPZ, const char* szPathNZ);

	//--------------------------------------------------------------------------
	// Accessors
	//--------------------------------------------------------------------------

	const Flux_SurfaceInfo& GetSurfaceInfo() const { return m_xSurfaceInfo; }
	uint32_t GetWidth() const { return m_xSurfaceInfo.m_uWidth; }
	uint32_t GetHeight() const { return m_xSurfaceInfo.m_uHeight; }
	TextureFormat GetFormat() const { return m_xSurfaceInfo.m_eFormat; }
	TextureType GetType() const { return m_xSurfaceInfo.m_eTextureType; }

	const Flux_ShaderResourceView& GetSRV() const { return m_xSRV; }
	const Flux_VRAMHandle& GetVRAMHandle() const { return m_xVRAMHandle; }

	bool IsValid() const { return m_xVRAMHandle.IsValid(); }

	//--------------------------------------------------------------------------
	// GPU Resources
	//--------------------------------------------------------------------------

	/**
	 * Release GPU resources
	 * Called automatically in destructor, but can be called manually
	 */
	void ReleaseGPU();

	//--------------------------------------------------------------------------
	// GPU Data (public for renderer access)
	//--------------------------------------------------------------------------

	Flux_SurfaceInfo m_xSurfaceInfo;
	Flux_VRAMHandle m_xVRAMHandle;
	Flux_ShaderResourceView m_xSRV;

private:
	bool m_bGPUResourcesAllocated = false;
};

//--------------------------------------------------------------------------
// Register loader with asset registry
//--------------------------------------------------------------------------

void Zenith_TextureAsset_RegisterLoader();
