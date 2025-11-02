#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"

Flux_MeshGeometry* Zenith_AssetHandler::s_pxMeshes = new Flux_MeshGeometry[ZENITH_MAX_MESHES];
Flux_Material* Zenith_AssetHandler::s_pxMaterials = new Flux_Material[ZENITH_MAX_MATERIALS];

std::unordered_map<std::string, uint32_t> Zenith_AssetHandler::s_xTextureNameMap;
std::unordered_set<Zenith_AssetHandler::AssetID>	Zenith_AssetHandler::s_xUsedTextureIDs;
std::unordered_map<std::string, Flux_ShaderResourceView> Zenith_AssetHandler::s_xTextureSRVMap;
std::unordered_map<uint32_t, Flux_ShaderResourceView> Zenith_AssetHandler::s_xTextureHandleToSRVMap;

std::unordered_map<std::string, Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xMeshNameMap;
std::unordered_set<Zenith_AssetHandler::AssetID>	Zenith_AssetHandler::s_xUsedMeshIDs;

std::unordered_map<std::string, Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xMaterialNameMap;
std::unordered_set<Zenith_AssetHandler::AssetID>	Zenith_AssetHandler::s_xUsedMaterialIDs;

Flux_Texture Zenith_AssetHandler::AddTexture2D(const std::string& strName, const void* pData, const Flux_SurfaceInfo& xInfo, bool bCreateMips)
{
	Flux_VRAMHandle xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(pData, xInfo, bCreateMips);
	s_xTextureNameMap.insert({ strName, xVRAMHandle.AsUInt()});
	
	// Create and store SRV
	Flux_ShaderResourceView xSRV;
	xSRV.m_xVRAMHandle = xVRAMHandle;
	xSRV.m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xVRAMHandle, xInfo);
	s_xTextureSRVMap.insert({ strName, xSRV });
	s_xTextureHandleToSRVMap.insert({ xVRAMHandle.AsUInt(), xSRV});
	
	// Create Flux_Texture to return
	Flux_Texture xTexture;
	xTexture.m_xSurfaceInfo = xInfo;
	xTexture.m_xVRAMHandle = xVRAMHandle;
	xTexture.m_xSRV = xSRV;
	
	return xTexture;
}

Flux_Texture Zenith_AssetHandler::AddTexture2D(const std::string& strName, const char* szPath)
{
	Flux_SurfaceInfo xInfo;
	Flux_VRAMHandle xVRAMHandle = Flux_MemoryManager::CreateTextureVRAM(szPath, &xInfo);
	s_xTextureNameMap.insert({ strName, xVRAMHandle.AsUInt()});
	
	// Create and store SRV
	Flux_ShaderResourceView xSRV;
	xSRV.m_xVRAMHandle = xVRAMHandle;
	xSRV.m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xVRAMHandle, xInfo);
	s_xTextureSRVMap.insert({ strName, xSRV });
	s_xTextureHandleToSRVMap.insert({ xVRAMHandle.AsUInt(), xSRV});
	
	// Create Flux_Texture to return
	Flux_Texture xTexture;
	xTexture.m_xSurfaceInfo = xInfo;
	xTexture.m_xVRAMHandle = xVRAMHandle;
	xTexture.m_xSRV = xSRV;
	
	return xTexture;
}

Flux_Texture Zenith_AssetHandler::AddTextureCube(const std::string& strName, const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ)
{
	Flux_SurfaceInfo xInfo;
	Flux_VRAMHandle xVRAMHandle = Flux_MemoryManager::CreateTextureCubeVRAM(szPathPX, szPathNX, szPathPY, szPathNY, szPathPZ, szPathNZ, &xInfo);
	s_xTextureNameMap.insert({ strName, xVRAMHandle.AsUInt()});
	
	// Create and store SRV
	Flux_ShaderResourceView xSRV;
	xSRV.m_xVRAMHandle = xVRAMHandle;
	xSRV.m_xImageView = Flux_MemoryManager::CreateShaderResourceView(xVRAMHandle, xInfo);
	s_xTextureSRVMap.insert({ strName, xSRV });
	s_xTextureHandleToSRVMap.insert({ xVRAMHandle.AsUInt(), xSRV});
	
	// Create Flux_Texture to return
	Flux_Texture xTexture;
	xTexture.m_xSurfaceInfo = xInfo;
	xTexture.m_xVRAMHandle = xVRAMHandle;
	xTexture.m_xSRV = xSRV;
	
	return xTexture;
}
Flux_MeshGeometry& Zenith_AssetHandler::AddMesh(const std::string& strName)
{
	AssetID uID = GetNextFreeMeshSlot();
	Flux_MeshGeometry& xMesh = s_pxMeshes[uID];
	s_xMeshNameMap.insert({ strName, uID });
	s_xUsedMeshIDs.insert(uID);
	return s_pxMeshes[uID];
}
Flux_MeshGeometry& Zenith_AssetHandler::AddMesh(const std::string& strName, const char* szPath, u_int uRetainAttributeBits /*= 0*/, const bool bUploadToGPU /*= true*/)
{
	AssetID uID = GetNextFreeMeshSlot();
	Flux_MeshGeometry& xMesh = s_pxMeshes[uID];
	s_xMeshNameMap.insert({ strName, uID });
	Flux_MeshGeometry::LoadFromFile(szPath, xMesh, uRetainAttributeBits, bUploadToGPU);
	s_xUsedMeshIDs.insert(uID);
	return s_pxMeshes[uID];
}

Flux_Material& Zenith_AssetHandler::AddMaterial(const std::string& strName)
{
	AssetID uID = GetNextFreeMaterialSlot();
	s_xMaterialNameMap.insert({ strName, uID });
	s_xUsedMaterialIDs.insert(uID);
	return s_pxMaterials[uID];
}

Flux_Texture Zenith_AssetHandler::GetTexture(const std::string& strName)
{
	Zenith_Assert(s_xTextureNameMap.find(strName) != s_xTextureNameMap.end(), "Texture2D doesn't exist");
	
	Flux_ShaderResourceView* pxSRV = GetTextureSRV(strName);
	
	Flux_Texture xTexture;
	xTexture.m_xVRAMHandle.SetValue(s_xTextureNameMap.at(strName));
	xTexture.m_xSRV = *pxSRV;
	xTexture.m_xSurfaceInfo = pxSRV->m_xVRAMHandle.IsValid() ? Flux_SurfaceInfo{} : Flux_SurfaceInfo{};
	
	return xTexture;
}

Flux_Texture Zenith_AssetHandler::TryGetTexture(const std::string& strName)
{
	if (s_xTextureNameMap.find(strName) != s_xTextureNameMap.end())
	{
		return GetTexture(strName);
	}
	else
	{
		return Flux_Graphics::s_xBlackBlankTexture2D;
	}
}

bool Zenith_AssetHandler::TextureExists(const std::string& strName)
{
	return s_xTextureNameMap.find(strName) != s_xTextureNameMap.end();
}

Flux_ShaderResourceView* Zenith_AssetHandler::GetTextureSRV(const std::string& strName)
{
	Zenith_Assert(s_xTextureSRVMap.find(strName) != s_xTextureSRVMap.end(), "Texture SRV doesn't exist");
	return &s_xTextureSRVMap.at(strName);
}

Flux_ShaderResourceView* Zenith_AssetHandler::TryGetTextureSRV(const std::string& strName)
{
	if (s_xTextureSRVMap.find(strName) != s_xTextureSRVMap.end())
	{
		return &s_xTextureSRVMap.at(strName);
	}
	else
	{
		return &Flux_Graphics::s_xBlackBlankTexture2D.m_xSRV;
	}
}

Flux_MeshGeometry& Zenith_AssetHandler::GetMesh(const std::string& strName)
{
	Zenith_Assert(s_xMeshNameMap.find(strName) != s_xMeshNameMap.end(), "Mesh doesn't exist");
	return s_pxMeshes[s_xMeshNameMap.at(strName)];
}

Flux_MeshGeometry& Zenith_AssetHandler::TryGetMesh(const std::string& strName)
{
	if (s_xMeshNameMap.find(strName) != s_xMeshNameMap.end())
	{
		return s_pxMeshes[s_xMeshNameMap.at(strName)];
	}
	else
	{
		return Flux_Graphics::s_xBlankMesh;
	}
}

bool Zenith_AssetHandler::MeshExists(const std::string& strName)
{
	return s_xMeshNameMap.find(strName) != s_xMeshNameMap.end();
}

Flux_Material& Zenith_AssetHandler::GetMaterial(const std::string& strName)
{
	Zenith_Assert(s_xMaterialNameMap.find(strName) != s_xMaterialNameMap.end(), "Material doesn't exist");
	return s_pxMaterials[s_xMaterialNameMap.at(strName)];
}

Flux_Material& Zenith_AssetHandler::TryGetMaterial(const std::string& strName)
{
	if (s_xMaterialNameMap.find(strName) != s_xMaterialNameMap.end())
	{
		return s_pxMaterials[s_xMaterialNameMap.at(strName)];
	}
	else
	{
		return *Flux_Graphics::s_pxBlankMaterial;
	}
}

bool Zenith_AssetHandler::MaterialExists(const std::string& strName)
{
	return s_xMaterialNameMap.find(strName) != s_xMaterialNameMap.end();
}


void Zenith_AssetHandler::DeleteTexture(const std::string& strName)
{
	STUBBED
}

void Zenith_AssetHandler::DeleteMesh(const std::string& strName)
{
	Zenith_Assert(s_xMeshNameMap.find(strName) != s_xMeshNameMap.end(), "Mesh doesn't exist");
	AssetID uID = s_xMeshNameMap.find(strName)->second;
	s_pxMeshes[uID].Reset();
	s_xMeshNameMap.erase(strName);
}

void Zenith_AssetHandler::DeleteMaterial(const std::string& strName)
{
	Zenith_Assert(s_xMaterialNameMap.find(strName) != s_xMaterialNameMap.end(), "Material doesn't exist");
	AssetID uID = s_xMaterialNameMap.find(strName)->second;
	s_pxMaterials[uID].Reset();
	s_xMaterialNameMap.erase(strName);
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
	return -1;
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
	return -1;
}

Flux_ShaderResourceView* Zenith_AssetHandler::GetTextureSRVByHandle(uint32_t uVRAMHandle)
{
	auto it = s_xTextureHandleToSRVMap.find(uVRAMHandle);
	Zenith_Assert(it != s_xTextureHandleToSRVMap.end(), "Texture handle SRV doesn't exist");
	return &it->second;
}

Flux_ShaderResourceView* Zenith_AssetHandler::TryGetTextureSRVByHandle(uint32_t uVRAMHandle)
{
	auto it = s_xTextureHandleToSRVMap.find(uVRAMHandle);
	if (it != s_xTextureHandleToSRVMap.end())
	{
		return &it->second;
	}
	return nullptr;
}