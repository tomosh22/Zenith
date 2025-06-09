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

	static Flux_Texture* CreateDummyTexture(const std::string& strName);
	static Flux_Texture* CreateColourAttachment(const std::string& strName, uint32_t uWidth, uint32_t uHeight, ColourFormat eFormat, uint32_t uBitsPerPixel);
	static Flux_Texture* CreateDepthStencilAttachment(const std::string& strName, uint32_t uWidth, uint32_t uHeight, DepthStencilFormat eFormat, uint32_t uBitsPerPixel);
	static Flux_Texture* AddTexture2D(const std::string& strName, const void* pData, const uint32_t uWidth, const uint32_t uHeight, const uint32_t uDepth, ColourFormat eFormat, DepthStencilFormat eDepthStencilFormat, bool bCreateMips);
	static Flux_Texture* AddTexture2D(const std::string& strName, const char* szPath);
	static Flux_Texture* AddTextureCube(const std::string& strName, const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ);
	static Flux_MeshGeometry& AddMesh(const std::string& strName, const char* szPath, const bool bRetainPositionsAndNormals = false);
	static Flux_Material& AddMaterial(const std::string& strName);

	static Flux_Texture* GetTexture(const std::string& strName);
	static Flux_Texture* TryGetTexture(const std::string& strName);
	static bool TextureExists(const std::string& strName);

	static Flux_MeshGeometry& GetMesh(const std::string& strName);
	static Flux_MeshGeometry& TryGetMesh(const std::string& strName);
	static bool MeshExists(const std::string& strName);

	static Flux_Material& GetMaterial(const std::string& strName);
	static Flux_Material& TryGetMaterial(const std::string& strName);
	static bool MaterialExists(const std::string& strName);

	static void DeleteTexture(const std::string& strName);
	static void DeleteMesh(const std::string& strName);
	static void DeleteMaterial(const std::string& strName);

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

	static std::unordered_map<std::string, AssetID> s_xTextureNameMap;
	static std::unordered_set<AssetID>				s_xUsedTextureIDs;

	static std::unordered_map<std::string, AssetID> s_xMeshNameMap;
	static std::unordered_set<AssetID>				s_xUsedMeshIDs;

	static std::unordered_map<std::string, AssetID> s_xMaterialNameMap;
	static std::unordered_set<AssetID>				s_xUsedMaterialIDs;
};
