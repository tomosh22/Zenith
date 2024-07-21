#pragma once
#include "Flux/Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
class Zenith_AssetHandler
{
public:
	using AssetID = uint32_t;
	Zenith_AssetHandler()
	{
#if 0
		LoadMeshCache();
		LoadTextureCache();
#endif
	}
	~Zenith_AssetHandler()
	{
	}

	static void LoadAssetsFromFile(const std::string& strFile);

	static void AddTexture(Zenith_GUID xGUID, const std::string& strName, const char* szPath);
	static void AddMesh(Zenith_GUID xGUID, const std::string& strName, const char* szPath);

	static Flux_Texture& GetTexture(Zenith_GUID xGUID);
	static Flux_Texture& TryGetTexture(Zenith_GUID xGUID);
	static Flux_MeshGeometry& GetMesh(Zenith_GUID xGUID);
	static Flux_MeshGeometry& TryGetMesh(Zenith_GUID xZGUID);

	static Flux_Texture& GetTexture(const std::string& strName);
	static Flux_Texture& TryGetTexture(const std::string& strName);
	static Flux_MeshGeometry& GetMesh(const std::string& strName);
	static Flux_MeshGeometry& TryGetMesh(const std::string& strName);

	static void DeleteTexture(Zenith_GUID xGUID);
	static void DeleteMesh(Zenith_GUID xGUID);

private:
	//array of length ZENITH_MAX_TEXTURES
	static Flux_Texture* s_pxTextures;
	static AssetID GetNextFreeTextureSlot();

	//array of length ZENITH_MAX_MESHES
	static Flux_MeshGeometry* s_pxMeshes;
	static AssetID GetNextFreeMeshSlot();

	static std::unordered_map<Zenith_GUID, AssetID> s_xTextureMap;
	static std::unordered_map<Zenith_GUID, AssetID> s_xMeshMap;
	static std::unordered_map<std::string, AssetID> s_xTextureNameMap;
	static std::unordered_map<std::string, AssetID> s_xMeshNameMap;
};
