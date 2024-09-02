#pragma once
#include "Flux/Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_Material.h"
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

	static Flux_Texture& AddTexture2D(Zenith_GUID xGUID, const std::string& strName, const char* szPath);
	static Flux_Texture& AddTextureCube(Zenith_GUID xGUID, const std::string& strName, const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ);
	static Flux_MeshGeometry& AddMesh(Zenith_GUID xGUID, const std::string& strName, const char* szPath);
	static Flux_Material& AddMaterial(Zenith_GUID xGUID, const std::string& strName);

	static Flux_Texture& GetTexture(Zenith_GUID xGUID);
	static Flux_Texture& TryGetTexture(Zenith_GUID xGUID);
	static Flux_MeshGeometry& GetMesh(Zenith_GUID xGUID);
	static Flux_MeshGeometry& TryGetMesh(Zenith_GUID xZGUID);
	static Flux_Material& GetMaterial(Zenith_GUID xGUID);
	static Flux_Material& TryGetMaterial(Zenith_GUID xZGUID);

	static Flux_Texture& GetTexture(const std::string& strName);
	static Flux_Texture& TryGetTexture(const std::string& strName);
	static bool TextureExists(const std::string& strName);

	static Flux_MeshGeometry& GetMesh(const std::string& strName);
	static Flux_MeshGeometry& TryGetMesh(const std::string& strName);
	static bool MeshExists(const std::string& strName);

	static Flux_Material& GetMaterial(const std::string& strName);
	static Flux_Material& TryGetMaterial(const std::string& strName);
	static bool MaterialExists(const std::string& strName);

	static void DeleteTexture(Zenith_GUID xGUID);
	static void DeleteMesh(Zenith_GUID xGUID);
	static void DeleteMaterial(Zenith_GUID xGUID);

private:
	//array of length ZENITH_MAX_TEXTURES
	static Flux_Texture* s_pxTextures;
	static AssetID GetNextFreeTextureSlot();

	//array of length ZENITH_MAX_MESHES
	static Flux_MeshGeometry* s_pxMeshes;
	static AssetID GetNextFreeMeshSlot();

	//array of length ZENITH_MAX_MATERIALS
	static Flux_Material* s_pxMaterials;
	static AssetID GetNextFreeMaterialSlot();

	static std::unordered_map<Zenith_GUID, AssetID> s_xTextureMap;
	static std::map<AssetID, Zenith_GUID> s_xReverseTextureMap;
	static std::unordered_map<std::string, AssetID> s_xTextureNameMap;

	static std::unordered_map<Zenith_GUID, AssetID> s_xMeshMap;
	static std::map<AssetID, Zenith_GUID> s_xReverseMeshMap;
	static std::unordered_map<std::string, AssetID> s_xMeshNameMap;

	static std::unordered_map<Zenith_GUID, AssetID> s_xMaterialMap;
	static std::map<AssetID, Zenith_GUID> s_xReverseMaterialMap;
	static std::unordered_map<std::string, AssetID> s_xMaterialNameMap;
};
