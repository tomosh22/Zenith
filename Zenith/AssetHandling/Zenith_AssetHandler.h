#pragma once
#include "Flux/Flux.h"
#include "Flux/MeshGeometry/Flux_MeshGeometry.h"
#include "Flux/Flux_MaterialAsset.h"
#include "AssetHandling/Zenith_MeshAsset.h"
#include "AssetHandling/Zenith_SkeletonAsset.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "Flux/Flux_ModelInstance.h"
#include "Flux/MeshGeometry/Flux_MeshInstance.h"
#include "Flux/MeshAnimation/Flux_SkeletonInstance.h"

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

	// NOTE: Materials are now managed by Flux_MaterialAsset::Create() and its static registry
	// Material creation/deletion has been moved to Flux_MaterialAsset for better lifecycle management

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

	//--------------------------------------------------------------------------
	// New Asset System - Mesh/Skeleton/Model Assets and Instances
	//--------------------------------------------------------------------------

	/**
	 * Load a mesh asset from file (cached - returns existing if already loaded)
	 * @param strPath Path to .zmesh file
	 * @return Loaded asset, or nullptr on failure
	 */
	static Zenith_MeshAsset* LoadMeshAsset(const std::string& strPath);

	/**
	 * Load a skeleton asset from file (cached - returns existing if already loaded)
	 * @param strPath Path to .zskel file
	 * @return Loaded asset, or nullptr on failure
	 */
	static Zenith_SkeletonAsset* LoadSkeletonAsset(const std::string& strPath);

	/**
	 * Load a model asset from file (cached - returns existing if already loaded)
	 * @param strPath Path to .zmodel file
	 * @return Loaded asset, or nullptr on failure
	 */
	static Zenith_ModelAsset* LoadModelAsset(const std::string& strPath);

	/**
	 * Create a mesh instance from asset (always creates new - caller owns lifetime)
	 * @param pxAsset Source mesh asset
	 * @return New mesh instance, or nullptr on failure
	 */
	static Flux_MeshInstance* CreateMeshInstance(Zenith_MeshAsset* pxAsset);

	/**
	 * Create a skeleton instance from asset (always creates new - caller owns lifetime)
	 * @param pxAsset Source skeleton asset
	 * @return New skeleton instance, or nullptr on failure
	 */
	static Flux_SkeletonInstance* CreateSkeletonInstance(Zenith_SkeletonAsset* pxAsset);

	/**
	 * Create a model instance from asset (always creates new - caller owns lifetime)
	 * @param pxAsset Source model asset
	 * @return New model instance, or nullptr on failure
	 */
	static Flux_ModelInstance* CreateModelInstance(Zenith_ModelAsset* pxAsset);

	/**
	 * High-level loader: loads model asset + creates instance (convenience function)
	 * @param strPath Path to .zmodel file
	 * @return New model instance, or nullptr on failure
	 */
	static Flux_ModelInstance* LoadAndCreateModelInstance(const std::string& strPath);

	/**
	 * Unload a mesh asset (decrements ref count, unloads when zero)
	 * @param pxAsset Asset to unload
	 */
	static void UnloadMeshAsset(Zenith_MeshAsset* pxAsset);

	/**
	 * Unload a skeleton asset (decrements ref count, unloads when zero)
	 * @param pxAsset Asset to unload
	 */
	static void UnloadSkeletonAsset(Zenith_SkeletonAsset* pxAsset);

	/**
	 * Unload a model asset (decrements ref count, unloads when zero)
	 * @param pxAsset Asset to unload
	 */
	static void UnloadModelAsset(Zenith_ModelAsset* pxAsset);

	/**
	 * Destroy a mesh instance
	 * @param pxInstance Instance to destroy (will be deleted)
	 */
	static void DestroyMeshInstance(Flux_MeshInstance* pxInstance);

	/**
	 * Destroy a skeleton instance
	 * @param pxInstance Instance to destroy (will be deleted)
	 */
	static void DestroySkeletonInstance(Flux_SkeletonInstance* pxInstance);

	/**
	 * Destroy a model instance
	 * @param pxInstance Instance to destroy (will be deleted)
	 */
	static void DestroyModelInstance(Flux_ModelInstance* pxInstance);

	/**
	 * Clear all new asset caches (called on scene reset)
	 * Note: This also destroys the cached assets themselves
	 */
	static void ClearAllNewAssets();

	/**
	 * Get count of loaded new assets for diagnostics
	 */
	static uint32_t GetLoadedMeshAssetCount();
	static uint32_t GetLoadedSkeletonAssetCount();
	static uint32_t GetLoadedModelAssetCount();

private:
	// Asset pools - fixed-size arrays for all assets
	static Flux_Texture* s_pxTextures;          // Array of ZENITH_MAX_TEXTURES
	static Flux_MeshGeometry* s_pxMeshes;       // Array of ZENITH_MAX_MESHES

	// Track which slots are in use (no string keys, just ID sets)
	static std::unordered_set<AssetID> s_xUsedTextureIDs;
	static std::unordered_set<AssetID> s_xUsedMeshIDs;

	// Slot allocation helpers
	static AssetID GetNextFreeTextureSlot();
	static AssetID GetNextFreeMeshSlot();

	// Pointer-to-ID conversion (for deletion)
	static AssetID GetIDFromTexturePointer(const Flux_Texture* pxTexture);
	static AssetID GetIDFromMeshPointer(const Flux_MeshGeometry* pxMesh);

	// Lifecycle logging flag
	static bool s_bLifecycleLoggingEnabled;

	//--------------------------------------------------------------------------
	// New Asset System - Path-to-Asset Caches
	//--------------------------------------------------------------------------

	// Cached asset maps (path -> asset pointer)
	static std::unordered_map<std::string, Zenith_MeshAsset*> s_xLoadedMeshAssets;
	static std::unordered_map<std::string, Zenith_SkeletonAsset*> s_xLoadedSkeletonAssets;
	static std::unordered_map<std::string, Zenith_ModelAsset*> s_xLoadedModelAssets;
};
