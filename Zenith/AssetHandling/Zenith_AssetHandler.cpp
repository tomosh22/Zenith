#include "Zenith.h"
#include "AssetHandling/Zenith_AssetHandler.h"
#include "Flux/Flux_Graphics.h"

Flux_Texture* Zenith_AssetHandler::s_pxTextures = new Flux_Texture[ZENITH_MAX_TEXTURES];
Flux_MeshGeometry* Zenith_AssetHandler::s_pxMeshes = new Flux_MeshGeometry[ZENITH_MAX_MESHES];

std::unordered_map<Zenith_GUID, Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xTextureMap;
std::map<Zenith_AssetHandler::AssetID, Zenith_GUID> Zenith_AssetHandler::s_xReverseTextureMap;
std::unordered_map<Zenith_GUID, Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xMeshMap;
std::map<Zenith_AssetHandler::AssetID, Zenith_GUID> Zenith_AssetHandler::s_xReverseMeshMap;
std::unordered_map<std::string, Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xTextureNameMap;
std::unordered_map<std::string, Zenith_AssetHandler::AssetID> Zenith_AssetHandler::s_xMeshNameMap;

#if 0
void Zenith_AssetHandler::LoadMeshCache()
{
	for (auto& xFile : std::filesystem::directory_iterator(MESHCACHEDIR))
	{
		const wchar_t* wszChecksum = xFile.path().c_str();
		size_t ulLength = wcslen(wszChecksum);
		char* szChecksum = new char[ulLength + 1];
		wcstombs(szChecksum, wszChecksum, ulLength);
		szChecksum[ulLength] = '\0';
		s_xMeshChecksums.insert(szChecksum + strlen(MESHCACHEDIR.c_str()));
		delete[] szChecksum;
	}
}

void Zenith_AssetHandler::LoadTextureCache()
{
	for (auto& xFile : std::filesystem::directory_iterator(TEXTURECACHEDIR))
	{
		const wchar_t* wszChecksum = xFile.path().c_str();
		size_t ulLength = wcslen(wszChecksum);
		char* szChecksum = new char[ulLength + 1];
		wcstombs(szChecksum, wszChecksum, ulLength);
		szChecksum[ulLength] = '\0';
		s_xTextureChecksums.insert(szChecksum + strlen(TEXTURECACHEDIR.c_str()));
		delete[] szChecksum;
	}
}
#endif

void Zenith_AssetHandler::LoadAssetsFromFile(const std::string& strFile)
{
#if 0
	std::ifstream xIn(strFile);
	std::string strLine;
	while (std::getline(xIn, strLine))
	{
		if (strLine == "Texture2D")
		{
			std::string strGUID;
			std::string strStreamPrio;
			std::string strFile;
			std::getline(xIn, strGUID);
			std::getline(xIn, strStreamPrio);
			std::getline(xIn, strFile);
			Zenith_GUID xGUID(strtoull(strGUID.c_str(), nullptr, 10));
			AddTexture2D(xGUID, strFile, (TextureStreamPriority)std::stoi(strStreamPrio));
		}
		if (strLine == "Material")
		{
			std::string strName;
			std::string strGUID;
			std::string strAlbedoGUID;
			std::string strBumpMapGUID;
			std::string strRoughnessTexGUID;
			std::string strMetallicTexGUID;
			std::string strHeightmapTexGUID;
			std::getline(xIn, strName);
			std::getline(xIn, strGUID);
			std::getline(xIn, strAlbedoGUID);
			std::getline(xIn, strBumpMapGUID);
			std::getline(xIn, strRoughnessTexGUID);
			std::getline(xIn, strMetallicTexGUID);
			std::getline(xIn, strHeightmapTexGUID);
			Zenith_GUID xGUID(strtoull(strGUID.c_str(), nullptr, 10));
			Zenith_GUID xAlbedoGUID(strtoull(strAlbedoGUID.c_str(), nullptr, 10));
			Zenith_GUID xBumpMapGUID(strtoull(strBumpMapGUID.c_str(), nullptr, 10));
			Zenith_GUID xRoughnessTexGUID(strtoull(strRoughnessTexGUID.c_str(), nullptr, 10));
			Zenith_GUID xMetallicTexGUID(strtoull(strMetallicTexGUID.c_str(), nullptr, 10));
			Zenith_GUID xHeightmapGUID(strtoull(strHeightmapTexGUID.c_str(), nullptr, 10));
			AddMaterial(xGUID, strName, xAlbedoGUID, xBumpMapGUID, xRoughnessTexGUID, xMetallicTexGUID, xHeightmapGUID);
		}
		if (strLine == "FoliageMaterial")
		{
			std::string strName;
			std::string strGUID;
			std::string strAlbedoGUID;
			std::string strBumpMapGUID;
			std::string strRoughnessTexGUID;
			std::string strHeightmapTexGUID;
			std::string strAlphaTexGUID;
			std::string strTranslucencyTexGUID;
			std::getline(xIn, strName);
			std::getline(xIn, strGUID);
			std::getline(xIn, strAlbedoGUID);
			std::getline(xIn, strBumpMapGUID);
			std::getline(xIn, strRoughnessTexGUID);
			std::getline(xIn, strHeightmapTexGUID);
			std::getline(xIn, strAlphaTexGUID);
			std::getline(xIn, strTranslucencyTexGUID);
			Zenith_GUID xGUID(strtoull(strGUID.c_str(), nullptr, 10));
			Zenith_GUID xAlbedoGUID(strtoull(strAlbedoGUID.c_str(), nullptr, 10));
			Zenith_GUID xBumpMapGUID(strtoull(strBumpMapGUID.c_str(), nullptr, 10));
			Zenith_GUID xRoughnessTexGUID(strtoull(strRoughnessTexGUID.c_str(), nullptr, 10));
			Zenith_GUID xHeightmapGUID(strtoull(strHeightmapTexGUID.c_str(), nullptr, 10));
			Zenith_GUID xAlphaGUID(strtoull(strAlphaTexGUID.c_str(), nullptr, 10));
			Zenith_GUID xTranslucencyGUID(strtoull(strTranslucencyTexGUID.c_str(), nullptr, 10));
			AddFoliageMaterial(xGUID, strName, xAlbedoGUID, xBumpMapGUID, xRoughnessTexGUID, xHeightmapGUID, xAlphaGUID, xTranslucencyGUID);
		}
		if (strLine == "Mesh")
		{
			std::string strGUID;
			std::string strFile;
			std::getline(xIn, strGUID);
			std::getline(xIn, strFile);
			Zenith_GUID xGUID(strtoull(strGUID.c_str(), nullptr, 10));
			AddMesh(xGUID, strFile);
		}
	}
#endif
}


void Zenith_AssetHandler::AddTexture2D(Zenith_GUID xGUID, const std::string& strName, const char* szPath)
{
	AssetID uID = GetNextFreeTextureSlot();
	Flux_Texture& xTex = s_pxTextures[uID];
	s_xTextureMap.insert({ xGUID,uID});
	s_xTextureNameMap.insert({ strName, uID });
	s_xReverseTextureMap.insert({ uID, xGUID });
	Flux_MemoryManager::CreateTexture(szPath, xTex);
}
void Zenith_AssetHandler::AddTextureCube(Zenith_GUID xGUID, const std::string& strName, const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ)
{
	AssetID uID = GetNextFreeTextureSlot();
	Flux_Texture& xTex = s_pxTextures[uID];
	s_xTextureMap.insert({ xGUID,uID });
	s_xTextureNameMap.insert({ strName, uID });
	s_xReverseTextureMap.insert({ uID, xGUID });
	Flux_MemoryManager::CreateTextureCube(szPathPX, szPathNX, szPathPY, szPathNY, szPathPZ, szPathNZ, xTex);
}
void Zenith_AssetHandler::AddMesh(Zenith_GUID xGUID, const std::string& strName, const char* szPath)
{
	AssetID uID = GetNextFreeMeshSlot();
	Flux_MeshGeometry& xMesh = s_pxMeshes[uID];
	s_xMeshMap.insert({ xGUID,uID });
	s_xMeshNameMap.insert({ strName, uID });
	s_xReverseMeshMap.insert({ uID, xGUID });
	Flux_MeshGeometry::LoadFromFile(szPath, xMesh);
}

Flux_Texture& Zenith_AssetHandler::GetTexture(Zenith_GUID xGUID)
{
	Zenith_Assert(s_xTextureMap.find(xGUID) != s_xTextureMap.end(), "Texture2D doesn't exist");
	return s_pxTextures[s_xTextureMap.at(xGUID)];
}
Flux_Texture& Zenith_AssetHandler::TryGetTexture(Zenith_GUID xGUID)
{
	if (s_xTextureMap.find(xGUID) != s_xTextureMap.end())
	{
		return s_pxTextures[s_xTextureMap.at(xGUID)];
	}
	else
	{
		return Flux_Graphics::s_xBlankTexture2D;
	}
}
Flux_MeshGeometry& Zenith_AssetHandler::GetMesh(Zenith_GUID xGUID)
{
	Zenith_Assert(s_xMeshMap.find(xGUID) != s_xMeshMap.end(), "Mesh doesn't exist");
	return s_pxMeshes[s_xMeshMap.at(xGUID)];
}
Flux_MeshGeometry& Zenith_AssetHandler::TryGetMesh(Zenith_GUID xGUID)
{
	if (s_xMeshMap.find(xGUID) != s_xMeshMap.end())
	{
		return s_pxMeshes[s_xMeshMap.at(xGUID)];
	}
	else
	{
		return Flux_Graphics::s_xBlankMesh;
	}
}

Flux_Texture& Zenith_AssetHandler::GetTexture(const std::string& strName)
{
	Zenith_Assert(s_xTextureNameMap.find(strName) != s_xTextureNameMap.end(), "Texture2D doesn't exist");
	return s_pxTextures[s_xTextureNameMap.at(strName)];
}

Flux_Texture& Zenith_AssetHandler::TryGetTexture(const std::string& strName)
{
	if (s_xTextureNameMap.find(strName) != s_xTextureNameMap.end())
	{
		return s_pxTextures[s_xTextureNameMap.at(strName)];
	}
	else
	{
		return Flux_Graphics::s_xBlankTexture2D;
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

void Zenith_AssetHandler::DeleteTexture(Zenith_GUID xGUID)
{
	Zenith_Assert(s_xTextureMap.find(xGUID) != s_xTextureMap.end(), "Texture2D doesn't exist");
	s_xTextureMap.erase(xGUID);
}
void Zenith_AssetHandler::DeleteMesh(Zenith_GUID xGUID)
{
	Zenith_Assert(s_xMeshMap.find(xGUID) != s_xMeshMap.end(), "Mesh doesn't exist");
	s_xMeshMap.erase(xGUID);
}

Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetNextFreeTextureSlot()
{
	for (AssetID u = 0; u < ZENITH_MAX_TEXTURES; u++)
	{
		if (s_xReverseTextureMap.find(u) == s_xReverseTextureMap.end())
		{
			return u;
		}
	}
}

Zenith_AssetHandler::AssetID Zenith_AssetHandler::GetNextFreeMeshSlot()
{
	for (AssetID u = 0; u < ZENITH_MAX_MESHES; u++)
	{
		if (s_xReverseMeshMap.find(u) == s_xReverseMeshMap.end())
		{
			return u;
		}
	}
}
