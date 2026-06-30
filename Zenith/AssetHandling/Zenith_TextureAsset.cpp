#include "Zenith.h"
#include "Profiling/Zenith_Profiling.h"
#include "Core/Zenith_Engine.h"
#include "AssetHandling/Zenith_TextureAsset.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "DataStream/Zenith_StreamEnvelope.h"
#include "Flux/Flux_GraphicsImpl.h"
#include "Flux/Flux_GraphicsImpl.h"

// Asset-type id for the .ztxtr envelope. No engine-wide numeric asset-type-id
// enum exists (assets are keyed by string type name), so a small local constant
// suffices — it only needs to be stable within the texture read/write pair.
// Current contract: the exporter (Tools/Zenith_Tools_TextureExport.cpp ExportV2)
// emits the envelope at schema 2 (uNumMips + a packed full mip chain), which
// ParseZtxtr reads via its schema>=2 branch. Schema <=1 and pre-envelope
// (headerless) files still load as single-mip via the legacy branch
// (Zenith_ReadStreamHeader returns BAD_MAGIC and restores the cursor).
// Envelope identity is shared with the exporter via Zenith_TextureAsset.h
// (uZENITH_TEXTURE_ASSET_TYPE_ID / uZENITH_TEXTURE_SCHEMA_V2). Local aliases keep
// the existing call sites terse.
static constexpr u_int uTEXTURE_ASSET_TYPE_ID = uZENITH_TEXTURE_ASSET_TYPE_ID;
static constexpr u_int uTEXTURE_SCHEMA_VERSION_V2 = uZENITH_TEXTURE_SCHEMA_V2;

// Unified data size calculation for both compressed and uncompressed textures
static size_t CalculateTextureDataSize(TextureFormat eFormat, uint32_t uWidth, uint32_t uHeight, uint32_t uDepth = 1)
{
	if (IsCompressedFormat(eFormat))
	{
		return CalculateCompressedTextureSize(eFormat, uWidth, uHeight);
	}
	return static_cast<size_t>(ColourFormatBytesPerPixel(eFormat)) * uWidth * uHeight * uDepth;
}

Zenith_TextureAsset::Zenith_TextureAsset()
{
	m_xSurfaceInfo = Flux_SurfaceInfo();
	m_xVRAMHandle = Flux_VRAMHandle();
	m_xSRV = Flux_ShaderResourceView();
}

Zenith_TextureAsset::~Zenith_TextureAsset()
{
	ReleaseGPU();
}

void Zenith_TextureAsset::MarkAsBindless(bool bRepeatAddressing)
{
	m_xSurfaceInfo.m_uMemoryFlags |= (1 << MEMORY_FLAGS__BINDLESS);

	if (m_xSRV.m_xImageViewHandle.IsValid())
	{
		auto& xGraphics = g_xEngine.FluxGraphics();

		// Allocate a dense bindless slot once. The SRV may already own one (created
		// with the BINDLESS flag set) — reuse it; the descriptor write is idempotent.
		if (m_xSRV.m_uBindlessIndex == uFLUX_INVALID_BINDLESS_INDEX)
		{
			m_xSRV.m_uBindlessIndex = xGraphics.BindlessAllocator().Allocate();
		}
		// Material textures tile (UV transform) → REPEAT addressing; UI textures CLAMP.
		// Engine-typed wrapper — backend extracts vk::ImageView / vk::Sampler internally.
		const Flux_Sampler& xSampler = bRepeatAddressing
			? xGraphics.m_xRepeatSampler
			: xGraphics.m_xClampSampler;
		g_xEngine.FluxBackend().WriteBindlessTextureSlot(
			m_xSRV.m_uBindlessIndex,
			m_xSRV,
			xSampler);
	}
}

Zenith_Status Zenith_TextureAsset::ParseZtxtr(const std::string& strPath, Zenith_DataStream& xStream,
	Flux_SurfaceInfo& xOutInfo, Zenith_Vector<uint8_t>& xOutBytes, bool& bOutIsV2)
{
	ZENITH_PROFILE_SCOPE("Texture Parse .ztxtr");
	bOutIsV2 = false;

	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: stream invalid for '%s'", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	// Envelope-aware read with back-compat:
	//   - v2 file  : header OK, m_uSchemaVersion >= 2 -> multi-mip chain payload.
	//   - v1 file  : header OK, schema <= 1 -> legacy single-mip payload.
	//   - legacy   : no envelope -> BAD_MAGIC; ReadStreamHeader is non-destructive
	//                (restores cursor to 0) so the headerless single-mip layout reads.
	//   - future   : newer envelope version -> hard fail (VERSION_MISMATCH).
	Zenith_Result<Zenith_StreamHeader> xHeaderResult = Zenith_ReadStreamHeader(xStream, uTEXTURE_ASSET_TYPE_ID);
	if (!xHeaderResult.IsOk() && xHeaderResult.Error() != Zenith_ErrorCode::BAD_MAGIC)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: Unsupported envelope in '%s'", strPath.c_str());
		return xHeaderResult.Error();
	}
	const bool bV2 = xHeaderResult.IsOk() && xHeaderResult.Value().m_uSchemaVersion >= uTEXTURE_SCHEMA_VERSION_V2;

	int32_t iWidth = 0, iHeight = 0, iDepth = 0;
	TextureFormat eFormat = TEXTURE_FORMAT_NONE;
	xStream >> iWidth;
	xStream >> iHeight;
	xStream >> iDepth;
	xStream >> eFormat;

	if (iWidth <= 0 || iHeight <= 0)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: invalid dims %dx%d in '%s'", iWidth, iHeight, strPath.c_str());
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	const int32_t iCorrectedDepth = std::max(1, iDepth);

	xOutInfo = Flux_SurfaceInfo();
	xOutInfo.m_uWidth = static_cast<uint32_t>(iWidth);
	xOutInfo.m_uHeight = static_cast<uint32_t>(iHeight);
	xOutInfo.m_uDepth = static_cast<uint32_t>(iCorrectedDepth);
	xOutInfo.m_uNumLayers = 1;
	xOutInfo.m_eFormat = eFormat;
	xOutInfo.m_eTextureType = TEXTURE_TYPE_2D;

	if (bV2)
	{
		uint32_t uNumMips = 0;
		size_t ulTotalDataSize = 0;
		xStream >> uNumMips;
		xStream >> ulTotalDataSize;

		// STRICT validation — v2 is a new, tightly-packed format with no slack.
		// The stored count and total must exactly match the shared layout contract.
		const uint32_t uExpectedMips = static_cast<uint32_t>(std::floor(std::log2((std::max)(iWidth, iHeight))) + 1);
		const size_t ulExpectedTotal = CalculateTotalMipChainSize(eFormat, xOutInfo.m_uWidth, xOutInfo.m_uHeight, uNumMips);
		if (uNumMips == 0 || uNumMips != uExpectedMips || ulTotalDataSize != ulExpectedTotal)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: v2 mip-chain mismatch in '%s' (mips=%u expected=%u, size=%zu expected=%zu)",
				strPath.c_str(), uNumMips, uExpectedMips, ulTotalDataSize, ulExpectedTotal);
			return Zenith_ErrorCode::CORRUPT_DATA;
		}
		// Bounds-check BEFORE ReadData: ReadData only logs on overflow (no status
		// return), so the loader must verify the stream actually holds the payload.
		if (xStream.GetCapacity() - xStream.GetCursor() < ulTotalDataSize)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: v2 payload truncated in '%s' (need %zu, have %llu)",
				strPath.c_str(), ulTotalDataSize, static_cast<unsigned long long>(xStream.GetCapacity() - xStream.GetCursor()));
			return Zenith_ErrorCode::CORRUPT_DATA;
		}

		// v2 is 2D-only by contract: CalculateTotalMipChainSize (validated above)
		// and the pre-baked upload's per-mip math treat depth as 1, so pin it here
		// rather than trust a stored depth the rest of the v2 path would ignore.
		xOutInfo.m_uDepth = 1;
		xOutInfo.m_uNumMips = uNumMips;
		xOutBytes.Resize(static_cast<u_int>(ulTotalDataSize));
		xStream.ReadData(xOutBytes.GetDataPointer(), ulTotalDataSize);
		bOutIsV2 = true;
		return true;
	}

	// Legacy / v1 single-mip payload (back-compat: matches the historical layout).
	size_t ulDataSize = 0;
	xStream >> ulDataSize;

	const size_t ulExpectedDataSize = CalculateTextureDataSize(eFormat, iWidth, iHeight, iCorrectedDepth);
	// Use the larger of file-stored size or expected size for safety (some legacy
	// files stored 0 for the size but still carry pixel data).
	const size_t ulAllocSize = std::max(ulDataSize, ulExpectedDataSize);
	if (ulAllocSize == 0)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: Zero data size for texture '%s' (dims: %dx%dx%d)", strPath.c_str(), iWidth, iHeight, iDepth);
		return Zenith_ErrorCode::CORRUPT_DATA;
	}

	xOutInfo.m_uNumMips = 1;
	xOutBytes.Resize(static_cast<u_int>(ulAllocSize), 0);  // zero-init in case the file has less than expected
	const size_t ulReadSize = (ulDataSize > 0) ? ulDataSize : ulExpectedDataSize;
	if (ulReadSize > 0)
	{
		if (xStream.GetCapacity() - xStream.GetCursor() < ulReadSize)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: legacy payload truncated in '%s'", strPath.c_str());
			return Zenith_ErrorCode::CORRUPT_DATA;
		}
		xStream.ReadData(xOutBytes.GetDataPointer(), ulReadSize);
	}
	return true;
}

Zenith_Status Zenith_TextureAsset::LoadCPUData(const std::string& strPath, Flux_SurfaceInfo& xOutInfo, Zenith_Vector<uint8_t>& xOutBytes)
{
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset::LoadCPUData: Failed to read file '%s'", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}
	bool bV2 = false;
	return ParseZtxtr(strPath, xStream, xOutInfo, xOutBytes, bV2);
}

Zenith_Status Zenith_TextureAsset::LoadFromFile(const std::string& strPath, bool bCreateMips)
{
	ZENITH_PROFILE_SCOPE("Texture Load + GPU Upload");
	Zenith_DataStream xStream;
	xStream.ReadFromFile(strPath.c_str());
	if (!xStream.IsValid())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Zenith_TextureAsset: Failed to read file '%s'", strPath.c_str());
		return Zenith_ErrorCode::FILE_NOT_FOUND;
	}

	Flux_SurfaceInfo xFileInfo;
	Zenith_Vector<uint8_t> xBytes;
	bool bV2 = false;
	Zenith_Status xParseStatus = ParseZtxtr(strPath, xStream, xFileInfo, xBytes, bV2);
	if (!xParseStatus.IsOk())
	{
		return xParseStatus;
	}

	m_xSurfaceInfo = xFileInfo;
	m_xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	// Resolve how the GPU populates the mip chain, and the final mip count the
	// IMAGE (and therefore the SRV) will have. This must be set before BOTH the
	// upload and the SRV creation so the view exposes exactly the image's mips.
	const bool bIsCompressed = IsCompressedFormat(m_xSurfaceInfo.m_eFormat);
	TextureUploadMipMode eMipMode;
	if (bV2)
	{
		// File already carries the full chain (m_uNumMips set by ParseZtxtr).
		eMipMode = TEXTURE_MIPS_PREBAKED;
	}
	else if (bCreateMips && !bIsCompressed)
	{
		// Legacy uncompressed: allocate a chain and blit-generate at runtime.
		eMipMode = TEXTURE_MIPS_GENERATE_RUNTIME;
		m_xSurfaceInfo.m_uNumMips = static_cast<uint32_t>(std::floor(std::log2((std::max)(m_xSurfaceInfo.m_uWidth, m_xSurfaceInfo.m_uHeight))) + 1);
	}
	else
	{
		// Legacy compressed (or mips not requested): single mip, no fake chain.
		eMipMode = TEXTURE_MIPS_NONE;
		m_xSurfaceInfo.m_uNumMips = 1;
	}

	// Create GPU resources. Surface an invalid VRAM handle via the release-
	// survivable check tier (WS8.3) for diagnosability, but DO NOT fail the load:
	// the renderer has always tolerated a not-yet-valid handle here (e.g. headless
	// boot, where FontAtlas uploads before the device is ready), so hard-failing
	// would be a behaviour change. Hard-failing on genuine GPU-OOM is Wave-9
	// error-handling scope, where the tolerant-caller contract is revisited.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	m_xVRAMHandle = xVulkanMemory.CreateTextureVRAM(xBytes.GetDataPointer(), m_xSurfaceInfo, eMipMode);
	Zenith_Check(m_xVRAMHandle.IsValid(), "Zenith_TextureAsset: GPU upload returned an invalid handle for '%s'", strPath.c_str());
	m_xSRV = xVulkanMemory.CreateShaderResourceView(m_xVRAMHandle, m_xSurfaceInfo, 0, m_xSurfaceInfo.m_uNumMips);
	m_bGPUResourcesAllocated = true;

	// SUCCESS — payload is the legacy "true" bool carried by Zenith_Status.
	return true;
}

bool Zenith_TextureAsset::CreateFromData(const void* pData, const Flux_SurfaceInfo& xSurfaceInfo, bool bCreateMips)
{
	if (!pData)
	{
		return false;
	}

	m_xSurfaceInfo = xSurfaceInfo;

	// Ensure proper memory flags
	if (m_xSurfaceInfo.m_uMemoryFlags == 0)
	{
		m_xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;
	}

	// Create GPU resources. CreateFromData keeps its bool API; translate to the
	// mip mode. Procedural callers never supply a pre-baked chain, so bCreateMips
	// maps to runtime generation (uncompressed) or none.
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	const TextureUploadMipMode eMipMode = bCreateMips ? TEXTURE_MIPS_GENERATE_RUNTIME : TEXTURE_MIPS_NONE;
	if (eMipMode == TEXTURE_MIPS_GENERATE_RUNTIME)
	{
		// Match the mip count CreateTextureVRAM/NormalizeTextureInfo will allocate,
		// so the SRV below exposes the whole chain.
		m_xSurfaceInfo.m_uNumMips = static_cast<uint32_t>(std::floor(std::log2((std::max)(m_xSurfaceInfo.m_uWidth, m_xSurfaceInfo.m_uHeight))) + 1);
	}
	else
	{
		m_xSurfaceInfo.m_uNumMips = 1;
	}
	m_xVRAMHandle = xVulkanMemory.CreateTextureVRAM(pData, m_xSurfaceInfo, eMipMode);
	m_xSRV = xVulkanMemory.CreateShaderResourceView(m_xVRAMHandle, m_xSurfaceInfo, 0, m_xSurfaceInfo.m_uNumMips);
	m_bGPUResourcesAllocated = true;

	return m_xVRAMHandle.IsValid();
}

bool Zenith_TextureAsset::CreateCubemap(const void* apFaceData[6], const Flux_SurfaceInfo& xSurfaceInfo)
{
	if (!apFaceData)
	{
		return false;
	}

	// Calculate face data size
	const size_t ulLayerDataSize = CalculateTextureDataSize(xSurfaceInfo.m_eFormat,
		xSurfaceInfo.m_uWidth, xSurfaceInfo.m_uHeight);

	const size_t ulTotalDataSize = ulLayerDataSize * 6;

	// Concatenate face data
	void* pAllData = Zenith_MemoryManagement::Allocate(ulTotalDataSize);
	if (!pAllData)
	{
		return false;
	}

	for (uint32_t u = 0; u < 6; u++)
	{
		memcpy(static_cast<uint8_t*>(pAllData) + (u * ulLayerDataSize), apFaceData[u], ulLayerDataSize);
	}

	// Set up surface info for cubemap
	m_xSurfaceInfo = xSurfaceInfo;
	m_xSurfaceInfo.m_eTextureType = TEXTURE_TYPE_CUBE;
	m_xSurfaceInfo.m_uNumLayers = 6;
	if (m_xSurfaceInfo.m_uMemoryFlags == 0)
	{
		m_xSurfaceInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;
	}

	// Create GPU resources. Cubemap faces are single-mip; pin m_uNumMips to 1 so
	// the image (TEXTURE_MIPS_NONE) and the SRV agree exactly.
	m_xSurfaceInfo.m_uNumMips = 1;
	auto& xVulkanMemory = g_xEngine.FluxMemory();
	m_xVRAMHandle = xVulkanMemory.CreateTextureVRAM(pAllData, m_xSurfaceInfo, TEXTURE_MIPS_NONE);
	m_xSRV = xVulkanMemory.CreateShaderResourceView(m_xVRAMHandle, m_xSurfaceInfo, 0, m_xSurfaceInfo.m_uNumMips);
	m_bGPUResourcesAllocated = true;

	Zenith_MemoryManagement::Deallocate(pAllData);

	return m_xVRAMHandle.IsValid();
}

bool Zenith_TextureAsset::LoadCubemapFromFiles(
	const char* szPathPX, const char* szPathNX,
	const char* szPathPY, const char* szPathNY,
	const char* szPathPZ, const char* szPathNZ)
{
	ZENITH_PROFILE_SCOPE("Cubemap Load From Files");
	const char* aszPaths[6] = { szPathPX, szPathNX, szPathPY, szPathNY, szPathPZ, szPathNZ };
	Zenith_Vector<uint8_t> axFaceBytes[6];   // mip 0 of each face
	uint32_t uWidth = 0, uHeight = 0, uDepth = 0;
	TextureFormat eFormat = TEXTURE_FORMAT_RGBA8_UNORM;

	for (uint32_t u = 0; u < 6; u++)
	{
		// Route through the single .ztxtr parser (no GPU upload). Cubemaps are
		// built single-mip, so take mip 0 of each face — the whole buffer for a
		// legacy file, or the first mip of a v2 chain. Never hand-parse here:
		// the engine cubemap faces are re-exported by ExportAllTextures and may
		// be v2, which the old per-face header read could not parse.
		Flux_SurfaceInfo xFaceInfo;
		Zenith_Vector<uint8_t> xFaceBytes;
		if (!LoadCPUData(aszPaths[u], xFaceInfo, xFaceBytes).IsOk())
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "LoadCubemapFromFiles: Failed to read face %u from '%s'", u, aszPaths[u]);
			return false;
		}

		const size_t ulMip0Size = CalculateMipDataSize(xFaceInfo.m_eFormat, xFaceInfo.m_uWidth, xFaceInfo.m_uHeight, 0);
		if (xFaceBytes.GetSize() < ulMip0Size)
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "LoadCubemapFromFiles: face %u from '%s' is too small (%zu < %zu)", u, aszPaths[u], static_cast<size_t>(xFaceBytes.GetSize()), ulMip0Size);
			return false;
		}
		xFaceBytes.Resize(static_cast<u_int>(ulMip0Size));   // drop any lower mips — cubemap is single-mip
		axFaceBytes[u] = std::move(xFaceBytes);

		if (u == 0)
		{
			uWidth = xFaceInfo.m_uWidth;
			uHeight = xFaceInfo.m_uHeight;
			uDepth = xFaceInfo.m_uDepth;
			eFormat = xFaceInfo.m_eFormat;
		}
	}

	// Set up surface info
	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uDepth = uDepth;
	xInfo.m_uNumLayers = 6;
	xInfo.m_eFormat = eFormat;
	xInfo.m_uNumMips = 1;
	xInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	// Create cubemap from face data (mip 0 of each face)
	const void* apFaceData[6] = { axFaceBytes[0].GetDataPointer(), axFaceBytes[1].GetDataPointer(), axFaceBytes[2].GetDataPointer(), axFaceBytes[3].GetDataPointer(), axFaceBytes[4].GetDataPointer(), axFaceBytes[5].GetDataPointer() };
	return CreateCubemap(apFaceData, xInfo);
}

void Zenith_TextureAsset::ReleaseGPU()
{
	if (m_bGPUResourcesAllocated && m_xVRAMHandle.IsValid())
	{
		auto& xEngine = g_xEngine;

		// Return the bindless slot to the allocator (deferred-recycled after the
		// frame-in-flight grace, mirroring the VRAM deferred deletion below).
		if (m_xSRV.m_uBindlessIndex != uFLUX_INVALID_BINDLESS_INDEX)
		{
			xEngine.FluxGraphics().BindlessAllocator().Free(m_xSRV.m_uBindlessIndex);
		}
		xEngine.FluxMemory().QueueVRAMDeletion(m_xVRAMHandle, m_xSRV.m_xImageViewHandle);
		m_xSRV = Flux_ShaderResourceView();
		m_bGPUResourcesAllocated = false;
	}
}

