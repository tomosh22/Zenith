#pragma once
#include "Flux/Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_Material.h"

/**
 * Zenith_AssetHandler - Raw Pointer-Based Asset Management
 *
 * OWNERSHIP RULES:
 * - All Add* functions allocate from internal pools and return raw pointers
 * - Callers MUST hold onto returned pointers for later use
 * - NO string-based registry - assets are accessed only via returned pointers
 * - Delete* functions take pointers and return the asset to the pool
 * - DestroyAllAssets() cleans up all assets at shutdown
 *
 * LIFECYCLE:
 * - Create: Add*() -> returns raw pointer
 * - Use: Caller stores and uses the pointer directly
 * - Destroy: Delete*() or DestroyAllAssets()
 *
 * SERIALIZATION:
 * - Flux_MeshGeometry stores m_strSourcePath when loaded from file
 * - Serialization should save source paths, deserialization should reload
 *
 * THREAD SAFETY:
 * - Not thread-safe by default
 * - Callers must synchronize if using from multiple threads
 */
class Zenith_AssetHandler
{
public:
	using AssetID = uint32_t;
	static constexpr AssetID INVALID_ASSET_ID = ~0u;

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

	//--------------------------------------------------------------------------
	// File Loading (disk paths only, no in-memory string keys)
	//--------------------------------------------------------------------------
	static TextureData LoadTexture2DFromFile(const char* szPath);
	static TextureData LoadTextureCubeFromFiles(const char* szPathPX, const char* szPathNX, const char* szPathPY, const char* szPathNY, const char* szPathPZ, const char* szPathNZ);

	//--------------------------------------------------------------------------
	// Asset Creation - Returns raw pointers, caller must store
	//--------------------------------------------------------------------------

	/**
	 * Creates a texture from loaded texture data
	 * @param xTextureData The texture data (loaded from file or procedural)
	 * @return Raw pointer to the created texture, or nullptr on failure
	 */
	static Flux_Texture* AddTexture(const TextureData& xTextureData);

	/**
	 * Creates an empty mesh for manual setup
	 * @return Raw pointer to the created mesh, or nullptr on failure
	 */
	static Flux_MeshGeometry* AddMesh();

	/**
	 * Loads a mesh from file
	 * @param szPath Path to the mesh file (stored in mesh for serialization)
	 * @param uRetainAttributeBits Bitmask of attributes to retain in CPU memory
	 * @param bUploadToGPU Whether to upload to GPU
	 * @return Raw pointer to the loaded mesh, or nullptr on failure
	 */
	static Flux_MeshGeometry* AddMeshFromFile(const char* szPath, u_int uRetainAttributeBits = 0, const bool bUploadToGPU = true);

	/**
	 * Finds a mesh by its source path (for reuse instead of creating duplicates)
	 * @param strPath Source path to search for
	 * @return Pointer to mesh if found, nullptr otherwise
	 */
	static Flux_MeshGeometry* GetMeshByPath(const std::string& strPath);

	/**
	 * Creates an empty material
	 * @return Raw pointer to the created material, or nullptr on failure
	 */
	static Flux_Material* AddMaterial();

	//--------------------------------------------------------------------------
	// Asset Deletion - By pointer
	//--------------------------------------------------------------------------

	/**
	 * Deletes a texture and returns its slot to the pool
	 * @param pxTexture Pointer to the texture to delete
	 */
	static void DeleteTexture(Flux_Texture* pxTexture);

	/**
	 * Finds and deletes a texture by its source path
	 * @param strPath Source path of the texture to delete
	 * @return true if texture was found and deleted, false otherwise
	 */
	static bool DeleteTextureByPath(const std::string& strPath);

	/**
	 * Finds a texture by its source path (for reuse instead of creating duplicates)
	 * @param strPath Source path to search for
	 * @return Pointer to texture if found, nullptr otherwise
	 */
	static Flux_Texture* GetTextureByPath(const std::string& strPath);

	/**
	 * Deletes a mesh and returns its slot to the pool
	 * @param pxMesh Pointer to the mesh to delete
	 */
	static void DeleteMesh(Flux_MeshGeometry* pxMesh);

	/**
	 * Deletes a material and returns its slot to the pool
	 * @param pxMaterial Pointer to the material to delete
	 */
	static void DeleteMaterial(Flux_Material* pxMaterial);

	/**
	 * Finds a material by its diffuse texture path (for reuse instead of creating duplicates)
	 * @param strDiffusePath Diffuse texture path to search for
	 * @return Pointer to material if found, nullptr otherwise
	 */
	static Flux_Material* GetMaterialByDiffusePath(const std::string& strDiffusePath);

	//--------------------------------------------------------------------------
	// Bulk Operations
	//--------------------------------------------------------------------------

	/**
	 * Destroys all assets - call at shutdown
	 * Iterates through all pools and cleans up active assets
	 */
	static void DestroyAllAssets();

	//--------------------------------------------------------------------------
	// Diagnostics & Debugging
	//--------------------------------------------------------------------------

	/**
	 * Enable/disable lifecycle logging for debugging
	 */
	static void EnableLifecycleLogging(bool bEnable);
	static bool IsLifecycleLoggingEnabled() { return s_bLifecycleLoggingEnabled; }

	/**
	 * Get count of active (allocated) assets for leak detection
	 */
	static uint32_t GetActiveTextureCount();
	static uint32_t GetActiveMeshCount();
	static uint32_t GetActiveMaterialCount();

	/**
	 * Log all active assets (for debugging memory leaks)
	 */
	static void LogActiveAssets();

	//--------------------------------------------------------------------------
	// Pointer Validation (debug helpers)
	//--------------------------------------------------------------------------

	/**
	 * Check if a pointer points to a valid, active asset
	 */
	static bool IsValidTexture(const Flux_Texture* pxTexture);
	static bool IsValidMesh(const Flux_MeshGeometry* pxMesh);
	static bool IsValidMaterial(const Flux_Material* pxMaterial);

private:
	// Asset pools - fixed-size arrays for all assets
	static Flux_Texture* s_pxTextures;          // Array of ZENITH_MAX_TEXTURES
	static Flux_MeshGeometry* s_pxMeshes;       // Array of ZENITH_MAX_MESHES
	static Flux_Material* s_pxMaterials;        // Array of ZENITH_MAX_MATERIALS

	// Track which slots are in use (no string keys, just ID sets)
	static std::unordered_set<AssetID> s_xUsedTextureIDs;
	static std::unordered_set<AssetID> s_xUsedMeshIDs;
	static std::unordered_set<AssetID> s_xUsedMaterialIDs;

	// Slot allocation helpers
	static AssetID GetNextFreeTextureSlot();
	static AssetID GetNextFreeMeshSlot();
	static AssetID GetNextFreeMaterialSlot();

	// Pointer-to-ID conversion (for deletion)
	static AssetID GetIDFromTexturePointer(const Flux_Texture* pxTexture);
	static AssetID GetIDFromMeshPointer(const Flux_MeshGeometry* pxMesh);
	static AssetID GetIDFromMaterialPointer(const Flux_Material* pxMaterial);

	// Lifecycle logging flag
	static bool s_bLifecycleLoggingEnabled;
};
