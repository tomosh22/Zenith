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

	struct TextureData
	{
		union
		{
			const void* apCubeFaceData[6] = { nullptr };  // Pointers to 6 cube face data (for cube textures)
			const void* pData;              // Pointer to texture data (for 2D textures)
		};
		Flux_SurfaceInfo xSurfaceInfo;
		bool bCreateMips = false;
		bool bIsCubemap = false;
		
		// Helper to free allocated memory
		void FreeAllocatedData()
		{
			if (bIsCubemap)
			{
				for (uint32_t u = 0; u < 6; u++)
				{
					if (apCubeFaceData[u])
					{
						Zenith_MemoryManagement::Deallocate(const_cast<void*>(apCubeFaceData[u]));
					}
				}
			}
			else if (pData)
			{
				Zenith_MemoryManagement::Deallocate(const_cast<void*>(pData));
			}
		}
	};

	// Unified texture addition function
	static Flux_Texture AddTexture(const std::string& strName, const TextureData& xTextureData);
	
	// Helper functions to load texture data from files
	static TextureData LoadTexture2DFromFile(const char* szPath);
	static TextureData LoadTextureCubeFromFiles(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ);
	static Flux_MeshGeometry& AddMesh(const std::string& strName);
	static Flux_MeshGeometry& AddMesh(const std::string& strName, const char* szPath, u_int uRetainAttributeBits = 0, const bool bUploadToGPU = true);
	static Flux_Material& AddMaterial(const std::string& strName);

	static Flux_Texture GetTexture(const std::string& strName);
	static Flux_Texture TryGetTexture(const std::string& strName);
	static bool TextureExists(const std::string& strName);
	
	static Flux_ShaderResourceView* GetTextureSRV(const std::string& strName);
	static Flux_ShaderResourceView* TryGetTextureSRV(const std::string& strName);
	static Flux_ShaderResourceView* GetTextureSRVByHandle(uint32_t uVRAMHandle);
	static Flux_ShaderResourceView* TryGetTextureSRVByHandle(uint32_t uVRAMHandle);

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

	//array of length ZENITH_MAX_MESHES
	static Flux_MeshGeometry* s_pxMeshes;
	static AssetID GetNextFreeMeshSlot();

	//array of length ZENITH_MAX_MATERIALS
	static Flux_Material* s_pxMaterials;
	static AssetID GetNextFreeMaterialSlot();

	static std::unordered_map<std::string, uint32_t> s_xTextureNameMap;
	static std::unordered_set<AssetID>				s_xUsedTextureIDs;
	static std::unordered_map<std::string, Flux_ShaderResourceView> s_xTextureSRVMap;
	static std::unordered_map<uint32_t, Flux_ShaderResourceView> s_xTextureHandleToSRVMap;

	static std::unordered_map<std::string, AssetID> s_xMeshNameMap;
	static std::unordered_set<AssetID>				s_xUsedMeshIDs;

	static std::unordered_map<std::string, AssetID> s_xMaterialNameMap;
	static std::unordered_set<AssetID>				s_xUsedMaterialIDs;
};
