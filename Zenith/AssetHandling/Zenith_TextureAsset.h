#pragma once

#include "AssetHandling/Zenith_Asset.h"
#include "Flux/Flux.h"

#include <vector>

class Zenith_DataStream;

// .ztxtr envelope identity — shared by the loader (Zenith_TextureAsset) and the
// exporter (Zenith_Tools_TextureExport) so the asset-type-id and the current
// payload schema version have ONE definition. Schema <=1 == legacy single-mip;
// schema 2 == uNumMips field + a packed full mip chain (see Zenith_TextureAsset.cpp).
inline constexpr u_int uZENITH_TEXTURE_ASSET_TYPE_ID = 1;
inline constexpr u_int uZENITH_TEXTURE_SCHEMA_V2 = 2;

/**
 * Zenith_TextureAsset - GPU texture asset
 *
 * This replaces the old Flux_Texture struct with a proper asset class.
 * Contains both the surface info and GPU resources (VRAM, SRV).
 *
 * Usage:
 *   // Load from file
 *   Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Get<Zenith_TextureAsset>("Assets/tex.ztxtr");
 *
 *   // Create procedural
 *   Zenith_TextureAsset* pTex = Zenith_AssetRegistry::Create<Zenith_TextureAsset>();
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

	/**
	 * Load a .ztxtr's pixel data into a CPU buffer WITHOUT any GPU upload.
	 * Shares the single .ztxtr parser with LoadFromFile (legacy/v1/v2 aware),
	 * so non-renderer consumers (e.g. the RenderTest roughness/metallic packer,
	 * the terrain heightmap readers) must use this instead of hand-parsing the
	 * file — there is exactly one .ztxtr parser engine-wide. Safe in tools /
	 * headless (no device required).
	 * @param strPath    Path to the .ztxtr file.
	 * @param xOutInfo    Receives format/dims and m_uNumMips as stored in the file
	 *                    (1 for legacy single-mip files, N for a v2 mip chain).
	 * @param xOutBytes   Receives the stored bytes: mip 0 for legacy, or the full
	 *                    chain packed mip0..mipN-1 for v2 (mip M starts at the
	 *                    prefix sum of CalculateMipDataSize for mips < M).
	 * @return SUCCESS, or an error code (FILE_NOT_FOUND / CORRUPT_DATA / ...).
	 */
	static Zenith_Status LoadCPUData(const std::string& strPath, Flux_SurfaceInfo& xOutInfo, std::vector<uint8_t>& xOutBytes);

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

	bool IsMarkedBindless() const { return (m_xSurfaceInfo.m_uMemoryFlags & (1 << MEMORY_FLAGS__BINDLESS)) != 0; }

	/**
	 * Mark this texture for bindless access
	 * Registers the SRV in the bindless descriptor set so it can be
	 * indexed by g_axBindlessTextures[handle] in shaders
	 */
	void MarkAsBindless();

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
	friend class Zenith_AssetRegistry;
	friend Zenith_Result<Zenith_Asset*> LoadTextureAsset(const std::string&);

	/**
	 * Load texture data from an image file (private - use Zenith_AssetRegistry::Get)
	 * @param strPath Path to image file (PNG, JPG, etc.)
	 * @param bCreateMips Generate mipmaps
	 * @return SUCCESS, or an error code on failure
	 */
	Zenith_Status LoadFromFile(const std::string& strPath, bool bCreateMips = true);

	// The single .ztxtr parser. Reads the envelope (if any) + header, then either
	// the legacy single-mip payload or the v2 multi-mip chain (strictly validated),
	// into xOutBytes. Sets xOutInfo (format/dims/type/layers + m_uNumMips = mips
	// actually present in the file) and bOutIsV2 (true => xOutBytes holds a packed
	// chain). Used by both LoadFromFile (then GPU-uploads) and LoadCPUData.
	static Zenith_Status ParseZtxtr(const std::string& strPath, Zenith_DataStream& xStream,
		Flux_SurfaceInfo& xOutInfo, std::vector<uint8_t>& xOutBytes, bool& bOutIsV2);

	bool m_bGPUResourcesAllocated = false;
};

//--------------------------------------------------------------------------
// Register loader with asset registry
//--------------------------------------------------------------------------

void Zenith_TextureAsset_RegisterLoader();
