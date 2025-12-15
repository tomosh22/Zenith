#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"

// Asset pools - fixed-size arrays
Flux_Texture* Zenith_AssetHandler::s_pxTextures = new Flux_Texture[ZENITH_MAX_TEXTURES];
Flux_MeshGeometry* Zenith_AssetHandler::s_pxMeshes = new Flux_MeshGeometry[ZENITH_MAX_MESHES];
Flux_Material* Zenith_AssetHandler::s_pxMaterials = new Flux_Material[ZENITH_MAX_MATERIALS];

// Used slot tracking (no string keys)
std::unordered_set<Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xUsedTextureIDs;
std::unordered_set<Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xUsedMeshIDs;
std::unordered_set<Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xUsedMaterialIDs;

// Lifecycle logging
bool Zenith_AssetHandler::s_bLifecycleLoggingEnabled = false;

//------------------------------------------------------------------------------
// Lifecycle Logging Helpers
//------------------------------------------------------------------------------
static constexpr const char* ASSET_LOG_TAG = "[AssetHandler]";

static void LogAssetCreation(const char* szType, Zenith_AssetHandler::AssetID uID, const void* pPointer)
{
	if (Zenith_AssetHandler::IsLifecycleLoggingEnabled())
	{
		Zenith_Log("%s CREATE %s: ID=%u, ptr=%p", ASSET_LOG_TAG, szType, uID, pPointer);
	}
}

static void LogAssetDeletion(const char* szType, Zenith_AssetHandler::AssetID uID, const void* pPointer)
{
	if (Zenith_AssetHandler::IsLifecycleLoggingEnabled())
	{
		Zenith_Log("%s DELETE %s: ID=%u, ptr=%p", ASSET_LOG_TAG, szType, uID, pPointer);
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
		Zenith_Log("%s ERROR: Failed to allocate texture - pool exhausted", ASSET_LOG_TAG);
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
			Zenith_Log("%s ERROR: Failed to allocate cubemap staging memory", ASSET_LOG_TAG);
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
		Zenith_Log("%s WARNING: Attempted to delete null texture", ASSET_LOG_TAG);
		return;
	}

	AssetID uID = GetIDFromTexturePointer(pxTexture);
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log("%s ERROR: Invalid texture pointer in DeleteTexture", ASSET_LOG_TAG);
		return;
	}

	if (s_xUsedTextureIDs.find(uID) == s_xUsedTextureIDs.end())
	{
		Zenith_Log("%s WARNING: Texture ID %u not in use", ASSET_LOG_TAG, uID);
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

	xStream >> uWidth;
	xStream >> uHeight;
	xStream >> uDepth;
	xStream >> eFormat;
	xStream >> ulDataSize;

	void* const pData = Zenith_MemoryManagement::Allocate(ulDataSize);
	if (!pData)
	{
		Zenith_Log("%s ERROR: Failed to allocate %zu bytes for texture from %s", ASSET_LOG_TAG, ulDataSize, szPath);
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
			Zenith_Log("%s ERROR: Failed to allocate cubemap face %u", ASSET_LOG_TAG, u);
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
		Zenith_Log("%s ERROR: Failed to allocate mesh - pool exhausted", ASSET_LOG_TAG);
		return nullptr;
	}

	Flux_MeshGeometry* pxMesh = &s_pxMeshes[uID];
	s_xUsedMeshIDs.insert(uID);
	LogAssetCreation("Mesh", uID, pxMesh);

	return pxMesh;
}

Flux_MeshGeometry* Zenith_AssetHandler::AddMeshFromFile(const char* szPath, u_int uRetainAttributeBits /*= 0*/, const bool bUploadToGPU /*= true*/)
{
	AssetID uID = GetNextFreeMeshSlot();
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log("%s ERROR: Failed to allocate mesh - pool exhausted", ASSET_LOG_TAG);
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
		Zenith_Log("%s   Loaded from: %s", ASSET_LOG_TAG, szPath);
	}

	return pxMesh;
}

void Zenith_AssetHandler::DeleteMesh(Flux_MeshGeometry* pxMesh)
{
	if (!pxMesh)
	{
		Zenith_Log("%s WARNING: Attempted to delete null mesh", ASSET_LOG_TAG);
		return;
	}

	AssetID uID = GetIDFromMeshPointer(pxMesh);
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log("%s ERROR: Invalid mesh pointer in DeleteMesh", ASSET_LOG_TAG);
		return;
	}

	if (s_xUsedMeshIDs.find(uID) == s_xUsedMeshIDs.end())
	{
		Zenith_Log("%s WARNING: Mesh ID %u not in use", ASSET_LOG_TAG, uID);
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
// Material Creation and Deletion
//------------------------------------------------------------------------------
Flux_Material* Zenith_AssetHandler::AddMaterial()
{
	AssetID uID = GetNextFreeMaterialSlot();
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log("%s ERROR: Failed to allocate material - pool exhausted", ASSET_LOG_TAG);
		return nullptr;
	}

	Flux_Material* pxMaterial = &s_pxMaterials[uID];
	s_xUsedMaterialIDs.insert(uID);
	LogAssetCreation("Material", uID, pxMaterial);

	return pxMaterial;
}

void Zenith_AssetHandler::DeleteMaterial(Flux_Material* pxMaterial)
{
	if (!pxMaterial)
	{
		Zenith_Log("%s WARNING: Attempted to delete null material", ASSET_LOG_TAG);
		return;
	}

	AssetID uID = GetIDFromMaterialPointer(pxMaterial);
	if (uID == INVALID_ASSET_ID)
	{
		Zenith_Log("%s ERROR: Invalid material pointer in DeleteMaterial", ASSET_LOG_TAG);
		return;
	}

	if (s_xUsedMaterialIDs.find(uID) == s_xUsedMaterialIDs.end())
	{
		Zenith_Log("%s WARNING: Material ID %u not in use", ASSET_LOG_TAG, uID);
		return;
	}

	LogAssetDeletion("Material", uID, pxMaterial);

	pxMaterial->Reset();

	s_xUsedMaterialIDs.erase(uID);
}

//------------------------------------------------------------------------------
// Bulk Operations
//------------------------------------------------------------------------------
void Zenith_AssetHandler::DestroyAllAssets()
{
	Zenith_Log("%s Destroying all assets...", ASSET_LOG_TAG);

	// Copy the sets because deletion modifies them
	std::unordered_set<AssetID> xTexturesToDelete = s_xUsedTextureIDs;
	std::unordered_set<AssetID> xMeshesToDelete = s_xUsedMeshIDs;
	std::unordered_set<AssetID> xMaterialsToDelete = s_xUsedMaterialIDs;

	for (AssetID uID : xTexturesToDelete)
	{
		DeleteTexture(&s_pxTextures[uID]);
	}

	for (AssetID uID : xMeshesToDelete)
	{
		DeleteMesh(&s_pxMeshes[uID]);
	}

	for (AssetID uID : xMaterialsToDelete)
	{
		DeleteMaterial(&s_pxMaterials[uID]);
	}

	Zenith_Log("%s All assets destroyed", ASSET_LOG_TAG);
}

//------------------------------------------------------------------------------
// Diagnostics
//------------------------------------------------------------------------------
void Zenith_AssetHandler::EnableLifecycleLogging(bool bEnable)
{
	s_bLifecycleLoggingEnabled = bEnable;
	Zenith_Log("%s Lifecycle logging %s", ASSET_LOG_TAG, bEnable ? "ENABLED" : "DISABLED");
}

uint32_t Zenith_AssetHandler::GetActiveTextureCount()
{
	return static_cast<uint32_t>(s_xUsedTextureIDs.size());
}

uint32_t Zenith_AssetHandler::GetActiveMeshCount()
{
	return static_cast<uint32_t>(s_xUsedMeshIDs.size());
}

uint32_t Zenith_AssetHandler::GetActiveMaterialCount()
{
	return static_cast<uint32_t>(s_xUsedMaterialIDs.size());
}

void Zenith_AssetHandler::LogActiveAssets()
{
	Zenith_Log("%s Active Asset Summary:", ASSET_LOG_TAG);
	Zenith_Log("%s   Textures: %u", ASSET_LOG_TAG, GetActiveTextureCount());
	Zenith_Log("%s   Meshes: %u", ASSET_LOG_TAG, GetActiveMeshCount());
	Zenith_Log("%s   Materials: %u", ASSET_LOG_TAG, GetActiveMaterialCount());

	if (s_bLifecycleLoggingEnabled)
	{
		Zenith_Log("%s Active Texture IDs:", ASSET_LOG_TAG);
		for (AssetID uID : s_xUsedTextureIDs)
		{
			Zenith_Log("%s   ID=%u, ptr=%p", ASSET_LOG_TAG, uID, &s_pxTextures[uID]);
		}

		Zenith_Log("%s Active Mesh IDs:", ASSET_LOG_TAG);
		for (AssetID uID : s_xUsedMeshIDs)
		{
			Flux_MeshGeometry* pxMesh = &s_pxMeshes[uID];
			Zenith_Log("%s   ID=%u, ptr=%p, source=%s", ASSET_LOG_TAG, uID, pxMesh,
				pxMesh->m_strSourcePath.empty() ? "(procedural)" : pxMesh->m_strSourcePath.c_str());
		}

		Zenith_Log("%s Active Material IDs:", ASSET_LOG_TAG);
		for (AssetID uID : s_xUsedMaterialIDs)
		{
			Zenith_Log("%s   ID=%u, ptr=%p", ASSET_LOG_TAG, uID, &s_pxMaterials[uID]);
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

bool Zenith_AssetHandler::IsValidMaterial(const Flux_Material* pxMaterial)
{
	if (!pxMaterial) return false;
	AssetID uID = GetIDFromMaterialPointer(pxMaterial);
	return uID != INVALID_ASSET_ID && s_xUsedMaterialIDs.find(uID) != s_xUsedMaterialIDs.end();
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

Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetNextFreeMaterialSlot()
{
	for (AssetID u = 0; u < ZENITH_MAX_MATERIALS; u++)
	{
		if (s_xUsedMaterialIDs.find(u) == s_xUsedMaterialIDs.end())
		{
			return u;
		}
	}
	Zenith_Assert(false, "Run out of material slots");
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

Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetIDFromMaterialPointer(const Flux_Material* pxMaterial)
{
	if (!pxMaterial) return INVALID_ASSET_ID;

	// Check if pointer is within the material array bounds
	if (pxMaterial < s_pxMaterials || pxMaterial >= s_pxMaterials + ZENITH_MAX_MATERIALS)
	{
		return INVALID_ASSET_ID;
	}

	return static_cast<AssetID>(pxMaterial - s_pxMaterials);
}
