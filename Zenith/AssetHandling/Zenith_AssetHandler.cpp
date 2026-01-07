#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"

// Asset pools - fixed-size arrays
Flux_Texture* Zenith_AssetHandler::s_pxTextures = new Flux_Texture[ZENITH_MAX_TEXTURES];
Flux_MeshGeometry* Zenith_AssetHandler::s_pxMeshes = new Flux_MeshGeometry[ZENITH_MAX_MESHES];

// Used slot tracking (no string keys)
std::unordered_set<Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xUsedTextureIDs;
std::unordered_set<Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xUsedMeshIDs;

// Lifecycle logging
bool Zenith_AssetHandler::s_bLifecycleLoggingEnabled = false;

// New asset system - path-to-asset caches
std::unordered_map<std::string, Zenith_MeshAsset*> Zenith_AssetHandler::s_xLoadedMeshAssets;
std::unordered_map<std::string, Zenith_SkeletonAsset*> Zenith_AssetHandler::s_xLoadedSkeletonAssets;
std::unordered_map<std::string, Zenith_ModelAsset*> Zenith_AssetHandler::s_xLoadedModelAssets;

//------------------------------------------------------------------------------
// Lifecycle Logging Helpers
//------------------------------------------------------------------------------

static void LogAssetCreation(const char* szType, Zenith_AssetHandler::AssetID uID, const void* pPointer)
{
	if (Zenith_AssetHandler::IsLifecycleLoggingEnabled())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "CREATE %s: ID=%u, ptr=%p", szType, uID, pPointer);
	}
}

static void LogAssetDeletion(const char* szType, Zenith_AssetHandler::AssetID uID, const void* pPointer)
{
	if (Zenith_AssetHandler::IsLifecycleLoggingEnabled())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "DELETE %s: ID=%u, ptr=%p", szType, uID, pPointer);
	}
}

//------------------------------------------------------------------------------
// Texture Creation and Deletion
//------------------------------------------------------------------------------
Flux_Texture* Zenith_AssetHandler::AddTexture(const TextureData& xTextureData)
{
	AssetID uID = GetNextFreeTextureSlot();
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate texture - pool exhausted");
		return nullptr;
	}

	Flux_VRAMHandle xVRAMHandle;

	if (xTextureData.bIsCubemap)
	{
		// Concatenate cube face data for unified VRAM creation
		size_t ulLayerDataSize;
		if (IsCompressedFormat(xTextureData.xSurfaceInfo.m_eFormat))
		{
			ulLayerDataSize = CalculateCompressedTextureSize(xTextureData.xSurfaceInfo.m_eFormat,
				xTextureData.xSurfaceInfo.m_uWidth, xTextureData.xSurfaceInfo.m_uHeight);
		}
		else
		{
			ulLayerDataSize = ColourFormatBytesPerPixel(xTextureData.xSurfaceInfo.m_eFormat) *
				xTextureData.xSurfaceInfo.m_uWidth * xTextureData.xSurfaceInfo.m_uHeight;
		}
		const size_t ulTotalDataSize = ulLayerDataSize * 6;

		void* pAllData = Zenith_MemoryManagement::Allocate(ulTotalDataSize);
		if (!pAllData)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate cubemap staging memory");
			return nullptr;
		}

		for (uint32_t u = 0; u < 6; u++)
		{
			memcpy((uint8_t*)pAllData + (u * ulLayerDataSize), xTextureData.apCubeFaceData[u], ulLayerDataSize);
		}

		xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(pAllData, xTextureData.xSurfaceInfo, xTextureData.bCreateMips);

		Zenith_MemoryManagement::Deallocate(pAllData);
	}
	else
	{
		// Create 2D texture
		xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(xTextureData.pData, xTextureData.xSurfaceInfo, xTextureData.bCreateMips);
	}

	// Create and store SRV
	Flux_ShaderResourceView xSRV;
	xSRV.m_xVRAMHandle = xVRAMHandle;
	xSRV.m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xVRAMHandle, xTextureData.xSurfaceInfo);

	Flux_Texture* pxTexture = &s_pxTextures[uID];
	pxTexture->m_xSurfaceInfo = xTextureData.xSurfaceInfo;
	pxTexture->m_xVRAMHandle = xVRAMHandle;
	pxTexture->m_xSRV = xSRV;

	s_xUsedTextureIDs.insert(uID);
	LogAssetCreation("Texture", uID, pxTexture);

	return pxTexture;
}

void Zenith_AssetHandler::DeleteTexture(Flux_Texture* pxTexture)
{
	if (!pxTexture)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to delete null texture");
		return;
	}

	AssetID uID = GetIDFromTexturePointer(pxTexture);
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Invalid texture pointer in DeleteTexture");
		return;
	}

	if (s_xUsedTextureIDs.find(uID) == s_xUsedTextureIDs.end())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Texture ID %u not in use", uID);
		return;
	}

	LogAssetDeletion("Texture", uID, pxTexture);

	Zenith_Assert(pxTexture->m_xVRAMHandle.IsValid(), "Deleting invalid texture");

	Zenith_Vulkan_VRAM* pxVRAM = Zenith_Vulkan::GetVRAM(pxTexture->m_xVRAMHandle);
	Flux_MemoryManager::QueueVRAMDeletion(pxVRAM, pxTexture->m_xVRAMHandle, pxTexture->m_xSRV.m_xImageView);

	// Clear the texture data
	*pxTexture = Flux_Texture();

	s_xUsedTextureIDs.erase(uID);
}

bool Zenith_AssetHandler::DeleteTextureByPath(const std::string& strPath)
{
	if (strPath.empty())
	{
		return false;
	}

	// Search for texture with matching source path
	for (AssetID uID : s_xUsedTextureIDs)
	{
		Flux_Texture* pxTexture = &s_pxTextures[uID];
		if (pxTexture->m_strSourcePath == strPath)
		{
			DeleteTexture(pxTexture);
			return true;
		}
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Texture not found by path: %s", strPath.c_str());
	return false;
}

Flux_Texture* Zenith_AssetHandler::GetTextureByPath(const std::string& strPath)
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Search for texture with matching source path
	for (AssetID uID : s_xUsedTextureIDs)
	{
		Flux_Texture* pxTexture = &s_pxTextures[uID];
		if (pxTexture->m_strSourcePath == strPath)
		{
			return pxTexture;
		}
	}

	return nullptr;
}

//------------------------------------------------------------------------------
// Texture File Loading
//------------------------------------------------------------------------------
Zenith_AssetHandler::TextureData Zenith_AssetHandler::LoadTexture2DFromFile(const char* szPath)
{
	// Load texture data from file
	size_t ulDataSize;
	int32_t uWidth = 0, uHeight = 0, uDepth = 0;
	TextureFormat eFormat;

	Zenith_DataStream xStream;
	xStream.ReadFromFile(szPath);

	// Validate file was loaded successfully
	if (!xStream.IsValid())
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "LoadTexture2DFromFile: Failed to read file '%s'", szPath);
		return TextureData();
	}

	xStream >> uWidth;
	xStream >> uHeight;
	xStream >> uDepth;
	xStream >> eFormat;
	xStream >> ulDataSize;

	void* const pData = Zenith_MemoryManagement::Allocate(ulDataSize);
	if (!pData)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate %zu bytes for texture from %s", ulDataSize, szPath);
		return TextureData();
	}
	xStream.ReadData(pData, ulDataSize);

	// For compressed formats, we only have mip 0 (no runtime mip generation)
	// For uncompressed formats, we generate mips at runtime
	const bool bIsCompressed = IsCompressedFormat(eFormat);
	const uint32_t uNumMips = bIsCompressed ? 1 : (std::floor(std::log2((std::max)(uWidth, uHeight))) + 1);

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uDepth = uDepth;
	xInfo.m_uNumLayers = 1;
	xInfo.m_eFormat = eFormat;
	xInfo.m_uNumMips = uNumMips;
	xInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	TextureData xTextureData;
	xTextureData.pData = pData;
	xTextureData.xSurfaceInfo = xInfo;
	xTextureData.bCreateMips = !bIsCompressed;
	xTextureData.bIsCubemap = false;

	return xTextureData;
}

Zenith_AssetHandler::TextureData Zenith_AssetHandler::LoadTextureCubeFromFiles(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ)
{
	const char* aszPaths[6] =
	{
		szPathPX,
		szPathNX,
		szPathPY,
		szPathNY,
		szPathPZ,
		szPathNZ,
	};

	void* apDatas[6] =
	{
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
		nullptr,
	};

	size_t aulDataSizes[6] = { 0, 0, 0, 0, 0, 0 };
	uint32_t uWidth = 0, uHeight = 0, uDepth = 0;
	TextureFormat eFormat = TEXTURE_FORMAT_RGBA8_UNORM;

	for (uint32_t u = 0; u < 6; u++)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(aszPaths[u]);

		// Validate file was loaded successfully
		if (!xStream.IsValid())
		{
			Zenith_Error(LOG_CATEGORY_ASSET, "LoadTextureCubeFromFiles: Failed to read face %u from '%s'", u, aszPaths[u]);
			// Clean up already allocated faces
			for (uint32_t j = 0; j < u; j++)
			{
				if (apDatas[j])
				{
					Zenith_MemoryManagement::Deallocate(apDatas[j]);
				}
			}
			return TextureData();
		}

		TextureFormat eFaceFormat;

		xStream >> uWidth;
		xStream >> uHeight;
		xStream >> uDepth;
		xStream >> eFaceFormat;
		xStream >> aulDataSizes[u];

		// Use format from first face (all faces should have same format)
		if (u == 0)
		{
			eFormat = eFaceFormat;
		}

		apDatas[u] = Zenith_MemoryManagement::Allocate(aulDataSizes[u]);
		if (!apDatas[u])
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate cubemap face %u", u);
			// Clean up already allocated faces
			for (uint32_t j = 0; j < u; j++)
			{
				if (apDatas[j])
				{
					Zenith_MemoryManagement::Deallocate(apDatas[j]);
				}
			}
			return TextureData();
		}
		xStream.ReadData(apDatas[u], aulDataSizes[u]);
	}

	const bool bIsCompressed = IsCompressedFormat(eFormat);
	const uint32_t uNumMips = bIsCompressed ? 1 : (std::floor(std::log2((std::max)(uWidth, uHeight))) + 1);

	Flux_SurfaceInfo xInfo;
	xInfo.m_uWidth = uWidth;
	xInfo.m_uHeight = uHeight;
	xInfo.m_uDepth = uDepth;
	xInfo.m_uNumLayers = 6;
	xInfo.m_eFormat = eFormat;
	xInfo.m_uNumMips = uNumMips;
	xInfo.m_uMemoryFlags = 1 << MEMORY_FLAGS__SHADER_READ;

	TextureData xTextureData;
	xTextureData.apCubeFaceData[0] = apDatas[0];
	xTextureData.apCubeFaceData[1] = apDatas[1];
	xTextureData.apCubeFaceData[2] = apDatas[2];
	xTextureData.apCubeFaceData[3] = apDatas[3];
	xTextureData.apCubeFaceData[4] = apDatas[4];
	xTextureData.apCubeFaceData[5] = apDatas[5];
	xTextureData.xSurfaceInfo = xInfo;
	xTextureData.bCreateMips = !bIsCompressed;
	xTextureData.bIsCubemap = true;

	return xTextureData;
}

//------------------------------------------------------------------------------
// Mesh Creation and Deletion
//------------------------------------------------------------------------------
Flux_MeshGeometry* Zenith_AssetHandler::AddMesh()
{
	AssetID uID = GetNextFreeMeshSlot();
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate mesh - pool exhausted");
		return nullptr;
	}

	Flux_MeshGeometry* pxMesh = &s_pxMeshes[uID];
	s_xUsedMeshIDs.insert(uID);
	LogAssetCreation("Mesh", uID, pxMesh);

	return pxMesh;
}

Flux_MeshGeometry* Zenith_AssetHandler::GetMeshByPath(const std::string& strPath)
{
	if (strPath.empty())
	{
		return nullptr;
	}

	// Search for mesh with matching source path
	for (AssetID uID : s_xUsedMeshIDs)
	{
		Flux_MeshGeometry* pxMesh = &s_pxMeshes[uID];
		if (pxMesh->m_strSourcePath == strPath)
		{
			return pxMesh;
		}
	}

	return nullptr;
}

Flux_MeshGeometry* Zenith_AssetHandler::AddMeshFromFile(const char* szPath, u_int uRetainAttributeBits /*= 0*/, const bool bUploadToGPU /*= true*/)
{
	AssetID uID = GetNextFreeMeshSlot();
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to allocate mesh - pool exhausted");
		return nullptr;
	}

	Flux_MeshGeometry* pxMesh = &s_pxMeshes[uID];

	// Load from file
	Flux_MeshGeometry::LoadFromFile(szPath, *pxMesh, uRetainAttributeBits, bUploadToGPU);

	// Store source path for serialization
	pxMesh->m_strSourcePath = szPath;

	s_xUsedMeshIDs.insert(uID);
	LogAssetCreation("Mesh", uID, pxMesh);

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "  Loaded from: %s", szPath);
	}

	return pxMesh;
}

void Zenith_AssetHandler::DeleteMesh(Flux_MeshGeometry* pxMesh)
{
	if (!pxMesh)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to delete null mesh");
		return;
	}

	AssetID uID = GetIDFromMeshPointer(pxMesh);
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Invalid mesh pointer in DeleteMesh");
		return;
	}

	if (s_xUsedMeshIDs.find(uID) == s_xUsedMeshIDs.end())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Mesh ID %u not in use", uID);
		return;
	}

	LogAssetDeletion("Mesh", uID, pxMesh);

	// Queue vertex buffer VRAM for deletion if it exists
	if (pxMesh->GetVertexBuffer().GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxVertexVRAM = Zenith_Vulkan::GetVRAM(pxMesh->GetVertexBuffer().GetBuffer().m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxVertexVRAM, pxMesh->GetVertexBuffer().GetBuffer().m_xVRAMHandle);
	}

	// Queue index buffer VRAM for deletion if it exists
	if (pxMesh->GetIndexBuffer().GetBuffer().m_xVRAMHandle.IsValid())
	{
		Zenith_Vulkan_VRAM* pxIndexVRAM = Zenith_Vulkan::GetVRAM(pxMesh->GetIndexBuffer().GetBuffer().m_xVRAMHandle);
		Flux_MemoryManager::QueueVRAMDeletion(pxIndexVRAM, pxMesh->GetIndexBuffer().GetBuffer().m_xVRAMHandle);
	}

	// Reset the mesh (clears CPU-side data)
	pxMesh->Reset();

	s_xUsedMeshIDs.erase(uID);
}

//------------------------------------------------------------------------------
// Bulk Operations
//------------------------------------------------------------------------------
void Zenith_AssetHandler::DestroyAllAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Destroying all assets...");

	// Copy the sets because deletion modifies them
	std::unordered_set<AssetID> xTexturesToDelete = s_xUsedTextureIDs;
	std::unordered_set<AssetID> xMeshesToDelete = s_xUsedMeshIDs;

	for (AssetID uID : xTexturesToDelete)
	{
		DeleteTexture(&s_pxTextures[uID]);
	}

	for (AssetID uID : xMeshesToDelete)
	{
		DeleteMesh(&s_pxMeshes[uID]);
	}

	// Materials are now managed by Flux_MaterialAsset::UnloadAll()
	Flux_MaterialAsset::UnloadAll();

	// Clear new asset system caches
	ClearAllNewAssets();

	Zenith_Log(LOG_CATEGORY_ASSET, "All assets destroyed");
}

//------------------------------------------------------------------------------
// Diagnostics
//------------------------------------------------------------------------------
void Zenith_AssetHandler::EnableLifecycleLogging(bool bEnable)
{
	s_bLifecycleLoggingEnabled = bEnable;
	Zenith_Log(LOG_CATEGORY_ASSET, "Lifecycle logging %s", bEnable ? "ENABLED" : "DISABLED");
}

uint32_t Zenith_AssetHandler::GetActiveTextureCount()
{
	return static_cast<uint32_t>(s_xUsedTextureIDs.size());
}

uint32_t Zenith_AssetHandler::GetActiveMeshCount()
{
	return static_cast<uint32_t>(s_xUsedMeshIDs.size());
}

void Zenith_AssetHandler::LogActiveAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Active Asset Summary:");
	Zenith_Log(LOG_CATEGORY_ASSET, "  Textures: %u", GetActiveTextureCount());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Meshes: %u", GetActiveMeshCount());

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Active Texture IDs:");
		for (AssetID uID : s_xUsedTextureIDs)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  ID=%u, ptr=%p", uID, &s_pxTextures[uID]);
		}

		Zenith_Log(LOG_CATEGORY_ASSET, "Active Mesh IDs:");
		for (AssetID uID : s_xUsedMeshIDs)
		{
			Flux_MeshGeometry* pxMesh = &s_pxMeshes[uID];
			Zenith_Log(LOG_CATEGORY_ASSET, "  ID=%u, ptr=%p, source=%s", uID, pxMesh,
				pxMesh->m_strSourcePath.empty() ? "(procedural)" : pxMesh->m_strSourcePath.c_str());
		}
	}

	// Material logging is now handled by Flux_MaterialAsset
	Zenith_Vector<std::string> xMaterialPaths;
	Flux_MaterialAsset::GetAllLoadedMaterialPaths(xMaterialPaths);
	Zenith_Log(LOG_CATEGORY_ASSET, "  Materials (Flux_MaterialAsset): %u", xMaterialPaths.GetSize());

	// New asset system counts
	Zenith_Log(LOG_CATEGORY_ASSET, "  Mesh Assets: %u", GetLoadedMeshAssetCount());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Skeleton Assets: %u", GetLoadedSkeletonAssetCount());
	Zenith_Log(LOG_CATEGORY_ASSET, "  Model Assets: %u", GetLoadedModelAssetCount());

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "Loaded Mesh Assets:");
		for (const auto& xPair : s_xLoadedMeshAssets)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  %s -> ptr=%p", xPair.first.c_str(), xPair.second);
		}

		Zenith_Log(LOG_CATEGORY_ASSET, "Loaded Skeleton Assets:");
		for (const auto& xPair : s_xLoadedSkeletonAssets)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  %s -> ptr=%p", xPair.first.c_str(), xPair.second);
		}

		Zenith_Log(LOG_CATEGORY_ASSET, "Loaded Model Assets:");
		for (const auto& xPair : s_xLoadedModelAssets)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  %s -> ptr=%p", xPair.first.c_str(), xPair.second);
		}
	}
}

//------------------------------------------------------------------------------
// Pointer Validation
//------------------------------------------------------------------------------
bool Zenith_AssetHandler::IsValidTexture(const Flux_Texture* pxTexture)
{
	if (!pxTexture) return false;
	AssetID uID = GetIDFromTexturePointer(pxTexture);
	return uID != INVALID_ASSET_ID && s_xUsedTextureIDs.find(uID) != s_xUsedTextureIDs.end();
}

bool Zenith_AssetHandler::IsValidMesh(const Flux_MeshGeometry* pxMesh)
{
	if (!pxMesh) return false;
	AssetID uID = GetIDFromMeshPointer(pxMesh);
	return uID != INVALID_ASSET_ID && s_xUsedMeshIDs.find(uID) != s_xUsedMeshIDs.end();
}

//------------------------------------------------------------------------------
// Slot Allocation
//------------------------------------------------------------------------------
Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetNextFreeTextureSlot()
{
	for (AssetID u = 0; u < ZENITH_MAX_TEXTURES; u++)
	{
		if (s_xUsedTextureIDs.find(u) == s_xUsedTextureIDs.end())
		{
			return u;
		}
	}
	Zenith_Assert(false, "Run out of texture slots");
	return INVALID_ASSET_ID;
}

Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetNextFreeMeshSlot()
{
	for (AssetID u = 0; u < ZENITH_MAX_MESHES; u++)
	{
		if (s_xUsedMeshIDs.find(u) == s_xUsedMeshIDs.end())
		{
			return u;
		}
	}
	Zenith_Assert(false, "Run out of mesh slots");
	return INVALID_ASSET_ID;
}

//------------------------------------------------------------------------------
// Pointer-to-ID Conversion
//------------------------------------------------------------------------------
Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetIDFromTexturePointer(const Flux_Texture* pxTexture)
{
	if (!pxTexture) return INVALID_ASSET_ID;

	// Check if pointer is within the texture array bounds
	if (pxTexture < s_pxTextures || pxTexture >= s_pxTextures + ZENITH_MAX_TEXTURES)
	{
		return INVALID_ASSET_ID;
	}

	return static_cast<AssetID>(pxTexture - s_pxTextures);
}

Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetIDFromMeshPointer(const Flux_MeshGeometry* pxMesh)
{
	if (!pxMesh) return INVALID_ASSET_ID;

	// Check if pointer is within the mesh array bounds
	if (pxMesh < s_pxMeshes || pxMesh >= s_pxMeshes + ZENITH_MAX_MESHES)
	{
		return INVALID_ASSET_ID;
	}

	return static_cast<AssetID>(pxMesh - s_pxMeshes);
}

//------------------------------------------------------------------------------
// New Asset System - Mesh/Skeleton/Model Assets and Instances
//------------------------------------------------------------------------------

Zenith_MeshAsset* Zenith_AssetHandler::LoadMeshAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Empty path passed to LoadMeshAsset");
		return nullptr;
	}

	// Check cache first
	auto it = s_xLoadedMeshAssets.find(strPath);
	if (it != s_xLoadedMeshAssets.end())
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "LoadMeshAsset: Cache hit for %s", strPath.c_str());
		}
		return it->second;
	}

	// Load from file
	Zenith_MeshAsset* pxAsset = Zenith_MeshAsset::LoadFromFile(strPath.c_str());
	if (pxAsset)
	{
		s_xLoadedMeshAssets[strPath] = pxAsset;
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "LoadMeshAsset: Loaded %s, ptr=%p", strPath.c_str(), pxAsset);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to load mesh asset from %s", strPath.c_str());
	}

	return pxAsset;
}

Zenith_SkeletonAsset* Zenith_AssetHandler::LoadSkeletonAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Empty path passed to LoadSkeletonAsset");
		return nullptr;
	}

	// Check cache first
	auto it = s_xLoadedSkeletonAssets.find(strPath);
	if (it != s_xLoadedSkeletonAssets.end())
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "LoadSkeletonAsset: Cache hit for %s", strPath.c_str());
		}
		return it->second;
	}

	// Load from file
	Zenith_SkeletonAsset* pxAsset = Zenith_SkeletonAsset::LoadFromFile(strPath.c_str());
	if (pxAsset)
	{
		s_xLoadedSkeletonAssets[strPath] = pxAsset;
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "LoadSkeletonAsset: Loaded %s, ptr=%p", strPath.c_str(), pxAsset);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to load skeleton asset from %s", strPath.c_str());
	}

	return pxAsset;
}

Zenith_ModelAsset* Zenith_AssetHandler::LoadModelAsset(const std::string& strPath)
{
	if (strPath.empty())
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Empty path passed to LoadModelAsset");
		return nullptr;
	}

	// Check cache first
	auto it = s_xLoadedModelAssets.find(strPath);
	if (it != s_xLoadedModelAssets.end())
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "LoadModelAsset: Cache hit for %s", strPath.c_str());
		}
		return it->second;
	}

	// Load from file
	Zenith_ModelAsset* pxAsset = Zenith_ModelAsset::LoadFromFile(strPath.c_str());
	if (pxAsset)
	{
		s_xLoadedModelAssets[strPath] = pxAsset;
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "LoadModelAsset: Loaded %s, ptr=%p", strPath.c_str(), pxAsset);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to load model asset from %s", strPath.c_str());
	}

	return pxAsset;
}

Flux_MeshInstance* Zenith_AssetHandler::CreateMeshInstance(Zenith_MeshAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Null asset passed to CreateMeshInstance");
		return nullptr;
	}

	Flux_MeshInstance* pxInstance = Flux_MeshInstance::CreateFromAsset(pxAsset);
	if (pxInstance)
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "CreateMeshInstance: Created instance ptr=%p from asset ptr=%p", pxInstance, pxAsset);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to create mesh instance from asset ptr=%p", pxAsset);
	}

	return pxInstance;
}

Flux_SkeletonInstance* Zenith_AssetHandler::CreateSkeletonInstance(Zenith_SkeletonAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Null asset passed to CreateSkeletonInstance");
		return nullptr;
	}

	Flux_SkeletonInstance* pxInstance = Flux_SkeletonInstance::CreateFromAsset(pxAsset);
	if (pxInstance)
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "CreateSkeletonInstance: Created instance ptr=%p from asset ptr=%p", pxInstance, pxAsset);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to create skeleton instance from asset ptr=%p", pxAsset);
	}

	return pxInstance;
}

Flux_ModelInstance* Zenith_AssetHandler::CreateModelInstance(Zenith_ModelAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Null asset passed to CreateModelInstance");
		return nullptr;
	}

	Flux_ModelInstance* pxInstance = Flux_ModelInstance::CreateFromAsset(pxAsset);
	if (pxInstance)
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "CreateModelInstance: Created instance ptr=%p from asset ptr=%p", pxInstance, pxAsset);
		}
	}
	else
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "ERROR: Failed to create model instance from asset ptr=%p", pxAsset);
	}

	return pxInstance;
}

Flux_ModelInstance* Zenith_AssetHandler::LoadAndCreateModelInstance(const std::string& strPath)
{
	Zenith_ModelAsset* pxAsset = LoadModelAsset(strPath);
	if (!pxAsset)
	{
		return nullptr;
	}

	return CreateModelInstance(pxAsset);
}

void Zenith_AssetHandler::UnloadMeshAsset(Zenith_MeshAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to unload null mesh asset");
		return;
	}

	// Find and remove from cache
	for (auto it = s_xLoadedMeshAssets.begin(); it != s_xLoadedMeshAssets.end(); ++it)
	{
		if (it->second == pxAsset)
		{
			if (s_bLifecycleLoggingEnabled)
			{
				Zenith_Log(LOG_CATEGORY_ASSET, "UnloadMeshAsset: Unloading %s, ptr=%p", it->first.c_str(), pxAsset);
			}
			s_xLoadedMeshAssets.erase(it);
			delete pxAsset;
			return;
		}
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Mesh asset ptr=%p not found in cache", pxAsset);
}

void Zenith_AssetHandler::UnloadSkeletonAsset(Zenith_SkeletonAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to unload null skeleton asset");
		return;
	}

	// Find and remove from cache
	for (auto it = s_xLoadedSkeletonAssets.begin(); it != s_xLoadedSkeletonAssets.end(); ++it)
	{
		if (it->second == pxAsset)
		{
			if (s_bLifecycleLoggingEnabled)
			{
				Zenith_Log(LOG_CATEGORY_ASSET, "UnloadSkeletonAsset: Unloading %s, ptr=%p", it->first.c_str(), pxAsset);
			}
			s_xLoadedSkeletonAssets.erase(it);
			delete pxAsset;
			return;
		}
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Skeleton asset ptr=%p not found in cache", pxAsset);
}

void Zenith_AssetHandler::UnloadModelAsset(Zenith_ModelAsset* pxAsset)
{
	if (!pxAsset)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to unload null model asset");
		return;
	}

	// Find and remove from cache
	for (auto it = s_xLoadedModelAssets.begin(); it != s_xLoadedModelAssets.end(); ++it)
	{
		if (it->second == pxAsset)
		{
			if (s_bLifecycleLoggingEnabled)
			{
				Zenith_Log(LOG_CATEGORY_ASSET, "UnloadModelAsset: Unloading %s, ptr=%p", it->first.c_str(), pxAsset);
			}
			s_xLoadedModelAssets.erase(it);
			delete pxAsset;
			return;
		}
	}

	Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Model asset ptr=%p not found in cache", pxAsset);
}

void Zenith_AssetHandler::DestroyMeshInstance(Flux_MeshInstance* pxInstance)
{
	if (!pxInstance)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to destroy null mesh instance");
		return;
	}

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "DestroyMeshInstance: Destroying ptr=%p", pxInstance);
	}

	pxInstance->Destroy();
	delete pxInstance;
}

void Zenith_AssetHandler::DestroySkeletonInstance(Flux_SkeletonInstance* pxInstance)
{
	if (!pxInstance)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to destroy null skeleton instance");
		return;
	}

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "DestroySkeletonInstance: Destroying ptr=%p", pxInstance);
	}

	pxInstance->Destroy();
	delete pxInstance;
}

void Zenith_AssetHandler::DestroyModelInstance(Flux_ModelInstance* pxInstance)
{
	if (!pxInstance)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "WARNING: Attempted to destroy null model instance");
		return;
	}

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log(LOG_CATEGORY_ASSET, "DestroyModelInstance: Destroying ptr=%p", pxInstance);
	}

	pxInstance->Destroy();
	delete pxInstance;
}

void Zenith_AssetHandler::ClearAllNewAssets()
{
	Zenith_Log(LOG_CATEGORY_ASSET, "Clearing all new asset caches...");

	// Delete all mesh assets
	for (auto& xPair : s_xLoadedMeshAssets)
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  Deleting mesh asset: %s, ptr=%p", xPair.first.c_str(), xPair.second);
		}
		delete xPair.second;
	}
	s_xLoadedMeshAssets.clear();

	// Delete all skeleton assets
	for (auto& xPair : s_xLoadedSkeletonAssets)
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  Deleting skeleton asset: %s, ptr=%p", xPair.first.c_str(), xPair.second);
		}
		delete xPair.second;
	}
	s_xLoadedSkeletonAssets.clear();

	// Delete all model assets
	for (auto& xPair : s_xLoadedModelAssets)
	{
		if (s_bLifecycleLoggingEnabled)
		{
			Zenith_Log(LOG_CATEGORY_ASSET, "  Deleting model asset: %s, ptr=%p", xPair.first.c_str(), xPair.second);
		}
		delete xPair.second;
	}
	s_xLoadedModelAssets.clear();

	Zenith_Log(LOG_CATEGORY_ASSET, "All new asset caches cleared");
}

uint32_t Zenith_AssetHandler::GetLoadedMeshAssetCount()
{
	return static_cast<uint32_t>(s_xLoadedMeshAssets.size());
}

uint32_t Zenith_AssetHandler::GetLoadedSkeletonAssetCount()
{
	return static_cast<uint32_t>(s_xLoadedSkeletonAssets.size());
}

uint32_t Zenith_AssetHandler::GetLoadedModelAssetCount()
{
	return static_cast<uint32_t>(s_xLoadedModelAssets.size());
}
